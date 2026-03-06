#include "GameModeManager.h"
#include "../network/NetworkManager.h"
#include <iostream>

namespace Archura {

const char* GameModeManager::GetModeString() const {
    switch (m_Mode) {
        case GameMode::SinglePlayer:      return "Single Player";
        case GameMode::MultiplayerHost:   return "Multiplayer (Host)";
        case GameMode::MultiplayerClient: return "Multiplayer (Client)";
        default: return "Unknown";
    }
}

bool GameModeManager::StartHost(const std::string& ip, uint16_t port) {
    m_Mode = GameMode::MultiplayerHost;
    auto& nm = NetworkManager::Get();
    if (nm.StartServer(port)) {
        std::cout << "[GameMode] Sunucu başlatıldı: " << ip << ":" << port << std::endl;
        return true;
    }
    std::cerr << "[GameMode] Sunucu başlatılamadı!" << std::endl;
    m_Mode = GameMode::SinglePlayer;
    return false;
}

bool GameModeManager::ConnectToHost(const std::string& ip, uint16_t port) {
    m_Mode = GameMode::MultiplayerClient;
    auto& nm = NetworkManager::Get();
    if (nm.Connect(ip, port)) {
        std::cout << "[GameMode] Bağlandı: " << ip << ":" << port << std::endl;
        return true;
    }
    std::cerr << "[GameMode] Bağlantı başarısız!" << std::endl;
    m_Mode = GameMode::SinglePlayer;
    return false;
}

void GameModeManager::Disconnect() {
    NetworkManager::Get().Shutdown();
    m_Mode = GameMode::SinglePlayer;
    std::cout << "[GameMode] Bağlantı kesildi. Single Player moduna geçildi." << std::endl;
}

} // namespace Archura
