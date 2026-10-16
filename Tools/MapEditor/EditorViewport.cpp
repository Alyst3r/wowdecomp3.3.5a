#include "EditorViewport.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <typeinfo>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Event/CKeyEvent.h"
#include "Engine/Source/Event/CMouseEvent.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CSimpleEditBox.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/Gxu.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/ShaderEffect/ShaderEffect.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "WoW/Source/WorldClient/FrameTime.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/WorldClient/World.h"
#include "ChunkEdit.h"
#include "EditorApp.h"
#include "EditorPopout.h"
#include "EditorWorld.h"
#include "ObjectEdit.h"
#include "TextureEdit.h"
#include "TerrainEdit.h"

CDataAllocator EditorViewport::s_viewportHeap(sizeof(EditorViewport), 1);
EditorViewport* EditorViewport::s_current;

EditorViewport::EditorViewport(CSimpleFrame* parent) : CSimpleFrame(parent) {
    this->m_position = C3Vector(-9449.0f, 64.0f, 80.0f);
    this->m_yaw = 0.0f;
    this->m_pitch = -0.4f;
    this->m_fov = 0.9424778f;
    this->m_speed = 40.0f;
    this->m_viewportNDC = { 0.0f, 0.0f, 0.0f, 0.0f };
    this->m_looking = false;
    this->m_leftDown = false;
    this->m_rightDown = false;
    this->m_cursorValid = false;
    this->m_cursor = C3Vector(0.0f, 0.0f, 0.0f);
    this->m_lookAnchorX = 0;
    this->m_lookAnchorY = 0;
    this->m_lastMouseX = 0;
    this->m_lastMouseY = 0;
    this->m_middleDown = false;
    this->m_rotatingObject = false;
    this->m_leftDragged = false;
    this->m_leftPressX = 0;
    this->m_leftPressY = 0;
    this->m_rayValid = false;
    this->m_elapsedSec = 0.0f;
    this->m_fps = 0.0f;
    this->m_fpsTime = 0.0f;
    this->m_fpsFrames = 0;

    EditorViewport::s_current = this;

    this->SetFrameStrata(FRAME_STRATA_WORLD);
    this->EnableEvent(SIMPLE_EVENT_KEY, -1);
    this->EnableEvent(SIMPLE_EVENT_MOUSE, -1);
    this->EnableEvent(SIMPLE_EVENT_MOUSEWHEEL, -1);
}

EditorViewport::~EditorViewport() {
    if (EditorViewport::s_current == this) {
        EditorViewport::s_current = nullptr;
    }
}

CSimpleFrame* EditorViewport::Create(CSimpleFrame* parent) {
    void* data = EditorViewport::s_viewportHeap.GetData(0, typeid(EditorViewport).raw_name(), SLOG_OBJECT);
    return data ? new (data) EditorViewport(parent) : nullptr;
}

void EditorViewport::operator delete(void* ptr) {
    if (ptr) {
        EditorViewport::s_viewportHeap.PutData(ptr, nullptr, 0);
    }
}

C3Vector EditorViewport::Forward() const {
    return C3Vector(cosf(this->m_yaw) * cosf(this->m_pitch), sinf(this->m_yaw) * cosf(this->m_pitch), sinf(this->m_pitch));
}

float EditorViewport::Aspect() const {
    CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSize(window);
    float width = (window.maxX - window.minX) * (this->m_viewportNDC.maxX - this->m_viewportNDC.minX);
    float height = (window.maxY - window.minY) * (this->m_viewportNDC.maxY - this->m_viewportNDC.minY);
    return width > 0.0f && height > 0.0f ? width / height : 16.0f / 9.0f;
}

