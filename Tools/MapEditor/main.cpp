#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Base/Base.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Os/Path.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Services/AsyncFile.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/Startup_StringsRec.h"
#include "WoW/Source/ConsoleDevice/ConsoleDevice.h"
#include "EditorApp.h"
#include "EditorProject.h"
#include "TextureEdit.h"

static const char* s_baseArchives[] = {
    "common.MPQ",
    "common-2.MPQ",
    "expansion.MPQ",
    "lichking.MPQ",
};

static const char* s_localeArchives[] = {
    "locale-%s.MPQ",
    "expansion-locale-%s.MPQ",
    "lichking-locale-%s.MPQ",
};

static const char* s_locales[] = {
    "enUS", "enGB", "deDE", "frFR", "esES", "esMX", "ruRU", "koKR", "zhCN", "zhTW", "enCN", "enTW", "ptBR", "ptPT", "itIT",
};

static const char s_patchSuffixes[] = "23456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

extern WowClientDB<Startup_StringsRec> g_startup_StringsDB;

static SArchive* s_archiveHandles[128];
static uint32_t s_archiveCount;
static char s_locale[8] = "enUS";

static const char* s_defaultConfig =
    "SET gxWindow \"1\"\n"
    "SET gxMaximize \"0\"\n"
    "SET gxResolution \"1600x900\"\n"
    "SET hwDetect \"0\"\n";

static bool ArchiveExists(const char* clientPath, const char* name) {
    char path[520];
    SStrPrintf(path, sizeof(path), "%sData\\%s", clientPath, name);
    FILE* file = fopen(path, "rb");

    if (!file) {
        return false;
    }

    fclose(file);
    return true;
}

static bool OpenArchive(const char* clientPath, const char* name, bool required) {
    if (!ArchiveExists(clientPath, name)) {
        if (required) {
            printf("MapEditor: %sData\\%s is missing\n", clientPath, name);
        }

        return !required;
    }

    if (s_archiveCount >= sizeof(s_archiveHandles) / sizeof(s_archiveHandles[0])) {
        return true;
    }

    char path[260];
    SStrPrintf(path, sizeof(path), "Data\\%s", name);

    if (!SFile::OpenArchive(path, 100 + s_archiveCount, 0, &s_archiveHandles[s_archiveCount])) {
        printf("MapEditor: SFile::OpenArchive(%s) failed, error %u\n", path, SErrGetLastError());
        return !required;
    }

    s_archiveCount++;
    return true;
}

static void DetectLocale(const char* clientPath) {
    for (uint32_t i = 0; i < sizeof(s_locales) / sizeof(s_locales[0]); i++) {
        char name[64];
        SStrPrintf(name, sizeof(name), "%s\\locale-%s.MPQ", s_locales[i], s_locales[i]);

        if (ArchiveExists(clientPath, name)) {
            SStrCopy(s_locale, s_locales[i], sizeof(s_locale));
            return;
        }
    }
}

static bool OpenArchives(const char* clientPath, const char* basePath) {
    SFile::DisableSFileCheckDisk();
    SFile::EnableDirectAccess(1);
    SFile::SetBasePath(clientPath);
    SFile::SetDataPath("Data\\");
    DetectLocale(clientPath);

    char name[64];

    for (uint32_t i = 0; i < sizeof(s_baseArchives) / sizeof(s_baseArchives[0]); i++) {
        if (!OpenArchive(clientPath, s_baseArchives[i], i == 0)) {
            return false;
        }
    }

    for (uint32_t i = 0; i < sizeof(s_localeArchives) / sizeof(s_localeArchives[0]); i++) {
        char file[64];
        SStrPrintf(file, sizeof(file), s_localeArchives[i], s_locale);
        SStrPrintf(name, sizeof(name), "%s\\%s", s_locale, file);
        OpenArchive(clientPath, name, false);
    }

    OpenArchive(clientPath, "patch.MPQ", false);

    for (const char* suffix = s_patchSuffixes; *suffix; suffix++) {
        SStrPrintf(name, sizeof(name), "patch-%c.MPQ", *suffix);
        OpenArchive(clientPath, name, false);
    }

    SStrPrintf(name, sizeof(name), "%s\\patch-%s.MPQ", s_locale, s_locale);
    OpenArchive(clientPath, name, false);

    for (const char* suffix = s_patchSuffixes; *suffix; suffix++) {
        SStrPrintf(name, sizeof(name), "%s\\patch-%s-%c.MPQ", s_locale, s_locale, *suffix);
        OpenArchive(clientPath, name, false);
    }

    SFile::SetBasePath(basePath);
    return true;
}

