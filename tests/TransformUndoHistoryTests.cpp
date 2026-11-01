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

class ValueCommand final : public EditorHistoryCommand {
public:
    ValueCommand(const Entity& entity, float before, float after, size_t claimedBytes = 0)
        : id(entity.GetPersistentID()), before(before), after(after), claimedBytes(claimedBytes),
          selection(std::make_shared<const std::vector<std::string>>(std::vector<std::string>{id})) {}
    bool CanApply(const Scene& scene, bool undo) const override {
        const auto* entity = scene.GetEntityByPersistentID(id);
        const auto* transform = entity ? entity->GetComponent<Transform>() : nullptr;
        return available && transform && transform->position.x == (undo ? after : before);
    }
    ActionResult Apply(Scene& scene, bool undo) override {
        if (!CanApply(scene, undo) || (undo ? failUndo : failRedo))
            return {ActionResult::Status::Blocked, nullptr, {}};
        if (throwBeforeMutation) throw std::bad_alloc();
        auto* entity = scene.GetEntityByPersistentID(id);
        entity->GetComponent<Transform>()->position.x = undo ? before : after;
        ++applications;
        return {ActionResult::Status::Applied, nullSelection ? nullptr : entity, selection};
    }
    size_t ByteSize() const noexcept override {
        return claimedBytes && !(shrinkReservation && applications)
            ? claimedBytes : sizeof(*this) + 1024 + id.capacity() * 2;
    }
    const char* Label() const noexcept override { return "Value command"; }
    bool available = true, failUndo = false, failRedo = false;
    bool throwBeforeMutation = false, nullSelection = false, shrinkReservation = false;
    int applications = 0;
private:
    std::string id;
    float before, after;
    size_t claimedBytes;
    std::shared_ptr<const std::vector<std::string>> selection;
};

void TestUnifiedCommandChronologyAndSelection() {
    Scene scene;
    TransformUndoHistory history;
    auto* first = scene.CreateEntity("transform target");
    auto* second = scene.CreateEntity("generic target");
    auto* a = first->GetComponent<Transform>();
    auto* b = second->GetComponent<Transform>();
    CHECK(history.Capture(*first)); a->position.x = 1;
    auto command = std::make_shared<ValueCommand>(*second, 0.0f, 2.0f);
    CHECK(history.Execute(scene, command).status == ActionResult::Status::Applied);
    CHECK(a->position.x == 1 && b->position.x == 2);
    CHECK(std::string(history.UndoLabel(scene)) == "Value command");
    CHECK(history.Capture(*first)); a->position.x = 3;
    CHECK(std::string(history.UndoLabel(scene)) == "Transform");
    CHECK(history.UndoAction(scene).selectedEntity == first && a->position.x == 1);
    const auto genericUndo = history.UndoAction(scene);
    CHECK(genericUndo.status == ActionResult::Status::Applied && genericUndo.selectedEntity == second);
    CHECK(genericUndo.selectionIDs && genericUndo.selectionIDs->size() == 1 &&
          genericUndo.selectionIDs->front() == second->GetPersistentID());
    CHECK(a->position.x == 1 && b->position.x == 0);
    CHECK(history.Undo(scene) == first && a->position.x == 0);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Empty);
    CHECK(history.Redo(scene) == first && a->position.x == 1);
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Applied && b->position.x == 2);
    CHECK(history.Redo(scene) == first && a->position.x == 3);
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Empty);

    // A command that successfully deletes/clears a selection has no Entity*.
    history.Clear();
    auto withoutSelectedEntity = std::make_shared<ValueCommand>(*second, 2.0f, 4.0f);
    withoutSelectedEntity->nullSelection = true;
    const auto applied = history.Execute(scene, withoutSelectedEntity);
    CHECK(applied.status == ActionResult::Status::Applied && !applied.selectedEntity && applied.selectionIDs);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied && b->position.x == 2);
    CHECK(applied.selectionIDs->front() == second->GetPersistentID());
}

