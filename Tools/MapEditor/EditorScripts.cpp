#include "EditorScripts.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRect.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/WorldClient/World.h"
#include "Engine/Source/Os/Input.h"
#include "EditorApp.h"
#include "EditorPopout.h"
#include "EditorViewport.h"
#include "EditorWorld.h"
#include "ChunkEdit.h"
#include "ObjectEdit.h"
#include "TerrainEdit.h"
#include "TextureEdit.h"

struct EditorViewFlag {
    const char* m_name;
    uint32_t m_flag;
    bool m_inverted;
};

static const EditorViewFlag s_viewFlags[] = {
    { "doodads", Enable_Doodads, false },
    { "wmo", Enable_MapObjs, false },
    { "terrain", Enable_Chunks, false },
    { "water", Enable_Water, false },
    { "wireframe", Enable_ShowTris, false },
    { "animations", Enable_NoAnimation, true },
    { "sky", Enable_Sky, false },
    { "detaildoodads", Enable_DetailDoodads, false },
    { "modelboxes", Enable_AABoxes, false },
};

static bool s_fogEnabled = true;

static float Clamp(float value, float minValue, float maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

static const EditorViewFlag* FindViewFlag(const char* name) {
    for (uint32_t i = 0; i < sizeof(s_viewFlags) / sizeof(s_viewFlags[0]); i++) {
        if (!SStrCmpI(name, s_viewFlags[i].m_name, STORM_MAX_STR)) {
            return &s_viewFlags[i];
        }
    }

    return nullptr;
}

static int32_t Script_EditorPrint(lua_State* L) {
    int32_t count = lua_gettop(L);

    for (int32_t i = 1; i <= count; i++) {
        const char* text = lua_tolstring(L, i, nullptr);
        printf("%s%s", i > 1 ? " " : "", text ? text : (lua_isnil(L, i) ? "nil" : lua_typename(L, lua_type(L, i))));
    }

    printf("\n");
    fflush(stdout);
    return 0;
}

static int32_t Script_EditorGetWindowSize(lua_State* L) {
    CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_theGxDevicePtr->CapsWindowSize(window);
    lua_pushnumber(L, window.maxX - window.minX);
    lua_pushnumber(L, window.maxY - window.minY);
    return 2;
}

static int32_t Script_EditorGetEditingMode(lua_State* L) {
    lua_pushnumber(L, TerrainEdit::s_mode + 1);
    return 1;
}

static int32_t Script_EditorSetEditingMode(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: EditorSetEditingMode(mode)");
        return 0;
    }

    int32_t mode = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;

    if (mode >= 0 && mode < TerrainEdit::NUM_MODES) {
        TerrainEdit::s_mode = static_cast<TerrainEdit::MODE>(mode);
        TerrainEdit::EndStroke();
        TextureEdit::EndStroke();
        ObjectEdit::EndEdit();
    }

    return 0;
}

static int32_t Script_EditorGetRaiseLower(lua_State* L) {
    const TerrainEdit::RaiseLowerSettings& settings = TerrainEdit::s_raiseLower;
    lua_pushnumber(L, settings.m_type + 1);
    lua_pushnumber(L, settings.m_radius);
    lua_pushnumber(L, settings.m_innerRadius);
    lua_pushnumber(L, settings.m_speed);
    return 4;
}

static int32_t Script_EditorSetRaiseLower(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetRaiseLower(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));
    TerrainEdit::RaiseLowerSettings& settings = TerrainEdit::s_raiseLower;

    if (!SStrCmpI(key, "type", STORM_MAX_STR)) {
        int32_t type = static_cast<int32_t>(value) - 1;

        if (type >= 0 && type < TerrainEdit::NUM_TERRAIN_TYPES) {
            settings.m_type = type;
        }
    } else if (!SStrCmpI(key, "radius", STORM_MAX_STR)) {
        settings.m_radius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "innerRadius", STORM_MAX_STR)) {
        settings.m_innerRadius = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "speed", STORM_MAX_STR)) {
        settings.m_speed = Clamp(value, 0.0f, 1000.0f);
    }

    return 0;
}

static int32_t Script_EditorGetFlattenBlur(lua_State* L) {
    const TerrainEdit::FlattenBlurSettings& settings = TerrainEdit::s_flattenBlur;
    lua_pushnumber(L, settings.m_type + 1);
    lua_pushnumber(L, settings.m_radius);
    lua_pushnumber(L, settings.m_speed);
    lua_pushboolean(L, settings.m_raise);
    lua_pushboolean(L, settings.m_lower);
    lua_pushboolean(L, settings.m_angled);
    lua_pushnumber(L, settings.m_angle);
    lua_pushnumber(L, settings.m_orientation);
    lua_pushboolean(L, settings.m_lock);
    lua_pushnumber(L, settings.m_lockPos.x);
    lua_pushnumber(L, settings.m_lockPos.y);
    lua_pushnumber(L, settings.m_lockPos.z);
    return 12;
}

