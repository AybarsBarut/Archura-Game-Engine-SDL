#include "network/ServerConfig.h"
#include "network/NetworkManager.h"
#include "network/AuthoritativeWorld.h"
#include <algorithm>
#include <csignal>
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

using namespace Archura;

// Global server state
static volatile std::sig_atomic_t g_ServerRunning = 1;
static ServerConfig g_Config;
static Net::AuthoritativeWorld g_World;
static std::vector<std::pair<std::uint32_t, Net::PlayerInputPacket>> g_Inputs;
static float g_SnapshotAccumulator = 0;

void HandleTerminationSignal(int) noexcept {
    g_ServerRunning = 0;
}

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
    auto& network = NetworkManager::Get();
    g_Inputs.clear();
    network.UpdateServer();
    if (network.GetState() != ConnectionState::Listening) {
        std::cerr << "Network listener failed: " << network.GetLastError() << std::endl;
        g_ServerRunning = 0;
        return;
    }
    if (!g_World.SynchronizePeers(network.GetConnectedPeerIds())) {
        g_ServerRunning = 0;
        return;
    }
    for (const auto& command : g_Inputs) {
        if (!g_World.AcceptInput(command.first, command.second)) {
            network.DisconnectPeer(command.first);
        }
    }
    g_World.Tick();
    g_SnapshotAccumulator += deltaTime;
    if (g_SnapshotAccumulator >= g_Config.GetSnapshotInterval()) {
        g_SnapshotAccumulator -= g_Config.GetSnapshotInterval();
        network.SendWorldState(Net::EncodeWorldSnapshot(g_World.Snapshot()));
    }
}

void RunServerLoop() {
    const float tickInterval = g_Config.GetTickInterval();
    const int maxTicksPerFrame = 5; // Prevent spiral of death
    
    auto lastTime = std::chrono::steady_clock::now();
    float accumulator = 0.0f;
    
    int tickCount = 0;
    auto lastSecond = std::chrono::steady_clock::now();
    
    std::cout << "Server started! Tick interval: " << (tickInterval * 1000.0f) << "ms\n";
    std::cout << "Press Ctrl+C to stop server\n\n";
    
    while (g_ServerRunning != 0) {
        auto currentTime = std::chrono::steady_clock::now();
        float frameTime = std::min(0.25f,
            std::chrono::duration<float>(currentTime - lastTime).count());
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
            std::chrono::steady_clock::now() - currentTime).count();
        
        if (sleepTime > 0.0f) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(static_cast<int>(sleepTime * 1000000.0f * 0.9f))
            );
        }
    }
    
    std::cout << "\nServer shutting down...\n";
}

int main(int argc, char** argv) {
    std::signal(SIGINT, HandleTerminationSignal);
    std::signal(SIGTERM, HandleTerminationSignal);
    PrintBanner();
    
    // Parse command line arguments
    g_Config.ParseCommandLine(argc, argv);
    if (g_Config.helpRequested) return 0;
    
    // Validate configuration
    if (!g_Config.Validate()) {
        std::cerr << "Invalid server configuration!\n";
        return 1;
    }
    if (g_Config.tickRate != 128 || !g_Config.password.empty() || g_Config.enableRcon) {
        std::cerr << "Authoritative movement currently requires 128 Hz; password/RCON are not implemented\n";
        return 1;
    }
    if (g_Config.map != "dm_arena") {
        std::cerr << "Only the built-in shared dm_arena map is supported\n"; return 1;
    }
    
    PrintServerInfo();
    
    std::cout << "Initializing server systems...\n";
    std::cout << "World: shared dm_arena obstacles with swept XZ player collision (no combat/vertical physics)\n";
    
    std::cout << "Starting network listener on port " << g_Config.port << "...\n";
    NetworkLimits limits;
    limits.authoritativeInputOnly = true;
    limits.maxClients = static_cast<std::size_t>(g_Config.maxPlayers);
    limits.handshakeTimeoutMs = static_cast<std::uint32_t>(
        std::max(0.1f, g_Config.connectionTimeout) * 1000.0f);
    limits.idleTimeoutMs = static_cast<std::uint32_t>(
        std::max(0.1f, g_Config.clientTimeout) * 1000.0f);
    auto& network = NetworkManager::Get();
    if (!network.SetLimits(limits) || !network.StartServer(g_Config.port)) {
        std::cerr << "Failed to start network listener: " << network.GetLastError() << '\n';
        network.Shutdown();
        return 2;
    }
    network.SetOnPlayerInput([&network](std::uint32_t id, const Net::PlayerInputPacket& input) {
        if (g_Inputs.size() < 4096) g_Inputs.emplace_back(id, input);
        else network.DisconnectPeer(id);
    });
    std::cout << "NETWORK_READY " << g_Config.port << std::endl;
    
    // Run server loop
    RunServerLoop();
    
    const bool failed = network.GetState() == ConnectionState::Failed;
    network.SetOnPlayerInput({});
    network.Shutdown();
    std::cout << "Cleanup complete.\n";
    
    return failed ? 2 : 0;
}
