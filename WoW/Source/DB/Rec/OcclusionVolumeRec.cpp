#include "WoW/Source/DB/Rec/OcclusionVolumeRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

OcclusionVolumeRec::OcclusionVolumeRec()
{
}

OcclusionVolumeRec::~OcclusionVolumeRec()
{
}

const char* OcclusionVolumeRec::GetFilename()
{
    return "DBFilesClient\\OcclusionVolume.dbc";
}

bool OcclusionVolumeRec::Read(SFileImpl* file, const char* stringBuffer)
{
    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t nameRead = SFileReadFile(file, &this->m_name, sizeof(this->m_name), nullptr, nullptr, 0);
    int32_t mapIDRead = SFileReadFile(file, &this->m_mapID, sizeof(this->m_mapID), nullptr, nullptr, 0);
    int32_t lightIDRead = SFileReadFile(file, &this->m_lightID, sizeof(this->m_lightID), nullptr, nullptr, 0);

    if (!idRead || !nameRead || !mapIDRead || !lightIDRead)
    {
        return false;
    }

    return true;
}
