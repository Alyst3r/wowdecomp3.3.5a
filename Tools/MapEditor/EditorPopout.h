#ifndef TOOLS_MAPEDITOR_EDITORPOPOUT_H
#define TOOLS_MAPEDITOR_EDITORPOPOUT_H

#include <cstdint>

namespace EditorPopout {
    void Initialize();
    bool Create(const char* frameName, const char* title);
    void Destroy(const char* frameName);
    void SetVisible(const char* frameName, bool visible);
    void SetTitle(const char* frameName, const char* title);
    bool IsActive(const char* frameName);
    bool IsRenderingPopouts();
    bool IsAppForeground();
    bool IsMainWindowForeground();
    bool TestClick(const char* frameName, int32_t x, int32_t y);
}

#endif
