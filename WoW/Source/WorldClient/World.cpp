#include "World.h"
#include "WoW/Source/WorldClient/WorldConstants.h"
#include <cstdlib>
#include <new>
#include <algorithm>
#include <cmath>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/BlizzardCore/Source/Packages/Streaming/Streaming.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxMatrixStack.h"
#include "Engine/Source/Gx/CGxVertexPC.h"
#include "Engine/Source/Tempest/CWFrustum.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/Types.h"
#include "Engine/Source/Gxu/Gxu.h"
#include "Engine/Source/Gxu/GxuLight.h"
#include "Engine/Source/Gxu/GxuScene.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/Model2/M2Light.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/Services/SysMessage.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/ShaderEffect/ShaderEffect.h"
#include "Engine/Source/ShaderEffect/ShaderEffectManager.h"
#include "Engine/Source/ShaderEffect/ShadowCache.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/C4Plane.h"
#include "Engine/Source/Tempest/C4Vector.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CAaSphere.h"
#include "Engine/Source/Tempest/CBoundingBox.h"
#include "Engine/Source/Tempest/CFacet.h"
#include "Engine/Source/Tempest/CFrustum.h"
#include "Engine/Source/Tempest/CMath.h"
#include "Engine/Source/Tempest/CRect.h"
#include "Engine/Source/Tempest/CiRect.h"
#include "WoW/Common/AaBsp.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/AreaTableRec.h"
#include "WoW/Source/DB/Rec/LiquidMaterialRec.h"
#include "WoW/Source/DB/Rec/LiquidTypeRec.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/DB/Rec/ZoneLightPointRec.h"
#include "WoW/Source/DB/Rec/ZoneLightRec.h"
#include "WoW/Source/DB/Rec/WMOAreaTableRec.h"
#include "WoW/Source/LiquidSystem/Liquid.h"
#include "WoW/Source/LoadingScreen.h"
#include "WoW/Source/Object/ObjectClient/Movement_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/Barrier.h"
#include "WoW/Source/WorldClient/CChunkLiquid.h"
#include "WoW/Source/WorldClient/CMapBaseObjLink.h"
#include "WoW/Source/WorldClient/CPortalView.h"
#include "WoW/Source/WorldClient/CWorldMath.h"
#include "WoW/Source/WorldClient/CWorldOcclusion.h"
#include "WoW/Source/WorldClient/ClipInfo.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/DetailDoodad.h"
#include "WoW/Source/WorldClient/FrameTime.h"
#include "WoW/Source/WorldClient/LiquidEnvironment.h"
#include "WoW/Source/WorldClient/LiquidGeomFactories.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/MapEntity.h"
#include "WoW/Source/WorldClient/MapLight.h"
#include "WoW/Source/WorldClient/MapLoad.h"
#include "WoW/Source/WorldClient/MapLowDetail.h"
#include "WoW/Source/WorldClient/MapObj.h"
#include "WoW/Source/WorldClient/MapObjDef.h"
#include "WoW/Source/WorldClient/MapObjGroup.h"
#include "WoW/Source/WorldClient/MapRenderChunk.h"
#include "WoW/Source/WorldClient/MapWeather.h"
#include "WoW/Source/WorldClient/Particulate.h"
#include "WoW/Source/WorldClient/ShadowQuery.h"
#include "WoW/Source/WorldClient/SortTable.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/WorldClient/WorldParam.h"
#include "WoW/Source/WorldClient/WorldView.h"
#include "WoW/Source/WorldClient/ZoneLight.h"
#include "WoW/Source/Ui/MinimapFrame.h"
#include <malloc.h>
#include <cstring>
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/Game/GameClient/ProjectTex.h"

static const float dirLightScaleAmount = 0.5f;

static const int32_t MAX_SOUND_EXT_LEVEL = 9999;
static const int32_t MIN_SOUND_EXT_LEVEL = 2;

// STATIC: 0x00ADEED8
static float s_preloadX = 3.4028235e38f;

// STATIC: 0x00ADEEDC
static float s_preloadY = 3.4028235e38f;

// STATIC: 0x00CD775C
static uint32_t s_preloadCount;

// STATIC: 0x00ADEEE4
float CWorld::s_unk_ADEEE4 = 0.94f;

// STATIC: 0x00ADEEB8
float CWorld::detailDoodadDist = 70.0f;

// STATIC: 0x00ADEEC4
int32_t World::lodMax = 3;

// STATIC: 0x00ADEEC8
int32_t World::lodMin = 2;

// STATIC: 0x00ADF168
C44Matrix World::idMat;

// STATIC: 0x00ADF1A8
TSExplicitList<SWModelFadeout, offsetof(SWModelFadeout, m_link)> s_fadeoutList;

// STATIC: 0x00ADF1B4
TSExplicitList<SWModelFadeout, offsetof(SWModelFadeout, m_link)> s_fadeoutDataPool;

// STATIC: 0x00CB753C
int32_t CWorld::bShowSimpleDoodads;

// STATIC: 0x00CD7540
int32_t CWorld::bLoadSimpleDoodads;

// STATIC: 0x00CD7544
Weather* World::s_weather;

// STATIC: 0x00CD7548
Particulate* CWorld::particulate;

// STATIC: 0x00CD754C
CM2Scene* World::s_m2Scene;

// STATIC: 0x00CD7550
int32_t CWorld::shadowMipLevel;

// STATIC: 0x00CD7554
CTexture* CWorld::shadowModTexture;

// STATIC: 0x00CD7658
int32_t CWorld::s_worldPoolUsage;

// STATIC: 0x00CD765C
int32_t CWorld::terrainAlphaBitDepth;

// STATIC: 0x00CD7660
uint32_t CWorld::maxLights;

// STATIC: 0x00CD7664
int32_t CWorld::enableLayerCnt;

// STATIC: 0x00CD7668
float CWorld::farFog;

// STATIC: 0x00CD766C
int32_t CWorld::detailDoodadAlphaRef;

// STATIC: 0x00CD7670
int32_t CWorld::detailDoodadTest;

// STATIC: 0x00CD7674
float World::groundEffectDistValueSqr;

// STATIC: 0x00CD7678
WorldStatus CWorld::status;

// STATIC: 0x00CD768C
int32_t CWorld::chunkCnt;

// STATIC: 0x00CD7690
int32_t CWorld::frameCnt;

// STATIC: 0x00CD7694
int32_t CWorld::s_elapsedTicks;

// STATIC: 0x00CD769C
float CWorld::s_elapsedTimeSec;

// STATIC: 0x00CD774C
uint32_t CWorld::enables;

// STATIC: 0x00CD7750
uint32_t CWorld::enables2;

// STATIC: 0x00CD7754
uint32_t CWorld::m_M2PassMask;

// STATIC: 0x00CD8624
uint32_t CWorldScene::s_unused_CD8624;

// STATIC: 0x00CD8774
uint32_t CWorldScene::s_unused_CD8774;

// STATIC: 0x00CD7758
float CWorld::s_unk_CD7758;

// STATIC: 0x00CD76B0
static float profTimes[30];

// STATIC: 0x00CD7728
static int32_t profIdx;

// STATIC: 0x00CD7740
int32_t s_disableAreaAmbient;

// STATIC: 0x00CD7764
static void (*s_FrameCntCallback)();

// STATIC: 0x00ADEE78
static C2Vector s_texVectSpeed[8] = { C2Vector(-1.0f, 0.0f), C2Vector(-1.0f, 1.0f), C2Vector(0.0f, 1.0f), C2Vector(1.0f, 1.0f), C2Vector(1.0f, 0.0f), C2Vector(1.0f, -1.0f), C2Vector(0.0f, -1.0f), C2Vector(-1.0f, -1.0f) };

// STATIC: 0x00CD7784
CAaBox CWorld::s_groupAreaOfInterest;

// STATIC: 0x00CD779C
CAaBox CWorld::s_objectAreaOfInterest;

// STATIC: 0x00CD77B8
static TSGrowableArray<CM2Model*> s_unk_CD77B8;

// STATIC: 0x00CD77C8
CiRect CWorld::chunkRect;

// STATIC: 0x00CD77D8
CiRect CWorld::gbChunkRect;

// STATIC: 0x00CD77E8
CiRect CWorld::gbChunkRectOld;

// STATIC: 0x00CD77F8
C4Vector World::texVect[8];

// STATIC: 0x00CD7878
uint32_t CWorld::shadowColor;

// STATIC: 0x00ADEECC
float World::s_horizonFarClipScale = 1.0f;

// STATIC: 0x00ADEED0
float World::s_horizonNearClipScale = 0.7f;

// STATIC: 0x00ADEED4
float World::s_nearClip = 0.1f;

// STATIC: 0x00CD7734
static CM2Model* s_unk_CD7734;

// STATIC: 0x00CD7738
static CM2Model* s_unk_CD7738;

// STATIC: 0x00CD773C
int32_t World::s_detailDoodadDensity;

// STATIC: 0x00CD7744
float World::s_prevFarClip;

// STATIC: 0x00CD7748
float World::s_farClip;

// STATIC: 0x00ADEEE0
int32_t World::s_texCacheDirty = 1;

// STATIC: 0x00CD7760
uint32_t World::s_texCacheOverride;

// STATIC: 0x00CDFFD0
int32_t World::s_waterLOD;

// STATIC: 0x00CD8790
float s_viewerLiquidDepth;


// STATIC: 0x00CD8798
int32_t CWorldScene::frustumIndex;

// STATIC: 0x00CDB108
C3Vector CWorldScene::s_frustumCorners[8];

// STATIC: 0x00CDB168
CWFrustum CWorldScene::frustumStack[32];

// STATIC: 0x00CD8F90
C4Plane CWorldScene::camPlaneXY;

// STATIC: 0x00CD9048
CSortTable CWorldScene::sortTable;

// STATIC: 0x00CDFFF4
CALLBACK_FUNC World::s_loadProgressCallback;

// STATIC: 0x00CDFFF0
void* World::s_loadProgressParam;

// STATIC: 0x00CDD0E8
TSGrowableArray<CPortalView> CWorldScene::s_exteriorPortalViews;

// STATIC: 0x00CDD0F8
TSGrowableArray<CPortalView> CWorldScene::s_portalViews;

// STATIC: 0x00CD7558
static CImVector s_shadowModPixels[64];

// STATIC: 0x00CD7778
C3Vector World::s_currentWorldPos;

// STATIC: 0x00ADF460
C44Matrix CWorldScene::s_clipBufferMatrix;

// STATIC: 0x00ADF4A0
CBarrier CWorldScene::s_barrier;

// STATIC: 0x00ADF628
C44Matrix CWorldScene::projMatrix;

// STATIC: 0x00CD872C
uint32_t CWorldScene::nDoodadsRendered;

// STATIC: 0x00CD8938
float CWorldScene::s_clipBuffer[384];

// STATIC: 0x00CD8F74
C3Vector CWorldScene::camVec;

// STATIC: 0x00CD8F80
C4Plane CWorldScene::camPlane;

// STATIC: 0x00ADF570
CPortalView CWorldScene::s_unk_ADF570;

// STATIC: 0x00ADF58C
CPortalView CWorldScene::frustumPortalView;

// STATIC: 0x00ADF5A8
C44Matrix CWorldScene::s_viewProjMatrix;

// STATIC: 0x00ADF5E8
C44Matrix CWorldScene::viewMatrix;

// STATIC: 0x00CD861C
char* CWorldScene::s_interiorSkybox;

// STATIC: 0x00CD8620
int32_t CWorldScene::s_unk_CD8620;

// STATIC: 0x00CD87A4
CMapObjDef* CWorldScene::s_viewerMapObjDef;

// STATIC: 0x00CD87AC
float CWorldScene::s_lodFarClip;

// STATIC: 0x00CD8F44
CAaBox CWorldScene::boundingBox;

// STATIC: 0x00CD8FB0
CBoundingBox CWorldScene::viewPort;

// STATIC: 0x00CD8FC8
C4Vector CWorldScene::s_viewProjW;

// STATIC: 0x00CDB0D4
TSGrowableArray<uint32_t> CWorldScene::s_viewerMapObjGroups;

// STATIC: 0x00CDB0F4
CiRect CWorldScene::s_frustumChunkRect;

// STATIC: 0x00CDD108
CFrustum CWorldScene::s_clipFrustum;

// STATIC: 0x00CDD200
static CWFrustum unk_CDD200;

// STATIC: 0x00CD8F40
static int32_t unk_CD8F40;

// STATIC: 0x00CD8778
int32_t CWorldScene::s_hasPendingExteriorGroups;

// STATIC: 0x00ADF3F0
int32_t s_entityCanLink;

// STATIC: 0x00CD7770
WOWGUID s_highlightedObjectGuid;

// STATIC: 0x00CD8770
uint32_t CWorldScene::s_chunksRendered;

// STATIC: 0x00CD8780
float CWorldScene::s_occlusionNearDepth;

// STATIC: 0x00CD8784
float CWorldScene::s_occlusionFarDepth;

// STATIC: 0x00CD87B8
uint8_t CWorldScene::s_clipBufferMask[384];

// STATIC: 0x00CD87A0
CMapObjDef* CWorldScene::s_viewerMovedMapObjDef;

// STATIC: 0x00CDB0E4
TSGrowableArray<uint32_t> CWorldScene::s_viewerMovedMapObjGroups;

// STATIC: 0x00CD8628
static char s_debugMapName[260];

// STATIC: 0x00CD8730
static char s_debugMapChunk[64];

// STATIC: 0x00ADF6B4
TSList<void, TSGetExplicitLink<void> > CWorldScene::s_unkList9FreeList = TSExplicitList<void, 8>();

// STATIC: 0x00ADF530
C44Matrix CWorldScene::camTransportView;

// STATIC: 0x00CE04B0
int32_t (*World::s_objectCollisionCallback)(WOWGUID* guid, uintptr_t param, World::ObjectCollisionData* data, uint32_t flags);

// STATIC: 0x00ADEE74
static const char* s_invisibleStalkerModel = "InvisibleStalker.m2";

// STATIC: 0x00ADF4F0
C44Matrix CWorldScene::camTransportMtx;

// STATIC: 0x00CD8788
WOWGUID CWorldScene::camTransportGUID;

// STATIC: 0x00CDD380
static C3Vector* s_clipInVerts[32];

// STATIC: 0x00CDD300
static C3Vector* s_clipOutVerts[32];

// STATIC: 0x00ADEEE8
static float s_horizonMinZ = 0.998046875f;

// STATIC: 0x00ADEEEC
static float s_horizonMaxZ = 0.9990234375f;

static TSGrowableArray<ZoneLight> s_zoneLights;

// VERIFY: 8564474b manual
// OFFSET: 0x005EEB70
void World::SetTexLodBias(float bias) {
}

// VERIFY: 64ab1743 auto
// OFFSET: 0x0077EC80
bool World::MapIsDungeon() {
    return CMap::bDungeon != 0;
}

// VERIFY: 3a6ff55e auto
// OFFSET: 0x0077EC90
void World::SetLoadProgressCallback(CALLBACK_FUNC callback, void* param) {
    World::s_loadProgressCallback = callback;
    World::s_loadProgressParam = param;
}

// VERIFY: e5c4a380 manual
// OFFSET: 0x0077ED40
// Aleist3r: this rework is kinda sloppily coded but w/e
static void ZonesInitialize()
{
    if (!g_zoneLightDB.m_loaded || !g_zoneLightDB.m_numRecords || !g_zoneLightPointDB.m_loaded || !g_zoneLightPointDB.m_numRecords)
    {
        return;
    }

    if (!s_zoneLights.Count())
    {
        s_zoneLights.SetCount(g_zoneLightDB.m_numRecords);
    }

    for (uint32_t i = 0; i < g_zoneLightDB.m_numRecords; i++)
    {
        ZoneLightRec& recA = g_zoneLightDB.m_records[i];
        ZoneLight& light = s_zoneLights.m_data[i];

        for (uint32_t j = 0; j < g_zoneLightPointDB.m_numRecords; j++)
        {
            if (g_zoneLightPointDB.m_records[j].m_zoneLightID == recA.m_id)
            {
                light.m_pointCount++;
            }
        }

        if (!light.m_pointCount)
        {
            continue;
        }

        light.m_mapID = recA.m_mapID;
        light.m_lightID = recA.m_lightID;
        light.m_points = static_cast<C2Vector*>(SMemAlloc(sizeof(C2Vector) * light.m_pointCount, __FILE__, __LINE__, 0));

        uint32_t currentCount = 1;

        for (uint32_t k = 0; k < g_zoneLightPointDB.m_numRecords; k++)
        {
            ZoneLightPointRec& recB = g_zoneLightPointDB.m_records[k];

            if (recB.m_zoneLightID == recA.m_id && recB.m_pointOrder == currentCount)
            {
                float tempX = -(recB.m_positionY - 17066.666f);
                float tempY = -(recB.m_positionX - 17066.666f);

                light.m_points[currentCount - 1].x = tempX;
                light.m_points[currentCount - 1].y = tempY;

                if (light.m_minX > tempX)
                {
                    light.m_minX = tempX;
                }

                if (light.m_minY > tempY)
                {
                    light.m_minY = tempY;
                }

                if (light.m_maxX < tempX)
                {
                    light.m_maxX = tempX;
                }

                if (light.m_maxY < tempY)
                {
                    light.m_maxY = tempY;
                }

                currentCount++;
            }

            if (currentCount >= light.m_pointCount)
            {
                break;
            }
        }

        light.m_minX -= 50.0f;
        light.m_minY -= 50.0f;
        light.m_maxX += 50.0f;
        light.m_maxY += 50.0f;
    }
}

// VERIFY: 5f002705 manual
// OFFSET: 0x0077EED0
void DayNight::FindAndAddZoneLight(const C3Vector* position)
{
    DNInfo* info = DayNight::GetInfo();

    if (!info)
    {
        return;
    }

    C2Vector point(-(position->y - HALF_WORLD_SIZE), -(position->x - HALF_WORLD_SIZE));

    for (uint32_t i = 0; i < s_zoneLights.Count(); i++)
    {
        const ZoneLight& zoneLight = s_zoneLights.m_data[i];

        if (zoneLight.m_mapID != s_mapId || zoneLight.m_minX > point.x || point.y < zoneLight.m_minY || zoneLight.m_maxX < point.x || point.y > zoneLight.m_maxY) {
            continue;
        }

        float distance = FLT_MAX;

        if (CWorldMath::PointInPolygon(zoneLight.m_pointCount, zoneLight.m_points, point, &distance)) {
            distance = -distance;
        }

        distance -= 50.0f;

        if (distance < 0.0f) {
            info->AddZoneLight(zoneLight.m_lightID, -distance);
        }
    }
}

// VERIFY: 4cdcc272 auto
// OFFSET: 0x0077F1E0
int32_t World::QueryObjectLiquid(CMapEntity* entity, int32_t* liquidType, float* height, int32_t* a4) {
    if (!entity || !(entity->m_staticFlags & 0x20)) {
        return 0;
    }

    *height = entity->m_liquidHeight;
    *liquidType = entity->m_liquidType;
    *a4 = 0;

    return 1;
}

// VERIFY: 52533b77 auto
// OFFSET: 0x0077F220
int32_t World::QueryObjectLiquidFlags(CMapEntity* entity, int32_t* flag8, int32_t* flag9) {
    if (!entity || !(entity->m_staticFlags & 0x20)) {
        return 0;
    }

    *flag8 = (entity->m_staticFlags >> 8) & 1;
    *flag9 = (entity->m_staticFlags >> 9) & 1;

    return 1;
}

// VERIFY: a72ff48f auto
// OFFSET: 0x0077F290
void World::FindOrCreateDestructibleProxy(CDestructibleProxy* proxy) {
    CDestructibleProxy::FindOrCreate(proxy);
}

// VERIFY: 529917d1 auto
// OFFSET: 0x0077F2A0
void World::FreeDestructibleProxy(CDestructibleProxy* proxy) {
    CDestructibleProxy::FreeMatching(proxy);
}

// VERIFY: f47c89de auto
// OFFSET: 0x0077F2B0
void World::SetObjectCollisionCallback(int32_t (*callback)(WOWGUID* guid, uintptr_t param, ObjectCollisionData* data, uint32_t flags)) {
    World::s_objectCollisionCallback = callback;
}

// VERIFY: 305f137e auto
// OFFSET: 0x0077F360
int32_t World::QueryLiquidStatus(C3Vector* pos, int32_t* liquidType, float* height) {
    int32_t a4 = 0;
    return CMap::QueryLiquidStatus(pos, liquidType, height, &a4, 0);
}

// VERIFY: 46023465 auto
// OFFSET: 0x0077F390
int32_t World::QueryLiquidSounds(CMapEntity* entity, float radius, TSGrowableArray<World::LiquidQueryResult>& results) {
    if (entity->m_staticFlags & 0x1) {
        uint32_t distance = MAX_SOUND_EXT_LEVEL;
        entity->QueryLiquidSounds(results, &distance);

        if (distance < MIN_SOUND_EXT_LEVEL) {
            CMap::QueryLiquidSounds(&entity->m_position, radius, results);
        }

        return 1;
    }

    CMap::QueryLiquidSounds(&entity->m_position, radius, results);

    return 1;
}

// VERIFY: baf0a035 auto
// OFFSET: 0x0077F400
void World::CreateWaterRipple(const C3Vector& pos, float a2, float a3, float a4, float a5, float a6, int32_t a7, int32_t a8) {
    CMap::CreateWaterRipple(pos, a2, a3, a4, a5, a6, a7, a8);
}

// VERIFY: e146f1dc auto
// OFFSET: 0x0077F490
void World::SetNearClip(float nearClip) {
    World::s_nearClip = 0.2f;
}

// VERIFY: 632775d6 auto
// OFFSET: 0x0077F4A0
void World::SetHorizonFarClipScale(float scale) {
    World::s_horizonFarClipScale = scale;
}

// VERIFY: 28a08d04 auto
// OFFSET: 0x0077F4B0
void World::SetHorizonNearClipScale(float scale) {
    World::s_horizonNearClipScale = scale;
}

// VERIFY: 32ac5266 manual
// OFFSET: 0x0077F4C0
static void UpdateShadowGxTex(EGxTexCommand command, uint32_t width, uint32_t height, uint32_t face, uint32_t level, void* userArg, uint32_t& texelStrideInBytes, const void*& texels) {
    switch (command) {
        case GxTex_Latch:
            texelStrideInBytes = 4 * width;
            texels = userArg;
            break;

        default:
            break;
    }
}

// VERIFY: 7e2d825d manual
// OFFSET: 0x0077F900
void CWorld::UpdateTextureCacheBudget() {
    if (!World::s_texCacheDirty || World::s_texCacheOverride) {
        return;
    }

    World::s_texCacheDirty = 0;

    uint32_t baseMip = g_theGxDevicePtr->DeviceBaseMipLevel();
    int32_t farClip = static_cast<int32_t>(World::s_farClip);
    int32_t megabytes;

    if (baseMip) {
        megabytes = farClip <= 177 ? 16 : 32;
    } else {
        megabytes = farClip <= 727 ? 64 : 128;
    }

    g_theGxDevicePtr->TexSetCacheSize(megabytes << 20);
}

// VERIFY: 27c4601b manual
// OFFSET: 0x0077FB90
int32_t World::QueryCameraFog(SMOFog* fog, CMapObjDef** mapObjDef, uint8_t* found, TSGrowableArray<uint32_t>** groups, float* distance) {
    CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPE_PLAYER, __FILE__, __LINE__));

    if (player && (player->m_player->m_playerFlags & 0x10)) {
        return 0;
    }

    *distance = FLT_MAX;
    return CMapEntity::QueryCameraFog(fog, mapObjDef, found, groups, distance);
}

// VERIFY: 7db3332e auto
// OFFSET: 0x00780620
int32_t World::SceneCamLiquidStatus(float* depth) {
    if (depth) {
        *depth = s_viewerLiquidDepth;
    }

    return s_liquidTypeId;
}

// VERIFY: ac1fbe8d auto
// OFFSET: 0x0077EFF0
void CWorld::Render(const C3Vector& cameraPos, float time) {
    CWorldScene::Render(cameraPos, time);
    CheckAndSendBotDetected();
}

// VERIFY: 3fc52c12 auto
// OFFSET: 0x0077F090
int32_t World::QueryObjectInside(CMapEntity* entity) {
    if (!entity) {
        return 0;
    }

    return entity->QueryInterior();
}

// VERIFY: a990e60d auto
// OFFSET: 0x0077F9A0
void CBarrier::SetViewerPos(const C3Vector& position) {
    CWorldScene::s_barrier.m_viewerPos = position;
}

// VERIFY: b95e4db0 auto
// OFFSET: 0x0077F9D0
void CWorldScene::RenderParticulate() {
    if ((CWorld::enables & Enable_Particulates) && s_liquidTypeId) {
        CWorld::particulate->Render();
    }
}

// VERIFY: 623133e8 auto
// OFFSET: 0x0077FCD0
int32_t World::ObjectIsDrawable(CMapBaseObj* object) {
    if (object->m_type & 0x8) {
        return static_cast<CMapObjDef*>(object)->IsAllDrawable();
    }

    if (!(object->m_type & 0x40)) {
        return 0;
    }

    CM2Model* model = static_cast<CMapStaticEntity*>(object)->m_model;

    if (!model) {
        return 1;
    }

    return model->IsDrawable(0, 0);
}

// VERIFY: 86d43bac auto
// OFFSET: 0x0077FD10
int32_t World::ObjectIsLoaded(CMapBaseObj* object) {
    if (object->m_type & 0x8) {
        CMapObjDef* mapObjDef = static_cast<CMapObjDef*>(object);

        if (!mapObjDef->IsAllLoaded()) {
            return 0;
        }

        return mapObjDef->m_owner->IsAllLoaded();
    }

    if (!(object->m_type & 0x40)) {
        return 0;
    }

    CM2Model* model = static_cast<CMapStaticEntity*>(object)->m_model;

    if (!model) {
        return 1;
    }

    return model->IsLoaded(0, 0);
}

// VERIFY: d8d20c5a auto
// OFFSET: 0x0077F340
bool World::GetTris(const CAaBox& box, World::TriData* triData, uint32_t flags) {
    return CMap::QueryTris(box, triData, flags);
}

// VERIFY: 457f7c03 auto
// OFFSET: 0x0077F350
uint32_t World::GetIntersectingDoodadModels(CM2Model** models, uint32_t maxModels, const CAaBox& box, uint32_t flags) {
    return CMap::QueryIntersectingModels(models, maxModels, box, flags);
}

// VERIFY: fbdca5c7 auto
// OFFSET: 0x0077F500
void World::ProjectTextureCallback(const CAaBox& box, const CImVector& color, int32_t index, void* param, int32_t flags) {
    if (flags || (CWorld::enables2 & 0x4)) {
        ProjectTex2dDrawShaderEffect(box, color, 0x200122, index ? 2 : 0, 0.40000001f);
    }
}