static int32_t Script_EditorSetFlattenBlur(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorSetFlattenBlur(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));
    bool enabled = lua_toboolean(L, 2) != 0;
    TerrainEdit::FlattenBlurSettings& settings = TerrainEdit::s_flattenBlur;

    if (!SStrCmpI(key, "type", STORM_MAX_STR)) {
        int32_t type = static_cast<int32_t>(value) - 1;

        if (type >= 0 && type < TerrainEdit::NUM_FLATTEN_TYPES) {
            settings.m_type = type;
        }
    } else if (!SStrCmpI(key, "radius", STORM_MAX_STR)) {
        settings.m_radius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "speed", STORM_MAX_STR)) {
        settings.m_speed = Clamp(value, 0.0f, 10.0f);
    } else if (!SStrCmpI(key, "raise", STORM_MAX_STR)) {
        settings.m_raise = enabled;
    } else if (!SStrCmpI(key, "lower", STORM_MAX_STR)) {
        settings.m_lower = enabled;
    } else if (!SStrCmpI(key, "angled", STORM_MAX_STR)) {
        settings.m_angled = enabled;
    } else if (!SStrCmpI(key, "angle", STORM_MAX_STR)) {
        settings.m_angle = Clamp(value, 0.0f, 89.0f);
    } else if (!SStrCmpI(key, "orientation", STORM_MAX_STR)) {
        settings.m_orientation = fmodf(value, 360.0f);
    } else if (!SStrCmpI(key, "lock", STORM_MAX_STR)) {
        settings.m_lock = enabled;
    } else if (!SStrCmpI(key, "lockX", STORM_MAX_STR)) {
        settings.m_lockPos.x = value;
    } else if (!SStrCmpI(key, "lockY", STORM_MAX_STR)) {
        settings.m_lockPos.y = value;
    } else if (!SStrCmpI(key, "lockH", STORM_MAX_STR)) {
        settings.m_lockPos.z = value;
    }

    return 0;
}

static int32_t Script_EditorGetViewFlag(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorGetViewFlag(\"flag\")");
        return 0;
    }

    const char* name = lua_tolstring(L, 1, nullptr);

    if (!SStrCmpI(name, "fog", STORM_MAX_STR)) {
        lua_pushboolean(L, s_fogEnabled);
        return 1;
    }

    if (!SStrCmpI(name, "lines", STORM_MAX_STR)) {
        lua_pushboolean(L, ChunkEdit::s_view.m_lines);
        return 1;
    }

    if (!SStrCmpI(name, "holelines", STORM_MAX_STR)) {
        lua_pushboolean(L, ChunkEdit::s_view.m_holeLines);
        return 1;
    }

    if (!SStrCmpI(name, "adtborders", STORM_MAX_STR)) {
        lua_pushboolean(L, ChunkEdit::s_view.m_adtBorders);
        return 1;
    }

    const EditorViewFlag* flag = FindViewFlag(name);

    if (!flag) {
        return 0;
    }

    bool set = (CWorld::enables & flag->m_flag) != 0;
    lua_pushboolean(L, flag->m_inverted ? !set : set);
    return 1;
}

static int32_t Script_EditorSetViewFlag(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorSetViewFlag(\"flag\", enabled)");
        return 0;
    }

    const char* name = lua_tolstring(L, 1, nullptr);
    bool enabled = lua_toboolean(L, 2) != 0;

    if (!SStrCmpI(name, "fog", STORM_MAX_STR)) {
        s_fogEnabled = enabled;
        g_theGxDevicePtr->MasterEnableSet(GxMasterEnable_Fog, enabled ? 1 : 0);
        return 0;
    }

    if (!SStrCmpI(name, "lines", STORM_MAX_STR)) {
        ChunkEdit::s_view.m_lines = enabled;
        return 0;
    }

    if (!SStrCmpI(name, "holelines", STORM_MAX_STR)) {
        ChunkEdit::s_view.m_holeLines = enabled;
        return 0;
    }

    if (!SStrCmpI(name, "adtborders", STORM_MAX_STR)) {
        ChunkEdit::s_view.m_adtBorders = enabled;
        return 0;
    }

    const EditorViewFlag* flag = FindViewFlag(name);

    if (!flag) {
        return 0;
    }

    if (enabled != flag->m_inverted) {
        CWorld::enables |= flag->m_flag;
    } else {
        CWorld::enables &= ~flag->m_flag;
    }

    return 0;
}

