#include "editor/HierarchyEditCommand.h"
#include "editor/ComponentEditCommand.h"
#include "editor/EntityNameEditCommand.h"
#include "editor/MeshColorEditCommand.h"
#include "rendering/SkyboxReload.h"

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
void GestureHistory() {
    Scene scene;
    auto* entity = scene.CreateEntity("initial name");
    auto* body = entity->AddComponent<RigidBody>();
    TransformUndoHistory history;
    history.Capture(*entity); entity->GetComponent<Transform>()->position.x = 10;
    CHECK(Applied(history.UndoAction(scene)) && history.CanRedo(scene));
    const auto delta = [&](float mass, uint64_t key) {
        RigidBody desired = *body; desired.mass = mass;
        return history.ExecuteGesture(scene, ComponentEditCommand<RigidBody>::Create(scene, entity->GetHandle(), desired), key);
    };
    CHECK(Applied(delta(2, 1)) && Applied(delta(3, 1)) && Applied(delta(4, 1)));
    CHECK(body->mass == 4 && history.CanUndo(scene) && !history.CanRedo(scene));
    CHECK(history.EndGesture(scene, 1));
    CHECK(Applied(history.UndoAction(scene)) && body->mass == 1);
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Empty); // One gesture, not three frames.
    CHECK(Applied(history.RedoAction(scene)) && body->mass == 4);
    CHECK(Applied(history.UndoAction(scene)) && history.CanRedo(scene));
    CHECK(Applied(delta(8, 2)) && Applied(delta(1, 2))); // Returned to initial value.
    CHECK(!history.CanUndo(scene) && history.CanRedo(scene) && history.EndGesture(scene, 2));
    CHECK(Applied(history.RedoAction(scene)) && body->mass == 4);
    // A new widget's key closes the previous interaction; one Undo per widget.
    CHECK(Applied(delta(5, 3)) && Applied(delta(6, 4)));
    CHECK(Applied(history.UndoAction(scene)) && body->mass == 5);
    CHECK(Applied(history.UndoAction(scene)) && body->mass == 4);
    CHECK(history.RetainedBytes() <= TransformUndoHistory::MaxHistoryBytes);
    CHECK(Applied(delta(7, 5)));
    auto rename = EntityNameEditCommand::Create(scene, entity->GetHandle(), "changed name");
    CHECK(Applied(history.Execute(scene, rename)) && entity->GetName() == "changed name");
    CHECK(Applied(history.UndoAction(scene)) && entity->GetName() == "initial name" && body->mass == 7);
    CHECK(Applied(history.UndoAction(scene)) && body->mass == 4);
    // Refuse cross-component, cross-entity and noncontinuous merge attempts.
    CHECK(Applied(delta(9, 6)));
    auto* other = scene.CreateEntity("other"); other->AddComponent<RigidBody>();
    RigidBody otherDesired; otherDesired.mass = 2;
    CHECK(history.ExecuteGesture(scene, ComponentEditCommand<RigidBody>::Create(scene, other->GetHandle(), otherDesired), 6).status == ActionResult::Status::Blocked);
    CHECK(history.ExecuteGesture(scene, ComponentEditCommand<BoxCollider>::Create(scene, entity->GetHandle(), BoxCollider{}), 6).status == ActionResult::Status::Blocked && !entity->HasComponent<BoxCollider>());
    body->mass = 99;
    CHECK(history.ExecuteGesture(scene, ComponentEditCommand<RigidBody>::Create(scene, entity->GetHandle(), RigidBody{}), 6).status == ActionResult::Status::Blocked);
    CHECK(body->mass == 99 && !history.CanUndo(scene));
    body->mass = 9;
    CHECK(Applied(history.UndoAction(scene)) && body->mass == 4);
    // Light color/intensity values use the same CPU-only gesture path.
    auto* light = entity->AddComponent<LightComponent>();
    LightComponent desired = *light; desired.intensity = 5; desired.color = {0.2f,0.3f,0.4f};
    CHECK(Applied(history.ExecuteGesture(scene, ComponentEditCommand<LightComponent>::Create(scene, entity->GetHandle(), desired), 7)));
    CHECK(Applied(history.UndoAction(scene)) && light->intensity == 1 && light->color == glm::vec3(1));
    Scene foreign;
    CHECK(!rename->CanApply(foreign, false));
}
void GestureAllocationFailures() {
    int blocked = 0, success = 0;
    for (int allocation = 0; allocation < 110; ++allocation) {
        Scene scene;
        auto* entity = scene.CreateEntity("entity");
        entity->AddComponent<ScriptComponent>()->className = std::string(300, 'A');
        TransformUndoHistory history;
        ScriptComponent desired; desired.className = std::string(300, 'B');
        CHECK(Applied(history.ExecuteGesture(scene, ComponentEditCommand<ScriptComponent>::Create(scene, entity->GetHandle(), desired), 1)));
        desired.className = std::string(300, 'C');
        auto delta = ComponentEditCommand<ScriptComponent>::Create(scene, entity->GetHandle(), desired);
        AllocationFailure::after = allocation;
        const auto result = history.ExecuteGesture(scene, delta, 1);
        AllocationFailure::after = -1;
        if (result.status == ActionResult::Status::Blocked) {
            ++blocked;
            CHECK(entity->GetComponent<ScriptComponent>()->className == std::string(300, 'B'));
        } else { ++success; CHECK(Applied(result)); }
        CHECK(Applied(history.UndoAction(scene)) && entity->GetComponent<ScriptComponent>()->className == std::string(300, 'A'));
    }
    CHECK(blocked > 0 && success > 0);
}
void MeshColorHistory() {
    Scene scene;
    auto* entity = scene.CreateEntity("colored");
    auto* renderer = entity->AddComponent<MeshRenderer>();
    auto* transform = entity->GetComponent<Transform>();
    TransformUndoHistory history;
    const auto delta = [&](const glm::vec3& color, uint64_t key) {
        return history.ExecuteGesture(scene, MeshColorEditCommand::Create(scene, entity->GetHandle(), color), key);
    };
    CHECK(Applied(delta({0.8f,0.7f,0.6f}, 1)) && Applied(delta({0.2f,0.3f,0.4f}, 1)));
    CHECK(Applied(history.UndoAction(scene)) && renderer->color == glm::vec3(1));
    CHECK(history.UndoAction(scene).status == ActionResult::Status::Empty && history.CanRedo(scene));
    CHECK(Applied(delta({0.1f,0.2f,0.3f}, 2)) && Applied(delta({1,1,1}, 2)) && history.EndGesture(scene, 2) && history.CanRedo(scene));
    CHECK(Applied(history.RedoAction(scene)) && renderer->color == glm::vec3(0.2f,0.3f,0.4f));
    CHECK(entity->GetComponent<MeshRenderer>() == renderer && entity->GetComponent<Transform>() == transform);
    renderer->color = {8,8,8};
    CHECK(!history.CanUndo(scene));
    renderer->color = {0.2f,0.3f,0.4f};
    CHECK(Applied(history.UndoAction(scene)));
    auto command = MeshColorEditCommand::Create(scene, entity->GetHandle(), {0,0,0});
    Scene foreign;
    CHECK(!command->CanApply(foreign, false));
    entity->RemoveComponent<MeshRenderer>();
    CHECK(!command->CanApply(scene, false));
}
void WeaponAndSkyboxHistory() {
    Scene scene;
    auto* entity = scene.CreateEntity("authoring");
    auto* weapon = entity->AddComponent<Weapon>(); weapon->InitInventory();
    weapon->stats.pattern.offsets = {{1,2},{3,4}};
    weapon->inventory[Weapon::WeaponType::Pistol].pattern.offsets = {{5,6}};
    weapon->isReloading = true; weapon->reloadTimer = 0.8f; weapon->shotsFired = 7;
    const Weapon original = *weapon;
    TransformUndoHistory history;
    for (float damage : {45.0f, 60.0f, 75.0f}) {
        Weapon desired = *weapon; desired.stats.damage = damage;
        CHECK(Applied(history.ExecuteGesture(scene, ComponentEditCommand<Weapon>::Create(scene, entity->GetHandle(), desired), 20)));
    }
    CHECK(weapon->isReloading && weapon->reloadTimer == 0.8f && weapon->shotsFired == 7);
    weapon->timeSinceLastShot = 12; weapon->currentRecoil = 9; // Runtime changes are not a history conflict.
    CHECK(Applied(history.UndoAction(scene)) && ComponentHistory::Equal(*weapon, original));
    CHECK(weapon->timeSinceLastShot == 12 && weapon->currentRecoil == 9 && weapon->isReloading);
    CHECK(Applied(history.RedoAction(scene)) && weapon->stats.damage == 75);
    const Weapon edited = *weapon;
    Weapon switched = *weapon; switched.SwitchWeapon(Weapon::WeaponType::Pistol);
    CHECK(Applied(history.Execute(scene, ComponentEditCommand<Weapon>::Create(scene, entity->GetHandle(), switched))));
    CHECK(weapon->type == Weapon::WeaponType::Pistol && weapon->inventory[Weapon::WeaponType::Rifle].damage == 75);
    CHECK(weapon->isReloading && weapon->reloadTimer == 0.8f);
    CHECK(Applied(history.UndoAction(scene)) && ComponentHistory::Equal(*weapon, edited));
    CHECK(weapon->stats.pattern.offsets == original.stats.pattern.offsets);
    Weapon invalid = *weapon; invalid.stats.fireRate = 0;
    CHECK(!ComponentEditCommand<Weapon>::Create(scene, entity->GetHandle(), invalid));
    invalid = *weapon; invalid.stats.pattern.offsets.resize(4097);
    CHECK(!ComponentEditCommand<Weapon>::Create(scene, entity->GetHandle(), invalid));
    Weapon noChange = *weapon; noChange.reloadTimer = 999;
    CHECK(!ComponentEditCommand<Weapon>::Create(scene, entity->GetHandle(), noChange) && history.CanRedo(scene));
    auto* sky = entity->AddComponent<SkyboxComponent>();
    sky->facePaths[0] = std::string(400, 'F');
    const auto originalFaces = sky->facePaths;
    SkyboxComponent desired = *sky; desired.facePaths[0] = "assets/textures/right.bmp";
    CHECK(Applied(history.Execute(scene, ComponentEditCommand<SkyboxComponent>::Create(scene, entity->GetHandle(), desired))) && sky->shouldReload);
    int attempts = 0;
    auto fail = [&](const auto&) { ++attempts; return false; };
    CHECK(!ConsumeSkyboxReloadRequest(*sky, fail) && attempts == 1 && !sky->shouldReload);
    CHECK(!ConsumeSkyboxReloadRequest(*sky, fail) && attempts == 1);
    CHECK(Applied(history.UndoAction(scene)) && sky->facePaths == originalFaces && sky->shouldReload);
    CHECK(ConsumeSkyboxReloadRequest(*sky, [](const auto&) { return true; }) && !sky->shouldReload);
    CHECK(Applied(history.RedoAction(scene)) && sky->facePaths[0] == desired.facePaths[0] && sky->shouldReload);
    desired = *sky; desired.shouldReload = false;
    CHECK(!ComponentEditCommand<SkyboxComponent>::Create(scene, entity->GetHandle(), desired));
    desired.facePaths.pop_back();
    CHECK(!ComponentEditCommand<SkyboxComponent>::Create(scene, entity->GetHandle(), desired));
}
void WeaponAllocationFailures() {
    int blocked = 0, applied = 0;
    for (int allocation = 0; allocation < 180; ++allocation) {
        Scene scene;
        auto* entity = scene.CreateEntity("weapon");
        auto* weapon = entity->AddComponent<Weapon>(); weapon->InitInventory();
        weapon->stats.pattern.offsets = std::vector<glm::vec2>(20, {1,2});
        const Weapon original = *weapon;
        Weapon desired = original; desired.SwitchWeapon(Weapon::WeaponType::Pistol);
        TransformUndoHistory history;
        auto command = ComponentEditCommand<Weapon>::Create(scene, entity->GetHandle(), desired);
        AllocationFailure::after = allocation;
        const auto result = history.Execute(scene, command);
        AllocationFailure::after = -1;
        if (result.status == ActionResult::Status::Blocked) { ++blocked; CHECK(ComponentHistory::Equal(*weapon, original) && !history.CanUndo(scene)); }
        else { ++applied; CHECK(Applied(result) && ComponentHistory::Equal(*weapon, desired)); }
    }
    CHECK(blocked > 0 && applied > 0);
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
    HierarchyRoundTrip(); NestedUnparentAndMixedHistory(); ComponentsRoundTrip(); GestureHistory(); GestureAllocationFailures(); MeshColorHistory(); WeaponAndSkyboxHistory(); WeaponAllocationFailures(); LifetimeAndBounds(); ComponentAllocationFailures();
    return failures ? 1 : 0;
}
