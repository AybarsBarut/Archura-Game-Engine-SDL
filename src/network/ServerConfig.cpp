#include "ServerConfig.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <filesystem>

namespace {

std::string EscapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '"') escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else escaped += c;
    }
    return escaped;
}

bool ExtractField(const std::string& json, const std::string& key,
                  std::string& value) {
    const std::string quoted = "\"" + key + "\"";
    const size_t keyPos = json.find(quoted);
    if (keyPos == std::string::npos) return false;
    size_t pos = json.find(':', keyPos + quoted.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return false;
    if (json[pos] == '"') {
        ++pos;
        value.clear();
        while (pos < json.size()) {
            const char c = json[pos++];
            if (c == '"') return true;
            if (c == '\\' && pos < json.size()) value += json[pos++];
            else value += c;
        }
        return false;
    }
    const size_t end = json.find_first_of(",}\n\r", pos);
    value = json.substr(pos, end == std::string::npos ? end : end - pos);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
    return !value.empty();
}

template <typename T, typename ParseFn>
bool ReadValue(const std::string& json, const std::string& key,
               T& target, ParseFn parse) {
    std::string raw;
    if (!ExtractField(json, key, raw)) return true; // optional field
    return parse(raw, target);
}

} // namespace

namespace Archura {

bool ServerConfig::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0 || size > 1024 * 1024) return false;
    std::string json(static_cast<size_t>(size), '\0');
    if (!file.read(json.data(), size)) return false;
    const size_t first = json.find_first_not_of(" \t\r\n");
    const size_t last = json.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || json[first] != '{' ||
        last == std::string::npos || json[last] != '}') return false;

    bool ok = true;
    ok &= ReadValue(json, "server_name", serverName,
                    [](const std::string& v, std::string& out) { out = v; return true; });
    ok &= ReadValue(json, "port", port, [](const std::string& v, int& out) {
        try { size_t n = 0; out = std::stoi(v, &n); return n == v.size(); }
        catch (...) { return false; }
    });
    ok &= ReadValue(json, "max_players", maxPlayers, [](const std::string& v, int& out) {
        try { size_t n = 0; out = std::stoi(v, &n); return n == v.size(); }
        catch (...) { return false; }
    });
    ok &= ReadValue(json, "tickrate", tickRate, [](const std::string& v, int& out) {
        try { size_t n = 0; out = std::stoi(v, &n); return n == v.size(); }
        catch (...) { return false; }
    });
    ok &= ReadValue(json, "snapshot_rate", snapshotRate, [](const std::string& v, int& out) {
        try { size_t n = 0; out = std::stoi(v, &n); return n == v.size(); }
        catch (...) { return false; }
    });
    ok &= ReadValue(json, "map", map,
                    [](const std::string& v, std::string& out) { out = v; return true; });
    ok &= ReadValue(json, "game_mode", gameMode,
                    [](const std::string& v, std::string& out) { out = v; return true; });
    ok &= ReadValue(json, "password", password,
                    [](const std::string& v, std::string& out) { out = v; return true; });
    ok &= ReadValue(json, "client_timeout", clientTimeout, [](const std::string& v, float& out) {
        try { size_t n = 0; out = std::stof(v, &n); return n == v.size() && std::isfinite(out); }
        catch (...) { return false; }
    });
    ok &= ReadValue(json, "connection_timeout", connectionTimeout, [](const std::string& v, float& out) {
        try { size_t n = 0; out = std::stof(v, &n); return n == v.size() && std::isfinite(out); }
        catch (...) { return false; }
    });
    ok &= ReadValue(json, "rcon_password", rconPassword,
                    [](const std::string& v, std::string& out) { out = v; return true; });
    ok &= ReadValue(json, "enable_rcon", enableRcon, [](const std::string& v, bool& out) {
        if (v == "true" || v == "1") { out = true; return true; }
        if (v == "false" || v == "0") { out = false; return true; }
        return false;
    });
    ok &= ReadValue(json, "verbose_logging", verboseLogging, [](const std::string& v, bool& out) {
        if (v == "true" || v == "1") { out = true; return true; }
        if (v == "false" || v == "0") { out = false; return true; }
        return false;
    });
    ok &= ReadValue(json, "log_file", logFile,
                    [](const std::string& v, std::string& out) { out = v; return true; });
    return ok && Validate();
}