void EditorViewport::OnFrameSizeChanged(const CRect& rect) {
    this->CSimpleFrame::OnFrameSizeChanged(rect);

    DDCToNDC(this->m_rect.minX, this->m_rect.minY, &this->m_viewportNDC.minX, &this->m_viewportNDC.minY);
    DDCToNDC(this->m_rect.maxX, this->m_rect.maxY, &this->m_viewportNDC.maxX, &this->m_viewportNDC.maxY);
    this->m_viewportNDC.minX = this->m_viewportNDC.minX > 0.0f ? this->m_viewportNDC.minX : 0.0f;
    this->m_viewportNDC.minY = this->m_viewportNDC.minY > 0.0f ? this->m_viewportNDC.minY : 0.0f;
    this->m_viewportNDC.maxX = this->m_viewportNDC.maxX < 1.0f ? this->m_viewportNDC.maxX : 1.0f;
    this->m_viewportNDC.maxY = this->m_viewportNDC.maxY < 1.0f ? this->m_viewportNDC.maxY : 1.0f;
}

static bool AltDown() {
    return OsInputIsKeyDown(KEY_LALT) || OsInputIsKeyDown(KEY_RALT);
}

static bool ShiftDown() {
    return OsInputIsKeyDown(KEY_LSHIFT) || OsInputIsKeyDown(KEY_RSHIFT);
}

static bool CtrlDown() {
    return OsInputIsKeyDown(KEY_LCONTROL) || OsInputIsKeyDown(KEY_RCONTROL);
}

void EditorViewport::UpdateCamera(float elapsedSec) {
    if (this->m_looking) {
        int32_t x;
        int32_t y;
        OsInputGetMousePosition(&x, &y);
        this->m_yaw -= static_cast<float>(x - this->m_lookAnchorX) * 0.005f;
        this->m_pitch -= static_cast<float>(y - this->m_lookAnchorY) * 0.005f;
        this->m_pitch = this->m_pitch < -1.55f ? -1.55f : (this->m_pitch > 1.55f ? 1.55f : this->m_pitch);
        OsInputSetMousePosition(this->m_lookAnchorX, this->m_lookAnchorY);
    }

    this->m_yaw = fmodf(this->m_yaw, 6.2831855f);

    if (this->m_yaw < 0.0f) {
        this->m_yaw += 6.2831855f;
    }

    if (CSimpleEditBox::s_currentFocus || CtrlDown() || !EditorPopout::IsMainWindowForeground()) {
        return;
    }

    float speed = this->m_speed * elapsedSec;

    if (ShiftDown()) {
        speed *= 2.0f;
    }

    C3Vector forward = this->Forward();
    C3Vector right(sinf(this->m_yaw), -cosf(this->m_yaw), 0.0f);
    float move = static_cast<float>(OsInputIsKeyDown(KEY_W) - OsInputIsKeyDown(KEY_S));
    float strafe = static_cast<float>(OsInputIsKeyDown(KEY_D) - OsInputIsKeyDown(KEY_A));
    float rise = static_cast<float>(OsInputIsKeyDown(KEY_Q) - OsInputIsKeyDown(KEY_E));

    this->m_position.x += (forward.x * move + right.x * strafe) * speed;
    this->m_position.y += (forward.y * move + right.y * strafe) * speed;
    this->m_position.z += (forward.z * move + rise) * speed;
}

float EditorViewport::PixelHeight() const {
    CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSize(window);
    float height = (window.maxY - window.minY) * (this->m_viewportNDC.maxY - this->m_viewportNDC.minY);
    return height > 1.0f ? height : 1.0f;
}

void EditorViewport::ReleaseButtons() {
    if (this->m_rightDown && !OsInputIsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        this->m_rightDown = false;
        this->m_looking = false;

        if (this->m_rotatingObject) {
            this->m_rotatingObject = false;
            ObjectEdit::EndEdit();
        }
    }

    if (this->m_middleDown && !OsInputIsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        this->m_middleDown = false;
        ObjectEdit::EndEdit();
    }

    if (this->m_leftDown && !OsInputIsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        this->m_leftDown = false;
        TerrainEdit::EndStroke();
        TextureEdit::EndStroke();
        ChunkEdit::EndStroke();
    }
}

