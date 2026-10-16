#include "ChunkEdit.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <map>
#include <vector>
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxVertexPC.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapRenderChunk.h"
#include "TerrainEdit.h"
#include "TextureEdit.h"

namespace ChunkEdit {
    static const float s_origin = 17066.666f;
    static const float s_chunkSize = 33.333332f;
    static const float s_quadSize = 33.333332f / 4.0f;
    static const uint32_t s_neutralColor = 0xFF7F7F7F;

    struct AddedColors {
        CMapChunk* m_chunk;
        uint32_t m_colors[145];
    };

    Settings s_settings = { 15.0f, 15.0f, -1, 15.0f, 1.0f, { 1.0f, 1.0f, 1.0f } };
    ViewFlags s_view = { false, false, false };

    static std::map<uint32_t, AddedColors*> s_addedColors;

    static uint32_t ChunkKey(int32_t row, int32_t col) {
        return (static_cast<uint32_t>(row) << 10) | static_cast<uint32_t>(col);
    }

    static float DistanceToRect(float px, float py, float minX, float minY, float size) {
        float dx = std::max(std::max(minX - px, 0.0f), px - (minX + size));
        float dy = std::max(std::max(minY - py, 0.0f), py - (minY + size));
        return sqrtf(dx * dx + dy * dy);
    }

    template <typename Fun>
    static void ForChunksInRange(const C3Vector& pos, float radius, Fun&& fun) {
        int32_t rowA = TerrainEdit::ChunkCoord(pos.x + radius);
        int32_t rowB = TerrainEdit::ChunkCoord(pos.x - radius);
        int32_t colA = TerrainEdit::ChunkCoord(pos.y + radius);
        int32_t colB = TerrainEdit::ChunkCoord(pos.y - radius);

        for (int32_t row = std::min(rowA, rowB); row <= std::max(rowA, rowB); row++) {
            for (int32_t col = std::min(colA, colB); col <= std::max(colA, colB); col++) {
                CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

                if (!chunk || !chunk->m_header) {
                    continue;
                }

                float minX = s_origin - (row + 1) * s_chunkSize;
                float minY = s_origin - (col + 1) * s_chunkSize;

                if (DistanceToRect(pos.x, pos.y, minX, minY, s_chunkSize) <= radius) {
                    fun(row, col, chunk);
                }
            }
        }
    }

    template <typename Fun>
    static void ForChunksOnTile(const C3Vector& pos, Fun&& fun) {
        int32_t tileRow = TerrainEdit::ChunkCoord(pos.x) & ~15;
        int32_t tileCol = TerrainEdit::ChunkCoord(pos.y) & ~15;

        for (int32_t row = tileRow; row < tileRow + 16; row++) {
            for (int32_t col = tileCol; col < tileCol + 16; col++) {
                CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

                if (chunk && chunk->m_header) {
                    fun(row, col, chunk);
                }
            }
        }
    }

    static void InvalidateVertices(CMapChunk* chunk) {
        CMapRenderChunk* renderChunk = chunk->m_renderChunk;

        if (renderChunk && renderChunk->m_chunkBuf && renderChunk->m_chunkBuf->m_vertexBuf) {
            renderChunk->m_chunkBuf->m_vertexBuf->m_hasData = 0;
        }
    }

    static void SetChunkHoles(int32_t row, int32_t col, CMapChunk* chunk, uint16_t holes) {
        if (chunk->m_header->m_holes == holes) {
            return;
        }

        EditorUndo::RecordChunk(row, col);
        chunk->m_header->m_holes = holes;
        TextureEdit::RequestRebuild(row, col);
    }

