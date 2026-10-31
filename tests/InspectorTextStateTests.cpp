#include "editor/InspectorTextState.h"
#include <cstring>
#include <iostream>
#include <new>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << ": failed: " << #condition << '\n'; ++failures; } } while (false)

void TestDraftsAndExternalChanges() {
    Archura::Scene scene;
    auto* entity = scene.CreateEntity("First");
    auto* component = entity->AddComponent<Archura::ScriptComponent>();
    component->className = "Gameplay.First";
    Archura::InspectorTextState state;
    const auto sync = [&] {
        state.Sync(&scene, entity->GetHandle(), entity->GetName(), &component->className);
    };
    sync();
    CHECK(std::strcmp(state.name, "First") == 0);
    CHECK(std::strcmp(state.script, "Gameplay.First") == 0);
    std::strcpy(state.name, "Name draft");
    std::strcpy(state.script, "Script draft");
    sync();
    CHECK(std::strcmp(state.name, "Name draft") == 0);
    CHECK(std::strcmp(state.script, "Script draft") == 0);

    entity->SetName("Externally renamed");
    sync();
    CHECK(std::strcmp(state.name, "Externally renamed") == 0);
    CHECK(std::strcmp(state.script, "Script draft") == 0);
    component->className = "Gameplay.External";
    sync();
    CHECK(std::strcmp(state.script, "Gameplay.External") == 0);

    std::strcpy(state.newScript, "Transient new component draft");
    entity->RemoveComponent<Archura::ScriptComponent>();
    state.Sync(&scene, entity->GetHandle(), entity->GetName(), nullptr);
    CHECK(state.script[0] == '\0' && state.newScript[0] == '\0');
    std::strcpy(state.newScript, "Gameplay.ToAdd");
    state.Sync(&scene, entity->GetHandle(), entity->GetName(), nullptr);
    CHECK(std::strcmp(state.newScript, "Gameplay.ToAdd") == 0);
    component = entity->AddComponent<Archura::ScriptComponent>();
    component->className = "Gameplay.Added";
    sync();
    CHECK(std::strcmp(state.script, "Gameplay.Added") == 0);
    CHECK(state.newScript[0] == '\0');
}

void TestSelectionAndEntityLifetime() {
    Archura::Scene scene;
    auto* first = scene.CreateEntity("First");
    auto* second = scene.CreateEntity("Second");
    Archura::InspectorTextState state;
    state.Sync(&scene, first->GetHandle(), first->GetName(), nullptr);
    std::strcpy(state.name, "first draft");
    std::strcpy(state.newScript, "first script draft");
    state.Sync(&scene, second->GetHandle(), second->GetName(), nullptr);
    CHECK(std::strcmp(state.name, "Second") == 0);
    CHECK(state.newScript[0] == '\0');
    const auto deadHandle = second->GetHandle();
    scene.DestroyEntity(deadHandle);
    auto* replacement = scene.CreateEntity("Replacement");
    state.Sync(&scene, deadHandle, "Deleted", nullptr);
    CHECK(state.name[0] == '\0');
    state.Sync(&scene, replacement->GetHandle(), replacement->GetName(), nullptr);
    CHECK(std::strcmp(state.name, "Replacement") == 0);
    state.Sync(nullptr, {}, "Ignored", nullptr);
    CHECK(state.name[0] == '\0' && state.script[0] == '\0' && state.newScript[0] == '\0');
}

void TestSceneAddressReuseAndBounds() {
    alignas(Archura::Scene) unsigned char storage[sizeof(Archura::Scene)];
    Archura::InspectorTextState state;
    auto* scene = new (storage) Archura::Scene;
    auto* first = scene->CreateEntity("Same source");
    const auto previousHandle = first->GetHandle();
    state.Sync(scene, previousHandle, first->GetName(), nullptr);
    std::strcpy(state.name, "draft from destroyed scene");
    scene->~Scene();

    scene = new (storage) Archura::Scene;
    auto* second = scene->CreateEntity("Same source");
    // Force the difficult case: identical address, handle and source text.
    CHECK(second->GetHandle() == previousHandle);
    state.Sync(scene, second->GetHandle(), second->GetName(), nullptr);
    CHECK(std::strcmp(state.name, "Same source") == 0);
    const std::string longName(400, 'N'), longScript(400, 'S');
    state.Sync(scene, second->GetHandle(), longName, &longScript);
    CHECK(std::strlen(state.name) == 127 && state.name[127] == '\0');
    CHECK(std::strlen(state.script) == 127 && state.script[127] == '\0');
    std::strcpy(state.name, "draft after truncation");
    state.Sync(scene, second->GetHandle(), longName, &longScript);
    CHECK(std::strcmp(state.name, "draft after truncation") == 0);
    scene->~Scene();
    state.Sync(nullptr, {}, "", nullptr);
}