bool ServerConfig::SaveToFile(const std::string& filepath) const {
    const std::filesystem::path path(filepath);
    if (!path.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return false;
    }
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file for writing: " << filepath << std::endl;
        return false;
    }
    
    // Write JSON format
    file << "{\n";
    file << "  \"server_name\": \"" << EscapeJson(serverName) << "\",\n";
    file << "  \"port\": " << port << ",\n";
    file << "  \"max_players\": " << maxPlayers << ",\n";
    file << "  \"tickrate\": " << tickRate << ",\n";
    file << "  \"snapshot_rate\": " << snapshotRate << ",\n";
    file << "  \"map\": \"" << EscapeJson(map) << "\",\n";
    file << "  \"game_mode\": \"" << EscapeJson(gameMode) << "\",\n";
    file << "  \"password\": \"" << EscapeJson(password) << "\",\n";
    file << "  \"client_timeout\": " << clientTimeout << ",\n";
    file << "  \"connection_timeout\": " << connectionTimeout << ",\n";
    file << "  \"rcon_password\": \"" << EscapeJson(rconPassword) << "\",\n";
    file << "  \"enable_rcon\": " << (enableRcon ? "true" : "false") << ",\n";
    file << "  \"verbose_logging\": " << (verboseLogging ? "true" : "false") << ",\n";
    file << "  \"log_file\": \"" << EscapeJson(logFile) << "\"\n";
    file << "}\n";
    
    file.close();
    std::cout << "Server config saved to: " << filepath << std::endl;
    return true;
}

void ServerConfig::ParseCommandLine(int argc, char** argv) {
    helpRequested = false;
    // Load the file first; explicit command-line values then consistently win
    // regardless of argument ordering.
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--config") {
            if (!LoadFromFile(argv[i + 1]))
                std::cerr << "Failed to load server config: " << argv[i + 1] << '\n';
            break;
        }
    }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        }
        else if (arg == "--tickrate" && i + 1 < argc) {
            tickRate = std::atoi(argv[++i]);
        }
        else if (arg == "--maxplayers" && i + 1 < argc) {
            maxPlayers = std::atoi(argv[++i]);
        }
        else if (arg == "--map" && i + 1 < argc) {
            map = argv[++i];
        }
        else if (arg == "--name" && i + 1 < argc) {
            serverName = argv[++i];
        }
        else if (arg == "--password" && i + 1 < argc) {
            password = argv[++i];
        }
        else if (arg == "--config" && i + 1 < argc) {
            ++i; // already loaded in the first pass
        }
        else if (arg == "--verbose") {
            verboseLogging = true;
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Archura Dedicated Server\n";
            std::cout << "Usage: ArchuraServer [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --config <file>      Load configuration from JSON file\n";
            std::cout << "  --port <port>        Server port (default: 27015)\n";
            std::cout << "  --tickrate <rate>    Server tickrate (default: 128)\n";
            std::cout << "  --maxplayers <num>   Maximum players (default: 16)\n";
            std::cout << "  --map <name>         Map name (default: dm_arena)\n";
            std::cout << "  --name <name>        Server name\n";
            std::cout << "  --password <pass>    Server password\n";
            std::cout << "  --verbose            Enable verbose logging\n";
            std::cout << "  --help, -h           Show this help message\n";
            helpRequested = true;
        }
    }
}

bool ServerConfig::Validate() const {
    if (port < 1024 || port > 65535) {
        std::cerr << "Invalid port: " << port << " (must be 1024-65535)\n";
        return false;
    }
    
    if (tickRate < 20 || tickRate > 256) {
        std::cerr << "Invalid tickrate: " << tickRate << " (must be 20-256)\n";
        return false;
    }
    
    if (snapshotRate < 10 || snapshotRate > tickRate) {
        std::cerr << "Invalid snapshot rate: " << snapshotRate << " (must be 10-" << tickRate << ")\n";
        return false;
    }
    
    if (maxPlayers < 1 || maxPlayers > 64) {
        std::cerr << "Invalid max players: " << maxPlayers << " (must be 1-64)\n";
        return false;
    }
    
    if (map.empty()) {
        std::cerr << "Map name cannot be empty\n";
        return false;
    }
    
    return true;
}

} // namespace Archura
