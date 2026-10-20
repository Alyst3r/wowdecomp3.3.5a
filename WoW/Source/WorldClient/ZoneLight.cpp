#include "ZoneLight.h"
#include "Engine/Source/Storm/H/storm.h"

ZoneLight::ZoneLight()
{
}

ZoneLight::~ZoneLight()
{
    if (this->m_points) {
        SMemFree(this->m_points, __FILE__, __LINE__, 0);
    }
}
