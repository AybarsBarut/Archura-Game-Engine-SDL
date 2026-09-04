#include "NetworkManager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <utility>

#if defined(ARCHURA_HAS_SDL_NET)
#  if __has_include(<SDL_net.h>)
#    include <SDL_net.h>
#  elif __has_include(<SDL2/SDL_net.h>)
#    include <SDL2/SDL_net.h>
#  else
#    error "ARCHURA_HAS_SDL_NET is set but SDL_net.h is unavailable"
#  endif
#endif

namespace Archura {
namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t kMaxAcceptsPerUpdate = 32;
constexpr std::size_t kMaxFramesPerReceive = 256;

std::uint32_t NextSequence(std::uint32_t sequence) noexcept {
    ++sequence;
    return sequence == 0 ? 1 : sequence;
}

std::uint64_t NewSessionToken() {
    std::random_device random;
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
    std::uint64_t token = (static_cast<std::uint64_t>(random()) << 32U) ^ random() ^ now;
    return token == 0 ? 1 : token;
}

bool ValidLimits(const NetworkLimits& limits) noexcept {
    return limits.maxClients > 0 && limits.maxClients <= 1024 &&
           limits.maxQueuedBytesPerPeer >= Net::kFrameHeaderSize &&
           limits.maxQueuedBytesPerPeer <= 64U * 1024U * 1024U &&
           limits.handshakeTimeoutMs >= 100 && limits.handshakeTimeoutMs <= 120'000 &&
           limits.idleTimeoutMs >= limits.handshakeTimeoutMs &&
           limits.idleTimeoutMs <= 600'000;
}

} // namespace

struct NetworkManager::Impl {
    mutable std::mutex mutex;
    bool initialized = false;
    bool server = false;
    ConnectionState state = ConnectionState::Stopped;
    DisconnectReason lastDisconnect = DisconnectReason::None;
    std::string lastError;
    NetworkLimits limits;
    NetworkStats stats;
    std::string reconnectHost;
    int reconnectPort = 0;
    std::function<void(const PlayerUpdatePacket&)> onPlayerUpdate;
    std::function<void(const PlayerShootPacket&)> onPlayerShoot;
    std::function<void(const std::vector<std::uint8_t>&)> onWorldState;
    std::function<void(DisconnectReason)> onDisconnected;

#if defined(ARCHURA_HAS_SDL_NET)
    enum class PeerPhase : std::uint8_t { AwaitingHello, AwaitingWelcome, Established };

    struct PendingWrite {
        std::vector<std::uint8_t> bytes;
        std::size_t offset = 0;
    };

    struct Peer {
        TCPsocket socket = nullptr;
        Net::StreamFrameParser parser;
        std::deque<PendingWrite> writes;
        std::size_t queuedBytes = 0;
        std::uint32_t nextSendSequence = 1;
        std::uint32_t expectedReceiveSequence = 1;
        std::uint64_t sessionToken = 0;
        std::uint32_t peerId = 0;
        PeerPhase phase = PeerPhase::AwaitingHello;
        Clock::time_point connectedAt = Clock::now();
        Clock::time_point lastReceiveActivity = Clock::now();
        Clock::time_point lastHeartbeatQueued = Clock::now();
        DisconnectReason pendingDisconnect = DisconnectReason::None;
    };

    struct Event {
        Net::PacketType type = Net::PacketType::Heartbeat;
        PlayerUpdatePacket update;
        PlayerShootPacket shoot;
        std::vector<std::uint8_t> worldState;
    };

    TCPsocket listener = nullptr;
    SDLNet_SocketSet socketSet = nullptr;
    std::vector<std::unique_ptr<Peer>> peers;
    std::vector<Event> events;
    std::uint64_t clientSessionToken = 0;
    std::uint32_t nextPeerId = 2; // 1 is reserved for the authoritative server.

    std::uint32_t AllocatePeerId() noexcept {
        for (std::size_t attempt = 0; attempt <= peers.size(); ++attempt) {
            const auto candidate = nextPeerId;
            nextPeerId = NextSequence(nextPeerId);
            if (nextPeerId == 1) {
                nextPeerId = 2;
            }
            const bool inUse = std::any_of(peers.begin(), peers.end(),
                [candidate](const auto& existing) {
                    return existing->peerId == candidate;
                });
            if (!inUse) {
                return candidate;
            }
        }
        return 0;
    }

