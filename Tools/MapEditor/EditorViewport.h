#ifndef TOOLS_MAPEDITOR_EDITORVIEWPORT_H
#define TOOLS_MAPEDITOR_EDITORVIEWPORT_H

#include <cstdint>
#include "Engine/Source/Base/CDataAllocator.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CRect.h"

class CKeyEvent;
class CMouseEvent;
class CRenderBatch;

class EditorViewport : public CSimpleFrame {
    public:
    static CDataAllocator s_viewportHeap;
    static EditorViewport* s_current;

    static CSimpleFrame* Create(CSimpleFrame* parent);
    static void RenderWorld(void* param);

    C3Vector m_position;
    float m_yaw;
    float m_pitch;
    float m_fov;
    float m_speed;
    CRect m_viewportNDC;
    bool m_looking;
    bool m_leftDown;
    bool m_rightDown;
    bool m_cursorValid;
    C3Vector m_cursor;
    int32_t m_lookAnchorX;
    int32_t m_lookAnchorY;
    int32_t m_lastMouseX;
    int32_t m_lastMouseY;
    bool m_middleDown;
    bool m_rotatingObject;
    bool m_leftDragged;
    int32_t m_leftPressX;
    int32_t m_leftPressY;
    bool m_rayValid;
    C3Vector m_rayStart;
    C3Vector m_rayDir;
    float m_elapsedSec;
    float m_fps;
    float m_fpsTime;
    uint32_t m_fpsFrames;

    EditorViewport(CSimpleFrame* parent);
    ~EditorViewport() override;
    void operator delete(void* ptr);
    void OnLayerUpdate(float elapsedSec) override;
    void OnFrameRender(CRenderBatch* batch, uint32_t layer) override;
    void OnFrameSizeChanged(const CRect& rect) override;
    int32_t OnLayerKeyDown(const CKeyEvent& evt) override;
    int32_t OnLayerMouseDown(const CMouseEvent& evt, const char* btn) override;
    int32_t OnLayerMouseUp(const CMouseEvent& evt, const char* btn) override;
    C3Vector Forward() const;
    float Aspect() const;
    void UpdateCamera(float elapsedSec);
    void UpdateCursor();
    void UpdateTool(float elapsedSec);
    void ReleaseButtons();
    float PixelHeight() const;
    void OnWorldRender();
};

#endif
