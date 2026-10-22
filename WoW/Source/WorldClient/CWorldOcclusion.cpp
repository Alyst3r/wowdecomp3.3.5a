#include "CWorldOcclusion.h"
#include <malloc.h>
#include <cmath>
#include <cstring>
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CAaSphere.h"
#include "Engine/Source/Tempest/CFrustum.h"
#include "Engine/Source/Tempest/CMath.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/WorldClient/CPortalView.h"
#include "WoW/Source/WorldClient/CWorldMath.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/WorldView.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxBuf.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"

static const float CBSCALE = 64.0f;

// STATIC: 0x00AEEF60
TSExplicitList<CWorldOccluder, offsetof(CWorldOccluder, m_link)> s_occluderFreeList;

// STATIC: 0x00AEEF6C
TSExplicitList<CWorldAntiOccluder, offsetof(CWorldAntiOccluder, m_link)> s_antiOccluderFreeList;

// STATIC: 0x00D2DCD8
TSGrowableArray<C4Plane> CWorldOcclusion::planeArray;

// STATIC: 0x00D2DCE8
TSGrowableArray<CClipVolume> CWorldOcclusion::clipVolumeArray;

// STATIC: 0x00ADF3F4
static int32_t aabbCornerSelectX[8] = { 0, 1, 1, 0, 0, 1, 1, 0 };

// STATIC: 0x00ADF414
static int32_t aabbCornerSelectY[8] = { 0, 0, 1, 1, 0, 0, 1, 1 };

// STATIC: 0x00ADF434
static int32_t aabbCornerSelectZ[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };

// STATIC: 0x00CD8FD8
static C3Vector s_clipBufferVerts[9];

// STATIC: 0x00D2DCD4
static int32_t s_occlusionVolumesInit;

// STATIC: 0x00AEEF78
static C3Vector s_worldOccluderVerts0[22] = { C3Vector(2287.72998f, 5169.47998f, 44.1699982f), C3Vector(2297.80005f, 5165.72021f, 44.1699982f), C3Vector(2296.86011f, 5166.18994f, 33.6300011f), C3Vector(2328.37988f, 5159.77002f, 33.6300011f), C3Vector(2327.02002f, 5159.72998f, 45.0f), C3Vector(2337.20996f, 5163.87988f, 45.0f), C3Vector(2335.61011f, 5162.58984f, 30.8999996f), C3Vector(2364.8999f, 5187.66992f, 30.8999996f), C3Vector(2364.08008f, 5186.95996f, 39.1500015f), C3Vector(2372.45996f, 5196.06006f, 39.1500015f), C3Vector(2372.95996f, 5192.8999f, 27.8099995f), C3Vector(2381.71997f, 5230.60986f, 27.8099995f), C3Vector(2381.62012f, 5229.2002f, 40.9000015f), C3Vector(2381.37988f, 5239.99023f, 40.9000015f), C3Vector(2370.56006f, 5272.83984f, 47.0f), C3Vector(2365.47998f, 5281.81006f, 47.0f), C3Vector(2366.57007f, 5281.31006f, 33.0999985f), C3Vector(2342.16992f, 5301.83984f, 33.0999985f), C3Vector(2341.38989f, 5302.18994f, 50.6300011f), C3Vector(2325.29004f, 5313.29004f, 50.6300011f), C3Vector(2325.29004f, 5313.29004f, 50.6300011f), C3Vector(2314.61011f, 5303.97021f, 50.6300011f) };

// STATIC: 0x00AEF080
static C3Vector s_worldOccluderVerts1[16] = { C3Vector(2189.1001f, 5348.62988f, 44.7400017f), C3Vector(2200.34009f, 5343.43018f, 44.7400017f), C3Vector(2122.22998f, 5368.75977f, 32.3600006f), C3Vector(2076.05005f, 5340.02002f, 32.3600006f), C3Vector(2076.05005f, 5340.02002f, 32.3600006f), C3Vector(2064.67993f, 5300.58984f, 32.3600006f), C3Vector(2065.1001f, 5302.50977f, 64.3199997f), C3Vector(2063.46997f, 5290.35986f, 64.3199997f), C3Vector(2062.59009f, 5291.6001f, 51.3300018f), C3Vector(2077.09009f, 5252.00977f, 51.3300018f), C3Vector(2077.09009f, 5252.00977f, 51.3300018f), C3Vector(2109.76001f, 5225.79004f, 51.3300018f), C3Vector(2108.5f, 5228.31006f, 39.7599983f), C3Vector(2190.55005f, 5195.1001f, 39.7599983f), C3Vector(2189.1001f, 5348.62988f, 44.7400017f), C3Vector(2200.34009f, 5343.43018f, 44.7400017f) };

