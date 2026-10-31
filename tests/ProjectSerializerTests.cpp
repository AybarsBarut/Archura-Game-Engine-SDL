#include "core/ProjectSerializer.h"
#include "core/GameSaveManager.h"
#include "core/ResourceManager.h"
#include "ecs/Component.h"
#include "rendering/Mesh.h"
#include "rendering/Texture.h"
#include "game/AudioSource.h"
#include "game/Weapon.h"
#include "ecs/components/AudioSourceComponent.h"
#include "core/SerializationJson.h"
#include <limits>
#include <sstream>
#include <iomanip>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void TestExplicitContentRoot() {
  namespace fs = std::filesystem;
  auto& resources = Archura::ResourceManager::Get();
  struct RestoreState {
    fs::path cwd = fs::current_path();
    fs::path root = Archura::ResourceManager::Get().GetProjectRoot();
    bool explicitRoot = Archura::ResourceManager::Get().HasExplicitContentRoot();
    ~RestoreState() {
      std::error_code error;
      fs::current_path(cwd, error);
      Archura::ResourceManager::Get().SwapProjectRoot(root, explicitRoot);
    }
  } restore;
  struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
      const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
      for (unsigned attempt = 0; attempt != 64; ++attempt) {
        auto candidate = fs::temp_directory_path() /
            ("archura-explicit-root-" + std::to_string(stamp) + "-" + std::to_string(attempt));
        if (fs::create_directory(candidate)) { path = std::move(candidate); return; }
      }
      throw std::runtime_error("could not create explicit-root test directory");
    }
    ~TemporaryDirectory() {
      std::error_code error;
      fs::current_path(path.parent_path(), error);
      fs::remove_all(path, error); // Only this exact, exclusively created fixture is owned.
    }
  } temporary;
  const auto contentRoot = temporary.path / "workspace" / "content";
  const auto sceneFolder = temporary.path / "workspace" / "scenes";
  const auto wrongCwd = temporary.path / "unrelated";
  const auto previousRoot = temporary.path / "previous";
  const auto missingRoot = temporary.path / "missing-assets";
  for (const auto& directory : {contentRoot, wrongCwd}) {
    fs::create_directories(directory / "models");
    fs::create_directories(directory / "textures");
    std::ofstream model(directory / "models" / "fixture.obj", std::ios::binary);
    model << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::ofstream texture(directory / "textures" / "fixture.ppm", std::ios::binary);
    const int width = directory == contentRoot ? 2 : 1;
    texture << "P6\n" << width << " 1\n255\n";
    for (int pixel = 0; pixel != width; ++pixel) texture.write("\x01\x02\x03", 3);
  }
  fs::create_directories(sceneFolder);
  fs::create_directory(previousRoot);
  fs::create_directory(missingRoot);
  const auto document = sceneFolder / "Main.gameproj";
  {
    std::ofstream scene(document, std::ios::binary);
    scene << R"({"project":{"name":"explicit assets","version":"1"},"entities":[{"name":"fixture","has_mesh_renderer":true,"mesh_type":"obj","model_path":"models/fixture.obj","texture":"textures/fixture.ppm"}]})";
  }
  fs::current_path(wrongCwd);
  resources.SetProjectRoot(previousRoot, true);
  Archura::Scene scene;
  scene.CreateEntity("original");
  Archura::ProjectConfig config{"original config", "original version", "original scene"};
  Check(Archura::ProjectSerializer::LoadProject(document.string(), config, &scene, contentRoot),
        "explicit project loads relative OBJ and texture from an unrelated working directory");
  Check(config.name == "explicit assets" && scene.GetEntities().size() == 1,
        "explicit project commits scene and config");
  auto* renderer = scene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>();
  Check(renderer && renderer->GetMesh() && renderer->GetTexture() && renderer->GetTexture()->GetWidth() == 2,
        "explicit asset root wins over a valid conflicting working-directory texture");
  Check(resources.GetProjectRoot() == fs::weakly_canonical(contentRoot) && resources.HasExplicitContentRoot(),
        "successful explicit load commits its content root and resolution mode");
  resources.SetProjectRoot(previousRoot, true);
  const auto originalHandle = scene.GetEntities()[0]->GetHandle();
  const auto originalName = config.name;
  Check(!Archura::ProjectSerializer::LoadProject(document.string(), config, &scene, missingRoot),
        "missing explicit project asset cannot fall back to document, old project or working directory");
  Check(scene.GetEntities().size() == 1 && scene.IsAlive(originalHandle) && config.name == originalName,
        "failed explicit load preserves scene generation and project config");
  Check(resources.GetProjectRoot() == fs::weakly_canonical(previousRoot) && resources.HasExplicitContentRoot(),
        "failed explicit load restores prior root and explicit resolution mode");
  Check(resources.ResolveAssetPath("models/fixture.obj", contentRoot, true).empty(),
        "authoritative root ignores a supplied document root with matching assets");
  Check(resources.ResolveAssetPath("models/fixture.obj", contentRoot, false) == previousRoot / "models/fixture.obj",
        "nonexistent explicit asset identity remains rooted in that project");
  Check(resources.ResolveAssetPath((contentRoot / "models/fixture.obj").string(), {}, true) == contentRoot / "models/fixture.obj",
        "explicit authoring continues to support absolute asset references");
  const auto saveAs = temporary.path / "SaveAs.gameproj";
  Check(Archura::ProjectSerializer::SaveProject(saveAs.string(), config, &scene, contentRoot),
        "explicit Save As validates relative asset references against its content root");
  Check(resources.GetProjectRoot() == fs::weakly_canonical(previousRoot) && resources.HasExplicitContentRoot(),
        "successful explicit save restores the previous resolution context");
  const auto readSaved = [&] {
    std::ifstream file(saveAs, std::ios::binary);
    std::ostringstream text; text << file.rdbuf(); return text.str();
  };
  const auto savedText = readSaved();
  Check(!Archura::ProjectSerializer::SaveProject(saveAs.string(), config, &scene, missingRoot),
        "explicit save rejects missing assets despite valid working-directory assets");
  Check(readSaved() == savedText && scene.IsAlive(originalHandle),
        "failed explicit save preserves the previous file and live scene handles");
  Check(resources.GetProjectRoot() == fs::weakly_canonical(previousRoot) && resources.HasExplicitContentRoot(),
        "failed explicit save restores previous resolution context");
  Check(!Archura::ProjectSerializer::LoadProject(document.string(), config, &scene, temporary.path / "absent"),
        "explicit serializer rejects a nonexistent content root");
  Check(Archura::ProjectSerializer::LoadProject(document.string(), config, &scene),
        "legacy scene load disables previous session's explicit root and retains working-directory fallback");
  renderer = scene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>();
  Check(renderer && renderer->GetTexture() && renderer->GetTexture()->GetWidth() == 1 &&
            resources.GetProjectRoot() == fs::weakly_canonical(sceneFolder) && !resources.HasExplicitContentRoot(),
        "successful legacy load commits the document directory and ordinary legacy resolution");
  const auto unicodeFolder = temporary.path / fs::u8path(u8"Proje-\u0130\u015f-\u6e38\u620f");
  const auto unicodeDocument = unicodeFolder / fs::u8path(u8"Sahne-\u015e\u00e9-\u573a\u666f.gameproj");
  fs::create_directory(unicodeFolder);
  Archura::Scene unicodeSource;
  auto* unicodeEntity = unicodeSource.CreateEntity(u8"Nesne-\u0130\u015f-\u6e38\u620f");
  unicodeEntity->GetComponent<Archura::Transform>()->position = {2, 4, 6};
  Archura::ProjectConfig unicodeConfig{"Native path project", "1", "MainScene"};
  Check(Archura::ProjectSerializer::SaveProjectPath(unicodeDocument, unicodeConfig, &unicodeSource, unicodeFolder),
        "native project path saves a scene below Turkish and CJK directory and file names");
  Check(fs::is_regular_file(unicodeDocument), "native project save produces the exact Unicode file name");
  Archura::Scene unicodeLoaded;
  Archura::ProjectConfig unicodeLoadedConfig;
  Check(Archura::ProjectSerializer::LoadProjectPath(unicodeDocument, unicodeLoadedConfig, &unicodeLoaded, unicodeFolder),
        "native project path reopens a Unicode scene from unrelated working directory");
  Check(unicodeLoadedConfig.name == unicodeConfig.name && unicodeLoaded.GetEntities().size() == 1 &&
            unicodeLoaded.GetEntities()[0]->GetName() == unicodeEntity->GetName() &&
            unicodeLoaded.GetEntities()[0]->GetComponent<Archura::Transform>()->position == glm::vec3(2, 4, 6),
        "native Unicode path round trip retains authored data");
  Check(resources.GetProjectRoot() == fs::weakly_canonical(unicodeFolder) && resources.HasExplicitContentRoot(),
        "native Unicode scene load commits the native content-root identity");
  Check(Archura::ProjectSerializer::SaveProjectPath(unicodeDocument, unicodeConfig, &unicodeSource),
        "native path default save preserves the active explicit context");
  auto unicodeBackup = unicodeDocument; unicodeBackup += ".bak";
  Check(fs::is_regular_file(unicodeBackup), "native Unicode replacement save retains a native-path backup");
  resources.ClearProjectRoot();
  Check(resources.GetProjectRoot().empty() && !resources.HasExplicitContentRoot(),
        "clearing a content root also clears explicit resolution mode");
}
} // namespace

