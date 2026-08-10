#include "NetworkProtocol.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace Archura::Net {
namespace {

static_assert(sizeof(float) == sizeof(std::uint32_t),
              "The network protocol requires 32-bit IEEE-754 floats");
static_assert(std::numeric_limits<float>::is_iec559,
              "The network protocol requires IEEE-754 floats");

void WriteU16(std::uint8_t* out, std::uint16_t value) noexcept {
    out[0] = static_cast<std::uint8_t>(value >> 8U);
    out[1] = static_cast<std::uint8_t>(value);
}

void WriteU32(std::uint8_t* out, std::uint32_t value) noexcept {
    out[0] = static_cast<std::uint8_t>(value >> 24U);
    out[1] = static_cast<std::uint8_t>(value >> 16U);
    out[2] = static_cast<std::uint8_t>(value >> 8U);
    out[3] = static_cast<std::uint8_t>(value);
}

void WriteU64(std::uint8_t* out, std::uint64_t value) noexcept {
    WriteU32(out, static_cast<std::uint32_t>(value >> 32U));
    WriteU32(out + 4, static_cast<std::uint32_t>(value));
}

std::uint16_t ReadU16(const std::uint8_t* in) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[0]) << 8U) |
                                      static_cast<std::uint16_t>(in[1]));
}

std::uint32_t ReadU32(const std::uint8_t* in) noexcept {
    return (static_cast<std::uint32_t>(in[0]) << 24U) |
           (static_cast<std::uint32_t>(in[1]) << 16U) |
           (static_cast<std::uint32_t>(in[2]) << 8U) |
           static_cast<std::uint32_t>(in[3]);
}

std::uint64_t ReadU64(const std::uint8_t* in) noexcept {
    return (static_cast<std::uint64_t>(ReadU32(in)) << 32U) | ReadU32(in + 4);
}

void AppendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    const auto oldSize = output.size();
    output.resize(oldSize + 4);
    WriteU32(output.data() + oldSize, value);
}

void AppendFloat(std::vector<std::uint8_t>& output, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    AppendU32(output, bits);
}

