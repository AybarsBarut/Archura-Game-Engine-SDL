#pragma once

#include <string>
#include <cstdint>

namespace Archura {

/**
 * @brief Server configuration structure
 * 
 * Loaded from server_config.json or command line arguments
 */
struct ServerConfig {
    // Network settings
    std::string serverName = "Archura Server";
    int port = 27015;
    int maxPlayers = 16;
    
    // Performance settings
    int tickRate = 128;                    // Server update rate (Hz)
    int snapshotRate = 64;                 // Client update rate (Hz)
    
    // Game settings
    std::string map = "dm_arena";
    std::string gameMode = "deathmatch";
    std::string password = "";             // Empty = no password
    
    // Timeouts
    float clientTimeout = 30.0f;           // Seconds before kicking inactive client
    float connectionTimeout = 10.0f;       // Seconds for initial connection
    
    // Admin
    std::string rconPassword = "";         // Remote console password
    bool enableRcon = false;
    
    // Logging
    bool verboseLogging = false;
    std::string logFile = "server.log";
    
    // Default constructor
    ServerConfig() = default;
    
    /**
     * @brief Load configuration from JSON file
     * @param filepath Path to config file
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& filepath);
    
    /**
     * @brief Save configuration to JSON file
     * @param filepath Path to save config
     * @return true if saved successfully
     */
    bool SaveToFile(const std::string& filepath) const;
    
    /**
     * @brief Parse command line arguments
     * @param argc Argument count
     * @param argv Argument values
     */
    void ParseCommandLine(int argc, char** argv);
    
    /**
     * @brief Validate configuration values
     * @return true if all values are valid
     */
    bool Validate() const;
    
    /**
     * @brief Get tick interval in seconds
     */
    float GetTickInterval() const { return 1.0f / static_cast<float>(tickRate); }
    
    /**
     * @brief Get snapshot interval in seconds
     */
    float GetSnapshotInterval() const { return 1.0f / static_cast<float>(snapshotRate); }
};

} // namespace Archura
