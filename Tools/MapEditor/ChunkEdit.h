#ifndef TOOLS_MAPEDITOR_CHUNKEDIT_H
#define TOOLS_MAPEDITOR_CHUNKEDIT_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "EditorUndo.h"

namespace ChunkEdit {
    struct Settings {
        float m_holeRadius;
        float m_areaRadius;
        int32_t m_areaId;
        float m_vertexRadius;
        float m_vertexSpeed;
        float m_color[3];
    };

    struct ViewFlags {
        bool m_lines;
        bool m_holeLines;
        bool m_adtBorders;
    };

    extern Settings s_settings;
    extern ViewFlags s_view;

    void SetHoles(const C3Vector& pos, bool whole, bool add);
    void SetTileHoles(const C3Vector& pos, bool add);
    void SetAreaId(const C3Vector& pos);
    void SetTileAreaId(const C3Vector& pos);
    int32_t PickAreaId(const C3Vector& pos);
    void SetImpass(const C3Vector& pos, bool add);
    void PaintVertexColor(const C3Vector& pos, float elapsedSec, bool add);
    bool PickVertexColor(const C3Vector& pos);
    void EndStroke();
    bool Capture(int32_t row, int32_t col, EditorUndo::ChunkSnapshot& snapshot);
    void Restore(const EditorUndo::ChunkSnapshot& snapshot);
    const uint32_t* GetAddedVertexColors(int32_t row, int32_t col);
    bool GetInfo(const C3Vector& pos, uint32_t* flags, uint32_t* holes, uint32_t* areaId, uint32_t* color);
    void DrawOverlay(const C3Vector& camPos);
    void Rebind();
    void ResetArea(int32_t areaX, int32_t areaY);
    void Reset();
}

#endif
