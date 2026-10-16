#include "EditorPopout.h"
#include "EditorViewport.h"
#include <windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Event/EvtInput.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CFrameStrataNode.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Scrn/Scrn.h"
#include "Engine/Source/Services/Camera.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/CRect.h"

namespace EditorPopout {
    struct Popout {
        char m_name[64];
        CSimpleFrame* m_frame;
        HWND m_hwnd;
        IDirect3DSwapChain9* m_swapChain;
        int32_t m_backBufferWidth;
        int32_t m_backBufferHeight;
        int32_t m_width;
        int32_t m_height;
        bool m_closeRequested;
        bool m_resizePending;
        bool m_internalResize;
        int32_t m_requestedWidth;
        int32_t m_requestedHeight;
        RECT m_source;
        uint32_t m_savedStrata;
        uint32_t m_savedLevel;
        bool m_hidden;
        uint32_t m_slot;
    };

    static const char* s_windowClass = "MapEditorPopout";
    static const uint32_t s_levelBand = 30;
    static const uint32_t s_maxPopouts = 4;
    static std::vector<Popout*> s_popouts;
    static HLAYER s_layer;
    static HWND s_mainWindow;
    static WNDPROC s_mainWindowProc;
    static bool s_initialized;
    static bool s_renderingPopouts;

    static CGxDeviceD3d* Device() {
        return static_cast<CGxDeviceD3d*>(g_theGxDevicePtr);
    }

    static void ReleaseSwapChain(Popout* popout) {
        if (popout->m_swapChain) {
            popout->m_swapChain->Release();
            popout->m_swapChain = nullptr;
        }
    }

    static void ReleaseAllSwapChains() {
        for (Popout* popout : s_popouts) {
            ReleaseSwapChain(popout);
        }
    }

    static Popout* Find(const char* name) {
        for (Popout* popout : s_popouts) {
            if (!SStrCmpI(popout->m_name, name, STORM_MAX_STR)) {
                return popout;
            }
        }

        return nullptr;
    }

    static float PixelsPerDdc() {
        RECT client;
        GetClientRect(s_mainWindow, &client);
        float ddcWidth;
        float ddcHeight;
        NDCToDDC(1.0f, 1.0f, &ddcWidth, &ddcHeight);
        return ddcHeight > 0.0f ? static_cast<float>(client.bottom - client.top) / ddcHeight : 1.0f;
    }

    static void DispatchMouse(Popout* popout, int32_t (*handler)(const void*, void*), MOUSEBUTTON button, int32_t x, int32_t y, int32_t wheel) {
        CSimpleTop* top = CSimpleTop::s_instance;

        if (!top || !popout->m_frame) {
            return;
        }

        float pixelsPerDdc = PixelsPerDdc();
        const CRect& rect = popout->m_frame->m_rect;
        float ddcX = rect.minX + static_cast<float>(x) / pixelsPerDdc;
        float ddcY = rect.maxY - static_cast<float>(y) / pixelsPerDdc;

        EVENT_DATA_MOUSE data;
        data.m_mode = MOUSE_MODE_NORMAL;
        data.m_button = button;
        data.m_buttonState = Input::s_buttonState;
        data.m_metaKeyState = Input::s_metaKeyState;
        data.m_flags = 0;
        DDCToNDC(ddcX, ddcY, &data.m_x, &data.m_y);
        data.m_wheelDistance = wheel;
        data.m_time = OsGetAsyncTimeMs();

        EditorViewport* viewport = EditorViewport::s_current;
        uint32_t layoutFlags = viewport ? viewport->m_layoutFlags : 0;

        if (viewport) {
            viewport->m_layoutFlags &= ~0x100u;
        }

        handler(&data, top);

        if (viewport) {
            viewport->m_layoutFlags = layoutFlags;
        }
    }