void TestFailedExecutePreservesPendingRedoAndBinding() {
    Scene scene;
    TransformUndoHistory history;
    auto* entity = scene.CreateEntity("pending");
    auto* other = scene.CreateEntity("failed command");
    auto* transform = entity->GetComponent<Transform>();
    CHECK(history.Capture(*entity)); transform->position.x = 1;
    CHECK(history.Undo(scene) == entity && history.CanRedo(scene));
    CHECK(history.Capture(*entity)); transform->position.x = 2;
    auto failure = std::make_shared<ValueCommand>(*other, 0.0f, 7.0f);
    failure->failRedo = true;
    CHECK(history.Execute(scene, failure).status == ActionResult::Status::Blocked);
    CHECK(transform->position.x == 2 && other->GetComponent<Transform>()->position.x == 0 && failure->applications == 0);
    // The failed command must not finalize the pending delta or consume old Redo.
    transform->position.x = 0;
    CHECK(history.CanRedo(scene) && !history.CanUndo(scene));
    CHECK(history.Redo(scene) == entity && transform->position.x == 1);
    CHECK(history.Undo(scene) == entity);
    CHECK(history.Capture(*entity));
    failure->failRedo = false;
    failure->throwBeforeMutation = true;
    CHECK(history.Execute(scene, failure).status == ActionResult::Status::Blocked);
    transform->position.x = 3;
    CHECK(history.Undo(scene) == entity && transform->position.x == 0);
    CHECK(history.Redo(scene) == entity && transform->position.x == 3);

    Scene foreign;
    auto* foreignEntity = foreign.CreateEntity("foreign");
    auto foreignFailure = std::make_shared<ValueCommand>(*foreignEntity, 0.0f, 1.0f);
    foreignFailure->failRedo = true;
    CHECK(history.Execute(foreign, foreignFailure).status == ActionResult::Status::Blocked);
    CHECK(history.CanUndo(scene) && !history.CanUndo(foreign));
    CHECK(history.Undo(scene) == entity && transform->position.x == 0);
}

void TestGenericFailureIsChronologyBarrier() {
    Scene scene;
    TransformUndoHistory history;
    auto* older = scene.CreateEntity("older transform");
    auto* target = scene.CreateEntity("generic barrier");
    auto* pending = scene.CreateEntity("pending activation");
    CHECK(history.Capture(*older)); older->GetComponent<Transform>()->position.x = 1;
    auto command = std::make_shared<ValueCommand>(*target, 0.0f, 4.0f);
    CHECK(history.Execute(scene, command).status == ActionResult::Status::Applied);
    CHECK(history.Capture(*pending));
    command->available = false;
    CHECK(!history.CanUndo(scene));
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Blocked);
    CHECK(older->GetComponent<Transform>()->position.x == 1 && target->GetComponent<Transform>()->position.x == 4);
    pending->GetComponent<Transform>()->position.x = 2;
    CHECK(history.Undo(scene) == pending && pending->GetComponent<Transform>()->position.x == 0);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Blocked);
    command->available = true;
    command->failUndo = true;
    CHECK(history.CanUndo(scene));
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Blocked);
    CHECK(target->GetComponent<Transform>()->position.x == 4);
    command->failUndo = false;
    command->throwBeforeMutation = true;
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Blocked);
    command->throwBeforeMutation = false;
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied);
    CHECK(target->GetComponent<Transform>()->position.x == 0);
    command->failRedo = true;
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Blocked);
    CHECK(target->GetComponent<Transform>()->position.x == 0);
    command->failRedo = false;
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Applied);
    CHECK(target->GetComponent<Transform>()->position.x == 4);
    // Actual new generic execution, unlike a failed one, starts a new branch.
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied);
    auto branch = std::make_shared<ValueCommand>(*target, 0.0f, 9.0f);
    CHECK(history.Execute(scene, branch).status == ActionResult::Status::Applied);
    CHECK(!history.CanRedo(scene) && history.RedoAction(scene).status == ActionResult::Status::Empty);
}

