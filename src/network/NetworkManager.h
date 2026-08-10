#pragma once

#include "NetworkProtocol.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Archura {

using PacketType = Net::PacketType;
using PlayerUpdatePacket = Net::PlayerUpdatePacket;
using PlayerShootPacket = Net::PlayerShootPacket;

enum class ConnectionState : std::uint8_t {
    Stopped,
    Listening,
    Handshaking,
    Connected,
    ShuttingDown,
    Failed
};

enum class DisconnectReason : std::uint8_t {
    None,
    LocalShutdown,
    PeerClosed,
    PeerRequested,
    TransportError,
    ProtocolViolation,
    HandshakeTimeout,
    IdleTimeout,
    Backpressure,
    Unavailable
};

struct NetworkLimits {
    std::size_t maxClients = 64;
    std::size_t maxQueuedBytesPerPeer = 512U * 1024U;
    std::uint32_t handshakeTimeoutMs = 10'000;
    std::uint32_t idleTimeoutMs = 30'000;
};

struct NetworkStats {
    std::uint64_t bytesReceived = 0;
    std::uint64_t bytesSent = 0;
    std::uint64_t framesReceived = 0;
    std::uint64_t framesSent = 0;
    std::uint64_t rejectedConnections = 0;
    std::size_t connectedPeers = 0;
    std::size_t queuedBytes = 0;
};

// SDL_net-backed framed TCP transport. Public methods are thread-safe. Socket I/O
// happens only in UpdateServer/UpdateClient; callbacks are invoked after the
// internal lock is released and may safely call NetworkManager again.
class NetworkManager final {
public:
    static NetworkManager& Get();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    bool Init();
    void Shutdown();

    bool IsServer() const noexcept;
    bool IsConnected() const noexcept;
    ConnectionState GetState() const noexcept;
    DisconnectReason GetLastDisconnectReason() const noexcept;
    std::string GetLastError() const;
    NetworkStats GetStats() const;

    bool SetLimits(const NetworkLimits& limits);

    bool StartServer(int port);
    void UpdateServer();

    // Returns true once the TCP transport is open. IsConnected becomes true only
    // after the protocol handshake completes in UpdateClient.
    bool Connect(const std::string& host, int port);
    bool Reconnect();
    void UpdateClient();

    bool SendPlayerUpdate(const PlayerUpdatePacket& packet);
    bool SendPlayerShoot(const PlayerShootPacket& packet);
    bool SendWorldState(const std::vector<std::uint8_t>& payload);

    void SetOnPlayerUpdate(std::function<void(const PlayerUpdatePacket&)> callback);
    void SetOnPlayerShoot(std::function<void(const PlayerShootPacket&)> callback);
    void SetOnWorldState(
        std::function<void(const std::vector<std::uint8_t>&)> callback);
    void SetOnDisconnected(std::function<void(DisconnectReason)> callback);

private:
    NetworkManager();
    ~NetworkManager();

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Archura
