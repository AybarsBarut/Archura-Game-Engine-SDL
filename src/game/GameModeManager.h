#pragma once

#include <string>
#include <cstdint>

namespace Archura {

/**
 * @brief Oyun modları
 */
enum class GameMode {
    SinglePlayer,       // Tek oyuncu (varsayılan)
    MultiplayerHost,    // Çok oyunculu - sunucu
    MultiplayerClient   // Çok oyunculu - istemci
};

/**
 * @brief GameModeManager – Oyun modu seçimi ve yönetimi
 *
 * Singleton. Mevcut NetworkManager ile entegre çalışır.
 * Mod değişimi Application::ProcessInput içinden tetiklenir.
 */
class GameModeManager {
public:
    static GameModeManager& Get() {
        static GameModeManager instance;
        return instance;
    }

    // Modu ayarla
    void SetMode(GameMode mode) { m_Mode = mode; }
    GameMode GetMode() const { return m_Mode; }
    const char* GetModeString() const;

    // Çok oyunculu bağlantı yardımcıları
    bool StartHost(const std::string& ip, uint16_t port);
    bool ConnectToHost(const std::string& ip, uint16_t port);
    void Disconnect();

    bool IsMultiplayer() const {
        return m_Mode == GameMode::MultiplayerHost ||
               m_Mode == GameMode::MultiplayerClient;
    }

    // IP / port cache (UI tarafından doldurulur)
    char   m_HostIP[64]  = "127.0.0.1";
    uint16_t m_Port      = 7777;

private:
    GameModeManager() = default;
    ~GameModeManager() = default;
    GameModeManager(const GameModeManager&) = delete;
    GameModeManager& operator=(const GameModeManager&) = delete;

    GameMode m_Mode = GameMode::SinglePlayer;
};

} // namespace Archura