// STATIC: 0x00AEF140
static C3Vector s_worldOccluderVerts2[6] = { C3Vector(701.409973f, -5045.22021f, 25.0f), C3Vector(702.150024f, -5032.52002f, 25.0f), C3Vector(702.320007f, -5034.81006f, 15.8100004f), C3Vector(704.030029f, -4994.79004f, 15.8100004f), C3Vector(703.460022f, -4995.75f, 19.4300003f), C3Vector(701.070007f, -4983.97998f, 19.4300003f) };

// STATIC: 0x00AEF188
static C3Vector s_worldOccluderVerts3[6] = { C3Vector(693.52002f, -4946.7002f, 20.2999992f), C3Vector(690.320007f, -4934.54004f, 19.3600006f), C3Vector(691.159973f, -4936.68994f, 15.3999996f), C3Vector(675.849976f, -4907.31006f, 15.3999996f), C3Vector(676.570007f, -4907.72021f, 28.3799992f), C3Vector(670.090027f, -4896.62012f, 28.3799992f) };

// STATIC: 0x00AEF1D0
static C3Vector s_worldOccluderVerts4[22] = { C3Vector(3922.27002f, -3780.83008f, 223.880005f), C3Vector(3931.12988f, -3816.6499f, 225.300003f), C3Vector(3930.42993f, -3816.25f, 247.050003f), C3Vector(3972.51001f, -3865.33008f, 246.880005f), C3Vector(3985.02002f, -3862.97998f, 266.380005f), C3Vector(4019.30005f, -3871.8501f, 266.380005f), C3Vector(4018.84009f, -3870.28003f, 283.140015f), C3Vector(4057.41992f, -3866.94995f, 283.600006f), C3Vector(4057.41992f, -3866.94995f, 283.600006f), C3Vector(4090.54004f, -3839.40991f, 283.140015f), C3Vector(4097.93018f, -3813.61011f, 312.859985f), C3Vector(4108.39014f, -3773.71997f, 310.559998f), C3Vector(4108.39014f, -3773.71997f, 310.559998f), C3Vector(4085.13989f, -3743.48999f, 312.859985f), C3Vector(4070.07007f, -3716.09009f, 279.630005f), C3Vector(4032.63989f, -3689.23999f, 283.549988f), C3Vector(4032.63989f, -3689.23999f, 283.549988f), C3Vector(4005.66992f, -3693.69995f, 279.630005f), C3Vector(4003.37988f, -3697.04004f, 266.73999f), C3Vector(3963.34009f, -3709.25f, 240.449997f), C3Vector(3963.34009f, -3709.25f, 240.449997f), C3Vector(3936.93994f, -3745.72998f, 230.220001f) };

// STATIC: 0x00D2DC20
static WorldOccluderData s_worldOccluders[5] = {
    WorldOccluderData(571, -100.0f, s_worldOccluderVerts0, 22),
    WorldOccluderData(571, -100.0f, s_worldOccluderVerts1, 16),
    WorldOccluderData(571, -50.0f, s_worldOccluderVerts2, 6),
    WorldOccluderData(571, -50.0f, s_worldOccluderVerts3, 6),
    WorldOccluderData(571, -100.0f, s_worldOccluderVerts4, 22)
};

static TSGrowableArray<OcclusionVolume> s_occlusionVolumes;

// VERIFY: f52cc1f6 manual
// OFFSET: 0x005EEB70
void CWorldOccluder::Initialize() {
}