// VERIFY: 7b66e8de auto
// OFFSET: 0x0077FE20
void World::ObjectSetAlwaysExterior(CMapBaseObj* object, int32_t enable) {
    if (enable) {
        object->m_flags = static_cast<MapObjFlag>(object->m_flags | MAPOBJ_FLAG_NO_MAPOBJ_LINK);
    } else {
        object->m_flags = static_cast<MapObjFlag>(object->m_flags & ~MAPOBJ_FLAG_NO_MAPOBJ_LINK);
    }
}

// VERIFY: 4870d850 manual
// OFFSET: 0x0077FE40
int32_t World::ObjectMoveDoodads(CMapBaseObj* source, uint16_t doodadSet, CMapBaseObj* target, uint16_t targetDoodadSet) {
    if (!(source->m_type & 0x8) || !(source->m_flags & MAPOBJ_FLAG_PREPARED) || !(target->m_type & 0x8) || !(target->m_flags & MAPOBJ_FLAG_PREPARED)) {
        return 0;
    }

    return CMapObjDef::MoveDoodadDefs(static_cast<CMapObjDef*>(source), doodadSet, static_cast<CMapObjDef*>(target), targetDoodadSet);
}

// VERIFY: dcda60dd auto
// OFFSET: 0x0077FE80
void World::ObjectSetDoodadEmittersIgnoresDistance(CMapBaseObj* object, int32_t ignore, uint16_t doodadSet) {
    if (object->m_type & 0x8) {
        CMapObjDef::SetDoodadEmittersIgnoresDistance(static_cast<CMapObjDef*>(object), ignore, doodadSet);
    }
}

// VERIFY: 35100e77 auto
// OFFSET: 0x0077FEA0
void World::ObjectSetDoodadsEnabled(CMapBaseObj* object, int32_t enable, uint16_t doodadSet) {
    if (object->m_type & 0x8) {
        CMapObjDef::SetDoodadsEnabled(static_cast<CMapObjDef*>(object), enable, doodadSet);
    }
}

// VERIFY: 2c7a8fa7 auto
// OFFSET: 0x0077FEC0
void World::ObjectSetSequence(CMapEntity* entity, uint32_t sequence, int32_t a3, int32_t doodadSet) {
    if (entity->m_type & 0x8) {
        reinterpret_cast<CMapObjDef*>(entity)->SetSequence(sequence, a3, static_cast<uint16_t>(doodadSet));
    } else if (entity->m_type & 0x40) {
        if (entity->m_model) {
            entity->m_model->SetBoneSequence(0xFFFFFFFF, sequence, 0xFFFFFFFF, 0, 1.0f, 1, 1);
        }
    }
}

// VERIFY: e33527b6 auto
// OFFSET: 0x0077FF60
void World::ObjectSetEventCallback(CMapBaseObj* object, void (*callback)(CM2Model* model, uint32_t boneId, uint32_t eventId, uint32_t data, const C3Vector* position, uint32_t time, uint64_t param), uint64_t param, uint16_t doodadSet) {
    if (object->m_type & 0x8) {
        static_cast<CMapObjDef*>(object)->SetEventCallback(callback, param, doodadSet);
    } else if (object->m_type & 0x40) {
        CM2Model* model = static_cast<CMapDoodadDef*>(object)->m_model;

        if (model) {
            model->SetEventCallback(reinterpret_cast<void*>(callback), param);
        }
    }
}

// VERIFY: 341fb3f3 auto
// OFFSET: 0x00780130
void World::ObjectGetExtents(CMapBaseObj* object, CAaBox* extents) {
    if (object->m_type & 0x8) {
        static_cast<CMapObjDef*>(object)->m_owner->GetBounds(extents);
    } else if (object->m_type & 0x40) {
        *extents = static_cast<CMapDoodadDef*>(object)->m_model->GetBoundingBox();
    }
}

// VERIFY: aa9498a7 auto
// OFFSET: 0x00780190
int32_t World::ObjectIsWMO(CMapBaseObj* object) {
    return object->m_type & 0x8;
}

// VERIFY: 740254e1 auto
// OFFSET: 0x007801A0
void World::ObjectEnableCollision(CMapBaseObj* object, int32_t enable) {
    if (enable) {
        object->m_flags = static_cast<MapObjFlag>(object->m_flags & ~MAPOBJ_FLAG_NO_HITTEST);
    } else {
        object->m_flags = static_cast<MapObjFlag>(object->m_flags | MAPOBJ_FLAG_NO_HITTEST);
    }
}

// VERIFY: b8f47c0c auto
// OFFSET: 0x007801C0
void World::ObjectSetShadow(CMapEntity* entity, uint32_t shadow, uint32_t mask) {
    if (mask & 0x1) {
        entity->m_staticFlags ^= (entity->m_staticFlags ^ (shadow << 1)) & 0x2;
    }

    if (mask & 0x2) {
        entity->m_staticFlags ^= (entity->m_staticFlags ^ ~(shadow << 10)) & 0x800;
    }

    if (mask & 0x4) {
        entity->m_staticFlags ^= (entity->m_staticFlags ^ (shadow << 8)) & 0x400;
    }

    if (mask & 0x8) {
        entity->m_staticFlags ^= (entity->m_staticFlags ^ (shadow << 10)) & 0x2000;
    }

    if (mask & 0x20) {
        if (shadow & 0x20) {
            entity->m_flags = static_cast<MapObjFlag>(entity->m_flags | MAPOBJ_FLAG_SHADOW_20000);
        } else {
            entity->m_flags = static_cast<MapObjFlag>(entity->m_flags & ~MAPOBJ_FLAG_SHADOW_20000);
        }
    }
}

// VERIFY: 4aa10153 manual
// OFFSET: 0x00780240
void CMap::ObjectUpdate(CMapEntity* entity, C44Matrix& mat, CAaBox& box, CAaSphere& sphere, C3Vector& vec, bool a6, uint32_t a7) {
    C3Vector position = { mat.d0, mat.d1, mat.d2 };
    float scale = sqrtf(mat.a2 * mat.a2 + (mat.a1 * mat.a1 + mat.a0 * mat.a0));
    C3Vector vec2 = vec * mat;
    float radius = 0.0f;
    CAaBox bbox(position, position);
    C3Vector center = position;

    if (sphere.r > 0.001f) {
        radius = scale * sphere.r;
        center = sphere.c * mat;
    }

    if (box.t.x > box.b.x && box.t.y > box.b.y && box.t.z > box.b.z) {
        bbox = CAaBox::Transform(box, mat);
    }

    C3Vector positionDelta = entity->m_position - position;
    C3Vector vec2Delta = entity->m_vec2 - vec2;
    C3Vector bottomDelta = entity->m_bbox.b - bbox.b;
    C3Vector topDelta = entity->m_bbox.t - bbox.t;
    C3Vector centerDelta = entity->m_sphere.c - center;
    float radiusDelta = entity->m_sphere.r - radius;

    entity->m_position = position;
    entity->m_vec2 = vec2;
    entity->m_bbox = bbox;
    entity->m_scale = scale;
    entity->m_sphere.c = center;
    entity->m_sphere.r = radius;
    entity->unk_00A4 = a7;

    if (positionDelta.SquaredMag() > 0.000001f || vec2Delta.SquaredMag() > 0.0001f || bottomDelta.SquaredMag() > 0.0001f || topDelta.SquaredMag() > 0.0001f || centerDelta.SquaredMag() > 0.0001f || radiusDelta > 0.0001f) {
        if (!a6) {
            CMap::UpdateEntity(entity);
        }
    }
}

// VERIFY: d0b5cfbd manual
// OFFSET: 0x00781500
void World::Preload(const C3Vector& position, int32_t showLoading) {
    World::PrepareAreaOfInterest(position);
    s_preloadX = 3.4028235e38f;
    s_preloadY = 3.4028235e38f;
    CWorld::gbChunkRectOld = CWorld::gbChunkRect;
    s_preloadCount = 0;

    if (showLoading) {
        World::s_loadProgressCallback = LoadingScreenWorldCallback;
        World::s_loadProgressParam = nullptr;
        AsyncFileReadSetProgressCallback(LoadingScreenAsyncCallback, nullptr);
        LoadingScreenEnable(s_mapId, 1);
    }

    CMap::Preload();

    if (showLoading) {
        World::s_loadProgressCallback = nullptr;
        World::s_loadProgressParam = nullptr;
        AsyncFileReadSetProgressCallback(nullptr, nullptr);
        LoadingScreenDisable();
    }
}

// VERIFY: a7db51a8 manual
// OFFSET: 0x007815C0
void World::Update(const C3Vector& position) {
    CWorld::gbChunkRectOld = CWorld::gbChunkRect;
    World::PrepareAreaOfInterest(position);
    CMap::PrepareUpdate(false);
}

// VERIFY: 6f30b112 auto
// OFFSET: 0x007819C0
uint64_t World::QueryObjectParentParam64(CMapBaseObj* object) {
    CMapBaseObjLink* link = object->m_parentLinkList.Head();

    if (!link) {
        return 0;
    }

    CMapBaseObj* parent = link->m_ref;

    if (!parent || !(parent->m_type & 0x10)) {
        return 0;
    }

    return static_cast<CMapObjDef*>(parent->m_parentLinkList.Head()->m_ref)->m_objectGuid.m_guid;
}