static int32_t Script_EditorGetCursor(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (!viewport || !viewport->m_cursorValid) {
        return 0;
    }

    lua_pushnumber(L, viewport->m_cursor.x);
    lua_pushnumber(L, viewport->m_cursor.y);
    lua_pushnumber(L, viewport->m_cursor.z);
    return 3;
}

static int32_t Script_EditorGetCamera(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (!viewport) {
        return 0;
    }

    lua_pushnumber(L, viewport->m_position.x);
    lua_pushnumber(L, viewport->m_position.y);
    lua_pushnumber(L, viewport->m_position.z);
    lua_pushnumber(L, viewport->m_yaw);
    lua_pushnumber(L, viewport->m_pitch);
    lua_pushnumber(L, viewport->m_speed);
    return 6;
}

static int32_t Script_EditorSetCamera(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        luaL_error(L, "Usage: EditorSetCamera(x, y, z [, yaw, pitch])");
        return 0;
    }

    if (!viewport) {
        return 0;
    }

    viewport->m_position = C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));

    if (lua_isnumber(L, 4)) {
        viewport->m_yaw = static_cast<float>(lua_tonumber(L, 4));
    }

    if (lua_isnumber(L, 5)) {
        viewport->m_pitch = Clamp(static_cast<float>(lua_tonumber(L, 5)), -1.55f, 1.55f);
    }

    return 0;
}

static int32_t Script_EditorSetCameraSpeed(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: EditorSetCameraSpeed(speed)");
        return 0;
    }

    if (EditorViewport::s_current) {
        EditorViewport::s_current->m_speed = Clamp(static_cast<float>(lua_tonumber(L, 1)), 1.0f, 2000.0f);
    }

    return 0;
}

static int32_t Script_EditorGetMapInfo(lua_State* L) {
    lua_pushstring(L, EditorApp::GetMapDirectory());
    lua_pushnumber(L, EditorApp::GetMapId());
    return 2;
}

static int32_t Script_EditorGetAreaName(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetAreaName(x, y)");
        return 0;
    }

    AreaTableRec* area = g_areaTableDB.GetRecord(TerrainEdit::GetAreaId(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2))));

    if (!area) {
        lua_pushstring(L, "");
        return 1;
    }

    AreaTableRec* parent = area->m_parentAreaID ? g_areaTableDB.GetRecord(area->m_parentAreaID) : nullptr;

    if (parent && parent->m_areaName) {
        char name[512];
        SStrPrintf(name, sizeof(name), "%s - %s", parent->m_areaName, area->m_areaName ? area->m_areaName : "");
        lua_pushstring(L, name);
    } else {
        lua_pushstring(L, area->m_areaName ? area->m_areaName : "");
    }

    return 1;
}

static int32_t Script_EditorGetStats(lua_State* L) {
    uint32_t areasRequested;
    uint32_t areasLoaded;
    uint32_t chunksDrawn;
    EditorWorld::Stats(areasRequested, areasLoaded, chunksDrawn);

    uint32_t doodads = 0;

    for (CMapDoodadDef* def = CMap::doodadDefHash.Head(); def; def = CMap::doodadDefHash.Next(def)) {
        doodads++;
    }

    lua_pushnumber(L, EditorViewport::s_current ? EditorViewport::s_current->m_fps : 0.0f);
    lua_pushnumber(L, chunksDrawn);
    lua_pushnumber(L, areasLoaded);
    lua_pushnumber(L, EditorUndo::GetModifiedChunkCount());
    lua_pushnumber(L, EditorUndo::GetUndoCount());
    lua_pushnumber(L, EditorUndo::GetRedoCount());
    lua_pushnumber(L, doodads);
    lua_pushnumber(L, CWorldScene::nDoodadsRendered);
    lua_pushnumber(L, EditorUndo::GetModifiedObjectCount());
    return 9;
}

static int32_t Script_EditorUndo(lua_State* L) {
    lua_pushboolean(L, EditorUndo::Undo());
    return 1;
}

static int32_t Script_EditorRedo(lua_State* L) {
    lua_pushboolean(L, EditorUndo::Redo());
    return 1;
}

static int32_t Script_EditorGetTimeOfDay(lua_State* L) {
    lua_pushnumber(L, EditorWorld::TimeOfDay());
    return 1;
}

static int32_t Script_EditorSetTimeOfDay(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        luaL_error(L, "Usage: EditorSetTimeOfDay(minutes)");
        return 0;
    }

    EditorWorld::SetTimeOfDay(static_cast<uint32_t>(Clamp(static_cast<float>(lua_tonumber(L, 1)), 0.0f, 1439.0f)));
    return 0;
}

