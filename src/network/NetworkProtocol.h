#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Archura::Net {

// Wire format (all integers and IEEE-754 float bit patterns are big endian):
// magic:u32, version:u16, type:u16, flags:u16, payloadLength:u32, sequence:u32.
// No flags are defined in protocol v1. Receivers reject unknown bits instead of
// silently interpreting an extension differently on each endpoint.
constexpr std::uint32_t kProtocolMagic = 0x41524348U; // "ARCH"
constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::uint16_t kKnownFrameFlags = 0;
constexpr std::size_t kFrameHeaderSize = 18;
constexpr std::size_t kMaxPayloadSize = 256U * 1024U;
constexpr std::size_t kMaxBufferedBytes = kMaxPayloadSize + kFrameHeaderSize;

enum class PacketType : std::uint16_t {
    Connect = 1,
    Disconnect = 2,
    PlayerUpdate = 3,
    PlayerShoot = 4,
    WorldState = 5,
    ConnectAck = 6,
    Heartbeat = 7
};

enum class ProtocolError {
    None,
    BufferLimitExceeded,
    BadMagic,
    UnsupportedVersion,
    UnknownPacketType,
    UnsupportedFlags,
    PayloadTooLarge,
    InvalidSequence,
    InvalidPayload
};

struct FrameHeader {
    std::uint16_t version = kProtocolVersion;
    PacketType type = PacketType::Heartbeat;
    std::uint16_t flags = 0;
    std::uint32_t payloadLength = 0;
    std::uint32_t sequence = 0;
};

struct Frame {
    FrameHeader header;
    std::vector<std::uint8_t> payload;
};

struct PlayerUpdatePacket {
    std::uint32_t id = 0;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;
};

struct PlayerShootPacket {
    std::uint32_t id = 0;
    float originX = 0.0F;
    float originY = 0.0F;
    float originZ = 0.0F;
    float dirX = 0.0F;
    float dirY = 0.0F;
    float dirZ = 0.0F;
    std::int32_t weaponType = 0;
};

struct ParseBatch {
    ProtocolError error = ProtocolError::None;
    std::vector<Frame> frames;

    explicit operator bool() const noexcept { return error == ProtocolError::None; }
};

bool IsKnownPacketType(PacketType type) noexcept;
const char* ToString(ProtocolError error) noexcept;

std::array<std::uint8_t, kFrameHeaderSize> EncodeHeader(const FrameHeader& header);
ProtocolError DecodeHeader(const std::uint8_t* bytes, std::size_t size,
                           FrameHeader& output) noexcept;
std::vector<std::uint8_t> EncodeFrame(PacketType type, std::uint32_t sequence,
                                      const std::vector<std::uint8_t>& payload);

std::vector<std::uint8_t> SerializePlayerUpdate(const PlayerUpdatePacket& packet);
bool DeserializePlayerUpdate(const std::vector<std::uint8_t>& payload,
                             PlayerUpdatePacket& packet) noexcept;
std::vector<std::uint8_t> SerializePlayerShoot(const PlayerShootPacket& packet);
bool DeserializePlayerShoot(const std::vector<std::uint8_t>& payload,
                            PlayerShootPacket& packet) noexcept;
std::vector<std::uint8_t> SerializeSessionToken(std::uint64_t token);
bool DeserializeSessionToken(const std::vector<std::uint8_t>& payload,
                             std::uint64_t& token) noexcept;

// Stateful TCP stream parser. Feed may receive any split/coalescing of frames.
// A protocol error is terminal: Reset is required before this object is reused.
class StreamFrameParser final {
public:
    ParseBatch Feed(const std::uint8_t* data, std::size_t size);
    ParseBatch Feed(const std::vector<std::uint8_t>& data) {
        return Feed(data.data(), data.size());
    }

    void Reset() noexcept;
    std::size_t BufferedBytes() const noexcept;
    ProtocolError Error() const noexcept { return m_Error; }

private:
    void Compact();

    std::vector<std::uint8_t> m_Buffer;
    std::size_t m_ReadOffset = 0;
    ProtocolError m_Error = ProtocolError::None;
};

} // namespace Archura::Net
