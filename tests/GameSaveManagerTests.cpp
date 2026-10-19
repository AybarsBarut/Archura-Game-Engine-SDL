#include "core/GameSaveManager.h"
#include "core/ResourceManager.h"
#include "core/AudioSystem.h"
#include "editor/GameBuilderPanel.h"
#include "ecs/Entity.h"
#include "rendering/Camera.h"
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
#include "rendering/Terrain.h"
#include "rendering/Texture.h"
#include "game/AudioSource.h"
#include "game/Weapon.h"
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void WriteMinimalOBJ(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << "v 0.0 0.0 0.0\n"
        << "v 1.0 0.0 0.0\n"
        << "v 0.0 1.0 0.0\n"
        << "vn 0.0 0.0 1.0\n"
        << "vt 0.0 0.0\n"
        << "vt 1.0 0.0\n"
        << "vt 0.0 1.0\n"
        << "f 1/1/1 2/2/1 3/3/1\n";
}

// Writes an uncompressed 24-bit top-down BMP image where each pixel (c, r) has
// grayscale intensity `intensities[r * width + c]`.
void WriteGrayscaleBMP(const std::filesystem::path& path,
                       int width,
                       int height,
                       const std::vector<uint8_t>& intensities) {
    std::filesystem::create_directories(path.parent_path());
    const int rowStride = (width * 3 + 3) & ~3;
    const uint32_t pixelDataSize = static_cast<uint32_t>(rowStride * height);
    const uint32_t fileSize = 54u + pixelDataSize;

    std::vector<uint8_t> bytes(fileSize, 0u);
    bytes[0] = 'B';
    bytes[1] = 'M';
    auto writeU32 = [&](size_t offset, uint32_t val) {
        bytes[offset + 0] = static_cast<uint8_t>(val & 0xFFu);
        bytes[offset + 1] = static_cast<uint8_t>((val >> 8u) & 0xFFu);
        bytes[offset + 2] = static_cast<uint8_t>((val >> 16u) & 0xFFu);
        bytes[offset + 3] = static_cast<uint8_t>((val >> 24u) & 0xFFu);
    };
    auto writeI32 = [&](size_t offset, int32_t val) {
        writeU32(offset, static_cast<uint32_t>(val));
    };
    auto writeU16 = [&](size_t offset, uint16_t val) {
        bytes[offset + 0] = static_cast<uint8_t>(val & 0xFFu);
        bytes[offset + 1] = static_cast<uint8_t>((val >> 8u) & 0xFFu);
    };

    writeU32(2, fileSize);
    writeU32(10, 54u);
    writeU32(14, 40u);
    writeI32(18, width);
    writeI32(22, -height); // top-down row order
    writeU16(26, 1u);
    writeU16(28, 24u);
    writeU32(34, pixelDataSize);

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            const uint8_t v = intensities[static_cast<size_t>(r * width + c)];
            const size_t idx = 54u + static_cast<size_t>(r * rowStride + c * 3);
            bytes[idx + 0] = v;
            bytes[idx + 1] = v;
            bytes[idx + 2] = v;
        }
    }

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}
} // namespace