void EditorViewport::UpdateTool(float elapsedSec) {
    int32_t x;
    int32_t y;
    OsInputGetMousePosition(&x, &y);
    float dx = static_cast<float>(x - this->m_lastMouseX);
    float dy = static_cast<float>(y - this->m_lastMouseY);
    this->m_lastMouseX = x;
    this->m_lastMouseY = y;

    ReleaseButtons();

    bool foreground = EditorPopout::IsMainWindowForeground();
    bool alt = foreground && AltDown();
    bool shift = foreground && ShiftDown();
    bool ctrl = foreground && CtrlDown();
    bool space = foreground && !CSimpleEditBox::s_currentFocus && OsInputIsKeyDown(KEY_SPACE);
    TerrainEdit::MODE mode = TerrainEdit::s_mode;

    if (this->m_rightDown && alt && !shift && !ctrl && mode == TerrainEdit::MODE_RAISE_LOWER) {
        TerrainEdit::ChangeInnerRadius(dx / 100.0f);
    }

    if (this->m_rotatingObject && (dx != 0.0f || dy != 0.0f)) {
        float amount = dx / 15.0f * 5.0f + dy / 15.0f * 5.0f;

        if (ctrl) {
            ObjectEdit::RotateBy(C3Vector(amount, 0.0f, 0.0f));
        }

        if (shift) {
            ObjectEdit::RotateBy(C3Vector(0.0f, amount, 0.0f));
        }

        if (alt) {
            ObjectEdit::RotateBy(C3Vector(0.0f, 0.0f, amount));
        }
    }

    if (this->m_middleDown && mode == TerrainEdit::MODE_OBJECT && (dx != 0.0f || dy != 0.0f)) {
        float vertical = -dy / this->PixelHeight();

        if (alt) {
            ObjectEdit::ScaleBy(powf(2.0f, vertical * 4.0f));
        } else if (shift) {
            ObjectEdit::MoveBy(C3Vector(0.0f, 0.0f, vertical * 80.0f));
        } else if (!ctrl) {
            if (ObjectEdit::s_settings.m_followCursor && this->m_cursorValid) {
                ObjectEdit::MoveTo(this->m_cursor);
            } else {
                C3Vector position;
                C3Vector rotation;
                float scale;
                float distance = 50.0f;

                if (ObjectEdit::GetTransform(&position, &rotation, &scale)) {
                    float wx = 17066.666f - position.z - this->m_position.x;
                    float wy = 17066.666f - position.x - this->m_position.y;
                    distance = std::max(sqrtf(wx * wx + wy * wy), 10.0f);
                }

                float horizontal = dx / this->PixelHeight();
                C3Vector forward(cosf(this->m_yaw), sinf(this->m_yaw), 0.0f);
                C3Vector right(sinf(this->m_yaw), -cosf(this->m_yaw), 0.0f);
                float k = distance * 1.2f;
                ObjectEdit::MoveBy(C3Vector((right.x * horizontal + forward.x * vertical) * k, (right.y * horizontal + forward.y * vertical) * k, 0.0f));
            }
        }
    }

    if (!this->m_leftDown) {
        return;
    }

    if (abs(x - this->m_leftPressX) + abs(y - this->m_leftPressY) > 10) {
        this->m_leftDragged = true;
    }

    if (alt && !shift && !ctrl) {
        TerrainEdit::ChangeRadius(dx / 15.0f);
    }

    if (space) {
        TerrainEdit::ChangeSpeed(dx / 30.0f);
    }

    if (!this->m_cursorValid || (alt && !shift && !ctrl) || space) {
        return;
    }

    if (mode == TerrainEdit::MODE_RAISE_LOWER) {
        if (shift) {
            TerrainEdit::Apply(TerrainEdit::OP_RAISE, this->m_cursor, elapsedSec);
        } else if (ctrl) {
            TerrainEdit::Apply(TerrainEdit::OP_LOWER, this->m_cursor, elapsedSec);
        }
    } else if (mode == TerrainEdit::MODE_FLATTEN_BLUR) {
        if (shift) {
            TerrainEdit::Apply(TerrainEdit::OP_FLATTEN, this->m_cursor, elapsedSec);
        } else if (ctrl) {
            TerrainEdit::Apply(TerrainEdit::OP_BLUR, this->m_cursor, elapsedSec);
        }
    } else if (mode == TerrainEdit::MODE_TEXTURE_PAINT && shift && !ctrl) {
        TextureEdit::Paint(this->m_cursor, elapsedSec);
    } else if (mode == TerrainEdit::MODE_HOLES) {
        if (shift) {
            ChunkEdit::SetHoles(this->m_cursor, alt, false);
        } else if (ctrl) {
            ChunkEdit::SetHoles(this->m_cursor, alt, true);
        }
    } else if (mode == TerrainEdit::MODE_AREA) {
        if (shift) {
            ChunkEdit::SetAreaId(this->m_cursor);
        } else if (ctrl && ChunkEdit::PickAreaId(this->m_cursor) >= 0) {
            FrameScript_Execute("if MapEditor_OnAreaPicked then MapEditor_OnAreaPicked() end", "pick", nullptr);
        }
    } else if (mode == TerrainEdit::MODE_IMPASS) {
        if (shift) {
            ChunkEdit::SetImpass(this->m_cursor, true);
        } else if (ctrl) {
            ChunkEdit::SetImpass(this->m_cursor, false);
        }
    } else if (mode == TerrainEdit::MODE_VERTEX && (shift ^ ctrl)) {
        ChunkEdit::PaintVertexColor(this->m_cursor, elapsedSec, shift);
    }
}