// VERIFY: 4db2afc6 manual
// OFFSET: 0x0078F6A0
void CWorldOcclusion::UpdateBufferChunk(const C3Vector* verts, const float* heights, const uint32_t* indices, int32_t count, const C3Vector* offset, int32_t holes) {
    if (!(CWorld::enables & Enable_Culling) || CWorldScene::camVec.z < -0.89999998f || CWorldScene::camVec.z > 0.89999998f) {
        return;
    }

    C3Vector* projected = s_clipBufferVerts;

    for (int32_t i = 0; i < count; i++) {
        projected->x = verts[*indices].x + offset->x;
        projected->y = verts[*indices].y + offset->y;
        projected->z = heights[*indices] + offset->z;
        *projected = *projected * CWorldScene::s_clipBufferMatrix;

        float inv = 1.0f / projected->z;
        projected++;
        indices++;
        projected[-1].x = projected[-1].x * inv;
        projected[-1].y = inv * projected[-1].y;
    }

    if (holes) {
        for (int32_t i = 0; i < count - 1; i++) {
            int32_t x0 = CMath::fint_(s_clipBufferVerts[i].x * CBSCALE - 0.5f) + 192;
            int32_t x1 = CMath::fint_(s_clipBufferVerts[i + 1].x * CBSCALE - 0.5f) + 192;

            if (x1 < x0) {
                int32_t swap = x0;
                x0 = x1;
                x1 = swap;
            }

            if (x0 < 0) {
                x0 = 0;
            }

            if (x1 >= 384) {
                x1 = 383;
            }

            for (; x0 <= x1; x0++) {
                if (!(CWorldScene::s_clipBufferMask[x0] & 1)) {
                    CWorldScene::s_clipBuffer[x0] = -1000001.0f;
                }
            }
        }

        return;
    }

    for (int32_t i = 0; i < count - 1; i++) {
        const C3Vector& p0 = s_clipBufferVerts[i];
        const C3Vector& p1 = s_clipBufferVerts[i + 1];

        if (p0.z < 0.027777778f || p1.z < 0.027777778f) {
            continue;
        }

        float height = p1.y <= p0.y ? p1.y : p0.y;
        int32_t x0 = CMath::fint_(p0.x * CBSCALE - 0.5f) + 192;
        int32_t x1 = CMath::fint_(p1.x * CBSCALE - 0.5f) + 192;

        if (x1 < x0) {
            int32_t swap = x0;
            x0 = x1;
            x1 = swap;
        }

        if (x0 < 0) {
            x0 = 0;
        }

        if (x1 >= 384) {
            x1 = 383;
        }

        for (; x0 <= x1; x0++) {
            if (height > CWorldScene::s_clipBuffer[x0]) {
                CWorldScene::s_clipBuffer[x0] = height;
            }
        }
    }
}

// VERIFY: 5b9f55f1 manual
// OFFSET: 0x0078F900
void CWorldOcclusion::UpdateBuffer(const C3Vector* points, int32_t count, uint32_t flags) {
    if (!(CWorld::enables & Enable_Culling) || CWorldScene::camVec.z < -0.89999998f || CWorldScene::camVec.z > 0.89999998f) {
        return;
    }

    C3Vector* projected = s_clipBufferVerts;

    for (int32_t i = 0; i < count; i++) {
        *projected = *points * CWorldScene::s_clipBufferMatrix;

        float inv = 1.0f / projected->z;

        if (inv > 0.0f) {
            projected->x = projected->x * inv;
            projected->y = inv * projected->y;
        }

        points++;
        projected++;
    }

    for (int32_t i = 0; i < count - 1; i++) {
        const C3Vector& p0 = s_clipBufferVerts[i];
        const C3Vector& p1 = s_clipBufferVerts[i + 1];

        if (p0.z < 0.027777778f || p1.z < 0.027777778f) {
            continue;
        }

        float height = p1.y <= p0.y ? p1.y : p0.y;
        int32_t x0 = CMath::fint_(p0.x * CBSCALE - 0.5f) + 192;
        int32_t x1 = CMath::fint_(p1.x * CBSCALE - 0.5f) + 192;

        if (x1 < x0) {
            int32_t swap = x0;
            x0 = x1;
            x1 = swap;
        }

        if (x0 < 0) {
            x0 = 0;
        }

        if (x1 >= 384) {
            x1 = 383;
        }

        if (flags & 1) {
            if (x0 <= x1) {
                memset(&CWorldScene::s_clipBufferMask[x0], 1, x1 - x0 + 1);

                for (int32_t x = x0; x <= x1; x++) {
                    if (height > CWorldScene::s_clipBuffer[x]) {
                        CWorldScene::s_clipBuffer[x] = height;
                    }
                }
            }
        } else {
            for (int32_t x = x0; x <= x1; x++) {
                if (height > CWorldScene::s_clipBuffer[x]) {
                    CWorldScene::s_clipBuffer[x] = height;
                }
            }
        }
    }
}