    static std::string SdlError(const char* operation) {
        const char* detail = SDL_GetError();
        std::string message(operation);
        if (detail != nullptr && detail[0] != '\0') {
            message += ": ";
            message += detail;
        }
        return message;
    }

    void SetFailure(DisconnectReason reason, std::string message) {
        lastDisconnect = reason;
        lastError = std::move(message);
        state = ConnectionState::Failed;
    }

    void ClosePeer(Peer& peer) noexcept {
        if (peer.socket != nullptr) {
            if (socketSet != nullptr) {
                SDLNet_TCP_DelSocket(socketSet, peer.socket);
            }
            SDLNet_TCP_Close(peer.socket);
            peer.socket = nullptr;
        }
        stats.queuedBytes -= std::min(stats.queuedBytes, peer.queuedBytes);
        peer.queuedBytes = 0;
        peer.writes.clear();
    }

    void CloseAllSockets() noexcept {
        for (auto& peer : peers) {
            ClosePeer(*peer);
        }
        peers.clear();
        if (listener != nullptr) {
            SDLNet_TCP_Close(listener);
            listener = nullptr;
        }
        if (socketSet != nullptr) {
            SDLNet_FreeSocketSet(socketSet);
            socketSet = nullptr;
        }
        stats.connectedPeers = 0;
        stats.queuedBytes = 0;
    }

    bool Queue(Peer& peer, Net::PacketType type,
               const std::vector<std::uint8_t>& payload) {
        if (payload.size() > Net::kMaxPayloadSize) {
            return false;
        }
        std::vector<std::uint8_t> frame;
        try {
            frame = Net::EncodeFrame(type, peer.nextSendSequence, payload);
        } catch (const std::exception& exception) {
            lastError = exception.what();
            return false;
        }
        if (frame.size() > limits.maxQueuedBytesPerPeer -
                               std::min(peer.queuedBytes, limits.maxQueuedBytesPerPeer)) {
            return false;
        }
        peer.nextSendSequence = NextSequence(peer.nextSendSequence);
        peer.queuedBytes += frame.size();
        stats.queuedBytes += frame.size();
        peer.writes.push_back(PendingWrite{std::move(frame), 0});
        return true;
    }

