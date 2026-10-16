#ifndef TOOLS_MAPEDITOR_EDITORAPP_H
#define TOOLS_MAPEDITOR_EDITORAPP_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

namespace EditorApp {
    void SetStartup(const char* map, const C3Vector* position, float yaw);
    int32_t InitializeEngine(const void* data, void* param);
    int32_t DestroyEngine(const void* data, void* param);
    const char* GetMapDirectory();
    int32_t GetMapId();
    void SetStartupLua(const char* code);
    void SetQuitAfter(float seconds);
    void Quit();
    bool LoadMap(int32_t mapId, const C3Vector& position, float yaw, float pitch);
    void UnloadMap();
    bool IsMapLoaded();
}

#endif
