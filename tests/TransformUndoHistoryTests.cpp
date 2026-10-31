#include "editor/TransformUndoHistory.h"

#include <iostream>
#include <new>

namespace {
using namespace Archura;
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": failed: " << #condition << '\n'; ++failures; } } while (false)

void TestFullTransformAndRoundTrips() {
    Scene scene;
    TransformUndoHistory history;
    auto* entity = scene.CreateEntity("edited");
    auto* transform = entity->GetComponent<Transform>();
    transform->position = {1, 2, 3};
    transform->rotation = {4, 5, 6};
    transform->scale = {2, 3, 4};
    CHECK(history.Capture(*entity));
    CHECK(!history.CanUndo(scene) && !history.CanRedo(scene));
    transform->position = {7, 8, 9};
    transform->rotation = {10, 11, 12};
    transform->scale = {5, 6, 7};
    CHECK(history.CanUndo(scene) && !history.CanRedo(scene));
    for (int cycle = 0; cycle != 10; ++cycle) {
        CHECK(history.Undo(scene) == entity);
        CHECK(transform->position == glm::vec3(1, 2, 3) && transform->rotation == glm::vec3(4, 5, 6) && transform->scale == glm::vec3(2, 3, 4));
        CHECK(!history.CanUndo(scene) && history.CanRedo(scene));
        CHECK(history.Redo(scene) == entity);
        CHECK(transform->position == glm::vec3(7, 8, 9) && transform->rotation == glm::vec3(10, 11, 12) && transform->scale == glm::vec3(5, 6, 7));
        CHECK(history.CanUndo(scene) && !history.CanRedo(scene));
    }
    history.Clear();
    CHECK(!history.CanUndo(scene) && !history.CanRedo(scene) && !history.Undo(scene) && !history.Redo(scene));
}

void TestDifferentEntitiesAndInverseCurrentValues() {
    Scene scene;
    TransformUndoHistory history;
    auto* first = scene.CreateEntity("first");
    auto* second = scene.CreateEntity("second");
    auto* a = first->GetComponent<Transform>();
    auto* b = second->GetComponent<Transform>();
    CHECK(history.Capture(*first)); a->position.x = 1;
    CHECK(history.Capture(*second)); b->rotation.y = 2;
    CHECK(history.Capture(*first)); a->scale.z = 3;
    CHECK(history.Undo(scene) == first && a->scale.z == 1 && a->position.x == 1);
    CHECK(history.Undo(scene) == second && b->rotation.y == 0);
    CHECK(history.Undo(scene) == first && a->position.x == 0);
    CHECK(!history.Undo(scene));
    CHECK(history.Redo(scene) == first && a->position.x == 1);
    CHECK(history.Redo(scene) == second && b->rotation.y == 2);
    CHECK(history.Redo(scene) == first && a->scale.z == 3);
    CHECK(!history.Redo(scene));

    // Even if an uncaptured external value changed, the opposite direction
    // records the actual current value rather than inventing a stale inverse.
    CHECK(history.Undo(scene) == first && a->scale.z == 1);
    a->scale.z = 42;
    CHECK(history.Redo(scene) == first && a->scale.z == 3);
    CHECK(history.Undo(scene) == first && a->scale.z == 42);
}

void TestNoOpCaptureQueriesAndBranching() {
    Scene scene;
    TransformUndoHistory history;
    auto* entity = scene.CreateEntity("gesture");
    auto* transform = entity->GetComponent<Transform>();
    CHECK(history.Capture(*entity));
    for (int frame = 0; frame < 5; ++frame) CHECK(!history.CanUndo(scene) && !history.CanRedo(scene));
    // Querying during the activation frame must not discard the pending edit.
    transform->position.x = 1;
    transform->position.x = 2;
    transform->position.x = 3;
    CHECK(history.Undo(scene) == entity && transform->position.x == 0);
    CHECK(!history.Undo(scene) && history.CanRedo(scene));
    CHECK(history.Capture(*entity));
    for (int frame = 0; frame < 5; ++frame) CHECK(!history.CanUndo(scene) && history.CanRedo(scene));
    CHECK(history.Capture(*entity)); // Finalize an unchanged activation.
    CHECK(history.CanRedo(scene));
    CHECK(history.Redo(scene) == entity && transform->position.x == 3);
    CHECK(history.Capture(*entity));
    CHECK(history.CanUndo(scene));
    CHECK(history.Undo(scene) == entity && transform->position.x == 0);
    CHECK(!history.Undo(scene) && history.CanRedo(scene));

    CHECK(history.Capture(*entity));
    CHECK(history.CanRedo(scene));
    transform->position.x = 8;
    CHECK(history.CanUndo(scene) && !history.CanRedo(scene));
    CHECK(!history.Redo(scene)); // Finalizes the actual branch and drops old Redo.
    CHECK(history.Undo(scene) == entity && transform->position.x == 0);
    CHECK(history.Redo(scene) == entity && transform->position.x == 8);
    CHECK(!history.Redo(scene));

    // An interaction that changed and returned exactly to its start is a no-op.
    CHECK(history.Undo(scene) == entity && transform->position.x == 0);
    CHECK(history.Capture(*entity));
    transform->position.x = 2;
    transform->position.x = 0;
    CHECK(history.CanRedo(scene));
    CHECK(history.Redo(scene) == entity && transform->position.x == 8);
}

void TestBoundedHistory() {
    Scene scene;
    TransformUndoHistory history;
    auto* entity = scene.CreateEntity("bounded");
    auto* transform = entity->GetComponent<Transform>();
    for (int index = 0; index != 60; ++index) {
        CHECK(history.Capture(*entity));
        transform->position.x = static_cast<float>(index + 1);
    }
    for (int index = 0; index != 50; ++index) {
        CHECK(history.CanUndo(scene));
        CHECK(history.Undo(scene) == entity);
        CHECK(transform->position.x == static_cast<float>(59 - index));
    }
    CHECK(transform->position.x == 10 && !history.CanUndo(scene) && !history.Undo(scene));
    for (int index = 0; index != 50; ++index) {
        CHECK(history.CanRedo(scene));
        CHECK(history.Redo(scene) == entity);
        CHECK(transform->position.x == static_cast<float>(11 + index));
    }
    CHECK(transform->position.x == 60 && !history.CanRedo(scene) && !history.Redo(scene));
}

void TestDeletedComponentsEntitiesAndForeignScenes() {
    Scene scene;
    TransformUndoHistory history;
    auto* first = scene.CreateEntity("valid older command");
    auto* firstTransform = first->GetComponent<Transform>();
    CHECK(history.Capture(*first)); firstTransform->position.x = 1;
    auto* deleted = scene.CreateEntity("deleted pending command");
    CHECK(history.Capture(*deleted)); deleted->GetComponent<Transform>()->position.x = 2;
    const auto deadHandle = deleted->GetHandle();
    scene.DestroyEntity(deadHandle);
    auto* replacement = scene.CreateEntity("replacement");
    CHECK(history.Undo(scene) == first && firstTransform->position.x == 0);
    CHECK(replacement->GetComponent<Transform>()->position == glm::vec3(0));
    history.Clear();
    CHECK(history.Capture(*replacement)); replacement->GetComponent<Transform>()->position.x = 5;
    CHECK(history.Undo(scene) == replacement);
    replacement->RemoveComponent<Transform>();
    CHECK(!history.CanRedo(scene) && !history.Redo(scene));
    replacement->AddComponent<Transform>();
    CHECK(!history.Redo(scene));
    CHECK(history.Capture(*replacement)); replacement->RemoveComponent<Transform>();
    CHECK(!history.Undo(scene));
    replacement->AddComponent<Transform>();

    Scene other;
    auto* foreign = other.CreateEntity("same initial handle in another scene");
    CHECK(history.Capture(*replacement)); replacement->GetComponent<Transform>()->position.x = 6;
    CHECK(!history.CanUndo(other) && !history.CanRedo(other));
    CHECK(history.Undo(scene) == replacement); // Foreign queries have no effects.
    CHECK(history.CanRedo(scene));
    CHECK(history.Capture(*foreign)); // A scene switch clears both directions.
    CHECK(!history.CanUndo(scene) && !history.CanRedo(scene));
    foreign->GetComponent<Transform>()->position.x = 3;
    CHECK(history.Undo(other) == foreign);
    CHECK(history.CanRedo(other));
    CHECK(!history.Redo(scene)); // A mutation command against a foreign Scene rejects and clears.
    CHECK(!history.CanRedo(other));
    Entity standalone(1, "without a Scene");
    standalone.AddComponent<Transform>();
    CHECK(!history.Capture(standalone));
}

void TestSceneLifetimeAndAddressReuse() {
    alignas(Scene) unsigned char storage[sizeof(Scene)];
    TransformUndoHistory history;
    auto* firstScene = new (storage) Scene;
    auto* first = firstScene->CreateEntity("old scene entity");
    const auto originalHandle = first->GetHandle();
    const auto lifetime = firstScene->GetLifetimeToken();
    CHECK(history.Capture(*first)); first->GetComponent<Transform>()->position.x = 10;
    CHECK(history.Undo(*firstScene) == first);
    CHECK(history.CanRedo(*firstScene));
    firstScene->~Scene();
    CHECK(lifetime.expired()); // History retains no strong Scene ownership.
    auto* secondScene = new (storage) Scene;
    auto* replacement = secondScene->CreateEntity("new scene entity");
    CHECK(replacement->GetHandle() == originalHandle);
    CHECK(!history.CanUndo(*secondScene) && !history.CanRedo(*secondScene));
    history.RebindRestoredScene(*secondScene);
    CHECK(!history.Redo(*secondScene) && replacement->GetComponent<Transform>()->position.x == 0);
    CHECK(history.Capture(*replacement)); replacement->GetComponent<Transform>()->position.x = 4;
    CHECK(history.Undo(*secondScene) == replacement);
    secondScene->~Scene();
}

Entity* ReplacePreservingID(Scene& scene, Entity* original) {
    const auto handle = original->GetHandle();
    const auto persistentID = original->GetPersistentID();
    const auto* transform = original->GetComponent<Transform>();
    auto* staged = scene.CreateEntity("restored generation");
    auto* copy = staged->GetComponent<Transform>();
    copy->position = transform->position;
    copy->rotation = transform->rotation;
    copy->scale = transform->scale;
    CHECK(scene.CommitEntityReplacement({handle}, {staged->GetHandle()}, {persistentID}));
    return scene.GetEntityByPersistentID(persistentID);
}

void TestRebindUndoRedoAndPending() {
    Scene scene;
    TransformUndoHistory history;
    auto* entity = scene.CreateEntity("rebound");
    CHECK(history.Capture(*entity)); entity->GetComponent<Transform>()->position.x = 1;
    CHECK(history.Capture(*entity)); entity->GetComponent<Transform>()->position.x = 2;
    CHECK(history.Undo(scene) == entity && entity->GetComponent<Transform>()->position.x == 1);
    const auto oldHandle = entity->GetHandle();
    entity = ReplacePreservingID(scene, entity);
    CHECK(entity && entity->GetHandle() != oldHandle);
    history.RebindRestoredScene(scene);
    CHECK(history.CanUndo(scene) && history.CanRedo(scene));
    CHECK(history.Redo(scene) == entity && entity->GetComponent<Transform>()->position.x == 2);
    CHECK(history.Undo(scene) == entity && entity->GetComponent<Transform>()->position.x == 1);
    CHECK(history.Undo(scene) == entity && entity->GetComponent<Transform>()->position.x == 0);

    CHECK(history.Capture(*entity));
    CHECK(history.CanRedo(scene));
    entity->GetComponent<Transform>()->scale = {2, 3, 4};
    entity = ReplacePreservingID(scene, entity);
    history.RebindRestoredScene(scene);
    CHECK(history.Undo(scene) == entity && entity->GetComponent<Transform>()->scale == glm::vec3(1));
    CHECK(history.Redo(scene) == entity && entity->GetComponent<Transform>()->scale == glm::vec3(2, 3, 4));
    scene.DestroyEntity(entity->GetHandle());
    auto* unrelated = scene.CreateEntity("not the restored persistent ID");
    history.RebindRestoredScene(scene);
    CHECK(!history.CanUndo(scene) && !history.CanRedo(scene));
    CHECK(!history.Undo(scene) && !history.Redo(scene));
    CHECK(unrelated->GetComponent<Transform>()->position == glm::vec3(0));
}
} // namespace

int main() {
    TestFullTransformAndRoundTrips();
    TestDifferentEntitiesAndInverseCurrentValues();
    TestNoOpCaptureQueriesAndBranching();
    TestBoundedHistory();
    TestDeletedComponentsEntitiesAndForeignScenes();
    TestSceneLifetimeAndAddressReuse();
    TestRebindUndoRedoAndPending();
    if (failures) return 1;
    std::cout << "Transform Undo/Redo: gestures, branches, bounds, identity and restored generations passed\n";
    return 0;
}