    bool Flush(Peer& peer) {
        // SDL_net documents a short transfer as connection closure or an
        // unknown socket error, not as EAGAIN. Retrying the suffix would risk
        // treating a failed stream as healthy.
        if (peer.writes.empty()) {
            return true;
        }
        auto& write = peer.writes.front();
        const auto remaining = write.bytes.size() - write.offset;
        const auto amount = static_cast<int>(std::min<std::size_t>(
            remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        SDL_ClearError();
        const int sent = SDLNet_TCP_Send(peer.socket, write.bytes.data() + write.offset, amount);
        if (sent != amount) {
            lastError = SdlError("SDLNet_TCP_Send failed");
            return false;
        }
        write.offset += static_cast<std::size_t>(sent);
        peer.queuedBytes -= static_cast<std::size_t>(sent);
        stats.queuedBytes -= std::min(stats.queuedBytes, static_cast<std::size_t>(sent));
        stats.bytesSent += static_cast<std::uint64_t>(sent);
        if (write.offset == write.bytes.size()) {
            peer.writes.pop_front();
            ++stats.framesSent;
        }
        return true;
    }

    bool ValidateSequence(Peer& peer, const Net::Frame& frame) noexcept {
        if (frame.header.sequence != peer.expectedReceiveSequence) {
            lastError = "out-of-order or replayed TCP frame";
            return false;
        }
        peer.expectedReceiveSequence = NextSequence(peer.expectedReceiveSequence);
        return true;
    }

    bool QueueHeartbeatIfDue(Peer& peer, Clock::time_point now) {
        if (peer.phase != PeerPhase::Established) {
            return true;
        }
        const auto intervalMs = std::max<std::uint32_t>(
            1'000, limits.idleTimeoutMs / 3);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - peer.lastHeartbeatQueued).count();
        if (elapsed < intervalMs) {
            return true;
        }
        if (!Queue(peer, Net::PacketType::Heartbeat, {})) {
            return false;
        }
        peer.lastHeartbeatQueued = now;
        return true;
    }

    bool Broadcast(Net::PacketType type, const std::vector<std::uint8_t>& payload,
                   const Peer* except) {
        bool allQueued = true;
        for (auto& destination : peers) {
            if (destination.get() != except && destination->phase == PeerPhase::Established) {
                if (!Queue(*destination, type, payload)) {
                    // Backpressure is a property of the slow destination. Do not
                    // punish the sender or leave the saturated peer connected.
                    destination->pendingDisconnect = DisconnectReason::Backpressure;
                    allQueued = false;
                }
            }
        }
        return allQueued;
    }

    bool HandleFrame(Peer& peer, const Net::Frame& frame) {
        if (!ValidateSequence(peer, frame)) {
            return false;
        }
        ++stats.framesReceived;

        if (peer.phase == PeerPhase::AwaitingHello) {
            if (!server || frame.header.type != Net::PacketType::Connect ||
                !Net::DeserializeSessionToken(frame.payload, peer.sessionToken)) {
                lastError = "expected a valid client hello";
                return false;
            }
            peer.phase = PeerPhase::Established;
            if (!Queue(peer, Net::PacketType::ConnectAck,
                       Net::SerializeSessionToken(peer.sessionToken))) {
                lastError = "connection acknowledgement exceeded send queue";
                return false;
            }
            return true;
        }

        if (peer.phase == PeerPhase::AwaitingWelcome) {
            std::uint64_t echoedToken = 0;
            if (server || frame.header.type != Net::PacketType::ConnectAck ||
                !Net::DeserializeSessionToken(frame.payload, echoedToken) ||
                echoedToken != clientSessionToken) {
                lastError = "invalid server handshake acknowledgement";
                return false;
            }
            peer.phase = PeerPhase::Established;
            state = ConnectionState::Connected;
            lastDisconnect = DisconnectReason::None;
            return true;
        }

        switch (frame.header.type) {
        case Net::PacketType::PlayerUpdate: {
            PlayerUpdatePacket packet;
            if (!Net::DeserializePlayerUpdate(frame.payload, packet)) {
                lastError = "invalid player update payload";
                return false;
            }
            if (server) {
                // Never trust a client-controlled entity identifier. Bind input
                // to the server-assigned connection identity.
                packet.id = peer.peerId;
            }
            events.push_back(Event{Net::PacketType::PlayerUpdate, packet, {}});
            const auto relayPayload = server ? Net::SerializePlayerUpdate(packet)
                                             : frame.payload;
            if (server && !Broadcast(frame.header.type, relayPayload, &peer)) {
                lastError = "one or more slow clients exceeded their send queue";
            }
            return true;
        }
        case Net::PacketType::PlayerShoot: {
            PlayerShootPacket packet;
            if (!Net::DeserializePlayerShoot(frame.payload, packet)) {
                lastError = "invalid player shoot payload";
                return false;
            }
            if (server) {
                packet.id = peer.peerId;
            }
            Event event;
            event.type = Net::PacketType::PlayerShoot;
            event.shoot = packet;
            events.push_back(event);
            const auto relayPayload = server ? Net::SerializePlayerShoot(packet)
                                             : frame.payload;
            if (server && !Broadcast(frame.header.type, relayPayload, &peer)) {
                lastError = "one or more slow clients exceeded their send queue";
            }
            return true;
        }
        case Net::PacketType::Heartbeat:
            return frame.payload.empty();
        case Net::PacketType::WorldState:
            if (server) {
                lastError = "client attempted to send authoritative world state";
                return false;
            }
            events.push_back(Event{Net::PacketType::WorldState, {}, {}, frame.payload});
            return true;
        case Net::PacketType::Disconnect:
            lastDisconnect = DisconnectReason::PeerRequested;
            lastError = "peer requested disconnect";
            return false;
        case Net::PacketType::Connect:
        case Net::PacketType::ConnectAck:
            lastError = "handshake packet received after session establishment";
            return false;
        }
        return false;
    }

    bool Receive(Peer& peer, DisconnectReason& reason) {
        std::uint8_t buffer[16U * 1024U];
        SDL_ClearError();
        const int received = SDLNet_TCP_Recv(peer.socket, buffer, sizeof(buffer));
        if (received <= 0) {
            const char* detail = SDL_GetError();
            if (detail != nullptr && detail[0] != '\0') {
                reason = DisconnectReason::TransportError;
                lastError = SdlError("SDLNet_TCP_Recv failed");
            } else {
                reason = DisconnectReason::PeerClosed;
                lastError = "peer closed the TCP stream";
            }
            return false;
        }
        stats.bytesReceived += static_cast<std::uint64_t>(received);
        peer.lastReceiveActivity = Clock::now();
        auto batch = peer.parser.Feed(buffer, static_cast<std::size_t>(received));
        if (!batch) {
            reason = DisconnectReason::ProtocolViolation;
            lastError = Net::ToString(batch.error);
            return false;
        }
        if (batch.frames.size() > kMaxFramesPerReceive) {
            reason = DisconnectReason::ProtocolViolation;
            lastError = "peer exceeded the per-receive frame budget";
            return false;
        }
        for (const auto& frame : batch.frames) {
            if (!HandleFrame(peer, frame)) {
                reason = lastDisconnect == DisconnectReason::PeerRequested
                    ? DisconnectReason::PeerRequested
                    : DisconnectReason::ProtocolViolation;
                return false;
            }
        }
        return true;
    }

    DisconnectReason TimeoutReason(const Peer& peer, Clock::time_point now) const noexcept {
        const auto handshakeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - peer.connectedAt).count();
        if (peer.phase != PeerPhase::Established &&
            handshakeElapsed >= limits.handshakeTimeoutMs) {
            return DisconnectReason::HandshakeTimeout;
        }
        const auto idleElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - peer.lastReceiveActivity).count();
        if (idleElapsed >= limits.idleTimeoutMs) {
            return DisconnectReason::IdleTimeout;
        }
        return DisconnectReason::None;
    }

    void DispatchEvents(std::vector<Event> pending,
                        std::function<void(const PlayerUpdatePacket&)> updateCallback,
                        std::function<void(const PlayerShootPacket&)> shootCallback,
                        std::function<void(const std::vector<std::uint8_t>&)> worldCallback) {
        for (const auto& event : pending) {
            try {
                if (event.type == Net::PacketType::PlayerUpdate && updateCallback) {
                    updateCallback(event.update);
                } else if (event.type == Net::PacketType::PlayerShoot && shootCallback) {
                    shootCallback(event.shoot);
                } else if (event.type == Net::PacketType::WorldState && worldCallback) {
                    worldCallback(event.worldState);
                }
            } catch (const std::exception& error) {
                std::cerr << "[Network] callback exception: " << error.what() << '\n';
            } catch (...) {
                std::cerr << "[Network] callback threw an unknown exception\n";
            }
        }
    }
#endif
};

NetworkManager& NetworkManager::Get() {
    static NetworkManager manager;
    return manager;
}

NetworkManager::NetworkManager() : m_Impl(std::make_unique<Impl>()) {}

NetworkManager::~NetworkManager() {
    {
        // Static destruction must never call back into objects whose own static
        // lifetime may already have ended.
        std::lock_guard<std::mutex> lock(m_Impl->mutex);
        m_Impl->onPlayerUpdate = {};
        m_Impl->onPlayerShoot = {};
        m_Impl->onWorldState = {};
        m_Impl->onDisconnected = {};
    }
    Shutdown();
}

bool NetworkManager::Init() {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    if (m_Impl->initialized) {
        return true;
    }
#if defined(ARCHURA_HAS_SDL_NET)
    if (SDLNet_Init() != 0) {
        m_Impl->SetFailure(DisconnectReason::TransportError,
                           Impl::SdlError("SDLNet_Init failed"));
        return false;
    }
    m_Impl->initialized = true;
    m_Impl->state = ConnectionState::Stopped;
    m_Impl->lastDisconnect = DisconnectReason::None;
    m_Impl->lastError.clear();
    return true;
#else
    m_Impl->state = ConnectionState::Failed;
    m_Impl->lastDisconnect = DisconnectReason::Unavailable;
    m_Impl->lastError = "SDL_net support was not available when this binary was built";
    std::cerr << "[Network] " << m_Impl->lastError << '\n';
    return false;
#endif
}

void NetworkManager::Shutdown() {
    std::function<void(DisconnectReason)> callback;
    DisconnectReason reason = DisconnectReason::None;
    {
        std::lock_guard<std::mutex> lock(m_Impl->mutex);
        if (!m_Impl->initialized) {
            if (m_Impl->state != ConnectionState::Failed) {
                m_Impl->state = ConnectionState::Stopped;
            }
            return;
        }
        m_Impl->state = ConnectionState::ShuttingDown;
#if defined(ARCHURA_HAS_SDL_NET)
        m_Impl->CloseAllSockets();
        SDLNet_Quit();
#endif
        m_Impl->initialized = false;
        m_Impl->server = false;
        m_Impl->state = ConnectionState::Stopped;
        m_Impl->lastDisconnect = DisconnectReason::LocalShutdown;
        callback = m_Impl->onDisconnected;
        reason = m_Impl->lastDisconnect;
    }
    if (callback) {
        callback(reason);
    }
}

bool NetworkManager::IsServer() const noexcept {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    return m_Impl->server;
}

bool NetworkManager::IsConnected() const noexcept {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    return m_Impl->state == ConnectionState::Connected ||
           (m_Impl->state == ConnectionState::Listening &&
            m_Impl->stats.connectedPeers != 0);
}

ConnectionState NetworkManager::GetState() const noexcept {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    return m_Impl->state;
}

DisconnectReason NetworkManager::GetLastDisconnectReason() const noexcept {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    return m_Impl->lastDisconnect;
}

std::string NetworkManager::GetLastError() const {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    return m_Impl->lastError;
}

NetworkStats NetworkManager::GetStats() const {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    return m_Impl->stats;
}

NetworkRuntimeSnapshot NetworkManager::GetRuntimeSnapshot() const {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    NetworkRuntimeSnapshot snapshot;
    snapshot.state = m_Impl->state;
    snapshot.lastDisconnectReason = m_Impl->lastDisconnect;
    snapshot.stats = m_Impl->stats;
    snapshot.lastError = m_Impl->lastError;
    return snapshot;
}

bool NetworkManager::SetLimits(const NetworkLimits& limits) {
    if (!ValidLimits(limits)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    if (m_Impl->state != ConnectionState::Stopped &&
        m_Impl->state != ConnectionState::Failed) {
        return false;
    }
    m_Impl->limits = limits;
    return true;
}

bool NetworkManager::StartServer(int port) {
    if (port < 1 || port > 65535) {
        return false;
    }
    Shutdown();
    if (!Init()) {
        return false;
    }
#if defined(ARCHURA_HAS_SDL_NET)
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    IPaddress address{};
    if (SDLNet_ResolveHost(&address, nullptr, static_cast<Uint16>(port)) != 0) {
        m_Impl->SetFailure(DisconnectReason::TransportError,
                           Impl::SdlError("SDLNet_ResolveHost failed"));
        return false;
    }
    m_Impl->listener = SDLNet_TCP_Open(&address);
    if (m_Impl->listener == nullptr) {
        m_Impl->SetFailure(DisconnectReason::TransportError,
                           Impl::SdlError("SDLNet_TCP_Open listener failed"));
        return false;
    }
    m_Impl->socketSet = SDLNet_AllocSocketSet(static_cast<int>(m_Impl->limits.maxClients));
    if (m_Impl->socketSet == nullptr) {
        SDLNet_TCP_Close(m_Impl->listener);
        m_Impl->listener = nullptr;
        m_Impl->SetFailure(DisconnectReason::TransportError,
                           Impl::SdlError("SDLNet_AllocSocketSet failed"));
        return false;
    }
    m_Impl->server = true;
    m_Impl->state = ConnectionState::Listening;
    m_Impl->lastDisconnect = DisconnectReason::None;
    m_Impl->lastError.clear();
    return true;
#else
    (void)port;
    return false;
#endif
}

bool NetworkManager::Connect(const std::string& host, int port) {
    if (host.empty() || port < 1 || port > 65535) {
        return false;
    }
    Shutdown();
    if (!Init()) {
        return false;
    }
#if defined(ARCHURA_HAS_SDL_NET)
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    IPaddress address{};
    if (SDLNet_ResolveHost(&address, host.c_str(), static_cast<Uint16>(port)) != 0) {
        m_Impl->SetFailure(DisconnectReason::TransportError,
                           Impl::SdlError("SDLNet_ResolveHost failed"));
        return false;
    }
    TCPsocket socket = SDLNet_TCP_Open(&address);
    if (socket == nullptr) {
        m_Impl->SetFailure(DisconnectReason::TransportError,
                           Impl::SdlError("SDLNet_TCP_Open connection failed"));
        return false;
    }
    m_Impl->socketSet = SDLNet_AllocSocketSet(1);
    if (m_Impl->socketSet == nullptr || SDLNet_TCP_AddSocket(m_Impl->socketSet, socket) < 0) {
        if (m_Impl->socketSet != nullptr) {
            SDLNet_FreeSocketSet(m_Impl->socketSet);
            m_Impl->socketSet = nullptr;
        }
        SDLNet_TCP_Close(socket);
        m_Impl->SetFailure(DisconnectReason::TransportError,
                           Impl::SdlError("failed to register client socket"));
        return false;
    }
    auto peer = std::make_unique<Impl::Peer>();
    peer->socket = socket;
    peer->phase = Impl::PeerPhase::AwaitingWelcome;
    peer->sessionToken = NewSessionToken();
    m_Impl->clientSessionToken = peer->sessionToken;
    m_Impl->reconnectHost = host;
    m_Impl->reconnectPort = port;
    if (!m_Impl->Queue(*peer, Net::PacketType::Connect,
                       Net::SerializeSessionToken(peer->sessionToken))) {
        m_Impl->ClosePeer(*peer);
        m_Impl->SetFailure(DisconnectReason::Backpressure,
                           "client hello exceeded send queue");
        return false;
    }
    m_Impl->peers.push_back(std::move(peer));
    m_Impl->server = false;
    m_Impl->state = ConnectionState::Handshaking;
    m_Impl->lastDisconnect = DisconnectReason::None;
    m_Impl->lastError.clear();
    return true;
#else
    (void)host;
    (void)port;
    return false;
#endif
}

bool NetworkManager::Reconnect() {
    std::string host;
    int port = 0;
    {
        std::lock_guard<std::mutex> lock(m_Impl->mutex);
        host = m_Impl->reconnectHost;
        port = m_Impl->reconnectPort;
    }
    return !host.empty() && Connect(host, port);
}

void NetworkManager::UpdateServer() {
#if defined(ARCHURA_HAS_SDL_NET)
    std::vector<Impl::Event> events;
    std::function<void(const PlayerUpdatePacket&)> updateCallback;
    std::function<void(const PlayerShootPacket&)> shootCallback;
    std::function<void(const std::vector<std::uint8_t>&)> worldCallback;
    {
        std::lock_guard<std::mutex> lock(m_Impl->mutex);
        if (!m_Impl->server || m_Impl->state != ConnectionState::Listening) {
            return;
        }

        // Bound accept work so a connection flood cannot monopolize the fixed
        // server tick even when every accepted socket is immediately rejected.
        for (std::size_t acceptedThisUpdate = 0;
             acceptedThisUpdate < kMaxAcceptsPerUpdate; ++acceptedThisUpdate) {
            TCPsocket accepted = SDLNet_TCP_Accept(m_Impl->listener);
            if (accepted == nullptr) {
                break;
            }
            if (m_Impl->peers.size() >= m_Impl->limits.maxClients ||
                SDLNet_TCP_AddSocket(m_Impl->socketSet, accepted) < 0) {
                SDLNet_TCP_Close(accepted);
                ++m_Impl->stats.rejectedConnections;
                continue;
            }
            auto peer = std::make_unique<Impl::Peer>();
            peer->socket = accepted;
            peer->phase = Impl::PeerPhase::AwaitingHello;
            peer->peerId = m_Impl->AllocatePeerId();
            if (peer->peerId == 0) {
                SDLNet_TCP_DelSocket(m_Impl->socketSet, accepted);
                SDLNet_TCP_Close(accepted);
                ++m_Impl->stats.rejectedConnections;
                continue;
            }
            m_Impl->peers.push_back(std::move(peer));
        }

        const int ready = SDLNet_CheckSockets(m_Impl->socketSet, 0);
        if (ready < 0) {
            m_Impl->SetFailure(DisconnectReason::TransportError,
                               Impl::SdlError("SDLNet_CheckSockets failed"));
            m_Impl->CloseAllSockets();
            m_Impl->server = false;
            return;
        }
        const auto now = Clock::now();
        for (auto it = m_Impl->peers.begin(); it != m_Impl->peers.end();) {
            auto& peer = **it;
            DisconnectReason reason = peer.pendingDisconnect;
            if (reason == DisconnectReason::None) {
                reason = m_Impl->TimeoutReason(peer, now);
            }
            bool keep = reason == DisconnectReason::None;
            if (keep && ready > 0 && SDLNet_SocketReady(peer.socket)) {
                keep = m_Impl->Receive(peer, reason);
            }
            if (keep && !m_Impl->QueueHeartbeatIfDue(peer, now)) {
                keep = false;
                reason = DisconnectReason::Backpressure;
            }
            if (keep && !peer.writes.empty()) {
                keep = m_Impl->Flush(peer);
                if (!keep) {
                    reason = DisconnectReason::TransportError;
                }
            }
            if (!keep) {
                m_Impl->lastDisconnect = reason;
                m_Impl->ClosePeer(peer);
                it = m_Impl->peers.erase(it);
            } else {
                ++it;
            }
        }
        m_Impl->stats.connectedPeers = static_cast<std::size_t>(std::count_if(
            m_Impl->peers.begin(), m_Impl->peers.end(), [](const auto& peer) {
                return peer->phase == Impl::PeerPhase::Established;
            }));
        events.swap(m_Impl->events);
        updateCallback = m_Impl->onPlayerUpdate;
        shootCallback = m_Impl->onPlayerShoot;
        worldCallback = m_Impl->onWorldState;
    }
    m_Impl->DispatchEvents(std::move(events), std::move(updateCallback),
                           std::move(shootCallback), std::move(worldCallback));
#endif
}

void NetworkManager::UpdateClient() {
#if defined(ARCHURA_HAS_SDL_NET)
    std::vector<Impl::Event> events;
    std::function<void(const PlayerUpdatePacket&)> updateCallback;
    std::function<void(const PlayerShootPacket&)> shootCallback;
    std::function<void(const std::vector<std::uint8_t>&)> worldCallback;
    std::function<void(DisconnectReason)> disconnectCallback;
    DisconnectReason disconnected = DisconnectReason::None;
    {
        std::lock_guard<std::mutex> lock(m_Impl->mutex);
        if (m_Impl->server || (m_Impl->state != ConnectionState::Handshaking &&
                               m_Impl->state != ConnectionState::Connected) ||
            m_Impl->peers.empty()) {
            return;
        }
        auto& peer = *m_Impl->peers.front();
        const int ready = SDLNet_CheckSockets(m_Impl->socketSet, 0);
        bool keep = ready >= 0;
        if (!keep) {
            disconnected = DisconnectReason::TransportError;
            m_Impl->lastError = Impl::SdlError("SDLNet_CheckSockets failed");
        }
        if (keep && ready > 0 && SDLNet_SocketReady(peer.socket)) {
            keep = m_Impl->Receive(peer, disconnected);
        }
        const auto now = Clock::now();
        if (keep && !m_Impl->QueueHeartbeatIfDue(peer, now)) {
            keep = false;
            disconnected = DisconnectReason::Backpressure;
        }
        if (keep && !peer.writes.empty()) {
            keep = m_Impl->Flush(peer);
            if (!keep) {
                disconnected = DisconnectReason::TransportError;
            }
        }
        if (keep) {
            disconnected = m_Impl->TimeoutReason(peer, now);
            keep = disconnected == DisconnectReason::None;
        }
        if (!keep) {
            m_Impl->ClosePeer(peer);
            m_Impl->peers.clear();
            m_Impl->stats.connectedPeers = 0;
            m_Impl->lastDisconnect = disconnected;
            m_Impl->state = ConnectionState::Failed;
            disconnectCallback = m_Impl->onDisconnected;
        } else {
            m_Impl->stats.connectedPeers = peer.phase == Impl::PeerPhase::Established ? 1 : 0;
        }
        events.swap(m_Impl->events);
        updateCallback = m_Impl->onPlayerUpdate;
        shootCallback = m_Impl->onPlayerShoot;
        worldCallback = m_Impl->onWorldState;
    }
    m_Impl->DispatchEvents(std::move(events), std::move(updateCallback),
                           std::move(shootCallback), std::move(worldCallback));
    if (disconnectCallback) {
        try {
            disconnectCallback(disconnected);
        } catch (const std::exception& error) {
            std::cerr << "[Network] disconnect callback exception: "
                      << error.what() << '\n';
        } catch (...) {
            std::cerr << "[Network] disconnect callback threw an unknown exception\n";
        }
    }
#endif
}

bool NetworkManager::SendPlayerUpdate(const PlayerUpdatePacket& packet) {
#if defined(ARCHURA_HAS_SDL_NET)
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    const auto payload = Net::SerializePlayerUpdate(packet);
    if (m_Impl->server && m_Impl->state == ConnectionState::Listening) {
        return m_Impl->Broadcast(Net::PacketType::PlayerUpdate, payload, nullptr);
    }
    return m_Impl->state == ConnectionState::Connected && !m_Impl->peers.empty() &&
           m_Impl->Queue(*m_Impl->peers.front(), Net::PacketType::PlayerUpdate, payload);
#else
    (void)packet;
    return false;
#endif
}

bool NetworkManager::SendPlayerShoot(const PlayerShootPacket& packet) {
#if defined(ARCHURA_HAS_SDL_NET)
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    const auto payload = Net::SerializePlayerShoot(packet);
    if (m_Impl->server && m_Impl->state == ConnectionState::Listening) {
        return m_Impl->Broadcast(Net::PacketType::PlayerShoot, payload, nullptr);
    }
    return m_Impl->state == ConnectionState::Connected && !m_Impl->peers.empty() &&
           m_Impl->Queue(*m_Impl->peers.front(), Net::PacketType::PlayerShoot, payload);
#else
    (void)packet;
    return false;
#endif
}

bool NetworkManager::SendWorldState(const std::vector<std::uint8_t>& payload) {
#if defined(ARCHURA_HAS_SDL_NET)
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    if (payload.size() > Net::kMaxPayloadSize) {
        return false;
    }
    return m_Impl->server && m_Impl->state == ConnectionState::Listening &&
           m_Impl->Broadcast(Net::PacketType::WorldState, payload, nullptr);
#else
    (void)payload;
    return false;
#endif
}

void NetworkManager::SetOnPlayerUpdate(
    std::function<void(const PlayerUpdatePacket&)> callback) {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    m_Impl->onPlayerUpdate = std::move(callback);
}

void NetworkManager::SetOnPlayerShoot(
    std::function<void(const PlayerShootPacket&)> callback) {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    m_Impl->onPlayerShoot = std::move(callback);
}

void NetworkManager::SetOnWorldState(
    std::function<void(const std::vector<std::uint8_t>&)> callback) {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    m_Impl->onWorldState = std::move(callback);
}

void NetworkManager::SetOnDisconnected(
    std::function<void(DisconnectReason)> callback) {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    m_Impl->onDisconnected = std::move(callback);
}

} // namespace Archura