void EditorViewport::UpdateCursor() {
    this->m_cursorValid = false;
    this->m_rayValid = false;

    if (!this->m_simpleTop || this->m_looking) {
        return;
    }

    if (!this->m_leftDown && !this->m_middleDown && this->m_simpleTop->m_mouseFocus != this) {
        return;
    }

    float ddcX;
    float ddcY;
    NDCToDDC(this->m_simpleTop->m_mousePosition.m_x, this->m_simpleTop->m_mousePosition.m_y, &ddcX, &ddcY);

    float width = this->m_rect.maxX - this->m_rect.minX;
    float height = this->m_rect.maxY - this->m_rect.minY;

    if (width <= 0.0f || height <= 0.0f) {
        return;
    }

    float u = (ddcX - this->m_rect.minX) / width;
    float v = (ddcY - this->m_rect.minY) / height;

    if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f) {
        return;
    }

    float aspect = this->Aspect();
    float fovY = this->m_fov / sqrtf(aspect * aspect + 1.0f);
    float tanY = tanf(fovY * 0.5f);
    float tanX = tanY * aspect;

    C3Vector forward = this->Forward();
    C3Vector up(0.0f, 0.0f, 1.0f);
    C3Vector right = C3Vector::Cross(forward, up);
    right.Normalize();
    C3Vector camUp = C3Vector::Cross(right, forward);
    camUp.Normalize();

    float sx = (u * 2.0f - 1.0f) * tanX;
    float sy = (v * 2.0f - 1.0f) * tanY;
    C3Vector dir(forward.x + right.x * sx + camUp.x * sy, forward.y + right.y * sx + camUp.y * sy, forward.z + right.z * sx + camUp.z * sy);
    dir.Normalize();

    this->m_rayValid = true;
    this->m_rayStart = this->m_position;
    this->m_rayDir = dir;
    this->m_cursorValid = TerrainEdit::Raycast(this->m_position, dir, World::s_farClip > 0.0f ? World::s_farClip : 1000.0f, &this->m_cursor);
}

void EditorViewport::OnLayerUpdate(float elapsedSec) {
    this->CSimpleFrame::OnLayerUpdate(elapsedSec);

    this->m_elapsedSec = elapsedSec > 0.25f ? 0.25f : elapsedSec;
    this->m_fpsFrames++;
    this->m_fpsTime += elapsedSec;

    if (this->m_fpsTime >= 0.5f) {
        this->m_fps = this->m_fpsFrames / this->m_fpsTime;
        this->m_fpsFrames = 0;
        this->m_fpsTime = 0.0f;
    }

    if (!EditorApp::IsMapLoaded()) {
        return;
    }

    FrameTime::Update(this->m_elapsedSec, OsGetAsyncTimeMs());
    this->UpdateCamera(this->m_elapsedSec);
    this->UpdateCursor();
    this->UpdateTool(this->m_elapsedSec);
}

void EditorViewport::OnFrameRender(CRenderBatch* batch, uint32_t layer) {
    this->CSimpleFrame::OnFrameRender(batch, layer);

    if (!layer) {
        batch->QueueCallback(EditorViewport::RenderWorld, this);
    }
}