// VERIFY: a0f63ed4 manual
// OFFSET: 0x0078FC40
uint32_t CWorldOcclusion::QueryBuffer(const C3Vector& center, float radius, uint8_t flags) {
    if (!(CWorld::enables & Enable_Culling) || fabs(radius) < 2.3841858e-07f || CWorldScene::camVec.z < -0.89999998f || CWorldScene::camVec.z > 0.89999998f) {
        return 0;
    }

    C3Vector view = center * CWorldScene::s_clipBufferMatrix;
    C3Vector extent = C3Vector(radius, radius, 0.0f) * CWorldScene::projMatrix;

    if (!(flags & 0x8) && view.z < 50.0f) {
        return 0;
    }

    float invZ = 1.0f / view.z;
    view.x *= invZ;
    extent.x *= invZ;
    extent.y *= invZ;
    view.y = extent.x + invZ * view.y;

    int32_t first = CMath::fint_((view.x - extent.y) * CBSCALE - 0.5f) + 192;
    int32_t last = CMath::fint_((extent.y + view.x) * CBSCALE - 0.5f) + 193;

    if (first >= 384 || last < 0) {
        return 0;
    }

    if (first < 0) {
        first = 0;
    }

    if (last >= 384) {
        last = 383;
    }

    for (int32_t i = first; i <= last; i++) {
        if (view.y > CWorldScene::s_clipBuffer[i]) {
            return 0;
        }
    }

    return 2;
}

// VERIFY: 3ac5bc19 auto
// OFFSET: 0x0078FDC0
uint32_t CWorldOcclusion::QueryBuffer(CAaBox* box, uint8_t flags) {
    if (!(CWorld::enables & Enable_Culling) || CWorldScene::camVec.z < -0.89999998f || CWorldScene::camVec.z > 0.89999998f) {
        return 0;
    }

    float minX = 3.4028235e38f;
    float maxX = -3.4028235e38f;
    float maxY = -3.4028235e38f;
    const C3Vector* corners[2] = { &box->b, &box->t };

    for (uint32_t i = 0; i < 8; i++) {
        C3Vector corner(corners[aabbCornerSelectX[i]]->x, corners[aabbCornerSelectY[i]]->y, corners[aabbCornerSelectZ[i]]->z);
        corner = corner * CWorldScene::s_clipBufferMatrix;

        if (!(flags & 0x8) && corner.z < 50.0f) {
            return 0;
        }

        float scale = 1.0f / corner.z;
        float x = corner.x * scale;
        float y = scale * corner.y;

        if (minX > x) {
            minX = x;
        }

        if (maxX < x) {
            maxX = x;
        }

        if (maxY < y) {
            maxY = y;
        }
    }

    int32_t first = CMath::fint_(minX * CBSCALE - 0.5f) + 192;
    int32_t last = CMath::fint_(maxX * CBSCALE - 0.5f) + 193;

    if (first >= 384 || last < 0) {
        return 0;
    }

    if (first < 0) {
        first = 0;
    }

    if (last >= 384) {
        last = 383;
    }

    for (int32_t i = first; i <= last; i++) {
        if (maxY > CWorldScene::s_clipBuffer[i]) {
            return 0;
        }
    }

    return 2;
}

// VERIFY: c855a200 auto
// OFFSET: 0x007CC810
void CWorldScene::AddWorldOccluders() {
    for (uint32_t i = 0; i < 5; i++) {
        WorldOccluderData& data = s_worldOccluders[i];

        if (data.m_mapId != s_mapId || CWorldScene::FrustumCull(&data.m_bounds)) {
            continue;
        }

        for (uint32_t j = 0; j < data.m_count; j += 2) {
            CWorldView::AddOccluder(data.m_verts[j], data.m_verts[j + 1]);
        }
    }
}

