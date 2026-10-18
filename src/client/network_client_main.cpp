#include "network/NetworkManager.h"
#include "network/NetworkSceneReplica.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// A headless native consumer of the same transport/snapshot/ECS APIs used by
// engine systems. This tool does not claim graphical multiplayer integration.
int main(int argc, char** argv) {
    try {
        int port = 27015, durationMs = 5000;
        float moveX = 0, moveZ = 0;
        std::string host = "127.0.0.1";
        bool replay = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto next = [&]() -> std::string { if (++i >= argc) throw std::invalid_argument("missing option value"); return argv[i]; };
            if (arg == "--port") port = std::stoi(next());
            else if (arg == "--host") host = next();
            else if (arg == "--duration-ms") durationMs = std::stoi(next());
            else if (arg == "--move-x") moveX = std::stof(next());
            else if (arg == "--move-z") moveZ = std::stof(next());
            else if (arg == "--replay-input") replay = true;
            else throw std::invalid_argument("unknown option");
        }
        if (durationMs < 100 || durationMs > 60000 || !Archura::Net::ValidPlayerInput({1, moveX, moveZ})) return 2;
        auto& network = Archura::NetworkManager::Get();
        Archura::Scene scene("replicated network world");
        Archura::Net::NetworkSceneReplica replica;
        bool badSnapshot = false;
        network.SetOnWorldState([&](const auto& bytes) {
            if (!replica.Apply(scene, bytes)) { badSnapshot = true; return; }
            std::cout << "SNAP " << replica.Snapshot().tick << " SELF " << network.GetLocalPeerId();
            for (const auto& player : replica.Snapshot().players)
                std::cout << " PLAYER " << player.id << ' ' << player.x << ' ' << player.z;
            std::cout << std::endl;
        });
        if (!network.Connect(host, port)) { std::cerr << network.GetLastError(); network.SetOnWorldState({}); return 2; }
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        auto nextInput = start;
        std::uint32_t sequence = 1;
        while (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count() < durationMs) {
            network.UpdateClient();
            if (badSnapshot || network.GetState() == Archura::ConnectionState::Failed) {
                std::cerr << "Connection failed: " << network.GetLastError() << std::endl;
                network.SetOnWorldState({}); network.Shutdown(); return 3;
            }
            const auto now = Clock::now();
            if (network.IsConnected() && now >= nextInput) {
                if (!network.SendPlayerInput({sequence, moveX, moveZ})) {
                    network.SetOnWorldState({}); network.Shutdown(); return 4;
                }
                if (!replay) ++sequence;
                nextInput = now + std::chrono::milliseconds(8);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const bool received = replica.Snapshot().tick != 0;
        network.SetOnWorldState({}); network.Shutdown();
        return received ? 0 : 5;
    } catch (const std::exception& error) {
        Archura::NetworkManager::Get().SetOnWorldState({});
        Archura::NetworkManager::Get().Shutdown();
        std::cerr << error.what() << std::endl; return 1;
    }
}
