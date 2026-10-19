#include "AuthoritativeWorld.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace Archura::Net {
namespace {
void Put(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
std::uint32_t Get(const std::uint8_t* bytes) {
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) |
           (std::uint32_t(bytes[2]) << 8) | bytes[3];
}
void PutFloat(std::vector<std::uint8_t>& bytes, float value) {
    std::uint32_t bits; std::memcpy(&bits, &value, 4); Put(bytes, bits);
}
float GetFloat(const std::uint8_t* bytes) {
    auto bits = Get(bytes); float value; std::memcpy(&value, &bits, 4); return value;
}
bool Valid(const WorldPlayer& player) {
    return player.id >= 2 && std::isfinite(player.x) && std::isfinite(player.z) &&
        std::abs(player.x) <= kWorldExtent && std::abs(player.z) <= kWorldExtent;
}
}

std::vector<std::uint8_t> EncodeWorldSnapshot(const WorldSnapshot& snapshot) {
    if (!snapshot.tick || snapshot.players.size() > kMaxWorldPlayers) throw std::invalid_argument("invalid world snapshot");
    std::vector<std::uint8_t> bytes;
    Put(bytes, 0x57530100); // WS, version1, reserved0
    Put(bytes, static_cast<std::uint32_t>(snapshot.tick >> 32));
    Put(bytes, static_cast<std::uint32_t>(snapshot.tick));
    Put(bytes, static_cast<std::uint32_t>(snapshot.players.size()));
    std::uint32_t previous = 1;
    for (const auto& player : snapshot.players) {
        if (!Valid(player) || player.id <= previous) throw std::invalid_argument("invalid world player");
        previous = player.id;
        Put(bytes, player.id); Put(bytes, player.acknowledgedInput);
        PutFloat(bytes, player.x); PutFloat(bytes, player.z);
    }
    return bytes;
}

bool DecodeWorldSnapshot(const std::vector<std::uint8_t>& bytes, WorldSnapshot& destination) {
    if (bytes.size() < 16 || Get(bytes.data()) != 0x57530100) return false;
    WorldSnapshot parsed;
    parsed.tick = (std::uint64_t(Get(bytes.data() + 4)) << 32) | Get(bytes.data() + 8);
    const auto count = Get(bytes.data() + 12);
    if (!parsed.tick || count > kMaxWorldPlayers || bytes.size() != 16 + std::size_t(count) * 16) return false;
    std::uint32_t previous = 1;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* data = bytes.data() + 16 + i * 16;
        WorldPlayer player{Get(data), Get(data + 4), GetFloat(data + 8), GetFloat(data + 12)};
        if (!Valid(player) || player.id <= previous) return false;
        previous = player.id;
        parsed.players.push_back(player);
    }
    destination = std::move(parsed);
    return true;
}

bool AuthoritativeWorld::SynchronizePeers(std::vector<std::uint32_t> ids) {
    if (ids.size() > kMaxWorldPlayers) return false;
    std::sort(ids.begin(), ids.end());
    if ((!ids.empty() && ids.front() < 2) || std::adjacent_find(ids.begin(), ids.end()) != ids.end()) return false;
    std::map<std::uint32_t, Player> next;
    for (auto id : ids) {
        auto it = m_Players.find(id);
        if (it != m_Players.end()) next.emplace(id, it->second);
        else { Player player; player.state.id = id; next.emplace(id, player); }
    }
    m_Players.swap(next);
    return true;
}

bool AuthoritativeWorld::AcceptInput(std::uint32_t peerID, const PlayerInputPacket& input) {
    const auto it = m_Players.find(peerID);
    if (it == m_Players.end() || !ValidPlayerInput(input)) return false;
    auto& player = it->second;
    const auto last = player.state.acknowledgedInput;
    const auto delta = input.sequence - last;
    if ((!last && input.sequence != 1) || (last && (!delta || delta >= 0x80000000U))) return false;
    player.input = input;
    player.state.acknowledgedInput = input.sequence;
    player.lastInputTick = m_Tick;
    return true;
}

void AuthoritativeWorld::Tick() {
    ++m_Tick;
    for (auto& entry : m_Players) {
        auto& player = entry.second;
        if (!player.state.acknowledgedInput || m_Tick - player.lastInputTick > 32) continue;
        float x = player.input.moveX, z = player.input.moveZ;
        const auto length = std::sqrt(x*x + z*z);
        if (length > 1) { x /= length; z /= length; }
        MoveArenaPlayer(player.state.x, player.state.z, x*kWorldSpeed*kWorldTickInterval, z*kWorldSpeed*kWorldTickInterval);
    }
}

WorldSnapshot AuthoritativeWorld::Snapshot() const {
    WorldSnapshot snapshot; snapshot.tick = m_Tick;
    for (const auto& entry : m_Players) snapshot.players.push_back(entry.second.state);
    return snapshot;
}

bool ClientWorld::Apply(const std::vector<std::uint8_t>& bytes) {
    WorldSnapshot parsed;
    if (!DecodeWorldSnapshot(bytes, parsed) || parsed.tick <= m_Snapshot.tick) return false;
    m_Snapshot = std::move(parsed);
    return true;
}
} // namespace Archura::Net