// VERIFY: be6bb79d auto
// OFFSET: 0x007CC880
void CWorldOccluder::ClipBufferUpdate() {
    CWorldOcclusion::UpdateBuffer(this->m_points, 2, 1);
}

// VERIFY: 7e61d360 manual
// OFFSET: 0x007CC890
WorldOccluderData::WorldOccluderData(int32_t mapId, float depth, C3Vector* verts, uint32_t count) {
    this->m_mapId = mapId;
    this->m_bounds.b = C3Vector(3.4028235e38f, 3.4028235e38f, 3.4028235e38f);
    this->m_bounds.t = C3Vector(-3.4028235e38f, -3.4028235e38f, -3.4028235e38f);
    this->m_verts = verts;
    this->m_count = count;

    for (uint32_t i = 0; i < this->m_count; i++) {
        const C3Vector& vert = verts[i];

        if (this->m_bounds.b.x > vert.x) {
            this->m_bounds.b.x = vert.x;
        }

        if (this->m_bounds.b.y > vert.y) {
            this->m_bounds.b.y = vert.y;
        }

        if (this->m_bounds.t.x < vert.x) {
            this->m_bounds.t.x = vert.x;
        }

        if (this->m_bounds.t.y < vert.y) {
            this->m_bounds.t.y = vert.y;
        }

        if (this->m_bounds.t.z < vert.z) {
            this->m_bounds.t.z = vert.z;
        }

        float bottom = vert.z + depth;

        if (bottom < this->m_bounds.b.z) {
            this->m_bounds.b.z = bottom;
        }
    }
}

// VERIFY: 8b33fdfb manual
// OFFSET: 0x007CC9A0
CWorldOccluder* CWorldOccluder::Alloc() {
    CWorldOccluder* occluder = s_occluderFreeList.Head();

    if (!occluder) {
        occluder = s_occluderFreeList.NewNode(2, 0, 0);
    }

    occluder->m_link.Unlink();

    return occluder;
}

// VERIFY: 21308e7d manual
// OFFSET: 0x007CCA90
void CWorldOccluder::Recycle() {
    this->m_link.Unlink();
    s_occluderFreeList.LinkToTail(this);
}

// OFFSET: 0x007CCD20
static void InitOcclusionVolumes()
{
    if (s_occlusionVolumesInit || !g_occlusionVolumeDB.m_loaded || !g_occlusionVolumeDB.m_numRecords || !g_occlusionVolumePointDB.m_loaded || !g_occlusionVolumePointDB.m_numRecords)
    {
        return;
    }

    s_occlusionVolumes.SetCount(g_occlusionVolumeDB.m_numRecords);

    for (uint32_t i = 0; i < g_occlusionVolumeDB.m_numRecords; i++)
    {
        OcclusionVolume& volume = s_occlusionVolumes.m_data[i];
        OcclusionVolumeRec& recA = g_occlusionVolumeDB.m_records[i];

        for (uint32_t j = 0; j < g_occlusionVolumePointDB.m_numRecords; j++)
        {
            if (g_occlusionVolumePointDB.m_records[j].m_occlusionID == recA.m_id)
            {
                volume.m_data.m_positionCount++;
            }
        }

        if (!volume.m_data.m_positionCount)
        {
            continue;
        }

        volume.m_mapID = recA.m_mapID;
        volume.m_flags = recA.m_flags;
        volume.m_data.m_positions = static_cast<C3Vector*>(SMemAlloc(sizeof(C3Vector) * volume.m_data.m_positionCount, __FILE__, __LINE__, 0));

        uint32_t currentCount = 1;

        for (uint32_t k = 0; k < g_occlusionVolumePointDB.m_numRecords; k++)
        {
            OcclusionVolumePointRec& recB = g_occlusionVolumePointDB.m_records[k];

            if (recA.m_id == recB.m_occlusionID)
            {
                float tempX = recB.m_positionX;
                float tempY = recB.m_positionY;
                float tempZ = recB.m_positionZ;

                volume.m_data.m_positions[currentCount - 1].x = tempX;
                volume.m_data.m_positions[currentCount - 1].y = tempY;
                volume.m_data.m_positions[currentCount - 1].z = tempZ;

                if (volume.m_data.m_bounds.b.x > tempX)
                {
                    volume.m_data.m_bounds.b.x = tempX;
                }

                if (volume.m_data.m_bounds.b.y > tempY)
                {
                    volume.m_data.m_bounds.b.y = tempY;
                }

                if (volume.m_data.m_bounds.b.z > tempZ)
                {
                    volume.m_data.m_bounds.b.z = tempZ;
                }

                if (volume.m_data.m_bounds.t.x < tempX)
                {
                    volume.m_data.m_bounds.t.x = tempX;
                }

                if (volume.m_data.m_bounds.t.y < tempY)
                {
                    volume.m_data.m_bounds.t.y = tempY;
                }

                if (volume.m_data.m_bounds.t.z < tempZ)
                {
                    volume.m_data.m_bounds.t.z = tempZ;
                }

                currentCount++;
            }

            if (currentCount >= volume.m_data.m_positionCount)
            {
                break;
            }
        }
    }

    s_occlusionVolumesInit = 1;
    CWorldOcclusion::planeArray.m_chunk = 32;
    CWorldOcclusion::clipVolumeArray.m_chunk = 32;
}