void TestCommandMemoryAndCountBounds() {
    Scene scene;
    TransformUndoHistory history;
    auto* entity = scene.CreateEntity("bounded generic commands");
    auto* transform = entity->GetComponent<Transform>();
    auto oversized = std::make_shared<ValueCommand>(*entity, 0.0f, 1.0f, TransformUndoHistory::MaxHistoryBytes);
    CHECK(history.Execute(scene, oversized).status == ActionResult::Status::Blocked);
    CHECK(transform->position.x == 0 && oversized->applications == 0 && !history.CanUndo(scene));
    for (int index = 0; index != 3; ++index) {
        // Claims are conservative upper bounds, avoiding large allocations in
        // this pure journal test while still exercising byte-budget eviction.
        auto command = std::make_shared<ValueCommand>(*entity, static_cast<float>(index),
                                                      static_cast<float>(index + 1), 11u * 1024u * 1024u);
        CHECK(history.Execute(scene, command).status == ActionResult::Status::Applied);
        CHECK(history.RetainedBytes() <= TransformUndoHistory::MaxHistoryBytes);
    }
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied && transform->position.x == 2);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied && transform->position.x == 1);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Empty);
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Applied);
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Applied && transform->position.x == 3);
    history.Clear();
    transform->position.x = 0;
    for (int index = 0; index != 60; ++index) {
        auto command = std::make_shared<ValueCommand>(*entity, static_cast<float>(index), static_cast<float>(index + 1));
        CHECK(history.Execute(scene, command).status == ActionResult::Status::Applied);
    }
    for (int index = 0; index != 50; ++index) CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied);
    CHECK(transform->position.x == 10 && history.UndoAction(scene).status == ActionResult::Status::Empty);
    for (int index = 0; index != 50; ++index) CHECK(history.RedoAction(scene).status == ActionResult::Status::Applied);
    CHECK(transform->position.x == 60 && history.RedoAction(scene).status == ActionResult::Status::Empty);

    history.Clear();
    auto nearlyFull = std::make_shared<ValueCommand>(*entity, 60.0f, 61.0f, TransformUndoHistory::MaxHistoryBytes - 4096);
    CHECK(history.Execute(scene, nearlyFull).status == ActionResult::Status::Applied);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied);
    CHECK(history.Capture(*entity));
    CHECK(history.CanRedo(scene) && history.RetainedBytes() <= TransformUndoHistory::MaxHistoryBytes);
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Applied && transform->position.x == 61);
    CHECK(history.RetainedBytes() <= TransformUndoHistory::MaxHistoryBytes);

    history.Clear();
    transform->position.x = 0;
    for (int index = 0; index != 60; ++index) {
        auto command = std::make_shared<ValueCommand>(*entity, static_cast<float>(index),
                                                      static_cast<float>(index + 1), 8u * 1024u * 1024u);
        command->shrinkReservation = true;
        CHECK(history.Execute(scene, command).status == ActionResult::Status::Applied);
    }
    // Initial 8 MiB reservations are replaced with actual captured payload sizes
    // before eviction, rather than retaining only three small creation commands.
    for (int index = 0; index != 50; ++index) CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied);
    CHECK(transform->position.x == 10 && history.UndoAction(scene).status == ActionResult::Status::Empty);
    CHECK(history.RetainedBytes() <= TransformUndoHistory::MaxHistoryBytes);
}

void TestGenericCommandAndTransformRebinding() {
    Scene scene;
    TransformUndoHistory history;
    auto* entity = scene.CreateEntity("mixed restored generation");
    CHECK(history.Capture(*entity)); entity->GetComponent<Transform>()->position.x = 1;
    auto command = std::make_shared<ValueCommand>(*entity, 1.0f, 2.0f);
    CHECK(history.Execute(scene, command).status == ActionResult::Status::Applied);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied);
    entity = ReplacePreservingID(scene, entity);
    history.RebindRestoredScene(scene);
    CHECK(history.RedoAction(scene).status == ActionResult::Status::Applied && entity->GetComponent<Transform>()->position.x == 2);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Applied && entity->GetComponent<Transform>()->position.x == 1);
    CHECK(history.Undo(scene) == entity && entity->GetComponent<Transform>()->position.x == 0);
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
    TestUnifiedCommandChronologyAndSelection();
    TestFailedExecutePreservesPendingRedoAndBinding();
    TestGenericFailureIsChronologyBarrier();
    TestCommandMemoryAndCountBounds();
    TestGenericCommandAndTransformRebinding();
    if (failures) return 1;
    std::cout << "Unified history: transforms, generic chronology, rollback, budgets, identity and restored generations passed\n";
    return 0;
}
