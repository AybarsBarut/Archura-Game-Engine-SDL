#include "network/ServerConfig.h"
#include <iostream>
#include <cstdio>
#include <thread>
#include <chrono>

/**
 * @brief Archura Dedicated Server Entry Point
 * 
 * Headless server executable without graphics/audio
 * Runs game logic at 128 tickrate and sends snapshots to clients
 */

namespace Archura {
    // Forward declarations
    class ServerGameState;
    class NetworkManager;
}

using namespace Archura;

// Global server state
static bool g_ServerRunning = true;
static ServerConfig g_Config;

void PrintBanner() {
    std::cout << "========================================\n";
    std::cout << "  Archura Dedicated Server v1.0.0\n";
    std::cout << "========================================\n";
    std::cout << "\n";
}

void PrintServerInfo() {
    std::cout << "Server Configuration:\n";
    std::cout << "  Name: " << g_Config.serverName << "\n";
    std::cout << "  Port: " << g_Config.port << "\n";
    std::cout << "  Map: " << g_Config.map << "\n";
    std::cout << "  Max Players: " << g_Config.maxPlayers << "\n";
    std::cout << "  Tickrate: " << g_Config.tickRate << " Hz\n";
    std::cout << "  Snapshot Rate: " << g_Config.snapshotRate << " Hz\n";
    std::cout << "\n";
}

void ServerTick(float deltaTime) {
    // TODO: Implement server game logic
    // - Process client inputs
    // - Update physics
    // - Check collisions
    // - Generate snapshots
    // - Send to clients
}

void RunServerLoop() {
    const float tickInterval = g_Config.GetTickInterval();
    const int maxTicksPerFrame = 5; // Prevent spiral of death
    
    auto lastTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;
    
    int tickCount = 0;
    auto lastSecond = std::chrono::high_resolution_clock::now();
    
    std::cout << "Server started! Tick interval: " << (tickInterval * 1000.0f) << "ms\n";
    std::cout << "Press Ctrl+C to stop server\n\n";
    
    while (g_ServerRunning) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        accumulator += frameTime;
        
        // Fixed timestep loop
        int ticks = 0;
        while (accumulator >= tickInterval && ticks < maxTicksPerFrame) {
            ServerTick(tickInterval);
            accumulator -= tickInterval;
            ticks++;
            tickCount++;
        }
        
        // Print tickrate every second
        auto timeSinceLastSecond = std::chrono::duration<float>(currentTime - lastSecond).count();
        if (timeSinceLastSecond >= 1.0f) {
            if (g_Config.verboseLogging) {
                std::cout << "Tickrate: " << tickCount << " ticks/sec\n";
            }
            tickCount = 0;
            lastSecond = currentTime;
        }
        
        // Sleep to prevent CPU spinning
        // Calculate remaining time in this frame
        auto sleepTime = tickInterval - std::chrono::duration<float>(
            std::chrono::high_resolution_clock::now() - currentTime).count();
        
        if (sleepTime > 0.0f) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<int>(sleepTime * 1000000.0f * 0.9f))
            );
        }
    }
    
    std::cout << "\nServer shutting down...\n";
}

int main(int argc, char** argv) {
    PrintBanner();
    
    // Parse command line arguments
    g_Config.ParseCommandLine(argc, argv);
    
    // Validate configuration
    if (!g_Config.Validate()) {
        std::cerr << "Invalid server configuration!\n";
        return 1;
    }
    
    PrintServerInfo();
    
    // TODO: Initialize server systems
    // - NetworkManager
    // - ServerGameState
    // - Physics
    // - etc.
    
    std::cout << "Initializing server systems...\n";
    
    // TODO: Load map
    std::cout << "Loading map: " << g_Config.map << "\n";
    
    // TODO: Start network listener
    std::cout << "Starting network listener on port " << g_Config.port << "...\n";
    
    // Run server loop
    RunServerLoop();
    
    // TODO: Cleanup
    std::cout << "Cleanup complete.\n";
    
    return 0;
}
