#include "core/ProjectSerializer.h"
#include "ecs/Component.h"

#include <filesystem>
#include <fstream>
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
                    "archura_project_serializer_test.json";
  std::error_code ec;
  std::filesystem::remove(path, ec);

  Archura::Scene source("source");
  auto* entity = source.CreateEntity("quoted \"entity\"");
  entity->GetComponent<Archura::Transform>()->position = {1.0f, 2.0f, 3.0f};
  Archura::ProjectConfig config{"Test Project", "1.0", "start.scene"};
  Check(Archura::ProjectSerializer::SaveProject(path.string(), config, &source),
        "project save should succeed");

  Archura::Scene loadedScene("loaded");
  loadedScene.CreateEntity("sentinel");
  Archura::ProjectConfig loadedConfig;
  Check(Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig,
                                                &loadedScene),
        "project load should succeed");
  Check(loadedConfig.name == config.name, "project name should round-trip");
  Check(loadedScene.GetEntities().size() == 1,
        "project entities should round-trip");
  Check(loadedScene.GetEntities()[0]->GetName() == "quoted \"entity\"",
        "entity names should be escaped and restored");

  {
    std::ofstream malformed(path, std::ios::binary);
    malformed << "not json";
  }
  loadedScene.CreateEntity("must survive malformed load");
  const size_t countBefore = loadedScene.GetEntities().size();
  Check(!Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig,
                                                  &loadedScene),
        "malformed project should fail");
  Check(loadedScene.GetEntities().size() == countBefore,
        "malformed load must not mutate the scene");

  std::filesystem::remove(path, ec);
  return failures == 0 ? 0 : 1;
}