static int32_t Script_EditorGetHeight(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetHeight(x, y)");
        return 0;
    }

    float z;

    if (!TerrainEdit::GetHeight(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), &z)) {
        return 0;
    }

    lua_pushnumber(L, z);
    return 1;
}

static int32_t Script_EditorApplyBrush(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
        luaL_error(L, "Usage: EditorApplyBrush(\"raise|lower|flatten|blur\", x, y, seconds)");
        return 0;
    }

    static const char* s_operations[] = { "raise", "lower", "flatten", "blur" };
    const char* name = lua_tolstring(L, 1, nullptr);
    int32_t op = -1;

    for (int32_t i = 0; i < 4; i++) {
        if (!SStrCmpI(name, s_operations[i], STORM_MAX_STR)) {
            op = i;
        }
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)), 0.0f);

    if (op < 0 || TerrainEdit::IsStroking() || !TerrainEdit::GetHeight(pos.x, pos.y, &pos.z)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    float seconds = static_cast<float>(lua_tonumber(L, 4));

    for (float done = 0.0f; done < seconds; done += 1.0f / 60.0f) {
        TerrainEdit::GetHeight(pos.x, pos.y, &pos.z);
        TerrainEdit::Apply(static_cast<TerrainEdit::OPERATION>(op), pos, 1.0f / 60.0f);
    }

    TerrainEdit::EndStroke();
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t Script_EditorIsMouseButtonDown(lua_State* L) {
    lua_pushboolean(L, OsInputIsMouseButtonDown(MOUSE_BUTTON_LEFT) || OsInputIsMouseButtonDown(MOUSE_BUTTON_RIGHT) || OsInputIsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    return 1;
}

static int32_t Script_EditorPopoutFrame(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorPopoutFrame(\"frame\" [, \"title\"])");
        return 0;
    }

    const char* name = lua_tolstring(L, 1, nullptr);
    const char* title = lua_isstring(L, 2) ? lua_tolstring(L, 2, nullptr) : name;
    lua_pushboolean(L, EditorPopout::Create(name, title));
    return 1;
}

static int32_t Script_EditorDockFrame(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorDockFrame(\"frame\")");
        return 0;
    }

    EditorPopout::Destroy(lua_tolstring(L, 1, nullptr));
    return 0;
}

static int32_t Script_EditorSetPopoutVisible(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: EditorSetPopoutVisible(\"frame\", visible)");
        return 0;
    }

    EditorPopout::SetVisible(lua_tolstring(L, 1, nullptr), lua_toboolean(L, 2) != 0);
    return 0;
}

static int32_t Script_EditorSetPopoutTitle(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
        luaL_error(L, "Usage: EditorSetPopoutTitle(\"frame\", \"title\")");
        return 0;
    }

    EditorPopout::SetTitle(lua_tolstring(L, 1, nullptr), lua_tolstring(L, 2, nullptr));
    return 0;
}

static int32_t Script_EditorPopoutTestClick(lua_State* L) {
    lua_pushboolean(L, EditorPopout::TestClick(lua_tolstring(L, 1, nullptr), static_cast<int32_t>(lua_tonumber(L, 2)), static_cast<int32_t>(lua_tonumber(L, 3))));
    return 1;
}

static int32_t Script_EditorGetTextureBrush(lua_State* L) {
    lua_pushnumber(L, TextureEdit::s_brush.m_radius);
    lua_pushnumber(L, TextureEdit::s_brush.m_hardness);
    lua_pushnumber(L, TextureEdit::s_brush.m_pressure);
    lua_pushnumber(L, TextureEdit::s_brush.m_opacity);
    return 4;
}

static int32_t Script_EditorSetTextureBrush(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetTextureBrush(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));

    if (!SStrCmpI(key, "radius", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_radius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "hardness", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_hardness = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "pressure", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_pressure = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "opacity", STORM_MAX_STR)) {
        TextureEdit::s_brush.m_opacity = Clamp(value, 0.0f, 255.0f);
    }

    return 0;
}

static int32_t Script_EditorGetTexture(lua_State* L) {
    const char* texture = TextureEdit::GetTexture();

    if (!texture || !*texture) {
        return 0;
    }

    lua_pushstring(L, texture);
    return 1;
}

static int32_t Script_EditorSetTexture(lua_State* L) {
    TextureEdit::SetTexture(lua_isstring(L, 1) ? lua_tolstring(L, 1, nullptr) : "");
    return 0;
}

static int32_t Script_EditorGetTilesetCount(lua_State* L) {
    lua_pushnumber(L, TextureEdit::GetTilesetCount());
    return 1;
}

static int32_t Script_EditorGetTileset(lua_State* L) {
    const char* name = TextureEdit::GetTileset(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1);

    if (!name) {
        return 0;
    }

    lua_pushstring(L, name);
    return 1;
}