// VERIFY: 58f32a53 auto
// OFFSET: 0x007CCDF0
uint32_t CWorldOcclusion::GetClipVolumeCount() {
    return CWorldOcclusion::clipVolumeArray.Count();
}

// VERIFY: 96f106cd manual
// OFFSET: 0x007CCE00
uint32_t CWorldOcclusion::QueryVolumes(CAaSphere* sphere) {
    for (uint32_t i = 0; i < CWorldOcclusion::clipVolumeArray.Count(); i++) {
        CClipVolume& volume = CWorldOcclusion::clipVolumeArray[i];
        C4Plane* planes = &CWorldOcclusion::planeArray[volume.m_planeStart];
        uint32_t j;

        for (j = 0; j < volume.m_planeCount; j++) {
            if (planes[j].n.z * sphere->c.z + planes[j].n.x * sphere->c.x + sphere->c.y * planes[j].n.y + planes[j].d + sphere->r > 0.0f) {
                break;
            }
        }

        if (j == volume.m_planeCount) {
            return 1;
        }
    }

    return 0;
}

// VERIFY: 9dd72105 manual
// OFFSET: 0x007CCFA0
uint32_t CWorldOcclusion::QueryVolumes(C3Vector* points, uint32_t count) {
    for (uint32_t i = 0; i < CWorldOcclusion::clipVolumeArray.Count(); i++) {
        CClipVolume& volume = CWorldOcclusion::clipVolumeArray[i];
        C4Plane* planes = &CWorldOcclusion::planeArray[volume.m_planeStart];
        uint32_t j;

        for (j = 0; j < volume.m_planeCount; j++) {
            uint32_t k;

            for (k = 0; k < count; k++) {
                if (points[k].z * planes[j].n.z + points[k].x * planes[j].n.x + planes[j].n.y * points[k].y + planes[j].d > 0.0f) {
                    break;
                }
            }

            if (k < count) {
                break;
            }
        }

        if (j == volume.m_planeCount) {
            return 1;
        }
    }

    return 0;
}