void TestCommitPendingDrafts() {
    Archura::Scene scene;
    auto* entity = scene.CreateEntity("Authored");
    auto* component = entity->AddComponent<Archura::ScriptComponent>();
    component->className = "Gameplay.Authored";
    Archura::InspectorTextState state;
    state.Sync(&scene, entity->GetHandle(), entity->GetName(), &component->className);
    std::strcpy(state.name, "Pending name");
    std::strcpy(state.script, "Gameplay.Pending");
    Archura::Scene other;
    state.CommitPending(&other);
    CHECK(entity->GetName() == "Authored" && component->className == "Gameplay.Authored");
    state.CommitPending(nullptr);
    state.CommitPending(&scene);
    CHECK(entity->GetName() == "Pending name" && component->className == "Gameplay.Pending");

    state.Sync(&scene, entity->GetHandle(), entity->GetName(), &component->className);
    std::strcpy(state.name, "Outdated name draft");
    std::strcpy(state.script, "Gameplay.OutdatedDraft");
    entity->SetName("External name");
    component->className = "Gameplay.External";
    state.CommitPending(&scene);
    CHECK(entity->GetName() == "External name" && component->className == "Gameplay.External");

    state.Sync(&scene, entity->GetHandle(), entity->GetName(), &component->className);
    state.name[0] = '\0';
    state.CommitPending(&scene);
    CHECK(entity->GetName() == "External name");
    entity->RemoveComponent<Archura::ScriptComponent>();
    state.Sync(&scene, entity->GetHandle(), entity->GetName(), nullptr);
    std::strcpy(state.newScript, "Gameplay.NotYetAdded");
    state.CommitPending(&scene);
    CHECK(!entity->GetComponent<Archura::ScriptComponent>());

    std::strcpy(state.name, "Deleted entity draft");
    scene.DestroyEntity(entity->GetHandle());
    auto* replacement = scene.CreateEntity("Replacement");
    state.CommitPending(&scene);
    CHECK(replacement->GetName() == "Replacement");

    alignas(Archura::Scene) unsigned char storage[sizeof(Archura::Scene)];
    auto* reusedScene = new (storage) Archura::Scene;
    auto* oldEntity = reusedScene->CreateEntity("Same name");
    const auto oldHandle = oldEntity->GetHandle();
    state.Sync(reusedScene, oldHandle, oldEntity->GetName(), nullptr);
    std::strcpy(state.name, "Destroyed scene draft");
    reusedScene->~Scene();
    reusedScene = new (storage) Archura::Scene;
    auto* newEntity = reusedScene->CreateEntity("Same name");
    CHECK(newEntity->GetHandle() == oldHandle);
    state.CommitPending(reusedScene);
    CHECK(newEntity->GetName() == "Same name");
    reusedScene->~Scene();
}

void TestCommitPreservesUntouchedLongSources() {
    Archura::Scene scene;
    const std::string longName(400, 'N'), longScript(400, 'S');
    auto* entity = scene.CreateEntity(longName);
    auto* component = entity->AddComponent<Archura::ScriptComponent>();
    component->className = longScript;
    Archura::InspectorTextState state;
    state.Sync(&scene, entity->GetHandle(), entity->GetName(), &component->className);
    CHECK(std::strlen(state.name) == 127 && std::strlen(state.script) == 127);
    state.CommitPending(&scene);
    state.CommitPending(&scene);
    CHECK(entity->GetName() == longName && component->className == longScript);

    // Editing the last displayed character still counts as an intentional edit,
    // even when both draft and displayed source occupy the entire field.
    state.name[126] = 'X';
    state.script[126] = 'Y';
    state.CommitPending(&scene);
    CHECK(entity->GetName() == std::string(126, 'N') + 'X');
    CHECK(component->className == std::string(126, 'S') + 'Y');
}
}

int main() {
    TestDraftsAndExternalChanges();
    TestSelectionAndEntityLifetime();
    TestSceneAddressReuseAndBounds();
    TestCommitPendingDrafts();
    TestCommitPreservesUntouchedLongSources();
    if (failures) return 1;
    std::cout << "Inspector text: drafts, external edits, components and scene/entity lifetime passed\n";
    return 0;
}
