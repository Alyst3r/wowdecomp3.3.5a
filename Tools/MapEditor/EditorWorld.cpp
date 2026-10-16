#include "EditorWorld.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "WoW/Source/WorldClient/MapObjDef.h"
#include <cstring>
#include "WoW/Source/LiquidSystem/Liquid.h"
#include "WoW/Source/WorldClient/MapObj.h"
#include "WoW/Source/WorldClient/CWorldOcclusion.h"
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/CiRect.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/DetailDoodad.h"
#include "WoW/Source/WorldClient/MapLight.h"
#include "Engine/Source/Model2/M2Light.h"
#include "WoW/Source/WorldClient/MapArea.h"
#include "ChunkEdit.h"
#include "EditorUndo.h"
#include "TextureEdit.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapRenderChunk.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/SortTable.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/WorldView.h"
#include "WoW/Source/WorldClient/CPortalView.h"
#include "WoW/Source/WorldClient/FrameTime.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Tempest/CRect.h"
#include <cmath>

namespace EditorWorld {
    static const int32_t s_chunkRadius = 24;

    static CiRect s_rect;
    static uint32_t s_areasRequested;
    static uint32_t s_areasLoaded;
    static uint32_t s_chunksDrawn;
    static float s_timeOfDay = 720.0f;
    static bool s_dayNightStarted;

    static std::vector<CMapBaseObjLink*> s_parkedAreas;

    static void ParkDirtyAreas() {
        int32_t minX = CWorld::gbChunkRect.minX >> 4;
        int32_t maxX = CWorld::gbChunkRect.maxX >> 4;
        int32_t minY = CWorld::gbChunkRect.minY >> 4;
        int32_t maxY = CWorld::gbChunkRect.maxY >> 4;
        const std::vector<uint32_t>& dirty = EditorUndo::GetDirtyAreas();

        if (dirty.empty()) {
            return;
        }

        for (CMapBaseObjLink* link = CMap::s_unk_ADFBEC.Head(); link;) {
            CMapBaseObjLink* next = CMap::s_unk_ADFBEC.Next(link);
            CMapArea* area = static_cast<CMapArea*>(link->m_owner);
            uint32_t index = static_cast<uint32_t>(64 * area->m_index.y + area->m_index.x);
            bool inside = area->m_index.x >= minX && area->m_index.x <= maxX && area->m_index.y >= minY && area->m_index.y <= maxY;

            if (!inside && !area->m_asyncObject && std::find(dirty.begin(), dirty.end(), index) != dirty.end()) {
                link->m_refLink.Unlink();
                s_parkedAreas.push_back(link);
            }

            link = next;
        }
    }

    static void UnparkAreas() {
        for (CMapBaseObjLink* link : s_parkedAreas) {
            CMap::s_unk_ADFBEC.LinkToTail(link);
        }

        s_parkedAreas.clear();
    }

    static int32_t ChunkIndex(float coord) {
        return static_cast<int32_t>((17066.666f - coord) / 33.333332f);
    }

    void LoadMap(const char* directory, int32_t mapId) {
        World::s_farClip = 777.0f;
        World::s_nearClip = 0.2f;
        World::s_prevFarClip = World::s_farClip;
        CWorld::enables &= ~Enable_LowDetail;
        CMap::Load(directory, mapId);
        Liquid::SetShadersEnabled((CWorld::enables2 & Enable_VertexShader) && (CWorld::enables & Enable_PixelShaders));
        Liquid::SetSpecularEnabled(CMap::enableSpecular);
        Liquid::CMaterialBank::Destroy();
    }