// VERIFY: 931f6673 manual
// OFFSET: 0x007CD4E0
void CWorldOcclusion::AddVolume(const C3Vector* eye, const C3Vector* verts, int32_t count, int32_t clipToView) {
    uint32_t planeStart = CWorldOcclusion::planeArray.Count();
    uint32_t planeCount = 0;

    if (clipToView && CWorldScene::frustumPortalView.m_maxViewDepth > 0.000001f) {
        float depth = CWorldScene::frustumPortalView.m_maxViewDepth;
        C4Plane viewPlane;
        viewPlane.n = CWorldScene::camVec;
        viewPlane.d = -((depth * CWorldScene::camVec.z + eye->z) * CWorldScene::camVec.z + CWorldScene::camVec.y * (CWorldScene::camVec.y * depth + eye->y) + CWorldScene::camVec.x * (CWorldScene::camVec.x * depth + eye->x));

        C3Vector* clipped = static_cast<C3Vector*>(alloca(sizeof(C3Vector) * 2 * count));
        uint32_t clippedCount = 0;
        CWorldMath::ClipVertexListToPlane(verts, count, clipped, clippedCount, viewPlane, 0.000099999997f, 1);

        if (clippedCount <= 2) {
            return;
        }

        for (uint32_t i = 0; i < clippedCount; i++) {
            const C3Vector& a = clipped[i];
            const C3Vector& b = clipped[(i + 1) % clippedCount];
            C3Vector toEye(eye->x - a.x, eye->y - a.y, eye->z - a.z);
            C3Vector edge(b.x - a.x, b.y - a.y, b.z - a.z);

            C4Plane plane;
            plane.n.x = edge.y * toEye.z - edge.z * toEye.y;
            plane.n.y = edge.z * toEye.x - toEye.z * edge.x;
            plane.n.z = edge.x * toEye.y - toEye.x * edge.y;
            plane.d = 0.0f;

            float lengthSq = plane.n.y * plane.n.y + plane.n.z * plane.n.z + plane.n.x * plane.n.x;

            if (lengthSq > 0.000099999997f) {
                float inv = 1.0f / sqrtf(lengthSq);
                plane.n.x = plane.n.x * inv;
                plane.n.y = plane.n.y * inv;
                plane.n.z = inv * plane.n.z;
                plane.d = -(plane.n.y * a.y + plane.n.z * a.z + plane.n.x * a.x);
                CWorldOcclusion::planeArray.Add(1, &plane);
                planeCount++;
            }
        }
    } else {
        if (count <= 0) {
            return;
        }

        for (int32_t i = 0; i < count; i++) {
            C4Plane* plane = CWorldOcclusion::planeArray.New();
            plane->From3Pos(verts[i], verts[(i + 1) % count], *eye);
            planeCount++;
        }
    }

    if (!planeCount) {
        return;
    }

    C4Plane* cap = CWorldOcclusion::planeArray.New();
    cap->From3Pos(verts[0], verts[1], verts[2]);
    planeCount++;

    if (cap->n.z * eye->z + cap->n.y * eye->y + eye->x * cap->n.x + cap->d < 0.0f) {
        for (uint32_t i = 0; i < planeCount; i++) {
            C4Plane& plane = CWorldOcclusion::planeArray.m_data[planeStart + i];
            plane.d = -plane.d;
            plane.n.x = -plane.n.x;
            plane.n.y = -plane.n.y;
            plane.n.z = -plane.n.z;
        }
    }

    CClipVolume volume;
    volume.m_planeStart = planeStart;
    volume.m_planeCount = planeCount;
    CWorldOcclusion::clipVolumeArray.Add(1, &volume);
}

// OFFSET: 0x007CD850
void CWorldScene::CreateOcclusionVolumes(const C3Vector* eye, const C3Vector* corners, int32_t onlyFlagged)
{
    CWorldOcclusion::planeArray.SetCount(0);
    CWorldOcclusion::clipVolumeArray.SetCount(0);

    if (!s_occlusionVolumesInit)
    {
        InitOcclusionVolumes();
    }

    CFrustum frustum;
    frustum.CalcPlanesFromCorners(corners);

    for (uint32_t i = 0; i < s_occlusionVolumes.Count(); i++)
    {
        OcclusionVolume& volume = s_occlusionVolumes[i];

        if (s_mapId != volume.m_mapID || (onlyFlagged && !(volume.m_flags & 2)))
        {
            continue;
        }

        if (frustum.Cull(volume.m_data.m_bounds))
        {
            CWorldOcclusion::AddVolume(eye, volume.m_data.m_positions, volume.m_data.m_positionCount, !(volume.m_flags & 1));
        }
    }
}

// VERIFY: 49cb931b auto
// OFFSET: 0x007CD910
void CWorldOcclusion::ClearVolumes() {
    CWorldOcclusion::planeArray.SetCount(0);
    CWorldOcclusion::clipVolumeArray.SetCount(0);
}