// VERIFY: 75ab3dc4 manual
// OFFSET: 0x00781A10
CMapEntity* CMap::ObjectCreate(CM2Model* model, MAP_OBJECT_FUNC func, void* funcParam, uint64_t param64, uintptr_t param32, uint32_t flags) {
    CMapEntity* entity = CMap::AllocEntity((flags >> 3) & 0x1);
    entity->m_model = model;
    entity->m_funcParam64 = param64;
    entity->m_funcParam32 = param32;
    entity->m_position = C3Vector(10000000.0f, 10000000.0f, 10000000.0f);
    entity->m_diffuseLightScale = 1.0f;
    entity->m_vec2 = C3Vector(10000000.0f, 10000000.0f, 10000000.0f);
    entity->m_targetDiffuseLightScale = 1.0f;
    entity->m_type |= 0x200;
    entity->m_staticFlags = (entity->m_staticFlags & ~0xEC02) | ((flags & 0x1) << 1) | ((flags & 0x4) << 8) | (~(flags << 10) & 0x800) | (((flags >> 3) & 0x1) << 13) | ((flags & 0x10) << 11);
    entity->m_flags = static_cast<MapObjFlag>(0);
    entity->m_func = nullptr;
    entity->unused_00B4 = 0;

    if (flags & 0x20) {
        entity->m_flags = static_cast<MapObjFlag>(0x20000);
    }

    C3Vector ambient = CMap::s_mapLight->m_light.m_ambColor;
    CImVector color;
    color.b = static_cast<uint8_t>(static_cast<int32_t>(ambient.z > 0.0f ? (ambient.z < 1.0f ? ambient.z * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.g = static_cast<uint8_t>(static_cast<int32_t>(ambient.y > 0.0f ? (ambient.y < 1.0f ? ambient.y * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.r = static_cast<uint8_t>(static_cast<int32_t>(ambient.x > 0.0f ? (ambient.x < 1.0f ? ambient.x * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.a = 0xFF;
    entity->m_targetAmbientColor = color;
    entity->m_m2AmbientColor = color;

    if (entity->m_model) {
        if (!SStrCmpI(s_invisibleStalkerModel, model->m_shared->m_fileNameWithoutPath, STORM_MAX_STR)) {
            entity->m_staticFlags |= 0x4000;
        }

        entity->m_model->m_lightingCallback = CMapStaticEntity::ModelLightingCallback;
        entity->m_model->m_lightingArg = entity;
        entity->m_model->m_refCount++;
    }

    entity->m_func = func;
    entity->m_funcParam = funcParam;
    return entity;
}

// VERIFY: 6292bfdd auto
// OFFSET: 0x00782350
void World::ObjectSetModel(CMapEntity* entity, CM2Model* model) {
    if (entity->m_model) {
        entity->m_model->m_lightingCallback = nullptr;
        entity->m_model->m_lightingArg = nullptr;
        entity->m_model->Release();
    }

    entity->m_staticFlags &= ~0x4000;
    entity->m_model = model;

    if (!model) {
        return;
    }

    if (!SStrCmpI(s_invisibleStalkerModel, model->m_shared->m_fileNameWithoutPath, STORM_MAX_STR)) {
        entity->m_staticFlags |= 0x4000;
    }

    entity->m_model->m_lightingCallback = CMapStaticEntity::ModelLightingCallback;
    entity->m_model->m_lightingArg = entity;
    entity->m_model->m_refCount++;
}

// VERIFY: f8350d49 manual
// OFFSET: 0x00782680
void CWorld::ObjectDestroy(CMapBaseObj* object) {
    CMapBaseObjLink* link = object->m_parentLinkList.Head();

    while (link) {
        CMapBaseObjLink* next = object->m_parentLinkList.Next(link);
        CMap::FreeBaseObjLink(link);
        link = next;
    }

    uint16_t type = object->m_type;
    object->m_refCount--;

    if (type & 0x8) {
        CMap::PurgeMapObjDef(static_cast<CMapObjDef*>(object));
    } else if (type & 0x40) {
        CMapDoodadDef::Purge(static_cast<CMapDoodadDef*>(object));
    }
}

// VERIFY: cc711ea7 manual
// OFFSET: 0x007826E0
void World::ObjectDestroy(CMapEntity* entity) {
    CMapBaseObjLink* link = entity->m_parentLinkList.Head();

    while (link) {
        CMapBaseObjLink* next = entity->m_parentLinkList.Next(link);
        CMap::FreeBaseObjLink(link);
        link = next;
    }

    if (entity->m_model && entity->m_model->m_attachParent) {
        entity->m_model->DetachFromParent();
    }

    CMap::FreeEntity(entity);
}

// VERIFY: a4bfc150 manual
// OFFSET: 0x00780860
void World::PrepareAreaOfInterest(const C3Vector& position) {
    World::s_currentWorldPos = position;

    CGCamera* camera = CGWorldFrame::GetActiveCamera();
    float farZ = camera->m_farZ;
    float nearZ = camera->m_nearZ;
    float fov = camera->FOV();

    C44Matrix projection;
    GxuXformCreateProjection_Exact(fov * 0.6f, camera->m_aspect, nearZ, farZ, projection);

    C44Matrix view;
    GxXformView(view);

    C3Vector corners[8];
    GxuXformCalcFrustumCorners(view, projection, corners);

    float distance = sqrtf(corners[7].z * corners[7].z + corners[7].y * corners[7].y + corners[7].x * corners[7].x);
    float farClip = s_farClip;
    float streamDistance;

    if (farClip > distance) {
        streamDistance = farClip * 1.25f;
    } else if (farClip + farClip < distance) {
        streamDistance = farClip + farClip;
    } else {
        streamDistance = distance;
    }

    CWorld::s_groupAreaOfInterest.b = C3Vector(position.x - 150.0f, position.y - 150.0f, position.z - 150.0f);
    CWorld::s_groupAreaOfInterest.t = C3Vector(position.x + 150.0f, position.y + 150.0f, position.z + 150.0f);
    CWorld::s_objectAreaOfInterest.b = C3Vector(position.x - farClip, position.y - farClip, position.z - farClip);
    CWorld::s_objectAreaOfInterest.t = C3Vector(position.x + farClip, position.y + farClip, position.z + farClip);

    int32_t radius = 1 - static_cast<int32_t>(streamDistance * -OO_COORD_TO_CHUNK);
    int32_t chunkX = static_cast<int32_t>(-(position.x - HALF_WORLD_SIZE) * OO_COORD_TO_CHUNK);
    int32_t chunkY = static_cast<int32_t>(-(position.y - HALF_WORLD_SIZE) * OO_COORD_TO_CHUNK);

    if (SFile::IsTrial()) {
        StreamingSetAreaOfInterest(chunkY, chunkX);
    }

    CWorld::chunkRect.minX = (chunkY - radius) & ~1;
    CWorld::chunkRect.maxX = ((chunkY + radius) & ~1) + 1;
    CWorld::chunkRect.minY = (chunkX - radius) & ~1;
    CWorld::chunkRect.maxY = ((chunkX + radius) & ~1) + 1;

    CWorld::gbChunkRect.minX = CWorld::chunkRect.minX - 2;
    CWorld::gbChunkRect.minY = CWorld::chunkRect.minY - 2;
    CWorld::gbChunkRect.maxY = CWorld::chunkRect.maxY + 2;
    CWorld::gbChunkRect.maxX = CWorld::chunkRect.maxX + 2;

    if (CWorld::gbChunkRect.maxX - chunkY < 8) {
        int32_t grow = 8 - (CWorld::gbChunkRect.maxX - chunkY);
        CWorld::gbChunkRect.minY = (CWorld::gbChunkRect.minY - grow) & ~1;
        CWorld::gbChunkRect.maxX = ((CWorld::gbChunkRect.maxX + grow) & ~1) + 1;
        CWorld::gbChunkRect.maxY = ((CWorld::gbChunkRect.maxY + grow) & ~1) + 1;
        CWorld::gbChunkRect.minX = (CWorld::gbChunkRect.minX - grow) & ~1;
    }

    if (CWorld::chunkRect.minX < 0) {
        CWorld::chunkRect.minX = 0;
    }

    if (CWorld::chunkRect.maxX >= 1024) {
        CWorld::chunkRect.maxX = 1023;
    }

    if (CWorld::chunkRect.minY < 0) {
        CWorld::chunkRect.minY = 0;
    }

    if (CWorld::chunkRect.maxY >= 1024) {
        CWorld::chunkRect.maxY = 1023;
    }

    if (CWorld::gbChunkRect.minX < 0) {
        CWorld::gbChunkRect.minX = 0;
    }

    if (CWorld::gbChunkRect.maxX >= 1024) {
        CWorld::gbChunkRect.maxX = 1023;
    }

    if (CWorld::gbChunkRect.minY < 0) {
        CWorld::gbChunkRect.minY = 0;
    }

    if (CWorld::gbChunkRect.maxY >= 1024) {
        CWorld::gbChunkRect.maxY = 1023;
    }
}

// VERIFY: fa04a252 auto
// OFFSET: 0x00780640
C3Vector* World::GetCamPos() {
    return &CWorldScene::s_activeWorldView;
}

// VERIFY: 21d7eb14 auto
// OFFSET: 0x00780650
C3Vector* CWorldScene::GetCamTarget() {
    return &CWorldScene::camTarget;
}

// VERIFY: 8e7667d1 auto
// OFFSET: 0x00780660
void CWorld::SetShadowColor(const CImVector& color) {
    if (CWorld::shadowColor != *reinterpret_cast<const uint32_t*>(&color)) {
        for (CImVector* pixel = s_shadowModPixels; pixel < s_shadowModPixels + 64; pixel++) {
            *pixel = color;
        }

        if (!CWorld::shadowModTexture) {
            CWorld::shadowModTexture = TextureCreate(GxTex_2d, 8, 8, 0, GxTex_Argb4444, GxTex_Argb8888, CGxTexFlags(), s_shadowModPixels, UpdateShadowGxTex, "CWorld::shadowMod", 0);
        }

        GxTexUpdate(TextureGetGxTex(CWorld::shadowModTexture, 1, nullptr), 0, 0, 8, 8, 1);
    }

    CWorld::shadowColor = *reinterpret_cast<const uint32_t*>(&color);
}

// VERIFY: bf7ac74d auto
// OFFSET: 0x00780710
void World::SetDetailDoodadDensity(int32_t density) {
    if (World::s_detailDoodadDensity != density) {
        World::s_detailDoodadDensity = density;
        CDetailDoodad::s_bPoolsDirty = 1;
    }
}

// VERIFY: 542dd26b auto
// OFFSET: 0x00780730
void World::SetDetailDoodadDist(float dist) {
    if (dist != CWorld::detailDoodadDist) {
        CWorld::detailDoodadDist = dist;
        CDetailDoodad::s_bPoolsDirty = 1;
        World::groundEffectDistValueSqr = dist * dist;
    }
}

// VERIFY: 5f5281b3 auto
// OFFSET: 0x00780770
float World::ValidateFarClip(float farClip, int32_t mapID) {
    float maxFarClip = 4750.0f / 3.0f;

    if (mapID < 530 || mapID == 575 || mapID == 543) {
        if (!CWorldParam::cvar_farClipOverride || CWorldParam::cvar_farClipOverride->m_intValue < 1) {
            maxFarClip = 2375.0f / 3.0f;
        }
    } else if (OsGetPhysicalMemory() <= 0x40000000) {
        maxFarClip = 2375.0f / 3.0f;
    }

    if (farClip < 550.0f / 3.0f) {
        return 550.0f / 3.0f;
    }

    if (farClip <= maxFarClip) {
        return farClip;
    }

    return maxFarClip;
}

// VERIFY: 1f595043 auto
// OFFSET: 0x00780800
void World::SetFarClip(float farClip) {
    float validated = World::ValidateFarClip(farClip, s_mapId);

    if (World::s_farClip != validated) {
        World::s_prevFarClip = World::s_farClip;
        World::s_farClip = validated;
        CMapRenderChunk::SetDirty();
        World::s_nearClip = 0.2f;
        CMapObj::bPoolsDirty = 1;
        World::s_texCacheDirty = 1;
    }
}

// VERIFY: 833d137a auto
// OFFSET: 0x00781430
void CWorld::LoadMap(const char* mapName, C3Vector* position, int32_t zoneID, int32_t unused_a4) {
    World::s_farClip = World::ValidateFarClip(CWorldParam::cvar_farClip->m_floatValue, zoneID);
    World::s_nearClip = 0.2f;
    World::s_prevFarClip = World::s_farClip;

    if (SFile::IsTrial()) {
        StreamingSetMapId(zoneID);
    }

    World::PrepareAreaOfInterest(*position);
    CWorld::gbChunkRectOld = CWorld::gbChunkRect;
    CMap::Load(mapName, zoneID);

    int32_t shaders = 1;

    if (!(CWorld::enables2 & Enable_VertexShader) || !(CWorld::enables & Enable_PixelShaders)) {
        shaders = 0;
    }

    Liquid::SetShadersEnabled(shaders);
    Liquid::SetSpecularEnabled(CMap::enableSpecular);
    Liquid::CMaterialBank::Destroy();
}

// VERIFY: 5c8f2417 manual
// OFFSET: 0x00781610
void CWorldScene::ApplyFogState() {
    DayNight::DNInfo* info = DayNight::GetInfo();

    if (!g_theGxDevicePtr->Caps().m_shaderTargets[0]) {
        GxRsSet(GxRs_FogStart, info->m_fog.m_start);
        GxRsSet(GxRs_FogEnd, info->m_fog.m_end);
    }

    GxRsSet(GxRs_FogColor, *reinterpret_cast<int32_t*>(&info->m_fog.m_color));
}

// VERIFY: bc9ebd69 manual
// OFFSET: 0x007816F0
void DayNight::Update(int32_t reset, const C3Vector* cameraPos) {
    float glare = 0.0f;
    DNInfo* info = DayNight::GetInfo();
    int32_t fogMode = static_cast<uint32_t>(g_theGxDevicePtr->Caps().m_shaderTargets[0]) > 1;

    if (s_mapId < 530) {
        fogMode = 0;
    }

    DayNight::SetFogMode(fogMode);
    info->ClearZoneLights();

    if (cameraPos) {
        DayNight::FindAndAddZoneLight(cameraPos);
    }

    if (reset) {
        if (cameraPos) {
            info->m_cameraPos = *cameraPos;
        }

        DayNight::ResetFogAccumulators(true);
        DayNight::ClearLightFlag(1);
    } else {
        glare = DayNight::g_sunGlare.m_screenGlow * 0.34999999f;

        if (glare == 0.0f) {
            DayNight::ClearLightFlag(1);
        }
    }

    DayNight::UpdateLighting();
    DayNight::g_clouds.Update();
    DayNight::UpdateStars();
    DayNight::UpdateFog();
    CWorld::SetShadowColor(info->m_shadowColor);

    uint8_t scale = CMath::fint_((1.0f - glare) * 255.0f);
    CImVector& ambient = info->m_light1.m_ambient;
    ambient.r = static_cast<uint8_t>((scale * ambient.r + 255) >> 8);
    ambient.g = static_cast<uint8_t>((scale * ambient.g + 255) >> 8);
    ambient.b = static_cast<uint8_t>((scale * ambient.b + 255) >> 8);
    CImVector& diffuse = info->m_light1.m_diffuse;
    diffuse.r = static_cast<uint8_t>((scale * diffuse.r + 255) >> 8);
    diffuse.g = static_cast<uint8_t>((scale * diffuse.g + 255) >> 8);
    diffuse.b = static_cast<uint8_t>((scale * diffuse.b + 255) >> 8);

    CM2Light& light = CMap::s_mapLight->m_light;
    light.SetDirection(info->m_light1.m_dir);
    light.m_ambColor = C3Vector(ambient.r * (1.0f / 255.0f), ambient.g * (1.0f / 255.0f), ambient.b * (1.0f / 255.0f));
    light.m_dirColor = C3Vector(diffuse.r * (1.0f / 255.0f), diffuse.g * (1.0f / 255.0f), diffuse.b * (1.0f / 255.0f));
    const CImVector& sun = info->m_bands.m_sunCloudColors[0];
    light.m_specColor = C3Vector(sun.r * (1.0f / 255.0f), sun.g * (1.0f / 255.0f), (1.0f / 255.0f) * sun.b);
    DayNight::SetFogMode(0);
}

// VERIFY: f80e8676 manual
// OFFSET: 0x00782E40
void CWorldScene::ClearObjectFadeoutList() {
    SWModelFadeout* fadeout = s_fadeoutList.Head();

    while (!s_fadeoutList.IsEnd(fadeout)) {
        SWModelFadeout* next = s_fadeoutList.RawNext(fadeout);

        World::ObjectDestroy(fadeout->m_entity);
        fadeout->m_entity = nullptr;
        fadeout->m_link.Unlink();
        s_fadeoutDataPool.LinkToTail(fadeout);

        fadeout = next;
    }
}

// VERIFY: ca15b88d auto
// OFFSET: 0x00783180
void World::UnloadMap() {
    CWorldScene::ClearObjectFadeoutList();

    if (CMap::bActive) {
        CMap::Unload();
    }

    CWorldScene::s_viewerMapObjDef = nullptr;
}

// VERIFY: 940255f7 manual
// OFFSET: 0x007831A0
void CWorld::Update(C3Vector* camPos, C3Vector* camTarget, C3Vector* position) {
    CWorld::UpdateTextureCacheBudget();

    profTimes[profIdx] = FrameTime::s_tickTimeSec;

    if (profIdx++ == 29) {
        profIdx = 0;
    }

    CWorld::gbChunkRectOld = CWorld::gbChunkRect;
    World::PrepareAreaOfInterest(*position);

    int32_t maxX = CWorld::gbChunkRectOld.maxX < CWorld::gbChunkRect.maxX ? CWorld::gbChunkRectOld.maxX : CWorld::gbChunkRect.maxX;
    int32_t maxY = CWorld::gbChunkRectOld.maxY < CWorld::gbChunkRect.maxY ? CWorld::gbChunkRectOld.maxY : CWorld::gbChunkRect.maxY;
    int32_t minX = CWorld::gbChunkRectOld.minX > CWorld::gbChunkRect.minX ? CWorld::gbChunkRectOld.minX : CWorld::gbChunkRect.minX;
    int32_t minY = CWorld::gbChunkRectOld.minY > CWorld::gbChunkRect.minY ? CWorld::gbChunkRectOld.minY : CWorld::gbChunkRect.minY;

    if (minY < maxY && minX < maxX) {
        CWorld::status.m_chunksChanged = 0;
    } else {
        CWorld::status.m_chunksChanged = 1;
        CShadowCache::s_needsRebuild = 1;
    }

    CWorld::frameCnt++;

    if (s_FrameCntCallback) {
        s_FrameCntCallback();
    }

    for (uint32_t i = 0; i < sizeof(s_texVectSpeed) / sizeof(s_texVectSpeed[0]); i++) {
        World::texVect[i].x = s_texVectSpeed[i].x * FrameTime::s_tickTimeSec + World::texVect[i].x;
        World::texVect[i].y = s_texVectSpeed[i].y * FrameTime::s_tickTimeSec + World::texVect[i].y;

        if (World::texVect[i].x >= 64.0f) {
            World::texVect[i].x = 0.0f;
        }

        if (World::texVect[i].y >= 64.0f) {
            World::texVect[i].y = 0.0f;
        }
    }

    CWorldScene::Update(camPos, camTarget);

    bool prepareAll = true;
    bool preload = false;

    if (World::s_farClip - World::s_prevFarClip > 10.0f) {
        prepareAll = false;
        preload = true;
    }

    World::s_prevFarClip = World::s_farClip;

    if (preload) {
        World::s_loadProgressCallback = LoadingScreenWorldCallback;
        World::s_loadProgressParam = nullptr;
        LoadingScreenEnable(s_mapId, true);
        CMap::bPreload = 1;
    }

    CMap::PrepareUpdate(prepareAll);

    if (preload) {
        CMap::bPreload = 0;
        World::s_loadProgressCallback = nullptr;
        World::s_loadProgressParam = nullptr;
        AsyncFileReadSetProgressCallback(nullptr, nullptr);
        LoadingScreenDisable();
    }

    CWorldScene::LocateViewer3();
    DayNight::Update(CWorld::status.m_chunksChanged | CWorld::status.m_prepareAll, camPos);
    CWorld::farFog = DayNight::GetInfo()->m_fog.m_end;

    if (!g_theGxDevicePtr->MasterEnable(GxMasterEnable_Fog)) {
        CWorld::farFog = 100000.0f;
    }

    CWorld::status.m_prepareAll = 0;

    if ((CWorld::enables & Enable_Particulates) && s_liquidTypeId) {
        CWorld::particulate->Update();
    }

    World::s_weather->Update();

    CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPE_PLAYER, __FILE__, __LINE__));

    if (player && !s_disableAreaAmbient) {
        uint32_t areaId;
        CMap::QueryAreaId(player->m_worldObject, areaId);
        AreaTableRec* area = g_areaTableDB.GetRecord(areaId);

        if (area) {
            if (!(area->m_flags & 0x2000) && area->m_parentAreaID) {
                area = g_areaTableDB.GetRecord(area->m_parentAreaID);
            }

            if (area) {
                float ambient = area->m_ambientMultiplier + area->m_ambientMultiplier + 1.0f;
                float delta = ambient - CMapStaticEntity::s_characterAmbient;

                if (fabsf(delta) > 0.01f && FrameTime::s_tickTimeSec < 1.0f) {
                    ambient = CMapStaticEntity::s_characterAmbient + FrameTime::s_tickTimeSec * delta;
                }

                CMapStaticEntity::s_characterAmbient = ambient;
            }
        }
    }

    World::s_m2Scene->m_liquidTypeId = s_liquidTypeId;
}

// VERIFY: 1eb0b466 manual
// OFFSET: 0x007837F0
void CWorld::Destroy() {
    CWorldScene::ClearObjectFadeoutList();
    s_fadeoutDataPool.Clear();
    CWorldParam::Destroy();
    CMap::Destroy();
    CWorldScene::Destroy();

    if (CWorld::shadowModTexture) {
        HandleClose(reinterpret_cast<HOBJECT>(CWorld::shadowModTexture));
    }

    CWorld::shadowModTexture = nullptr;

    if (g_BspDigestCache) {
        CAaBspDigestCache* cache = g_BspDigestCache;
        cache->Reset();
        SMemFree(cache, "delete", SLOG_FUNCTION, 0);
    }

    g_BspDigestCache = nullptr;

    if (CWorld::particulate) {
        Particulate* particulate = CWorld::particulate;
        particulate->~Particulate();
        SMemFree(particulate, "delete", SLOG_FUNCTION, 0);
    }

    if (World::s_weather) {
        Weather* weather = World::s_weather;
        weather->~Weather();
        SMemFree(weather, "delete", SLOG_FUNCTION, 0);
    }

    for (uint32_t i = 0; i < s_unk_CD77B8.Count(); i++) {
        if (s_unk_CD77B8[i]) {
            s_unk_CD77B8[i]->Release();
        }
    }

    s_unk_CD77B8.SetCount(0);

    if (s_unk_CD7734) {
        s_unk_CD7734->Release();
    }

    if (s_unk_CD7738) {
        s_unk_CD7738->Release();
    }

    if (World::s_m2Scene) {
        World::s_m2Scene->Release();
        World::s_m2Scene = nullptr;
    }

    CShadowCache::Destroy();
    CShadowQuery::FreeBuffers();
}

// VERIFY: 7261c1de manual
// OFFSET: 0x00780F50
void CWorld::Initialize() {
    CWorld::enables |= 0x7104B73;
    CWorld::s_elapsedTimeSec = 0.0f;
    CWorld::frameCnt = 0;
    CWorld::chunkCnt = 0;
    CWorld::s_elapsedTicks = 0;

    if (g_theGxDevicePtr->Caps().m_shaderTargets[4] > 0) {
        CWorld::enables |= Enable_PixelShaders;
    }

    if (g_theGxDevicePtr->Caps().m_shaderTargets[0] > 0) {
        CWorld::enables2 |= Enable_VertexShader;
    }

    CWorld::m_M2PassMask |= 0x7;
    CWorld::terrainAlphaBitDepth = CWorldParam::cvar_terrainAlphaBitDepth->m_intValue;
    CWorld::shadowMipLevel = CWorldParam::cvar_shadowLevel->m_intValue;
    CWorld::farFog = CWorldParam::cvar_farClip->m_floatValue;
    CWorld::enableLayerCnt = 4;
    CWorld::status.m_chunksChanged = 0;
    CWorld::status.m_prepareAll = 0;
    CWorld::bShowSimpleDoodads = 0;
    CWorld::bLoadSimpleDoodads = 0;
    World::s_m2Scene = M2CreateScene();

    for (uint32_t i = 0; i < 8; i++) {
        World::texVect[i].x = 0.0f;
        World::texVect[i].y = 0.0f;
        World::texVect[i].z = 0.0f;
        World::texVect[i].w = 1.0f;
    }

    World::idMat.Identity();
    CWorld::s_worldPoolUsage = 1;
    CWorld::shadowColor = 0xFFFFFFFF;
    CWorld::detailDoodadAlphaRef = 128;
    CWorld::detailDoodadTest = 0;
    World::groundEffectDistValueSqr = CWorld::detailDoodadDist * CWorld::detailDoodadDist;
    CWorld::shadowMipLevel = CWorldParam::cvar_shadowLevel->m_intValue;

    CShaderEffect::InitShaderSystem(M2GetCacheFlags() & 0x8, CWorld::enables2 & Enable_HwPcf);
    CShaderEffectManager::AddEffectFile("MapObj.wfx");
    CShaderEffectManager::AddEffectFile("MapObjU.wfx");
    CShaderEffectManager::AddEffectFile("Model2.wfx");
    CShaderEffectManager::AddEffectFile("Particle.wfx");
    CShaderEffectManager::AddEffectFile("ShadowMap.wfx");
    CShadowQuery::Initialize();
    CWorldScene::Initialize();
    CMap::Initialize();

    CWorld::chunkRect = CiRect();
    CWorld::gbChunkRect = CiRect();
    CWorld::gbChunkRectOld = CiRect();

    CWorld::s_groupAreaOfInterest.b.x = 0.0f;
    CWorld::s_groupAreaOfInterest.b.y = 0.0f;
    CWorld::s_groupAreaOfInterest.b.z = 0.0f;
    CWorld::s_groupAreaOfInterest.t.x = 0.0f;
    CWorld::s_groupAreaOfInterest.t.y = 0.0f;
    CWorld::s_groupAreaOfInterest.t.z = 0.0f;
    CWorld::s_objectAreaOfInterest.b.x = 0.0f;
    CWorld::s_objectAreaOfInterest.b.y = 0.0f;
    CWorld::s_objectAreaOfInterest.b.z = 0.0f;
    CWorld::s_objectAreaOfInterest.t.x = 0.0f;
    CWorld::s_objectAreaOfInterest.t.y = 0.0f;
    CWorld::s_objectAreaOfInterest.t.z = 0.0f;

    World::lodMax = 3;
    World::lodMin = 2;

    CWorld::particulate = new (SMemAlloc(sizeof(Particulate), __FILE__, __LINE__, 0)) Particulate(0.027777778f, 30.0f, "Textures\\WaterPoop02.blp");
    World::s_weather = new (SMemAlloc(sizeof(Weather), __FILE__, __LINE__, 0)) Weather();

    World::s_m2Scene->SetProjectTextureCallback(World::ProjectTextureCallback, nullptr);
    World::s_m2Scene->SetProjectPositionCallback(World::ProjectPositionCallback, nullptr);

    ConsoleCommandRegister("showDetailDoodads", CWorld::ConsoleCommand_ShowDetailDoodads, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("maxLOD", CWorld::ConsoleCommand_MaxLOD, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("showCull", CWorld::ConsoleCommand_ShowCull, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("setShadow", CWorld::ConsoleCommand_SetShadow, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("waterRipples", CWorld::ConsoleCommand_WaterRipples, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("waterParticulates", CWorld::ConsoleCommand_WaterParticulates, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("showShadow", CWorld::ConsoleCommand_ShowShadow, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("showLowDetail", CWorld::ConsoleCommand_ShowLowDetail, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("showSimpleDoodads", CWorld::ConsoleCommand_ShowSimpleDoodads, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("detailDoodadAlpha", CWorld::ConsoleCommand_DetailDoodadAlpha, CATEGORY_GRAPHICS, nullptr);
    ConsoleCommandRegister("characterAmbient", CWorld::ConsoleCommand_CharacterAmbient, CATEGORY_GRAPHICS, nullptr);

    ZonesInitialize();
}

// VERIFY: e7c98865 auto
// OFFSET: 0x00406E90
bool World::IsValidPosition(const C3Vector& position, float margin) {
    return World::IsValidPosition(position.x, position.y, position.z, margin);
}

// VERIFY: 8553e69c auto
// OFFSET: 0x00406DE0
bool World::IsValidPosition(float x, float y, float z, float margin) {
    if (_finite(x) && _finite(y) && _finite(z)) {
        float v4 = -(y - HALF_WORLD_SIZE);
        float v5 = -(x - HALF_WORLD_SIZE);

        if (margin > v4) {
            return 0;
        }

        if (34133.332f - margin > v4 && margin <= v5 && v5 < 34133.332f - margin) {
            return 1;
        }
    }

    return 0;
}



// VERIFY: 91f1484d auto
// OFFSET: 0x0077F8D0
int32_t World::GetFlightBoundsLower(const C3Vector& pos, float* height) {
    return CMap::GetFlightBounds(pos, height, 1);
}




// VERIFY: 581a8753 manual
// OFFSET: 0x00783910
bool World::GetFacets(CAaBox* a1, CAaBox* a2, FacetData* a3, uint32_t a4, uint32_t* a5) {
    a3->facets.SetCount(0);
    a3->facetIds.SetCount(0);
    if (a5)
        *a5 = 0;
    if ((a4 & 0x4000) == 0)
        return CMap::GetFacets(a1, a2, a3, a4, a5) != 0;

    static FacetData facetData;

    facetData.facets.SetCount(0);
    facetData.facetIds.SetCount(0);
    if (!CMap::GetFacets(a1, a2, &facetData, a4, a5))
        return 0;

    for (int32_t i = 0; i < facetData.facets.Count(); i++) {
        if (facetData.facets[i].plane.n.z >= 0.64278764f) {
            a3->facets.Add(1, &facetData.facets[i]);
            a3->facetIds.Add(1, &facetData.facetIds[i]);
        }
    }
    return 1;
}




// VERIFY: 9d92b389 auto
// OFFSET: 0x0078FB00
void CWorldScene::FrustumXform(C44Matrix& mat) {
    CWorldScene::frustumStack[CWorldScene::frustumIndex].Transform(mat);
}

// VERIFY: ad5c1827 auto
// OFFSET: 0x0078FB20
int32_t CWorldScene::FrustumCull(CAaBox* box) {
    return CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(*box) == WorldCull_outside;
}

// VERIFY: 7ebc59b0 auto
// OFFSET: 0x0078FB50
void CWorldScene::FrustumPop() {
    CWorldScene::frustumIndex--;
}

// VERIFY: d93d3390 auto
// OFFSET: 0x00790620
float CWorldScene::DistanceToCamPlaneXY(const C3Vector* pos) {
    return pos->y * CWorldScene::camPlaneXY.n.y + pos->z * CWorldScene::camPlaneXY.n.z + pos->x * CWorldScene::camPlaneXY.n.x + CWorldScene::camPlaneXY.d;
}

// VERIFY: a1ada3aa manual
// OFFSET: 0x00790650
void CWorldScene::GetNearestCornerToCamera(CAaBox* box, C3Vector* outCorner) {
    outCorner->x = (CWorldScene::camTarget.x >= CWorldScene::s_activeWorldView.x)
                       ? box->b.x
                       : box->t.x;

    outCorner->y = (CWorldScene::camTarget.y >= CWorldScene::s_activeWorldView.y)
                       ? box->b.y
                       : box->t.y;

    outCorner->z = (CWorldScene::camTarget.z >= CWorldScene::s_activeWorldView.z)
                       ? box->b.z
                       : box->t.z;
}





// VERIFY: 827ec2fa manual
// OFFSET: 0x007906C0
void CSortTable::BuildPlanes(const C3Vector& dir, const C3Vector& origin) {
    float dist = 0.0f;

    for (uint32_t i = 0; i < 64; i++) {
        C3Vector point(dir.x * dist + origin.x, dist * dir.y + origin.y, dist * dir.z + origin.z);
        this->m_bucketPlanes[i].x = dir.x;
        this->m_bucketPlanes[i].y = dir.y;
        this->m_bucketPlanes[i].z = dir.z;
        this->m_bucketPlanes[i].w = -(point.x * dir.x + point.z * dir.z + point.y * dir.y);
        dist += 33.333332f;
    }
}

// VERIFY: 96c18245 manual
// OFFSET: 0x00790920
void CWorldScene::UpdateViewerLiquid() {
    s_viewerLiquidDepth = 0.0f;
    int32_t liquidType = 0;
    float level;

    if (!CWorldScene::s_viewerMapObjDef) {
        int32_t flags = 0;

        if (CMap::QueryLiquidStatus(&CWorldScene::s_activeWorldView, &liquidType, &level, &flags, 1)) {
            s_viewerLiquidDepth = level - CWorldScene::s_activeWorldView.z;
        }
    } else {
        C3Vector local = CWorldScene::s_activeWorldView * CWorldScene::s_viewerMapObjDef->m_invMat;
        CMapObjGroup* group = CWorldScene::s_viewerMapObjDef->m_owner->GetGroup(CWorldScene::s_viewerMapObjGroups.m_data[0], 0);

        if (group && group->QueryLiquid(local, &liquidType, &level)) {
            s_viewerLiquidDepth = level - local.z;
        }
    }

    int32_t leftLiquid = 0;

    if (liquidType) {
        if (liquidType != static_cast<int32_t>(s_liquidTypeId) && (CWorld::enables & Enable_Particulates)) {
            const LiquidTypeRec* rec = g_liquidTypeDB.GetRecord(liquidType);
            bool particles = (rec->m_flags & 8) != 0;
            CWorld::particulate->m_enabled = particles;

            if (particles) {
                CWorld::particulate->InitParticles(liquidType);
                CWorld::particulate->SetScale(rec->m_particleScale * 0.027777778f);
            }
        }
    } else if (s_liquidTypeId) {
        leftLiquid = 1;
    }

    s_liquidTypeId = liquidType;

    if (leftLiquid) {
        DayNight::ResetFogAccumulators(true);
    }

    Liquid::InvalidateProceduralTextures();
}

// VERIFY: a0789a2d auto
// OFFSET: 0x00790A80
void CWorldScene::RenderLiquidPass() {
    CWorldScene::ApplyFogState();

    if ((CWorld::enables & Enable_Water) && CWorldScene::s_liquidRenderer) {
        CWorldScene::s_liquidRenderer->Flush(CWorldScene::s_activeWorldView, 1);
    }

    CMap::RenderWaterRipples();
}

// VERIFY: 58728bc0 auto
// OFFSET: 0x00790AB0
void CWorldScene::MergeIntoWorldRect(CPortalView* portalView) {
    CWorldScene::s_unk_ADF570.Merge(*portalView);
}

// VERIFY: f4c2895d auto
// OFFSET: 0x00790AD0
void CWorldScene::MergeIntoFrustumRect(CPortalView* portalView) {
    CWorldScene::frustumPortalView.Merge(*portalView);
}

// VERIFY: d2772b54 manual
// OFFSET: 0x00790AF0
void CWorldScene::FrustumSet(CRect* rect) {
    C3Vector corners[8];

    for (uint32_t i = 0; i < 8; i += 4) {
        C3Vector* src = &CWorldScene::s_frustumCorners[i];

        C3Vector edgeTop(src[2].x - src[1].x, src[2].y - src[1].y, src[2].z - src[1].z);
        C3Vector topLeft(edgeTop.x * rect->minX + src[1].x, edgeTop.y * rect->minX + src[1].y, edgeTop.z * rect->minX + src[1].z);
        C3Vector topRight(edgeTop.x * rect->maxX + src[1].x, edgeTop.y * rect->maxX + src[1].y, edgeTop.z * rect->maxX + src[1].z);

        C3Vector edgeBottom(src[3].x - src[0].x, src[3].y - src[0].y, src[3].z - src[0].z);
        C3Vector bottomLeft(edgeBottom.x * rect->minX + src[0].x, edgeBottom.y * rect->minX + src[0].y, edgeBottom.z * rect->minX + src[0].z);
        C3Vector bottomRight(edgeBottom.x * rect->maxX + src[0].x, edgeBottom.y * rect->maxX + src[0].y, edgeBottom.z * rect->maxX + src[0].z);

        C3Vector left(topLeft.x - bottomLeft.x, topLeft.y - bottomLeft.y, topLeft.z - bottomLeft.z);
        corners[i + 0] = C3Vector(left.x * rect->minY + bottomLeft.x, left.y * rect->minY + bottomLeft.y, left.z * rect->minY + bottomLeft.z);
        corners[i + 1] = C3Vector(left.x * rect->maxY + bottomLeft.x, left.y * rect->maxY + bottomLeft.y, left.z * rect->maxY + bottomLeft.z);

        C3Vector right(topRight.x - bottomRight.x, topRight.y - bottomRight.y, topRight.z - bottomRight.z);
        corners[i + 3] = C3Vector(right.x * rect->minY + bottomRight.x, right.y * rect->minY + bottomRight.y, right.z * rect->minY + bottomRight.z);
        corners[i + 2] = C3Vector(right.x * rect->maxY + bottomRight.x, right.y * rect->maxY + bottomRight.y, right.z * rect->maxY + bottomRight.z);
    }

    CWorldScene::frustumStack[CWorldScene::frustumIndex].CalcPlanesFromCorners(corners);
}

// VERIFY: 65468f88 manual
// OFFSET: 0x00790E20
void __fastcall CWorldScene::FrustumSet(const C3Vector* corners, const CRect& rect) {
    C3Vector frustumCorners[8];

    for (uint32_t i = 0; i < 2; i++) {
        const C3Vector* quad = &corners[4 * i];

        float leftX = (quad[2].x - quad[1].x) * rect.minX + quad[1].x;
        float leftY = (quad[2].y - quad[1].y) * rect.minX + quad[1].y;
        float leftZ = rect.minX * (quad[2].z - quad[1].z) + quad[1].z;
        float rightX = (quad[2].x - quad[1].x) * rect.maxX + quad[1].x;
        float rightY = (quad[2].y - quad[1].y) * rect.maxX + quad[1].y;
        float rightZ = (quad[2].z - quad[1].z) * rect.maxX + quad[1].z;

        float left0X = (quad[3].x - quad[0].x) * rect.minX + quad[0].x;
        float left0Y = (quad[3].y - quad[0].y) * rect.minX + quad[0].y;
        float left0Z = rect.minX * (quad[3].z - quad[0].z) + quad[0].z;
        float right0X = (quad[3].x - quad[0].x) * rect.maxX + quad[0].x;
        float right0Y = (quad[3].y - quad[0].y) * rect.maxX + quad[0].y;
        float right0Z = rect.maxX * (quad[3].z - quad[0].z) + quad[0].z;

        C3Vector* out = &frustumCorners[4 * i];
        out[0].x = (leftX - left0X) * rect.minY + left0X;
        out[0].y = (leftY - left0Y) * rect.minY + left0Y;
        out[0].z = rect.minY * (leftZ - left0Z) + left0Z;
        out[1].x = left0X + (leftX - left0X) * rect.maxY;
        out[1].y = left0Y + (leftY - left0Y) * rect.maxY;
        out[1].z = left0Z + rect.maxY * (leftZ - left0Z);
        out[3].x = (rightX - right0X) * rect.minY + right0X;
        out[3].y = (rightY - right0Y) * rect.minY + right0Y;
        out[3].z = rect.minY * (rightZ - right0Z) + right0Z;
        out[2].x = right0X + (rightX - right0X) * rect.maxY;
        out[2].y = right0Y + (rightY - right0Y) * rect.maxY;
        out[2].z = (rightZ - right0Z) * rect.maxY + right0Z;
    }

    CWorldScene::frustumStack[CWorldScene::frustumIndex].CalcPlanesFromCorners(frustumCorners);
}

// VERIFY: d9802889 auto
// OFFSET: 0x00791100
void CWorldScene::FrustumSet(CFrustum* frustum) {
    static_cast<CFrustum&>(CWorldScene::frustumStack[CWorldScene::frustumIndex]) = *frustum;
}

// VERIFY: 939d636d auto
// OFFSET: 0x00791120
int32_t CWorldScene::FrustumCull(const C3Vector& center, float radius) {
    CAaSphere sphere;
    sphere.c = center;
    sphere.r = radius;

    return CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(sphere) == WorldCull_outside;
}

// VERIFY: e94a022f auto
// OFFSET: 0x00791170
void CWorldScene::BuildHorizonProjection(C44Matrix* projection) {
    CGCamera* camera = CGWorldFrame::GetActiveCamera();
    float farZ = camera->m_farZ;
    float fov = camera->FOV();
    GxuXformCreateProjection_Exact(fov * 0.60000002f, camera->m_aspect, farZ - 50.0f, World::s_horizonFarClipScale * World::s_farClip, *projection);
}

// VERIFY: 6cb64bd1 manual
// OFFSET: 0x00791640
bool World::NDCXform(CFrustum* frustum, C44Matrix* out, bool includeTranslation) {
    C3Vector* corners = frustum->corners;

    float ax = corners[3].x - corners[0].x;
    float ay = corners[3].y - corners[0].y;
    float az = corners[3].z - corners[0].z;

    float bx = corners[1].x - corners[0].x;
    float by = corners[1].y - corners[0].y;
    float bz = corners[1].z - corners[0].z;

    float cx = corners[4].x - corners[0].x;
    float cy = corners[4].y - corners[0].y;
    float cz = corners[4].z - corners[0].z;

    *out = C44Matrix();

    out->a0 = ax;
    out->a1 = ay;
    out->a2 = az;
    out->b0 = bx;
    out->b1 = by;
    out->b2 = bz;
    out->c0 = cx;
    out->c1 = cy;
    out->c2 = cz;

    if (includeTranslation) {
        out->d0 = corners[0].x;
        out->d1 = corners[0].y;
        out->d2 = corners[0].z;
    }

    float det = out->Determinant();

    if (fabs(det) < 0.00000023841858f) {
        *out = C44Matrix();
        return false;
    }

    *out = out->Inverse(det);

    return true;
}





// VERIFY: 4c456906 auto
// OFFSET: 0x00791950
void CWorldScene::FrustumPush() {
    CWorldScene::frustumIndex++;
    static_cast<CFrustum&>(CWorldScene::frustumStack[CWorldScene::frustumIndex]) = CWorldScene::frustumStack[CWorldScene::frustumIndex - 1];
}

// VERIFY: 11ca4f8e manual
// OFFSET: 0x00791980
void CWorldScene::CullHorizon(CRect* rect) {
    if (!(CWorld::enables & Enable_LowDetail)) {
        return;
    }

    C44Matrix savedProjection;
    C44Matrix projection;
    CWorldScene::BuildHorizonProjection(&projection);
    savedProjection = g_theGxDevicePtr->m_projection;
    g_theGxDevicePtr->XformSetProjection(projection);

    C44Matrix view;
    CGxMatrixStack& viewStack = g_theGxDevicePtr->m_xforms[GxXform_View];
    view = viewStack.m_mtx[viewStack.m_level];

    C3Vector corners[8];
    GxuXformCalcFrustumCorners(view, projection, corners);

    for (uint32_t i = 0; i < 8; i++) {
        corners[i].x = corners[i].x + CWorldScene::s_activeWorldView.x;
        corners[i].y = corners[i].y + CWorldScene::s_activeWorldView.y;
        corners[i].z = corners[i].z + CWorldScene::s_activeWorldView.z;
    }

    CWorldScene::frustumIndex++;
    CWorldScene::frustumStack[CWorldScene::frustumIndex] = CWorldScene::frustumStack[CWorldScene::frustumIndex - 1];
    CWorldScene::FrustumSet(corners, *rect);
    CMap::s_lowDetail.CullHorizonAreas(&CWorldScene::s_activeWorldView);
    CWorldScene::frustumIndex--;

    g_theGxDevicePtr->XformSetProjection(savedProjection);
}

// VERIFY: b484c5b7 manual
// OFFSET: 0x00791CB0
void CWorldScene::AddDoodadDefModelToModelScene(CMapDoodadDef* doodadDef) {
    doodadDef->m_doodadDefLink.Unlink();

    uint32_t flags = doodadDef->m_flags;

    if ((flags & 0x20) || !(CWorld::enables & Enable_Doodads)) {
        return;
    }

    doodadDef->m_distanceToCamera = doodadDef->m_sphere.c.y * CWorldScene::camPlane.n.y + doodadDef->m_sphere.c.z * CWorldScene::camPlane.n.z + doodadDef->m_sphere.c.x * CWorldScene::camPlane.n.x + CWorldScene::camPlane.d - doodadDef->m_sphere.r;

    float alpha = 1.0f;

    if ((CWorld::enables & Enable_NoFullAlpha) && !(flags & 0x800)) {
        float dx = doodadDef->m_sphere.c.x - CWorldScene::s_activeWorldView.x;
        float dy = doodadDef->m_sphere.c.y - CWorldScene::s_activeWorldView.y;
        float dz = doodadDef->m_sphere.c.z - CWorldScene::s_activeWorldView.z;
        float distSq = dz * dz + dy * dy + dx * dx;

        if (distSq > CWorldView::s_fadeDistSqr[doodadDef->m_fadeLevel]) {
            return;
        }

        if (distSq > CWorldScene::s_fadeDistMinSqr[doodadDef->m_fadeLevel]) {
            float dist = CMap::s_useSSE ? sqrtf(distSq) : sqrt(distSq);
            alpha = 1.0f - (dist - CWorldScene::s_fadeDistMin[doodadDef->m_fadeLevel]) / CWorldScene::s_fadeRangeDefault[doodadDef->m_fadeLevel];

            if (alpha > 0.99000001f) {
                alpha = 1.0f;
            } else if (alpha <= 0.0099999998f) {
                return;
            }
        }
    }

    doodadDef->m_model->SetAnimating(1);

    CM2Model* model = doodadDef->m_model;

    if (model->m_attachParent) {
        model->m_fFlags |= 0x80;
    } else {
        model->m_fFlags |= 0x8;
    }

    if (model->m_attachParent) {
        model->m_fFlags |= 0x20000;
    } else {
        model->m_fFlags |= 0x10000;
    }

    doodadDef->m_model->m_fadeAlpha = alpha;
}

// VERIFY: 300a631e auto
// OFFSET: 0x00792AD0
void CWorldScene::AddMapObjDefGroup(CMapObjDef* mapObjDef, CMapObjDefGroup* mapObjDefGroup) {
    if (!(mapObjDef->m_owner->GetGroupFlags(mapObjDefGroup->m_groupNum) & 0x10008)) {
        return;
    }

    if (mapObjDef->m_flags & MAPOBJ_FLAG_NO_DEPTH_SORT) {
        CWorldScene::sortTable.m_pendingExteriorGroupList.LinkToTail(mapObjDefGroup);
        return;
    }

    C3Vector nearest(0.0f, 0.0f, 0.0f);
    CWorldScene::GetNearestCornerToCamera(&mapObjDefGroup->m_bbox, &nearest);
    mapObjDefGroup->m_distanceToCamera = CWorldScene::camPlane.n.x * nearest.x + CWorldScene::camPlane.n.y * nearest.y + CWorldScene::camPlane.n.z * nearest.z + CWorldScene::camPlane.d;

    float dist = nearest.y * CWorldScene::camPlaneXY.n.y + nearest.z * CWorldScene::camPlaneXY.n.z + nearest.x * CWorldScene::camPlaneXY.n.x + CWorldScene::camPlaneXY.d;
    int32_t bucket = 0;

    if (dist > 0.0f) {
        bucket = CMath::fint_(dist * CBDIVISOR - 0.5f);

        if (bucket >= 64) {
            return;
        }
    }

    CWorldScene::sortTable.m_table[bucket].m_exteriorGroupList.LinkToTail(mapObjDefGroup);
}

// VERIFY: 0086281a manual
// OFFSET: 0x00792BD0
void CWorldScene::ComputeExteriorSortDistances() {
    CMapObjDefGroup* group = CWorldScene::sortTable.m_pendingExteriorGroupList.Head();

    while (group) {
        CMapObjDefGroup* next = CWorldScene::sortTable.m_pendingExteriorGroupList.Next(group);
        group->m_sortEntryLink.Unlink();

        float x = CWorldScene::camTarget.x >= CWorldScene::s_activeWorldView.x ? group->m_bbox.b.x : group->m_bbox.t.x;
        float y = CWorldScene::camTarget.y >= CWorldScene::s_activeWorldView.y ? group->m_bbox.b.y : group->m_bbox.t.y;
        float z = CWorldScene::camTarget.z >= CWorldScene::s_activeWorldView.z ? group->m_bbox.b.z : group->m_bbox.t.z;
        int32_t bucket = 0;
        group->m_distanceToCamera = CWorldScene::camPlane.n.z * z + CWorldScene::camPlane.n.y * y + CWorldScene::camPlane.n.x * x + CWorldScene::camPlane.d;

        float dist = x * CWorldScene::camPlaneXY.n.x + y * CWorldScene::camPlaneXY.n.y + z * CWorldScene::camPlaneXY.n.z + CWorldScene::camPlaneXY.d;

        if (dist > 0.0f) {
            bucket = CMath::fint_(dist * CBDIVISOR - 0.5f);

            if (bucket >= 64) {
                return;
            }
        }

        CWorldScene::sortTable.m_table[bucket].m_exteriorGroupList.LinkToTail(group);
        group = next;
    }
}

// VERIFY: 7ea57828 manual
// OFFSET: 0x00792D80
void CWorldScene::AddMapChunkToRenderList(CMapChunk* mapChunk, C3Vector* pos) {
    int32_t bucket = 0;
    float distance = pos->y * CWorldScene::camPlaneXY.n.y + pos->z * CWorldScene::camPlaneXY.n.z + pos->x * CWorldScene::camPlaneXY.n.x + CWorldScene::camPlaneXY.d;

    if (distance <= 0.0f || (bucket = CMath::fint_(distance * CBDIVISOR - 0.5f)) < 64) {
        CWorldScene::sortTable.m_table[bucket].m_mapChunkList.LinkToTail(mapChunk);
    }
}

// VERIFY: b58d1116 manual
// OFFSET: 0x00792DF0
void CWorldScene::AddToDistanceBucket(CChunkLiquid* liquid, const C3Vector& pos) {
    int32_t bucket = 0;
    float distance = pos.y * CWorldScene::camPlaneXY.n.y + pos.z * CWorldScene::camPlaneXY.n.z + pos.x * CWorldScene::camPlaneXY.n.x + CWorldScene::camPlaneXY.d;

    if (distance > 0.0f) {
        float scaled = distance * CBDIVISOR;
        bucket = CMath::fint_(scaled - 0.5f);

        if (bucket >= 64) {
            return;
        }
    }

    CWorldScene::sortTable.m_table[bucket].m_liquidList.LinkToTail(liquid);
}

// VERIFY: 82a2066d auto
// OFFSET: 0x00792F60
void CWorldScene::AddHorizonChunk(CMapAreaLow* area) {
    CWorldScene::sortTable.m_horizonChunkList.LinkToTail(area);
}

// VERIFY: d71b0b88 auto
// OFFSET: 0x00792F80
void CWorldScene::AddHorizonMapObjDef(CMapObjDef* def) {
    CWorldScene::sortTable.m_horizonMapObjList.LinkToTail(def);
}

// VERIFY: 140835b8 auto
// OFFSET: 0x00792FA0
void CWorldScene::AddChunkToSortList(CDetailDoodadInst* inst) {
    CWorldScene::sortTable.m_detailDoodadInstList.LinkToTail(inst);
}

// VERIFY: 648c17d7 manual
// OFFSET: 0x00792FC0
uint32_t CWorldScene::AddViewerGroup(TSGrowableArray<uint32_t>* groups, uint32_t group) {
    uint32_t count = groups->Count();

    for (uint32_t i = 0; i < count; i++) {
        if (groups->m_data[i] == group) {
            return i;
        }
    }

    groups->Add(1, &group);
    return groups->Count();
}


















// VERIFY: 3118c690 manual
// OFFSET: 0x00793060
void CWorldScene::CullEntitys(CSortEntry* entry) {
    s_entityCanLink = 0;

    CMapEntity* entity = static_cast<CMapEntity*>(entry->m_entityList.Head());

    while (entity) {
        CMapEntity* next = static_cast<CMapEntity*>(entry->m_entityList.Next(entity));
        entity->m_sortEntryLink.Unlink();
        entity->m_culled = 1;

        CAaSphere sphere = entity->m_sphere;

        if (!CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(sphere) || CWorldOcclusion::QueryVolumes(&entity->m_sphere) || CWorldOcclusion::QueryBuffer(entity->m_sphere.c, entity->m_sphere.r, 0)) {
            CWorldScene::sortTable.m_culledEntityList.LinkToTail(entity);
            entity = next;
            continue;
        }

        entity->m_culled = 0;
        CM2Model* model = entity->m_model;

        if (model) {
            model->SetAnimating(1);

            uint32_t lit = !(entity->m_staticFlags & 0x4);
            uint32_t flags = model->m_fFlags;

            if (model->m_attachParent) {
                flags = (flags & ~0x80) | (lit << 7);
            } else {
                flags = (flags & ~0x8) | (lit << 3);
            }

            model->m_fFlags = flags;

            if (model->m_attachParent) {
                flags = (flags & ~0x20000) | (lit << 17);
            } else {
                flags = (flags & ~0x10000) | (lit << 16);
            }

            model->m_fFlags = flags;
            model->m_lightingCallback = CMapStaticEntity::ModelLightingCallback;
            model->m_lightingArg = entity;
        }

        if (!entity->m_func || entity->m_func(entity->m_funcParam, 5, entity->m_funcParam64, entity->m_funcParam32)) {
            CWorldScene::sortTable.m_visibleEntityList.LinkToTail(entity);
        }

        entity = next;
    }

    s_entityCanLink = 1;
}

// VERIFY: 91948de5 manual
// OFFSET: 0x00793270
void CWorldScene::CullEntitysInterior(TSList<CMapBaseObjLink, TSGetExplicitLink<CMapBaseObjLink> >* linkList, CWFrustum* frustum, int32_t force, int32_t interior) {
    s_entityCanLink = 0;

    CMapBaseObjLink* link = linkList->Head();

    while (link) {
        CMapEntity* entity = static_cast<CMapEntity*>(link->m_owner);
        CMapBaseObjLink* next = linkList->Next(link);
        uint32_t entityFlags = entity->m_staticFlags;

        if (!(force | (entityFlags & 0x1)) || !entity->m_culled || (entityFlags & 0x4)) {
            link = next;
            continue;
        }

        entity->m_culled = 1;

        for (CWFrustum* test = frustum; test; test = test->m_link.Next()) {
            if (!test->Cull(entity->m_sphere.c, entity->m_sphere.r)) {
                continue;
            }

            entity->m_sortEntryLink.Unlink();
            entity->m_culled = 0;

            if (interior) {
                entity->m_flags = static_cast<MapObjFlag>(entity->m_flags | MAPOBJ_FLAG_INT_FOG);
            } else {
                entity->m_flags = static_cast<MapObjFlag>(entity->m_flags & ~MAPOBJ_FLAG_INT_FOG);
            }

            CM2Model* model = entity->m_model;

            if (model) {
                model->SetAnimating(1);

                uint32_t lit = !(entity->m_staticFlags & 0x4);
                uint32_t flags = model->m_fFlags;

                if (model->m_attachParent) {
                    flags = (flags & ~0x80) | (lit << 7);
                } else {
                    flags = (flags & ~0x8) | (lit << 3);
                }

                model->m_fFlags = flags;

                if (model->m_attachParent) {
                    flags = (flags & ~0x20000) | (lit << 17);
                } else {
                    flags = (flags & ~0x10000) | (lit << 16);
                }

                model->m_fFlags = flags;
                model->m_lightingCallback = CMapStaticEntity::ModelLightingCallback;
                model->m_lightingArg = entity;
            }

            if (!entity->m_func || entity->m_func(entity->m_funcParam, 5, entity->m_funcParam64, entity->m_funcParam32)) {
                CWorldScene::sortTable.m_visibleEntityList.LinkToTail(entity);
            }

            break;
        }

        link = next;
    }

    s_entityCanLink = 1;
}

// VERIFY: e5eeddd6 manual
// OFFSET: 0x00793450
void CWorldScene::UpdateSortedModels() {
    CMapEntity* entity = static_cast<CMapEntity*>(CWorldScene::sortTable.m_culledEntityList.Head());

    while (entity) {
        CMapEntity* next = static_cast<CMapEntity*>(CWorldScene::sortTable.m_culledEntityList.Next(entity));
        uint32_t nearFlags = 0;
        entity->m_sortEntryLink.Unlink();

        CM2Model* model = entity->m_model;

        if (model) {
            int32_t animating = 0;

            if (!(entity->m_staticFlags & 0x4000)) {
                if (entity->m_staticFlags & 0x400) {
                    animating = 1;
                }

                C3Vector delta = C3Vector(entity->m_sphere.c.x - CWorldScene::s_activeWorldView.x, entity->m_sphere.c.y - CWorldScene::s_activeWorldView.y, entity->m_sphere.c.z - CWorldScene::s_activeWorldView.z);

                if (delta.x * delta.x + delta.z * delta.z + delta.y * delta.y < 1111.1111f) {
                    animating = 1;
                    nearFlags = 2;
                }

                if (model->m_attachParent) {
                    model->m_fFlags &= ~0x80;
                } else {
                    model->m_fFlags &= ~0x8;
                }

                if (model->m_attachParent) {
                    model->m_fFlags &= ~0x20000;
                } else {
                    model->m_fFlags &= ~0x10000;
                }
            }

            entity->m_model->SetAnimating(animating);
        }

        if (entity->m_func) {
            uint32_t fadeFlags = entity->m_culled < 2 ? 4 : 0;
            entity->m_func(entity->m_funcParam, nearFlags | fadeFlags, entity->m_funcParam64, entity->m_funcParam32);
        }

        entity = next;
    }
}

// VERIFY: b0cc1bd0 auto
// OFFSET: 0x007935A0
void CWorldScene::CullLiquid(CSortEntry* entry) {
    if (!(CWorld::enables & Enable_Water)) {
        CChunkLiquid* liquid = entry->m_liquidList.Head();

        while (liquid) {
            CChunkLiquid* next = entry->m_liquidList.Next(liquid);
            CMap::s_mapChunkLiquidUpdateList.LinkToTail(liquid);
            liquid = next;
        }
    } else {
        CChunkLiquid* liquid = entry->m_liquidList.Head();

        while (liquid) {
            CChunkLiquid* next = entry->m_liquidList.Next(liquid);
            CMap::s_mapChunkLiquidUpdateList.LinkToTail(liquid);

            CAaBox box;
            liquid->GetAaBox(box);

            if (CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(box) && !CWorldOcclusion::QueryBuffer(&box, 0) && !CWorldOcclusion::QueryVolumes(liquid->GetSphere())) {
                liquid->RenderPrep();

                if (CWorldScene::s_liquidRenderer && liquid->m_instance) {
                    CWorldScene::s_liquidRenderer->Draw(liquid->m_instance);
                }
            }

            liquid = next;
        }
    }
}

// VERIFY: 672d50fa manual
// OFFSET: 0x00793760
void CWorldScene::UpdateOcclusion(CSortEntry* entry) {
    if (!(CWorld::enables & Enable_Culling)) {
        entry->m_holedChunkList.UnlinkAll();

        CWorldOccluder* occluder = entry->m_occluderList.Head();

        while (occluder) {
            CWorldOccluder* next = entry->m_occluderList.Next(occluder);
            occluder->Recycle();
            occluder = next;
        }

        return;
    }

    CMapChunk* chunk = entry->m_holedChunkList.Head();

    while (chunk) {
        CMapChunk* next = entry->m_holedChunkList.Next(chunk);

        if (!chunk->m_header->m_holes) {
            chunk->m_sortListLink.Unlink();

            if (CWorldScene::s_occlusionFarDepth > chunk->m_distToCamera && CWorldScene::s_occlusionNearDepth < chunk->m_distToCamera) {
                chunk->ClipBufferUpdate();
            }
        }

        chunk = next;
    }

    chunk = entry->m_holedChunkList.Head();

    while (chunk) {
        CMapChunk* next = entry->m_holedChunkList.Next(chunk);
        chunk->m_sortListLink.Unlink();
        chunk->ClipBufferUpdate();
        chunk = next;
    }

    CWorldOccluder* occluder = entry->m_occluderList.Head();

    while (occluder) {
        CWorldOccluder* next = entry->m_occluderList.Next(occluder);
        occluder->ClipBufferUpdate();

        if (CWorld::enables & Enable_PortalVis) {
            CWorldScene::sortTable.m_visibleOccluderList.LinkToTail(occluder);
        } else {
            occluder->Recycle();
        }

        occluder = next;
    }
}

// VERIFY: edc7b1e9 manual
// OFFSET: 0x00793B10
void CWorldScene::RenderChunksSolid() {
    CMapRenderChunk::SetShaders(0, 0);

    if (CMapRenderChunk::s_currentShaderX[0]) {
        g_theGxDevicePtr->RsSet(GxRs_PixelShader, CMapRenderChunk::s_currentShaderX[0]);
    }

    TSExplicitList<CMapRenderChunk, offsetof(CMapRenderChunk, m_renderChunkLink)>& list = CWorldScene::sortTable.m_renderChunkList[0];
    CMapRenderChunk* renderChunk = list.Head();

    while (!list.IsEnd(renderChunk)) {
        CMapRenderChunk* next = list.RawNext(renderChunk);
        renderChunk->RenderSetup(1);

        if (CWorld::enables & Enable_Chunks) {
            if (CMapRenderChunk::s_currentShaderX[0]) {
                if (CMap::enableTerrainShaderVertex) {
                    renderChunk->RenderSolidVertexPixelShader();
                } else {
                    renderChunk->RenderSolidPixelShader();
                }
            } else {
                renderChunk->RenderSolid();
            }
        }

        renderChunk->m_renderChunkLink.Unlink();
        CMap::s_mapRenderChunkUpdateList.LinkToTail(renderChunk);
        renderChunk = next;
    }
}

// VERIFY: 18ef6719 manual
// OFFSET: 0x00793C30
void CWorldScene::RenderChunksZoneDebug() {
    CMapRenderChunk::SetShaders(0, 0);

    TSExplicitList<CMapRenderChunk, offsetof(CMapRenderChunk, m_renderChunkLink)>& list = CWorldScene::sortTable.m_renderChunkList[20];
    CMapRenderChunk* renderChunk = list.Head();

    while (!list.IsEnd(renderChunk)) {
        CMapRenderChunk* next = list.RawNext(renderChunk);
        renderChunk->RenderSetup(0);

        if (CWorld::enables & Enable_Chunks) {
            renderChunk->RenderZoneDebug();
        }

        renderChunk->m_renderChunkLink.Unlink();
        CMap::s_mapRenderChunkUpdateList.LinkToTail(renderChunk);
        renderChunk = next;
    }
}

// VERIFY: 8ead38ba manual
// OFFSET: 0x00793D20
void CWorldScene::PrepareLiquidInstances() {
    CMapObjDefGroup* group = CWorldScene::sortTable.m_interiorLiquidGroupList.Head();

    while (group) {
        CMapObjDefGroup* next = CWorldScene::sortTable.m_interiorLiquidGroupList.Next(group);
        group->m_liquidLink.Unlink();

        if ((CWorld::enables & Enable_MapObjs) && CWorldScene::s_liquidRenderer) {
            if (!group->m_liquidInstance) {
                CMapObjDef* def = static_cast<CMapObjDef*>(group->m_parentLinkList.Head()->m_ref);
                CMapObj* mapObj = def->m_owner;
                CMapObjGroup* mapObjGroup = mapObj->GetGroup(group->m_groupNum, 0);
                uint32_t liquidType = mapObjGroup->GetLiquidType();
                const LiquidTypeRec* typeRec = g_liquidTypeDB.GetRecord(liquidType);

                if (!typeRec) {
                    SysMsgPrintf(SYSMSG_ERROR, 2, "WMO: Liquid type [%d] not found, defaulting to water!", liquidType);
                    liquidType = 1;
                    typeRec = g_liquidTypeDB.GetRecord(liquidType);
                }

                int32_t exterior;

                if (((mapObjGroup->m_flags & 0x48) && !(group->m_flags & 2)) || (typeRec->m_flags & 0x200)) {
                    exterior = 1;
                } else {
                    exterior = 0;

                    if (liquidType < 21 && liquidType && !((liquidType - 1) & 3)) {
                        liquidType = 17;
                        typeRec = g_liquidTypeDB.GetRecord(liquidType);
                    }
                }

                Liquid::CClientEnvironment* environment = Liquid::CClientEnvironment::Create(exterior == 0);
                Liquid::CMeshGeomFactory* geomFactory = Liquid::CMeshGeomFactory::Create(mapObj, mapObjGroup);
                const LiquidMaterialRec* materialRec = g_liquidMaterialDB.GetRecord(typeRec->m_materialID);
                geomFactory->SetMagma(materialRec->m_lvf == 1);

                float texCoord1ValueX = 0.0f;
                CImVector color;
                *reinterpret_cast<int32_t*>(&color) = -1;

                if (!exterior) {
                    SMOMaterial* material = mapObj->GetLiquidMaterial(group->m_groupNum);

                    if (material) {
                        color = material->m_diffColor;
                    }

                    texCoord1ValueX = 1.0f;
                }

                geomFactory->SetDiffuseColor(color);
                geomFactory->SetTexCoord1ValueX(texCoord1ValueX);

                group->m_liquidInstance = Liquid::CInstance::Create();
                group->m_liquidInstance->m_material = Liquid::CMaterialBank::GetMaterial(liquidType);
                group->m_liquidInstance->m_settings = Liquid::CSettingsBank::GetSettings(liquidType);
                group->m_liquidInstance->m_environment = environment;
                group->m_liquidInstance->m_geomFactory = geomFactory;
                group->m_liquidInstance->m_transform = def->m_mat;
                group->m_liquidInstance->m_bounds = group->m_sphere;
            }

            CWorldScene::s_liquidRenderer->Draw(group->m_liquidInstance);
        }

        group = next;
    }
}

// VERIFY: 065faca9 manual
// OFFSET: 0x00794250
void CSortTable::Clear() {
    for (uint32_t i = 0; i < 64; i++) {
        CSortEntry& entry = this->m_table[i];
        entry.m_mapChunkList.UnlinkAll();

        CChunkLiquid* liquid = entry.m_liquidList.Head();

        while (liquid) {
            CChunkLiquid* next = entry.m_liquidList.Next(liquid);
            CMap::s_mapChunkLiquidUpdateList.LinkToTail(liquid);
            liquid = next;
        }

        entry.m_exteriorGroupList.UnlinkAll();
        this->m_culledEntityList.Combine(&entry.m_entityList, 2, nullptr);

        CWorldOccluder* occluder = entry.m_occluderList.Head();

        while (occluder) {
            CWorldOccluder* next = entry.m_occluderList.Next(occluder);
            occluder->Recycle();
            occluder = next;
        }

        void* item = entry.m_unkList9.Head();

        while (item) {
            void* next = entry.m_unkList9.Next(item);
            entry.m_unkList9.UnlinkNode(item);
            CWorldScene::s_unkList9FreeList.LinkToTail(item);
            item = next;
        }
    }

    this->m_horizonChunkList.UnlinkAll();
    this->m_horizonMapObjList.UnlinkAll();
}

// VERIFY: fbff50c3 auto
// OFFSET: 0x00795400
void CWorldScene::Update(C3Vector* camPos, C3Vector* camTarget) {
    if (CWorldScene::s_liquidRenderer) {
        CWorldScene::s_liquidRenderer->Validate();
    }

    CWorldScene::s_unk_CD8620 = 0;
    CWorldScene::s_interiorSkybox = 0;
    CWorldScene::s_unk_ADF570 = CPortalView();
    CWorldScene::frustumPortalView = CPortalView();
    CWorldScene::s_exteriorPortalViews.SetCount(0);
    CWorldScene::s_portalViews.SetCount(0);

    CWorldScene::s_activeWorldView = *camPos;
    CWorldScene::camTarget = *camTarget;

    C3Vector dir(camTarget->x - camPos->x, camTarget->y - camPos->y, camTarget->z - camPos->z);
    float invLen = 1.0f / sqrt(dir.y * dir.y + dir.z * dir.z + dir.x * dir.x);
    CWorldScene::camVec.x = dir.x * invLen;
    CWorldScene::camVec.y = dir.y * invLen;
    CWorldScene::camVec.z = invLen * dir.z;
    CWorldScene::camPlane.n = CWorldScene::camVec;
    CWorldScene::camPlane.d = -(camPos->x * CWorldScene::camVec.x + camPos->y * CWorldScene::camVec.y + CWorldScene::camVec.z * camPos->z);

    CWorldScene::s_lodFarClip = World::s_farClip - 100.0f;

    if (CWorldScene::s_lodFarClip < 400.0f) {
        CWorldScene::s_lodFarClip = 400.0f;
    }

    C3Vector dirXY(CWorldScene::camVec.x, CWorldScene::camVec.y, 0.0f);
    float lenSqXY = dirXY.y * dirXY.y + dirXY.x * dirXY.x;

    if (lenSqXY > 0.000099999997f) {
        float invXY = 1.0f / sqrt(lenSqXY);
        dirXY.x *= invXY;
        dirXY.y *= invXY;
        dirXY.z *= invXY;
    } else {
        dirXY.z = 0.0f;
    }

    CWorldScene::camPlaneXY.n = dirXY;
    CWorldScene::camPlaneXY.d = -(dirXY.x * camPos->x + dirXY.y * camPos->y + dirXY.z * camPos->z);
    CWorldScene::sortTable.BuildPlanes(dirXY, *camPos);

    CGxMatrixStack& view = g_theGxDevicePtr->m_xforms[GxXform_View];
    CWorldScene::viewMatrix = view.m_mtx[view.m_level];
    CWorldScene::projMatrix = g_theGxDevicePtr->m_projection;
    CWorldScene::viewPort = g_theGxDevicePtr->m_viewport;

    GxuXformCalcFrustumCorners(CWorldScene::viewMatrix, CWorldScene::projMatrix, CWorldScene::s_frustumCorners);

    for (uint32_t i = 0; i < 8; i++) {
        CWorldScene::s_frustumCorners[i].x = CWorldScene::s_frustumCorners[i].x + CWorldScene::s_activeWorldView.x;
        CWorldScene::s_frustumCorners[i].y = CWorldScene::s_frustumCorners[i].y + CWorldScene::s_activeWorldView.y;
        CWorldScene::s_frustumCorners[i].z = CWorldScene::s_frustumCorners[i].z + CWorldScene::s_activeWorldView.z;
    }

    CWorldScene::s_clipFrustum.CalcPlanesFromCorners(CWorldScene::s_frustumCorners);
    CWorldScene::viewMatrix.Translate(-CWorldScene::s_activeWorldView);
    CWorldScene::s_viewProjMatrix = CWorldScene::viewMatrix * CWorldScene::projMatrix;
    CWorldScene::s_viewProjW.x = CWorldScene::s_viewProjMatrix.a3;
    CWorldScene::s_viewProjW.y = CWorldScene::s_viewProjMatrix.b3;
    CWorldScene::s_viewProjW.z = CWorldScene::s_viewProjMatrix.c3;
    CWorldScene::s_viewProjW.w = CWorldScene::s_viewProjMatrix.d3;

    CWorldScene::boundingBox = CAaBox::Bounding(CWorldScene::s_frustumCorners, 8);
    CWorldScene::s_frustumChunkRect.minX = CMath::fint_(-(CWorldScene::boundingBox.t.y - HALF_WORLD_SIZE) * OO_COORD_TO_CHUNK - 0.5f);
    CWorldScene::s_frustumChunkRect.minY = CMath::fint_(-(CWorldScene::boundingBox.t.x - HALF_WORLD_SIZE) * OO_COORD_TO_CHUNK - 0.5f);
    CWorldScene::s_frustumChunkRect.maxX = CMath::fint_(-(CWorldScene::boundingBox.b.y - HALF_WORLD_SIZE) * OO_COORD_TO_CHUNK - 0.5f);
    CWorldScene::s_frustumChunkRect.maxY = CMath::fint_(-(CWorldScene::boundingBox.b.x - HALF_WORLD_SIZE) * OO_COORD_TO_CHUNK - 0.5f);

    CWorldScene::frustumIndex = 0;
    CWorldScene::frustumStack[0].CalcPlanesFromCorners(CWorldScene::s_frustumCorners);

    CMapChunk::farCornerIndex = 0;

    if (CWorldScene::camTarget.x > CWorldScene::s_activeWorldView.x) {
        CMapChunk::farCornerIndex = 2;
    }

    if (CWorldScene::camTarget.y > CWorldScene::s_activeWorldView.y) {
        CMapChunk::farCornerIndex++;
    }

    CMapChunk::nearCornerIndex = 0;

    if (CWorldScene::camTarget.x < CWorldScene::s_activeWorldView.x) {
        CMapChunk::nearCornerIndex = 2;
    }

    if (CWorldScene::camTarget.y < CWorldScene::s_activeWorldView.y) {
        CMapChunk::nearCornerIndex++;
    }

    CWorldScene::camPlane.n = CWorldScene::camVec;
    CWorldScene::camPlane.d = -(camPos->x * CWorldScene::camVec.x + camPos->y * CWorldScene::camVec.y + CWorldScene::camVec.z * camPos->z);

    if (lenSqXY > 0.000099999997f) {
        C44Matrix lookAt;
        GxuXformCreateLookAtSgCompat(C3Vector(0.0f, 0.0f, 0.0f), dirXY, C3Vector(0.0f, 0.0f, 1.0f), lookAt);
        lookAt.Translate(-CWorldScene::s_activeWorldView);
        CWorldScene::s_clipBufferMatrix = lookAt * CWorldScene::projMatrix;
    } else {
        CWorldScene::s_clipBufferMatrix = C44Matrix();
    }

    CWorldScene::s_unk_B2EB68.Update(OsGetAsyncTimeMs());
}

// VERIFY: a6801f6f auto
// OFFSET: 0x00795D00
void CWorldScene::PushExteriorPortalView(CPortalView* portalView) {
    CWorldScene::s_exteriorPortalViews.Add(1, portalView);
}

// VERIFY: 7373a38b auto
// OFFSET: 0x00795D20
void CWorldScene::PushPortalView(CPortalView* portalView) {
    CWorldScene::s_portalViews.Add(1, portalView);
}

// VERIFY: 7852eb0b auto
// OFFSET: 0x00795D40
void CWorldScene::LocateViewer3() {
    CWorldScene::s_viewerMapObjDef = nullptr;
    CWorldScene::s_viewerMovedMapObjDef = nullptr;
    CWorldScene::s_viewerMapObjGroups.m_count = 0;
    CWorldScene::s_viewerMovedMapObjGroups.m_count = 0;
    s_debugMapName[0] = 0;
    s_debugMapChunk[0] = 0;

    if (!(CWorld::enables & Enable_MapObjs)) {
        return;
    }

    C3Vector start = CWorldScene::s_activeWorldView;
    C3Vector end(CWorldScene::s_activeWorldView.x, CWorldScene::s_activeWorldView.y, CWorldScene::s_activeWorldView.z - 1760.0f);
    float dist = 1.0f;
    CMapChunk* chunk = nullptr;
    bool hitTerrain = CMap::VectorIntersectTerrain(&start, &end, &dist, 0x100, &chunk);

    CMapObjDef* defs[2];
    uint32_t groups[4];

    if (!CMap::LocateViewerMapObjs(start, end, dist, defs, groups)) {
        if (hitTerrain) {
            SStrCopy(s_debugMapName, CMap::mapName, sizeof(s_debugMapName));
            SStrPrintf(s_debugMapChunk, sizeof(s_debugMapChunk), "%i, %i", chunk->m_cOffset.x / 16, chunk->m_cOffset.y / 16);
        }

        return;
    }

    CWorldScene::s_viewerMapObjDef = defs[0];

    if (defs[0]) {
        CMapObj* mapObj = defs[0]->m_owner;
        const char* wmoName = mapObj->m_wmoName;
        const char* groupName = mapObj->GetGroupName(groups[0]);

        if (wmoName) {
            SStrCopy(s_debugMapName, wmoName, sizeof(s_debugMapName));
        }

        if (groupName) {
            SStrCopy(s_debugMapChunk, groupName, sizeof(s_debugMapChunk));
        }

        CWorldScene::AddViewerGroup(&CWorldScene::s_viewerMapObjGroups, groups[0]);

        if (groups[1] != 0xFFFF) {
            CWorldScene::AddViewerGroup(&CWorldScene::s_viewerMapObjGroups, groups[1]);
        }

        uint32_t groupFlags = 0;

        if (groups[0] != 0xFFFF) {
            groupFlags = CWorldScene::s_viewerMapObjDef->m_owner->GetGroupFlags(groups[0]);
        }

        if (groups[1] != 0xFFFF) {
            groupFlags |= CWorldScene::s_viewerMapObjDef->m_owner->GetGroupFlags(groups[1]);
        }

        if (groupFlags & 0x40140) {
            CRect rect;
            rect.minY = 0.0f;
            rect.minX = 0.0f;
            rect.maxY = 1.0f;
            rect.maxX = 1.0f;
            CPortalView view(rect, 0.0f);
            CWorldScene::s_unk_ADF570.Merge(view);
            CWorldScene::PushPortalView(&view);
        }
    }

    CWorldScene::s_viewerMovedMapObjDef = defs[1];

    if (defs[1]) {
        CWorldScene::AddViewerGroup(&CWorldScene::s_viewerMovedMapObjGroups, groups[2]);

        if (groups[3] != 0xFFFF) {
            CWorldScene::AddViewerGroup(&CWorldScene::s_viewerMovedMapObjGroups, groups[3]);
        }
    }
}

// VERIFY: a040cfec manual
// OFFSET: 0x007964A0
void CWorldScene::RenderMapObjDefGroups() {
    g_theGxDevicePtr->RsPush();
    CWorldScene::frustumIndex++;
    CWorldScene::frustumStack[CWorldScene::frustumIndex] = CWorldScene::frustumStack[CWorldScene::frustumIndex - 1];
    CShadowCache::SetShadowMapGenericGlobal();

    CMapObjDefGroup* group = CWorldScene::sortTable.m_mapObjDefGroup.Head();

    while (group) {
        CMapObjDefGroup* next = CWorldScene::sortTable.m_mapObjDefGroup.Next(group);
        group->m_sortTableLink.Unlink();

        CMapObjDef* mapObjDef = static_cast<CMapObjDef*>(group->m_parentLinkList.Head()->m_ref);
        CMapObj* mapObj = mapObjDef->m_owner;

        if (CWorld::enables & Enable_MapObjs) {
            C44Matrix view;
            C3Vector offset(-CWorldScene::s_activeWorldView.x, -CWorldScene::s_activeWorldView.y, -CWorldScene::s_activeWorldView.z);
            view.Translate(offset);
            C44Matrix world = mapObjDef->m_mat * view;
            CWorldScene::SetWorldProjection(world);

            int32_t interior = (group->m_flags >> 15) & 1;

            if (group->m_liquidInstance && group->m_liquidInstance->m_environment) {
                group->m_liquidInstance->m_environment->SetInteriorFog(interior);
            }

            CM2Lighting lighting(group->m_sphere);
            World::s_m2Scene->SelectLights(&lighting);
            group->SelectLights(&lighting);
            DayNight::DNInfo* info = DayNight::GetInfo();
            CWorldScene::SetupLighting(&lighting, &CWorldScene::s_activeWorldView);

            if (static_cast<uint32_t>(mapObjDef->m_objectGuid.m_guid) && mapObjDef->m_objectGuid == s_highlightedObjectGuid) {
                CMapObj::SetEmissiveAmbient(info->m_light1.m_ambient);
            }

            s_curGroupIsInterior = interior;
            mapObj->RenderGroup(group->m_groupNum, mapObjDef->m_invMat, &group->m_frustumList);
        }

        CWFrustum* frustum = group->m_frustumList.Head();

        while (frustum) {
            CWFrustum* nextFrustum = group->m_frustumList.Next(frustum);
            frustum->m_link.Unlink();
            CWorldScene::frustumFreeList.LinkToTail(frustum);
            frustum = nextFrustum;
        }

        group = next;
    }

    CWorldScene::frustumIndex--;
    g_theGxDevicePtr->RsPop();
}

// VERIFY: 4e07fb4f manual
// OFFSET: 0x007968D0
void CWorldScene::CreateInversePortalList() {
    CRect rects[2][64];
    uint32_t counts[2];

    for (uint32_t i = 0; i < 64; i++) {
        rects[0][i].minY = 0.0f;
        rects[0][i].minX = 0.0f;
        rects[0][i].maxY = 0.0f;
        rects[0][i].maxX = 0.0f;
        rects[1][i].minY = 0.0f;
        rects[1][i].minX = 0.0f;
        rects[1][i].maxY = 0.0f;
        rects[1][i].maxX = 0.0f;
    }

    rects[0][0].minY = 0.0f;
    rects[0][0].minX = 0.0f;
    rects[0][0].maxY = 1.0f;
    rects[0][0].maxX = 1.0f;
    counts[0] = 1;

    uint32_t index = 0;
    TSGrowableArray<CPortalView>& views = CWorldScene::s_exteriorPortalViews;

    for (; index < views.Count(); index++) {
        uint32_t cur = index & 1;
        uint32_t next = (index - 1) & 1;
        counts[next] = 0;

        if (counts[cur] > 60) {
            break;
        }

        const CRect& portal = views.m_data[index].m_rect;

        for (uint32_t i = 0; i < counts[cur]; i++) {
            CRect r = rects[cur][i];

            if (r.maxX <= portal.minX || r.minX >= portal.maxX || r.maxY <= portal.minY || r.minY >= portal.maxY) {
                rects[next][counts[next]++] = r;
            } else {
                float maxY = r.maxY;
                float minX = r.minX;
                float maxX = r.maxX;

                if (r.minY < portal.minY) {
                    CRect& out = rects[next][counts[next]++];
                    out.minY = r.minY;
                    out.minX = r.minX;
                    out.maxY = portal.minY;
                    out.maxX = r.maxX;
                    r.minY = portal.minY;
                }

                if (maxY > portal.maxY) {
                    CRect& out = rects[next][counts[next]++];
                    out.minY = portal.maxY;
                    out.minX = r.minX;
                    out.maxY = r.maxY;
                    out.maxX = r.maxX;
                    r.maxY = portal.maxY;
                }

                if (minX < portal.minX) {
                    CRect& out = rects[next][counts[next]++];
                    out.minY = r.minY;
                    out.minX = r.minX;
                    out.maxY = r.maxY;
                    out.maxX = portal.minX;
                    r.minX = portal.minX;
                }

                if (maxX > portal.maxX) {
                    CRect& out = rects[next][counts[next]++];
                    out.minY = r.minY;
                    out.minX = portal.maxX;
                    out.maxY = r.maxY;
                    out.maxX = r.maxX;
                }
            }

            if (counts[next] > 60) {
                break;
            }
        }
    }

    uint32_t result = index & 1;
    uint32_t count = counts[result];
    views.SetCount(count);

    for (uint32_t i = 0; i < count; i++) {
        views.m_data[i].m_rect = rects[result][i];
        views.m_data[i].m_maxViewDepth = 0.0f;
    }
}

// VERIFY: 5846aa37 manual
// OFFSET: 0x00796C10
void CWorldScene::RenderPortalList(TSGrowableArray<CPortalView>* views, int32_t interior) {
    if (!views->Count()) {
        return;
    }

    C44Matrix projection;
    C44Matrix view;
    projection = g_theGxDevicePtr->m_projection;
    CGxMatrixStack& viewStack = g_theGxDevicePtr->m_xforms[GxXform_View];
    view = viewStack.m_mtx[viewStack.m_level];

    C44Matrix identity;
    g_theGxDevicePtr->XformSetView(identity);
    g_theGxDevicePtr->XformSetProjection(identity);
    g_theGxDevicePtr->XformPushAndSet(GxXform_World, identity);
    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_DepthWrite, 1);
    GxRsSet(GxRs_DepthTest, 1);

    DayNight::DNInfo* info = DayNight::GetInfo();
    CImVector color = interior ? info->m_fogInterior.m_color : info->m_fog.m_color;

    if (g_theGxDevicePtr->Caps().m_colorFormat == GxCF_rgba) {
        CImVector swapped = color;
        swapped.r = color.b;
        swapped.b = color.r;
        color = swapped;
    }

    GxRsSet(GxRs_BlendingMode, 0);
    GxRsSet(GxRs_MatDiffuse, *reinterpret_cast<int32_t*>(&color));
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);

    uint32_t viewCount = views->Count();
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;

    for (uint32_t i = 0; i < viewCount; i++) {
        uint32_t n = views->m_data[i].m_vertCount;

        if (n) {
            vertexCount += n;
            indexCount += 3 * (n - 2);
        } else {
            vertexCount += 4;
            indexCount += 6;
        }
    }

    CGxBuf* vertexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPC), vertexCount);
    CGxBuf* indexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Index, sizeof(uint16_t), indexCount);
    CGxVertexPC* vertices = reinterpret_cast<CGxVertexPC*>(g_theGxDevicePtr->BufLock(vertexBuf));
    uint16_t* indices = reinterpret_cast<uint16_t*>(g_theGxDevicePtr->BufLock(indexBuf));

    if (vertices && indices) {
        uint16_t base = 0;

        for (uint32_t i = 0; i < viewCount; i++) {
            CPortalView& portal = views->m_data[i];

            if (portal.m_vertCount) {
                for (uint32_t k = 0; k < portal.m_vertCount; k++) {
                    vertices->p = C3Vector(portal.m_verts[k].x, portal.m_verts[k].y, 1.0f);
                    vertices->c = color;
                    vertices++;
                }

                if (portal.m_vertCount != 2) {
                    for (uint32_t k = 0; k < portal.m_vertCount - 2; k++) {
                        indices[1] = base + k + 1;
                        indices[2] = base + k + 2;
                        indices[0] = base;
                        indices += 3;
                    }
                }

                base += portal.m_vertCount;
            } else {
                portal.m_rect.minX = portal.m_rect.minX * 2.0f - 1.0f;
                portal.m_rect.maxX = portal.m_rect.maxX * 2.0f - 1.0f;
                portal.m_rect.minY = portal.m_rect.minY * 2.0f - 1.0f;
                portal.m_rect.maxY = portal.m_rect.maxY * 2.0f - 1.0f;

                vertices[0].p = C3Vector(portal.m_rect.minX, portal.m_rect.minY, 1.0f);
                vertices[0].c = color;
                vertices[1].p = C3Vector(portal.m_rect.minX, portal.m_rect.maxY, 1.0f);
                vertices[1].c = color;
                vertices[2].p = C3Vector(portal.m_rect.maxX, portal.m_rect.maxY, 1.0f);
                vertices[2].c = color;
                vertices[3].p = C3Vector(portal.m_rect.maxX, portal.m_rect.minY, 1.0f);
                vertices[3].c = color;
                vertices += 4;

                indices[0] = base;
                indices[1] = base + 1;
                indices[2] = base + 2;
                indices[3] = base;
                indices[4] = base + 2;
                indices[5] = base + 3;
                indices += 6;
                base += 4;
            }
        }
    }

    g_theGxDevicePtr->BufUnlock(vertexBuf, 0);
    vertexBuf->m_hasData = 1;
    g_theGxDevicePtr->BufUnlock(indexBuf, 0);
    indexBuf->m_hasData = 1;

    GxPrimVertexPtr(vertexBuf, GxVBF_PC);
    g_theGxDevicePtr->PrimIndexPtr(indexBuf);

    CGxBatch batch;
    batch.m_primType = GxPrim_Triangles;
    batch.m_start = 0;
    batch.m_count = indexCount;
    batch.m_minIndex = 0;
    batch.m_maxIndex = vertexCount - 1;
    g_theGxDevicePtr->Draw(&batch, 1);

    g_theGxDevicePtr->RsPop();
    GxXformPop(GxXform_World);
    g_theGxDevicePtr->XformSetProjection(projection);
    g_theGxDevicePtr->XformSetView(view);
}

// VERIFY: 3633b96c manual
// OFFSET: 0x00797280
void CWorldScene::RenderOccluders() {
    CWorldOccluder* occluder = CWorldScene::sortTable.m_visibleOccluderList.Head();

    if (!occluder) {
        return;
    }

    int32_t count = 0;

    for (CWorldOccluder* it = occluder; it; it = CWorldScene::sortTable.m_visibleOccluderList.Next(it)) {
        count++;
    }

    C44Matrix world;
    world.Translate(C3Vector(-CWorldScene::s_activeWorldView.x, -CWorldScene::s_activeWorldView.y, -CWorldScene::s_activeWorldView.z));
    g_theGxDevicePtr->XformPushAndSet(GxXform_World, world);

    CImVector color;
    color.b = 0x20;
    color.g = 0x80;
    color.r = 0xFF;
    color.a = 0x8F;

    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_DepthWrite, 0);
    GxRsSet(GxRs_BlendingMode, 2);
    GxUpdateAlphaRef(GxRs_AlphaRef);
    GxRsSet(GxRs_MatDiffuse, *reinterpret_cast<int32_t*>(&color));
    GxRsSet(GxRs_Culling, 0);

    CGxBuf* vertexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPN), 4 * count);
    CGxVertexPN* vertex = reinterpret_cast<CGxVertexPN*>(g_theGxDevicePtr->BufLock(vertexBuf));

    C3Vector normal0(1.0f, 0.0f, 0.0f);
    C3Vector normal1(1.0f, 0.0f, 0.0f);
    C3Vector normal2(1.0f, 0.0f, 0.0f);
    C3Vector normal3(1.0f, 0.0f, 0.0f);

    while (occluder) {
        CWorldOccluder* next = CWorldScene::sortTable.m_visibleOccluderList.Next(occluder);

        vertex[0].m_position = occluder->m_points[0];
        vertex[0].m_normal = normal0;
        vertex[1].m_position = occluder->m_points[1];
        vertex[1].m_normal = normal1;
        vertex[2].m_position = occluder->m_points[0];
        vertex[2].m_position.z = occluder->m_points[0].z - 100.0f;
        vertex[2].m_normal = normal2;
        vertex[3].m_position = occluder->m_points[1];
        vertex[3].m_position.z = occluder->m_points[1].z - 100.0f;
        vertex[3].m_normal = normal3;
        vertex += 4;

        occluder->Recycle();
        occluder = next;
    }

    g_theGxDevicePtr->BufUnlock(vertexBuf, 0);
    vertexBuf->m_hasData = 1;
    GxPrimVertexPtr(vertexBuf, GxVBF_PN);

    CGxBuf* indexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Index, sizeof(uint16_t), 6 * count);
    uint16_t* indices = reinterpret_cast<uint16_t*>(g_theGxDevicePtr->BufLock(indexBuf));

    for (int32_t i = 0; i < count; i++) {
        indices[0] = static_cast<uint16_t>(4 * i);
        indices[1] = static_cast<uint16_t>(4 * i + 1);
        indices[2] = static_cast<uint16_t>(4 * i + 2);
        indices[3] = static_cast<uint16_t>(4 * i + 1);
        indices[4] = static_cast<uint16_t>(4 * i + 3);
        indices[5] = static_cast<uint16_t>(4 * i + 2);
        indices += 6;
    }

    g_theGxDevicePtr->BufUnlock(indexBuf, 0);
    indexBuf->m_hasData = 1;
    g_theGxDevicePtr->PrimIndexPtr(indexBuf);

    CGxBatch batch;
    batch.m_primType = GxPrim_Triangles;
    batch.m_start = 0;
    batch.m_count = 6 * count;
    batch.m_minIndex = 0;
    batch.m_maxIndex = static_cast<uint16_t>(4 * count);
    g_theGxDevicePtr->Draw(&batch, 1);

    g_theGxDevicePtr->RsPop();
    GxXformPop(GxXform_World);
}

// VERIFY: 5a09ddbc manual
// OFFSET: 0x007976A0
void CWorldScene::RenderDebugFrustum() {
    if (!unk_CD8F40) {
        return;
    }

    static const int16_t s_faces[6][4] = { { 1, 2, 5, 6 }, { 0, 3, 4, 7 }, { 0, 1, 4, 5 }, { 2, 3, 6, 7 }, { 5, 6, 4, 7 }, { 1, 2, 0, 3 } };

    C44Matrix world;

    CImVector colors[6];
    colors[0].b = 0x00;
    colors[0].g = 0x00;
    colors[0].r = 0xFF;
    colors[0].a = 0x7F;
    colors[1].b = 0x00;
    colors[1].g = 0xFF;
    colors[1].r = 0x00;
    colors[1].a = 0x7F;
    colors[2].b = 0xFF;
    colors[2].g = 0x00;
    colors[2].r = 0x00;
    colors[2].a = 0x7F;
    colors[3].b = 0xFF;
    colors[3].g = 0x00;
    colors[3].r = 0xFF;
    colors[3].a = 0x7F;
    colors[4].b = 0x00;
    colors[4].g = 0xFF;
    colors[4].r = 0xFF;
    colors[4].a = 0x7F;
    colors[5].b = 0xFF;
    colors[5].g = 0xFF;
    colors[5].r = 0x00;
    colors[5].a = 0x7F;

    world.Translate(C3Vector(-CWorldScene::s_activeWorldView.x, -CWorldScene::s_activeWorldView.y, -CWorldScene::s_activeWorldView.z));
    g_theGxDevicePtr->XformPushAndSet(GxXform_World, world);

    CImVector diffuse;
    diffuse.b = 0x20;
    diffuse.g = 0x80;
    diffuse.r = 0xFF;
    diffuse.a = 0x8F;

    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_DepthWrite, 0);
    GxRsSet(GxRs_BlendingMode, 2);
    GxUpdateAlphaRef(GxRs_AlphaRef);
    GxRsSet(GxRs_MatDiffuse, *reinterpret_cast<int32_t*>(&diffuse));
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);

    const C3Vector* corners = unk_CDD200.corners;

    CGxBuf* faceBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPC), 24);
    CGxVertexPC* faceVertex = reinterpret_cast<CGxVertexPC*>(g_theGxDevicePtr->BufLock(faceBuf));

    for (int32_t i = 0; i < 6; i++) {
        for (int32_t j = 0; j < 4; j++) {
            faceVertex->p = corners[s_faces[i][j]];
            faceVertex->c = colors[i];
            faceVertex++;
        }
    }

    g_theGxDevicePtr->BufUnlock(faceBuf, 0);
    faceBuf->m_hasData = 1;
    GxPrimVertexPtr(faceBuf, GxVBF_PC);

    CGxBuf* faceIndexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Index, sizeof(uint16_t), 36);
    uint16_t* faceIndices = reinterpret_cast<uint16_t*>(g_theGxDevicePtr->BufLock(faceIndexBuf));

    for (int32_t i = 0; i < 6; i++) {
        faceIndices[0] = static_cast<uint16_t>(4 * i);
        faceIndices[1] = static_cast<uint16_t>(4 * i + 1);
        faceIndices[2] = static_cast<uint16_t>(4 * i + 2);
        faceIndices[3] = static_cast<uint16_t>(4 * i + 1);
        faceIndices[4] = static_cast<uint16_t>(4 * i + 2);
        faceIndices[5] = static_cast<uint16_t>(4 * i + 3);
        faceIndices += 6;
    }

    g_theGxDevicePtr->BufUnlock(faceIndexBuf, 0);
    faceIndexBuf->m_hasData = 1;
    g_theGxDevicePtr->PrimIndexPtr(faceIndexBuf);

    CGxBatch faceBatch;
    faceBatch.m_primType = GxPrim_Triangles;
    faceBatch.m_start = 0;
    faceBatch.m_count = 36;
    faceBatch.m_minIndex = 0;
    faceBatch.m_maxIndex = 24;
    g_theGxDevicePtr->Draw(&faceBatch, 1);

    CGxBuf* lineBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPC), 12);
    CGxVertexPC* lineVertex = reinterpret_cast<CGxVertexPC*>(g_theGxDevicePtr->BufLock(lineBuf));

    for (int32_t i = 0; i < 6; i++) {
        const C3Vector& a = corners[s_faces[i][0]];
        const C3Vector& b = corners[s_faces[i][1]];
        const C3Vector& c = corners[s_faces[i][2]];
        const C3Vector& d = corners[s_faces[i][3]];
        const C3Vector& normal = unk_CDD200.planes[i].n;

        C3Vector center((a.x + b.x + c.x + d.x) * 0.25f, (a.y + b.y + c.y + d.y) * 0.25f, (a.z + b.z + c.z + d.z) * 0.25f);

        lineVertex[0].p = center;
        *reinterpret_cast<uint32_t*>(&lineVertex[0].c) = 0xFFFFFFFF;
        lineVertex[1].p = C3Vector(center.x + normal.x * 10.0f, center.y + normal.y * 10.0f, center.z + normal.z * 10.0f);
        *reinterpret_cast<uint32_t*>(&lineVertex[1].c) = 0xFFFFFFFF;
        lineVertex += 2;
    }

    g_theGxDevicePtr->BufUnlock(lineBuf, 0);
    lineBuf->m_hasData = 1;
    GxPrimVertexPtr(lineBuf, GxVBF_PC);

    CGxBuf* lineIndexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Index, sizeof(uint16_t), 12);
    uint16_t* lineIndices = reinterpret_cast<uint16_t*>(g_theGxDevicePtr->BufLock(lineIndexBuf));

    for (int32_t i = 0; i < 6; i++) {
        lineIndices[0] = static_cast<uint16_t>(2 * i);
        lineIndices[1] = static_cast<uint16_t>(2 * i + 1);
        lineIndices += 2;
    }

    g_theGxDevicePtr->BufUnlock(lineIndexBuf, 0);
    lineIndexBuf->m_hasData = 1;
    g_theGxDevicePtr->PrimIndexPtr(lineIndexBuf);

    CGxBatch lineBatch;
    lineBatch.m_primType = GxPrim_Lines;
    lineBatch.m_start = 0;
    lineBatch.m_count = 12;
    lineBatch.m_minIndex = 0;
    lineBatch.m_maxIndex = 12;
    g_theGxDevicePtr->Draw(&lineBatch, 1);

    g_theGxDevicePtr->RsPop();
    GxXformPop(GxXform_World);
}