void EditorViewport::RenderWorld(void* param) {
    if (EditorPopout::IsRenderingPopouts() || !EditorApp::IsMapLoaded()) {
        return;
    }

    EditorViewport* viewport = static_cast<EditorViewport*>(param);
    C44Matrix projection = g_theGxDevicePtr->m_projection;
    C44Matrix view;
    GxXformView(view);

    viewport->OnWorldRender();

    g_theGxDevicePtr->XformSetProjection(projection);
    g_theGxDevicePtr->XformSetView(view);
    CShaderEffect::UpdateProjMatrix();
}

void EditorViewport::OnWorldRender() {
    CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSize(window);

    if (window.maxY - window.minY == 0.0f || window.maxX - window.minX == 0.0f) {
        return;
    }

    if (!(this->m_viewportNDC.maxY > this->m_viewportNDC.minY && this->m_viewportNDC.maxX > this->m_viewportNDC.minX)) {
        return;
    }

    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
    GxXformViewport(minX, maxX, minY, maxY, minZ, maxZ);

    CGxTex* renderTarget;
    g_theGxDevicePtr->DeviceGetRenderTarget(GxBuffers_Color, renderTarget);

    float width = maxX - minX;
    float height = maxY - minY;
    float top = renderTarget ? 1.0f - this->m_viewportNDC.minY : this->m_viewportNDC.maxY;
    float bottom = renderTarget ? 1.0f - this->m_viewportNDC.maxY : this->m_viewportNDC.minY;

    float nearClip = World::s_nearClip > 0.0f ? World::s_nearClip : 0.2f;
    float farClip = World::s_farClip > nearClip ? World::s_farClip : 1000.0f;

    C44Matrix projection;
    GxuXformCreateProjection_SG(this->m_fov, this->Aspect(), nearClip, farClip, projection);
    GxXformSetProjection(projection);

    C44Matrix view;
    GxuXformCreateLookAtSgCompat(C3Vector(0.0f, 0.0f, 0.0f), this->Forward(), C3Vector(0.0f, 0.0f, 1.0f), view);
    GxXformSetView(view);

    g_theGxDevicePtr->RsPush();

    if (CWorld::enables & Enable_ShowTris) {
        g_theGxDevicePtr->MasterEnableSet(GxMasterEnable_PolygonFill, 0);
    }

    GxXformSetViewport(width * this->m_viewportNDC.minX + minX, this->m_viewportNDC.maxX * width + minX, height * bottom + minY, top * height + minY, minZ, maxZ);
    CShaderEffect::UpdateProjMatrix();

    C3Vector forward = this->Forward();
    C3Vector target(this->m_position.x + forward.x, this->m_position.y + forward.y, this->m_position.z + forward.z);
    EditorWorld::Update(this->m_position, target);

    GxXformSetViewport(width * this->m_viewportNDC.minX + minX, this->m_viewportNDC.maxX * width + minX, height * bottom + minY, top * height + minY, minZ, maxZ);
    CShaderEffect::UpdateProjMatrix();
    EditorWorld::Render();

    if (World::s_m2Scene) {
        World::s_m2Scene->AdvanceTime(static_cast<uint32_t>(this->m_elapsedSec * 1000.0f));
        World::s_m2Scene->Animate(this->m_position);
        World::s_m2Scene->Draw(M2PASS_0);
        CWorldScene::RenderDetailDoodads();
        World::s_m2Scene->Draw(M2PASS_2);
        CWorldScene::RenderLiquidPass();
        World::s_m2Scene->Draw(M2PASS_1);
    }

    if (CWorld::enables & Enable_ShowTris) {
        g_theGxDevicePtr->MasterEnableSet(GxMasterEnable_PolygonFill, 1);
    }

    ChunkEdit::DrawOverlay(this->m_position);

    if (this->m_cursorValid) {
        TerrainEdit::DrawBrush(this->m_cursor, this->m_position);
    }

    ObjectEdit::DrawSelection(this->m_position);

    g_theGxDevicePtr->RsPop();
    GxXformSetViewport(minX, maxX, minY, maxY, minZ, maxZ);
}