    void SetHoles(const C3Vector& pos, bool whole, bool add) {
        float radius = s_settings.m_holeRadius;

        ForChunksInRange(pos, radius, [&](int32_t row, int32_t col, CMapChunk* chunk) {
            uint16_t holes = chunk->m_header->m_holes;

            if (whole) {
                holes = add ? 0xFFFF : 0;
            } else {
                float top = s_origin - row * s_chunkSize;
                float left = s_origin - col * s_chunkSize;

                for (int32_t qr = 0; qr < 4; qr++) {
                    for (int32_t qc = 0; qc < 4; qc++) {
                        if (DistanceToRect(pos.x, pos.y, top - (qr + 1) * s_quadSize, left - (qc + 1) * s_quadSize, s_quadSize) <= radius) {
                            uint16_t bit = static_cast<uint16_t>(1 << (qr * 4 + qc));
                            holes = add ? (holes | bit) : (holes & ~bit);
                        }
                    }
                }
            }

            SetChunkHoles(row, col, chunk, holes);
        });
    }

    void SetTileHoles(const C3Vector& pos, bool add) {
        ForChunksOnTile(pos, [&](int32_t row, int32_t col, CMapChunk* chunk) {
            SetChunkHoles(row, col, chunk, add ? 0xFFFF : 0);
        });

        EditorUndo::EndStep();
    }

    static void SetChunkArea(int32_t row, int32_t col, CMapChunk* chunk) {
        if (s_settings.m_areaId < 0 || chunk->m_header->m_areaid == static_cast<uint32_t>(s_settings.m_areaId)) {
            return;
        }

        EditorUndo::RecordChunk(row, col);
        chunk->m_header->m_areaid = s_settings.m_areaId;
        chunk->m_areaId = s_settings.m_areaId;
    }

    void SetAreaId(const C3Vector& pos) {
        ForChunksInRange(pos, s_settings.m_areaRadius, SetChunkArea);
    }

    void SetTileAreaId(const C3Vector& pos) {
        ForChunksOnTile(pos, SetChunkArea);
        EditorUndo::EndStep();
    }

    int32_t PickAreaId(const C3Vector& pos) {
        CMapChunk* chunk = TerrainEdit::GetChunk(TerrainEdit::ChunkCoord(pos.x), TerrainEdit::ChunkCoord(pos.y));

        if (!chunk || !chunk->m_header) {
            return -1;
        }

        s_settings.m_areaId = chunk->m_areaId;
        return chunk->m_areaId;
    }

    void SetImpass(const C3Vector& pos, bool add) {
        int32_t row = TerrainEdit::ChunkCoord(pos.x);
        int32_t col = TerrainEdit::ChunkCoord(pos.y);
        CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

        if (!chunk || !chunk->m_header) {
            return;
        }

        uint32_t flags = add ? (chunk->m_header->m_flags | 0x2) : (chunk->m_header->m_flags & ~0x2u);

        if (flags != chunk->m_header->m_flags) {
            EditorUndo::RecordChunk(row, col);
            chunk->m_header->m_flags = flags;
        }
    }

    static uint32_t* ColorsFor(int32_t row, int32_t col, CMapChunk* chunk, bool create) {
        auto found = s_addedColors.find(ChunkKey(row, col));

        if (found != s_addedColors.end() && found->second->m_chunk == chunk && chunk->m_vertexShading == found->second->m_colors) {
            return found->second->m_colors;
        }

        if (chunk->m_vertexShading && (found == s_addedColors.end() || chunk->m_vertexShading != found->second->m_colors)) {
            return chunk->m_vertexShading;
        }

        if (!create) {
            return nullptr;
        }

        AddedColors* added = found != s_addedColors.end() ? found->second : new AddedColors();
        added->m_chunk = chunk;

        for (uint32_t i = 0; i < 145; i++) {
            added->m_colors[i] = s_neutralColor;
        }

        s_addedColors[ChunkKey(row, col)] = added;
        chunk->m_vertexShading = added->m_colors;
        chunk->m_header->m_flags |= 0x40;
        return added->m_colors;
    }