// VERIFY: bccacfe8 manual
// OFFSET: 0x007983B0
CWFrustum* CWorldScene::AllocFrustum() {
    CWFrustum* frustum = CWorldScene::frustumFreeList.Head();

    if (!frustum) {
        void* mem = SMemAlloc(sizeof(CWFrustum), __FILE__, __LINE__, 0x8);
        frustum = mem ? new (mem) CWFrustum : nullptr;
        CWorldScene::frustumFreeList.LinkToTail(frustum);
    }

    frustum->m_link.Unlink();

    return frustum;
}

// VERIFY: 09b34ce3 manual
// OFFSET: 0x007984A0
void CWorldScene::RenderDetailDoodads() {
    g_theGxDevicePtr->RsPush();
    g_theGxDevicePtr->XformPush(GxXform_World);
    g_theGxDevicePtr->XformPush(GxXform_Tex1);

    int32_t shaders = CDetailDoodad::InitializeRenderState();
    C44Matrix world;

    CDetailDoodadInst* next;

    for (CDetailDoodadInst* inst = CWorldScene::sortTable.m_detailDoodadInstList.Head(); inst; inst = next) {
        next = CWorldScene::sortTable.m_detailDoodadInstList.Next(inst);
        CMapChunk* chunk = inst->m_chunk;
        inst->m_age = 0;

        CM2Lighting lighting(chunk->m_sphere);
        World::s_m2Scene->SelectLights(&lighting);
        chunk->SelectLights(&lighting);

        world = C44Matrix();
        world.Translate(chunk->m_topLeftCoords - CWorldScene::s_activeWorldView);

        if (shaders) {
            CDetailDoodad::SetupVertexShader(world, 0);
        } else {
            lighting.SetupGxLights(&CWorldScene::s_activeWorldView);
            lighting.SetupGxFog();

            CGxMatrixStack& stack = g_theGxDevicePtr->m_xforms[GxXform_World];
            stack.m_dirty = 1;
            stack.m_flags[stack.m_level] &= ~CGxMatrixStack::F_Identity;
            stack.m_mtx[stack.m_level] = world;
        }

        inst->DrawBatches();
        inst->m_link.Unlink();
        CMap::s_detailDoodadUpdateList.LinkToTail(inst);
    }

    GxXformPop(GxXform_Tex1);
    GxXformPop(GxXform_World);
    g_theGxDevicePtr->RsPop();

    CWorldScene::RenderOccluders();
    CWorldScene::RenderOcclusionVolumes();
    CWorldScene::RenderDebugFrustum();
}