int32_t EditorViewport::OnLayerKeyDown(const CKeyEvent& evt) {
    if (this->CSimpleFrame::OnLayerKeyDown(evt)) {
        return 1;
    }

    bool ctrl = CtrlDown();
    bool shift = ShiftDown();

    if (ctrl && shift && evt.m_data.m_key == KEY_Z) {
        EditorUndo::Redo();
        return 1;
    }

    if (ctrl && evt.m_data.m_key == KEY_Z) {
        EditorUndo::Undo();
        return 1;
    }

    if (ctrl && evt.m_data.m_key == KEY_Y) {
        EditorUndo::Redo();
        return 1;
    }

    if (evt.m_data.m_key >= KEY_F1 && evt.m_data.m_key <= KEY_F12) {
        char code[96];
        SStrPrintf(code, sizeof(code), "MapEditor_OnHotkey(\"%s%sF%d\")", ctrl ? "CTRL-" : "", shift ? "SHIFT-" : "", static_cast<int32_t>(evt.m_data.m_key - KEY_F1) + 1);
        FrameScript_Execute(code, "hotkey", nullptr);
        return 1;
    }

    KEY key = evt.m_data.m_key;

    if (ctrl && key >= KEY_A && key <= KEY_Z) {
        char code[96];
        SStrPrintf(code, sizeof(code), "MapEditor_OnHotkey(\"CTRL-%s%c\")", shift ? "SHIFT-" : "", static_cast<char>(key));
        FrameScript_Execute(code, "hotkey", nullptr);
        return 1;
    }

    if (key >= KEY_1 && key <= KEY_1 + 8 && CSimpleEditBox::s_currentFocus) {
        return 0;
    }

    bool movement = key == KEY_W || key == KEY_A || key == KEY_S || key == KEY_D || key == KEY_Q || key == KEY_E;

    if (!ctrl && !CSimpleEditBox::s_currentFocus && ((key >= KEY_A && key <= KEY_Z && !movement) || key == KEY_TAB)) {
        char code[96];
        SStrPrintf(code, sizeof(code), "MapEditor_OnHotkey(\"%s%s\")", shift ? "SHIFT-" : "", key == KEY_TAB ? "TAB" : "");

        if (key != KEY_TAB) {
            SStrPrintf(code, sizeof(code), "MapEditor_OnHotkey(\"%s%s%c\")", AltDown() ? "ALT-" : "", shift ? "SHIFT-" : "", static_cast<char>(key));
        }

        FrameScript_Execute(code, "hotkey", nullptr);
        return 1;
    }

    if (!ctrl && !shift && evt.m_data.m_key >= KEY_1 && evt.m_data.m_key <= KEY_3) {
        TerrainEdit::s_mode = static_cast<TerrainEdit::MODE>(evt.m_data.m_key - KEY_1);
        return 1;
    }

    if (!ctrl && !shift && evt.m_data.m_key >= KEY_1 + 3 && evt.m_data.m_key <= KEY_1 + 7 && evt.m_data.m_key != KEY_1 + 6) {
        static const TerrainEdit::MODE modes[] = { TerrainEdit::MODE_HOLES, TerrainEdit::MODE_AREA, TerrainEdit::MODE_IMPASS, TerrainEdit::MODE_VERTEX, TerrainEdit::MODE_VERTEX };
        TerrainEdit::s_mode = modes[evt.m_data.m_key - (KEY_1 + 3)];
        return 1;
    }

    if (!ctrl && !shift && evt.m_data.m_key == KEY_1 + 8) {
        TerrainEdit::s_mode = TerrainEdit::MODE_OBJECT;
        return 1;
    }

    if (evt.m_data.m_key == KEY_DELETE && TerrainEdit::s_mode == TerrainEdit::MODE_OBJECT && !CSimpleEditBox::s_currentFocus) {
        ObjectEdit::DeleteSelection();
        return 1;
    }

    if (evt.m_data.m_key == KEY_PAGEDOWN && TerrainEdit::s_mode == TerrainEdit::MODE_OBJECT) {
        ObjectEdit::SnapToGround();
        return 1;
    }

    return 0;
}

