#include "EditorApp.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Engine/Source/Base/Base.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Event/CSizeEvent.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Frame/CLayoutFrame.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/SimpleFrameScript.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuLight.h"
#include "Engine/Source/Model2/M2Cache.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Scrn/Scrn.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/Services/SysMessage.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "Engine/Source/Tempest/CRect.h"
#include "WoW/Source/Console/ConsoleClient.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Glue/CWOWClientStatus.h"
#include "WoW/Source/Ui/Screen.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/UIUtil/InputControl.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/WorldParam.h"
#include "WoW/Source/WorldClient/shadow.h"
#include "ChunkEdit.h"
#include "EditorProject.h"
#include "EditorScripts.h"
#include "EditorUndo.h"
#include "ObjectEdit.h"
#include "TerrainEdit.h"
#include "TextureEdit.h"
#include "EditorViewport.h"
#include "EditorWorld.h"

namespace EditorApp {
    static const char* s_mapArg;
    static char s_mapDirectory[260];
    static int32_t s_mapId = -1;
    static C3Vector s_startPosition(-9449.0f, 64.0f, 80.0f);
    static float s_startYaw = 0.0f;
    static CSimpleTop* s_simpleTop;
    static MipBits* s_cursorMipBits;
    static float s_aspect;
    static uint32_t s_quitAfterMs;
    static uint32_t s_startTime;
    static const char* s_startupLua;
    static bool s_startupLuaPending;
    static bool s_mapLoaded;

    void SetStartup(const char* map, const C3Vector* position, float yaw) {
        if (map) {
            s_mapArg = map;
        }

        if (position) {
            s_startPosition = *position;
            s_startYaw = yaw;
        }
    }

    const char* GetMapDirectory() {
        return s_mapDirectory;
    }

    int32_t GetMapId() {
        return s_mapId;
    }

    void SetStartupLua(const char* code) {
        s_startupLua = code;
    }

    void SetQuitAfter(float seconds) {
        s_quitAfterMs = static_cast<uint32_t>(seconds * 1000.0f);
    }

    static int32_t QuitTimer(const void* data, void* param) {
        if (s_startupLuaPending && OsGetAsyncTimeMs() - s_startTime >= 3000) {
            s_startupLuaPending = false;
            FrameScript_Execute(s_startupLua, "startup", nullptr);
        }

        if (s_quitAfterMs && OsGetAsyncTimeMs() - s_startTime >= s_quitAfterMs) {
            s_quitAfterMs = 0;
            EventPostClose();
        }

        return 1;
    }

    void Quit() {
        EventPostClose();
    }

    static bool ResolveMap() {
        if (!s_mapArg) {
            return true;
        }

        char* end = nullptr;
        long id = strtol(s_mapArg, &end, 10);
        bool numeric = end && end != s_mapArg && *end == '\0';

        for (int32_t i = 0; i < g_mapDB.m_numRecords; i++) {
            MapRec* rec = &g_mapDB.m_records[i];

            if ((numeric && rec->m_id == id) || (!numeric && rec->m_directory && !SStrCmpI(rec->m_directory, s_mapArg, STORM_MAX_STR))) {
                s_mapId = rec->m_id;
                SStrCopy(s_mapDirectory, rec->m_directory, sizeof(s_mapDirectory));
                return true;
            }
        }

        if (numeric) {
            printf("MapEditor: map id %ld not in Map.dbc\n", id);
            return false;
        }

        printf("MapEditor: map '%s' not in Map.dbc, loading it as directory with id 0\n", s_mapArg);
        s_mapId = 0;
        SStrCopy(s_mapDirectory, s_mapArg, sizeof(s_mapDirectory));
        return true;
    }

    static void ProjectTextureCallback(const CAaBox& box, const CImVector& color, int32_t index, void* param, int32_t flags) {
    }

    static void ApplyCamera(const C3Vector& position, float yaw, float pitch) {
        EditorViewport* viewport = EditorViewport::s_current;

        if (viewport) {
            viewport->m_position = position;
            viewport->m_yaw = yaw;
            viewport->m_pitch = pitch < -1.55f ? -1.55f : (pitch > 1.55f ? 1.55f : pitch);
        }
    }

    bool LoadMap(int32_t mapId, const C3Vector& position, float yaw, float pitch) {
        MapRec* rec = g_mapDB.GetRecord(mapId);

        if (!rec || !rec->m_directory) {
            return false;
        }

        if (s_mapLoaded) {
            UnloadMap();
        }

        s_mapId = rec->m_id;
        SStrCopy(s_mapDirectory, rec->m_directory, sizeof(s_mapDirectory));
        EditorWorld::LoadMap(s_mapDirectory, s_mapId);
        s_mapLoaded = true;
        ApplyCamera(position, yaw, pitch);
        return true;
    }

