#include "WoW/Source/DB/Rec/LFGRolesRec.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"

LFGRolesRec::LFGRolesRec()
{
}

LFGRolesRec::~LFGRolesRec()
{
}

const char* LFGRolesRec::GetFilename()
{
    return "DBFilesClient\\LFGRoles.dbc";
}

bool LFGRolesRec::Read(SFileImpl* file, const char* stringBuffer)
{
    int32_t idRead = SFileReadFile(file, &this->m_id, sizeof(this->m_id), nullptr, nullptr, 0);
    int32_t rolesRead = SFileReadFile(file, &this->m_roles, sizeof(this->m_roles), nullptr, nullptr, 0);

    if (!idRead || !rolesRead)
    {
        return false;
    }

    return true;
}
