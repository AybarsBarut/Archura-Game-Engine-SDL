#pragma once
#include "NetworkProtocol.h"
#include <map>

namespace Archura::Net {
constexpr std::size_t kMaxWorldPlayers = 64;
constexpr float kWorldSpeed = 6.0F;
constexpr float kWorldExtent = 256.0F;
constexpr float kWorldTickInterval = 1.0F / 128.0F;

struct WorldPlayer {
    std::uint32_t id = 0;
    std::uint32_t acknowledgedInput = 0;
    float x = 0, z = 0;
};
struct WorldSnapshot {
    std::uint64_t tick = 0;
    std::vector<WorldPlayer> players;
};
std::vector<std::uint8_t> EncodeWorldSnapshot(const WorldSnapshot& snapshot);
bool DecodeWorldSnapshot(const std::vector<std::uint8_t>& bytes, WorldSnapshot& destination);

// Minimal server-owned XZ world. No client position/delta-time is consumed.
// Collision/map physics, combat, prediction and interpolation are separate work.
class AuthoritativeWorld {
public:
    bool SynchronizePeers(std::vector<std::uint32_t> ids);
    bool AcceptInput(std::uint32_t peerID, const PlayerInputPacket& input);
    void Tick();
    WorldSnapshot Snapshot() const;
    std::uint64_t GetTick() const noexcept { return m_Tick; }
private:
    struct Player {
        WorldPlayer state;
        PlayerInputPacket input;
        std::uint64_t lastInputTick = 0;
    };
    std::map<std::uint32_t, Player> m_Players;
    std::uint64_t m_Tick = 0;
};

class ClientWorld {
public:
    bool Apply(const std::vector<std::uint8_t>& bytes);
    const WorldSnapshot& Snapshot() const noexcept { return m_Snapshot; }
    void Reset() { m_Snapshot = {}; }
private:
    WorldSnapshot m_Snapshot;
};
} // namespace Archura::Net