    void Update(const C3Vector& camPos, const C3Vector& camTarget) {
        World::s_currentWorldPos = camPos;
        CWorld::s_groupAreaOfInterest.b = C3Vector(camPos.x - 150.0f, camPos.y - 150.0f, camPos.z - 150.0f);
        CWorld::s_groupAreaOfInterest.t = C3Vector(camPos.x + 150.0f, camPos.y + 150.0f, camPos.z + 150.0f);
        CWorld::s_objectAreaOfInterest.b = C3Vector(camPos.x - World::s_farClip, camPos.y - World::s_farClip, camPos.z - World::s_farClip);
        CWorld::s_objectAreaOfInterest.t = C3Vector(camPos.x + World::s_farClip, camPos.y + World::s_farClip, camPos.z + World::s_farClip);

        C3Vector pos = camPos;
        C3Vector target = camTarget;
        CWorldScene::Update(&pos, &target);
        CWorldScene::LocateViewer3();
        CWorldScene::s_cullPassStamp++;

        int32_t chunkX = ChunkIndex(camPos.y);
        int32_t chunkY = ChunkIndex(camPos.x);

        s_rect.minX = std::max((chunkX - s_chunkRadius) & ~1, 0);
        s_rect.maxX = std::min(((chunkX + s_chunkRadius) & ~1) + 1, 1023);
        s_rect.minY = std::max((chunkY - s_chunkRadius) & ~1, 0);
        s_rect.maxY = std::min(((chunkY + s_chunkRadius) & ~1) + 1, 1023);

        CWorld::chunkRect = s_rect;
        CWorld::gbChunkRect.minX = std::max(s_rect.minX - 2, 0);
        CWorld::gbChunkRect.maxX = std::min(s_rect.maxX + 2, 1023);
        CWorld::gbChunkRect.minY = std::max(s_rect.minY - 2, 0);
        CWorld::gbChunkRect.maxY = std::min(s_rect.maxY + 2, 1023);

        s_areasLoaded = 0;

        for (int32_t areaY = s_rect.minY >> 4; areaY <= s_rect.maxY >> 4; areaY++) {
            for (int32_t areaX = s_rect.minX >> 4; areaX <= s_rect.maxX >> 4; areaX++) {
                int32_t index = 64 * areaY + areaX;

                if (!(CMap::m_areaInfo[index].m_flags & 1)) {
                    continue;
                }

                CMapArea* area = CMap::m_areaTable[index];

                if (!area) {
                    area = CMap::PrepareArea(areaX, areaY);
                    CMap::LoadArea(area);
                    s_areasRequested++;
                    continue;
                }

                if (area->m_asyncObject) {
                    continue;
                }

                s_areasLoaded++;

                CiRect areaRect;
                areaRect.minX = std::max(s_rect.minX, 16 * areaX);
                areaRect.maxX = std::min(s_rect.maxX, 16 * areaX + 15);
                areaRect.minY = std::max(s_rect.minY, 16 * areaY);
                areaRect.maxY = std::min(s_rect.maxY, 16 * areaY + 15);
                area->Update(1, &areaRect);
            }
        }

        TextureEdit::Rebind();
        ChunkEdit::Rebind();
        TextureEdit::FlushRenderRebuilds();
        ParkDirtyAreas();
        CMap::PrepareUpdate(true);
        UnparkAreas();

        float progression = s_timeOfDay / 1440.0f;

        C3Vector dir(camTarget.x - camPos.x, camTarget.y - camPos.y, camTarget.z - camPos.z);
        float length = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

        if (length > 0.0f) {
            dir = C3Vector(dir.x / length, dir.y / length, dir.z / length);
        }

        DayNight::DNInfo* info = DayNight::GetInfo();
        info->m_dayProgression = progression;
        info->m_timeOfDay = static_cast<uint32_t>(progression * 1440.0f);
        info->m_day = 0.0f;
        info->m_farClip = World::s_farClip;
        info->m_deltaSec = FrameTime::s_tickTimeSec;
        info->m_timeSec = OsGetAsyncTimeMs() * 0.001f;
        info->m_cameraPos = camPos;
        info->m_lightRefPos = camPos;
        info->m_playerPos = camPos;
        info->m_cameraDir = dir;
        info->m_weatherIntensity = 0.0f;

        C3Vector camera = camPos;
        DayNight::Update(s_dayNightStarted ? 0 : 1, &camera);
        s_dayNightStarted = true;
        CWorld::farFog = info->m_fog.m_end;
    }

