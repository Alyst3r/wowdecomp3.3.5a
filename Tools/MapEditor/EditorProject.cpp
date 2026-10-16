#include "EditorProject.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/Time.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/CRect.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "EditorApp.h"
#include "EditorSave.h"
#include "EditorViewport.h"
#include "TerrainEdit.h"

namespace EditorProject {
    static const float s_zeroPoint = 17066.666f;
    static const float s_tileSize = 533.33333f;
    static const char* s_settingsFile = "WTF\\MapEditorSettings.txt";
    static const char* s_defaultVersion = "Wrath Of The Lich King";

    static const uint32_t FOURCC_MVER = 0x4D564552;
    static const uint32_t FOURCC_MPHD = 0x4D504844;
    static const uint32_t FOURCC_MAIN = 0x4D41494E;
    static const uint32_t FOURCC_MODF = 0x4D4F4446;
    static const uint32_t FOURCC_MAOF = 0x4D414F46;
    static const uint32_t FOURCC_MARE = 0x4D415245;

    struct Settings {
        std::vector<std::string> m_recent;
        std::string m_favorite;
        std::string m_gamePath;
        int32_t m_mapComboSearch = 2;
        bool m_loaded = false;
    };

    struct MapEntry {
        int32_t m_id;
        std::string m_name;
        std::string m_directory;
        int32_t m_type;
        int32_t m_expansion;
        bool m_wmo;
    };

    struct WdtInfo {
        bool m_valid = false;
        bool m_globalWmo = false;
        uint8_t m_tiles[4096] = {};
        float m_wmoPos[3] = {};
        float m_wmoExtents[6] = {};
    };

    struct WdlTile {
        int16_t m_height17[17][17];
        int16_t m_height16[16][16];
    };

    static Settings s_settings;
    static Project s_project;
    static bool s_hasProject;
    static std::string s_startupProject;
    static bool s_autoLoadChecked;
    static std::string s_clientPath;
    static std::vector<MapEntry> s_maps;
    static bool s_mapsBuilt;

    struct JsonValue {
        enum TYPE {
            TYPE_NULL = 0,
            TYPE_BOOL = 1,
            TYPE_NUMBER = 2,
            TYPE_STRING = 3,
            TYPE_ARRAY = 4,
            TYPE_OBJECT = 5,
        };

        TYPE m_type = TYPE_NULL;
        bool m_bool = false;
        double m_number = 0.0;
        std::string m_string;
        std::vector<JsonValue> m_array;
        std::vector<std::pair<std::string, JsonValue>> m_object;

        const JsonValue* Get(const char* key) const {
            if (this->m_type != TYPE_OBJECT) {
                return nullptr;
            }

            for (const auto& member : this->m_object) {
                if (member.first == key) {
                    return &member.second;
                }
            }

            return nullptr;
        }

        double Number(const char* key, double fallback) const {
            const JsonValue* value = this->Get(key);
            return value && value->m_type == TYPE_NUMBER ? value->m_number : fallback;
        }

        std::string String(const char* key) const {
            const JsonValue* value = this->Get(key);
            return value && value->m_type == TYPE_STRING ? value->m_string : std::string();
        }
    };

    class JsonParser {
        public:
        const char* m_cur;
        const char* m_end;

        JsonParser(const char* data, size_t size) : m_cur(data), m_end(data + size) {
        }

        void SkipSpace() {
            while (this->m_cur < this->m_end && (*this->m_cur == ' ' || *this->m_cur == '\t' || *this->m_cur == '\r' || *this->m_cur == '\n')) {
                this->m_cur++;
            }
        }

        static void AppendUtf8(std::string& out, uint32_t codepoint) {
            if (codepoint < 0x80) {
                out += static_cast<char>(codepoint);
            } else if (codepoint < 0x800) {
                out += static_cast<char>(0xC0 | (codepoint >> 6));
                out += static_cast<char>(0x80 | (codepoint & 0x3F));
            } else if (codepoint < 0x10000) {
                out += static_cast<char>(0xE0 | (codepoint >> 12));
                out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (codepoint & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (codepoint >> 18));
                out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (codepoint & 0x3F));
            }
        }

        uint32_t ParseHex4() {
            uint32_t value = 0;

            for (int32_t i = 0; i < 4 && this->m_cur < this->m_end; i++, this->m_cur++) {
                char c = *this->m_cur;
                value <<= 4;

                if (c >= '0' && c <= '9') {
                    value |= c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    value |= c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    value |= c - 'A' + 10;
                }
            }

            return value;
        }

