#ifndef WOW_SOURCE_WORLDCLIENT_ZONELIGHT_H
#define WOW_SOURCE_WORLDCLIENT_ZONELIGHT_H

#include <cfloat>
#include <cstdint>

class C2Vector;

struct ZoneLight
{
    /* 0000 */ int32_t m_mapID = -1;
    /* 0004 */ int32_t m_lightID = -1;
    /* 0008 */ int32_t m_pointCount = 0;
    /* 000C */ C2Vector* m_points = nullptr;
    /* 0010 */ float m_minX = FLT_MAX;
    /* 0014 */ float m_minY = FLT_MAX;
    /* 0018 */ float m_maxX = -FLT_MAX;
    /* 001C */ float m_maxY = -FLT_MAX;

    ZoneLight();
    ~ZoneLight();
};

#endif
