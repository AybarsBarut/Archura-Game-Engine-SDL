#pragma once

#include "SerializationJson.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

namespace Archura {

// A portable package is rooted at its manifest, never the launching shell's CWD.
struct GameManifest {
    std::filesystem::path root;
    std::string title;
    std::string scene;
    std::string playerID;
    std::vector<std::string> content;

    static bool IsPackagePath(const std::string& value) {
        if (value.empty() || value.size() > 240 || value.front() == '/' ||
            value.find_first_of("\\:\"<>|?*") != std::string::npos) return false;
        for (unsigned char c : value) if (c < 32 || c == 127) return false;
        for (const auto& part : std::filesystem::path(value)) {
            auto name = part.generic_string();
            if (name.empty() || name.back() == '.' || name.back() == ' ') return false;
            name = name.substr(0, name.find('.'));
            for (char& c : name) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
            if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL" ||
                name == "CONIN$" || name == "CONOUT$" ||
                (name.size() == 4 && (name.substr(0, 3) == "COM" || name.substr(0, 3) == "LPT") &&
                 name[3] >= '1' && name[3] <= '9')) return false;
        }
        return value.find("//") == std::string::npos && value.back() != '/';
    }

    static bool Parse(std::string_view text, GameManifest& destination) {
        using namespace Serialization;
        if (text.size() > 65536) return false;
        Json json;
        if (!JsonParser(text).Parse(json) || json.kind != Json::Kind::Object) return false;
        const std::set<std::string> keys{"version", "title", "scene", "player_id", "content"};
        for (const auto& entry : json.object) if (!keys.count(entry.first)) return false;
        int version = 0;
        GameManifest parsed;
        if (!IntField(json, "version", version) || version != 1 ||
            !StringField(json, "title", parsed.title, true) || parsed.title.empty() || parsed.title.size() > 128 ||
            !StringField(json, "scene", parsed.scene, true) || !IsPackagePath(parsed.scene) ||
            !StringField(json, "player_id", parsed.playerID, true) || parsed.playerID.size() != 32) return false;
        bool nonzero = false;
        for (char c : parsed.playerID) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
            nonzero |= c != '0';
        }
        if (!nonzero) return false;
        for (unsigned char c : parsed.title) if (c < 32 || c == 127) return false;
        const auto* files = json.Find("content");
        if (!files || files->kind != Json::Kind::Array || files->array.empty() || files->array.size() > 1024) return false;
        std::set<std::string> names;
        for (const auto& file : files->array) {
            if (file.kind != Json::Kind::String || !IsPackagePath(file.string)) return false;
            std::string key = file.string;
            for (char& c : key) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (!names.insert(key).second) return false;
            parsed.content.push_back(file.string);
        }
        if (std::find(parsed.content.begin(), parsed.content.end(), parsed.scene) == parsed.content.end()) return false;
        destination = std::move(parsed);
        return true;
    }

    static bool Load(const std::filesystem::path& path, GameManifest& destination, std::string& error) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size == 0 || size > 65536) { error = "Missing or oversized game manifest"; return false; }
        std::ifstream input(path, std::ios::binary);
        std::string text(static_cast<size_t>(size), '\0');
        GameManifest parsed;
        if (!input.read(text.data(), static_cast<std::streamsize>(size)) || !Parse(text, parsed)) {
            error = "Invalid game manifest (version 1 required)"; return false;
        }
        parsed.root = std::filesystem::canonical(path, ec).parent_path();
        if (ec) { error = "Cannot resolve game root"; return false; }
        for (const auto& file : parsed.content) {
            const auto full = std::filesystem::canonical(parsed.root / file, ec);
            if (ec || !std::filesystem::is_regular_file(full, ec)) {
                error = "Missing game content: " + file; return false;
            }
            const auto relative = full.lexically_relative(parsed.root).generic_string();
            if (!IsPackagePath(relative)) { error = "Content escapes game root: " + file; return false; }
        }
        destination = std::move(parsed);
        error.clear();
        return true;
    }
};

} // namespace Archura