        bool ParseString(std::string& out) {
            if (this->m_cur >= this->m_end || *this->m_cur != '"') {
                return false;
            }

            this->m_cur++;

            while (this->m_cur < this->m_end && *this->m_cur != '"') {
                char c = *this->m_cur++;

                if (c != '\\') {
                    out += c;
                    continue;
                }

                if (this->m_cur >= this->m_end) {
                    return false;
                }

                char escape = *this->m_cur++;

                switch (escape) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        uint32_t codepoint = this->ParseHex4();

                        if (codepoint >= 0xD800 && codepoint < 0xDC00 && this->m_end - this->m_cur >= 6 && this->m_cur[0] == '\\' && this->m_cur[1] == 'u') {
                            this->m_cur += 2;
                            uint32_t low = this->ParseHex4();
                            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                        }

                        AppendUtf8(out, codepoint);
                        break;
                    }
                    default: out += escape; break;
                }
            }

            if (this->m_cur >= this->m_end) {
                return false;
            }

            this->m_cur++;
            return true;
        }

        bool Parse(JsonValue& value) {
            this->SkipSpace();

            if (this->m_cur >= this->m_end) {
                return false;
            }

            char c = *this->m_cur;

            if (c == '{') {
                value.m_type = JsonValue::TYPE_OBJECT;
                this->m_cur++;
                this->SkipSpace();

                if (this->m_cur < this->m_end && *this->m_cur == '}') {
                    this->m_cur++;
                    return true;
                }

                while (true) {
                    this->SkipSpace();
                    std::pair<std::string, JsonValue> member;

                    if (!this->ParseString(member.first)) {
                        return false;
                    }

                    this->SkipSpace();

                    if (this->m_cur >= this->m_end || *this->m_cur != ':') {
                        return false;
                    }

                    this->m_cur++;

                    if (!this->Parse(member.second)) {
                        return false;
                    }

                    value.m_object.push_back(member);
                    this->SkipSpace();

                    if (this->m_cur < this->m_end && *this->m_cur == ',') {
                        this->m_cur++;
                        continue;
                    }

                    if (this->m_cur < this->m_end && *this->m_cur == '}') {
                        this->m_cur++;
                        return true;
                    }

                    return false;
                }
            }

            if (c == '[') {
                value.m_type = JsonValue::TYPE_ARRAY;
                this->m_cur++;
                this->SkipSpace();

                if (this->m_cur < this->m_end && *this->m_cur == ']') {
                    this->m_cur++;
                    return true;
                }

                while (true) {
                    JsonValue element;

                    if (!this->Parse(element)) {
                        return false;
                    }

                    value.m_array.push_back(element);
                    this->SkipSpace();

                    if (this->m_cur < this->m_end && *this->m_cur == ',') {
                        this->m_cur++;
                        continue;
                    }

                    if (this->m_cur < this->m_end && *this->m_cur == ']') {
                        this->m_cur++;
                        return true;
                    }

                    return false;
                }
            }

            if (c == '"') {
                value.m_type = JsonValue::TYPE_STRING;
                return this->ParseString(value.m_string);
            }

            if (this->m_end - this->m_cur >= 4 && !strncmp(this->m_cur, "true", 4)) {
                value.m_type = JsonValue::TYPE_BOOL;
                value.m_bool = true;
                this->m_cur += 4;
                return true;
            }

            if (this->m_end - this->m_cur >= 5 && !strncmp(this->m_cur, "false", 5)) {
                value.m_type = JsonValue::TYPE_BOOL;
                this->m_cur += 5;
                return true;
            }

            if (this->m_end - this->m_cur >= 4 && !strncmp(this->m_cur, "null", 4)) {
                this->m_cur += 4;
                return true;
            }

            std::string number;

            while (this->m_cur < this->m_end && strchr("+-0123456789.eE", *this->m_cur)) {
                number += *this->m_cur++;
            }

            if (number.empty()) {
                return false;
            }

            value.m_type = JsonValue::TYPE_NUMBER;
            value.m_number = atof(number.c_str());
            return true;
        }
    };

    static std::wstring Widen(const std::string& text) {
        if (text.empty()) {
            return std::wstring();
        }

        int32_t length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), nullptr, 0);
        std::wstring result(length, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int32_t>(text.size()), &result[0], length);
        return result;
    }

    static std::string Narrow(const wchar_t* text) {
        if (!text || !*text) {
            return std::string();
        }

        int32_t length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        std::string result(length > 0 ? length - 1 : 0, '\0');

        if (length > 1) {
            WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], length, nullptr, nullptr);
        }

        return result;
    }

    static std::string GenericPath(std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    static std::string NativePath(std::string path) {
        std::replace(path.begin(), path.end(), '/', '\\');

        while (path.size() > 3 && path.back() == '\\') {
            path.pop_back();
        }

        return path;
    }

    static std::string NormalizedPath(const std::string& path) {
        std::wstring wide = Widen(NativePath(path));
        wchar_t full[MAX_PATH * 2];

        if (!GetFullPathNameW(wide.c_str(), MAX_PATH * 2, full, nullptr)) {
            return std::string();
        }

        std::string result = NativePath(Narrow(full));
        std::transform(result.begin(), result.end(), result.begin(), [](char c) { return static_cast<char>(tolower(static_cast<unsigned char>(c))); });
        return result;
    }

    static bool DirectoryExists(const std::string& path) {
        DWORD attributes = GetFileAttributesW(Widen(NativePath(path)).c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    static bool FileExists(const std::string& path) {
        DWORD attributes = GetFileAttributesW(Widen(NativePath(path)).c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    static bool ReadWholeFile(const std::string& path, std::string& out) {
        FILE* file = _wfopen(Widen(NativePath(path)).c_str(), L"rb");

        if (!file) {
            return false;
        }

        char buffer[4096];
        size_t count;

        while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            out.append(buffer, count);
        }

        fclose(file);
        return true;
    }

    static bool WriteWholeFile(const std::string& path, const std::string& data) {
        FILE* file = _wfopen(Widen(NativePath(path)).c_str(), L"wb");

        if (!file) {
            return false;
        }

        bool ok = fwrite(data.data(), 1, data.size(), file) == data.size();
        fclose(file);
        return ok;
    }

    static void LoadSettings() {
        if (s_settings.m_loaded) {
            return;
        }

        s_settings.m_loaded = true;
        std::string data;

        if (!ReadWholeFile(s_settingsFile, data)) {
            return;
        }

        size_t start = 0;

        while (start < data.size()) {
            size_t end = data.find('\n', start);

            if (end == std::string::npos) {
                end = data.size();
            }

            std::string line = data.substr(start, end - start);
            start = end + 1;

            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }

            size_t equals = line.find('=');

            if (equals == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, equals);
            std::string value = line.substr(equals + 1);

            if (key == "recent_project") {
                s_settings.m_recent.push_back(value);
            } else if (key == "favorite_project") {
                s_settings.m_favorite = value;
            } else if (key == "game_path") {
                s_settings.m_gamePath = value;
            } else if (key == "mapComboSearch") {
                s_settings.m_mapComboSearch = atoi(value.c_str());
            }
        }
    }

    static void SaveSettings() {
        std::string data;

        for (const std::string& path : s_settings.m_recent) {
            data += "recent_project=" + path + "\r\n";
        }

        data += "favorite_project=" + s_settings.m_favorite + "\r\n";
        data += "game_path=" + s_settings.m_gamePath + "\r\n";
        data += "mapComboSearch=" + std::to_string(s_settings.m_mapComboSearch) + "\r\n";

        CreateDirectoryW(L"WTF", nullptr);
        WriteWholeFile(s_settingsFile, data);
    }

    static bool SamePath(const std::string& a, const std::string& b) {
        return NormalizedPath(a) == NormalizedPath(b);
    }

    static void RegisterProjectChange(const std::string& path) {
        LoadSettings();
        std::vector<std::string> recent;
        recent.push_back(path);

        for (const std::string& other : s_settings.m_recent) {
            if (DirectoryExists(other) && !SamePath(other, path)) {
                recent.push_back(other);
            }
        }

        s_settings.m_recent = recent;
        SaveSettings();
    }

    static void RegisterProjectRemove(const std::string& path) {
        LoadSettings();
        auto it = std::remove_if(s_settings.m_recent.begin(), s_settings.m_recent.end(), [&](const std::string& other) { return SamePath(other, path); });
        s_settings.m_recent.erase(it, s_settings.m_recent.end());

        if (!s_settings.m_favorite.empty() && SamePath(s_settings.m_favorite, path)) {
            s_settings.m_favorite.clear();
        }

        SaveSettings();
    }

    static std::string FindProjectFile(const std::string& directory) {
        WIN32_FIND_DATAW data;
        std::wstring pattern = Widen(NativePath(directory) + "\\*.noggitproj");
        HANDLE find = FindFirstFileW(pattern.c_str(), &data);

        if (find == INVALID_HANDLE_VALUE) {
            return std::string();
        }

        std::string result = NativePath(directory) + "\\" + Narrow(data.cFileName);
        FindClose(find);
        return result;
    }

    static bool ReadProject(const std::string& directory, Project& project) {
        if (!DirectoryExists(directory)) {
            return false;
        }

        std::string fileName = FindProjectFile(directory);
        std::string data;

        if (fileName.empty() || !ReadWholeFile(fileName, data)) {
            return false;
        }

        JsonValue root;
        JsonParser parser(data.data(), data.size());

        if (!parser.Parse(root)) {
            return false;
        }

        const JsonValue* configuration = root.Get("Project");

        if (!configuration || configuration->m_type != JsonValue::TYPE_OBJECT) {
            return false;
        }

        const JsonValue* client = configuration->Get("Client");

        if (!client || client->m_type != JsonValue::TYPE_OBJECT) {
            return false;
        }

        project = Project();
        project.m_path = GenericPath(directory);
        project.m_name = configuration->String("ProjectName");
        project.m_clientPath = client->String("ClientPath");
        project.m_clientVersion = client->String("ClientVersion");

        if (project.m_clientVersion.empty()) {
            project.m_clientVersion = s_defaultVersion;
        }

        const JsonValue* bookmarks = configuration->Get("Bookmarks");

        if (bookmarks && bookmarks->m_type == JsonValue::TYPE_ARRAY) {
            for (const JsonValue& entry : bookmarks->m_array) {
                Bookmark bookmark;
                bookmark.m_mapId = static_cast<int32_t>(entry.Number("MapId", 0.0));
                bookmark.m_name = entry.String("BookmarkName");
                bookmark.m_pitch = static_cast<float>(entry.Number("CameraPitch", 0.0));
                bookmark.m_yaw = static_cast<float>(entry.Number("CameraYaw", 0.0));
                const JsonValue* position = entry.Get("Position");
                bookmark.m_position[0] = position ? static_cast<float>(position->Number("X", 0.0)) : 0.0f;
                bookmark.m_position[1] = position ? static_cast<float>(position->Number("Y", 0.0)) : 0.0f;
                bookmark.m_position[2] = position ? static_cast<float>(position->Number("Z", 0.0)) : 0.0f;
                project.m_bookmarks.push_back(bookmark);
            }
        }

        const JsonValue* pinned = configuration->Get("PinnedMaps");

        if (pinned && pinned->m_type == JsonValue::TYPE_ARRAY) {
            for (const JsonValue& entry : pinned->m_array) {
                PinnedMap map;
                map.m_mapId = static_cast<int32_t>(entry.Number("MapId", 0.0));
                map.m_mapName = entry.String("MapName");
                project.m_pinnedMaps.push_back(map);
            }
        }

        return true;
    }

    static std::string JsonString(const std::string& text) {
        std::string out = "\"";

        for (char c : text) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }

        return out + "\"";
    }

    static std::string JsonNumber(double value) {
        if (value == floor(value) && fabs(value) < 1e15) {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%.0f", value);
            return buffer;
        }

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.17g", value);
        return buffer;
    }

    bool Save(const Project& project) {
        std::string pad1 = "    ";
        std::string pad2 = pad1 + pad1;
        std::string pad3 = pad2 + pad1;
        std::string pad4 = pad3 + pad1;
        std::string pad5 = pad4 + pad1;
        std::string out = "{\n" + pad1 + "\"Project\": {\n";

        out += pad2 + "\"Bookmarks\": [\n";

        for (size_t i = 0; i < project.m_bookmarks.size(); i++) {
            const Bookmark& bookmark = project.m_bookmarks[i];
            out += pad3 + "{\n";
            out += pad4 + "\"BookmarkName\": " + JsonString(bookmark.m_name) + ",\n";
            out += pad4 + "\"CameraPitch\": " + JsonNumber(bookmark.m_pitch) + ",\n";
            out += pad4 + "\"CameraYaw\": " + JsonNumber(bookmark.m_yaw) + ",\n";
            out += pad4 + "\"MapId\": " + JsonNumber(bookmark.m_mapId) + ",\n";
            out += pad4 + "\"Position\": {\n";
            out += pad5 + "\"X\": " + JsonNumber(bookmark.m_position[0]) + ",\n";
            out += pad5 + "\"Y\": " + JsonNumber(bookmark.m_position[1]) + ",\n";
            out += pad5 + "\"Z\": " + JsonNumber(bookmark.m_position[2]) + "\n";
            out += pad4 + "}\n";
            out += pad3 + "}" + (i + 1 < project.m_bookmarks.size() ? ",\n" : "\n");
        }

        out += pad2 + "],\n";
        out += pad2 + "\"Client\": {\n";
        out += pad3 + "\"ClientPath\": " + JsonString(project.m_clientPath) + ",\n";
        out += pad3 + "\"ClientVersion\": " + JsonString(project.m_clientVersion) + "\n";
        out += pad2 + "},\n";
        out += pad2 + "\"PinnedMaps\": [\n";

        for (size_t i = 0; i < project.m_pinnedMaps.size(); i++) {
            const PinnedMap& map = project.m_pinnedMaps[i];
            out += pad3 + "{\n";
            out += pad4 + "\"MapId\": " + JsonNumber(map.m_mapId) + ",\n";
            out += pad4 + "\"MapName\": " + JsonString(map.m_mapName) + "\n";
            out += pad3 + "}" + (i + 1 < project.m_pinnedMaps.size() ? ",\n" : "\n");
        }

        out += pad2 + "],\n";
        out += pad2 + "\"ProjectName\": " + JsonString(project.m_name) + "\n";
        out += pad1 + "}\n}\n";

        return WriteWholeFile(NativePath(project.m_path) + "\\" + project.m_name + ".noggitproj", out);
    }

    void SetStartupProject(const char* path) {
        s_startupProject = path ? path : "";
    }

    static void SetOverridePath(const std::string& path) {
        SFile::SetDataPathAlternate(path.empty() ? "" : NativePath(path).c_str());
        SFile::RebuildHash();
    }

    void ApplyStartupProject() {
        if (!s_startupProject.empty() && DirectoryExists(s_startupProject)) {
            SetOverridePath(s_startupProject);
        }
    }

    void SetClientPath(const char* path) {
        s_clientPath = path ? path : "";
    }

    const char* GetClientPath() {
        return s_clientPath.c_str();
    }

    Project* GetCurrent() {
        return s_hasProject ? &s_project : nullptr;
    }

    static HWND MainWindow() {
        return g_theGxDevicePtr ? static_cast<HWND>(g_theGxDevicePtr->DeviceWindow()) : nullptr;
    }

    static std::string BrowseFolder(const char* title, const char* initial) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        IFileOpenDialog* dialog = nullptr;
        std::string result;

        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            return result;
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NODEREFERENCELINKS);
        dialog->SetTitle(Widen(title ? title : "").c_str());

        if (initial && *initial && DirectoryExists(initial)) {
            IShellItem* folder = nullptr;

            if (SUCCEEDED(SHCreateItemFromParsingName(Widen(NativePath(initial)).c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(MainWindow()))) {
            IShellItem* item = nullptr;

            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;

                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    result = GenericPath(Narrow(path));
                    CoTaskMemFree(path);
                }

                item->Release();
            }
        }

        dialog->Release();
        return result;
    }

    static std::string BrowseProjectFile() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        IFileOpenDialog* dialog = nullptr;
        std::string result;

        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            return result;
        }

        COMDLG_FILTERSPEC filter = { L"*.noggitproj", L"*.noggitproj" };
        dialog->SetFileTypes(1, &filter);
        dialog->SetTitle(L"Open File");

        if (SUCCEEDED(dialog->Show(MainWindow()))) {
            IShellItem* item = nullptr;

            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;

                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    result = GenericPath(Narrow(path));
                    CoTaskMemFree(path);
                }

                item->Release();
            }
        }

        dialog->Release();
        return result;
    }

    static void OpenDirectory(const std::string& path) {
        if (!DirectoryExists(path)) {
            return;
        }

        ShellExecuteW(nullptr, L"open", L"explorer.exe", Widen(NativePath(path)).c_str(), nullptr, SW_SHOWNORMAL);
    }

    static bool Relaunch(const std::string& clientPath, const std::string& projectPath) {
        wchar_t exePath[MAX_PATH * 2];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH * 2);

        std::wstring commandLine = L"\"" + std::wstring(exePath) + L"\" --client \"" + Widen(NativePath(clientPath)) + L"\" --project \"" + Widen(NativePath(projectPath)) + L"\"";
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};

        if (!CreateProcessW(exePath, &commandLine[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
            return false;
        }

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        EditorApp::Quit();
        return true;
    }

    static bool LoadArchiveFile(const std::string& name, std::vector<uint8_t>& out) {
        void* data = nullptr;
        uint32_t size = 0;

        if (!SFile::LoadFile(name.c_str(), &data, &size, 0, nullptr) || !data) {
            return false;
        }

        out.assign(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
        SFile::Unload(data);
        return true;
    }

    static bool ReadWdt(const std::string& directory, WdtInfo& info) {
        std::vector<uint8_t> data;

        if (!LoadArchiveFile("World\\Maps\\" + directory + "\\" + directory + ".wdt", data)) {
            return false;
        }

        size_t offset = 0;

        while (offset + 8 <= data.size()) {
            uint32_t fourcc;
            uint32_t size;
            memcpy(&fourcc, &data[offset], 4);
            memcpy(&size, &data[offset + 4], 4);
            const uint8_t* chunk = &data[offset + 8];

            if (offset + 8 + size > data.size()) {
                break;
            }

            if (fourcc == FOURCC_MPHD && size >= 4) {
                uint32_t flags;
                memcpy(&flags, chunk, 4);
                info.m_globalWmo = (flags & 1) != 0;
            } else if (fourcc == FOURCC_MAIN && size >= 4096 * 8) {
                for (uint32_t i = 0; i < 4096; i++) {
                    uint32_t flags;
                    memcpy(&flags, chunk + i * 8, 4);
                    info.m_tiles[i] = (flags & 1) ? 1 : 0;
                }
            } else if (fourcc == FOURCC_MODF && size >= 64) {
                memcpy(info.m_wmoPos, chunk + 8, 12);
                memcpy(info.m_wmoExtents, chunk + 32, 24);
            }

            offset += 8 + size;
        }

        info.m_valid = true;
        return true;
    }

    static bool ReadWdl(const std::string& directory, std::vector<WdlTile>& tiles, std::vector<uint8_t>& present) {
        std::vector<uint8_t> data;
        tiles.assign(4096, WdlTile());
        present.assign(4096, 0);

        if (!LoadArchiveFile("World\\Maps\\" + directory + "\\" + directory + ".wdl", data)) {
            return false;
        }

        size_t offset = 0;

        while (offset + 8 <= data.size()) {
            uint32_t fourcc;
            uint32_t size;
            memcpy(&fourcc, &data[offset], 4);
            memcpy(&size, &data[offset + 4], 4);

            if (fourcc == FOURCC_MAOF && offset + 8 + 4096 * 4 <= data.size()) {
                for (uint32_t i = 0; i < 4096; i++) {
                    uint32_t tileOffset;
                    memcpy(&tileOffset, &data[offset + 8 + i * 4], 4);

                    if (!tileOffset || tileOffset + 8 + sizeof(WdlTile) > data.size()) {
                        continue;
                    }

                    uint32_t tileFourcc;
                    memcpy(&tileFourcc, &data[tileOffset], 4);

                    if (tileFourcc != FOURCC_MARE) {
                        continue;
                    }

                    memcpy(&tiles[i], &data[tileOffset + 8], sizeof(WdlTile));
                    present[i] = 1;
                }

                return true;
            }

            offset += 8 + size;
        }

        return true;
    }

    static void BuildMaps() {
        if (s_mapsBuilt) {
            return;
        }

        s_mapsBuilt = true;

        for (int32_t i = 0; i < g_mapDB.m_numRecords; i++) {
            MapRec* rec = &g_mapDB.m_records[i];

            if (rec->m_instanceType < 0 || rec->m_instanceType > 5 || !rec->m_directory || !*rec->m_directory) {
                continue;
            }

            WdtInfo info;

            if (!ReadWdt(rec->m_directory, info)) {
                continue;
            }

            MapEntry entry;
            entry.m_id = rec->m_id;
            entry.m_name = rec->m_mapName ? rec->m_mapName : "";
            entry.m_directory = rec->m_directory;
            entry.m_type = rec->m_instanceType;
            entry.m_expansion = rec->m_expansionID;
            entry.m_wmo = info.m_globalWmo;
            s_maps.push_back(entry);
        }
    }

    static const MapEntry* FindMap(int32_t mapId) {
        BuildMaps();

        for (const MapEntry& entry : s_maps) {
            if (entry.m_id == mapId) {
                return &entry;
            }
        }

        return nullptr;
    }

    struct Rgba {
        float r;
        float g;
        float b;
        float a;
    };

    static Rgba ColorForHeight(int16_t height) {
        struct Range {
            float r;
            float g;
            float b;
            int32_t start;
            int32_t stop;
        };

        static const Range ranges[] = {
            { 20, 149, 7, 0, 600 },
            { 137, 84, 21, 600, 1200 },
            { 96, 96, 96, 1200, 1600 },
            { 255, 255, 255, 1600, 0x7FFF },
        };

        if (height < 0) {
            float blue = 255.0f + std::max(height / 2.0f, -255.0f);
            return { 0.0f, 0.0f, blue / 255.0f, 1.0f };
        }

        for (int32_t i = 0; i < 3; i++) {
            if (height >= ranges[i].start && height < ranges[i].stop) {
                float t = static_cast<float>(height - ranges[i].start) / static_cast<float>(ranges[i].stop - ranges[i].start);
                const Range& a = ranges[i];
                const Range& b = ranges[i + 1];
                return { (b.r * t + a.r * (1.0f - t)) / 255.0f, (b.g * t + a.g * (1.0f - t)) / 255.0f, (b.b * t + a.b * (1.0f - t)) / 255.0f, 1.0f };
            }
        }

        return { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    static void Blend(Rgba& dst, const Rgba& src) {
        float outA = src.a + dst.a * (1.0f - src.a);

        if (outA <= 0.0f) {
            dst = { 0.0f, 0.0f, 0.0f, 0.0f };
            return;
        }

        dst.r = (src.r * src.a + dst.r * dst.a * (1.0f - src.a)) / outA;
        dst.g = (src.g * src.a + dst.g * dst.a * (1.0f - src.a)) / outA;
        dst.b = (src.b * src.a + dst.b * dst.a * (1.0f - src.a)) / outA;
        dst.a = outA;
    }

    static void DrawRectOutline(std::vector<Rgba>& image, int32_t stride, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const Rgba& color) {
        int32_t limit = stride;

        auto plot = [&](int32_t x, int32_t y) {
            if (x >= 0 && y >= 0 && x < limit && y < limit) {
                Blend(image[y * stride + x], color);
            }
        };

        for (int32_t x = x0; x <= x1; x++) {
            plot(x, y0);

            if (y1 != y0) {
                plot(x, y1);
            }
        }

        for (int32_t y = y0 + 1; y < y1; y++) {
            plot(x0, y);

            if (x1 != x0) {
                plot(x1, y);
            }
        }
    }

    static bool WriteTga(const std::string& path, const std::vector<Rgba>& image, int32_t textureSize) {
        std::string data(18 + textureSize * textureSize * 4, '\0');
        data[2] = 2;
        data[12] = static_cast<char>(textureSize & 0xFF);
        data[13] = static_cast<char>(textureSize >> 8);
        data[14] = static_cast<char>(textureSize & 0xFF);
        data[15] = static_cast<char>(textureSize >> 8);
        data[16] = 32;
        data[17] = 8;

        for (int32_t y = 0; y < textureSize; y++) {
            const Rgba* row = &image[(textureSize - 1 - y) * textureSize];
            char* out = &data[18 + y * textureSize * 4];

            for (int32_t x = 0; x < textureSize; x++) {
                out[x * 4 + 0] = static_cast<char>(std::min(255.0f, row[x].b * 255.0f + 0.5f));
                out[x * 4 + 1] = static_cast<char>(std::min(255.0f, row[x].g * 255.0f + 0.5f));
                out[x * 4 + 2] = static_cast<char>(std::min(255.0f, row[x].r * 255.0f + 0.5f));
                out[x * 4 + 3] = static_cast<char>(std::min(255.0f, row[x].a * 255.0f + 0.5f));
            }
        }

        return WriteWholeFile(path, data);
    }

    static bool BuildMinimap(const MapEntry& map, int32_t tileSize, std::string& texturePath, int32_t& textureSize, std::string& tiles) {
        WdtInfo wdt;
        ReadWdt(map.m_directory, wdt);
        std::vector<WdlTile> wdl;
        std::vector<uint8_t> present;
        ReadWdl(map.m_directory, wdl, present);

        int32_t imageSize = tileSize * 64;
        textureSize = 1;

        while (textureSize < imageSize) {
            textureSize <<= 1;
        }

        std::vector<Rgba> image(textureSize * textureSize, Rgba { 0.0f, 0.0f, 0.0f, 0.0f });
        tiles.assign(4096, '0');

        for (int32_t y = 0; y < 64; y++) {
            for (int32_t x = 0; x < 64; x++) {
                int32_t index = y * 64 + x;
                tiles[index] = wdt.m_tiles[index] ? '1' : '0';

                for (int32_t j = 0; j < tileSize; j++) {
                    for (int32_t i = 0; i < tileSize; i++) {
                        Rgba& pixel = image[(y * tileSize + j) * textureSize + x * tileSize + i];

                        if (present[index]) {
                            pixel = ColorForHeight(wdl[index].m_height17[j * 16 / tileSize][i * 16 / tileSize]);
                        } else if (wdt.m_tiles[index]) {
                            pixel = { 200.0f / 255.0f, 100.0f / 255.0f, 25.0f / 255.0f, 1.0f };
                        }

                        Blend(pixel, Rgba { 1.0f, 1.0f, 1.0f, 30.0f / 255.0f });
                    }
                }
            }
        }

        for (int32_t y = 0; y < 64; y++) {
            for (int32_t x = 0; x < 64; x++) {
                Rgba pen = wdt.m_tiles[y * 64 + x] ? Rgba { 0.8f, 0.8f, 0.8f, 0.4f } : Rgba { 1.0f, 1.0f, 1.0f, 0.05f };
                DrawRectOutline(image, textureSize, x * tileSize, y * tileSize, x * tileSize + tileSize, y * tileSize + tileSize, pen);
            }
        }

        if (wdt.m_globalWmo) {
            float center = tileSize * 64 / 2.0f;
            int32_t x0 = static_cast<int32_t>(center + wdt.m_wmoExtents[0] / s_tileSize * tileSize);
            int32_t y0 = static_cast<int32_t>(center + wdt.m_wmoExtents[1] / s_tileSize * tileSize);
            int32_t x1 = static_cast<int32_t>(center + wdt.m_wmoExtents[3] / s_tileSize * tileSize);
            int32_t y1 = static_cast<int32_t>(center + wdt.m_wmoExtents[4] / s_tileSize * tileSize);
            DrawRectOutline(image, textureSize, std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1), Rgba { 1.0f, 0.0f, 0.0f, 1.0f });
        }

        CreateDirectoryW(L"Interface", nullptr);
        CreateDirectoryW(L"Interface\\MapEditor", nullptr);
        CreateDirectoryW(L"Interface\\MapEditor\\Cache", nullptr);
        texturePath = "Interface\\MapEditor\\Cache\\" + map.m_directory + "_" + std::to_string(tileSize) + ".tga";
        bool existed = FileExists(texturePath);

        if (!WriteTga(texturePath, image, textureSize)) {
            return false;
        }

        if (!existed) {
            SFile::RebuildHash();
        }

        return true;
    }

    static float HeightAtTile(const MapEntry& map, int32_t tileX, int32_t tileY) {
        std::vector<WdlTile> wdl;
        std::vector<uint8_t> present;
        ReadWdl(map.m_directory, wdl, present);
        int32_t index = tileY * 64 + tileX;

        if (index < 0 || index >= 4096 || !present[index]) {
            return 100.0f;
        }

        int16_t highest = wdl[index].m_height17[8][8];

        for (int32_t j = 6; j <= 10; j++) {
            for (int32_t i = 6; i <= 10; i++) {
                highest = std::max(highest, wdl[index].m_height17[j][i]);
            }
        }

        return highest + 50.0f;
    }

    static bool EnterMapAt(int32_t mapId, const float noggitPos[3], float noggitPitch, float noggitYaw) {
        C3Vector position(s_zeroPoint - noggitPos[2], s_zeroPoint - noggitPos[0], noggitPos[1]);
        float yaw = (noggitYaw + 180.0f) * 0.017453292f;
        float pitch = -noggitPitch * 0.017453292f;
        return EditorApp::LoadMap(mapId, position, yaw, pitch);
    }

    static void PushString(lua_State* L, const char* key, const std::string& value) {
        lua_pushstring(L, value.c_str());
        lua_setfield(L, -2, key);
    }

    static void PushNumber(lua_State* L, const char* key, double value) {
        lua_pushnumber(L, value);
        lua_setfield(L, -2, key);
    }

    static void PushBool(lua_State* L, const char* key, bool value) {
        lua_pushboolean(L, value);
        lua_setfield(L, -2, key);
    }

    static const char* OptString(lua_State* L, int32_t index) {
        const char* text = lua_isstring(L, index) ? lua_tolstring(L, index, nullptr) : nullptr;
        return text ? text : "";
    }

    static int32_t Script_EditorGetRecentProjects(lua_State* L) {
        LoadSettings();
        lua_newtable(L);
        int32_t count = 0;

        for (const std::string& path : s_settings.m_recent) {
            Project project;

            if (!ReadProject(path, project)) {
                continue;
            }

            lua_newtable(L);
            PushString(L, "path", GenericPath(path));
            PushString(L, "name", project.m_name);
            PushString(L, "version", project.m_clientVersion);
            PushString(L, "client", project.m_clientPath);
            PushBool(L, "favorite", !s_settings.m_favorite.empty() && SamePath(s_settings.m_favorite, path));
            lua_rawseti(L, -2, ++count);
        }

        return 1;
    }

    static int32_t Script_EditorSetFavoriteProject(lua_State* L) {
        LoadSettings();
        s_settings.m_favorite = OptString(L, 1);
        SaveSettings();
        return 0;
    }

    static int32_t Script_EditorForgetProject(lua_State* L) {
        RegisterProjectRemove(OptString(L, 1));
        return 0;
    }

    static int32_t Script_EditorGetStartupProject(lua_State* L) {
        LoadSettings();

        if (s_autoLoadChecked) {
            return 0;
        }

        s_autoLoadChecked = true;

        if (!s_startupProject.empty()) {
            lua_pushstring(L, GenericPath(s_startupProject).c_str());
            lua_pushboolean(L, false);
            return 2;
        }

        Project project;

        if (!s_settings.m_favorite.empty() && ReadProject(s_settings.m_favorite, project)) {
            lua_pushstring(L, GenericPath(s_settings.m_favorite).c_str());
            lua_pushboolean(L, true);
            return 2;
        }

        return 0;
    }

    static int32_t Script_EditorCreateProject(lua_State* L) {
        std::string name = OptString(L, 1);
        std::string projectPath = OptString(L, 2);
        std::string clientPath = OptString(L, 3);
        std::string version = OptString(L, 4);
        const char* error = nullptr;

        if (name.empty()) {
            error = "Project must have a name.";
        } else if (clientPath.empty()) {
            error = "Game client path is empty.";
        } else if (!DirectoryExists(clientPath)) {
            error = "Game client path does not exist. \nAvoid special characters.";
        } else if (!FileExists(NativePath(clientPath) + "\\Data\\common.MPQ")) {
            error = "Game client path has no Data\\common.MPQ.\nPoint to the folder that contains Wow.exe.";
        } else if (projectPath.empty()) {
            error = "Project path is empty.";
        } else if (!DirectoryExists(projectPath)) {
            error = "Project path does not exist. \nAvoid special characters.";
        } else if (NativePath(projectPath).size() <= 3) {
            error = "Project path can't be the root of a drive.\nPoint to a folder, preferrably empty.";
        } else if (!FindProjectFile(projectPath).empty()) {
            error = "Selected already contains an existing project.";
        }

        if (error) {
            lua_pushboolean(L, false);
            lua_pushstring(L, error);
            return 2;
        }

        Project project;
        project.m_name = name;
        project.m_path = GenericPath(projectPath);
        project.m_clientPath = GenericPath(clientPath);
        project.m_clientVersion = version.empty() ? s_defaultVersion : version;

        if (!Save(project)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to write the project file.");
            return 2;
        }

        LoadSettings();
        s_settings.m_gamePath = GenericPath(clientPath);
        RegisterProjectChange(GenericPath(projectPath));
        lua_pushboolean(L, true);
        return 1;
    }

    static int32_t Script_EditorBrowseFolder(lua_State* L) {
        std::string path = BrowseFolder(OptString(L, 1), OptString(L, 2));

        if (path.empty()) {
            return 0;
        }

        lua_pushstring(L, path.c_str());
        return 1;
    }

    static int32_t Script_EditorBrowseProjectFile(lua_State* L) {
        std::string path = BrowseProjectFile();

        if (path.empty()) {
            return 0;
        }

        size_t slash = path.find_last_of('/');
        lua_pushstring(L, path.c_str());
        lua_pushstring(L, slash == std::string::npos ? "" : path.substr(0, slash).c_str());
        return 2;
    }

    static int32_t Script_EditorGetGamePath(lua_State* L) {
        LoadSettings();
        lua_pushstring(L, s_settings.m_gamePath.c_str());
        return 1;
    }

    static int32_t Script_EditorOpenProject(lua_State* L) {
        std::string path = OptString(L, 1);
        Project project;

        if (!DirectoryExists(path) || !ReadProject(path, project)) {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to read project");
            return 2;
        }

        RegisterProjectChange(GenericPath(path));

        bool projectDatabases = DirectoryExists(NativePath(path) + "\\DBFilesClient") || (!s_startupProject.empty() && DirectoryExists(NativePath(s_startupProject) + "\\DBFilesClient"));
        bool startupProject = !s_startupProject.empty() && SamePath(s_startupProject, path);
        bool otherClient = !project.m_clientPath.empty() && !SamePath(project.m_clientPath, s_clientPath);

        if (otherClient || (projectDatabases && !startupProject)) {
            if (otherClient && !FileExists(NativePath(project.m_clientPath) + "\\Data\\common.MPQ")) {
                lua_pushboolean(L, false);
                lua_pushstring(L, ("Game client path has no Data\\common.MPQ:\n" + project.m_clientPath).c_str());
                return 2;
            }

            if (Relaunch(otherClient ? project.m_clientPath : s_clientPath, path)) {
                lua_pushboolean(L, true);
                lua_pushstring(L, "relaunch");
                return 2;
            }

            lua_pushboolean(L, false);
            lua_pushstring(L, "Failed to restart the editor with the project's client.");
            return 2;
        }

        s_project = project;
        s_hasProject = true;
        SetOverridePath(path);
        lua_pushboolean(L, true);
        return 1;
    }

    static int32_t Script_EditorCloseProject(lua_State* L) {
        s_hasProject = false;
        s_project = Project();
        SetOverridePath(std::string());
        return 0;
    }

    static int32_t Script_EditorGetProject(lua_State* L) {
        if (!s_hasProject) {
            return 0;
        }

        lua_pushstring(L, s_project.m_name.c_str());
        lua_pushstring(L, s_project.m_path.c_str());
        lua_pushstring(L, s_project.m_clientPath.c_str());
        lua_pushstring(L, s_project.m_clientVersion.c_str());
        return 4;
    }

    static int32_t Script_EditorOpenDirectory(lua_State* L) {
        OpenDirectory(OptString(L, 1));
        return 0;
    }

    static int32_t Script_EditorGetMaps(lua_State* L) {
        BuildMaps();
        lua_newtable(L);
        int32_t count = 0;

        for (const MapEntry& entry : s_maps) {
            bool pinned = false;

            if (s_hasProject) {
                for (const PinnedMap& map : s_project.m_pinnedMaps) {
                    pinned = pinned || map.m_mapId == entry.m_id;
                }
            }

            lua_newtable(L);
            PushNumber(L, "id", entry.m_id);
            PushString(L, "name", entry.m_name);
            PushString(L, "directory", entry.m_directory);
            PushNumber(L, "type", entry.m_type);
            PushNumber(L, "expansion", entry.m_expansion);
            PushBool(L, "wmo", entry.m_wmo);
            PushBool(L, "pinned", pinned);
            lua_rawseti(L, -2, ++count);
        }

        return 1;
    }

    static int32_t Script_EditorPinMap(lua_State* L) {
        if (!s_hasProject || !lua_isnumber(L, 1)) {
            return 0;
        }

        int32_t mapId = static_cast<int32_t>(lua_tonumber(L, 1));
        bool pin = lua_isnone(L, 3) || lua_toboolean(L, 3);
        auto it = std::remove_if(s_project.m_pinnedMaps.begin(), s_project.m_pinnedMaps.end(), [&](const PinnedMap& map) { return map.m_mapId == mapId; });
        s_project.m_pinnedMaps.erase(it, s_project.m_pinnedMaps.end());

        if (pin) {
            s_project.m_pinnedMaps.push_back(PinnedMap { mapId, OptString(L, 2) });
        }

        Save(s_project);
        return 0;
    }

    static int32_t Script_EditorGetBookmarks(lua_State* L) {
        lua_newtable(L);

        if (!s_hasProject) {
            return 1;
        }

        int32_t count = 0;

        for (const Bookmark& bookmark : s_project.m_bookmarks) {
            lua_newtable(L);
            PushString(L, "name", bookmark.m_name);
            PushNumber(L, "mapId", bookmark.m_mapId);
            PushNumber(L, "x", bookmark.m_position[0]);
            PushNumber(L, "y", bookmark.m_position[1]);
            PushNumber(L, "z", bookmark.m_position[2]);
            PushNumber(L, "pitch", bookmark.m_pitch);
            PushNumber(L, "yaw", bookmark.m_yaw);
            lua_rawseti(L, -2, ++count);
        }

        return 1;
    }

    static std::string AreaFullName(int32_t areaId) {
        AreaTableRec* area = areaId > 0 ? g_areaTableDB.GetRecord(areaId) : nullptr;

        if (!area) {
            return "Unknown location";
        }

        std::string name = area->m_areaName ? area->m_areaName : "";
        AreaTableRec* parent = area->m_parentAreaID ? g_areaTableDB.GetRecord(area->m_parentAreaID) : nullptr;

        if (parent && parent->m_areaName) {
            name = std::string(parent->m_areaName) + ": " + name;
        }

        return name;
    }

    static int32_t Script_EditorAddBookmark(lua_State* L) {
        EditorViewport* viewport = EditorViewport::s_current;

        if (!s_hasProject || !viewport || !EditorApp::IsMapLoaded()) {
            return 0;
        }

        Bookmark bookmark;
        bookmark.m_mapId = EditorApp::GetMapId();
        bookmark.m_position[0] = s_zeroPoint - viewport->m_position.y;
        bookmark.m_position[1] = viewport->m_position.z;
        bookmark.m_position[2] = s_zeroPoint - viewport->m_position.x;
        bookmark.m_pitch = -viewport->m_pitch * 57.29578f;
        float yaw = viewport->m_yaw * 57.29578f - 180.0f;

        while (yaw >= 180.0f) {
            yaw -= 360.0f;
        }

        while (yaw < -180.0f) {
            yaw += 360.0f;
        }

        bookmark.m_yaw = yaw;
        bookmark.m_name = AreaFullName(TerrainEdit::GetAreaId(viewport->m_position.x, viewport->m_position.y));
        s_project.m_bookmarks.push_back(bookmark);
        Save(s_project);
        lua_pushstring(L, bookmark.m_name.c_str());
        return 1;
    }

    static int32_t Script_EditorGetMinimap(lua_State* L) {
        if (!lua_isnumber(L, 1)) {
            return 0;
        }

        const MapEntry* map = FindMap(static_cast<int32_t>(lua_tonumber(L, 1)));
        int32_t tileSize = lua_isnumber(L, 2) ? static_cast<int32_t>(lua_tonumber(L, 2)) : 16;
        tileSize = std::max(1, std::min(tileSize, 16));
        std::string path;
        std::string tiles;
        int32_t textureSize = 0;

        if (!map || !BuildMinimap(*map, tileSize, path, textureSize, tiles)) {
            return 0;
        }

        lua_pushstring(L, path.c_str());
        lua_pushnumber(L, textureSize);
        lua_pushstring(L, tiles.c_str());
        lua_pushboolean(L, map->m_wmo);
        return 4;
    }

    static int32_t Script_EditorEnterMap(lua_State* L) {
        if (!lua_isnumber(L, 1)) {
            return 0;
        }

        const MapEntry* map = FindMap(static_cast<int32_t>(lua_tonumber(L, 1)));

        if (!map) {
            return 0;
        }

        float noggitPos[3];
        float pitch = 30.0f;
        float yaw = 90.0f;

        if (map->m_wmo) {
            WdtInfo wdt;
            ReadWdt(map->m_directory, wdt);
            float dx = wdt.m_wmoExtents[0] - wdt.m_wmoExtents[3];
            float dy = wdt.m_wmoExtents[2] - wdt.m_wmoExtents[5];
            float dz = wdt.m_wmoExtents[1] - wdt.m_wmoExtents[4] * 2.0f;
            yaw = atan2f(dx, dy) * 57.29578f;
            float distance = sqrtf(dx * dx + dy * dy + dz * dz);
            pitch = distance > 0.0f ? -asinf(dz / distance) * 57.29578f : 0.0f;
            C3Vector position(-wdt.m_wmoPos[2], -wdt.m_wmoPos[0], wdt.m_wmoPos[1]);
            lua_pushboolean(L, EditorApp::LoadMap(map->m_id, position, (yaw + 180.0f) * 0.017453292f, -pitch * 0.017453292f));
            return 1;
        } else {
            int32_t tileX = static_cast<int32_t>(lua_tonumber(L, 2));
            int32_t tileY = static_cast<int32_t>(lua_tonumber(L, 3));
            noggitPos[0] = tileX * s_tileSize + s_tileSize / 2.0f;
            noggitPos[1] = HeightAtTile(*map, tileX, tileY);
            noggitPos[2] = tileY * s_tileSize + s_tileSize / 2.0f;
        }

        lua_pushboolean(L, EnterMapAt(map->m_id, noggitPos, pitch, yaw));
        return 1;
    }

    static int32_t Script_EditorEnterBookmark(lua_State* L) {
        int32_t index = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;

        if (!s_hasProject || index < 0 || index >= static_cast<int32_t>(s_project.m_bookmarks.size())) {
            return 0;
        }

        const Bookmark& bookmark = s_project.m_bookmarks[index];
        lua_pushboolean(L, EnterMapAt(bookmark.m_mapId, bookmark.m_position, bookmark.m_pitch, bookmark.m_yaw));
        return 1;
    }

    static int32_t Script_EditorUnloadMap(lua_State* L) {
        EditorApp::UnloadMap();
        return 0;
    }

    static int32_t Script_EditorIsMapLoaded(lua_State* L) {
        lua_pushboolean(L, EditorApp::IsMapLoaded());
        return 1;
    }

    static int32_t Script_EditorGetMapComboSearch(lua_State* L) {
        LoadSettings();
        lua_pushnumber(L, s_settings.m_mapComboSearch);
        return 1;
    }

    static int32_t Script_EditorSetMapComboSearch(lua_State* L) {
        LoadSettings();
        s_settings.m_mapComboSearch = static_cast<int32_t>(lua_tonumber(L, 1));
        SaveSettings();
        return 0;
    }

    static int32_t Script_EditorSaveTiles(lua_State* L) {
        std::string mode = OptString(L, 1);
        EditorSave::MODE saveMode = mode == "all" ? EditorSave::SAVE_ALL : (mode == "current" ? EditorSave::SAVE_CURRENT : EditorSave::SAVE_CHANGED);
        std::string error;
        int32_t saved = EditorSave::Save(saveMode, s_hasProject ? NativePath(s_project.m_path) : std::string(), error);

        if (saved < 0) {
            lua_pushnil(L);
            lua_pushstring(L, error.c_str());
            return 2;
        }

        lua_pushnumber(L, saved);
        return 1;
    }

    static int32_t Script_EditorGetCursorPosition(lua_State* L) {
        CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_theGxDevicePtr->CapsWindowSize(window);
        float height = window.maxY - window.minY;
        int32_t x = 0;
        int32_t y = 0;
        OsInputGetMousePosition(&x, &y);
        float scale = height > 0.0f ? 768.0f / height : 1.0f;
        lua_pushnumber(L, x * scale);
        lua_pushnumber(L, (height - y) * scale);
        return 2;
    }

    static int32_t Script_EditorGetTime(lua_State* L) {
        lua_pushnumber(L, OsGetAsyncTimeMs() / 1000.0);
        return 1;
    }

    static int32_t Script_EditorGetDate(lua_State* L) {
        time_t now = time(nullptr);
        struct tm local;
        localtime_s(&local, &now);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%a %b %d %Y", &local);
        lua_pushstring(L, buffer);
        return 1;
    }

    static FrameScript_Method s_projectFunctions[] = {
        { "EditorGetRecentProjects", &Script_EditorGetRecentProjects },
        { "EditorSetFavoriteProject", &Script_EditorSetFavoriteProject },
        { "EditorForgetProject", &Script_EditorForgetProject },
        { "EditorGetStartupProject", &Script_EditorGetStartupProject },
        { "EditorCreateProject", &Script_EditorCreateProject },
        { "EditorBrowseFolder", &Script_EditorBrowseFolder },
        { "EditorBrowseProjectFile", &Script_EditorBrowseProjectFile },
        { "EditorGetGamePath", &Script_EditorGetGamePath },
        { "EditorOpenProject", &Script_EditorOpenProject },
        { "EditorCloseProject", &Script_EditorCloseProject },
        { "EditorGetProject", &Script_EditorGetProject },
        { "EditorOpenDirectory", &Script_EditorOpenDirectory },
        { "EditorGetMaps", &Script_EditorGetMaps },
        { "EditorPinMap", &Script_EditorPinMap },
        { "EditorGetBookmarks", &Script_EditorGetBookmarks },
        { "EditorAddBookmark", &Script_EditorAddBookmark },
        { "EditorGetMinimap", &Script_EditorGetMinimap },
        { "EditorEnterMap", &Script_EditorEnterMap },
        { "EditorEnterBookmark", &Script_EditorEnterBookmark },
        { "EditorUnloadMap", &Script_EditorUnloadMap },
        { "EditorIsMapLoaded", &Script_EditorIsMapLoaded },
        { "EditorGetMapComboSearch", &Script_EditorGetMapComboSearch },
        { "EditorSetMapComboSearch", &Script_EditorSetMapComboSearch },
        { "EditorSaveTiles", &Script_EditorSaveTiles },
        { "EditorGetCursorPosition", &Script_EditorGetCursorPosition },
        { "EditorGetTime", &Script_EditorGetTime },
        { "EditorGetDate", &Script_EditorGetDate },
    };

    void RegisterScriptFunctions() {
        for (uint32_t i = 0; i < sizeof(s_projectFunctions) / sizeof(s_projectFunctions[0]); i++) {
            FrameScript_RegisterFunction(s_projectFunctions[i].m_name, s_projectFunctions[i].m_method);
        }
    }
}