static int32_t Script_EditorGetCursorTextures(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (!viewport || !viewport->m_cursorValid) {
        return 0;
    }

    const char* names[4];
    uint32_t count = TextureEdit::GetChunkTextures(viewport->m_cursor, names, 4);

    for (uint32_t i = 0; i < count; i++) {
        lua_pushstring(L, names[i]);
    }

    return static_cast<int32_t>(count);
}

static int32_t Script_EditorGetSelectionCount(lua_State* L) {
    lua_pushnumber(L, ObjectEdit::GetSelectionCount());
    return 1;
}

static int32_t Script_EditorGetSelection(lua_State* L) {
    int32_t kind;
    uint32_t uniqueId;
    const char* fileName;

    if (!ObjectEdit::GetSelectionInfo(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1, &kind, &uniqueId, &fileName)) {
        return 0;
    }

    lua_pushstring(L, kind == ObjectEdit::KIND_DOODAD ? "m2" : "wmo");
    lua_pushnumber(L, uniqueId);
    lua_pushstring(L, fileName);
    return 3;
}

static int32_t Script_EditorClearSelection(lua_State* L) {
    ObjectEdit::ClearSelection();
    return 0;
}

static int32_t Script_EditorGetObjectTransform(lua_State* L) {
    C3Vector position;
    C3Vector rotation;
    float scale;

    if (!ObjectEdit::GetTransform(&position, &rotation, &scale)) {
        return 0;
    }

    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    lua_pushnumber(L, rotation.x);
    lua_pushnumber(L, rotation.y);
    lua_pushnumber(L, rotation.z);
    lua_pushnumber(L, scale);
    return 7;
}

static int32_t Script_EditorSetObjectTransform(lua_State* L) {
    C3Vector position;
    C3Vector rotation;
    float scale;

    if (!ObjectEdit::GetTransform(&position, &rotation, &scale)) {
        return 0;
    }

    if (lua_isnumber(L, 1)) {
        position.x = static_cast<float>(lua_tonumber(L, 1));
    }

    if (lua_isnumber(L, 2)) {
        position.y = static_cast<float>(lua_tonumber(L, 2));
    }

    if (lua_isnumber(L, 3)) {
        position.z = static_cast<float>(lua_tonumber(L, 3));
    }

    if (lua_isnumber(L, 4)) {
        rotation.x = static_cast<float>(lua_tonumber(L, 4));
    }

    if (lua_isnumber(L, 5)) {
        rotation.y = static_cast<float>(lua_tonumber(L, 5));
    }

    if (lua_isnumber(L, 6)) {
        rotation.z = static_cast<float>(lua_tonumber(L, 6));
    }

    if (lua_isnumber(L, 7)) {
        scale = static_cast<float>(lua_tonumber(L, 7));
    }

    if (ObjectEdit::GetSelectionCount() == 1) {
        ObjectEdit::SetTransform(&position, &rotation, &scale);
    } else {
        ObjectEdit::SetTransform(lua_isnumber(L, 1) || lua_isnumber(L, 2) || lua_isnumber(L, 3) ? &position : nullptr, nullptr, nullptr);
    }

    return 0;
}

static int32_t Script_EditorObjectSnapToGround(lua_State* L) {
    ObjectEdit::SnapToGround();
    return 0;
}

static int32_t Script_EditorGetObjectSetting(lua_State* L) {
    const char* key = lua_tolstring(L, 1, nullptr);

    if (!key) {
        return 0;
    }

    if (!SStrCmpI(key, "followCursor", STORM_MAX_STR)) {
        lua_pushboolean(L, ObjectEdit::s_settings.m_followCursor);
        return 1;
    }

    if (!SStrCmpI(key, "snapMultiToGround", STORM_MAX_STR)) {
        lua_pushboolean(L, ObjectEdit::s_settings.m_snapMultiToGround);
        return 1;
    }

    return 0;
}

static int32_t Script_EditorSetObjectSetting(lua_State* L) {
    const char* key = lua_tolstring(L, 1, nullptr);
    bool value = lua_toboolean(L, 2) != 0;

    if (!key) {
        return 0;
    }

    if (!SStrCmpI(key, "followCursor", STORM_MAX_STR)) {
        ObjectEdit::s_settings.m_followCursor = value;
    } else if (!SStrCmpI(key, "snapMultiToGround", STORM_MAX_STR)) {
        ObjectEdit::s_settings.m_snapMultiToGround = value;
    }

    return 0;
}

