#include "WoW/Source/DB/Rec/ZoneLightPointRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

ZoneLightPointRec::ZoneLightPointRec()
{
}

ZoneLightPointRec::~ZoneLightPointRec()
{
}

const char* ZoneLightPointRec::GetFilename()
{
    return "DBFilesClient\\ZoneLightPoint.dbc";
}

bool ZoneLightPointRec::Read(SFileImpl* file, const char* stringBuffer)
{
    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t zoneLightIDRead = SFileReadFile(file, &this->m_zoneLightID, sizeof(this->m_zoneLightID), nullptr, nullptr, 0);
    int32_t positionXRead = SFileReadFile(file, &this->m_positionX, sizeof(this->m_positionX), nullptr, nullptr, 0);
    int32_t positionYRead = SFileReadFile(file, &this->m_positionY, sizeof(this->m_positionY), nullptr, nullptr, 0);
    int32_t pointOrderRead = SFileReadFile(file, &this->m_pointOrder, sizeof(this->m_pointOrder), nullptr, nullptr, 0);

    if (!idRead || !zoneLightIDRead || !positionXRead || !positionYRead || !pointOrderRead)
    {
        return false;
    }

    return true;
}
