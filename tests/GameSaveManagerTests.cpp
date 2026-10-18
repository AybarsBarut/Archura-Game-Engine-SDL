#include "core/GameSaveManager.h"
#include "ecs/Entity.h"
#include "rendering/Mesh.h"
#include "rendering/Shader.h"
#include "rendering/Texture.h"
#include "game/Weapon.h"
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
int failures=0;
void Check(bool condition,const char* message){if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}}
}
int main(){
    const auto original = std::filesystem::current_path();
    const auto work = std::filesystem::temp_directory_path() / ("archura_save_tests_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(work);
    std::filesystem::current_path(work);
    auto& saves=Archura::GameSaveManager::Get();
    Archura::Scene scene;
    auto* originalEntity=scene.CreateEntity("original");
    const auto handle=originalEntity->GetHandle();
    auto write=[](const std::string& text){std::ofstream f("input.scene",std::ios::binary);f<<text;};
    { std::ofstream broken("broken.obj"); broken << "not an OBJ model"; }
    write(R"({"entities":[{"name":"stage succeeds","has_mesh_renderer":false},{"name":"invalid model","mesh_type":"obj","model_path":"broken.obj"}]})");
    Check(!saves.LoadProject("input.scene",&scene),"failed asset instantiate returns failure");
    Check(scene.GetEntities().size()==1 && scene.IsAlive(handle),"failed instantiate rolls back staged entities and preserves original handle");
    write(R"({"entities":[{"name":"legacy sphere","mesh_type":"sphere","position":[1,2,3]}]})");
    Check(saves.LoadProject("input.scene",&scene),"legacy scene loads");
    Check(scene.GetEntities().size()==1,"legacy scene replaces original");
    Check(scene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>()->GetMesh()->GetSource().type==Archura::MeshSourceType::Sphere,"legacy sphere retains source type");
    auto* mesh=scene.GetEntities()[0]->GetComponent<Archura::MeshRenderer>();
    mesh->SetMeshAsset(Archura::Mesh::CreateSphereShared(2.5f,12));
    auto* child=scene.CreateEntity("child");child->TrySetParent(scene.GetEntities()[0].get());
    child->AddComponent<Archura::ScriptComponent>()->className="Example.Script";
    Check(saves.SaveProject("roundtrip",&scene),"scene save succeeds");
    Archura::Scene restored;
    auto* system=restored.CreateEntity("Player");const auto systemHandle=system->GetHandle();
    system->SetOwnership(Archura::EntityOwnership::Engine);
    Check(saves.LoadProject("saves/roundtrip.scene",&restored),"scene reload succeeds");
    Check(restored.IsAlive(systemHandle),"system entity survives load");
    Check(restored.GetEntities().size()==3,"all user entities reload");
    const auto& source=restored.GetEntities()[1]->GetComponent<Archura::MeshRenderer>()->GetMesh()->GetSource();
    Check(source.type==Archura::MeshSourceType::Sphere && source.radius==2.5f && source.segments==12,"mesh type and dimensions survive roundtrip");
    Check(restored.GetEntities()[2]->GetParent()==restored.GetEntities()[1].get(),"hierarchy survives roundtrip");
    Check(restored.GetEntities()[2]->GetComponent<Archura::ScriptComponent>()->className=="Example.Script","script config survives roundtrip");
    const auto persistentParent = restored.GetEntities()[1]->GetPersistentID();
    Check(persistentParent == scene.GetEntities()[0]->GetPersistentID(), "content restores persistent identity");
    const auto previousAuthoredHandle = restored.GetEntities()[1]->GetHandle();
    Check(saves.LoadProject("saves/roundtrip.scene", &restored), "same content persistent IDs can replace their old instances");
    Check(!restored.IsAlive(previousAuthoredHandle) && restored.GetEntityByPersistentID(persistentParent), "runtime handles invalidate while content identity survives");
    const auto beforeCollision = restored.GetEntities().size();
    write("{\"format_version\":2,\"entities\":[{\"name\":\"collision\",\"has_mesh_renderer\":false,\"persistent_id\":\"" + system->GetPersistentID() + "\",\"parent_persistent_id\":\"\"}]}");
    Check(!saves.LoadProject("input.scene", &restored) && restored.GetEntities().size() == beforeCollision && restored.IsAlive(systemHandle) && restored.GetEntityByPersistentID(persistentParent), "retained engine identity collision rejects and rolls back without losing content");
    const auto oldCount=restored.GetEntities().size();
    write(R"({"entities":[{"name":"missing","mesh_type":"obj","model_path":"missing.obj"}]})");
    Check(!saves.LoadProject("input.scene",&restored) && restored.GetEntities().size()==oldCount,"missing model rejects without changing scene");
    write(R"({"entities":[{"name":"malformed","position":[1 2 3]}]})");
    Check(!saves.LoadProject("input.scene",&restored),"malformed arrays reject");
    Check(saves.SaveProject("roundtrip",&scene),"atomic replacement succeeds");
    Check(std::filesystem::is_regular_file("saves/roundtrip.scene.bak"),"replacement retains backup");
    std::ifstream previous("saves/roundtrip.scene",std::ios::binary);std::ostringstream expected;expected<<previous.rdbuf();previous.close();
    auto verifyPrevious = [&]() { std::ifstream f("saves/roundtrip.scene",std::ios::binary);std::ostringstream text;text<<f.rdbuf();return text.str()==expected.str(); };
    auto* invalidLight = scene.GetEntities()[0]->AddComponent<Archura::LightComponent>();
    invalidLight->intensity = -1.0f;
    Check(!saves.SaveProject("roundtrip",&scene) && verifyPrevious(),"semantically invalid component fails save without overwrite");
    scene.GetEntities()[0]->RemoveComponent<Archura::LightComponent>();
    Archura::Shader assignedShader;
    mesh->shader = &assignedShader;
    Check(!saves.SaveProject("roundtrip",&scene) && verifyPrevious(),"assigned shader fails without overwriting previous file");
    mesh->shader = nullptr;
    Archura::Texture assignedTexture;
    mesh->texture = &assignedTexture;
    Check(!saves.SaveProject("roundtrip",&scene) && verifyPrevious(),"generated/source-less texture fails without overwrite");
    mesh->texture = nullptr;
    struct Unsupported final : Archura::Component {};
    scene.GetEntities()[0]->AddComponent<Unsupported>();
    Check(!saves.SaveProject("roundtrip",&scene) && verifyPrevious(),"unknown component fails without overwrite");
    scene.GetEntities()[0]->RemoveComponent<Unsupported>();
    auto* excluded = scene.CreateEntity("Player");
    excluded->SetOwnership(Archura::EntityOwnership::Engine);
    child->TrySetParent(excluded);
    Check(!saves.SaveProject("roundtrip",&scene) && verifyPrevious(),"omitted parent fails without overwrite");
    child->TrySetParent(scene.GetEntities()[0].get());
    excluded->TrySetParent(child);
    Check(!saves.SaveProject("roundtrip",&scene) && verifyPrevious(),"omitted child fails without overwrite");
    excluded->TrySetParent(nullptr);
    mesh->GetMesh()->GetVertices(); // Mutable access invalidates source metadata.
    Check(!saves.SaveProject("roundtrip",&scene),"custom/edited mesh save fails explicitly");
    std::ifstream retained("saves/roundtrip.scene",std::ios::binary);std::ostringstream actual;actual<<retained.rdbuf();retained.close();
    Check(actual.str()==expected.str(),"unsupported mesh never overwrites prior save");

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
        Check(saves.LoadProject("saves/authored_names.scene", &loaded), "authored system-like names load");
        Check(loaded.GetEntities().size() == 2, "Player/Floor authored entities must not disappear");
        if (loaded.GetEntities().size() == 2) {
            Check(loaded.GetEntities()[1]->GetParent() == loaded.GetEntities()[0].get(), "system-like name hierarchy survives");
            Check(loaded.GetEntities()[1]->GetComponent<Archura::Transform>()->position == glm::vec3(3, 4, 5), "system-like name transform survives");
        }
        auto* preservedEngine = loaded.CreateEntity("Runtime service with arbitrary name");
        preservedEngine->SetOwnership(Archura::EntityOwnership::Engine);
        preservedEngine->SetName("Renamed again");
        const auto serviceHandle = preservedEngine->GetHandle();
        for (int reload = 0; reload < 3; ++reload) {
            const auto replacedHandle = loaded.GetEntities().front()->GetHandle();
            Check(saves.LoadProject("saves/authored_names.scene", &loaded), "repeated content reload succeeds");
            Check(loaded.IsAlive(serviceHandle), "explicit engine ownership survives rename and reload");
            Check(loaded.GetEntities().size() == 3, "authored Player/Floor names do not accumulate across loads");
            if (replacedHandle != serviceHandle) Check(!loaded.IsAlive(replacedHandle), "authored old handles are invalidated");
        }
        write(R"({"entities":[{"name":"injected service","engine_owned":true,"has_mesh_renderer":false}]})");
        Check(!saves.LoadProject("input.scene", &loaded) && loaded.GetEntities().size() == 3 && loaded.IsAlive(serviceHandle), "scene content cannot claim permanent engine ownership");
        write(R"({"entities":[{"name":"invalid ownership","engine_owned":"true","has_mesh_renderer":false}]})");
        Check(!saves.LoadProject("input.scene", &loaded), "ownership metadata requires a boolean");
        write(R"({"entities":[{"name":"Player","has_mesh_renderer":false},{"name":"Floor","has_mesh_renderer":false}]})");
        Check(saves.LoadProject("input.scene", &loaded) && loaded.GetEntities().size() == 3, "legacy scene system-like names migrate as authored content");
        write(R"({"format_version":1,"entities":[{"name":"legacy parent","id":41,"has_mesh_renderer":false},{"name":"legacy child","id":42,"parent_id":41,"has_mesh_renderer":false}]})");
        Check(saves.LoadProject("input.scene", &loaded), "version-one numeric hierarchy migrates");
        Check(loaded.GetEntities()[2]->GetParent() == loaded.GetEntities()[1].get(), "numeric parent references survive migration");
        const auto migratedID = loaded.GetEntities()[1]->GetPersistentID();
        Check(saves.SaveProject("migrated", &loaded) && saves.LoadProject("saves/migrated.scene", &loaded), "migrated scene saves and reloads as version two");
        Check(loaded.GetEntities()[1]->GetPersistentID() == migratedID, "migrated identity stabilizes after its first version-two save");
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
                const auto* restoredWeapon = loaded.GetEntities()[0]->GetComponent<Archura::Weapon>();
                Check(restoredWeapon && restoredWeapon->stats.damage == 42.5f && restoredWeapon->stats.currentMag == 7,
                      "weapon active stats survive scene save");
                Check(restoredWeapon && restoredWeapon->stats.pattern.offsets == weapon->stats.pattern.offsets &&
                      restoredWeapon->inventory.size() == weapon->inventory.size(), "weapon inventory and recoil pattern survive scene save");
                Check(restoredWeapon && !restoredWeapon->isReloading && restoredWeapon->reloadTimer == 0,
                      "transient reload state resets on authored scene load");
                auto readArmed = [] { std::ifstream f("saves/armed.scene", std::ios::binary); std::ostringstream out; out << f.rdbuf(); return out.str(); };
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
                    Check(!saves.LoadProject("input.scene", &loaded) && loaded.IsAlive(oldArmedHandle),
                          "malformed weapon scene preserves original live handle");
                }
            }
        }
    }
    std::filesystem::current_path(original);
    // This unique directory was created exclusively by this test.
    std::error_code ec;std::filesystem::remove_all(work,ec);
    return failures?1:0;
}
