#include "network/ServerConfig.h"

#include <filesystem>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
bool Same(const Archura::ServerConfig& a, const Archura::ServerConfig& b) {
  return a.serverName == b.serverName && a.port == b.port && a.maxPlayers == b.maxPlayers &&
      a.tickRate == b.tickRate && a.snapshotRate == b.snapshotRate && a.map == b.map &&
      a.gameMode == b.gameMode && a.password == b.password && a.clientTimeout == b.clientTimeout &&
      a.connectionTimeout == b.connectionTimeout && a.rconPassword == b.rconPassword &&
      a.enableRcon == b.enableRcon && a.verboseLogging == b.verboseLogging &&
      a.logFile == b.logFile && a.helpRequested == b.helpRequested;
}
void Write(const std::filesystem::path& path, const std::string& text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
}
bool Parse(Archura::ServerConfig& config, std::vector<std::string> args) {
  std::vector<char*> argv;
  for (auto& arg : args) argv.push_back(arg.data());
  return config.TryParseCommandLine(static_cast<int>(argv.size()), argv.data());
}
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
      ("archura-server-config-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  if (!std::filesystem::create_directory(root)) return 1;
  struct Cleanup {
    std::filesystem::path root;
    ~Cleanup() { std::error_code ec; std::filesystem::remove_all(root, ec); }
  } cleanup{root};
  const auto path = root / "server.json";
  std::error_code ec;
  std::filesystem::remove(path, ec);

  Archura::ServerConfig source;
  source.serverName = "Test \"Server\"";
  source.port = 28015;
  source.maxPlayers = 8;
  source.verboseLogging = true;
  Check(source.SaveToFile(path.string()), "server config save should succeed");

  Archura::ServerConfig loaded;
  Check(loaded.LoadFromFile(path.string()), "server config load should succeed");
  Check(loaded.serverName == source.serverName, "server name should round-trip");
  Check(loaded.port == source.port && loaded.maxPlayers == source.maxPlayers,
        "numeric settings should round-trip");
  Check(loaded.verboseLogging, "boolean settings should round-trip");

  const auto retained = loaded;
  for (const std::string& invalid : std::vector<std::string>{
           R"({"server_name":"changed","port":1})", R"({"port":28016,"port":28017})",
           R"({"port":"28016"})", R"({"port":28016.5})", R"({"map":false})",
           R"({"verbose_logging":"true"})", R"({"enable_rcon":1})", R"({"client_timeout":null})",
           R"({"port":28016} trailing})", R"({"port":28016,})", R"({"port":})", "[]",
           R"({"port":2147483648})", R"({"client_timeout":1e39})", R"({"client_timeout":NaN})",
           R"({"connection_timeout":0.09})", R"({"connection_timeout":120.1})",
           R"({"client_timeout":601})", R"({"connection_timeout":20,"client_timeout":19})",
           R"({"server_name":"\uD800"})", R"({"p\u006frt":28016,"port":28017})"}) {
    Write(path, invalid);
    Check(!loaded.LoadFromFile(path.string()), "malformed or invalid config must reject");
    Check(Same(loaded, retained), "failed load must preserve every configuration field");
  }
  Write(path, R"({"unknown":{"port":1},"connection_timeout":0.2,"client_timeout":0.7})");
  Check(loaded.LoadFromFile(path.string()), "valid partial/unknown fields remain compatible");
  Check(loaded.port == retained.port && loaded.map == retained.map &&
        loaded.connectionTimeout == 0.2f && loaded.clientTimeout == 0.7f,
        "partial configs only update explicitly declared root fields");
  Write(path, std::string(1024 * 1024 + 1, ' '));
  const auto partial = loaded;
  Check(!loaded.LoadFromFile(path.string()) && Same(loaded, partial), "oversized load preserves config");

  Archura::ServerConfig limits;
  for (float timeout : {std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN(), -1.0f, 0.0f, 601.0f}) {
    limits.clientTimeout = timeout;
    Check(!limits.Validate(), "unsafe client timeouts reject before millisecond conversion");
  }
  limits = {};
  limits.connectionTimeout = 0.1f;
  limits.clientTimeout = 0.1f;
  Check(limits.Validate(), "minimum timeout boundaries remain valid");
  limits.connectionTimeout = std::nextafter(0.1f, 0.0f);
  Check(!limits.Validate(), "connection timeout below minimum rejects");
  limits.connectionTimeout = 120.0f;
  limits.clientTimeout = 600.0f;
  Check(limits.Validate(), "maximum timeout boundaries remain valid");
  limits.connectionTimeout = std::nextafter(120.0f, 121.0f);
  Check(!limits.Validate(), "connection timeout above maximum rejects");
  limits.connectionTimeout = 120.0f;
  limits.clientTimeout = std::nextafter(600.0f, 601.0f);
  Check(!limits.Validate(), "client timeout above maximum rejects");

  source.serverName = "Escaped\tName\nLine\r\\\"";
  source.password = "control\b\f";
  source.connectionTimeout = 10.123456f;
  Check(source.SaveToFile(path.string()), "escaped strings and precise floats save");
  loaded = {};
  Check(loaded.LoadFromFile(path.string()) && Same(source, loaded), "all saved fields round-trip exactly");
  source.clientTimeout = std::numeric_limits<float>::infinity();
  Check(!source.SaveToFile(path.string()), "invalid save must reject");
  Archura::ServerConfig preserved;
  Check(preserved.LoadFromFile(path.string()) && Same(preserved, loaded), "invalid save preserves prior file");
  const auto backup = std::filesystem::path(path.string() + ".bak");
  std::filesystem::remove(backup);
  std::filesystem::create_directory(backup);
  source = loaded;
  source.serverName = "must not publish";
  Check(!source.SaveToFile(path.string()), "backup failure must reject replacement");
  Check(preserved.LoadFromFile(path.string()) && Same(preserved, loaded), "failed replacement preserves prior file");
  std::filesystem::remove(backup);
  source.serverName = std::string(1, static_cast<char>(0xff));
  Check(!source.SaveToFile(path.string()), "invalid UTF-8 must not be published");

  Write(path, R"({"port":28015,"max_players":8,"server_name":"File","connection_timeout":0.2,"client_timeout":0.7})");
  Archura::ServerConfig cliBefore, cliAfter;
  Check(Parse(cliBefore, {"server", "--port", "28016", "--config", path.string(), "--verbose"}),
        "CLI overrides before config option succeed");
  Check(Parse(cliAfter, {"server", "--config", path.string(), "--port", "28016", "--verbose"}),
        "CLI overrides after config option succeed");
  Check(Same(cliBefore, cliAfter) && cliAfter.port == 28016 && cliAfter.maxPlayers == 8 &&
        cliAfter.verboseLogging, "CLI wins over file regardless of option order");
  const auto cliRetained = cliAfter;
  for (auto args : std::vector<std::vector<std::string>>{
           {"server", "--config", path.string(), "--port", "28016junk"},
           {"server", "--port", "9999999999999999999999"}, {"server", "--tickrate", "128x"},
           {"server", "--maxplayers", "2.5"}, {"server", "--port", "1"},
           {"server", "--config", (root / "missing.json").string()},
           {"server", "--config", path.string(), "--config", path.string()},
           {"server", "--port"}, {"server", "--config"}, {"server", "--name"},
           {"server", "--unknown"}, {"server", "--help", "--unknown"}}) {
    Check(!Parse(cliAfter, args), "invalid CLI must report failure");
    Check(Same(cliAfter, cliRetained), "failed CLI preserves entire original config");
  }
  Write(path, R"({"port":1})");
  Check(!Parse(cliAfter, {"server", "--config", path.string(), "--port", "28016"}) &&
        Same(cliAfter, cliRetained), "CLI cannot mask an invalid explicit config file");
  Check(Parse(cliAfter, {"server", "--name", "--config"}) && cliAfter.serverName == "--config",
        "option-like string values must not become config directives");
  Check(Parse(cliAfter, {"server", "--port", "28017", "--port", "28018"}) && cliAfter.port == 28018,
        "repeated ordinary overrides retain last-value precedence");
  Check(Parse(cliAfter, {"server", "--help"}) && cliAfter.helpRequested, "help remains supported");
  Check(Parse(cliAfter, {"server"}) && !cliAfter.helpRequested, "successful reparse clears help state");
  auto wrapperExpected = cliAfter;
  char program[] = "server", flag[] = "--port", bad[] = "bad";
  char* wrapperArgs[] = {program, flag, bad};
  cliAfter.ParseCommandLine(3, wrapperArgs);
  Check(Same(cliAfter, wrapperExpected), "legacy void wrapper also preserves config on failure");

  std::filesystem::remove(path, ec);
  return failures == 0 ? 0 : 1;
}
