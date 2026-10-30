#include "ServerConfig.h"
#include "../core/SerializationJson.h"
#include "../core/AtomicFile.h"
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace {
constexpr std::streamsize MaxConfigBytes = 1024 * 1024;

bool ValueOption(const std::string& arg) {
    return arg == "--config" || arg == "--port" || arg == "--tickrate" ||
           arg == "--maxplayers" || arg == "--map" || arg == "--name" || arg == "--password";
}

bool ParseInteger(const char* text, int& value) {
    const auto end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool CliError(const std::string& option) {
    std::cerr << "Invalid server command line option or value: " << option << '\n';
    return false;
}

void PrintUsage() {
    std::cout << "Archura Dedicated Server\nUsage: ArchuraServer [options]\n\n"
              << "  --config <file>      Load configuration from JSON file\n"
              << "  --port <port>        Server port (default: 27015)\n"
              << "  --tickrate <rate>    Server tickrate (default: 128)\n"
              << "  --maxplayers <num>   Maximum players (default: 16)\n"
              << "  --map <name>         Map name (default: dm_arena)\n"
              << "  --name <name>        Server name\n"
              << "  --password <pass>    Server password\n"
              << "  --verbose            Enable verbose logging\n"
              << "  --help, -h           Show this help message\n";
}
} // namespace

namespace Archura {
bool ServerConfig::LoadFromFile(const std::string& filepath) {
    if (filepath.empty() || filepath.find('\0') != std::string::npos) return false;
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0 || size > MaxConfigBytes) return false;
    file.seekg(0, std::ios::beg);
    std::string text(static_cast<size_t>(size), '\0');
    if (!file.read(text.data(), static_cast<std::streamsize>(size))) return false;
    Serialization::Json json;
    if (!Serialization::JsonParser(text).Parse(json) || json.kind != Serialization::Json::Kind::Object)
        return false;

    // Optional and unknown fields retain their legacy behavior. Known fields
    // require their actual JSON type; no mutation is published before validation.
    ServerConfig candidate = *this;
    using namespace Serialization;
    if (!StringField(json, "server_name", candidate.serverName) ||
        !IntField(json, "port", candidate.port) ||
        !IntField(json, "max_players", candidate.maxPlayers) ||
        !IntField(json, "tickrate", candidate.tickRate) ||
        !IntField(json, "snapshot_rate", candidate.snapshotRate) ||
        !StringField(json, "map", candidate.map) ||
        !StringField(json, "game_mode", candidate.gameMode) ||
        !StringField(json, "password", candidate.password) ||
        !FloatField(json, "client_timeout", candidate.clientTimeout) ||
        !FloatField(json, "connection_timeout", candidate.connectionTimeout) ||
        !StringField(json, "rcon_password", candidate.rconPassword) ||
        !BoolField(json, "enable_rcon", candidate.enableRcon) ||
        !BoolField(json, "verbose_logging", candidate.verboseLogging) ||
        !StringField(json, "log_file", candidate.logFile) || !candidate.Validate()) return false;
    *this = std::move(candidate);
    return true;
}

bool ServerConfig::SaveToFile(const std::string& filepath) const {
    if (!Validate()) return false;
    std::ostringstream file;
    file.imbue(std::locale::classic());
    file << std::setprecision(std::numeric_limits<float>::max_digits10);
    using Serialization::Escape;
    file << "{\n";
    file << "  \"server_name\": \"" << Escape(serverName) << "\",\n";
    file << "  \"port\": " << port << ",\n";
    file << "  \"max_players\": " << maxPlayers << ",\n";
    file << "  \"tickrate\": " << tickRate << ",\n";
    file << "  \"snapshot_rate\": " << snapshotRate << ",\n";
    file << "  \"map\": \"" << Escape(map) << "\",\n";
    file << "  \"game_mode\": \"" << Escape(gameMode) << "\",\n";
    file << "  \"password\": \"" << Escape(password) << "\",\n";
    file << "  \"client_timeout\": " << clientTimeout << ",\n";
    file << "  \"connection_timeout\": " << connectionTimeout << ",\n";
    file << "  \"rcon_password\": \"" << Escape(rconPassword) << "\",\n";
    file << "  \"enable_rcon\": " << (enableRcon ? "true" : "false") << ",\n";
    file << "  \"verbose_logging\": " << (verboseLogging ? "true" : "false") << ",\n";
    file << "  \"log_file\": \"" << Escape(logFile) << "\"\n}\n";
    const auto text = file.str();
    Serialization::Json validation;
    if (!file || text.size() > static_cast<size_t>(MaxConfigBytes) ||
        !Serialization::JsonParser(text).Parse(validation)) return false;
    return Serialization::AtomicWrite(filepath, text);
}

void ServerConfig::ParseCommandLine(int argc, char** argv) {
    (void)TryParseCommandLine(argc, argv);
}

bool ServerConfig::TryParseCommandLine(int argc, char** argv) {
    if (argc < 1 || !argv) return CliError("arguments");
    ServerConfig candidate = *this;
    candidate.helpRequested = false;
    std::string configPath;
    bool hasConfig = false;
    // Consume operands during discovery so a string value such as "--config"
    // is not accidentally interpreted as a separate option.
    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) return CliError("arguments");
        const std::string arg = argv[i];
        if (ValueOption(arg)) {
            if (i + 1 >= argc || !argv[i + 1]) return CliError(arg);
            const char* value = argv[++i];
            if (arg == "--config") {
                if (hasConfig) return CliError(arg);
                hasConfig = true;
                configPath = value;
            }
        } else if (arg != "--verbose" && arg != "--help" && arg != "-h") {
            return CliError(arg);
        }
    }
    if (hasConfig && !candidate.LoadFromFile(configPath)) {
        std::cerr << "Failed to load server config\n";
        return false;
    }
    // Explicit CLI values consistently win regardless of --config position.
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--verbose") candidate.verboseLogging = true;
        else if (arg == "--help" || arg == "-h") candidate.helpRequested = true;
        else {
            const char* value = argv[++i];
            if (arg == "--port") {
                if (!ParseInteger(value, candidate.port)) return CliError(arg);
            } else if (arg == "--tickrate") {
                if (!ParseInteger(value, candidate.tickRate)) return CliError(arg);
            } else if (arg == "--maxplayers") {
                if (!ParseInteger(value, candidate.maxPlayers)) return CliError(arg);
            } else if (arg == "--map") candidate.map = value;
            else if (arg == "--name") candidate.serverName = value;
            else if (arg == "--password") candidate.password = value;
        }
    }
    if (!candidate.Validate()) return false;
    *this = std::move(candidate);
    if (helpRequested) PrintUsage();
    return true;
}

bool ServerConfig::Validate() const {
    if (port < 1024 || port > 65535) {
        std::cerr << "Invalid port (must be 1024-65535)\n";
        return false;
    }
    if (tickRate < 20 || tickRate > 256) {
        std::cerr << "Invalid tickrate (must be 20-256)\n";
        return false;
    }
    if (snapshotRate < 10 || snapshotRate > tickRate) {
        std::cerr << "Invalid snapshot rate (must be 10-tickrate)\n";
        return false;
    }
    if (maxPlayers < 1 || maxPlayers > 64) {
        std::cerr << "Invalid max players (must be 1-64)\n";
        return false;
    }
    if (map.empty()) {
        std::cerr << "Map name cannot be empty\n";
        return false;
    }
    if (!std::isfinite(connectionTimeout) || connectionTimeout < 0.1f || connectionTimeout > 120.0f ||
        !std::isfinite(clientTimeout) || clientTimeout < connectionTimeout || clientTimeout > 600.0f) {
        std::cerr << "Invalid timeouts (connection: 0.1-120s; client: connection-600s)\n";
        return false;
    }
    return true;
}
} // namespace Archura