// VERIFY: 4aaed5dd manual
// OFFSET: 0x007987A0
void CWorldScene::CullDoodads(CSortEntry* entry, uint16_t fadeLevel) {
    CMapDoodadDef* doodadDef = entry->m_doodadDefList.Head();

    while (doodadDef) {
        CMapDoodadDef* next = entry->m_doodadDefList.Next(doodadDef);
        doodadDef->m_doodadDefLink.Unlink();

        if (doodadDef->m_fadeLevel < fadeLevel) {
            doodadDef = next;
            continue;
        }

        if (!doodadDef->m_model || !(doodadDef->m_flags & MAPOBJ_FLAG_PREPARED)) {
            CAaBox box;
            CAaBox bounds = doodadDef->m_model->m_shared->m_boundingBox;
            CWorldMath::TransformAABox(doodadDef->m_mat, bounds, box);
            CWorldScene::s_barrier.AddBarrier(box, 10.0f);
            doodadDef = next;
            continue;
        }

        doodadDef->m_cullPassStamp = CWorldScene::s_cullPassStamp;
        doodadDef->m_culled = 1;

        bool visible = CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(doodadDef->m_sphere) && !CWorldOcclusion::QueryVolumes(&doodadDef->m_sphere);

        if (visible) {
            doodadDef->m_culled = 0;
            visible = CWorldOcclusion::QueryBuffer(doodadDef->m_sphere.c, doodadDef->m_sphere.r, 0x10) < 2;
        }

        if (visible) {
            CWorldScene::AddDoodadDefModelToModelScene(doodadDef);
            CWorldScene::nDoodadsRendered++;
        } else if ((doodadDef->m_staticFlags & 0x400) || (doodadDef->m_sphere.c.x - CWorldScene::s_activeWorldView.x) * (doodadDef->m_sphere.c.x - CWorldScene::s_activeWorldView.x) + ((doodadDef->m_sphere.c.y - CWorldScene::s_activeWorldView.y) * (doodadDef->m_sphere.c.y - CWorldScene::s_activeWorldView.y) + (doodadDef->m_sphere.c.z - CWorldScene::s_activeWorldView.z) * (doodadDef->m_sphere.c.z - CWorldScene::s_activeWorldView.z)) < 100.0f) {
            doodadDef->m_model->SetAnimating(1);
        } else {
            doodadDef->m_model->SetAnimating(0);
        }

        doodadDef = next;
    }
}