static void WriteDefaultConfig(const char* basePath) {
    char path[260];
    SStrPrintf(path, sizeof(path), "%sWTF\\MapEditor.wtf", basePath);
    FILE* file = fopen(path, "rb");

    if (file) {
        fclose(file);
        return;
    }

    char directory[260];
    SStrPrintf(directory, sizeof(directory), "%sWTF", basePath);
    OsCreateDirectory(directory, 0);
    file = fopen(path, "wb");

    if (file) {
        fputs(s_defaultConfig, file);
        fclose(file);
    }
}

static void Usage() {
    printf("usage: MapEditor [--client path] [--project path] [map [x y z [facing]]]\n");
    printf("  no map: shows the project selection\n");
    printf("  map: Map.dbc directory (Azeroth, Kalimdor, Expansion01, Northrend, ...) or map id\n");
    printf("  camera: WASD move, Q/E down/up, Shift fast, hold right mouse to look\n");
    printf("  editing: left mouse applies the brush, Ctrl inverts raise, wheel = radius, Shift+wheel = speed\n");
    printf("  keys: 1 raise/lower, 2 flatten, 3 smooth, Ctrl+Z undo, Ctrl+Y redo\n");
}

int main(int argc, char** argv) {
    const char* positional[5];
    const char* clientArg = nullptr;
    int32_t positionalCount = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            Usage();
            return 0;
        }

        if (!strcmp(argv[i], "--lua") && i + 1 < argc) {
            EditorApp::SetStartupLua(argv[++i]);
            continue;
        }

        if (!strcmp(argv[i], "--client") && i + 1 < argc) {
            clientArg = argv[++i];
            continue;
        }

        if (!strcmp(argv[i], "--project") && i + 1 < argc) {
            EditorProject::SetStartupProject(argv[++i]);
            continue;
        }

        if (!strcmp(argv[i], "--quit-after") && i + 1 < argc) {
            EditorApp::SetQuitAfter(static_cast<float>(atof(argv[++i])));
            continue;
        }

        if (positionalCount < 5) {
            positional[positionalCount++] = argv[i];
        } else {
            Usage();
            return 1;
        }
    }

    if (positionalCount == 2 || positionalCount == 3) {
        Usage();
        return 1;
    }

    if (positionalCount >= 4) {
        C3Vector position(static_cast<float>(atof(positional[1])), static_cast<float>(atof(positional[2])), static_cast<float>(atof(positional[3])));
        EditorApp::SetStartup(positional[0], &position, positionalCount == 5 ? static_cast<float>(atof(positional[4])) : 0.0f);
    } else if (positionalCount == 1) {
        EditorApp::SetStartup(positional[0], nullptr, 0.0f);
    }

    StormRtlInitialize();
    StormInitialize();

    char basePath[260];
    OsGetExePath(basePath, sizeof(basePath));

    char clientPath[260];
    SStrCopy(clientPath, clientArg ? clientArg : basePath, sizeof(clientPath));
    uint32_t clientLength = SStrLen(clientPath);

    if (clientLength && clientPath[clientLength - 1] != '\\' && clientPath[clientLength - 1] != '/') {
        SStrPack(clientPath, "\\", sizeof(clientPath));
    }

    if (!OpenArchives(clientPath, basePath)) {
        return 1;
    }

    EditorProject::SetClientPath(clientPath);
    EditorProject::ApplyStartupProject();
    TextureEdit::SetArchives(s_archiveHandles, s_archiveCount);

    WriteDefaultConfig(basePath);
    ConsoleInitializeClientCommand();
    CVar::Initialize("MapEditor.wtf");
    CVar::Register("locale", "Set the game locale", 0, s_locale, nullptr, CATEGORY_DEFAULT, false, nullptr, false);

    BaseInitializeGlobal();
    EventInitialize(1, 0);
    OsTimeStartup(OsTimeManager::BestAvailable);
    AsyncFileReadInitialize(0, 100);

    g_startup_StringsDB.Load(__FILE__, __LINE__);
    ConsoleDeviceInitialize("Map Editor", 1, 1);

    EventCreateContextEx(1, EditorApp::InitializeEngine, EditorApp::DestroyEngine, 0, 0);
    EventDoMessageLoop();
    EventDestroy();

    AsyncFileReadDestroy();
    OsTimeShutdown();

    return 0;
}