int32_t EditorViewport::OnLayerMouseDown(const CMouseEvent& evt, const char* btn) {
    if (this->CSimpleFrame::OnLayerMouseDown(evt, btn)) {
        return 1;
    }

    OsInputGetMousePosition(&this->m_lastMouseX, &this->m_lastMouseY);
    bool objectMode = TerrainEdit::s_mode == TerrainEdit::MODE_OBJECT;

    if (evt.m_data.m_button == MOUSE_BUTTON_RIGHT) {
        this->m_rightDown = true;

        if (objectMode && ObjectEdit::GetSelectionCount() && (CtrlDown() || ShiftDown() || AltDown())) {
            this->m_rotatingObject = true;
            ObjectEdit::BeginEdit();
            return 1;
        }

        if (!AltDown()) {
            OsInputGetMousePosition(&this->m_lookAnchorX, &this->m_lookAnchorY);
            this->m_looking = true;
        }

        return 1;
    }

    if (evt.m_data.m_button == MOUSE_BUTTON_MIDDLE) {
        if (objectMode && ObjectEdit::GetSelectionCount()) {
            this->m_middleDown = true;
            ObjectEdit::BeginEdit();
        }

        if (TerrainEdit::s_mode == TerrainEdit::MODE_VERTEX) {
            this->UpdateCursor();

            if (this->m_cursorValid && ChunkEdit::PickVertexColor(this->m_cursor)) {
                FrameScript_Execute("if MapEditor_OnColorPicked then MapEditor_OnColorPicked() end", "pick", nullptr);
            }
        }

        return 1;
    }

    if (evt.m_data.m_button == MOUSE_BUTTON_LEFT) {
        this->m_leftDown = true;
        this->m_leftDragged = false;
        this->m_leftPressX = this->m_lastMouseX;
        this->m_leftPressY = this->m_lastMouseY;
        this->UpdateCursor();

        if (TerrainEdit::s_mode == TerrainEdit::MODE_TEXTURE_PAINT && CtrlDown() && !ShiftDown() && this->m_cursorValid) {
            TextureEdit::PickTexture(this->m_cursor);
        }

        return 1;
    }

    return 0;
}

int32_t EditorViewport::OnLayerMouseUp(const CMouseEvent& evt, const char* btn) {
    if (this->CSimpleFrame::OnLayerMouseUp(evt, btn)) {
        return 1;
    }

    if (evt.m_data.m_button == MOUSE_BUTTON_RIGHT) {
        this->m_rightDown = false;
        this->m_looking = false;

        if (this->m_rotatingObject) {
            this->m_rotatingObject = false;
            ObjectEdit::EndEdit();
        }

        return 1;
    }

    if (evt.m_data.m_button == MOUSE_BUTTON_MIDDLE) {
        if (this->m_middleDown) {
            this->m_middleDown = false;
            ObjectEdit::EndEdit();
        }

        return 1;
    }

    if (evt.m_data.m_button == MOUSE_BUTTON_LEFT) {
        bool click = this->m_leftDown && !this->m_leftDragged;
        this->m_leftDown = false;
        TerrainEdit::EndStroke();
        TextureEdit::EndStroke();

        if (click && TerrainEdit::s_mode == TerrainEdit::MODE_OBJECT && !AltDown()) {
            this->UpdateCursor();
            bool add = ShiftDown() || CtrlDown();
            int32_t kind;
            uint32_t uniqueId;
            float distance;
            float farClip = World::s_farClip > 0.0f ? World::s_farClip : 1000.0f;
            bool hit = this->m_rayValid && ObjectEdit::Pick(this->m_rayStart, this->m_rayDir, farClip, &kind, &uniqueId, &distance);

            if (hit && this->m_cursorValid) {
                C3Vector toCursor(this->m_cursor.x - this->m_position.x, this->m_cursor.y - this->m_position.y, this->m_cursor.z - this->m_position.z);
                hit = distance <= sqrtf(toCursor.x * toCursor.x + toCursor.y * toCursor.y + toCursor.z * toCursor.z) + 0.5f;
            }

            if (hit) {
                ObjectEdit::Select(kind, uniqueId, add);
            } else if (!add) {
                ObjectEdit::ClearSelection();
            }
        }

        return 1;
    }

    return 0;
}