float ReadFloat(const std::uint8_t* input) noexcept {
    const std::uint32_t bits = ReadU32(input);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace

bool IsKnownPacketType(PacketType type) noexcept {
    switch (type) {
    case PacketType::Connect:
    case PacketType::Disconnect:
    case PacketType::PlayerUpdate:
    case PacketType::PlayerShoot:
    case PacketType::WorldState:
    case PacketType::ConnectAck:
    case PacketType::Heartbeat:
        return true;
    }
    return false;
}

const char* ToString(ProtocolError error) noexcept {
    switch (error) {
    case ProtocolError::None: return "none";
    case ProtocolError::BufferLimitExceeded: return "stream buffer limit exceeded";
    case ProtocolError::BadMagic: return "invalid frame magic";
    case ProtocolError::UnsupportedVersion: return "unsupported protocol version";
    case ProtocolError::UnknownPacketType: return "unknown packet type";
    case ProtocolError::UnsupportedFlags: return "unsupported frame flags";
    case ProtocolError::PayloadTooLarge: return "payload exceeds protocol limit";
    case ProtocolError::InvalidSequence: return "invalid frame sequence";
    case ProtocolError::InvalidPayload: return "invalid packet payload";
    }
    return "unknown protocol error";
}

std::array<std::uint8_t, kFrameHeaderSize> EncodeHeader(const FrameHeader& header) {
    if (header.version != kProtocolVersion || !IsKnownPacketType(header.type) ||
        header.payloadLength > kMaxPayloadSize || header.sequence == 0) {
        throw std::invalid_argument("invalid network frame header");
    }

    std::array<std::uint8_t, kFrameHeaderSize> bytes{};
    WriteU32(bytes.data(), kProtocolMagic);
    WriteU16(bytes.data() + 4, header.version);
    WriteU16(bytes.data() + 6, static_cast<std::uint16_t>(header.type));
    if ((header.flags & static_cast<std::uint16_t>(~kKnownFrameFlags)) != 0) {
        throw std::invalid_argument("unsupported network frame flags");
    }
    WriteU16(bytes.data() + 8, header.flags);
    WriteU32(bytes.data() + 10, header.payloadLength);
    WriteU32(bytes.data() + 14, header.sequence);
    return bytes;
}

ProtocolError DecodeHeader(const std::uint8_t* bytes, std::size_t size,
                           FrameHeader& output) noexcept {
    if (bytes == nullptr || size < kFrameHeaderSize) {
        return ProtocolError::InvalidPayload;
    }
    if (ReadU32(bytes) != kProtocolMagic) {
        return ProtocolError::BadMagic;
    }

    output.version = ReadU16(bytes + 4);
    if (output.version != kProtocolVersion) {
        return ProtocolError::UnsupportedVersion;
    }
    output.type = static_cast<PacketType>(ReadU16(bytes + 6));
    if (!IsKnownPacketType(output.type)) {
        return ProtocolError::UnknownPacketType;
    }
    output.flags = ReadU16(bytes + 8);
    if ((output.flags & static_cast<std::uint16_t>(~kKnownFrameFlags)) != 0) {
        return ProtocolError::UnsupportedFlags;
    }
    output.payloadLength = ReadU32(bytes + 10);
    if (output.payloadLength > kMaxPayloadSize) {
        return ProtocolError::PayloadTooLarge;
    }
    output.sequence = ReadU32(bytes + 14);
    if (output.sequence == 0) {
        return ProtocolError::InvalidSequence;
    }
    return ProtocolError::None;
}

std::vector<std::uint8_t> EncodeFrame(PacketType type, std::uint32_t sequence,
                                      const std::vector<std::uint8_t>& payload) {
    if (payload.size() > kMaxPayloadSize) {
        throw std::length_error("network payload exceeds protocol limit");
    }
    const FrameHeader header{kProtocolVersion, type, 0,
                             static_cast<std::uint32_t>(payload.size()), sequence};
    const auto encodedHeader = EncodeHeader(header);
    std::vector<std::uint8_t> output;
    output.reserve(kFrameHeaderSize + payload.size());
    output.insert(output.end(), encodedHeader.begin(), encodedHeader.end());
    output.insert(output.end(), payload.begin(), payload.end());
    return output;
}

std::vector<std::uint8_t> SerializePlayerUpdate(const PlayerUpdatePacket& packet) {
    std::vector<std::uint8_t> output;
    output.reserve(24);
    AppendU32(output, packet.id);
    AppendFloat(output, packet.x);
    AppendFloat(output, packet.y);
    AppendFloat(output, packet.z);
    AppendFloat(output, packet.yaw);
    AppendFloat(output, packet.pitch);
    return output;
}

bool DeserializePlayerUpdate(const std::vector<std::uint8_t>& payload,
                             PlayerUpdatePacket& packet) noexcept {
    if (payload.size() != 24) {
        return false;
    }
    packet.id = ReadU32(payload.data());
    packet.x = ReadFloat(payload.data() + 4);
    packet.y = ReadFloat(payload.data() + 8);
    packet.z = ReadFloat(payload.data() + 12);
    packet.yaw = ReadFloat(payload.data() + 16);
    packet.pitch = ReadFloat(payload.data() + 20);
    return std::isfinite(packet.x) && std::isfinite(packet.y) &&
           std::isfinite(packet.z) && std::isfinite(packet.yaw) &&
           std::isfinite(packet.pitch);
}

std::vector<std::uint8_t> SerializePlayerShoot(const PlayerShootPacket& packet) {
    std::vector<std::uint8_t> output;
    output.reserve(32);
    AppendU32(output, packet.id);
    AppendFloat(output, packet.originX);
    AppendFloat(output, packet.originY);
    AppendFloat(output, packet.originZ);
    AppendFloat(output, packet.dirX);
    AppendFloat(output, packet.dirY);
    AppendFloat(output, packet.dirZ);
    AppendU32(output, static_cast<std::uint32_t>(packet.weaponType));
    return output;
}

bool DeserializePlayerShoot(const std::vector<std::uint8_t>& payload,
                            PlayerShootPacket& packet) noexcept {
    if (payload.size() != 32) {
        return false;
    }
    packet.id = ReadU32(payload.data());
    packet.originX = ReadFloat(payload.data() + 4);
    packet.originY = ReadFloat(payload.data() + 8);
    packet.originZ = ReadFloat(payload.data() + 12);
    packet.dirX = ReadFloat(payload.data() + 16);
    packet.dirY = ReadFloat(payload.data() + 20);
    packet.dirZ = ReadFloat(payload.data() + 24);
    packet.weaponType = static_cast<std::int32_t>(ReadU32(payload.data() + 28));
    return std::isfinite(packet.originX) && std::isfinite(packet.originY) &&
           std::isfinite(packet.originZ) && std::isfinite(packet.dirX) &&
           std::isfinite(packet.dirY) && std::isfinite(packet.dirZ);
}

std::vector<std::uint8_t> SerializeSessionToken(std::uint64_t token) {
    std::vector<std::uint8_t> output(8);
    WriteU64(output.data(), token);
    return output;
}

bool DeserializeSessionToken(const std::vector<std::uint8_t>& payload,
                             std::uint64_t& token) noexcept {
    if (payload.size() != 8) {
        return false;
    }
    token = ReadU64(payload.data());
    return token != 0;
}

ParseBatch StreamFrameParser::Feed(const std::uint8_t* data, std::size_t size) {
    ParseBatch result;
    if (m_Error != ProtocolError::None) {
        result.error = m_Error;
        return result;
    }
    if (size != 0 && data == nullptr) {
        m_Error = ProtocolError::InvalidPayload;
        result.error = m_Error;
        return result;
    }

    // Consume complete frames directly from the caller's span. Only an incomplete
    // frame is retained, so a large coalesced TCP read cannot force a second copy
    // of the entire read or trip the per-frame memory bound.
    while (size != 0) {
        Compact();
        if (m_Buffer.empty() && size >= kFrameHeaderSize) {
            FrameHeader header;
            const auto error = DecodeHeader(data, size, header);
            if (error != ProtocolError::None) {
                m_Error = error;
                result.error = error;
                return result;
            }
            const auto frameSize = kFrameHeaderSize +
                                   static_cast<std::size_t>(header.payloadLength);
            if (size >= frameSize) {
                Frame frame;
                frame.header = header;
                frame.payload.assign(data + kFrameHeaderSize, data + frameSize);
                result.frames.push_back(std::move(frame));
                data += frameSize;
                size -= frameSize;
                continue;
            }
        }

        std::size_t targetSize = kFrameHeaderSize;
        if (m_Buffer.size() >= kFrameHeaderSize) {
            FrameHeader header;
            const auto error = DecodeHeader(m_Buffer.data(), m_Buffer.size(), header);
            if (error != ProtocolError::None) {
                m_Error = error;
                result.error = error;
                return result;
            }
            targetSize += static_cast<std::size_t>(header.payloadLength);
        }

        const auto required = targetSize - m_Buffer.size();
        const auto copied = std::min(required, size);
        if (copied > kMaxBufferedBytes - m_Buffer.size()) {
            m_Error = ProtocolError::BufferLimitExceeded;
            result.error = m_Error;
            return result;
        }
        m_Buffer.insert(m_Buffer.end(), data, data + copied);
        data += copied;
        size -= copied;

        if (m_Buffer.size() < kFrameHeaderSize) {
            break;
        }

        FrameHeader header;
        const auto error = DecodeHeader(m_Buffer.data(), m_Buffer.size(), header);
        if (error != ProtocolError::None) {
            m_Error = error;
            result.error = error;
            return result;
        }
        const auto frameSize = kFrameHeaderSize +
                               static_cast<std::size_t>(header.payloadLength);
        if (m_Buffer.size() < frameSize) {
            // The header became available on this iteration. Continue once so
            // the remaining payload can be copied without retaining later frames.
            if (size != 0) {
                continue;
            }
            break;
        }

        Frame frame;
        frame.header = header;
        frame.payload.assign(m_Buffer.begin() +
                                 static_cast<std::vector<std::uint8_t>::difference_type>(
                                     kFrameHeaderSize),
                             m_Buffer.end());
        result.frames.push_back(std::move(frame));
        m_Buffer.clear();
    }
    return result;
}

void StreamFrameParser::Reset() noexcept {
    m_Buffer.clear();
    m_ReadOffset = 0;
    m_Error = ProtocolError::None;
}

std::size_t StreamFrameParser::BufferedBytes() const noexcept {
    return m_Buffer.size() - m_ReadOffset;
}

void StreamFrameParser::Compact() {
    if (m_ReadOffset == 0) {
        return;
    }
    if (m_ReadOffset == m_Buffer.size()) {
        m_Buffer.clear();
        m_ReadOffset = 0;
        return;
    }
    if (m_ReadOffset >= 4096 || m_ReadOffset * 2 >= m_Buffer.size()) {
        m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() +
            static_cast<std::vector<std::uint8_t>::difference_type>(m_ReadOffset));
        m_ReadOffset = 0;
    }
}

} // namespace Archura::Net