    static MOUSEBUTTON ButtonFromMessage(UINT message) {
        switch (message) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return MOUSE_BUTTON_LEFT;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return MOUSE_BUTTON_RIGHT;
            default:
                return MOUSE_BUTTON_MIDDLE;
        }
    }

    static LRESULT CALLBACK PopoutWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        Popout* popout = reinterpret_cast<Popout*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

        if (!popout) {
            return DefWindowProcA(hwnd, message, wparam, lparam);
        }

        int32_t x = static_cast<int16_t>(LOWORD(lparam));
        int32_t y = static_cast<int16_t>(HIWORD(lparam));

        switch (message) {
            case WM_MOUSEMOVE:
                DispatchMouse(popout, CSimpleTop::OnMouseMove, static_cast<MOUSEBUTTON>(0), x, y, 0);
                return 0;

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN: {
                MOUSEBUTTON button = ButtonFromMessage(message);
                SetCapture(hwnd);
                DispatchMouse(popout, CSimpleTop::OnMouseMove, static_cast<MOUSEBUTTON>(0), x, y, 0);
                Input::s_buttonState |= button;
                DispatchMouse(popout, CSimpleTop::OnMouseDown, button, x, y, 0);
                return 0;
            }

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP: {
                MOUSEBUTTON button = ButtonFromMessage(message);
                Input::s_buttonState &= ~static_cast<uint32_t>(button);

                if (!(wparam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))) {
                    ReleaseCapture();
                }

                DispatchMouse(popout, CSimpleTop::OnMouseUp, button, x, y, 0);
                return 0;
            }

            case WM_MOUSEWHEEL: {
                POINT point = { x, y };
                ScreenToClient(hwnd, &point);
                DispatchMouse(popout, CSimpleTop::OnMouseWheel, static_cast<MOUSEBUTTON>(0), point.x, point.y, static_cast<int16_t>(HIWORD(wparam)) / WHEEL_DELTA);
                return 0;
            }

            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
                return SendMessageA(s_mainWindow, message, wparam, lparam);

            case WM_SETCURSOR:
                if (LOWORD(lparam) == HTCLIENT) {
                    SetCursor(LoadCursorA(nullptr, IDC_ARROW));
                    return 1;
                }
                break;

            case WM_SIZE:
                if (!popout->m_internalResize && wparam != SIZE_MINIMIZED) {
                    popout->m_resizePending = true;
                    popout->m_requestedWidth = LOWORD(lparam);
                    popout->m_requestedHeight = HIWORD(lparam);
                }
                return 0;

            case WM_CLOSE:
                popout->m_closeRequested = true;
                return 0;

            case WM_ERASEBKGND:
                return 1;

            case WM_PAINT: {
                PAINTSTRUCT paint;
                BeginPaint(hwnd, &paint);
                EndPaint(hwnd, &paint);
                return 0;
            }

            default:
                break;
        }

        return DefWindowProcA(hwnd, message, wparam, lparam);
    }

    static LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        if (message == WM_SIZE) {
            ReleaseAllSwapChains();
        }

        return CallWindowProcA(s_mainWindowProc, hwnd, message, wparam, lparam);
    }

    static bool CreateSwapChain(Popout* popout) {
        CGxDeviceD3d* device = Device();
        RECT client;
        GetClientRect(s_mainWindow, &client);
        int32_t width = client.right - client.left;
        int32_t height = client.bottom - client.top;

        if (width <= 0 || height <= 0) {
            return false;
        }

        D3DPRESENT_PARAMETERS params = {};
        params.BackBufferWidth = width;
        params.BackBufferHeight = height;
        params.BackBufferFormat = D3DFMT_UNKNOWN;
        params.BackBufferCount = 1;
        params.SwapEffect = D3DSWAPEFFECT_COPY;
        params.hDeviceWindow = popout->m_hwnd;
        params.Windowed = TRUE;
        params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        if (FAILED(device->m_d3dDevice->CreateAdditionalSwapChain(&params, &popout->m_swapChain))) {
            popout->m_swapChain = nullptr;
            return false;
        }

        popout->m_backBufferWidth = width;
        popout->m_backBufferHeight = height;
        return true;
    }

    static void SyncWindowSize(Popout* popout, int32_t width, int32_t height) {
        if (width == popout->m_width && height == popout->m_height) {
            return;
        }

        popout->m_width = width;
        popout->m_height = height;

        RECT rect = { 0, 0, width, height };
        AdjustWindowRectEx(&rect, static_cast<DWORD>(GetWindowLongPtrA(popout->m_hwnd, GWL_STYLE)), FALSE, static_cast<DWORD>(GetWindowLongPtrA(popout->m_hwnd, GWL_EXSTYLE)));
        popout->m_internalResize = true;
        SetWindowPos(popout->m_hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        popout->m_internalResize = false;
    }

    static void FramePixelRect(CSimpleFrame* frame, RECT& rect) {
        float pixelsPerDdc = PixelsPerDdc();
        float ddcWidth;
        float ddcHeight;
        NDCToDDC(1.0f, 1.0f, &ddcWidth, &ddcHeight);
        rect.left = static_cast<LONG>(floorf(frame->m_rect.minX * pixelsPerDdc + 0.5f));
        rect.right = static_cast<LONG>(floorf(frame->m_rect.maxX * pixelsPerDdc + 0.5f));
        rect.top = static_cast<LONG>(floorf((ddcHeight - frame->m_rect.maxY) * pixelsPerDdc + 0.5f));
        rect.bottom = static_cast<LONG>(floorf((ddcHeight - frame->m_rect.minY) * pixelsPerDdc + 0.5f));
    }

    static void PaintPopouts(void* param, const RECTF* rect, const RECTF* visible, float elapsedSec) {
        CGxDeviceD3d* device = Device();
        CSimpleTop* top = CSimpleTop::s_instance;

        if (s_popouts.empty() || !top || !device->m_context || !device->m_d3dDevice) {
            return;
        }

        IDirect3DDevice9* d3d = device->m_d3dDevice;
        RECT client;
        GetClientRect(s_mainWindow, &client);

        IDirect3DSurface9* mainTarget = nullptr;
        IDirect3DSurface9* mainDepth = nullptr;
        d3d->GetRenderTarget(0, &mainTarget);
        d3d->GetDepthStencilSurface(&mainDepth);

        DWORD zEnable;
        DWORD zWrite;
        d3d->GetRenderState(D3DRS_ZENABLE, &zEnable);
        d3d->GetRenderState(D3DRS_ZWRITEENABLE, &zWrite);
        d3d->SetRenderState(D3DRS_ZENABLE, FALSE);
        d3d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d3d->SetDepthStencilSurface(nullptr);

        C2Vector offset(0.0f, 0.0f);
        CameraSetupScreenProjection(top->m_rect, offset, 0.0f, 0);
        GxXformSetViewport(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

        CFrameStrata* strata = top->m_strata[FRAME_STRATA_WORLD];
        s_renderingPopouts = true;

        for (Popout* popout : s_popouts) {
            if (!popout->m_frame || popout->m_hidden) {
                continue;
            }

            if (popout->m_swapChain && (popout->m_backBufferWidth != client.right || popout->m_backBufferHeight != client.bottom)) {
                ReleaseSwapChain(popout);
            }

            if (!popout->m_swapChain && !CreateSwapChain(popout)) {
                continue;
            }

            RECT frameRect;
            FramePixelRect(popout->m_frame, frameRect);
            int32_t width = frameRect.right - frameRect.left;
            int32_t height = frameRect.bottom - frameRect.top;

            if (width <= 0 || height <= 0) {
                continue;
            }

            SyncWindowSize(popout, width, height);

            IDirect3DSurface9* backBuffer = nullptr;

            if (FAILED(popout->m_swapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer))) {
                continue;
            }

            d3d->SetRenderTarget(0, backBuffer);
            d3d->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(38, 40, 45), 1.0f, 0);

            uint32_t firstLevel = popout->m_frame->m_level;
            uint32_t lastLevel = firstLevel + s_levelBand < strata->m_topLevel ? firstLevel + s_levelBand : strata->m_topLevel;

            for (uint32_t level = firstLevel; level < lastLevel; level++) {
                CFrameStrataNode* node = strata->m_levels[level];

                for (CRenderBatch* batch = node->m_renderList.Head(); batch; batch = node->m_renderList.Next(batch)) {
                    CSimpleRender::DrawBatch(batch);
                }
            }

            popout->m_source = frameRect;
            backBuffer->Release();
        }

        s_renderingPopouts = false;

        d3d->SetRenderTarget(0, mainTarget);
        d3d->SetDepthStencilSurface(mainDepth);
        d3d->SetRenderState(D3DRS_ZENABLE, zEnable);
        d3d->SetRenderState(D3DRS_ZWRITEENABLE, zWrite);

        if (mainTarget) {
            mainTarget->Release();
        }

        if (mainDepth) {
            mainDepth->Release();
        }

        d3d->EndScene();

        for (Popout* popout : s_popouts) {
            if (!popout->m_hidden && popout->m_swapChain && popout->m_source.right > popout->m_source.left && popout->m_source.bottom > popout->m_source.top) {
                popout->m_swapChain->Present(&popout->m_source, nullptr, popout->m_hwnd, nullptr, 0);
            }
        }

        d3d->BeginScene();
    }

    static int32_t OnIdle(const void* data, void* param) {
        for (size_t i = 0; i < s_popouts.size(); i++) {
            Popout* popout = s_popouts[i];
            char code[160];

            if (popout->m_resizePending) {
                popout->m_resizePending = false;
                popout->m_width = popout->m_requestedWidth;
                popout->m_height = popout->m_requestedHeight;
                SStrPrintf(code, sizeof(code), "MapEditor_OnPopoutResized(\"%s\", %d, %d)", popout->m_name, popout->m_requestedWidth, popout->m_requestedHeight);
                FrameScript_Execute(code, "popout", nullptr);
            }

            if (popout->m_closeRequested) {
                popout->m_closeRequested = false;
                SStrPrintf(code, sizeof(code), "MapEditor_OnPopoutClosed(\"%s\")", popout->m_name);
                FrameScript_Execute(code, "popout", nullptr);
            }
        }

        return 1;
    }

    void Initialize() {
        if (s_initialized) {
            return;
        }

        s_initialized = true;
        s_mainWindow = static_cast<HWND>(g_theGxDevicePtr->DeviceWindow());

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_DBLCLKS;
        windowClass.lpfnWndProc = PopoutWindowProc;
        windowClass.hInstance = GetModuleHandleA(nullptr);
        windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        windowClass.hIcon = reinterpret_cast<HICON>(GetClassLongPtrA(s_mainWindow, GCLP_HICON));
        windowClass.lpszClassName = s_windowClass;
        RegisterClassExA(&windowClass);

        s_mainWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(s_mainWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindowProc)));

        ScrnLayerCreate(nullptr, 1.5f, 5, nullptr, &PaintPopouts, &s_layer);
        EventRegister(EVENT_ID_IDLE, OnIdle);
    }

    static uint32_t FreeSlot() {
        for (uint32_t slot = 0; slot < s_maxPopouts; slot++) {
            bool used = false;

            for (Popout* popout : s_popouts) {
                if (popout->m_slot == slot) {
                    used = true;
                }
            }

            if (!used) {
                return slot;
            }
        }

        return s_maxPopouts - 1;
    }

    bool Create(const char* frameName, const char* title) {
        Initialize();

        if (Find(frameName)) {
            return true;
        }

        if (s_popouts.size() >= s_maxPopouts) {
            return false;
        }

        CSimpleFrame* frame = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName(frameName, CSimpleFrame::GetObjectType()));

        if (!frame) {
            return false;
        }

        Popout* popout = new Popout();
        SStrCopy(popout->m_name, frameName, sizeof(popout->m_name));
        popout->m_frame = frame;
        popout->m_swapChain = nullptr;
        popout->m_width = 0;
        popout->m_height = 0;
        popout->m_closeRequested = false;
        popout->m_resizePending = false;
        popout->m_internalResize = false;
        popout->m_source = { 0, 0, 0, 0 };
        popout->m_hidden = false;

        float pixelsPerDdc = PixelsPerDdc();
        int32_t width = static_cast<int32_t>((frame->m_rect.maxX - frame->m_rect.minX) * pixelsPerDdc + 0.5f);
        int32_t height = static_cast<int32_t>((frame->m_rect.maxY - frame->m_rect.minY) * pixelsPerDdc + 0.5f);
        width = width > 32 ? width : 265;
        height = height > 32 ? height : 600;

        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
        DWORD exStyle = WS_EX_TOOLWINDOW;
        RECT rect = { 0, 0, width, height };
        AdjustWindowRectEx(&rect, style, FALSE, exStyle);

        RECT mainRect;
        GetWindowRect(s_mainWindow, &mainRect);
        POINT cursor;
        GetCursorPos(&cursor);
        int32_t left = cursor.x - (rect.right - rect.left) + 40;
        int32_t topEdge = cursor.y - 12;

        popout->m_hwnd = CreateWindowExA(exStyle, s_windowClass, title, style, left, topEdge, rect.right - rect.left, rect.bottom - rect.top, s_mainWindow, nullptr, GetModuleHandleA(nullptr), nullptr);

        if (!popout->m_hwnd) {
            delete popout;
            return false;
        }

        SetWindowLongPtrA(popout->m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(popout));
        popout->m_savedStrata = frame->m_strata;
        popout->m_savedLevel = frame->m_level;
        popout->m_slot = FreeSlot();
        frame->SetFrameStrata(FRAME_STRATA_WORLD);
        frame->SetFrameLevel(static_cast<int32_t>(2 + popout->m_slot * s_levelBand), 1);
        popout->m_width = width;
        popout->m_height = height;
        ShowWindow(popout->m_hwnd, SW_SHOWNOACTIVATE);
        s_popouts.push_back(popout);
        return true;
    }

    void Destroy(const char* frameName) {
        for (size_t i = 0; i < s_popouts.size(); i++) {
            Popout* popout = s_popouts[i];

            if (SStrCmpI(popout->m_name, frameName, STORM_MAX_STR)) {
                continue;
            }

            ReleaseSwapChain(popout);
            popout->m_frame->SetFrameStrata(static_cast<FRAME_STRATA>(popout->m_savedStrata));
            popout->m_frame->SetFrameLevel(static_cast<int32_t>(popout->m_savedLevel), 1);
            DestroyWindow(popout->m_hwnd);
            s_popouts.erase(s_popouts.begin() + i);
            delete popout;
            return;
        }
    }

    void SetVisible(const char* frameName, bool visible) {
        Popout* popout = Find(frameName);

        if (!popout || popout->m_hidden == !visible) {
            return;
        }

        popout->m_hidden = !visible;
        ShowWindow(popout->m_hwnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    }

    void SetTitle(const char* frameName, const char* title) {
        Popout* popout = Find(frameName);

        if (popout) {
            SetWindowTextA(popout->m_hwnd, title);
        }
    }

    bool IsActive(const char* frameName) {
        return Find(frameName) != nullptr;
    }

    bool TestClick(const char* frameName, int32_t x, int32_t y) {
        Popout* popout = Find(frameName);

        if (!popout) {
            return false;
        }

        DispatchMouse(popout, CSimpleTop::OnMouseMove, static_cast<MOUSEBUTTON>(0), x, y, 0);
        Input::s_buttonState |= MOUSE_BUTTON_LEFT;
        DispatchMouse(popout, CSimpleTop::OnMouseDown, MOUSE_BUTTON_LEFT, x, y, 0);
        Input::s_buttonState &= ~static_cast<uint32_t>(MOUSE_BUTTON_LEFT);
        DispatchMouse(popout, CSimpleTop::OnMouseUp, MOUSE_BUTTON_LEFT, x, y, 0);
        return true;
    }

    bool IsAppForeground() {
        HWND foreground = GetForegroundWindow();

        if (!s_mainWindow) {
            s_mainWindow = static_cast<HWND>(g_theGxDevicePtr->DeviceWindow());
        }

        if (foreground == s_mainWindow) {
            return true;
        }

        for (Popout* popout : s_popouts) {
            if (foreground == popout->m_hwnd) {
                return true;
            }
        }

        return false;
    }

    bool IsMainWindowForeground() {
        if (!s_mainWindow) {
            s_mainWindow = static_cast<HWND>(g_theGxDevicePtr->DeviceWindow());
        }

        return GetForegroundWindow() == s_mainWindow;
    }

    bool IsRenderingPopouts() {
        return s_renderingPopouts;
    }
}