static int32_t Script_EditorPickObject(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6)) {
        luaL_error(L, "Usage: EditorPickObject(x, y, z, dirX, dirY, dirZ [, add])");
        return 0;
    }

    C3Vector start(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));
    C3Vector dir(static_cast<float>(lua_tonumber(L, 4)), static_cast<float>(lua_tonumber(L, 5)), static_cast<float>(lua_tonumber(L, 6)));
    dir.Normalize();
    int32_t kind;
    uint32_t uniqueId;
    float distance;

    if (!ObjectEdit::Pick(start, dir, 2000.0f, &kind, &uniqueId, &distance)) {
        return 0;
    }

    ObjectEdit::Select(kind, uniqueId, lua_toboolean(L, 7) != 0);
    lua_pushstring(L, kind == ObjectEdit::KIND_DOODAD ? "m2" : "wmo");
    lua_pushnumber(L, uniqueId);
    lua_pushnumber(L, distance);
    return 3;
}

static int32_t Script_EditorMoveSelection(lua_State* L) {
    ObjectEdit::MoveBy(C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3))));
    return 0;
}

static int32_t Script_EditorPaintTexture(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)) {
        luaL_error(L, "Usage: EditorPaintTexture(x, y, seconds)");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f);

    if (!TerrainEdit::GetHeight(pos.x, pos.y, &pos.z)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    float seconds = static_cast<float>(lua_tonumber(L, 3));

    for (float done = 0.0f; done < seconds; done += 1.0f / 60.0f) {
        TextureEdit::Paint(pos, 1.0f / 60.0f);
    }

    TextureEdit::EndStroke();
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t Script_EditorGetTexturesAt(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetTexturesAt(x, y)");
        return 0;
    }

    const char* names[4];
    uint32_t count = TextureEdit::GetChunkTextures(C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f), names, 4);

    for (uint32_t i = 0; i < count; i++) {
        lua_pushstring(L, names[i]);
    }

    return static_cast<int32_t>(count);
}

static int32_t Script_EditorGetChunkTool(lua_State* L) {
    const ChunkEdit::Settings& settings = ChunkEdit::s_settings;
    lua_pushnumber(L, settings.m_holeRadius);
    lua_pushnumber(L, settings.m_areaRadius);
    lua_pushnumber(L, settings.m_areaId);
    lua_pushnumber(L, settings.m_vertexRadius);
    lua_pushnumber(L, settings.m_vertexSpeed);
    lua_pushnumber(L, settings.m_color[0]);
    lua_pushnumber(L, settings.m_color[1]);
    lua_pushnumber(L, settings.m_color[2]);
    return 8;
}

static int32_t Script_EditorSetChunkTool(lua_State* L) {
    if (!lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetChunkTool(\"key\", value)");
        return 0;
    }

    const char* key = lua_tolstring(L, 1, nullptr);
    float value = static_cast<float>(lua_tonumber(L, 2));
    ChunkEdit::Settings& settings = ChunkEdit::s_settings;

    if (!SStrCmpI(key, "holeRadius", STORM_MAX_STR)) {
        settings.m_holeRadius = Clamp(value, 0.0f, 250.0f);
    } else if (!SStrCmpI(key, "areaRadius", STORM_MAX_STR)) {
        settings.m_areaRadius = Clamp(value, 0.0f, 1000.0f);
    } else if (!SStrCmpI(key, "areaId", STORM_MAX_STR)) {
        settings.m_areaId = static_cast<int32_t>(value);
    } else if (!SStrCmpI(key, "vertexRadius", STORM_MAX_STR)) {
        settings.m_vertexRadius = Clamp(value, 0.0f, 10000.0f);
    } else if (!SStrCmpI(key, "vertexSpeed", STORM_MAX_STR)) {
        settings.m_vertexSpeed = Clamp(value, 0.0f, 10.0f);
    } else if (!SStrCmpI(key, "colorR", STORM_MAX_STR)) {
        settings.m_color[0] = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "colorG", STORM_MAX_STR)) {
        settings.m_color[1] = Clamp(value, 0.0f, 1.0f);
    } else if (!SStrCmpI(key, "colorB", STORM_MAX_STR)) {
        settings.m_color[2] = Clamp(value, 0.0f, 1.0f);
    }

    return 0;
}

static int32_t Script_EditorGetAreas(lua_State* L) {
    int32_t mapId = EditorApp::GetMapId();
    lua_newtable(L);
    int32_t count = 0;

    for (int32_t i = 0; i < g_areaTableDB.m_numRecords; i++) {
        AreaTableRec* area = &g_areaTableDB.m_records[i];

        if (area->m_continentID != mapId) {
            continue;
        }

        lua_newtable(L);
        lua_pushnumber(L, area->m_id);
        lua_setfield(L, -2, "id");
        lua_pushnumber(L, area->m_parentAreaID);
        lua_setfield(L, -2, "parent");
        lua_pushstring(L, area->m_areaName ? area->m_areaName : "");
        lua_setfield(L, -2, "name");
        lua_rawseti(L, -2, ++count);
    }

    return 1;
}

