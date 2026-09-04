#include "NetworkConsole.h"

#include <cctype>
#include <limits>
#include <sstream>

namespace Archura {
namespace {

bool HasWhitespace(std::string_view text) {
    for (const unsigned char character : text) {
        if (std::isspace(character) != 0)
            return true;
    }
    return false;
}

bool ParsePort(std::string_view text, std::uint16_t& port) {
    if (text.empty())
        return false;

    unsigned int value = 0;
    for (const unsigned char character : text) {
        if (!std::isdigit(character))
            return false;
        value = value * 10U + static_cast<unsigned int>(character - '0');
        if (value > std::numeric_limits<std::uint16_t>::max())
            return false;
    }
    if (value == 0)
        return false;
    port = static_cast<std::uint16_t>(value);
    return true;
}

} // namespace

NetworkEndpointParseResult ParseNetworkEndpoint(std::string_view text) {
    NetworkEndpointParseResult result;
    if (text.empty()) {
        result.error = "endpoint is empty; expected host:port";
        return result;
    }
    if (HasWhitespace(text)) {
        result.error = "endpoint must not contain whitespace";
        return result;
    }

    std::string_view host;
    std::string_view portText;
    if (text.front() == '[') {
        const std::size_t closingBracket = text.find(']');
        if (closingBracket == std::string_view::npos || closingBracket == 1 ||
            closingBracket + 1 >= text.size() ||
            text[closingBracket + 1] != ':') {
            result.error = "malformed bracketed host; expected [host]:port";
            return result;
        }
        host = text.substr(1, closingBracket - 1);
        portText = text.substr(closingBracket + 2);
    } else {
        const std::size_t separator = text.find(':');
        if (separator == std::string_view::npos || separator == 0 ||
            text.find(':', separator + 1) != std::string_view::npos) {
            result.error = "malformed endpoint; expected host:port";
            return result;
        }
        host = text.substr(0, separator);
        portText = text.substr(separator + 1);
    }

    std::uint16_t port = 0;
    if (host.empty() || !ParsePort(portText, port)) {
        result.error = "port must be an integer from 1 through 65535";
        return result;
    }

    result.valid = true;
    result.endpoint.host.assign(host.begin(), host.end());
    result.endpoint.port = port;
    return result;
}

std::string FormatNetworkEndpoint(const NetworkEndpoint& endpoint) {
    std::ostringstream formatted;
    if (endpoint.host.find(':') != std::string::npos)
        formatted << '[' << endpoint.host << ']';
    else
        formatted << endpoint.host;
    formatted << ':' << endpoint.port;
    return formatted.str();
}

std::string_view ConnectionStateName(ConnectionState state) noexcept {
    switch (state) {
    case ConnectionState::Stopped: return "Stopped";
    case ConnectionState::Listening: return "Listening";
    case ConnectionState::Handshaking: return "Handshaking";
    case ConnectionState::Connected: return "Connected";
    case ConnectionState::ShuttingDown: return "ShuttingDown";
    case ConnectionState::Failed: return "Failed";
    }
    return "Unknown";
}

std::string_view DisconnectReasonName(DisconnectReason reason) noexcept {
    switch (reason) {
    case DisconnectReason::None: return "None";
    case DisconnectReason::LocalShutdown: return "LocalShutdown";
    case DisconnectReason::PeerClosed: return "PeerClosed";
    case DisconnectReason::PeerRequested: return "PeerRequested";
    case DisconnectReason::TransportError: return "TransportError";
    case DisconnectReason::ProtocolViolation: return "ProtocolViolation";
    case DisconnectReason::HandshakeTimeout: return "HandshakeTimeout";
    case DisconnectReason::IdleTimeout: return "IdleTimeout";
    case DisconnectReason::Backpressure: return "Backpressure";
    case DisconnectReason::Unavailable: return "Unavailable";
    }
    return "Unknown";
}

std::string FormatNetworkStatus(const NetworkRuntimeSnapshot& snapshot) {
    std::ostringstream report;
    report << "=== Network Status ===\n";
    report << "Connection state: " << ConnectionStateName(snapshot.state) << '\n';
    report << "Last disconnect reason: "
           << DisconnectReasonName(snapshot.lastDisconnectReason) << '\n';
    report << "Connected peers: " << snapshot.stats.connectedPeers << '\n';
    report << "Last error: "
           << (snapshot.lastError.empty() ? "none reported" : snapshot.lastError)
           << '\n';
    report << "======================\n";
    return report.str();
}

std::string FormatNetworkStats(const NetworkStats& stats) {
    std::ostringstream report;
    report << "=== Network Transport Statistics ===\n";
    report << "Bytes received: " << stats.bytesReceived << '\n';
    report << "Bytes sent: " << stats.bytesSent << '\n';
    report << "Frames received: " << stats.framesReceived << '\n';
    report << "Frames sent: " << stats.framesSent << '\n';
    report << "Rejected connections: " << stats.rejectedConnections << '\n';
    report << "Connected peers: " << stats.connectedPeers << '\n';
    report << "Queued bytes: " << stats.queuedBytes << '\n';
    report << "====================================\n";
    return report.str();
}

std::string FormatNetworkConnectResult(const NetworkEndpoint& endpoint,
                                       bool transportOpened,
                                       const NetworkRuntimeSnapshot& snapshot) {
    std::ostringstream report;
    if (transportOpened) {
        report << "[Network] Transport opened to "
               << FormatNetworkEndpoint(endpoint) << '\n';
    } else {
        report << "[Network] Transport open failed for "
               << FormatNetworkEndpoint(endpoint) << '\n';
    }
    report << "[Network] Connection state: "
           << ConnectionStateName(snapshot.state) << '\n';
    report << "[Network] Last disconnect reason: "
           << DisconnectReasonName(snapshot.lastDisconnectReason) << '\n';
    report << "[Network] Last error: "
           << (snapshot.lastError.empty() ? "none reported" : snapshot.lastError)
           << '\n';
    return report.str();
}

} // namespace Archura
