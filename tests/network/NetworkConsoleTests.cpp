#include "network/NetworkConsole.h"
#include "network/NetworkManager.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}

void RequireInvalid(std::string_view endpoint) {
  const auto parsed = Archura::ParseNetworkEndpoint(endpoint);
  Require(!parsed.valid, "malformed endpoint was accepted");
  Require(!parsed.error.empty(), "invalid endpoint did not explain failure");
}

void TestEndpointParser() {
  auto parsed = Archura::ParseNetworkEndpoint("localhost:27015");
  Require(parsed.valid, "valid hostname endpoint was rejected");
  Require(parsed.endpoint.host == "localhost", "hostname parsed incorrectly");
  Require(parsed.endpoint.port == 27015, "port parsed incorrectly");
  Require(Archura::FormatNetworkEndpoint(parsed.endpoint) == "localhost:27015",
          "hostname endpoint formatted incorrectly");

  parsed = Archura::ParseNetworkEndpoint("127.0.0.1:1");
  Require(parsed.valid && parsed.endpoint.port == 1,
          "minimum valid port was rejected");

  parsed = Archura::ParseNetworkEndpoint("example.com:65535");
  Require(parsed.valid && parsed.endpoint.port == 65535,
          "maximum valid port was rejected");

  parsed = Archura::ParseNetworkEndpoint("[::1]:443");
  Require(parsed.valid && parsed.endpoint.host == "::1" &&
              parsed.endpoint.port == 443,
          "bracketed IPv6 endpoint parsed incorrectly");
  Require(Archura::FormatNetworkEndpoint(parsed.endpoint) == "[::1]:443",
          "IPv6 endpoint formatted incorrectly");

  const std::string_view invalidEndpoints[] = {
      "",          "localhost",   ":27015",       "localhost:",
      "host:0",    "host:65536",  "host:-1",      "host:+80",
      "host:80x",  "host:1:2",    "host name:80", "[]:80",
      "[::1]80",   "[::1]:",      "[::1]:65536"};
  for (const auto endpoint : invalidEndpoints)
    RequireInvalid(endpoint);
}

void TestStateFormatting() {
  using Archura::ConnectionState;
  Require(Archura::ConnectionStateName(ConnectionState::Stopped) == "Stopped",
          "Stopped state label is wrong");
  Require(Archura::ConnectionStateName(ConnectionState::Listening) == "Listening",
          "Listening state label is wrong");
  Require(Archura::ConnectionStateName(ConnectionState::Handshaking) ==
              "Handshaking",
          "Handshaking state label is wrong");
  Require(Archura::ConnectionStateName(ConnectionState::Connected) == "Connected",
          "Connected state label is wrong");
  Require(Archura::ConnectionStateName(ConnectionState::ShuttingDown) ==
              "ShuttingDown",
          "ShuttingDown state label is wrong");
  Require(Archura::ConnectionStateName(ConnectionState::Failed) == "Failed",
          "Failed state label is wrong");

  Archura::NetworkRuntimeSnapshot sampleSnapshot;
  sampleSnapshot.state = ConnectionState::Listening;
  sampleSnapshot.lastDisconnectReason = Archura::DisconnectReason::PeerClosed;
  sampleSnapshot.stats.connectedPeers = 3;
  sampleSnapshot.lastError = "socket detail";
  const std::string status = Archura::FormatNetworkStatus(sampleSnapshot);
  Require(status.find("Connection state: Listening") != std::string::npos,
          "status omitted actual state");
  Require(status.find("Last disconnect reason: PeerClosed") !=
              std::string::npos,
          "status omitted disconnect reason");
  Require(status.find("Connected peers: 3") != std::string::npos,
          "status omitted peer count");
  Require(status.find("Last error: socket detail") != std::string::npos,
          "status omitted error");
  Require(status.find("192.168.1.100") == std::string::npos,
          "status retained fabricated server address");
  Require(status.find("de_mirage") == std::string::npos,
          "status retained fabricated map");
  Require(status.find("32/32") == std::string::npos,
          "status retained fabricated player count");

  const Archura::NetworkEndpoint endpoint{"example.com", 27015};
  Archura::NetworkRuntimeSnapshot openedSnapshot;
  openedSnapshot.state = ConnectionState::Handshaking;
  const std::string opened = Archura::FormatNetworkConnectResult(
      endpoint, true, openedSnapshot);
  Require(opened.find("Transport opened to example.com:27015") !=
              std::string::npos,
          "open transport was not reported");
  Require(opened.find("Connection state: Handshaking") != std::string::npos,
          "open transport omitted handshake state");
  Require(opened.find("Connected successfully") == std::string::npos,
          "open transport was mislabeled as a completed handshake");
}