static int32_t Script_EditorSetTileHoles(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (viewport && EditorApp::IsMapLoaded()) {
        ChunkEdit::SetTileHoles(viewport->m_position, lua_toboolean(L, 1) != 0);
    }

    return 0;
}

static int32_t Script_EditorSetTileArea(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (viewport && EditorApp::IsMapLoaded()) {
        ChunkEdit::SetTileAreaId(viewport->m_position);
    }

    return 0;
}

static int32_t Script_EditorPaintVertex(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
        luaL_error(L, "Usage: EditorPaintVertex(x, y, z, seconds [, remove])");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));
    ChunkEdit::PaintVertexColor(pos, static_cast<float>(lua_tonumber(L, 4)), !lua_toboolean(L, 5));
    ChunkEdit::EndStroke();
    return 0;
}

static int32_t Script_EditorSetHoles(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetHoles(x, y, add [, wholeChunk])");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f);
    ChunkEdit::SetHoles(pos, lua_toboolean(L, 4) != 0, lua_toboolean(L, 3) != 0);
    ChunkEdit::EndStroke();
    return 0;
}

static int32_t Script_EditorSetImpass(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorSetImpass(x, y, add)");
        return 0;
    }

    C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f);
    ChunkEdit::SetImpass(pos, lua_toboolean(L, 3) != 0);
    ChunkEdit::EndStroke();
    return 0;
}

static int32_t Script_EditorGetChunkInfo(lua_State* L) {
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        luaL_error(L, "Usage: EditorGetChunkInfo(x, y)");
        return 0;
    }

    uint32_t flags;
    uint32_t holes;
    uint32_t areaId;
    uint32_t color;

    if (!ChunkEdit::GetInfo(C3Vector(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), 0.0f), &flags, &holes, &areaId, &color)) {
        return 0;
    }

    lua_pushnumber(L, flags);
    lua_pushnumber(L, holes);
    lua_pushnumber(L, areaId);
    lua_pushnumber(L, color);
    return 4;
}

static int32_t Script_EditorDeleteSelection(lua_State* L) {
    ObjectEdit::DeleteSelection();
    return 0;
}

static int32_t Script_EditorCopySelection(lua_State* L) {
    ObjectEdit::CopySelection();
    return 0;
}

static int32_t Script_EditorPasteObjects(lua_State* L) {
    EditorViewport* viewport = EditorViewport::s_current;

    if (lua_isboolean(L, 1)) {
        if (lua_toboolean(L, 1) && viewport && viewport->m_cursorValid) {
            ObjectEdit::Paste(&viewport->m_cursor);
        }
    } else if (lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3)) {
        C3Vector pos(static_cast<float>(lua_tonumber(L, 1)), static_cast<float>(lua_tonumber(L, 2)), static_cast<float>(lua_tonumber(L, 3)));
        ObjectEdit::Paste(&pos);
    } else {
        ObjectEdit::Paste(nullptr);
    }

    return 0;
}

static int32_t Script_EditorDuplicateSelection(lua_State* L) {
    ObjectEdit::Duplicate();
    return 0;
}

static int32_t Script_EditorHasClipboard(lua_State* L) {
    lua_pushboolean(L, ObjectEdit::HasClipboard());
    return 1;
}

static int32_t Script_EditorGetModelCount(lua_State* L) {
    lua_pushnumber(L, ObjectEdit::GetModelCount());
    return 1;
}

static int32_t Script_EditorGetModel(lua_State* L) {
    const char* name = lua_isnumber(L, 1) ? ObjectEdit::GetModel(static_cast<uint32_t>(lua_tonumber(L, 1)) - 1) : nullptr;

    if (!name) {
        return 0;
    }

    lua_pushstring(L, name);
    return 1;
}

static int32_t Script_EditorSetClipboardModel(lua_State* L) {
    lua_pushboolean(L, ObjectEdit::SetClipboardModel(lua_isstring(L, 1) ? lua_tolstring(L, 1, nullptr) : nullptr));
    return 1;
}

static int32_t Script_EditorQuit(lua_State* L) {
    EditorApp::Quit();
    return 0;
}