// VERIFY: 1d148ad8 manual
// OFFSET: 0x007989C0
void CWorldScene::RenderChunksSinglePass() {
    TSExplicitList<CMapRenderChunk, offsetof(CMapRenderChunk, m_renderChunkLink)> singlePassList;

    for (uint32_t pass = 0; pass < 4; pass++) {
        switch (pass) {
            case 0:
                CMapRenderChunk::SetShaders(0, 0);
                break;

            case 1:
                CMapRenderChunk::SetShaders(0, 1);
                break;

            case 2:
                CMapRenderChunk::SetShaders(1, 0);
                break;

            case 3:
                CMapRenderChunk::SetShaders(1, 1);
                break;
        }

        for (uint32_t layers = 1; layers <= 4; layers++) {
            if (CMapRenderChunk::s_currentShaderX[layers - 1]) {
                g_theGxDevicePtr->RsSet(GxRs_PixelShader, CMapRenderChunk::s_currentShaderX[layers - 1]);
            }

            TSExplicitList<CMapRenderChunk, offsetof(CMapRenderChunk, m_renderChunkLink)>& list = CWorldScene::sortTable.m_renderChunkList[4 * layers + pass];
            CMapRenderChunk* renderChunk = list.Head();

            while (!list.IsEnd(renderChunk)) {
                CMapRenderChunk* next = list.RawNext(renderChunk);
                renderChunk->RenderSetup(1);

                if (CWorld::enables & Enable_Chunks) {
                    if (CMapRenderChunk::s_currentShaderX[layers - 1]) {
                        if (CMap::enableTerrainShaderVertex) {
                            renderChunk->RenderSinglePassVertexPixelShader();
                        } else {
                            renderChunk->RenderSinglePassPixelShader();
                        }
                    } else {
                        CMapRenderChunk::s_renderLayersFunc(renderChunk);
                    }
                }

                renderChunk->m_renderChunkLink.Unlink();

                if (CWorld::enables & Enable_ShowNormals) {
                    singlePassList.LinkToTail(renderChunk);
                } else {
                    CMap::s_mapRenderChunkUpdateList.LinkToTail(renderChunk);
                }

                renderChunk = next;
            }
        }
    }

    g_theGxDevicePtr->RsSet(GxRs_VertexShader, static_cast<void*>(nullptr));
    g_theGxDevicePtr->RsSet(GxRs_PixelShader, static_cast<void*>(nullptr));

    if (!singlePassList.IsEnd(singlePassList.Head())) {
        C44Matrix world;
        world.Translate(-CWorldScene::s_activeWorldView);
        GxXformSet(GxXform_World, world);

        CMapRenderChunk* renderChunk = singlePassList.Head();

        while (!singlePassList.IsEnd(renderChunk)) {
            CMapRenderChunk* next = singlePassList.RawNext(renderChunk);
            renderChunk->RenderSinglePass();
            CMap::s_mapRenderChunkUpdateList.LinkToTail(renderChunk);
            renderChunk = next;
        }
    }
}

// VERIFY: 981a5585 manual
// OFFSET: 0x00798DA0
void CWorldScene::RenderChunks() {
    g_theGxDevicePtr->RsPush();
    CMapRenderChunk::s_fogColor = *reinterpret_cast<CImVector*>(&g_theGxDevicePtr->m_appRenderStates[GxRs_FogColor].m_value.m_data.i[0]);

    if (CMap::enableTerrainShaderVertex) {
        C44Matrix world;
        world.Translate(-CWorldScene::s_activeWorldView);
        C44Matrix view;
        CGxMatrixStack& viewStack = g_theGxDevicePtr->m_xforms[GxXform_View];
        view = viewStack.m_mtx[viewStack.m_level];
        CMapRenderChunk::InitializeVertexShaderConstants(world, view);
    } else {
        DayNight::DNInfo* info = DayNight::GetInfo();
        GxRsSet(GxRs_FogStart, info->m_fog.m_start);
        GxRsSet(GxRs_FogEnd, info->m_fog.m_end);
        GxRsSet(GxRs_MatDiffuse, static_cast<int32_t>(0xFF7F7F7F));

        if (CMap::enableSpecularTerrain) {
            GxRsSet(GxRs_MatSpecular, static_cast<int32_t>(0xFFFFFFFF));
            GxRsSet(GxRs_MatSpecularExp, TERRAIN_SPEC_EXP);
        }

        for (int32_t i = 0; i < 5; i++) {
            GxRsSet(static_cast<EGxRenderState>(GxRs_TextureCoord0 + i), i);
            GxRsSet(static_cast<EGxRenderState>(GxRs_TexGen0 + i), GxTexGen_World);
            GxRsSet(static_cast<EGxRenderState>(GxRs_TextureShader0 + i), 1);
        }
    }

    if (CMap::gTerrainPixelShadersValid) {
        DayNight::DNInfo* info = DayNight::GetInfo();

        if (g_theGxDevicePtr->Caps().unk_0134) {
            GxRsSet(GxRs_FogColor, *reinterpret_cast<int32_t*>(&info->m_fog.m_color));
        } else {
            C4Vector color(info->m_fog.m_color);
            g_theGxDevicePtr->ShaderConstantsSet(GxSh_Pixel, 2, reinterpret_cast<const float*>(&color), 1);
        }

        if (g_theGxDevicePtr->Caps().unk_0138) {
            GxRsSet(GxRs_Fog, 1);
        }

        CShadowCache::SetShadowMapTerrain();
    } else {
        DayNight::DNInfo* info = DayNight::GetInfo();
        GxRsSet(GxRs_FogColor, *reinterpret_cast<int32_t*>(&info->m_fog.m_color));
        GxRsSet(GxRs_Fog, 1);
        GxRsSet(GxRs_ColorOp0, GxTexOp_Mod2x);
        GxRsSet(GxRs_AlphaOp0, GxTexOp_Mod);
        GxRsSet(GxRs_ColorOp1, GxTexOp_Mod);
        GxRsSet(GxRs_AlphaOp1, GxTexOp_Mod);
    }

    CWorldScene::RenderChunksSinglePass();
    CWorldScene::RenderChunksSolid();
    CWorldScene::RenderChunksZoneDebug();

    for (int32_t i = 0; i < 5; i++) {
        g_theGxDevicePtr->m_xforms[i].Identity();
    }

    g_theGxDevicePtr->RsPop();
}

// VERIFY: 8c4ec802 manual
// OFFSET: 0x00799310
void CWorldScene::AddMapObjDefGroupToSortTable(uint32_t groupIndex, void* param) {
    CMapObjDef* mapObjDef = static_cast<CMapObjDef*>(param);
    CMapObjDefGroup* mapObjDefGroup = mapObjDef->m_defGroups[groupIndex];
    CMapObjGroup* group = mapObjDef->m_owner->GetGroup(mapObjDefGroup->m_groupNum, 0);

    if (!mapObjDefGroup->m_sortTableLink.m_next) {
        CWorldScene::sortTable.m_mapObjDefGroup.LinkToTail(mapObjDefGroup);

        if (group && (group->m_flags & 0x1000) && (CWorld::enables & Enable_Water)) {
            CWorldScene::sortTable.m_interiorLiquidGroupList.LinkToTail(mapObjDefGroup);
        }

        C3Vector nearest(0.0f, 0.0f, 0.0f);
        CWorldScene::GetNearestCornerToCamera(&mapObjDefGroup->m_bbox, &nearest);
        mapObjDefGroup->m_distanceToCamera = CWorldScene::camPlane.n.z * nearest.z + CWorldScene::camPlane.n.y * nearest.y + CWorldScene::camPlane.n.x * nearest.x + CWorldScene::camPlane.d;

        if (group->m_flags & 0x40) {
            CWorldScene::s_hasPendingExteriorGroups = 1;
        }

        mapObjDefGroup->m_flags = static_cast<MapObjFlag>(mapObjDefGroup->m_flags & ~MAPOBJ_FLAG_INT_FOG);
    }

    if (s_curGroupIsInterior) {
        mapObjDefGroup->m_flags = static_cast<MapObjFlag>(mapObjDefGroup->m_flags | MAPOBJ_FLAG_INT_FOG);
    }

    CWFrustum* frustum = CWorldScene::AllocFrustum();
    *static_cast<CFrustum*>(frustum) = CWorldScene::frustumStack[CWorldScene::frustumIndex];
    mapObjDefGroup->m_frustumList.LinkToTail(frustum);
}

// VERIFY: dfda4d02 manual
// OFFSET: 0x00799430
CSortEntry::CSortEntry() {
    this->m_mapChunkList.SetLinkOffset(offsetof(CMapChunk, m_sortListLink));
    this->m_exteriorGroupList.SetLinkOffset(offsetof(CMapObjDefGroup, m_sortEntryLink));
    this->m_entityList.SetLinkOffset(offsetof(CMapEntity, m_sortEntryLink));
    this->m_doodadDefList.SetLinkOffset(offsetof(CMapDoodadDef, m_doodadDefLink));
    this->m_liquidList.SetLinkOffset(offsetof(CChunkLiquid, m_updateLink));
    this->m_occluderList.SetLinkOffset(offsetof(CWorldOccluder, m_link));
    this->m_antiOccluderList.SetLinkOffset(offsetof(CWorldAntiOccluder, m_link));
    this->m_holedChunkList.SetLinkOffset(offsetof(CMapChunk, m_sortListLink));
    this->m_unkList9.SetLinkOffset(8);
}

// VERIFY: 8dcf7c04 manual
// OFFSET: 0x007998A0
void CWorldScene::AddDoodadDefs(TSList<CMapBaseObjLink, TSGetExplicitLink<CMapBaseObjLink> >* linkList, uint32_t index) {
    if (!(CWorld::enables & Enable_Doodads)) {
        return;
    }

    for (CMapBaseObjLink* link = linkList->Head(); link; link = linkList->Next(link)) {
        CMapDoodadDef* doodadDef = static_cast<CMapDoodadDef*>(link->m_owner);

        if (!(doodadDef->m_flags & MAPOBJ_FLAG_PREPARED) || doodadDef->m_doodadDefLink.m_next || !doodadDef->m_model) {
            continue;
        }

        uint32_t bucket = index;
        float dist = doodadDef->m_sphere.c.y * CWorldScene::camPlaneXY.n.y + doodadDef->m_sphere.c.z * CWorldScene::camPlaneXY.n.z + doodadDef->m_sphere.c.x * CWorldScene::camPlaneXY.n.x + CWorldScene::camPlaneXY.d;

        if (dist > 0.0f) {
            bucket = CMath::fint_(dist * CBDIVISOR - 0.5f);

            if (bucket >= 64) {
                continue;
            }

            if (bucket < index) {
                bucket = index;
            }
        }

        CWorldScene::sortTable.m_table[bucket].m_doodadDefList.LinkToTail(doodadDef);
    }
}

// VERIFY: 9f2071e4 manual
// OFFSET: 0x00799980
void CWorldScene::CullDoodadsExterior(TSList<CMapBaseObjLink, TSGetExplicitLink<CMapBaseObjLink> >* linkList, uint8_t fadeLevel) {
    CMapBaseObjLink* link = linkList->Head();

    while (!linkList->IsEnd(link)) {
        CMapDoodadDef* doodadDef = static_cast<CMapDoodadDef*>(link->m_owner);

        if (doodadDef->m_fadeLevel < fadeLevel) {
            if (CMap::header.m_flags & 0x8) {
                return;
            }
        } else if (doodadDef->m_model && (doodadDef->m_flags & MAPOBJ_FLAG_PREPARED)) {
            if (doodadDef->m_cullPassStamp != CWorldScene::s_cullPassStamp) {
                doodadDef->m_cullPassStamp = CWorldScene::s_cullPassStamp;
                doodadDef->m_culled = 1;
                int32_t culled = CWorldScene::FrustumCull(doodadDef->m_sphere.c, doodadDef->m_sphere.r);

                if (!culled) {
                    doodadDef->m_culled = 0;
                }

                if (!culled && CWorldOcclusion::QueryBuffer(doodadDef->m_sphere.c, doodadDef->m_sphere.r, 0x10) < 2) {
                    CWorldScene::AddDoodadDefModelToModelScene(doodadDef);
                    CWorldScene::nDoodadsRendered++;
                } else {
                    C3Vector delta = doodadDef->m_sphere.c - CWorldScene::s_activeWorldView;

                    if ((doodadDef->m_staticFlags & 0x400) || delta.x * delta.x + delta.z * delta.z + delta.y * delta.y < 100.0f) {
                        doodadDef->m_model->SetAnimating(1);
                    } else {
                        doodadDef->m_model->SetAnimating(0);
                    }
                }
            }
        } else {
            CAaBox box;
            CAaBox bounds = doodadDef->m_model->m_shared->m_boundingBox;
            CWorldMath::TransformAABox(doodadDef->m_mat, bounds, box);
            CWorldScene::s_barrier.AddBarrier(box, 10.0f);
        }

        link = linkList->RawNext(link);
    }
}

// VERIFY: c1e21c79 manual
// OFFSET: 0x00799B70
void CWorldScene::CullDoodadsInterior(TSList<CMapBaseObjLink, TSGetExplicitLink<CMapBaseObjLink> >* linkList, CWFrustum* frustum, uint16_t fadeLevel, int32_t interior) {
    for (CMapBaseObjLink* link = linkList->Head(); link; link = linkList->Next(link)) {
        CMapDoodadDef* doodadDef = static_cast<CMapDoodadDef*>(link->m_owner);

        if (doodadDef->m_fadeLevel < fadeLevel) {
            continue;
        }

        if (!doodadDef->m_model || !(doodadDef->m_flags & MAPOBJ_FLAG_PREPARED)) {
            CAaBox box;
            CAaBox bounds = doodadDef->m_model->m_shared->m_boundingBox;
            CWorldMath::TransformAABox(doodadDef->m_mat, bounds, box);
            CWorldScene::s_barrier.AddBarrier(box, 10.0f);
            continue;
        }

        if (doodadDef->m_cullPassStamp != CWorldScene::s_cullPassStamp) {
            doodadDef->m_cullPassStamp = CWorldScene::s_cullPassStamp;
            doodadDef->m_culled = 1;
            int32_t animate = 1;

            if (!(doodadDef->m_staticFlags & 0x400)) {
                float dz = doodadDef->m_sphere.c.z - CWorldScene::s_activeWorldView.z;
                float dy = doodadDef->m_sphere.c.y - CWorldScene::s_activeWorldView.y;
                float dx = doodadDef->m_sphere.c.x - CWorldScene::s_activeWorldView.x;

                if (dx * dx + (dy * dy + dz * dz) >= 100.0f) {
                    animate = 0;
                }
            }

            doodadDef->m_model->SetAnimating(animate);
        }

        if (!doodadDef->m_culled) {
            continue;
        }

        for (CWFrustum* test = frustum; test; test = test->m_link.Next()) {
            if (test->Cull(doodadDef->m_sphere.c, doodadDef->m_sphere.r)) {
                doodadDef->m_culled = 0;

                if (interior) {
                    doodadDef->m_flags = static_cast<MapObjFlag>(doodadDef->m_flags | MAPOBJ_FLAG_INT_FOG);
                } else {
                    doodadDef->m_flags = static_cast<MapObjFlag>(doodadDef->m_flags & ~MAPOBJ_FLAG_INT_FOG);
                }

                CWorldScene::AddDoodadDefModelToModelScene(doodadDef);
                CWorldScene::nDoodadsRendered++;
                break;
            }
        }
    }
}

// VERIFY: 1b6761ad manual
// OFFSET: 0x00799D40
void CWorldScene::CullChunks(CSortEntry* entry, uint32_t index) {
    C3Vector corner(0.0f, 0.0f, 0.0f);
    int32_t occlude = (CWorld::enables & Enable_Culling) && index < 63;

    CMapChunk* chunk = entry->m_mapChunkList.Head();

    while (chunk) {
        CMapChunk* next = entry->m_mapChunkList.Next(chunk);
        chunk->m_sortListLink.Unlink();

        if (CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(chunk->m_bbox) && !CWorldOcclusion::QueryVolumes(&chunk->m_sphere) && !CWorldOcclusion::QueryBuffer(&chunk->m_bbox, 0)) {
            CWorldScene::CullDoodadsExterior(&chunk->m_doodadDefLinkList, CWorldView::GetFadeLevelForDistance(chunk->m_distToCamera));

            if (CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(chunk->m_bbox2) && !CWorldOcclusion::QueryBuffer(&chunk->m_bbox2, 0)) {
                if (chunk->m_header->m_holes != 0xFFFF) {
                    CWorldScene::s_chunksRendered++;
                    chunk->RenderPrep();

                    CMapRenderChunk* renderChunk = chunk->m_renderChunk;

                    if (renderChunk) {
                        uint32_t layerCount = (renderChunk->m_unkFlags & 8) ? renderChunk->m_layersCount : 0;
                        uint32_t variant = 0;

                        if (layerCount) {
                            uint32_t flags = renderChunk->m_renderFlags;
                            variant = (flags & 1) ? ((flags & 4) | 2) >> 1 : (flags >> 1) & 2;
                        }

                        CWorldScene::sortTable.m_renderChunkList[4 * layerCount + variant].LinkToTail(chunk->m_renderChunk);
                    }
                }

                if (occlude) {
                    if (chunk->m_header->m_holes) {
                        CWorldScene::sortTable.m_table[index].m_holedChunkList.LinkToTail(chunk);
                    } else {
                        chunk->GetVertexPosition(CMapChunk::cornerVertexIndex[CMapChunk::nearCornerIndex], &corner);

                        int32_t bucket = 0;
                        float distance = CWorldScene::DistanceToCamPlaneXY(&corner);

                        if (distance <= 0.0f || (bucket = CMath::fint_(distance * CBDIVISOR - 0.5f)) < 64) {
                            CWorldScene::sortTable.m_table[bucket].m_holedChunkList.LinkToTail(chunk);
                        }
                    }
                }
            }
        }

        chunk = next;
    }
}

// VERIFY: 19b0b167 manual
// OFFSET: 0x00799F80
void CWorldScene::CullThroughPortal(CRect* rect) {
    CWorldScene::frustumIndex++;
    CWorldScene::frustumStack[CWorldScene::frustumIndex] = CWorldScene::frustumStack[CWorldScene::frustumIndex - 1];
    CWorldScene::FrustumSet(rect);

    CMapObjDefGroup* group = CWorldScene::sortTable.m_pendingExteriorGroupList.Head();

    while (group) {
        CMapObjDefGroup* next = CWorldScene::sortTable.m_pendingExteriorGroupList.Next(group);
        group->m_sortEntryLink.Unlink();

        CMapObjDef* def = static_cast<CMapObjDef*>(group->m_parentLinkList.Head()->m_ref);
        bool overlaps = false;

        for (CMapObjDefGroup* visible = CWorldScene::sortTable.m_mapObjDefGroup.Head(); visible; visible = CWorldScene::sortTable.m_mapObjDefGroup.Next(visible)) {
            const CAaBox& a = visible->m_bbox;
            const CAaBox& b = group->m_bbox;

            if (a.t.x >= b.b.x && a.t.y >= b.b.y && a.t.z >= b.b.z && a.b.x <= b.t.x && a.b.y <= b.t.y && a.b.z <= b.t.z) {
                overlaps = true;
                break;
            }
        }

        if ((overlaps || CWorldScene::frustumPortalView.m_maxViewDepth >= 0.0f) && CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(group->m_bbox)) {
            def->CullMapObjDefGroupFromExterior(group, rect, 1);

            uint16_t fadeLevel = CWorldView::GetFadeLevelForDistance(group->m_distanceToCamera);
            int32_t interior = group->m_flags & 0x8000;
            CWorldScene::CullDoodadsInterior(&group->m_doodadDefLinkList, group->m_frustumList.Head(), fadeLevel, interior);
            CWorldScene::CullEntitysInterior(&group->m_entityLinkList, group->m_frustumList.Head(), 1, interior);
        }

        group = next;
    }

    CWorldScene::frustumIndex--;
}

// VERIFY: 520d6234 manual
// OFFSET: 0x0079A160
void CWorldScene::CullMapObjDefGroups(CSortEntry* entry, CRect* rect, uint32_t index) {
    CMapObjDefGroup* group = entry->m_exteriorGroupList.Head();

    while (group) {
        CMapObjDefGroup* next = entry->m_exteriorGroupList.Next(group);
        group->m_sortEntryLink.Unlink();

        CMapObjDef* mapObjDef = static_cast<CMapObjDef*>(group->m_parentLinkList.Head()->m_ref);

        if (CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(group->m_bbox) && !CWorldOcclusion::QueryVolumes(&group->m_sphere) && !CWorldOcclusion::QueryBuffer(&group->m_bbox, 0)) {
            mapObjDef->CullMapObjDefGroupFromExterior(group, rect, 0);
            CWorldScene::AddDoodadDefs(&group->m_doodadDefLinkList, index);
        }

        group = next;
    }
}