int main() {
  TestExplicitContentRoot();
  const auto path = std::filesystem::temp_directory_path() /
                    "archura_project_serializer_test.json";
  std::error_code ec;
  std::filesystem::remove(path, ec);

  {
    const std::string sentinel = "previous project must survive an oversized save";
    {
      std::ofstream previous(path, std::ios::binary);
      previous << sentinel;
    }
    Archura::Scene emptyScene("oversized save source");
    Archura::ProjectConfig oversized{std::string(32u * 1024u * 1024u, 'N'), "1.0", "MainScene"};
    Check(!Archura::ProjectSerializer::SaveProject(path.string(), oversized, &emptyScene),
          "save rejects metadata plus JSON wrapper exceeding the complete project load limit");
    std::ifstream retained(path, std::ios::binary);
    std::ostringstream retainedText;
    retainedText << retained.rdbuf();
    Check(retainedText.str() == sentinel, "oversized project save preserves the previous file");
    Check(emptyScene.GetEntities().empty(), "oversized project save leaves its source scene unchanged");
  }

  Archura::Scene source("source");
  auto* entity = source.CreateEntity("quoted \"entity\"");
  entity->SetOwnership(Archura::EntityOwnership::Engine);
  auto* authoredWeapon = entity->AddComponent<Archura::Weapon>();
  authoredWeapon->InitInventory();
  authoredWeapon->stats.currentMag = 5;
  authoredWeapon->isReloading = true;
  entity->GetComponent<Archura::Transform>()->position = {1.0f, 2.0f, 3.0f};
  entity->AddComponent<Archura::MeshRenderer>()->SetMeshAsset(Archura::Mesh::CreateCapsuleShared(0.75f, 3.25f));
  entity->AddComponent<Archura::AudioSource>()->clipName = "ambient.wav";
  entity->GetComponent<Archura::AudioSource>()->isPlaying = true;
  entity->GetComponent<Archura::AudioSource>()->runtimeAlias = "old alias";
  entity->AddComponent<Archura::AudioSourceComponent>()->pitch = 1.25f;
  auto* body = entity->AddComponent<Archura::RigidBody>();
  body->mass = 7.5f;
  body->angularVelocity = {1.0f, -2.0f, 3.0f};
  body->angularDrag = 0.35f;
  body->canBePushed = false;
  body->useGravity = false;
  body->restitution = 0.8f;
  auto* collider = entity->AddComponent<Archura::BoxCollider>();
  collider->shape = Archura::BoxCollider::Shape::Sphere;
  collider->radius = 0.75f;
  collider->center = {0.25f, 0.5f, -0.25f};
  entity->AddComponent<Archura::Health>()->current = 23.0f;
  entity->AddComponent<Archura::ScriptComponent>()->className = "Gameplay.Test";
  auto* child = source.CreateEntity("child");
  child->TrySetParent(entity);
  child->AddComponent<Archura::LightComponent>()->intensity = 4.0f;
  Archura::ProjectConfig config{"Test Project", "1.0", "start.scene"};
  Check(Archura::ProjectSerializer::SaveProject(path.string(), config, &source),
        "project save should succeed");

  Archura::Scene loadedScene("loaded");
  loadedScene.CreateEntity("sentinel")->SetOwnership(Archura::EntityOwnership::Engine);
  Archura::ProjectConfig loadedConfig;
  Check(Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig,
                                                &loadedScene),
        "project load should succeed");
  Check(loadedConfig.name == config.name, "project name should round-trip");
  Check(loadedScene.GetEntities().size() == 2,
        "project entities should round-trip");
  Check(loadedScene.GetEntities()[0]->IsEngineOwned() && !loadedScene.GetEntities()[1]->IsEngineOwned(),
        "project restores explicit engine/authored ownership and replaces prior engine entities");
  Check(loadedScene.GetEntities()[0]->GetName() == "quoted \"entity\"",
        "entity names should be escaped and restored");

  const auto& meshSource = loadedScene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>()->GetMesh()->GetSource();
  Check(meshSource.type == Archura::MeshSourceType::Capsule && meshSource.radius == 0.75f && meshSource.height == 3.25f, "project preserves mesh recipe");
  const auto* audio = loadedScene.GetEntities()[0]->GetComponent<Archura::AudioSource>();
  Check(audio && audio->clipName == "ambient.wav" && !audio->isPlaying && audio->runtimeAlias.empty(), "audio configuration round trips and runtime state resets");
  Check(loadedScene.GetEntities()[0]->GetComponent<Archura::AudioSourceComponent>()->pitch == 1.25f, "optional audio component config round trips");
  Check(loadedScene.GetEntities()[0]->GetComponent<Archura::RigidBody>()->mass == 7.5f, "rigid body mass round trips");
  const auto* restoredBody = loadedScene.GetEntities()[0]->GetComponent<Archura::RigidBody>();
  const auto* restoredCollider = loadedScene.GetEntities()[0]->GetComponent<Archura::BoxCollider>();
  Check(restoredBody->angularVelocity == body->angularVelocity && restoredBody->angularDrag == body->angularDrag &&
        !restoredBody->canBePushed && !restoredBody->useGravity && restoredBody->restitution == body->restitution,
        "project preserves ball rotation, damping, player push, gravity and bounce configuration");
  Check(restoredCollider && restoredCollider->shape == Archura::BoxCollider::Shape::Sphere &&
        restoredCollider->radius == 0.75f && restoredCollider->center == collider->center,
        "project preserves sphere collider radius and local center");
  Check(loadedScene.GetEntities()[0]->GetComponent<Archura::ScriptComponent>()->className == "Gameplay.Test", "script class round trips");
  Check(loadedScene.GetEntities()[1]->GetParent() == loadedScene.GetEntities()[0].get(), "hierarchy restored");
  Check(loadedScene.GetEntities()[1]->GetComponent<Archura::LightComponent>()->intensity == 4.0f, "light round trips");
  const auto* restoredWeapon = loadedScene.GetEntities()[0]->GetComponent<Archura::Weapon>();
  Check(restoredWeapon && restoredWeapon->stats.currentMag == 5 && restoredWeapon->inventory.size() == 4 && !restoredWeapon->isReloading,
        "project persists real weapon configuration while resetting transient state");
  Check(loadedScene.GetEntities()[0]->GetPersistentID() == entity->GetPersistentID() &&
        loadedScene.GetEntities()[1]->GetPersistentID() == child->GetPersistentID(), "both entity identities restore");

  {
    std::ifstream originalFile(path, std::ios::binary);
    std::ostringstream originalText; originalText << originalFile.rdbuf(); originalFile.close();
    Archura::Serialization::Json originalDocument, reloadedDocument;
    Check(Archura::Serialization::JsonParser(originalText.str()).Parse(originalDocument), "saved identity document parses");
    Check(Archura::ProjectSerializer::SaveProject(path.string(), loadedConfig, &loadedScene), "save after load succeeds");
    std::ifstream reloadedFile(path, std::ios::binary);
    std::ostringstream reloadedText; reloadedText << reloadedFile.rdbuf(); reloadedFile.close();
    Check(Archura::Serialization::JsonParser(reloadedText.str()).Parse(reloadedDocument), "re-saved identity document parses");
    const auto* beforeID = originalDocument.Find("entities")->array[0].Find("persistent_id");
    const auto* afterID = reloadedDocument.Find("entities")->array[0].Find("persistent_id");
    Check(beforeID && afterID && beforeID->string == afterID->string && !beforeID->string.empty(),
          "persistent scene identity must survive save/load/save independently of runtime handles");
    Check(originalText.str() == reloadedText.str(), "project save/load/save is deterministic for supported authored data");
    for (int i = 0; i < 3; ++i) {
      const auto oldHandle = loadedScene.GetEntities()[0]->GetHandle();
      Check(Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene), "same persistent IDs can replace the active scene repeatedly");
      Check(!loadedScene.IsAlive(oldHandle), "previous runtime handles stay invalid after identity restoration");
      Check(loadedScene.GetEntityByPersistentID(entity->GetPersistentID()) == loadedScene.GetEntities()[0].get(), "persistent lookup finds the fresh runtime object");
    }
  }

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
    R"({"format_version":3,"project":{"name":"bad"},"entities":[]})",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[{"name":"missing identity"}]})",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[{"name":"a","persistent_id":"11111111111111111111111111111111","parent_persistent_id":""},{"name":"b","persistent_id":"11111111111111111111111111111111","parent_persistent_id":""}]})",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[{"name":"a","persistent_id":"00000000000000000000000000000000","parent_persistent_id":""}]})",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[{"name":"a","persistent_id":"ABCDEFABCDEFABCDEFABCDEFABCDEFABCD","parent_persistent_id":""}]})",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[{"name":"a","persistent_id":"11111111111111111111111111111111","parent_persistent_id":"22222222222222222222222222222222"}]})",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[{"name":"a","persistent_id":"11111111111111111111111111111111","parent_persistent_id":"11111111111111111111111111111111"}]})",
    R"({"format_version":2,"project":{"name":"bad"},"entities":[{"name":"a","persistent_id":"11111111111111111111111111111111","parent_persistent_id":"","id":1}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","id":1,"parent_id":2},{"name":"b","id":2,"parent_id":1}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"future_component":{}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","transform":{"scale":[1e90,1,1]}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"collider":{"shape":3}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"collider":{"shape":2,"radius":0}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"collider":{"radius":-1}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"rigid_body":{"angular_velocity":[0,1e90,0]}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"rigid_body":{"angular_drag":-0.1}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"rigid_body":{"can_be_pushed":1}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"rigid_body":{"mass":-1}}}]})",
    R"({"project":{"name":"bad"},"entities":[{"name":"a","components":{"rigid_body":{"restitution":1.1}}}]})"
  };
  for (const auto& text : invalid) {
    write(text);
    Check(!Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene), "malformed/schema-invalid project rejects");
    Check(loadedConfig.name == "compact", "failed load preserves config");
    Check(loadedScene.GetEntities().size() == 1 && loadedScene.IsAlive(sentinel), "failed load preserves original entity handles");
  }
  auto deepScene = [](int count) {
    auto id = [](int value) { std::ostringstream out; out << std::hex << std::setw(32) << std::setfill('0') << value; return out.str(); };
    std::ostringstream out;
    out << "{\"format_version\":2,\"project\":{\"name\":\"depth boundary\"},\"entities\":[";
    for (int i = 1; i <= count; ++i) {
      if (i > 1) out << ',';
      out << "{\"name\":\"node\",\"persistent_id\":\"" << id(i)
          << "\",\"parent_persistent_id\":\"" << (i == 1 ? "" : id(i - 1)) << "\"}";
    }
    return out.str() + "]}";
  };
  write(deepScene(128));
  Check(Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene) && loadedScene.GetEntities().size() == 128,
        "version-two hierarchy permits 128 nodes / 127 parent links");
  const auto depthSentinel = loadedScene.GetEntities()[0]->GetHandle();
  write(deepScene(129));
  Check(!Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene) && loadedScene.IsAlive(depthSentinel) && loadedScene.GetEntities().size() == 128,
        "version-two excessive hierarchy depth rejects without replacing live state");
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
  {
    Archura::SceneReconstructionBudget tightBudget;
    tightBudget.maxEntities = 1;
    Archura::GameSaveManager::Get().SetReconstructionBudget(tightBudget);
    const auto priorHandle = loadedScene.GetEntities().front()->GetHandle();
    const auto priorCount = loadedScene.GetEntities().size();
    Check(!Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene) &&
              loadedScene.GetEntities().size() == priorCount &&
              loadedScene.IsAlive(priorHandle),
          "ProjectSerializer::LoadProject enforces SceneReconstructionBudget without mutating live scene");
    Archura::GameSaveManager::Get().ResetReconstructionBudget();
    Check(Archura::ProjectSerializer::LoadProject(path.string(), loadedConfig, &loadedScene) &&
              !Archura::ResourceManager::Get().GetProjectRoot().empty(),
          "ProjectSerializer::LoadProject registers project directory as ResourceManager project root");
    Archura::ResourceManager::Get().ClearProjectRoot();
  }
  std::filesystem::remove(path, ec);
  std::filesystem::remove(path.string() + ".bak", ec);
  return failures == 0 ? 0 : 1;
}
