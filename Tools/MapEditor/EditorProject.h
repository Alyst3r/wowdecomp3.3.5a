#ifndef TOOLS_MAPEDITOR_EDITORPROJECT_H
#define TOOLS_MAPEDITOR_EDITORPROJECT_H

#include <cstdint>
#include <string>
#include <vector>

namespace EditorProject {
    struct PinnedMap {
        int32_t m_mapId;
        std::string m_mapName;
    };

    struct Bookmark {
        std::string m_name;
        int32_t m_mapId;
        float m_pitch;
        float m_yaw;
        float m_position[3];
    };

    struct Project {
        std::string m_path;
        std::string m_name;
        std::string m_clientPath;
        std::string m_clientVersion;
        std::vector<PinnedMap> m_pinnedMaps;
        std::vector<Bookmark> m_bookmarks;
    };

    void SetStartupProject(const char* path);
    void SetClientPath(const char* path);
    void ApplyStartupProject();
    const char* GetClientPath();
    void RegisterScriptFunctions();
    Project* GetCurrent();
    bool Save(const Project& project);
}

#endif
