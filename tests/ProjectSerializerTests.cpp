#include "core/ProjectSerializer.h"
#include "ecs/Component.h"
#include "rendering/Mesh.h"
#include "game/AudioSource.h"
#include "ecs/components/AudioSourceComponent.h"
#include "core/SerializationJson.h"
#include <limits>
#include <sstream>

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
  entity->AddComponent<Archura::MeshRenderer>()->SetMeshAsset(Archura::Mesh::CreateCapsuleShared(0.75f, 3.25f));
  entity->AddComponent<Archura::AudioSource>()->clipName = "ambient.wav";
  entity->GetComponent<Archura::AudioSource>()->isPlaying = true;
  entity->GetComponent<Archura::AudioSource>()->runtimeAlias = "old alias";
  entity->AddComponent<Archura::AudioSourceComponent>()->pitch = 1.25f;
  entity->AddComponent<Archura::RigidBody>()->mass = 7.5f;
  entity->AddComponent<Archura::Health>()->current = 23.0f;
  entity->AddComponent<Archura::ScriptComponent>()->className = "Gameplay.Test";
  auto* child = source.CreateEntity("child");
  child->TrySetParent(entity);
  child->AddComponent<Archura::LightComponent>()->intensity = 4.0f;
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
  Check(loadedScene.GetEntities().size() == 2,
        "project entities should round-trip");
  Check(loadedScene.GetEntities()[0]->GetName() == "quoted \"entity\"",
        "entity names should be escaped and restored");

  const auto& meshSource = loadedScene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>()->GetMesh()->GetSource();
  Check(meshSource.type == Archura::MeshSourceType::Capsule && meshSource.radius == 0.75f && meshSource.height == 3.25f, "project preserves mesh recipe");
  const auto* audio = loadedScene.GetEntities()[0]->GetComponent<Archura::AudioSource>();
  Check(audio && audio->clipName == "ambient.wav" && !audio->isPlaying && audio->runtimeAlias.empty(), "audio configuration round trips and runtime state resets");
  Check(loadedScene.GetEntities()[0]->GetComponent<Archura::AudioSourceComponent>()->pitch == 1.25f, "optional audio component config round trips");
  Check(loadedScene.GetEntities()[0]->GetComponent<Archura::RigidBody>()->mass == 7.5f, "rigid body mass round trips");
  Check(loadedScene.GetEntities()[0]->GetComponent<Archura::ScriptComponent>()->className == "Gameplay.Test", "script class round trips");
  Check(loadedScene.GetEntities()[1]->GetParent() == loadedScene.GetEntities()[0].get(), "hierarchy restored");
  Check(loadedScene.GetEntities()[1]->GetComponent<Archura::LightComponent>()->intensity == 4.0f, "light round trips");

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

  auto write = [&](const std::string& text) { std::ofstream f(path, std::ios::binary); f << text; };
  const std::string compact = R"({"project":{"name":"compact","version":"1"},"entities":[{"name":"first","transform":{"position":[1,2,3]}}]})";
  write(compact);
  Check(Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene), "compact valid JSON loads");
  Check(loadedScene.GetEntities().size() == 1, "compact scene contains expected entity");
  const auto sentinel = loadedScene.GetEntities()[0]->GetHandle();
  const std::string invalid[] = {
    R"({"project":{"name":"bad"},"entities":[{"name":"a","transform":{"position":[1,2,3,4]}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","transform":{"position":[1 2 3]}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","transform":{"position":[1,2,3}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","name":"b"}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a"},]})",
    R"({"project":{"name":"bad"},"entities":[]} trailing)",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","id":1,"parent_id":2},{"name":"b","id":2,"parent_id":1}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"future_component":{}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","transform":{"scale":[1e90,1,1]}}]})"
  };
  for (const auto& text : invalid) {
    write(text);
    Check(!Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene), "malformed/schema-invalid project rejects");
    Check(loadedConfig.name == "compact", "failed load preserves config");
    Check(loadedScene.GetEntities().size() == 1 && loadedScene.IsAlive(sentinel), "failed load preserves original entity handles");
  }
  Archura::Serialization::Json parsed;
  Check(Archura::Serialization::JsonParser(R"({"unicode":"\uD83D\uDE00","control":"\u0001"})").Parse(parsed), "unicode surrogate pairs and escaped controls parse");
  Check(!Archura::Serialization::JsonParser(R"({"unicode":"\uD800"})").Parse(parsed), "unpaired surrogate rejects");
  Check(!Archura::Serialization::JsonParser(R"({"number":01})").Parse(parsed), "leading zero rejects");
  Check(!Archura::Serialization::JsonParser(R"({"number":1e})").Parse(parsed), "missing exponent rejects");
  Check(Archura::ProjectSerializer::SaveProject(path.string(), config, &source), "first atomic save succeeds");
  std::ifstream before(path, std::ios::binary); std::ostringstream saved; saved << before.rdbuf(); before.close();
  Check(Archura::ProjectSerializer::SaveProject(path.string(), config, &source), "replacement save succeeds");
  Check(std::filesystem::exists(path.string() + ".bak"), "previous save retained as backup");
  std::ifstream backup(path.string() + ".bak", std::ios::binary); std::ostringstream backupText; backupText << backup.rdbuf(); backup.close();
  Check(saved.str() == backupText.str(), "backup retains preceding successful contents");
  entity->GetComponent<Archura::Transform>()->position.x = std::numeric_limits<float>::quiet_NaN();
  Check(!Archura::ProjectSerializer::SaveProject(path.string(), config, &source), "non-finite scene rejects save");
  std::ifstream after(path, std::ios::binary); std::ostringstream retained; retained << after.rdbuf(); after.close();
  Check(retained.str() == saved.str(), "failed save preserves previous destination");
  std::filesystem::remove(path, ec);
  std::filesystem::remove(path.string() + ".bak", ec);
  return failures == 0 ? 0 : 1;
}