// VERIFY: 1ad39c5b manual
// OFFSET: 0x007CD930
void CWorldScene::RenderOcclusionVolumes() {
    if (!(CWorld::enables & Enable_PortalVis)) {
        return;
    }

    int32_t vertexCount = 0;
    int32_t indexCount = 0;

    for (uint32_t i = 0; i < 62; i++) {
        if (s_occlusionVolumes[i].m_mapID == s_mapId) {
            int32_t count = s_occlusionVolumes[i].m_data.m_positionCount;
            vertexCount += count;
            indexCount += 3 * (count - 2);
        }
    }

    if (!vertexCount || !indexCount) {
        return;
    }

    CImVector color;
    color.b = 0x20;
    color.g = 0x80;
    color.r = 0xFF;
    color.a = 0x8F;

    C44Matrix world;
    world.Translate(C3Vector(-CWorldScene::s_activeWorldView.x, -CWorldScene::s_activeWorldView.y, -CWorldScene::s_activeWorldView.z));
    g_theGxDevicePtr->XformPushAndSet(GxXform_World, world);

    g_theGxDevicePtr->RsPush();
    GxRsSet(GxRs_BlendingMode, 2);
    GxUpdateAlphaRef(GxRs_AlphaRef);
    GxRsSet(GxRs_DepthWrite, 0);
    GxRsSet(GxRs_Lighting, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_PixelShader, 0);
    GxRsSet(GxRs_VertexShader, 0);
    GxRsSet(GxRs_MatDiffuse, *reinterpret_cast<int32_t*>(&color));
    GxRsSet(GxRs_Culling, 0);

    CGxBuf* vertexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Vertex, sizeof(CGxVertexPN), vertexCount);
    CGxVertexPN* vertex = reinterpret_cast<CGxVertexPN*>(g_theGxDevicePtr->BufLock(vertexBuf));

    for (uint32_t i = 0; i < 62; i++) {
        OcclusionVolumeData& data = s_occlusionVolumes[i].m_data;

        if (s_occlusionVolumes[i].m_mapID != s_mapId) {
            continue;
        }

        const C3Vector* positions = data.m_positions;
        C3Vector edgeA(positions[1].x - positions[0].x, positions[1].y - positions[0].y, positions[1].z - positions[0].z);
        C3Vector edgeB(positions[2].x - positions[0].x, positions[2].y - positions[0].y, positions[2].z - positions[0].z);
        C3Vector normal = C3Vector::Cross(edgeA, edgeB);

        for (int32_t j = 0; j < static_cast<int32_t>(data.m_positionCount); j++) {
            vertex->m_position = data.m_positions[j];
            vertex->m_normal = normal;
            vertex++;
        }
    }

    g_theGxDevicePtr->BufUnlock(vertexBuf, 0);
    vertexBuf->m_hasData = 1;
    GxPrimVertexPtr(vertexBuf, GxVBF_PN);

    CGxBuf* indexBuf = g_theGxDevicePtr->BufStream(GxPoolTarget_Index, sizeof(uint16_t), indexCount);
    uint16_t* indices = reinterpret_cast<uint16_t*>(g_theGxDevicePtr->BufLock(indexBuf));
    int32_t base = 0;

    for (uint32_t i = 0; i < 62; i++) {
        OcclusionVolume& volume = s_occlusionVolumes[i];

        if (volume.m_mapID != s_mapId) {
            continue;
        }

        for (int32_t j = 0; j < static_cast<int32_t>(volume.m_data.m_positionCount) - 2; j++) {
            indices[0] = static_cast<uint16_t>(base);
            indices[1] = static_cast<uint16_t>(base + j + 1);
            indices[2] = static_cast<uint16_t>(base + j + 2);
            indices += 3;
        }

        base += volume.m_data.m_positionCount;
    }

    g_theGxDevicePtr->BufUnlock(indexBuf, 0);
    indexBuf->m_hasData = 1;
    g_theGxDevicePtr->PrimIndexPtr(indexBuf);

    CGxBatch batch;
    batch.m_primType = GxPrim_Triangles;
    batch.m_start = 0;
    batch.m_count = indexCount;
    batch.m_minIndex = 0;
    batch.m_maxIndex = static_cast<uint16_t>(vertexCount);
    g_theGxDevicePtr->Draw(&batch, 1);

    g_theGxDevicePtr->RsPop();
    GxXformPop(GxXform_World);
}

OcclusionVolumeData::OcclusionVolumeData()
{
}

OcclusionVolumeData::~OcclusionVolumeData()
{
    if (this->m_positions)
    {
        SMemFree(this->m_positions, __FILE__, __LINE__, 0);
    }
}