static FrameScript_Method s_editorFunctions[] = {
    { "EditorPrint", &Script_EditorPrint },
    { "EditorGetWindowSize", &Script_EditorGetWindowSize },
    { "EditorGetEditingMode", &Script_EditorGetEditingMode },
    { "EditorSetEditingMode", &Script_EditorSetEditingMode },
    { "EditorGetRaiseLower", &Script_EditorGetRaiseLower },
    { "EditorSetRaiseLower", &Script_EditorSetRaiseLower },
    { "EditorGetFlattenBlur", &Script_EditorGetFlattenBlur },
    { "EditorSetFlattenBlur", &Script_EditorSetFlattenBlur },
    { "EditorGetViewFlag", &Script_EditorGetViewFlag },
    { "EditorSetViewFlag", &Script_EditorSetViewFlag },
    { "EditorGetCursor", &Script_EditorGetCursor },
    { "EditorGetCamera", &Script_EditorGetCamera },
    { "EditorSetCamera", &Script_EditorSetCamera },
    { "EditorSetCameraSpeed", &Script_EditorSetCameraSpeed },
    { "EditorGetMapInfo", &Script_EditorGetMapInfo },
    { "EditorGetAreaName", &Script_EditorGetAreaName },
    { "EditorGetStats", &Script_EditorGetStats },
    { "EditorUndo", &Script_EditorUndo },
    { "EditorRedo", &Script_EditorRedo },
    { "EditorGetTimeOfDay", &Script_EditorGetTimeOfDay },
    { "EditorSetTimeOfDay", &Script_EditorSetTimeOfDay },
    { "EditorGetHeight", &Script_EditorGetHeight },
    { "EditorApplyBrush", &Script_EditorApplyBrush },
    { "EditorIsMouseButtonDown", &Script_EditorIsMouseButtonDown },
    { "EditorPopoutFrame", &Script_EditorPopoutFrame },
    { "EditorDockFrame", &Script_EditorDockFrame },
    { "EditorSetPopoutVisible", &Script_EditorSetPopoutVisible },
    { "EditorSetPopoutTitle", &Script_EditorSetPopoutTitle },
    { "EditorPopoutTestClick", &Script_EditorPopoutTestClick },
    { "EditorGetTextureBrush", &Script_EditorGetTextureBrush },
    { "EditorSetTextureBrush", &Script_EditorSetTextureBrush },
    { "EditorGetTexture", &Script_EditorGetTexture },
    { "EditorSetTexture", &Script_EditorSetTexture },
    { "EditorGetTilesetCount", &Script_EditorGetTilesetCount },
    { "EditorGetTileset", &Script_EditorGetTileset },
    { "EditorGetCursorTextures", &Script_EditorGetCursorTextures },
    { "EditorGetTexturesAt", &Script_EditorGetTexturesAt },
    { "EditorPaintTexture", &Script_EditorPaintTexture },
    { "EditorGetSelectionCount", &Script_EditorGetSelectionCount },
    { "EditorGetSelection", &Script_EditorGetSelection },
    { "EditorClearSelection", &Script_EditorClearSelection },
    { "EditorGetObjectTransform", &Script_EditorGetObjectTransform },
    { "EditorSetObjectTransform", &Script_EditorSetObjectTransform },
    { "EditorObjectSnapToGround", &Script_EditorObjectSnapToGround },
    { "EditorGetObjectSetting", &Script_EditorGetObjectSetting },
    { "EditorSetObjectSetting", &Script_EditorSetObjectSetting },
    { "EditorPickObject", &Script_EditorPickObject },
    { "EditorMoveSelection", &Script_EditorMoveSelection },
    { "EditorGetChunkTool", &Script_EditorGetChunkTool },
    { "EditorSetChunkTool", &Script_EditorSetChunkTool },
    { "EditorGetAreas", &Script_EditorGetAreas },
    { "EditorSetTileHoles", &Script_EditorSetTileHoles },
    { "EditorSetTileArea", &Script_EditorSetTileArea },
    { "EditorPaintVertex", &Script_EditorPaintVertex },
    { "EditorSetHoles", &Script_EditorSetHoles },
    { "EditorSetImpass", &Script_EditorSetImpass },
    { "EditorGetChunkInfo", &Script_EditorGetChunkInfo },
    { "EditorDeleteSelection", &Script_EditorDeleteSelection },
    { "EditorCopySelection", &Script_EditorCopySelection },
    { "EditorPasteObjects", &Script_EditorPasteObjects },
    { "EditorDuplicateSelection", &Script_EditorDuplicateSelection },
    { "EditorHasClipboard", &Script_EditorHasClipboard },
    { "EditorGetModelCount", &Script_EditorGetModelCount },
    { "EditorGetModel", &Script_EditorGetModel },
    { "EditorSetClipboardModel", &Script_EditorSetClipboardModel },
    { "EditorQuit", &Script_EditorQuit },
};

void EditorRegisterScriptFunctions() {
    for (uint32_t i = 0; i < sizeof(s_editorFunctions) / sizeof(s_editorFunctions[0]); i++) {
        FrameScript_RegisterFunction(s_editorFunctions[i].m_name, s_editorFunctions[i].m_method);
    }
}