    void PaintVertexColor(const C3Vector& pos, float elapsedSec, bool add) {
        float radius = s_settings.m_vertexRadius;
        float change = 2.0f * elapsedSec * s_settings.m_vertexSpeed;

        if (radius <= 0.0f) {
            return;
        }

        ForChunksInRange(pos, radius, [&](int32_t row, int32_t col, CMapChunk* chunk) {
            bool touched = false;

            for (int32_t i = 0; i < 145 && !touched; i++) {
                C3Vector vertex;
                chunk->GetVertexPosition(i, &vertex);
                float dx = vertex.x - pos.x;
                float dy = vertex.y - pos.y;
                float dz = vertex.z - pos.z;
                touched = sqrtf(dx * dx + dy * dy + dz * dz) <= radius;
            }

            if (!touched) {
                return;
            }

            EditorUndo::RecordChunk(row, col);
            uint32_t* colors = ColorsFor(row, col, chunk, true);

            for (int32_t i = 0; i < 145; i++) {
                C3Vector vertex;
                chunk->GetVertexPosition(i, &vertex);
                float dx = vertex.x - pos.x;
                float dy = vertex.y - pos.y;
                float dz = vertex.z - pos.z;
                float dist = sqrtf(dx * dx + dy * dy + dz * dz);

                if (dist > radius) {
                    continue;
                }

                float edit = change * (1.0f - dist / radius);
                uint8_t* bytes = reinterpret_cast<uint8_t*>(&colors[i]);
                float rgb[3] = { bytes[2] / 127.0f, bytes[1] / 127.0f, bytes[0] / 127.0f };

                for (int32_t c = 0; c < 3; c++) {
                    float target = add ? s_settings.m_color[c] / 0.5f : 1.0f;
                    rgb[c] += (target - rgb[c]) * edit;
                    rgb[c] = std::min(std::max(rgb[c], 0.0f), 2.0f);
                }

                bytes[2] = static_cast<uint8_t>(rgb[0] * 127.0f);
                bytes[1] = static_cast<uint8_t>(rgb[1] * 127.0f);
                bytes[0] = static_cast<uint8_t>(rgb[2] * 127.0f);
            }

            InvalidateVertices(chunk);
        });
    }

    bool PickVertexColor(const C3Vector& pos) {
        int32_t row = TerrainEdit::ChunkCoord(pos.x);
        int32_t col = TerrainEdit::ChunkCoord(pos.y);
        CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

        if (!chunk) {
            return false;
        }

        uint32_t* colors = ColorsFor(row, col, chunk, false);
        int32_t best = 0;
        float bestDist = 1e30f;

        for (int32_t i = 0; i < 145; i++) {
            C3Vector vertex;
            chunk->GetVertexPosition(i, &vertex);
            float dx = vertex.x - pos.x;
            float dy = vertex.y - pos.y;
            float dist = dx * dx + dy * dy;

            if (dist < bestDist) {
                bestDist = dist;
                best = i;
            }
        }

        const uint8_t* bytes = colors ? reinterpret_cast<const uint8_t*>(&colors[best]) : reinterpret_cast<const uint8_t*>(&s_neutralColor);
        s_settings.m_color[0] = std::min(bytes[2] / 254.0f, 1.0f);
        s_settings.m_color[1] = std::min(bytes[1] / 254.0f, 1.0f);
        s_settings.m_color[2] = std::min(bytes[0] / 254.0f, 1.0f);
        return true;
    }

    void EndStroke() {
        if (EditorUndo::IsOpen()) {
            EditorUndo::EndStep();
        }
    }

    bool Capture(int32_t row, int32_t col, EditorUndo::ChunkSnapshot& snapshot) {
        CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

        if (!chunk || !chunk->m_header) {
            return false;
        }

        snapshot.m_row = row;
        snapshot.m_col = col;
        snapshot.m_flags = chunk->m_header->m_flags;
        snapshot.m_holes = chunk->m_header->m_holes;
        snapshot.m_areaId = chunk->m_header->m_areaid;
        uint32_t* colors = ColorsFor(row, col, chunk, false);
        snapshot.m_hasColors = colors != nullptr;

        if (colors) {
            memcpy(snapshot.m_colors, colors, sizeof(snapshot.m_colors));
        }

        return true;
    }

