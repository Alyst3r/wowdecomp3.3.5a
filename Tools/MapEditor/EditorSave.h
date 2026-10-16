#ifndef TOOLS_MAPEDITOR_EDITORSAVE_H
#define TOOLS_MAPEDITOR_EDITORSAVE_H

#include <cstdint>
#include <string>
#include <vector>
#include "WoW/Source/WorldClient/Map.h"

class CMapArea;

namespace EditorSave {
    enum MODE {
        SAVE_CURRENT = 0,
        SAVE_CHANGED = 1,
        SAVE_ALL = 2,
    };

    struct AddedObject {
        int32_t m_kind;
        std::string m_fileName;
        SMDoodadDef m_doodad;
        SMMapObjDef m_mapObj;
    };

    struct ObjectChanges {
        std::vector<uint32_t> m_removedDoodads;
        std::vector<uint32_t> m_removedMapObjs;
        std::vector<AddedObject> m_added;
    };

    bool BuildTile(CMapArea* area, std::vector<uint8_t>& out, const ObjectChanges* changes = nullptr);
    bool ReloadArea(CMapArea* area, const std::vector<uint8_t>& data);
    bool ApplyObjectChanges(CMapArea* area, const ObjectChanges& changes);
    int32_t Save(MODE mode, const std::string& projectPath, std::string& error);
}

#endif