// VERIFY: da3cee1d manual
// OFFSET: 0x0079A260
void CWorldScene::CullMapObjDefGroup() {
    for (CMapObjDefGroup* group = CWorldScene::sortTable.m_mapObjDefGroup.Head(); group; group = CWorldScene::sortTable.m_mapObjDefGroup.Next(group)) {
        CMapObjDef* mapObjDef = static_cast<CMapObjDef*>(group->m_parentLinkList.Head()->m_ref);
        uint32_t groupFlags = mapObjDef->m_owner->GetGroupFlags(group->m_groupNum);

        if (mapObjDef != CWorldScene::s_viewerMapObjDef && (groupFlags & 0x10008)) {
            continue;
        }

        uint16_t fadeLevel = CWorldView::GetFadeLevelForDistance(group->m_distanceToCamera);
        int32_t interior = group->m_flags & MAPOBJ_FLAG_INT_FOG;
        CWorldScene::CullDoodadsInterior(&group->m_doodadDefLinkList, group->m_frustumList.Head(), fadeLevel, group->m_flags & MAPOBJ_FLAG_INT_FOG);
        CWorldScene::CullEntitysInterior(&group->m_entityLinkList, group->m_frustumList.Head(), 0, interior);
    }
}

// VERIFY: ce664a92 manual
// OFFSET: 0x0079A320
CSortTable::CSortTable() {
    for (uint32_t i = 0; i < 64; i++) {
        this->m_bucketPlanes[i].x = 0.0f;
        this->m_bucketPlanes[i].w = 0.0f;
        this->m_bucketPlanes[i].y = 0.0f;
        this->m_bucketPlanes[i].z = 1.0f;
    }

    this->m_pendingExteriorGroupList.SetLinkOffset(offsetof(CMapObjDefGroup, m_sortEntryLink));
    this->m_detailDoodadInstList.SetLinkOffset(offsetof(CDetailDoodadInst, m_link));
    this->m_mapObjDefGroup.SetLinkOffset(offsetof(CMapObjDefGroup, m_sortTableLink));
    this->m_interiorLiquidGroupList.SetLinkOffset(offsetof(CMapObjDefGroup, m_liquidLink));
    this->m_visibleEntityList.SetLinkOffset(offsetof(CMapEntity, m_sortEntryLink));
    this->m_culledEntityList.SetLinkOffset(offsetof(CMapEntity, m_sortEntryLink));
    this->m_visibleOccluderList.SetLinkOffset(offsetof(CWorldOccluder, m_link));
    this->m_horizonChunkList.SetLinkOffset(offsetof(CMapAreaLow, m_horizonLink));
    this->m_horizonMapObjList.SetLinkOffset(offsetof(CMapObjDef, m_horizonLink));
}

// VERIFY: 4e2f336e manual
// OFFSET: 0x0079A790
void CWorldScene::CullSortTable(CRect* rect) {
    CWorldScene::CreateOcclusionVolumes(&CWorldScene::s_activeWorldView, CWorldScene::s_frustumCorners, 0);
    CWorldScene::frustumIndex++;
    CWorldScene::frustumStack[CWorldScene::frustumIndex] = CWorldScene::frustumStack[CWorldScene::frustumIndex - 1];
    CWorldScene::FrustumSet(rect);

    CSortEntry* entry = CWorldScene::sortTable.m_table;

    for (uint32_t i = 0; i < 64; i++) {
        CWorldScene::CullChunks(entry, i);
        CWorldScene::CullMapObjDefGroups(entry, rect, i);
        CWorldScene::CullLiquid(entry);
        CWorldScene::CullEntitys(entry);
        CWorldScene::CullDoodads(entry, CWorldView::GetFadeLevelForDistance(i * 33.333332f));
        CWorldScene::UpdateOcclusion(entry);
        entry++;
    }

    CWorldScene::CullHorizon(rect);
    CWorldScene::frustumIndex--;
}

// VERIFY: 8a7f0f01 manual
// OFFSET: 0x007A3E40
void World::AddAaBoxFacets(CAaBox* box, FacetData* facets) {
    static const int32_t s_boxTriIndex[36] = { 0, 2, 3, 0, 1, 2, 1, 6, 2, 1, 5, 6, 4, 6, 5, 4, 7, 6, 0, 7, 4, 0, 3, 7, 0, 5, 1, 0, 4, 5, 2, 7, 3, 2, 6, 7 };

    if (box->t.x <= box->b.x || box->t.y <= box->b.y || box->t.z <= box->b.z)
        return;

    uint32_t firstFacet = facets->facets.Count();

    C3Vector corner[8];
    corner[0] = { box->b.x, box->t.y, box->b.z };
    corner[1] = { box->t.x, box->t.y, box->b.z };
    corner[2] = { box->t.x, box->b.y, box->b.z };
    corner[3] = { box->b.x, box->b.y, box->b.z };
    corner[4] = { box->b.x, box->t.y, box->t.z };
    corner[5] = { box->t.x, box->t.y, box->t.z };
    corner[6] = { box->t.x, box->b.y, box->t.z };
    corner[7] = { box->b.x, box->b.y, box->t.z };

    C4Plane plane[6];
    plane[0].From3Pos(corner[0], corner[2], corner[3]);
    plane[1].From3Pos(corner[1], corner[6], corner[2]);
    plane[2].From3Pos(corner[4], corner[6], corner[5]);
    plane[3].From3Pos(corner[0], corner[7], corner[4]);
    plane[4].From3Pos(corner[0], corner[5], corner[1]);
    plane[5].From3Pos(corner[2], corner[7], corner[3]);

    CFacet facet(0.0f);
    for (int32_t i = 0; i < 36; i += 3) {
        facet.plane = plane[i / 6];
        facet.v[0] = corner[s_boxTriIndex[i]];
        facet.v[1] = corner[s_boxTriIndex[i + 1]];
        facet.v[2] = corner[s_boxTriIndex[i + 2]];
        facets->facets.Add(1, &facet);
    }

    uint32_t count = facets->facets.Count();
    facets->facetIds.SetCount(count);
    for (uint32_t i = firstFacet; i < count; i++)
        facets->facetIds[i] = 0;
}

// VERIFY: 215cffa8 manual
// OFFSET: 0x007A6B90
void __stdcall CWorldScene::CalcScreenRectFromVerts(CRect& rect, C3Vector* clippedVerts, uint32_t clippedCount) {
    rect.minX = 3.4028235e38f;
    rect.maxX = -3.4028235e38f;
    rect.maxY = -3.4028235e38f;
    rect.minY = 3.4028235e38f;

    for (uint32_t i = 0; i < clippedCount; i++) {
        if (rect.minX > clippedVerts[i].x)
            rect.minX = clippedVerts[i].x;
        if (rect.maxX < clippedVerts[i].x)
            rect.maxX = clippedVerts[i].x;
        if (rect.minY > clippedVerts[i].y)
            rect.minY = clippedVerts[i].y;
        if (rect.maxY < clippedVerts[i].y)
            rect.maxY = clippedVerts[i].y;
    }
}

// VERIFY: 2d6bb11f manual
// OFFSET: 0x007A8320
void CWorldScene::SetWorldProjection(C44Matrix& mat) {
    if (CShaderEffect::s_enableShaders) {
        CGxMatrixStack& view = g_theGxDevicePtr->m_xforms[GxXform_View];
        C44Matrix viewMatrix;
        viewMatrix = view.m_mtx[view.m_level];
        C44Matrix worldView = mat * viewMatrix;
        worldView = worldView.Transpose();
        g_theGxDevicePtr->ShaderConstantsSet(GxSh_Vertex, 31, reinterpret_cast<const float*>(&worldView), 4);
    }

    CGxMatrixStack& world = g_theGxDevicePtr->m_xforms[GxXform_World];
    world.m_dirty = 1;
    world.m_flags[world.m_level] &= ~CGxMatrixStack::F_Identity;
    world.m_mtx[world.m_level] = mat;
}

// VERIFY: 1a7d5728 manual
// OFFSET: 0x0077F0B0
int32_t World::QueryObjectVisible(CMapEntity* entity) {
    if (!entity) {
        return 0;
    }

    return entity->m_culled == 0;
}

// VERIFY: 839ab621 auto
// OFFSET: 0x0077F2E0
void World::TickObject(CMapEntity* entity) {
    entity->Tick();
}

// VERIFY: 8b1b26ac manual
// OFFSET: 0x0077F2F0
void World::ObjectSetHidden(CMapEntity* entity, int32_t hidden) {
    entity->m_staticFlags ^= (entity->m_staticFlags ^ (4 * hidden)) & 0x4;
}

// VERIFY: 8e291dc6 manual
// OFFSET: 0x0077F310
int32_t World::Intersect(const C3Vector& start, const C3Vector& end, C3Vector* hit, float* distance, uint32_t flags, HitTestDebug* debug) {
    return CMap::Intersect(const_cast<C3Vector*>(&start), const_cast<C3Vector*>(&end), hit, distance, flags, debug);
}

// VERIFY: e6c442dd manual
// OFFSET: 0x00792E60
void CWorldScene::AddEntityToSortTable(CMapEntity* entity) {
    uint32_t flags = entity->m_staticFlags;

    if ((flags & 0x4000) && !entity->m_model->m_attachList) {
        CWorldScene::sortTable.m_culledEntityList.LinkToTail(entity);
        return;
    }

    C3Vector corner = C3Vector(0.0f, 0.0f, 0.0f);
    CWorldScene::GetNearestCornerToCamera(&entity->m_bbox, &corner);
    entity->m_culled = 2;
    entity->m_distanceToCamera = CWorldScene::camPlane.n.x * corner.x + CWorldScene::camPlane.n.y * corner.y + CWorldScene::camPlane.n.z * corner.z + CWorldScene::camPlane.d;

    if (flags & 0x1) {
        CWorldScene::sortTable.m_culledEntityList.LinkToTail(entity);
        return;
    }

    int32_t bucket = 0;
    float distance = corner.y * CWorldScene::camPlaneXY.n.y + corner.z * CWorldScene::camPlaneXY.n.z + corner.x * CWorldScene::camPlaneXY.n.x + CWorldScene::camPlaneXY.d;

    if (distance > 0.0f) {
        bucket = static_cast<int32_t>(distance * CBDIVISOR);

        if (bucket >= 64) {
            return;
        }
    }

    CWorldScene::sortTable.m_table[bucket].m_entityList.LinkToHead(entity);
}

// VERIFY: a1012270 manual
// OFFSET: 0x00780500
void CWorldScene::SetCameraTarget(CMapEntity* entity, WOWGUID transportGuid) {
    CWorldScene::camTargetEntity = entity;

    if (transportGuid) {
        C44Matrix transport;
        MovementGetTransportMtxX(transportGuid, &transport, WOWGUID(), __FILE__, __LINE__);

        if (CWorldScene::camTransportGUID) {
            CWorldScene::camTransportView = transport.AffineInverse() * CWorldScene::camTransportMtx * World::s_m2Scene->m_view;
        } else {
            CWorldScene::camTransportView = World::s_m2Scene->m_view;
        }

        CWorldScene::camTransportMtx = transport;
    } else {
        CWorldScene::camTransportView = World::s_m2Scene->m_view;
    }

    CWorldScene::camTransportGUID = transportGuid;
}

// VERIFY: c79fbbdc manual
// OFFSET: 0x007A1BC0
void CMap::UpdateEntity(CMapEntity* entity) {
    entity->m_staticFlags &= 0xFFFFFC96;
    entity->m_flags = static_cast<MapObjFlag>((entity->m_flags & ~0x7) | 0x1);
    CMap::LinkStaticEntity(entity);

    if (entity->m_staticFlags & 0x1) {
        entity->UpdateMapObjLiquid();
    } else {
        int32_t liquidType;
        int32_t a4;

        if (CMap::QueryLiquidStatus(&entity->m_position, &liquidType, &entity->m_liquidHeight, &a4, 1)) {
            entity->m_staticFlags |= 0x20;

            if (entity->m_liquidHeight <= entity->m_bbox.t.z) {
                entity->m_staticFlags |= 0x40;
            } else {
                entity->m_staticFlags &= ~0x40;
            }

            entity->m_liquidType = static_cast<uint16_t>(liquidType);
        }
    }

    if (!(entity->m_staticFlags & 0x2000) && (entity->m_staticFlags & 0x40) && entity->m_liquidHeight + 0.0099999998f > entity->m_position.z) {
        uint32_t areaId;

        if (CMap::QueryAreaId(entity, areaId)) {
            const LiquidTypeRec* liquid = GetLiquidTypeRecForArea(areaId, entity->m_liquidType);

            if (liquid && ((liquid->m_flags & 0x4) || entity->m_liquidHeight > entity->m_position.z)) {
                entity->m_staticFlags ^= (entity->m_staticFlags ^ (liquid->m_flags << 8)) & 0x100;
                entity->m_staticFlags ^= (entity->m_staticFlags ^ (liquid->m_flags << 8)) & 0x200;
            }
        }
    }

    if (entity->m_flags & 0x2) {
        if (entity->m_staticFlags & 0x1000) {
            entity->m_targetDiffuseLightScale = (2.5f - 1.0f) * (entity->m_m2DiffuseColor.a * 0.0039215689f) + 1.0f;
        } else {
            entity->m_targetDiffuseLightScale = 1.0f;
        }

        return;
    }

    C3Vector ambient = CMap::s_mapLight->m_light.m_ambColor;
    float b = ambient.z > 0.0f ? (ambient.z < 1.0f ? ambient.z * 255.0f + 0.5f : 255.0f) : 0.0f;
    float g = ambient.y > 0.0f ? (ambient.y < 1.0f ? ambient.y * 255.0f + 0.5f : 255.0f) : 0.0f;
    float r = ambient.x > 0.0f ? (ambient.x < 1.0f ? 255.0f * ambient.x + 0.5f : 255.0f) : 0.0f;
    CImVector color;
    color.a = 0xFF;
    color.b = static_cast<uint8_t>(static_cast<int32_t>(b));
    color.g = static_cast<uint8_t>(static_cast<int32_t>(g));
    color.r = static_cast<uint8_t>(static_cast<int32_t>(r));
    entity->m_targetAmbientColor = color;

    if (CShadowCache::GetShadowMode() > 1 || (entity->m_flags & 0x200) || !CMap::QueryShadow(&entity->m_position)) {
        entity->m_targetDiffuseLightScale = 2.5f;
    } else {
        entity->m_staticFlags |= 0x8;
        entity->m_targetDiffuseLightScale = dirLightScaleAmount;
    }
}

// VERIFY: c07aad22 manual
// OFFSET: 0x00791240
void CWorldScene::CullModelSplitBody(CM2Model* model, uint32_t boneId, int32_t skip) {
    if (skip) {
        model->m_fFlags &= ~0x2000;
    } else {
        CAaBox box = model->GetSplitBodyBoundingBox(boneId);
        uint32_t culled = CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(box) == WorldCull_outside;
        model->m_fFlags ^= (model->m_fFlags ^ (culled << 13)) & 0x2000;
    }

    for (CM2Model* attached = model->m_attachList; attached; attached = attached->m_attachNext) {
        CWorldScene::CullModelRecursive(attached);
    }
}

// VERIFY: a400f932 manual
// OFFSET: 0x00793980
void CWorldScene::RenderEntitys() {
    int32_t streaming = SFile::IsStreamingMode();
    CMapEntity* entity = CWorldScene::sortTable.m_visibleEntityList.Head();

    while (entity) {
        CMapEntity* next = CWorldScene::sortTable.m_visibleEntityList.Next(entity);
        entity->m_sortEntryLink.Unlink();

        if (entity->m_model) {
            if (entity->m_model->IsLoaded(0, 1)) {
                CWorldScene::CullModelSplitBody(entity->m_model, entity->unk_00A4, (entity->m_staticFlags >> 14) & 1);
            } else if (streaming) {
                entity->m_model->LinkToAsyncHead(0);
            }
        }

        CM2Model* model = entity->m_model;

        if (model && model->IsLoaded(0, 0) && !(entity->m_staticFlags & 0x4)) {
            CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(WOWGUID{ entity->m_funcParam64 }, TYPE_UNIT, __FILE__, __LINE__));

            if (!unit) {
                if (entity->m_staticFlags & 0x800) {
                    M2SequenceInfo info;
                    info.Clear();
                    model->GetSequenceInfo(0, 0, info);
                    CWorldScene::RequestModelStreaming(model, &info.m_bounds);
                }
            } else {
                if ((entity->m_staticFlags & 0x800) && unit->m_unit->m_unitFieldBytes1.m_standState != UNIT_SUBMERGED) {
                    CAaBox standBox = unit->GetStandBox();
                    CWorldScene::RequestModelStreaming(model, &standBox);
                }

                unit->Render();
            }
        }

        entity = next;
    }
}


// VERIFY: 28592f1e auto
// OFFSET: 0x00783A40
bool World::GetFacets(CAaBox* box, FacetData* facets, uint32_t flags, uint32_t* status) {
    return World::GetFacets(box, box, facets, flags, status);
}

// VERIFY: 4313dc59 manual
// OFFSET: 0x00782740
uint32_t World::TriDataToFacetData(void* unused, FacetData* facets, WOWGUID guid) {
    uint32_t first = facets->facets.Count();

    for (uint32_t b = 0; b < World::TriData::nBatches; b++) {
        World::TriData::Batch& batch = World::TriData::batches[b];
        const uint16_t* indices = batch.m_indices;

        for (uint32_t f = 0; f < batch.m_faceCount; f++, indices += 3) {
            CFacet* facet = facets->facets.New();
            const C3Vector& a = batch.m_vertexList[indices[0]];
            const C3Vector& b1 = batch.m_vertexList[indices[1]];
            const C3Vector& c = batch.m_vertexList[indices[2]];
            C3Vector e1 = b1 - a;
            C3Vector e2 = c - a;
            float nx = e1.y * e2.z - e1.z * e2.y;
            float ny = e1.z * e2.x - e2.z * e1.x;
            float nz = e1.x * e2.y - e2.x * e1.y;
            const C44Matrix& m = *batch.m_matrix;
            facet->plane.n.x = m.c0 * nz + m.b0 * ny + m.a0 * nx;
            facet->plane.n.y = m.c1 * nz + m.b1 * ny + m.a1 * nx;
            facet->plane.n.z = nx * m.a2 + (ny * m.b2 + nz * m.c2);
            facet->v[0] = a * m;
            facet->v[1] = b1 * m;
            facet->v[2] = c * m;
            float lengthSq = facet->plane.n.z * facet->plane.n.z + facet->plane.n.y * facet->plane.n.y + facet->plane.n.x * facet->plane.n.x;

            if (lengthSq == 0.0f) {
                SysMsgPrintf(SYSMSG_ERROR, 2, "Found degenerate triangle -- data needs to be fixed\n");
                facet->plane.n.x = 0.0f;
                facet->plane.n.y = 0.0f;
                facet->plane.n.z = 1.0f;
            } else {
                float scale = 1.0f / sqrtf(lengthSq);
                facet->plane.n.x *= scale;
                facet->plane.n.y *= scale;
                facet->plane.n.z *= scale;
            }

            facet->plane.d = -(facet->v[0].z * facet->plane.n.z + facet->v[0].y * facet->plane.n.y + facet->v[0].x * facet->plane.n.x);
        }
    }

    uint32_t count = facets->facets.Count();

    if (count == first) {
        return 0;
    }

    facets->facetIds.SetCount(count);

    for (uint32_t i = first; i < count; i++) {
        facets->facetIds.m_data[i] = guid.m_guid;
    }

    return count;
}

// VERIFY: a052c929 auto
// OFFSET: 0x0077F330
bool World::GetFacets(CFrustum* frustum, FacetData* facets, uint32_t flags, uint32_t* status) {
    return CMap::QueryFacets(frustum, facets, flags, status);
}

// VERIFY: 95a6852b manual
// OFFSET: 0x00791380
bool World::NDCClip(C3Vector* verts, uint32_t count, C3Vector*** outVerts, uint32_t* outCount) {
    // STATIC: 0x00CDD400
    static C3Vector s_clipNewVerts[32];

    World::CLIPINFO infos[32];
    World::CLIPINFO newInfos[32];
    World::CLIPINFO* outInfos[32];
    World::CLIPINFO* inInfos[32];
    C3Vector* newVert = s_clipNewVerts;
    World::CLIPINFO* newInfo = newInfos;
    uint32_t andCode = 0xFFFFFFFF;
    uint32_t orCode = 0;

    if (!count) {
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        infos[i].Set(verts[i]);
        inInfos[i] = &infos[i];
        andCode &= infos[i].outcode;
        orCode |= infos[i].outcode;
        s_clipInVerts[i] = &verts[i];
    }

    if (andCode) {
        return false;
    }

    if (!orCode) {
        *outVerts = s_clipInVerts;
        *outCount = count;
        return true;
    }

    World::CLIPPOLY polyA;
    polyA.verts = s_clipInVerts;
    polyA.infos = inInfos;
    polyA.count = count;
    World::CLIPPOLY polyB;
    polyB.verts = s_clipOutVerts;
    polyB.infos = outInfos;
    polyB.count = 0;
    World::CLIPPOLY* src = &polyA;
    World::CLIPPOLY* dst = &polyB;
    uint32_t mask = 0x80000000;

    for (uint32_t plane = 0; plane < 6; plane++) {
        if (mask & orCode) {
            uint32_t prev = src->count - 1;
            uint32_t prevCode = mask & src->infos[prev]->outcode;
            dst->count = 0;

            for (uint32_t i = 0; i < src->count; i++) {
                uint32_t code = mask & src->infos[i]->outcode;

                if (prevCode != code) {
                    float denom = src->infos[prev]->d[plane] - src->infos[i]->d[plane];

                    if (0.0f == denom) {
                        denom = 0.000099999997f;
                    }

                    float t = src->infos[prev]->d[plane] / denom;
                    const C3Vector* a = src->verts[prev];
                    const C3Vector* b = src->verts[i];
                    newVert->x = (b->x - a->x) * t + a->x;
                    newVert->y = (b->y - a->y) * t + a->y;
                    newVert->z = t * (b->z - a->z) + a->z;
                    newInfo->Set(*newVert);
                    dst->verts[dst->count] = newVert;
                    dst->infos[dst->count] = newInfo;
                    newVert++;
                    dst->count++;
                    newInfo++;
                }

                if (!code) {
                    dst->verts[dst->count] = src->verts[i];
                    dst->infos[dst->count] = src->infos[i];
                    dst->count++;
                }

                prev = i;
                prevCode = code;
            }

            if (!dst->count) {
                return false;
            }

            World::CLIPPOLY* swap = src;
            src = dst;
            dst = swap;
        }

        mask >>= 1;
    }

    *outVerts = src->verts;
    *outCount = src->count;
    return true;
}

// VERIFY: 8a7aac37 auto
// OFFSET: 0x0077EFE0
int32_t World::FrustumCull(const CAaBox& box) {
    return CWorldScene::FrustumCull(const_cast<CAaBox*>(&box));
}

// VERIFY: aa42c615 auto
// OFFSET: 0x0077F080
int32_t World::QueryMapId() {
    return s_mapId;
}

// VERIFY: a272ba51 auto
// OFFSET: 0x0077F540
int32_t World::ProjectPositionCallback(const C3Vector& position, float& height, void* param) {
    C3Vector end = { position.x, position.y, position.z - 20.0f };
    C3Vector hit = { 0.0f, 0.0f, 0.0f };
    float distance = 1.0f;

    if (!CMap::Intersect(const_cast<C3Vector*>(&position), &end, &hit, &distance, 0x100111, nullptr)) {
        return 0;
    }

    height = hit.z;
    return 1;
}

// VERIFY: 7a574776 auto
// OFFSET: 0x0077F0D0
int32_t World::QueryMapObjZoneName(CMapEntity* entity, const char** zoneName) {
    if (!entity) {
        return 0;
    }

    return entity->QueryMapObjZoneName(zoneName);
}

// VERIFY: 9105b684 auto
// OFFSET: 0x0077F0F0
int32_t World::QueryMapObjSubzoneName(CMapEntity* entity, const char** subzoneName, int32_t* areaId) {
    if (!entity) {
        return 0;
    }

    return entity->QueryMapObjSubzoneName(subzoneName, areaId);
}

// VERIFY: a5a2719e auto
// OFFSET: 0x0077F160
bool World::QueryMapObjIDs(CMapEntity* entity, int32_t* wmoId, int32_t* wmoHash, int32_t* groupNum) {
    if (!entity) {
        return false;
    }

    return entity->QueryMapObjIDs(wmoId, wmoHash, groupNum);
}

// VERIFY: 3e1ad2c7 manual
// OFFSET: 0x0077F1B0
bool World::QueryMapObjAreaTable(CMapEntity* entity, WMOAreaTableRec** groupRec, WMOAreaTableRec** rootRec, int32_t* wmoGroupId) {
    return entity && entity->QueryMapObjAreaTable(groupRec, rootRec, wmoGroupId);
}

// VERIFY: 018bde21 auto
// OFFSET: 0x0077FA00
int32_t World::QueryAreaIdAtPosition(const C3Vector* position, int32_t* areaId) {
    if (CMap::bDungeon) {
        MapRec* map = g_mapDB.GetRecord(s_mapId);

        if (map->m_areaTableID) {
            *areaId = map->m_areaTableID;
            return 1;
        }
    }

    C3Vector start(position->x, position->y, position->z + 0.33f);
    C3Vector end(position->x, position->y, position->z - 1760.0f);
    float terrainDistance = 1.0f;
    CMapChunk* chunk = nullptr;
    bool hitTerrain = CMap::VectorIntersectTerrain(&start, &end, &terrainDistance, 0x20100, &chunk);
    float objectDistance = 1.0f;
    bool hitObject = false;
    CMapObjDef* mapObjDef = nullptr;
    CMapObjDefGroup* mapObjDefGroup = nullptr;

    if ((CWorld::enables & Enable_MapObjs) && CMap::VectorIntersect(&start, &end, 0x10, 1280, &objectDistance, nullptr, nullptr, &mapObjDef, &mapObjDefGroup)) {
        hitObject = true;
    }

    if ((!hitTerrain || (hitObject && objectDistance <= terrainDistance)) && hitObject) {
        CMapObjGroup* group = mapObjDef->m_owner->GetGroup(mapObjDefGroup->m_groupNum, 0);
        WMOAreaTableRec* rec = GetWMOAreaRec(mapObjDef->m_owner->m_header->m_wmoID, mapObjDef->m_nameSet, group->m_wmoGroupId);

        if (rec && rec->m_areaTableID) {
            *areaId = rec->m_areaTableID;
            return 1;
        }
    }

    *areaId = CMap::QueryAreaIdTerrain(position);
    return 1;
}