    void Restore(const EditorUndo::ChunkSnapshot& snapshot) {
        CMapChunk* chunk = TerrainEdit::GetChunk(snapshot.m_row, snapshot.m_col);

        if (!chunk || !chunk->m_header) {
            return;
        }

        bool holesChanged = chunk->m_header->m_holes != snapshot.m_holes;
        chunk->m_header->m_flags = snapshot.m_flags;
        chunk->m_header->m_holes = snapshot.m_holes;
        chunk->m_header->m_areaid = snapshot.m_areaId;
        chunk->m_areaId = snapshot.m_areaId;

        uint32_t* colors = ColorsFor(snapshot.m_row, snapshot.m_col, chunk, snapshot.m_hasColors);

        if (snapshot.m_hasColors && colors) {
            memcpy(colors, snapshot.m_colors, sizeof(snapshot.m_colors));
        } else if (!snapshot.m_hasColors && colors) {
            auto found = s_addedColors.find(ChunkKey(snapshot.m_row, snapshot.m_col));

            if (found != s_addedColors.end() && chunk->m_vertexShading == found->second->m_colors) {
                chunk->m_vertexShading = nullptr;
                delete found->second;
                s_addedColors.erase(found);
            }
        }

        InvalidateVertices(chunk);

        if (holesChanged) {
            TextureEdit::RequestRebuild(snapshot.m_row, snapshot.m_col);
        }
    }

    bool GetInfo(const C3Vector& pos, uint32_t* flags, uint32_t* holes, uint32_t* areaId, uint32_t* color) {
        int32_t row = TerrainEdit::ChunkCoord(pos.x);
        int32_t col = TerrainEdit::ChunkCoord(pos.y);
        CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

        if (!chunk || !chunk->m_header) {
            return false;
        }

        uint32_t* colors = ColorsFor(row, col, chunk, false);
        *flags = chunk->m_header->m_flags;
        *holes = chunk->m_header->m_holes;
        *areaId = chunk->m_header->m_areaid;
        int32_t best = 0;
        float bestDist = 1e30f;

        for (int32_t i = 0; i < 145; i++) {
            C3Vector vertex;
            chunk->GetVertexPosition(i, &vertex);
            float dist = (vertex.x - pos.x) * (vertex.x - pos.x) + (vertex.y - pos.y) * (vertex.y - pos.y);

            if (dist < bestDist) {
                bestDist = dist;
                best = i;
            }
        }

        *color = colors ? colors[best] : 0;
        return true;
    }

    const uint32_t* GetAddedVertexColors(int32_t row, int32_t col) {
        auto found = s_addedColors.find(ChunkKey(row, col));
        return found != s_addedColors.end() ? found->second->m_colors : nullptr;
    }

    static CImVector AreaColor(uint32_t areaId) {
        uint32_t hash = areaId * 2654435761u;
        CImVector color;
        color.r = static_cast<uint8_t>(64 + (hash & 0xBF));
        color.g = static_cast<uint8_t>(64 + ((hash >> 8) & 0xBF));
        color.b = static_cast<uint8_t>(64 + ((hash >> 16) & 0xBF));
        color.a = 0x60;
        return color;
    }

    static std::vector<CGxVertexPC> s_fillVertices;
    static std::vector<CGxVertexPC> s_lineVertices;

    static void Flush(std::vector<CGxVertexPC>& vertices, EGxPrim prim, uint32_t unit) {
        const uint32_t batchSize = 6000 - 6000 % unit;

        for (size_t start = 0; start < vertices.size(); start += batchSize) {
            uint32_t count = static_cast<uint32_t>(std::min<size_t>(batchSize, vertices.size() - start));
            CGxBuf* buf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPC), count);
            CGxVertexPC* dst = reinterpret_cast<CGxVertexPC*>(g_theGxDevicePtr->BufLock(buf));
            memcpy(dst, &vertices[start], count * sizeof(CGxVertexPC));
            GxBufUnlock(buf, 0);
            GxPrimVertexPtr(buf, GxVBF_PC);