int main() {
    const auto original = std::filesystem::current_path();
    const auto work = std::filesystem::temp_directory_path() /
                      ("archura_save_tests_" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(work);
    std::filesystem::current_path(work);
    auto& saves = Archura::GameSaveManager::Get();
    saves.ResetReconstructionBudget();
    Archura::ResourceManager::Get().ClearProjectRoot();

    Archura::Scene scene;
    auto* originalEntity = scene.CreateEntity("original");
    const auto handle = originalEntity->GetHandle();
    auto write = [](const std::string& text) {
        std::ofstream f("input.scene", std::ios::binary);
        f << text;
    };
    {
        Archura::Scene compatibility;
        write(R"({"entities":[{"name":"old box","has_mesh_renderer":false,"collider":[1,1,1],"components":{"rigid_body":{"mass":2}}}]})");
        Check(saves.LoadProject("input.scene", &compatibility), "legacy body and box load with absent new fields");
        const auto* oldBody = compatibility.GetEntities()[0]->GetComponent<Archura::RigidBody>();
        const auto* oldCollider = compatibility.GetEntities()[0]->GetComponent<Archura::BoxCollider>();
        Check(oldBody && oldBody->canBePushed && oldBody->angularDrag == 0.1f &&
                  oldBody->angularVelocity == glm::vec3(0) && oldBody->mass == 2 &&
                  oldCollider && oldCollider->shape == Archura::BoxCollider::Shape::Box && oldCollider->radius == 0.5f,
              "legacy files keep existing shape and default push/angular settings");
        write(R"({"entities":[{"name":"explicit sphere","has_mesh_renderer":false,"collider":[1,1,1],"collider_shape":2}]})");
        Check(saves.LoadProject("input.scene", &compatibility), "top-level collider sphere shape remains readable");
        const auto* explicitSphere = compatibility.GetEntities()[0]->GetComponent<Archura::BoxCollider>();
        Check(explicitSphere && explicitSphere->shape == Archura::BoxCollider::Shape::Sphere && explicitSphere->radius == 0.5f,
              "explicit sphere with absent radius uses local radius 0.5");
        const auto protectedHandle = compatibility.GetEntities()[0]->GetHandle();
        const std::string invalidPhysics[] = {
            R"("collider_shape":3)", R"("collider_radius":0)",
            R"("components":{"collider":{"shape":2,"radius":1e90}})",
            R"("components":{"rigid_body":{"angular_drag":-1}})",
            R"("components":{"rigid_body":{"angular_velocity":[1,2]}})",
            R"("components":{"rigid_body":{"can_be_pushed":"true"}})"
        };
        for (const auto& fields : invalidPhysics) {
            write("{\"entities\":[{\"name\":\"invalid physics\",\"has_mesh_renderer\":false," + fields + "}]}");
            Check(!saves.LoadProject("input.scene", &compatibility) && compatibility.IsAlive(protectedHandle) &&
                      compatibility.GetEntities().size() == 1,
                  "malformed sphere/body scene rejects transactionally");
        }
    }
    {
        std::ofstream broken("broken.obj");
        broken << "not an OBJ model";
    }
    write(R"({"entities":[{"name":"stage succeeds","has_mesh_renderer":false},{"name":"invalid model","mesh_type":"obj","model_path":"broken.obj"}]})");
    Check(!saves.LoadProject("input.scene", &scene), "failed asset instantiate returns failure");
    Check(scene.GetEntities().size() == 1 && scene.IsAlive(handle),
          "failed instantiate rolls back staged entities and preserves original handle");
    write(R"({"entities":[{"name":"legacy sphere","mesh_type":"sphere","position":[1,2,3]}]})");
    Check(saves.LoadProject("input.scene", &scene), "legacy scene loads");
    Check(scene.GetEntities().size() == 1, "legacy scene replaces original");
    Check(scene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>()->GetMesh()->GetSource().type ==
              Archura::MeshSourceType::Sphere,
          "legacy sphere retains source type");
    auto* mesh = scene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>();
    mesh->SetMeshAsset(Archura::Mesh::CreateSphereShared(2.5f, 12));
    auto* ballCollider = scene.GetEntities()[0]->AddComponent<Archura::BoxCollider>();
    ballCollider->shape = Archura::BoxCollider::Shape::Sphere;
    ballCollider->radius = 2.5f;
    auto* ballBody = scene.GetEntities()[0]->AddComponent<Archura::RigidBody>();
    ballBody->mass = 4.5f;
    ballBody->angularVelocity = {2.0f, 3.0f, -4.0f};
    ballBody->angularDrag = 0.25f;
    ballBody->canBePushed = false;
    ballBody->restitution = 0.6f;
    auto* child = scene.CreateEntity("child");
    child->TrySetParent(scene.GetEntities()[0].get());
    child->AddComponent<Archura::ScriptComponent>()->className = "Example.Script";
    Check(saves.SaveProject("roundtrip", &scene), "scene save succeeds");
    Archura::Scene restored;
    auto* system = restored.CreateEntity("Player");
    const auto systemHandle = system->GetHandle();
    system->SetOwnership(Archura::EntityOwnership::Engine);
    Check(saves.LoadProject("saves/roundtrip.scene", &restored), "scene reload succeeds");
    Check(restored.IsAlive(systemHandle), "system entity survives load");
    Check(restored.GetEntities().size() == 3, "all user entities reload");
    const auto& source =
        restored.GetEntities()[1]->GetComponent<Archura::MeshRenderer>()->GetMesh()->GetSource();
    Check(source.type == Archura::MeshSourceType::Sphere && source.radius == 2.5f &&
              source.segments == 12,
          "mesh type and dimensions survive roundtrip");
    const auto* reloadedCollider = restored.GetEntities()[1]->GetComponent<Archura::BoxCollider>();
    const auto* reloadedBody = restored.GetEntities()[1]->GetComponent<Archura::RigidBody>();
    Check(reloadedCollider && reloadedCollider->shape == Archura::BoxCollider::Shape::Sphere &&
              reloadedCollider->radius == 2.5f,
          "scene content preserves sphere collider independently from the mesh recipe");
    Check(reloadedBody && reloadedBody->mass == 4.5f && reloadedBody->angularVelocity == ballBody->angularVelocity &&
              reloadedBody->angularDrag == 0.25f && !reloadedBody->canBePushed &&
              reloadedBody->useGravity && reloadedBody->restitution == 0.6f,
          "scene content preserves dynamic ball settings and angular state");
    Check(restored.GetEntities()[2]->GetParent() == restored.GetEntities()[1].get(),
          "hierarchy survives roundtrip");
    Check(restored.GetEntities()[2]->GetComponent<Archura::ScriptComponent>()->className ==
              "Example.Script",
          "script config survives roundtrip");
    const auto persistentParent = restored.GetEntities()[1]->GetPersistentID();
    Check(persistentParent == scene.GetEntities()[0]->GetPersistentID(),
          "content restores persistent identity");
    const auto previousAuthoredHandle = restored.GetEntities()[1]->GetHandle();
    Check(saves.LoadProject("saves/roundtrip.scene", &restored),
          "same content persistent IDs can replace their old instances");
    Check(!restored.IsAlive(previousAuthoredHandle) &&
              restored.GetEntityByPersistentID(persistentParent),
          "runtime handles invalidate while content identity survives");
    const auto beforeCollision = restored.GetEntities().size();
    write("{\"format_version\":2,\"entities\":[{\"name\":\"collision\",\"has_mesh_renderer\":false,\"persistent_id\":\"" +
          system->GetPersistentID() + "\",\"parent_persistent_id\":\"\"}]}");
    Check(!saves.LoadProject("input.scene", &restored) &&
              restored.GetEntities().size() == beforeCollision &&
              restored.IsAlive(systemHandle) &&
              restored.GetEntityByPersistentID(persistentParent),
          "retained engine identity collision rejects and rolls back without losing content");
    const auto oldCount = restored.GetEntities().size();
    write(R"({"entities":[{"name":"missing","mesh_type":"obj","model_path":"missing.obj"}]})");
    Check(!saves.LoadProject("input.scene", &restored) &&
              restored.GetEntities().size() == oldCount,
          "missing model rejects without changing scene");
    write(R"({"entities":[{"name":"malformed","position":[1 2 3]}]})");
    Check(!saves.LoadProject("input.scene", &restored), "malformed arrays reject");
    Check(saves.SaveProject("roundtrip", &scene), "atomic replacement succeeds");
    Check(std::filesystem::is_regular_file("saves/roundtrip.scene.bak"),
          "replacement retains backup");
    std::ifstream previous("saves/roundtrip.scene", std::ios::binary);
    std::ostringstream expected;
    expected << previous.rdbuf();
    previous.close();
    auto verifyPrevious = [&]() {
        std::ifstream f("saves/roundtrip.scene", std::ios::binary);
        std::ostringstream text;
        text << f.rdbuf();
        return text.str() == expected.str();
    };
    ballCollider->radius = -1.0f;
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "negative collider radius rejects save without replacing previous content");
    ballCollider->radius = 2.5f;
    ballBody->angularDrag = -1.0f;
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "negative angular damping rejects save without replacing previous content");
    ballBody->angularDrag = 0.25f;
    ballBody->angularVelocity.y = std::numeric_limits<float>::infinity();
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "nonfinite angular velocity rejects save without replacing previous content");
    ballBody->angularVelocity.y = 3.0f;
    auto* invalidLight = scene.GetEntities()[0]->AddComponent<Archura::LightComponent>();
    invalidLight->intensity = -1.0f;
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "semantically invalid component fails save without overwrite");
    scene.GetEntities()[0]->RemoveComponent<Archura::LightComponent>();
    Archura::Shader assignedShader;
    mesh->shader = &assignedShader;
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "assigned shader fails without overwriting previous file");
    mesh->shader = nullptr;
    Archura::Texture assignedTexture;
    mesh->texture = &assignedTexture;
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "generated/source-less texture fails without overwrite");
    mesh->texture = nullptr;
    struct Unsupported final : Archura::Component {};
    scene.GetEntities()[0]->AddComponent<Unsupported>();
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "unknown component fails without overwrite");
    scene.GetEntities()[0]->RemoveComponent<Unsupported>();
    auto* excluded = scene.CreateEntity("Player");
    excluded->SetOwnership(Archura::EntityOwnership::Engine);
    child->TrySetParent(excluded);
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "omitted parent fails without overwrite");
    child->TrySetParent(scene.GetEntities()[0].get());
    excluded->TrySetParent(child);
    Check(!saves.SaveProject("roundtrip", &scene) && verifyPrevious(),
          "omitted child fails without overwrite");
    excluded->TrySetParent(nullptr);
    mesh->GetMesh()->GetVertices(); // Mutable access invalidates source metadata.
    Check(!saves.SaveProject("roundtrip", &scene), "custom/edited mesh save fails explicitly");
    std::ifstream retained("saves/roundtrip.scene", std::ios::binary);
    std::ostringstream actual;
    actual << retained.rdbuf();
    retained.close();
    Check(actual.str() == expected.str(), "unsupported mesh never overwrites prior save");

    {
        Archura::Scene authored;
        auto* playerNamed = authored.CreateEntity("Player");
        auto* floorNamed = authored.CreateEntity("Floor");
        floorNamed->TrySetParent(playerNamed);
        floorNamed->GetComponent<Archura::Transform>()->position = {3, 4, 5};
        auto* renamedEngine = authored.CreateEntity("Renamed bootstrap service");
        renamedEngine->SetOwnership(Archura::EntityOwnership::Engine);
        Check(saves.SaveProject("authored_names", &authored), "authored system-like names save");
        Archura::Scene loaded;
        Check(saves.LoadProject("saves/authored_names.scene", &loaded),
              "authored system-like names load");
        Check(loaded.GetEntities().size() == 2,
              "Player/Floor authored entities must not disappear");
        if (loaded.GetEntities().size() == 2) {
            Check(loaded.GetEntities()[1]->GetParent() == loaded.GetEntities()[0].get(),
                  "system-like name hierarchy survives");
            Check(loaded.GetEntities()[1]->GetComponent<Archura::Transform>()->position ==
                      glm::vec3(3, 4, 5),
                  "system-like name transform survives");
        }
        auto* preservedEngine = loaded.CreateEntity("Runtime service with arbitrary name");
        preservedEngine->SetOwnership(Archura::EntityOwnership::Engine);
        preservedEngine->SetName("Renamed again");
        const auto serviceHandle = preservedEngine->GetHandle();
        for (int reload = 0; reload < 3; ++reload) {
            const auto replacedHandle = loaded.GetEntities().front()->GetHandle();
            Check(saves.LoadProject("saves/authored_names.scene", &loaded),
                  "repeated content reload succeeds");
            Check(loaded.IsAlive(serviceHandle),
                  "explicit engine ownership survives rename and reload");
            Check(loaded.GetEntities().size() == 3,
                  "authored Player/Floor names do not accumulate across loads");
            if (replacedHandle != serviceHandle)
                Check(!loaded.IsAlive(replacedHandle), "authored old handles are invalidated");
        }
        write(R"({"entities":[{"name":"injected service","engine_owned":true,"has_mesh_renderer":false}]})");
        Check(!saves.LoadProject("input.scene", &loaded) &&
                  loaded.GetEntities().size() == 3 && loaded.IsAlive(serviceHandle),
              "scene content cannot claim permanent engine ownership");
        write(R"({"entities":[{"name":"invalid ownership","engine_owned":"true","has_mesh_renderer":false}]})");
        Check(!saves.LoadProject("input.scene", &loaded),
              "ownership metadata requires a boolean");
        write(R"({"entities":[{"name":"Player","has_mesh_renderer":false},{"name":"Floor","has_mesh_renderer":false}]})");
        Check(saves.LoadProject("input.scene", &loaded) && loaded.GetEntities().size() == 3,
              "legacy scene system-like names migrate as authored content");
        write(R"({"format_version":1,"entities":[{"name":"legacy parent","id":41,"has_mesh_renderer":false},{"name":"legacy child","id":42,"parent_id":41,"has_mesh_renderer":false}]})");
        Check(saves.LoadProject("input.scene", &loaded),
              "version-one numeric hierarchy migrates");
        Check(loaded.GetEntities()[2]->GetParent() == loaded.GetEntities()[1].get(),
              "numeric parent references survive migration");
        const auto migratedID = loaded.GetEntities()[1]->GetPersistentID();
        Check(saves.SaveProject("migrated", &loaded) &&
                  saves.LoadProject("saves/migrated.scene", &loaded),
              "migrated scene saves and reloads as version two");
        Check(loaded.GetEntities()[1]->GetPersistentID() == migratedID,
              "migrated identity stabilizes after its first version-two save");
    }
    {
        Archura::Scene armed;
        auto* player = armed.CreateEntity("authored armed Player");
        auto* weapon = player->AddComponent<Archura::Weapon>();
        weapon->InitInventory();
        weapon->stats.damage = 42.5f;
        weapon->stats.currentMag = 7;
        weapon->stats.pattern.offsets = {{1.25f, -2.5f}, {0.5f, 3.0f}};
        weapon->isReloading = true;
        weapon->reloadTimer = 0.75f;
        const bool saved = saves.SaveProject("armed", &armed);
        Check(saved, "authored Weapon configuration must be saveable");
        if (saved) {
            Archura::Scene loaded;
            const bool success = saves.LoadProject("saves/armed.scene", &loaded);
            Check(success && loaded.GetEntities().size() == 1, "armed scene round trips");
            if (success && !loaded.GetEntities().empty()) {
                const auto* restoredWeapon =
                    loaded.GetEntities()[0]->GetComponent<Archura::Weapon>();
                Check(restoredWeapon && restoredWeapon->stats.damage == 42.5f &&
                          restoredWeapon->stats.currentMag == 7,
                      "weapon active stats survive scene save");
                Check(restoredWeapon &&
                          restoredWeapon->stats.pattern.offsets == weapon->stats.pattern.offsets &&
                          restoredWeapon->inventory.size() == weapon->inventory.size(),
                      "weapon inventory and recoil pattern survive scene save");
                Check(restoredWeapon && !restoredWeapon->isReloading &&
                          restoredWeapon->reloadTimer == 0,
                      "transient reload state resets on authored scene load");
                auto readArmed = [] {
                    std::ifstream f("saves/armed.scene", std::ios::binary);
                    std::ostringstream out;
                    out << f.rdbuf();
                    return out.str();
                };
                const auto goodSave = readArmed();
                weapon->stats.currentMag = weapon->stats.magSize + 1;
                Check(!saves.SaveProject("armed", &armed) && readArmed() == goodSave,
                      "invalid weapon ammo rejects without overwriting the preceding save");
                weapon->stats.currentMag = 7;
                const auto oldArmedHandle = loaded.GetEntities()[0]->GetHandle();
                std::string malformedWeapon = goodSave;
                const std::string key = "\"current_mag\":7";
                const auto position = malformedWeapon.find(key);
                Check(position != std::string::npos, "fixture contains active magazine count");
                if (position != std::string::npos) {
                    malformedWeapon.replace(position, key.size(), "\"current_mag\":-1");
                    write(malformedWeapon);
                    Check(!saves.LoadProject("input.scene", &loaded) &&
                              loaded.IsAlive(oldArmedHandle),
                          "malformed weapon scene preserves original live handle");
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // GAUNTLET Loop 4: Aggregate SceneReconstructionBudget & Asset Resolution
    // -----------------------------------------------------------------------
    {
        Archura::Scene budgetScene;
        auto* sentinel = budgetScene.CreateEntity("BudgetSentinel");
        const auto sentinelHandle = sentinel->GetHandle();

        // 1. maxEntities budget enforcement
        Archura::SceneReconstructionBudget entityBudget;
        entityBudget.maxEntities = 2;
        saves.SetReconstructionBudget(entityBudget);
        write(R"({"entities":[{"name":"e1","has_mesh_renderer":false},{"name":"e2","has_mesh_renderer":false},{"name":"e3","has_mesh_renderer":false}]})");
        Check(!saves.LoadProject("input.scene", &budgetScene) &&
                  budgetScene.IsAlive(sentinelHandle) &&
                  budgetScene.GetEntities().size() == 1,
              "exceeding maxEntities budget rejects before replacing scene");

        // 2. maxTotalMeshVertices budget enforcement across multiple entities
        saves.ResetReconstructionBudget();
        Archura::SceneReconstructionBudget meshBudget;
        meshBudget.maxTotalMeshVertices = 30; // 1 cube = 24 vertices, 2 cubes = 48 vertices
        saves.SetReconstructionBudget(meshBudget);
        write(R"({"entities":[{"name":"cube1","mesh_type":"cube"},{"name":"cube2","mesh_type":"cube"}]})");
        Check(!saves.LoadProject("input.scene", &budgetScene) &&
                  budgetScene.IsAlive(sentinelHandle) &&
                  budgetScene.GetEntities().size() == 1,
              "exceeding maxTotalMeshVertices across multiple entities rejects and preserves scene");

        // 3. maxTotalTextureBytes & maxSingleAssetFileBytes budget enforcement
        WriteGrayscaleBMP(work / "assets/textures/test4x4.bmp", 4, 4,
                          std::vector<uint8_t>(16, 180u));
        saves.ResetReconstructionBudget();
        Archura::SceneReconstructionBudget texBudget;
        texBudget.maxTotalTextureBytes = 80; // 1x 4x4 RGBA = 64 bytes, 2x 4x4 RGBA = 128 bytes
        saves.SetReconstructionBudget(texBudget);
        write(R"({"entities":[{"name":"t1","mesh_type":"cube","texture_path":"assets/textures/test4x4.bmp"},{"name":"t2","mesh_type":"cube","texture_path":"assets/textures/test4x4.bmp"}]})");
        Check(!saves.LoadProject("input.scene", &budgetScene) &&
                  budgetScene.IsAlive(sentinelHandle) &&
                  budgetScene.GetEntities().size() == 1,
              "exceeding maxTotalTextureBytes rejects and preserves scene");

        Archura::SceneReconstructionBudget fileBudget;
        fileBudget.maxSingleAssetFileBytes = 16; // smaller than BMP file size
        saves.SetReconstructionBudget(fileBudget);
        write(R"({"entities":[{"name":"t1","mesh_type":"cube","texture_path":"assets/textures/test4x4.bmp"}]})");
        Check(!saves.LoadProject("input.scene", &budgetScene) &&
                  budgetScene.IsAlive(sentinelHandle),
              "exceeding maxSingleAssetFileBytes rejects oversized asset file");
        saves.ResetReconstructionBudget();

        // 4. Reject parent directory traversal in asset paths
        write(R"({"entities":[{"name":"traversal","mesh_type":"obj","model_path":"../outside.obj"}]})");
        Check(!saves.LoadProject("input.scene", &budgetScene) &&
                  budgetScene.IsAlive(sentinelHandle),
              "parent directory traversal in model_path rejects");
        write(R"({"entities":[{"name":"tex_traversal","mesh_type":"cube","texture_path":"assets/textures/../../outside.bmp"}]})");
        Check(!saves.LoadProject("input.scene", &budgetScene) &&
                  budgetScene.IsAlive(sentinelHandle),
              "parent directory traversal in texture_path rejects");
    }

    // -----------------------------------------------------------------------
    // GAUNTLET Loop 4: Project-root relocation, duplicate-stem textures,
    // and GameBuilderPanel assets/models fallback (ASSET-01)
    // -----------------------------------------------------------------------
    {
        const auto relocatedProject = work / "relocated_project";
        const auto unrelatedCwd = work / "unrelated_cwd";
        std::filesystem::create_directories(unrelatedCwd);

        WriteMinimalOBJ(relocatedProject / "assets/models/crate.obj");
        WriteGrayscaleBMP(relocatedProject / "assets/textures/wood/albedo.bmp", 2, 2,
                          std::vector<uint8_t>(4, 64u));
        WriteGrayscaleBMP(relocatedProject / "assets/textures/metal/albedo.bmp", 4, 4,
                          std::vector<uint8_t>(16, 200u));

        // Switch CWD to an unrelated folder so relative CWD lookups fail unless
        // documentRoot / ResourceManager::GetProjectRoot resolves them.
        std::filesystem::current_path(unrelatedCwd);
        Archura::ResourceManager::Get().SetProjectRoot(relocatedProject);

        auto woodTex = Archura::ResourceManager::Get().LoadTextureShared(
            "albedo", "assets/textures/wood/albedo.bmp");
        auto metalTex = Archura::ResourceManager::Get().LoadTextureShared(
            "albedo", "assets/textures/metal/albedo.bmp");
        Check(woodTex != nullptr && metalTex != nullptr && woodTex != metalTex,
              "same-stem textures from different directories must not collide");
        if (woodTex && metalTex) {
            Check(woodTex->GetWidth() == 2 && metalTex->GetWidth() == 4,
                  "distinct canonical texture entries preserve their own dimensions");
            Check(woodTex->GetCanonicalPath() != metalTex->GetCanonicalPath(),
                  "canonical paths distinguish wood/albedo.bmp from metal/albedo.bmp");
        }

        // GameBuilderPanel fallback to assets/models when assets/objects is absent
        Archura::GameBuilderPanel builder;
        builder.RefreshModelList();
        Check(builder.GetActiveModelDirectory() == "assets/models" &&
                  builder.GetModelFiles().size() == 1 &&
                  builder.GetModelFiles()[0] == "crate.obj",
              "GameBuilderPanel discovers OBJ models in assets/models when assets/objects is missing");
        Archura::Scene builderScene;
        Archura::Entity* spawnedModel = builder.SpawnModelEntity(&builderScene, "crate.obj");
        Check(spawnedModel != nullptr &&
                  spawnedModel->GetComponent<Archura::MeshRenderer>() != nullptr &&
                  spawnedModel->GetComponent<Archura::MeshRenderer>()->GetMesh() != nullptr,
              "GameBuilderPanel spawns OBJ model from assets/models fallback directory");

        // Save and reload a scene in relocatedProject while CWD is unrelatedCwd
        if (spawnedModel) {
            auto* mr = spawnedModel->GetComponent<Archura::MeshRenderer>();
            mr->GetMesh()->SetSourcePath("assets/models/crate.obj");
            woodTex->SetPath("assets/textures/wood/albedo.bmp");
            mr->SetTextureAsset(woodTex);
        }
        const auto relocatedSceneFile = relocatedProject / "saves/relocated.scene";
        Archura::ResourceManager::Get().ClearProjectRoot();
        Check(saves.SaveProject(relocatedSceneFile.string(), &builderScene),
              "saving relocated scene with relative asset paths succeeds");

        Archura::Scene reloadedRelocated;
        Check(saves.LoadProject(relocatedSceneFile.string(), &reloadedRelocated) &&
                  reloadedRelocated.GetEntities().size() == 1,
              "loading relocated scene from unrelated CWD resolves assets via document root");
        if (!reloadedRelocated.GetEntities().empty()) {
            const auto* mr =
                reloadedRelocated.GetEntities()[0]->GetComponent<Archura::MeshRenderer>();
            Check(mr && mr->GetMesh() &&
                      mr->GetMesh()->GetSource().path == "assets/models/crate.obj",
                  "relocated load preserves authored relative model_path");
            Check(mr && mr->GetTexture() &&
                      mr->GetTexture()->GetPath() == "assets/textures/wood/albedo.bmp",
                  "relocated load preserves authored relative texture_path");
        }

        std::filesystem::current_path(work);
        Archura::ResourceManager::Get().ClearProjectRoot();
    }

    // -----------------------------------------------------------------------
    // GAUNTLET Loop 5: AudioSystem path normalization & runtime alias (AUD-01/02)
    // and Terrain heightmap / non-square / scaled bounds checks (TERR-01)
    // -----------------------------------------------------------------------
    {
        const std::string p1 = Archura::AudioSystem::ResolveAudioPath("explosion.wav");
        const std::string p2 = Archura::AudioSystem::ResolveAudioPath("assets/audio/explosion.wav");
        const std::string p3 =
            Archura::AudioSystem::ResolveAudioPath("assets/audio/assets/audio/explosion.wav");
        Check(p1 == "assets/audio/explosion.wav" && p2 == p1 && p3 == p1,
              "AudioSystem::ResolveAudioPath prevents double assets/audio/ prefixing");
        Check(Archura::AudioSystem::ResolveAudioPath("../escape.wav").empty(),
              "AudioSystem::ResolveAudioPath rejects parent-traversal paths");

        Archura::Scene audioScene;
        auto* emitter = audioScene.CreateEntity("Emitter");
        auto* sourceComp = emitter->AddComponent<Archura::AudioSource>();
        sourceComp->clipName = "explosion.wav";
        sourceComp->isPlaying = true;
        Archura::Camera cam(glm::vec3(0.0f, 0.0f, 0.0f));
        Archura::AudioSystem::Get().Update(&audioScene, &cam);
        Check(sourceComp->runtimeAlias == "sound_" + std::to_string(emitter->GetID()),
              "AudioSystem::Update assigns deterministic runtimeAlias when isPlaying is true");
        Archura::AudioSystem::Get().StopAllSceneAudio(&audioScene);
        Check(!sourceComp->isPlaying && sourceComp->runtimeAlias.empty(),
              "StopAllSceneAudio clears runtimeAlias and isPlaying");

        // Terrain missing heightmap fallback (must not leak or crash headlessly)
        std::unique_ptr<Archura::Terrain> fallbackTerrain(
            Archura::Terrain::LoadFromHeightmap("assets/textures/nonexistent_heightmap.bmp",
                                                120.0f, 80.0f, 25.0f));
        Check(fallbackTerrain != nullptr && fallbackTerrain->GetCols() == 64 &&
                  fallbackTerrain->GetRows() == 64 &&
                  fallbackTerrain->GetHeightAt(0.0f, 0.0f) == 0.0f,
              "missing heightmap falls back to flat terrain cleanly in headless mode");

        // Rectangular 4x2 heightmap with horizontal scale and bounds check
        const auto heightmapFile = work / "assets/textures/rect_heightmap.bmp";
        WriteGrayscaleBMP(heightmapFile, 4, 2,
                          {0u, 85u, 170u, 255u,
                           0u, 85u, 170u, 255u});
        std::unique_ptr<Archura::Terrain> rectTerrain(
            Archura::Terrain::LoadFromHeightmap(heightmapFile.string(), 30.0f, 10.0f, 30.0f));
        Check(rectTerrain != nullptr && rectTerrain->GetCols() == 4 &&
                  rectTerrain->GetRows() == 2,
              "rectangular 4x2 heightmap preserves distinct column and row counts");
        if (rectTerrain) {
            rectTerrain->position = glm::vec3(10.0f, 5.0f, -20.0f);
            rectTerrain->scale = glm::vec3(2.0f, 2.0f, 3.0f);
            // Local X spans [-15, +15], scaled by 2.0 around X=10 -> world X spans [-20, +40].
            // At worldX = -20 (left edge, pixel 0 -> height 0), world height = 0 * 2 + 5 = 5.0f.
            // At worldX = +40 (right edge, pixel 255 -> height 30), world height = 30 * 2 + 5 = 65.0f.
            const float leftH = rectTerrain->GetHeightAt(-20.0f, -20.0f);
            const float rightH = rectTerrain->GetHeightAt(40.0f, -20.0f);
            const float midH = rectTerrain->GetHeightAt(10.0f, -20.0f);
            Check(std::abs(leftH - 5.0f) < 1.0e-3f &&
                      std::abs(rightH - 65.0f) < 1.0e-2f &&
                      std::abs(midH - 35.0f) < 0.25f,
                  "Terrain::GetHeightAt accounts for rectangular grid, position, and horizontal/vertical scale");
            Check(rectTerrain->GetHeightAt(-25.0f, -20.0f) == 0.0f &&
                      rectTerrain->GetHeightAt(45.0f, -20.0f) == 0.0f &&
                      rectTerrain->GetHeightAt(10.0f, 0.0f) == 0.0f,
                  "Terrain::GetHeightAt returns 0.0f outside scaled world bounds");
        }
    }

    std::filesystem::current_path(original);
    // This unique directory was created exclusively by this test.
    std::error_code ec;
    std::filesystem::remove_all(work, ec);
    return failures ? 1 : 0;
}
