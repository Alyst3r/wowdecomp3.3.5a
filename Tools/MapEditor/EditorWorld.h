#ifndef TOOLS_MAPEDITOR_EDITORWORLD_H
#define TOOLS_MAPEDITOR_EDITORWORLD_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

namespace EditorWorld {
    void LoadMap(const char* directory, int32_t mapId);
    void Update(const C3Vector& camPos, const C3Vector& camTarget);
    void Render();
    uint32_t TimeOfDay();
    void SetTimeOfDay(uint32_t minutes);
    void Stats(uint32_t& areasRequested, uint32_t& areasLoaded, uint32_t& chunksDrawn);
}

#endif