            CGxBatch batch;
            batch.m_primType = prim;
            batch.m_start = 0;
            batch.m_count = count;
            batch.m_minIndex = 0;
            batch.m_maxIndex = count - 1;
            g_theGxDevicePtr->Draw(&batch, 0);
        }

        vertices.clear();
    }

    static void PushVertex(std::vector<CGxVertexPC>& vertices, CMapChunk* chunk, int32_t row, int32_t col, float lift, CImVector color) {
        CGxVertexPC vertex;
        chunk->GetVertexPosition(row * 17 + col, &vertex.p);
        vertex.p.z += lift;
        vertex.c = color;
        vertices.push_back(vertex);
    }

    static void DrawChunk(CMapChunk* chunk, CImVector color) {
        const int32_t step = 2;
        GxFormatColor(color);

        for (int32_t row = 0; row < 8; row += step) {
            for (int32_t col = 0; col < 8; col += step) {
                PushVertex(s_fillVertices, chunk, row, col, 0.3f, color);
                PushVertex(s_fillVertices, chunk, row, col + step, 0.3f, color);
                PushVertex(s_fillVertices, chunk, row + step, col, 0.3f, color);
                PushVertex(s_fillVertices, chunk, row, col + step, 0.3f, color);
                PushVertex(s_fillVertices, chunk, row + step, col + step, 0.3f, color);
                PushVertex(s_fillVertices, chunk, row + step, col, 0.3f, color);
            }
        }
    }

    static void DrawChunkLines(CMapChunk* chunk, int32_t row, int32_t col, bool lines, bool holeLines, bool adtBorders) {
        CImVector lineColor;
        lineColor.r = 0x10;
        lineColor.g = 0x10;
        lineColor.b = 0x10;
        lineColor.a = 0xB0;
        CImVector holeColor;
        holeColor.r = 0x52;
        holeColor.g = 0x81;
        holeColor.b = 0xB9;
        holeColor.a = 0xC0;
        CImVector borderColor;
        borderColor.r = 0xFF;
        borderColor.g = 0x30;
        borderColor.b = 0x30;
        borderColor.a = 0xFF;
        GxFormatColor(lineColor);
        GxFormatColor(holeColor);
        GxFormatColor(borderColor);

        auto edge = [&](int32_t r0, int32_t c0, int32_t r1, int32_t c1, CImVector color) {
            int32_t steps = std::max(std::abs(r1 - r0), std::abs(c1 - c0));

            for (int32_t i = 0; i < steps; i++) {
                PushVertex(s_lineVertices, chunk, r0 + (r1 - r0) * i / steps, c0 + (c1 - c0) * i / steps, 0.2f, color);
                PushVertex(s_lineVertices, chunk, r0 + (r1 - r0) * (i + 1) / steps, c0 + (c1 - c0) * (i + 1) / steps, 0.2f, color);
            }
        };

        if (holeLines) {
            for (int32_t i = 2; i < 8; i += 2) {
                edge(i, 0, i, 8, holeColor);
                edge(0, i, 8, i, holeColor);
            }
        }

        bool top = adtBorders && (row & 15) == 0;
        bool bottom = adtBorders && (row & 15) == 15;
        bool left = adtBorders && (col & 15) == 0;
        bool right = adtBorders && (col & 15) == 15;

        if (lines || top) {
            edge(0, 0, 0, 8, top ? borderColor : lineColor);
        }

        if (lines || bottom) {
            edge(8, 0, 8, 8, bottom ? borderColor : lineColor);
        }

        if (lines || left) {
            edge(0, 0, 8, 0, left ? borderColor : lineColor);
        }

        if (lines || right) {
            edge(0, 8, 8, 8, right ? borderColor : lineColor);
        }
    }

    void DrawOverlay(const C3Vector& camPos) {
        TerrainEdit::MODE mode = TerrainEdit::s_mode;
        bool lines = s_view.m_lines || mode == TerrainEdit::MODE_HOLES;
        bool holeLines = s_view.m_holeLines || mode == TerrainEdit::MODE_HOLES;
        bool adtBorders = s_view.m_adtBorders;
        bool fill = mode == TerrainEdit::MODE_AREA || mode == TerrainEdit::MODE_IMPASS;

        if (!fill && !lines && !holeLines && !adtBorders) {
            return;
        }

        g_theGxDevicePtr->RsPush();
        GxRsSet(GxRs_Fog, 0);
        GxRsSet(GxRs_Lighting, 0);
        GxRsSet(GxRs_VertexShader, nullptr);
        GxRsSet(GxRs_PixelShader, nullptr);
        GxRsSet(GxRs_Texture0, nullptr);
        GxRsSet(GxRs_DepthTest, 1);
        GxRsSet(GxRs_DepthWrite, 0);
        GxRsSet(GxRs_Culling, 0);
        GxRsSet(GxRs_BlendingMode, GxBlend_Alpha);

        C44Matrix world;
        world.Translate(C3Vector(-camPos.x, -camPos.y, -camPos.z));
        GxXformSet(GxXform_World, world);

        const int32_t range = 12;
        int32_t centerRow = TerrainEdit::ChunkCoord(camPos.x);
        int32_t centerCol = TerrainEdit::ChunkCoord(camPos.y);
        CImVector impass;
        impass.r = 0xFF;
        impass.g = 0xFF;
        impass.b = 0xFF;
        impass.a = 0x70;

        for (int32_t row = centerRow - range; row <= centerRow + range; row++) {
            for (int32_t col = centerCol - range; col <= centerCol + range; col++) {
                CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

                if (!chunk || !chunk->m_header) {
                    continue;
                }

                if (mode == TerrainEdit::MODE_AREA) {
                    DrawChunk(chunk, AreaColor(chunk->m_header->m_areaid));
                } else if (mode == TerrainEdit::MODE_IMPASS && (chunk->m_header->m_flags & 0x2)) {
                    DrawChunk(chunk, impass);
                }

                if (lines || holeLines || adtBorders) {
                    DrawChunkLines(chunk, row, col, lines, holeLines, adtBorders);
                }
            }
        }

        Flush(s_fillVertices, GxPrim_Triangles, 3);
        Flush(s_lineVertices, GxPrim_Lines, 2);

        GxXformSet(GxXform_World, C44Matrix());
        g_theGxDevicePtr->RsPop();
    }

    void Rebind() {
        for (auto& entry : s_addedColors) {
            int32_t row = static_cast<int32_t>(entry.first >> 10);
            int32_t col = static_cast<int32_t>(entry.first & 0x3FF);
            CMapChunk* chunk = TerrainEdit::GetChunk(row, col);

            if (!chunk || !chunk->m_header || chunk->m_vertexShading) {
                continue;
            }

            entry.second->m_chunk = chunk;
            chunk->m_vertexShading = entry.second->m_colors;
            chunk->m_header->m_flags |= 0x40;
            InvalidateVertices(chunk);
        }
    }

    void ResetArea(int32_t areaX, int32_t areaY) {
        for (auto it = s_addedColors.begin(); it != s_addedColors.end();) {
            int32_t row = static_cast<int32_t>(it->first >> 10);
            int32_t col = static_cast<int32_t>(it->first & 0x3FF);

            if ((row >> 4) == areaY && (col >> 4) == areaX) {
                delete it->second;
                it = s_addedColors.erase(it);
            } else {
                ++it;
            }
        }
    }

    void Reset() {
        for (auto& entry : s_addedColors) {
            delete entry.second;
        }

        s_addedColors.clear();
    }
}
