#pragma once

#include "NetworkManager.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Archura {

struct NetworkEndpoint {
    std::string host;
    std::uint16_t port = 0;
};

struct NetworkEndpointParseResult {
    bool valid = false;
    NetworkEndpoint endpoint;
    std::string error;
};

NetworkEndpointParseResult ParseNetworkEndpoint(std::string_view text);
std::string FormatNetworkEndpoint(const NetworkEndpoint& endpoint);
std::string_view ConnectionStateName(ConnectionState state) noexcept;
std::string_view DisconnectReasonName(DisconnectReason reason) noexcept;
std::string FormatNetworkStatus(const NetworkRuntimeSnapshot& snapshot);
std::string FormatNetworkStats(const NetworkStats& stats);
std::string FormatNetworkConnectResult(const NetworkEndpoint& endpoint,
                                       bool transportOpened,
                                       const NetworkRuntimeSnapshot& snapshot);

} // namespace Archura
