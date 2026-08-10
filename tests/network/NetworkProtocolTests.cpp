#include "network/NetworkProtocol.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Archura::Net;

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::uint32_t FloatBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void TestHeaderNetworkByteOrder() {
    const FrameHeader header{kProtocolVersion, PacketType::PlayerUpdate, 0,
                             0x010203U, 0xA1B2C3D4U};
    const auto bytes = EncodeHeader(header);
    const std::array<std::uint8_t, kFrameHeaderSize> expected{
        0x41, 0x52, 0x43, 0x48, 0x00, 0x01, 0x00, 0x03,
        0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0xA1, 0xB2, 0xC3, 0xD4};
    Require(bytes == expected, "header is not encoded in canonical network byte order");

    FrameHeader decoded;
    Require(DecodeHeader(bytes.data(), bytes.size(), decoded) == ProtocolError::None,
            "valid header did not decode");
    Require(decoded.type == header.type && decoded.payloadLength == header.payloadLength &&
                decoded.sequence == header.sequence,
            "header roundtrip changed fields");
}

void TestEveryTwoPartSplit() {
    const std::vector<std::uint8_t> payload{0, 1, 2, 3, 4, 5, 6, 7, 8};
    const auto encoded = EncodeFrame(PacketType::WorldState, 7, payload);
    for (std::size_t split = 0; split < encoded.size(); ++split) {
        StreamFrameParser parser;
        const auto first = parser.Feed(encoded.data(), split);
        Require(first && first.frames.empty(), "partial frame was emitted or rejected");
        const auto second = parser.Feed(encoded.data() + split, encoded.size() - split);
        Require(second && second.frames.size() == 1, "split frame was not reassembled");
        Require(second.frames.front().payload == payload, "split frame payload changed");
        Require(parser.BufferedBytes() == 0, "parser retained a completed frame");
    }
}

void TestCoalescedFrames() {
    auto first = EncodeFrame(PacketType::Heartbeat, 1, {});
    auto second = EncodeFrame(PacketType::WorldState, 2, {9, 8, 7});
    first.insert(first.end(), second.begin(), second.end());
    StreamFrameParser parser;
    const auto batch = parser.Feed(first);
    Require(batch && batch.frames.size() == 2, "coalesced frames were not separated");
    Require(batch.frames[0].header.type == PacketType::Heartbeat &&
                batch.frames[1].payload == std::vector<std::uint8_t>({9, 8, 7}),
            "coalesced frame order or payload changed");
}

void TestMalformedInputsAreTerminal() {
    auto badMagic = EncodeFrame(PacketType::Heartbeat, 1, {});
    badMagic[0] ^= 0xFF;
    StreamFrameParser parser;
    Require(parser.Feed(badMagic).error == ProtocolError::BadMagic,
            "bad magic was accepted");
    const auto valid = EncodeFrame(PacketType::Heartbeat, 1, {});
    Require(parser.Feed(valid).error == ProtocolError::BadMagic,
            "terminal parser error was not sticky");
    parser.Reset();
    Require(parser.Feed(valid).frames.size() == 1, "parser reset did not recover");

    auto oversized = valid;
    const auto length = static_cast<std::uint32_t>(kMaxPayloadSize + 1);
    oversized[10] = static_cast<std::uint8_t>(length >> 24U);
    oversized[11] = static_cast<std::uint8_t>(length >> 16U);
    oversized[12] = static_cast<std::uint8_t>(length >> 8U);
    oversized[13] = static_cast<std::uint8_t>(length);
    StreamFrameParser oversizedParser;
    Require(oversizedParser.Feed(oversized).error == ProtocolError::PayloadTooLarge,
            "oversized payload declaration was accepted");

    auto zeroSequence = valid;
    std::fill(zeroSequence.begin() + 14, zeroSequence.begin() + 18,
              static_cast<std::uint8_t>(0));
    StreamFrameParser sequenceParser;
    Require(sequenceParser.Feed(zeroSequence).error == ProtocolError::InvalidSequence,
            "zero sequence was accepted");
    auto unknownFlags = valid;
    unknownFlags[9] = 1;
    StreamFrameParser flagsParser;
    Require(flagsParser.Feed(unknownFlags).error == ProtocolError::UnsupportedFlags,
            "unknown protocol flags were accepted");

    auto unknownVersion = valid;
    unknownVersion[5] = static_cast<std::uint8_t>(kProtocolVersion + 1);
    StreamFrameParser versionParser;
    Require(versionParser.Feed(unknownVersion).error ==
                ProtocolError::UnsupportedVersion,
            "unsupported protocol version was accepted");

    auto unknownType = valid;
    unknownType[6] = 0x7F;
    unknownType[7] = 0xFF;
    StreamFrameParser typeParser;
    Require(typeParser.Feed(unknownType).error == ProtocolError::UnknownPacketType,
            "unknown packet type was accepted");

    StreamFrameParser nullParser;
    Require(nullParser.Feed(nullptr, 1).error == ProtocolError::InvalidPayload,
            "non-empty null input was accepted");
}