void TestExactStatsFormatting() {
  Archura::NetworkStats stats;
  stats.bytesReceived = 101;
  stats.bytesSent = 202;
  stats.framesReceived = 303;
  stats.framesSent = 404;
  stats.rejectedConnections = 5;
  stats.connectedPeers = 6;
  stats.queuedBytes = 707;

  const std::string report = Archura::FormatNetworkStats(stats);
  Require(report.find("Bytes received: 101") != std::string::npos,
          "bytes received were not exact");
  Require(report.find("Bytes sent: 202") != std::string::npos,
          "bytes sent were not exact");
  Require(report.find("Frames received: 303") != std::string::npos,
          "frames received were not exact");
  Require(report.find("Frames sent: 404") != std::string::npos,
          "frames sent were not exact");
  Require(report.find("Rejected connections: 5") != std::string::npos,
          "rejected connections were not exact");
  Require(report.find("Connected peers: 6") != std::string::npos,
          "connected peers were not exact");
  Require(report.find("Queued bytes: 707") != std::string::npos,
          "queued bytes were not exact");

  const char* fabricatedLabels[] = {
      "Ping:", "Packet Loss:", "Download:", "Upload:", "Mbps"};
  for (const char* label : fabricatedLabels)
    Require(report.find(label) == std::string::npos,
            "stats retained a fabricated metric");
}

void TestNoBackendConnectAndDisconnectAreTruthful() {
  Archura::NetworkManager& network = Archura::NetworkManager::Get();
  network.Shutdown();

  const auto parsed = Archura::ParseNetworkEndpoint("127.0.0.1:27015");
  Require(parsed.valid, "integration endpoint did not parse");
  const bool transportOpened = network.Connect(
      parsed.endpoint.host, static_cast<int>(parsed.endpoint.port));
  const Archura::NetworkRuntimeSnapshot failedSnapshot =
      network.GetRuntimeSnapshot();
  Require(!transportOpened, "no-backend transport unexpectedly opened");
  Require(failedSnapshot.state == Archura::ConnectionState::Failed,
          "no-backend connection did not expose Failed state");
  Require(failedSnapshot.lastDisconnectReason ==
              Archura::DisconnectReason::Unavailable,
          "no-backend snapshot omitted Unavailable reason");
  Require(failedSnapshot.stats.connectedPeers == 0,
          "no-backend snapshot reported connected peers");
  Require(!failedSnapshot.lastError.empty(),
          "no-backend connection did not expose its error");

  const std::string connectReport = Archura::FormatNetworkConnectResult(
      parsed.endpoint, transportOpened, failedSnapshot);
  Require(connectReport.find("Transport open failed") != std::string::npos,
          "failed connection was not labeled as failure");
  Require(connectReport.find("Connection state: Failed") != std::string::npos,
          "failed connection omitted manager state");
  Require(connectReport.find(failedSnapshot.lastError) != std::string::npos,
          "failed connection omitted manager error");
  Require(connectReport.find("Transport opened to") == std::string::npos,
          "failed connection claimed the transport opened");
  Require(connectReport.find("Connected successfully") == std::string::npos,
          "failed connection claimed success");

  network.Shutdown();
  const Archura::NetworkRuntimeSnapshot shutdownSnapshot =
      network.GetRuntimeSnapshot();
  Require(shutdownSnapshot.state == Archura::ConnectionState::Failed,
          "no-backend shutdown erased the truthful failure state");
  Require(shutdownSnapshot.lastDisconnectReason ==
              Archura::DisconnectReason::Unavailable,
          "no-backend shutdown changed the snapshot disconnect reason");
  Require(shutdownSnapshot.stats.connectedPeers == 0,
          "no-backend shutdown reported connected peers");
  Require(shutdownSnapshot.lastError == failedSnapshot.lastError,
          "no-backend shutdown changed the snapshot error");
  const std::string shutdownStatus =
      Archura::FormatNetworkStatus(shutdownSnapshot);
  Require(shutdownStatus.find("Connection state: Failed") != std::string::npos,
          "post-shutdown status fabricated a stopped connection");
  Require(shutdownStatus.find("Connected peers: 0") != std::string::npos,
          "post-shutdown status omitted actual peer count");
}

void TestInertNetworkVariablesAreNotRegistered() {
#if !defined(ARCHURA_SOURCE_DIR)
#error ARCHURA_SOURCE_DIR must identify the source tree for this contract test
#endif
  const std::string path =
      std::string(ARCHURA_SOURCE_DIR) + "/src/game/FPSConsoleCommands.cpp";
  std::ifstream source(path, std::ios::in | std::ios::binary);
  Require(static_cast<bool>(source), "could not inspect console command source");
  const std::string contents((std::istreambuf_iterator<char>(source)),
                             std::istreambuf_iterator<char>());
  const char* inertVariables[] = {
      "net_lerp", "net_lag_compensate", "cl_predict_correct",
      "net_stats_display"};
  for (const char* variable : inertVariables)
    Require(contents.find(variable) == std::string::npos,
            "inert network variable registration returned");
}

} // namespace

int main() {
  try {
    TestEndpointParser();
    TestStateFormatting();
    TestExactStatsFormatting();
    TestNoBackendConnectAndDisconnectAreTruthful();
    TestInertNetworkVariablesAreNotRegistered();
    std::cout << "Network console tests passed\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "Network console test failure: " << exception.what() << '\n';
    return 1;
  }
}
