#include "ServerConfig.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>

namespace Archura {

bool ServerConfig::LoadFromFile(const std::string& filepath) {
    // TODO: Implement JSON parsing
    // For now, just return true with defaults
    std::cout << "Loading server config from: " << filepath << std::endl;
    std::cout << "Note: JSON parsing not yet implemented, using defaults" << std::endl;
    return true;
}

bool ServerConfig::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file for writing: " << filepath << std::endl;
        return false;
    }
    
    // Write JSON format
    file << "{\n";
    file << "  \"server_name\": \"" << serverName << "\",\n";
    file << "  \"port\": " << port << ",\n";
    file << "  \"max_players\": " << maxPlayers << ",\n";
    file << "  \"tickrate\": " << tickRate << ",\n";
    file << "  \"snapshot_rate\": " << snapshotRate << ",\n";
    file << "  \"map\": \"" << map << "\",\n";
    file << "  \"game_mode\": \"" << gameMode << "\",\n";
    file << "  \"password\": \"" << password << "\",\n";
    file << "  \"client_timeout\": " << clientTimeout << ",\n";
    file << "  \"connection_timeout\": " << connectionTimeout << ",\n";
    file << "  \"rcon_password\": \"" << rconPassword << "\",\n";
    file << "  \"enable_rcon\": " << (enableRcon ? "true" : "false") << ",\n";
    file << "  \"verbose_logging\": " << (verboseLogging ? "true" : "false") << ",\n";
    file << "  \"log_file\": \"" << logFile << "\"\n";
    file << "}\n";
    
    file.close();
    std::cout << "Server config saved to: " << filepath << std::endl;
    return true;
}

void ServerConfig::ParseCommandLine(int argc, char** argv) {
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
            LoadFromFile(argv[++i]);
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