// VERIFY: 585e9636 manual
// OFFSET: 0x0077F440
double World::GetFramerate() {
    double total = 0.0;
    int32_t index = profIdx;

    for (int32_t i = 30; i; i--) {
        total += profTimes[index++];

        if (index == 30) {
            index = 0;
        }
    }

    double average = total * 0.033333335;

    if (average >= 0.001) {
        return 1.0 / average;
    }

    return 1000.0;
}

// VERIFY: fa93aad1 auto
// OFFSET: 0x0077F2C0
void World::ObjectSetUpdateCallback(CMapEntity* entity, int32_t (*func)(void*, uint32_t, uint64_t, uintptr_t), void* param) {
    entity->m_func = func;
    entity->m_funcParam = param;
}

// VERIFY: e0c34576 manual
// OFFSET: 0x00782000
CMapEntity* CMap::ObjectCreateStatic(CM2Model* model, const C44Matrix& matrix, MAP_OBJECT_FUNC func, void* funcParam, uintptr_t param32) {
    CMapEntity* entity = CMap::AllocEntity(0);
    entity->m_type |= 0x200;
    entity->m_staticFlags = (entity->m_staticFlags & 0xFFFFA390) | 0x400;
    entity->m_flags = MAPOBJ_FLAG_UNPLACED;
    entity->m_func = nullptr;
    entity->unused_00B4 = 0;
    C3Vector ambient = CMap::s_mapLight->m_light.m_ambColor;
    CImVector color;
    color.b = static_cast<uint8_t>(static_cast<int32_t>(ambient.z > 0.0f ? (ambient.z < 1.0f ? ambient.z * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.g = static_cast<uint8_t>(static_cast<int32_t>(ambient.y > 0.0f ? (ambient.y < 1.0f ? ambient.y * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.r = static_cast<uint8_t>(static_cast<int32_t>(ambient.x > 0.0f ? (ambient.x < 1.0f ? ambient.x * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.a = 0xFF;
    entity->m_targetAmbientColor = color;
    entity->m_m2AmbientColor = color;
    entity->m_funcParam32 = param32;
    entity->m_model = model;
    entity->m_sphere.c.x = matrix.d0;
    entity->m_sphere.c.y = matrix.d1;
    entity->m_diffuseLightScale = 1.0f;
    entity->m_sphere.c.z = matrix.d2;
    entity->m_targetDiffuseLightScale = 1.0f;
    entity->m_sphere.r = 0.0f;
    entity->m_vec2 = entity->m_sphere.c;
    ambient = CMap::s_mapLight->m_light.m_ambColor;
    color.b = static_cast<uint8_t>(static_cast<int32_t>(ambient.z > 0.0f ? (ambient.z < 1.0f ? ambient.z * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.g = static_cast<uint8_t>(static_cast<int32_t>(ambient.y > 0.0f ? (ambient.y < 1.0f ? ambient.y * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.r = static_cast<uint8_t>(static_cast<int32_t>(ambient.x > 0.0f ? (ambient.x < 1.0f ? ambient.x * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.a = 0xFF;
    entity->m_targetAmbientColor = color;
    entity->m_m2AmbientColor = color;
    entity->m_scale = sqrtf(matrix.a2 * matrix.a2 + matrix.a1 * matrix.a1 + matrix.a0 * matrix.a0);
    entity->m_position.x = matrix.d0;
    entity->m_position.y = matrix.d1;
    entity->m_staticFlags |= 0x400;
    entity->m_position.z = matrix.d2;
    CMap::LinkStaticEntity(entity);

    if (entity->m_model) {
        if (!SStrCmpI(s_invisibleStalkerModel, model->m_shared->m_fileNameWithoutPath, STORM_MAX_STR)) {
            entity->m_staticFlags |= 0x4000;
        }

        entity->m_model->m_lightingCallback = CMapStaticEntity::ModelLightingCallback;
        entity->m_model->m_lightingArg = entity;
        entity->m_model->m_refCount++;
    }

    entity->m_func = func;
    entity->m_funcParam = funcParam;
    return entity;
}

// VERIFY: d82f4501 manual
// OFFSET: 0x00781C90
CMapEntity* CMap::ObjectCreateStaticEx(CM2Model* model, const C44Matrix& matrix, MAP_OBJECT_FUNC func, void* funcParam, uint64_t param64, uintptr_t param32, const char* file, int32_t line) {
    CMapEntity* entity = CMap::AllocEntity(0);
    entity->m_type |= 0x300;
    entity->m_staticFlags = (entity->m_staticFlags & 0xFFFFA7D0) | 0x400;
    entity->m_func = nullptr;
    entity->unused_00B4 = 0;
    entity->m_flags = MAPOBJ_FLAG_UNPLACED;
    C3Vector ambient = CMap::s_mapLight->m_light.m_ambColor;
    CImVector color;
    color.b = static_cast<uint8_t>(static_cast<int32_t>(ambient.z > 0.0f ? (ambient.z < 1.0f ? ambient.z * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.g = static_cast<uint8_t>(static_cast<int32_t>(ambient.y > 0.0f ? (ambient.y < 1.0f ? ambient.y * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.r = static_cast<uint8_t>(static_cast<int32_t>(ambient.x > 0.0f ? (ambient.x < 1.0f ? ambient.x * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.a = 0xFF;
    entity->m_targetAmbientColor = color;
    entity->m_m2AmbientColor = color;
    entity->m_flags = MAPOBJ_FLAG_UNPLACED;
    entity->m_funcParam32 = param32;
    entity->m_funcParam64 = param64;
    entity->m_model = model;
    entity->m_sphere.c.x = matrix.d0;
    entity->m_sphere.c.y = matrix.d1;
    entity->m_diffuseLightScale = 1.0f;
    entity->m_sphere.c.z = matrix.d2;
    entity->m_targetDiffuseLightScale = 1.0f;
    entity->m_sphere.r = 0.0f;
    entity->m_vec2 = entity->m_sphere.c;
    ambient = CMap::s_mapLight->m_light.m_ambColor;
    color.b = static_cast<uint8_t>(static_cast<int32_t>(ambient.z > 0.0f ? (ambient.z < 1.0f ? ambient.z * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.g = static_cast<uint8_t>(static_cast<int32_t>(ambient.y > 0.0f ? (ambient.y < 1.0f ? ambient.y * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.r = static_cast<uint8_t>(static_cast<int32_t>(ambient.x > 0.0f ? (ambient.x < 1.0f ? ambient.x * 255.0f + 0.5f : 255.0f) : 0.0f));
    color.a = 0xFF;
    entity->m_targetAmbientColor = color;
    entity->m_m2AmbientColor = color;
    entity->m_scale = sqrtf(matrix.a2 * matrix.a2 + matrix.a1 * matrix.a1 + matrix.a0 * matrix.a0);
    entity->m_position.x = matrix.d0;
    entity->m_position.y = matrix.d1;
    entity->m_position.z = matrix.d2;
    CMap::LinkStaticEntity(entity);

    if (entity->m_model) {
        if (!SStrCmpI(s_invisibleStalkerModel, model->m_shared->m_fileNameWithoutPath, STORM_MAX_STR)) {
            entity->m_staticFlags |= 0x4000;
        }

        entity->m_model->m_lightingCallback = CMapStaticEntity::ModelLightingCallback;
        entity->m_model->m_lightingArg = entity;
        entity->m_model->m_refCount++;
    }

    entity->m_func = func;
    entity->m_funcParam = funcParam;
    return entity;
}

// VERIFY: 146e9bb7 auto
// OFFSET: 0x0077F110
int32_t World::QueryMapObjFileName(CMapEntity* entity, const char** fileName) {
    if (!entity) {
        return 0;
    }

    return entity->QueryMapObjFileName(fileName);
}

// VERIFY: 51235d30 auto
// OFFSET: 0x0077F130
bool World::QueryMapObjMinimap(CMapEntity* entity, CAaBox* box, TSStackArray<MINIMAPQUADINFO>* quads, uint32_t maxQuads, int32_t inside, uint8_t indoorArea) {
    if (!entity) {
        return false;
    }

    return entity->QueryMapObjMinimap(box, quads, maxQuads, inside, indoorArea);
}

// VERIFY: 474c6326 auto
// OFFSET: 0x0077F190
bool World::QueryMapObjMatrix(CMapEntity* entity, C44Matrix* matrix, C44Matrix* invMatrix) {
    if (!entity) {
        return false;
    }

    return entity->QueryMapObjMatrix(matrix, invMatrix);
}

// VERIFY: 1c0aecde manual
// OFFSET: 0x0077F030
void CWorldScene::RenderWeather() {
    World::s_weather->Render();
}

// VERIFY: dc10d6e4 auto
// OFFSET: 0x0077F980
void CWorldScene::RenderBarriers(float elapsedSec) {
    CWorldScene::s_barrier.Render(CWorldScene::s_activeWorldView, elapsedSec);
}

// VERIFY: 0fc0d93c manual
// OFFSET: 0x007911D0
void CWorldScene::CullModelRecursive(CM2Model* model) {
    CAaBox box = model->GetCurrentBoundingBox();
    model->m_fFlags = (model->m_fFlags & ~0x2000u) | ((CWorldScene::frustumStack[CWorldScene::frustumIndex].Cull(box) == WorldCull_outside) << 13);

    for (CM2Model* attached = model->m_attachList; attached; attached = attached->m_attachNext) {
        CWorldScene::CullModelRecursive(attached);
    }
}

// VERIFY: 545e3065 manual
// OFFSET: 0x00795F80
void CWorldScene::RenderHorizon() {
    C44Matrix savedProjection;
    C44Matrix projection;
    CWorldScene::BuildHorizonProjection(&projection);
    savedProjection = g_theGxDevicePtr->m_projection;
    g_theGxDevicePtr->XformSetProjection(projection);
    float minX = g_theGxDevicePtr->m_viewport.m_x.m_l;
    float maxX = g_theGxDevicePtr->m_viewport.m_x.m_h;
    float minY = g_theGxDevicePtr->m_viewport.m_y.m_l;
    float maxY = g_theGxDevicePtr->m_viewport.m_y.m_h;
    float minZ = g_theGxDevicePtr->m_viewport.m_z.m_l;
    float maxZ = g_theGxDevicePtr->m_viewport.m_z.m_h;
    GxXformSetViewport(minX, maxX, minY, maxY, s_horizonMinZ, s_horizonMaxZ);
    g_theGxDevicePtr->RsPush();
    C44Matrix world;
    world.Translate(C3Vector(-CWorldScene::s_activeWorldView.x, -CWorldScene::s_activeWorldView.y, -CWorldScene::s_activeWorldView.z));
    g_theGxDevicePtr->XformPushAndSet(GxXform_World, world);

    CMapAreaLow* nextArea;

    for (CMapAreaLow* area = CWorldScene::sortTable.m_horizonChunkList.Head(); area; area = nextArea) {
        nextArea = CWorldScene::sortTable.m_horizonChunkList.Next(area);
        CMap::RenderLowDetail(area);
        area->m_horizonLink.Unlink();
    }

    uint32_t count = 0;
    CMapObjGroup** groups = static_cast<CMapObjGroup**>(alloca(256 * sizeof(CMapObjGroup*)));
    C44Matrix** matrices = static_cast<C44Matrix**>(alloca(256 * sizeof(C44Matrix*)));
    CMapObjDef* nextDef;

    for (CMapObjDef* def = CWorldScene::sortTable.m_horizonMapObjList.Head(); def; def = nextDef) {
        CMapObj* mapObj = def->m_owner;
        nextDef = CWorldScene::sortTable.m_horizonMapObjList.Next(def);

        for (int32_t i = 0; i < mapObj->m_groupInfoCount; i++) {
            if (mapObj->IsGroupLoaded(i)) {
                CMapObjGroup* group = mapObj->GetGroup(i, 0);

                if (group->m_flags & 0x8) {
                    groups[count] = group;
                    matrices[count] = &def->m_mat;
                    count++;
                }
            } else if (!mapObj->IsGroupLoading(i)) {
                mapObj->ReadGroup(i);
            }
        }

        def->m_horizonLink.Unlink();
    }

    if (count) {
        DayNight::DNInfo* info = DayNight::GetInfo();
        GxRsSet(GxRs_FogStart, 0.0f);
        GxRsSet(GxRs_FogEnd, 1.0f);
        GxRsSet(GxRs_FogColor, *reinterpret_cast<int32_t*>(&info->m_fog.m_color));
        GxRsSet(GxRs_Fog, 1);
        GxRsSet(GxRs_Lighting, 0);
        int32_t a5 = -1;
        CMapObj::RenderMapObjGroupsColor(groups, count, matrices, &world, &a5);
    }

    GxXformPop(GxXform_World);
    g_theGxDevicePtr->RsPop();
    GxXformSetViewport(minX, maxX, minY, maxY, minZ, maxZ);
    g_theGxDevicePtr->XformSetProjection(savedProjection);
}

// VERIFY: 8f0548db manual
// OFFSET: 0x00783A60
void World::ObjectSetRenderEnabled(CMapBaseObj* object, int32_t enable) {
    if (enable) {
        object->m_flags = static_cast<MapObjFlag>(object->m_flags & ~MAPOBJ_FLAG_DISABLED);
    } else {
        object->m_flags = static_cast<MapObjFlag>(object->m_flags | MAPOBJ_FLAG_DISABLED);
    }

    if (!(object->m_type & 0x8)) {
        return;
    }

    CMapObjDef* mapObjDef = static_cast<CMapObjDef*>(object);

    for (CMapBaseObjLink* link = mapObjDef->m_mapObjDefGroupLinkList.Head(); link; link = mapObjDef->m_mapObjDefGroupLinkList.Next(link)) {
        CMapObjDefGroup* group = static_cast<CMapObjDefGroup*>(link->m_owner);
        CMapBaseObjLink* next;

        for (CMapBaseObjLink* entityLink = group->m_entityLinkList.Head(); entityLink; entityLink = next) {
            next = group->m_entityLinkList.Next(entityLink);
            CMapEntity* entity = static_cast<CMapEntity*>(entityLink->m_owner);
            CMap::FreeBaseObjLink(entityLink);
            CMap::UpdateEntity(entity);
        }
    }
}

// VERIFY: f64c99f7 auto
// OFFSET: 0x0078FAE0
void CWorldScene::GetFrustumCorners(C3Vector* corners) {
    memcpy(corners, CWorldScene::s_frustumCorners, sizeof(CWorldScene::s_frustumCorners));
}

// VERIFY: e2777b99 auto
// OFFSET: 0x0077F320
void World::GetFrustumCorners(C3Vector* corners) {
    CWorldScene::GetFrustumCorners(corners);
}

// VERIFY: 66d860e9 manual
// OFFSET: 0x0077F260
int32_t World::QueryGroundType(CMapEntity* entity, int32_t* groundType) {
    if (!entity) {
        return 0;
    }

    if (entity->m_groundType == -1) {
        return 0;
    }

    *groundType = entity->m_groundType;
    return 1;
}

// VERIFY: 09260cf1 manual
// OFFSET: 0x00782F20
void World::UpdateObjectFadeouts() {
    uint32_t now = OsGetAsyncTimeMs();

    for (SWModelFadeout* fadeout = s_fadeoutList.Head(); fadeout; ) {
        int32_t elapsed = now - fadeout->m_startTime;
        CM2Model* model = fadeout->m_entity->m_model;
        SWModelFadeout* next = s_fadeoutList.Next(fadeout);

        if (elapsed <= 2000 && model->CanUpdateNonReadyTextures()) {
            if (fadeout->m_transportGuid) {
                CGObject_C* transport = ClntObjMgrObjectPtr(fadeout->m_transportGuid, TYPE_GAMEOBJECT, __FILE__, __LINE__);

                if (transport) {
                    model->SetWorldTransform(fadeout->m_localTransform * transport->GetMatrix());
                } else {
                    fadeout->m_transportGuid = WOWGUID();
                }
            }

            float alpha = (1.0f - static_cast<float>(elapsed) * 0.00050000002f) * fadeout->m_alpha;

            if (alpha >= 0.0f) {
                model->m_fadeAlpha = alpha <= 1.0f ? alpha * ((3.0f - (alpha + alpha)) * alpha) : 1.0f;
            } else {
                model->m_fadeAlpha = 0.0f;
            }
        } else {
            World::ObjectDestroy(fadeout->m_entity);
            fadeout->m_entity = nullptr;
            fadeout->m_link.Unlink();
            s_fadeoutDataPool.LinkToTail(fadeout);
        }

        fadeout = next;
    }
}

// VERIFY: 6429c545 auto
// OFFSET: 0x0077FE00
void World::ObjectSetMoveable(CMapEntity* entity, int32_t moveable) {
    if (moveable) {
        entity->m_flags = static_cast<MapObjFlag>(entity->m_flags | 0x1000);
    } else {
        entity->m_flags = static_cast<MapObjFlag>(entity->m_flags & ~0x1000);
    }
}

// VERIFY: 380eef33 manual
// OFFSET: 0x007823D0
static int32_t ObjectFadeoutCallback(void* param, uint32_t flags, uint64_t param64, uintptr_t param32) {
    CMapEntity* entity = static_cast<CMapEntity*>(param);

    if (!entity) {
        return 0;
    }

    if (flags & 0x1) {
        entity->m_model->SetAnimating(1);
        CM2Model* model = entity->m_model;

        if (model->m_attachParent) {
            model->m_fFlags |= 0x80;
        } else {
            model->m_fFlags |= 0x8;
        }

        if (model->m_attachParent) {
            model->m_fFlags |= 0x20000;
        } else {
            model->m_fFlags |= 0x10000;
        }

        return 1;
    }

    CM2Model* model = entity->m_model;

    if (model->m_attachParent) {
        model->m_fFlags &= ~0x80;
    } else {
        model->m_fFlags &= ~0x8;
    }

    if (model->m_attachParent) {
        model->m_fFlags &= ~0x20000;
    } else {
        model->m_fFlags &= ~0x10000;
    }

    entity->m_model->SetAnimating(0);
    model = entity->m_model;

    if (model->m_attachParent) {
        model->m_fFlags &= ~0x20000;
    } else {
        model->m_fFlags &= ~0x10000;
    }

    return 1;
}

// VERIFY: b75aa230 manual
// OFFSET: 0x00783630
void CGObject_C::PrepareToFadeInt(CMapEntity* entity, float alpha, uint64_t transportGuid) {
    if (!entity) {
        return;
    }

    if (!entity->m_model || !entity->m_model->IsDrawable(0, 0) || alpha < 0.0099999998f || (entity->m_staticFlags & 0x4)) {
        World::ObjectDestroy(entity);
        return;
    }

    SWModelFadeout* fadeout = s_fadeoutDataPool.Head();

    if (!fadeout) {
        fadeout = s_fadeoutDataPool.NewNode(0, 0, 0);
    }

    fadeout->m_entity = entity;
    fadeout->m_startTime = OsGetAsyncTimeMs();
    fadeout->m_alpha = alpha;

    if (transportGuid) {
        CGObject_C* transport = ClntObjMgrObjectPtr(WOWGUID{ transportGuid }, TYPE_GAMEOBJECT, __FILE__, __LINE__);

        if (transport) {
            fadeout->m_transportGuid.m_guid = transportGuid;
            C44Matrix world = entity->m_model->m_worldTransform;
            fadeout->m_localTransform = world * transport->GetMatrix().AffineInverse();
        } else {
            fadeout->m_transportGuid = WOWGUID();
        }
    } else {
        fadeout->m_transportGuid = WOWGUID();
    }

    s_fadeoutList.LinkToTail(fadeout);
    entity->m_func = ObjectFadeoutCallback;
    entity->m_funcParam = entity;
}

// VERIFY: 9ca78c2b auto
// OFFSET: 0x0077F040
void World::NewFootprint(int32_t textureId, const float* size, const C3Vector* position, float facing, int32_t left, int32_t terrainType, int32_t activePlayer) {
    CMap::NewFootprint(textureId, size, position, facing, left, terrainType, activePlayer);
}

// VERIFY: 04260407 manual
// OFFSET: 0x0077FBF0
int32_t World::QueryOutdoors(CMapEntity* entity) {
    if (!entity) {
        return 1;
    }

    CMapObjDef* mapObjDef;
    CMapObj* mapObj;
    CMapObjDefGroup* mapObjDefGroup;
    CMapObjGroup* mapObjGroup = nullptr;

    if (entity->GetMapObjAndGroup(&mapObjDef, &mapObj, &mapObjDefGroup, &mapObjGroup, 0)) {
        int32_t outdoors = (mapObjGroup->m_flags >> 3) & 1;
        WMOAreaTableRec* rec = GetWMOAreaRec(mapObj->m_header->m_wmoID, mapObjDef->m_nameSet, mapObjGroup->m_wmoGroupId);

        if (!rec) {
            return outdoors;
        }

        if (rec->m_flags & 0x4) {
            return 1;
        }

        return (rec->m_flags & 0x2) ? 0 : outdoors;
    }

    int32_t areaId = CMap::QueryAreaIdTerrain(&entity->m_position);
    AreaTableRec* area = g_areaTableDB.GetRecord(areaId);

    if (!area || (area->m_flags & 0x4000000)) {
        return 1;
    }

    return (area->m_flags & 0x2000000) ? 0 : 1;
}

// VERIFY: 4fb148c8 auto
// OFFSET: 0x00783500
CMapBaseObj* CWorld::ObjectCreate(const char* name, const C3Vector& position, float facing, int32_t waitLoad, int32_t snap, uint64_t guid, uint32_t doodadSetCount, const int16_t* doodadSets, float* radius, uint32_t id) {
    const char* extension = SStrChrR(name, '.');

    if (extension && !SStrCmpI(extension, ".wmo", STORM_MAX_STR)) {
        CMapObjDef* mapObjDef = CMap::CreateMapObjDef(name, &position, facing, waitLoad, doodadSetCount, doodadSets, radius, id);
        mapObjDef->m_objectGuid.m_guid = guid;
        CMapBaseObjLink* link = CMap::AllocBaseObjLink(mapObjDef);
        link->m_ref = nullptr;
        CMap::mapObjDefLinkList.LinkToTail(link);

        if (snap) {
            CMap::SnapBaseObjToSubChunk(mapObjDef, &position, facing);
            CMapObjDef::UpdatePos(mapObjDef, &position, facing, 0.0f, 0.0f);
        }

        mapObjDef->m_refCount++;

        return mapObjDef;
    }

    CMapDoodadDef* doodadDef = CMap::CreateDoodadDef(name, &position, facing, waitLoad);
    doodadDef->m_staticFlags |= 0x2010;
    doodadDef->m_collisionGuid.m_guid = guid;

    if (snap) {
        CMap::SnapBaseObjToSubChunk(doodadDef, &position, facing);
        CMap::UpdateDoodadDef(doodadDef, &position, facing);
    }

    doodadDef->m_refCount++;

    return doodadDef;
}

// VERIFY: 1612d04a auto
// OFFSET: 0x0077FF10
void World::ObjectSetSequenceFinishedCallback(CMapEntity* entity, void (*callback)(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time, uint64_t param), uint64_t param, uint16_t doodad) {
    CMapBaseObj* object = entity;

    if (object->m_type & 0x8) {
        static_cast<CMapObjDef*>(object)->SetSequenceFinishedCallback(callback, param, doodad);
    } else if ((object->m_type & 0x40) && entity->m_model) {
        entity->m_model->SetSequenceCallback(reinterpret_cast<void*>(callback), param);
    }
}

// VERIFY: f7bce300 auto
// OFFSET: 0x0077FD60
void World::ObjectUpdate(CMapBaseObj* object, const C3Vector* position, float facing, float a4, float a5, int32_t snap) {
    if (snap) {
        CMap::SnapBaseObjToSubChunk(object, position, facing);
    }

    if (object->m_type & 0x8) {
        CMapObjDef::UpdatePos(static_cast<CMapObjDef*>(object), position, facing, a4, a5);
    } else if (object->m_type & 0x40) {
        CMap::UpdateDoodadDef(static_cast<CMapDoodadDef*>(object), position, facing);
    }
}

// VERIFY: b0e04ce8 auto
// OFFSET: 0x0077FDD0
void World::ObjectUpdate(CMapEntity* entity, const C44Matrix& matrix) {
    CMapBaseObj* object = entity;

    if (object->m_type & 0x8) {
        CMapObjDef::UpdateMatrix(reinterpret_cast<CMapObjDef*>(object), matrix);
    } else if (object->m_type & 0x40) {
        CMap::UpdateDoodadDefTransform(reinterpret_cast<CMapDoodadDef*>(object), matrix);
    }
}

// VERIFY: 5db11fb5 auto
// OFFSET: 0x0077F8F0
void World::SetFrameCntCallback(void (*callback)()) {
    s_FrameCntCallback = callback;
}

// VERIFY: 0bce3425 manual
// OFFSET: 0x0077FFB0
bool World::ObjectTestConvexVolume(CMapEntity* entity, const C3Vector& point) {
    uint16_t type = entity->m_type;

    if (type & 0x8) {
        return static_cast<CMapObjDef*>(static_cast<CMapBaseObj*>(entity))->m_owner->TestConvexVolume(point);
    }

    if (!(type & 0x40)) {
        return true;
    }

    CM2Model* model = static_cast<CMapStaticEntity*>(static_cast<CMapBaseObj*>(entity))->m_model;

    if (!model || !model->IsLoaded(0, 0)) {
        return false;
    }

    model = static_cast<CMapStaticEntity*>(static_cast<CMapBaseObj*>(entity))->m_model;

    if (!(model->m_shared->m_flags & 0x1)) {
        model->WaitForLoad(nullptr);
    }

    CAaBox bounds = model->m_shared->m_data->m_collisionBounds.m_extent;
    C4Plane planes[6];
    planes[0].n = C3Vector(1.0f, 0.0f, 0.0f);
    planes[0].d = -bounds.t.x;
    planes[1].n = C3Vector(0.0f, 1.0f, 0.0f);
    planes[1].d = -bounds.t.y;
    planes[2].n = C3Vector(0.0f, 0.0f, 1.0f);
    planes[2].d = -bounds.t.z - 1.6404123f - 0.013888889f;
    planes[3].n = C3Vector(-1.0f, 0.0f, 0.0f);
    planes[3].d = bounds.b.x;
    planes[4].n = C3Vector(0.0f, -1.0f, 0.0f);
    planes[4].d = bounds.b.y;
    planes[5].n = C3Vector(0.0f, 0.0f, -1.0f);
    planes[5].d = bounds.b.z;

    for (uint32_t i = 0; i < 6; i++) {
        if (planes[i].n.x * point.x + planes[i].n.z * point.z + planes[i].n.y * point.y + planes[i].d > 0.0f) {
            return false;
        }
    }

    return true;
}
