#include "network/NetworkManager.h"

#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        auto& network = Archura::NetworkManager::Get();
        Archura::NetworkLimits invalid;
        invalid.maxClients = 0;
        Require(!network.SetLimits(invalid), "invalid network limits were accepted");

        Archura::NetworkLimits limits;
        limits.maxClients = 8;
        Require(network.SetLimits(limits), "valid network limits were rejected");
        Require(!network.Init(), "no-backend Init unexpectedly succeeded");
        Require(network.GetState() == Archura::ConnectionState::Failed,
                "no-backend Init did not enter Failed state");
        Require(network.GetLastDisconnectReason() == Archura::DisconnectReason::Unavailable,
                "no-backend Init did not report Unavailable");
        Require(!network.StartServer(27015), "no-backend server unexpectedly started");
        Require(!network.Connect("127.0.0.1", 27015),
                "no-backend client unexpectedly connected");
        Require(!network.IsConnected(), "no-backend manager reported a connection");

        std::vector<std::thread> readers;
        for (int threadIndex = 0; threadIndex < 4; ++threadIndex) {
            readers.emplace_back([&network] {
                for (int iteration = 0; iteration < 2'000; ++iteration) {
                    (void)network.GetState();
                    (void)network.GetStats();
                    (void)network.GetLastError();
                    (void)network.IsServer();
                }
            });
        }
        for (auto& reader : readers) {
            reader.join();
        }

        network.Shutdown();
        network.Shutdown();
        Require(network.GetState() == Archura::ConnectionState::Failed,
                "idempotent no-backend shutdown erased the failure state");
        std::cout << "Archura no-SDL_net backend tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Archura no-SDL_net backend test failure: "
                  << exception.what() << '\n';
        return 1;
    }
}