void TestCoalescedMaximumFramesStayBounded() {
    const std::vector<std::uint8_t> payload(kMaxPayloadSize, 0xA5);
    auto bytes = EncodeFrame(PacketType::WorldState, 1, payload);
    const auto second = EncodeFrame(PacketType::WorldState, 2, payload);
    bytes.insert(bytes.end(), second.begin(), second.end());

    StreamFrameParser parser;
    const auto batch = parser.Feed(bytes);
    Require(batch && batch.frames.size() == 2,
            "valid coalesced maximum-size frames were rejected");
    Require(parser.BufferedBytes() == 0, "completed coalesced frames remained buffered");

    const auto oneFrame = EncodeFrame(PacketType::WorldState, 1, payload);
    StreamFrameParser partialParser;
    const auto partial = partialParser.Feed(oneFrame.data(), oneFrame.size() - 1);
    Require(partial && partial.frames.empty(), "maximum partial frame was rejected");
    Require(partialParser.BufferedBytes() == kMaxBufferedBytes - 1,
            "maximum partial frame exceeded or escaped the hard buffer bound");
    const auto completed = partialParser.Feed(oneFrame.data() + oneFrame.size() - 1, 1);
    Require(completed && completed.frames.size() == 1,
            "maximum partial frame did not complete");
}

void TestTypedPayloadRoundtrip() {
    PlayerUpdatePacket update{0xDEADBEEFU, -0.0F, 1.25F, -17.5F, 180.0F, -45.0F};
    PlayerUpdatePacket decodedUpdate;
    Require(DeserializePlayerUpdate(SerializePlayerUpdate(update), decodedUpdate),
            "player update did not decode");
    Require(decodedUpdate.id == update.id && FloatBits(decodedUpdate.x) == FloatBits(update.x) &&
                FloatBits(decodedUpdate.pitch) == FloatBits(update.pitch),
            "player update bit representation changed");

    PlayerShootPacket shoot{42, 1, 2, 3, -1, -2, -3, -9};
    PlayerShootPacket decodedShoot;
    Require(DeserializePlayerShoot(SerializePlayerShoot(shoot), decodedShoot),
            "player shoot did not decode");
    Require(decodedShoot.id == shoot.id && decodedShoot.weaponType == shoot.weaponType &&
                decodedShoot.dirZ == shoot.dirZ,
            "player shoot roundtrip changed fields");

    auto truncated = SerializePlayerShoot(shoot);
    truncated.pop_back();
    Require(!DeserializePlayerShoot(truncated, decodedShoot),
            "truncated typed payload was accepted");

    update.x = std::numeric_limits<float>::quiet_NaN();
    Require(!DeserializePlayerUpdate(SerializePlayerUpdate(update), decodedUpdate),
            "non-finite player state was accepted");

    const std::uint64_t sessionToken = 0x0102030405060708ULL;
    const auto tokenBytes = SerializeSessionToken(sessionToken);
    Require(tokenBytes == std::vector<std::uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}),
            "session token is not in canonical network byte order");
    std::uint64_t decodedToken = 0;
    Require(DeserializeSessionToken(tokenBytes, decodedToken) && decodedToken == sessionToken,
            "session token roundtrip failed");
    Require(!DeserializeSessionToken(std::vector<std::uint8_t>(8, 0), decodedToken),
            "zero session token was accepted");
}

void TestDeterministicFragmentationStress() {
    std::vector<std::uint8_t> stream;
    std::vector<std::vector<std::uint8_t>> expected;
    for (std::uint32_t sequence = 1; sequence <= 500; ++sequence) {
        std::vector<std::uint8_t> payload(sequence % 97, static_cast<std::uint8_t>(sequence));
        expected.push_back(payload);
        auto frame = EncodeFrame(PacketType::WorldState, sequence, payload);
        stream.insert(stream.end(), frame.begin(), frame.end());
    }

    std::mt19937 random(0xA7C4U);
    StreamFrameParser parser;
    std::vector<Frame> decoded;
    for (std::size_t offset = 0; offset < stream.size();) {
        const auto chunk = std::min<std::size_t>(1 + random() % 113, stream.size() - offset);
        auto batch = parser.Feed(stream.data() + offset, chunk);
        Require(static_cast<bool>(batch), "valid fragmented stream was rejected");
        decoded.insert(decoded.end(), std::make_move_iterator(batch.frames.begin()),
                       std::make_move_iterator(batch.frames.end()));
        offset += chunk;
    }
    Require(decoded.size() == expected.size(), "fragmentation stress lost frames");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        Require(decoded[i].header.sequence == i + 1 && decoded[i].payload == expected[i],
                "fragmentation stress reordered or corrupted a frame");
    }
    Require(parser.BufferedBytes() == 0, "fragmentation stress left buffered bytes");
}

} // namespace

int main() {
    try {
        TestHeaderNetworkByteOrder();
        TestEveryTwoPartSplit();
        TestCoalescedFrames();
        TestMalformedInputsAreTerminal();
        TestCoalescedMaximumFramesStayBounded();
        TestTypedPayloadRoundtrip();
        TestDeterministicFragmentationStress();
        std::cout << "Archura network protocol tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Archura network protocol test failure: " << exception.what() << '\n';
        return 1;
    }
}
