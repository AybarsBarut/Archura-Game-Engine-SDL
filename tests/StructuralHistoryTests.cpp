#include "editor/HierarchyEditCommand.h"
#include "editor/ComponentEditCommand.h"

#include <cstdlib>
#include <iostream>
#include <new>
#include <sstream>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace AllocationFailure { thread_local int after = -1; }
void* operator new(std::size_t size) {
  bool injectable = true;
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
  // Match ECSCoreTests: Debug STL proxy allocations can occur in noexcept
  // vendor constructors. Production code contains no allocation test hook.
  injectable = size != sizeof(std::_Container_proxy);
#endif
  if (injectable && AllocationFailure::after == 0) {
    AllocationFailure::after = -1;
    throw std::bad_alloc();
  }
  if (injectable && AllocationFailure::after > 0) --AllocationFailure::after;
  if (void* memory = std::malloc(size ? size : 1)) return memory;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace Archura;
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": failed: " << #condition << '\n'; ++failures; } } while (false)
bool Applied(const ActionResult& result) { return result.status == ActionResult::Status::Applied; }
void HierarchyRoundTrip() {
    Scene scene;
    auto* oldParent = scene.CreateEntity("old parent");
    auto* newParent = scene.CreateEntity("new parent");
    auto* a = scene.CreateEntity("a");
    auto* b = scene.CreateEntity("b");
    auto* c = scene.CreateEntity("c");
    auto* retained = scene.CreateEntity("retained");
    CHECK(c->TrySetParent(oldParent)); CHECK(a->TrySetParent(oldParent)); CHECK(b->TrySetParent(oldParent));
    CHECK(retained->TrySetParent(newParent));
    a->GetComponent<Transform>()->position = {1,2,3};
    const auto aHandle = a->GetHandle(), bHandle = b->GetHandle();
    const std::vector<Entity*> before{c,a,b}, after{retained,b,a};
    TransformUndoHistory history;
    const auto command = HierarchyEditCommand::Create(scene, {bHandle,aHandle}, newParent->GetHandle(), {a->GetPersistentID(), b->GetPersistentID()}, a->GetPersistentID());
    CHECK(command && Applied(history.Execute(scene, command)));
    CHECK(oldParent->GetChildren() == std::vector<Entity*>{c} && newParent->GetChildren() == after);
    CHECK(scene.GetEntity(aHandle) == a && scene.GetEntity(bHandle) == b && a->GetComponent<Transform>()->position == glm::vec3(1,2,3));
    auto result = history.UndoAction(scene);
    CHECK(Applied(result) && result.selectedEntity == a && result.selectionIDs && result.selectionIDs->size() == 2);
    CHECK(oldParent->GetChildren() == before && newParent->GetChildren() == std::vector<Entity*>{retained});
    CHECK(Applied(history.RedoAction(scene)) && newParent->GetChildren() == after);
    auto unparent = HierarchyEditCommand::Create(scene, {aHandle,bHandle}, {});
    CHECK(Applied(history.Execute(scene, unparent)) && !a->GetParent() && !b->GetParent());
    CHECK(Applied(history.UndoAction(scene)) && newParent->GetChildren() == after);
    // A no-op cannot consume the existing redo branch.
    CHECK(!HierarchyEditCommand::Create(scene, {aHandle}, newParent->GetHandle()) && history.CanRedo(scene));
    CHECK(!HierarchyEditCommand::Create(scene, {aHandle}, aHandle));
    auto cycle = HierarchyEditCommand::Create(scene, {newParent->GetHandle()}, aHandle);
    const auto bytes = history.RetainedBytes();
    CHECK(history.Execute(scene, cycle).status == ActionResult::Status::Blocked && history.RetainedBytes() == bytes && history.CanRedo(scene));
    CHECK(newParent->GetParent() == nullptr && newParent->GetChildren() == after);
    // Unrelated hierarchy changes remain untouched; affected siblings block.
    auto* other = scene.CreateEntity("other");
    auto* unrelatedChild = scene.CreateEntity("unrelated child");
    unrelatedChild->TrySetParent(other);
    CHECK(Applied(history.RedoAction(scene)) && unrelatedChild->GetParent() == other);
    auto* extra = scene.CreateEntity("external sibling");
    extra->TrySetParent(newParent);
    CHECK(!history.CanUndo(scene) && history.UndoAction(scene).status == ActionResult::Status::Blocked);
    extra->TrySetParent(nullptr);
    CHECK(Applied(history.UndoAction(scene)) && newParent->GetChildren() == after);
    Scene foreign;
    CHECK(!command->CanApply(foreign, true));
    scene.ForEachEntity([&](Entity&) { CHECK(!command->CanApply(scene, true)); });
}
void NestedUnparentAndMixedHistory() {
    Scene scene;
    auto* grand = scene.CreateEntity("grand");
    auto* parent = scene.CreateEntity("parent");
    auto* child = scene.CreateEntity("child");
    auto* sibling = scene.CreateEntity("sibling");
    parent->TrySetParent(grand); child->TrySetParent(parent); sibling->TrySetParent(grand);
    TransformUndoHistory history;
    history.Capture(*child); child->GetComponent<Transform>()->scale = {2,3,4};
    CHECK(Applied(history.Execute(scene, HierarchyEditCommand::Create(scene, {parent->GetHandle(),child->GetHandle()}, {}))));
    CHECK(!parent->GetParent() && !child->GetParent());
    CHECK(Applied(history.UndoAction(scene)) && parent->GetParent() == grand && child->GetParent() == parent);
    CHECK(grand->GetChildren() == (std::vector<Entity*>{parent,sibling}));
    CHECK(Applied(history.UndoAction(scene)) && child->GetComponent<Transform>()->scale == glm::vec3(1));
    CHECK(Applied(history.RedoAction(scene)) && child->GetComponent<Transform>()->scale == glm::vec3(2,3,4));
    CHECK(Applied(history.RedoAction(scene)) && !child->GetParent());
    auto stale = HierarchyEditCommand::Create(scene, {child->GetHandle()}, parent->GetHandle());
    scene.DestroyEntity(parent->GetHandle());
    CHECK(!stale->CanApply(scene, false));
}
void ComponentsRoundTrip() {
    Scene scene;
    auto* entity = scene.CreateEntity("entity");
    const auto handle = entity->GetHandle();
    auto* transform = entity->GetComponent<Transform>();
    TransformUndoHistory history;
    RigidBody body;
    body.mass = 13; body.angularDrag = 0.7f; body.velocity = {1,2,3}; body.force = {4,5,6};
    body.angularVelocity = {7,8,9}; body.isKinematic = true; body.canBePushed = false;
    auto add = ComponentEditCommand<RigidBody>::Create(scene, handle, body);
    CHECK(add && Applied(history.Execute(scene, add)));
    CHECK(ComponentHistory::Equal(*entity->GetComponent<RigidBody>(), body));
    CHECK(entity->GetComponent<Transform>() == transform && scene.GetEntity(handle) == entity);
    auto remove = ComponentEditCommand<RigidBody>::Create(scene, handle, std::nullopt);
    CHECK(Applied(history.Execute(scene, remove)) && !entity->HasComponent<RigidBody>());
    CHECK(Applied(history.UndoAction(scene)) && ComponentHistory::Equal(*entity->GetComponent<RigidBody>(), body));
    CHECK(Applied(history.UndoAction(scene)) && !entity->HasComponent<RigidBody>());
    CHECK(Applied(history.RedoAction(scene)) && ComponentHistory::Equal(*entity->GetComponent<RigidBody>(), body));
    CHECK(!ComponentEditCommand<RigidBody>::Create(scene, handle, body) && history.CanRedo(scene));
    entity->GetComponent<RigidBody>()->mass = 99;
    CHECK(!history.CanRedo(scene) && history.RedoAction(scene).status == ActionResult::Status::Blocked);
    entity->GetComponent<RigidBody>()->mass = 13;
    CHECK(Applied(history.RedoAction(scene)));
    BoxCollider collider;
    collider.shape = BoxCollider::Shape::Sphere; collider.radius = 4; collider.center = {1,0,2}; collider.isTrigger = true;
    CHECK(Applied(history.Execute(scene, ComponentEditCommand<BoxCollider>::Create(scene, handle, collider))));
    CHECK(Applied(history.UndoAction(scene)) && !entity->HasComponent<BoxCollider>());
    CHECK(Applied(history.RedoAction(scene)) && ComponentHistory::Equal(*entity->GetComponent<BoxCollider>(), collider));
    ScriptComponent script;
    script.className = std::string(300, 'S'); script.scriptInstanceID = 456;
    CHECK(Applied(history.Execute(scene, ComponentEditCommand<ScriptComponent>::Create(scene, handle, script))));
    CHECK(entity->GetComponent<ScriptComponent>()->className == script.className && entity->GetComponent<ScriptComponent>()->scriptInstanceID == -1);
    entity->GetComponent<ScriptComponent>()->scriptInstanceID = 222;
    CHECK(Applied(history.Execute(scene, ComponentEditCommand<ScriptComponent>::Create(scene, handle, std::nullopt))));
    CHECK(Applied(history.UndoAction(scene)) && entity->GetComponent<ScriptComponent>()->className == script.className && entity->GetComponent<ScriptComponent>()->scriptInstanceID == -1);
    auto* actualScript = entity->GetComponent<ScriptComponent>();
    ScriptComponent replacement; replacement.className = "Replacement";
    CHECK(Applied(history.Execute(scene, ComponentEditCommand<ScriptComponent>::Create(scene, handle, replacement))));
    CHECK(entity->GetComponent<ScriptComponent>() == actualScript && actualScript->className == "Replacement");
    CHECK(Applied(history.UndoAction(scene)) && actualScript->className == script.className);
    Scene foreign;
    CHECK(!add->CanApply(foreign, true));
    scene.ForEachEntity([&](Entity&) { CHECK(!add->CanApply(scene, false)); });
    scene.DestroyEntity(handle);
    CHECK(!history.CanUndo(scene));
}
void LifetimeAndBounds() {
    alignas(Scene) unsigned char storage[sizeof(Scene)];
    Scene* scene = new (storage) Scene;
    auto* parent = scene->CreateEntity("parent");
    auto* child = scene->CreateEntity("child");
    auto hierarchy = HierarchyEditCommand::Create(*scene, {child->GetHandle()}, parent->GetHandle());
    RigidBody body;
    auto component = ComponentEditCommand<RigidBody>::Create(*scene, child->GetHandle(), body);
    scene->~Scene();
    scene = new (storage) Scene;
    CHECK(!hierarchy->CanApply(*scene, false) && !component->CanApply(*scene, false));
    CHECK(hierarchy->Apply(*scene, false).status == ActionResult::Status::Blocked);
    CHECK(component->Apply(*scene, false).status == ActionResult::Status::Blocked);
    auto* entity = scene->CreateEntity("new lifetime");
    ScriptComponent oversized;
    oversized.className = std::string(1024u * 1024u, 'X');
    CHECK(!ComponentEditCommand<ScriptComponent>::Create(*scene, entity->GetHandle(), oversized));
    CHECK(!HierarchyEditCommand::Create(*scene, std::vector<EntityHandle>(2049, entity->GetHandle()), {}));
    scene->~Scene();
}
void ComponentAllocationFailures() {
    int blocked = 0, succeeded = 0;
    for (int allocation = 0; allocation < 110; ++allocation) {
        Scene scene;
        auto* entity = scene.CreateEntity("entity");
        auto* transform = entity->GetComponent<Transform>();
        TransformUndoHistory history;
        history.Capture(*entity); transform->position.x = 5;
        history.UndoAction(scene);
        ScriptComponent desired; desired.className = std::string(300, 'X');
        auto command = ComponentEditCommand<ScriptComponent>::Create(scene, entity->GetHandle(), desired);
        AllocationFailure::after = allocation;
        const auto result = history.Execute(scene, command);
        AllocationFailure::after = -1;
        if (result.status == ActionResult::Status::Blocked) {
            ++blocked;
            CHECK(!entity->HasComponent<ScriptComponent>() && entity->GetComponent<Transform>() == transform);
            CHECK(history.CanRedo(scene) && Applied(history.RedoAction(scene)) && transform->position.x == 5);
        } else {
            ++succeeded;
            CHECK(Applied(result) && entity->GetComponent<ScriptComponent>()->className == desired.className && !history.CanRedo(scene));
        }
    }
    CHECK(blocked > 0 && succeeded > 0);
    std::cout << "Component command rollback checked at " << blocked << " allocation points\n";
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    HierarchyRoundTrip(); NestedUnparentAndMixedHistory(); ComponentsRoundTrip(); LifetimeAndBounds(); ComponentAllocationFailures();
    return failures ? 1 : 0;
}
