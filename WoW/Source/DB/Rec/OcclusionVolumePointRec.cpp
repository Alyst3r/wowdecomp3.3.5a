#include "WoW/Source/DB/Rec/OcclusionVolumePointRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

OcclusionVolumePointRec::OcclusionVolumePointRec()
{
}

OcclusionVolumePointRec::~OcclusionVolumePointRec()
{
}

const char* OcclusionVolumePointRec::GetFilename()
{
    return "DBFilesClient\\OcclusionVolumePoint.dbc";
}

bool OcclusionVolumePointRec::Read(SFileImpl* file, const char* stringBuffer)
{
    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t zoneLightIDRead = SFileReadFile(file, &this->m_occlusionID, sizeof(this->m_occlusionID), nullptr, nullptr, 0);
    int32_t positionXRead = SFileReadFile(file, &this->m_positionX, sizeof(this->m_positionX), nullptr, nullptr, 0);
    int32_t positionYRead = SFileReadFile(file, &this->m_positionY, sizeof(this->m_positionY), nullptr, nullptr, 0);
    int32_t positionZRead = SFileReadFile(file, &this->m_positionZ, sizeof(this->m_positionZ), nullptr, nullptr, 0);

    if (!idRead || !zoneLightIDRead || !positionXRead || !positionYRead || !positionZRead)
    {
        return false;
    }

    return true;
}
