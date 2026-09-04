#include "network/ServerConfig.h"

#include <filesystem>
#include <iostream>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
} // namespace

int main() {
  const auto path = std::filesystem::temp_directory_path() /
                    "archura_server_config_test.json";
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

  std::filesystem::remove(path, ec);
  return failures == 0 ? 0 : 1;
}