    void Render() {
        CWorldScene::nDoodadsRendered = 0;
        CWorldScene::s_chunksRendered = 0;

        if (!CWorld::shadowModTexture) {
            CImVector shadow;
            shadow.r = 0x40;
            shadow.g = 0x40;
            shadow.b = 0x48;
            shadow.a = 0x4C;
            CWorld::SetShadowColor(shadow);
        }

        if (!CWorldScene::s_viewerMapObjDef) {
            CWorldScene::ComputeExteriorSortDistances();
        }

        CWorldScene::UpdateViewerLiquid();
        CWorldScene::frustumStack[CWorldScene::frustumIndex].CalcPlanesFromCorners(CWorldScene::s_frustumCorners);
        CWorldScene::s_occlusionFarDepth = World::s_farClip - 33.333332f;
        CMapRenderChunk::UpdatePools();
        CMapObj::UpdatePools();
        CDetailDoodad::UpdatePools();
        memset(CWorldScene::s_clipBufferMask, 0, sizeof(CWorldScene::s_clipBufferMask));

        for (uint32_t i = 0; i < 384; i++) {
            CWorldScene::s_clipBuffer[i] = -1000000.0f;
        }

        CWorldOcclusion::ClearVolumes();
        CWorldScene::AddWorldOccluders();

        CRect fullScreen;
        fullScreen.minY = 0.0f;
        fullScreen.minX = 0.0f;
        fullScreen.maxY = 1.0f;
        fullScreen.maxX = 1.0f;

        if (CWorldScene::s_viewerMapObjDef) {
            if (CWorldScene::s_viewerMovedMapObjDef) {
                CWorldScene::s_viewerMovedMapObjDef->RenderWithCallback(&CWorldScene::s_viewerMovedMapObjGroups);
                CWorldScene::s_unk_ADF570 = CPortalView();
                CWorldScene::frustumPortalView = CPortalView();
                CWorldScene::s_exteriorPortalViews.SetCount(0);
                CWorldScene::s_portalViews.SetCount(0);
            }

            CWorldScene::s_viewerMapObjDef->RenderWithCallback(&CWorldScene::s_viewerMapObjGroups);

            if (CWorldScene::frustumPortalView.m_maxViewDepth < 0.0f) {
                CWorldScene::sortTable.Clear();
            } else {
                CWorldScene::s_occlusionNearDepth = CWorldScene::frustumPortalView.m_maxViewDepth + 33.333332f;
                CWorldScene::CullSortTable(&CWorldScene::frustumPortalView.m_rect);
            }

            CWorldScene::CullThroughPortal(&fullScreen);
        } else {
            CWorldScene::s_unk_ADF570.m_rect = fullScreen;
            CWorldScene::s_unk_ADF570.m_maxViewDepth = 0.0f;
            CWorldScene::s_unk_ADF570.m_vertCount = 0;
            CWorldScene::frustumPortalView.m_rect = fullScreen;
            CWorldScene::frustumPortalView.m_maxViewDepth = 0.0f;
            CWorldScene::frustumPortalView.m_vertCount = 0;
            CWorldScene::s_occlusionNearDepth = -10000.0f;
            CWorldScene::CullSortTable(&CWorldScene::frustumPortalView.m_rect);
        }

        CWorldScene::CullMapObjDefGroup();
        CWorldScene::sortTable.m_pendingExteriorGroupList.UnlinkAll();

        DayNight::DNInfo* info = DayNight::GetInfo();
        CImVector clearColor;
        *reinterpret_cast<uint32_t*>(&clearColor) = 0;

        if (CWorldScene::s_unk_ADF570.m_maxViewDepth < 0.0f) {
            clearColor = info->m_fogInterior.m_color;
        } else if (s_liquidTypeId) {
            clearColor = info->m_fog.m_color;
        }

        g_theGxDevicePtr->SceneClear(3, clearColor);

        CWorldScene::ApplyFogState();
        CWorldScene::RenderChunks();
        CWorldScene::RenderMapObjDefGroups();

        if (CWorldScene::s_unk_ADF570.m_maxViewDepth >= 0.0f) {
            if (CWorldScene::s_exteriorPortalViews.Count()) {
                CWorldScene::CreateInversePortalList();
                CWorldScene::RenderPortalList(&CWorldScene::s_exteriorPortalViews, 1);
            }

            if (CWorldScene::s_portalViews.Count()) {
                CWorldScene::RenderPortalList(&CWorldScene::s_portalViews, 0);
            }

            if (!s_liquidTypeId && (CWorld::enables & Enable_Sky)) {
                DayNight::RenderSky(&CWorldScene::s_unk_ADF570);
            }
        }

        Liquid::UpdateProceduralTextures();
        CWorldScene::PrepareLiquidInstances();

        if ((CWorld::enables & Enable_Water) && CWorldScene::s_liquidRenderer) {
            CWorldScene::s_liquidRenderer->Flush(CWorldScene::s_activeWorldView, 0);
        }

        s_chunksDrawn = CWorldScene::s_chunksRendered;
    }

    uint32_t TimeOfDay() {
        return static_cast<uint32_t>(s_timeOfDay);
    }

    void SetTimeOfDay(uint32_t minutes) {
        s_timeOfDay = static_cast<float>(minutes % 1440);
    }

    void Stats(uint32_t& areasRequested, uint32_t& areasLoaded, uint32_t& chunksDrawn) {
        areasRequested = s_areasRequested;
        areasLoaded = s_areasLoaded;
        chunksDrawn = s_chunksDrawn;
    }
}