    void UnloadMap() {
        if (!s_mapLoaded) {
            return;
        }

        TerrainEdit::EndStroke();
        TextureEdit::Reset();
        ObjectEdit::Reset();
        ChunkEdit::Reset();
        EditorUndo::Clear();
        World::UnloadMap();
        s_mapLoaded = false;
        s_mapId = -1;
        s_mapDirectory[0] = '\0';
    }

    bool IsMapLoaded() {
        return s_mapLoaded;
    }

    static void HandleDisplaySizeChanged(const CSizeEvent& event) {
        s_aspect = Screen::GetAspectRatio();
        CoordinateSetAspectRatio(s_aspect);

        CSimpleFrame* parent = static_cast<CSimpleFrame*>(CScriptObject::GetScriptObjectByName("EditorParent", CSimpleFrame::GetObjectType()));

        if (parent) {
            parent->SetFrameScale(1.0f, true);
            CLayoutFrame::ResizePending();
        }
    }

    static void InitializeUI() {
        s_aspect = Screen::GetAspectRatio();
        CoordinateSetAspectRatio(s_aspect);

        s_simpleTop = new (SMemAlloc(sizeof(CSimpleTop), __FILE__, __LINE__, 0)) CSimpleTop();
        s_simpleTop->m_displaySizeCallback = HandleDisplaySizeChanged;

        uint32_t width = 0;
        uint32_t height = 0;
        PIXEL_FORMAT format = PIXEL_ARGB8888;
        s_cursorMipBits = TextureLoadImage("Interface\\Cursor\\Point.blp", &width, &height, &format, nullptr, nullptr, nullptr, 0);
        s_simpleTop->SetCursor(s_cursorMipBits);

        CGInputControl* control = CGInputControl::GetActive();
        control->m_mouseModeFlags = 3;
        control->m_forceCursorOn = 0;
        control->UpdateMouseMode(1);

        FrameScript_Flush();
        SystemRegisterFunctions();
        RegisterSimpleFrameScriptMethods();
        EditorRegisterScriptFunctions();
        EditorProject::RegisterScriptFunctions();
        FrameXML_RegisterFactory("EditorViewport", EditorViewport::Create, true);

        OsCreateDirectory("Logs", 0);
        CWOWClientStatus status("Logs\\MapEditorXML.log");

        MD5_CTX md5;
        MD5Init(&md5);
        FrameXML_FreeHashNodes();
        FrameXML_CreateFrames("Interface\\MapEditor\\MapEditor.toc", nullptr, &md5, &status);

        for (CStatus::STATUSENTRY* entry = status.m_statusList.Head(); entry; entry = status.m_statusList.Next(entry)) {
            if (entry->m_severity > STATUS_INFO) {
                printf("FrameXML: %s\n", entry->m_text);
            }
        }
    }

    int32_t InitializeEngine(const void* data, void* param) {
        BaseInitializeContext();
        TextureInitialize();
        ScrnInitialize(0);
        ConsoleScreenInitialize(nullptr);
        M2Initialize(0, 0);
        EventRegister(EVENT_ID_POLL, AsyncFileReadPollHandler);

        SysMsgInitialize();
        ClientDBInitialize();
        FrameScript_Initialize(0);
        FrameXML_RegisterDefault();

        if (!ResolveMap()) {
            EventPostClose();
            return 1;
        }

        CWorldParam::Initialize();
        CWorld::Initialize();
        World::s_m2Scene->SetProjectTextureCallback(ProjectTextureCallback, nullptr);
        ShadowInit();
        GxuLightInitialize();
        GxuLightSetGridSize(16.665001f);
        g_theGxDevicePtr->MasterEnableSet(GxMasterEnable_Fog, 1);
        InputControlInitialize();

        if (s_mapArg) {
            EditorWorld::LoadMap(s_mapDirectory, s_mapId);
            s_mapLoaded = true;
        }

        InitializeUI();

        if (EditorViewport::s_current) {
            EditorViewport::s_current->m_position = s_startPosition;
            EditorViewport::s_current->m_yaw = s_startYaw;
        } else {
            printf("MapEditor: no EditorViewport frame was created, see Logs\\MapEditorXML.log\n");
        }

        s_startTime = OsGetAsyncTimeMs();
        s_startupLuaPending = s_startupLua != nullptr;
        EventRegister(EVENT_ID_IDLE, QuitTimer);
        fflush(stdout);
        return 1;
    }

    int32_t DestroyEngine(const void* data, void* param) {
        EventUnregister(EVENT_ID_POLL, AsyncFileReadPollHandler);
        return 1;
    }
}
