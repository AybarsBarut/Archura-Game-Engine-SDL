#include "ecs/ComponentStorage.h"
#include "ecs/Entity.h"
#include "ecs/ScriptComponent.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <cstdlib>
#include <new>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace AllocationFailure {
thread_local int after = -1;
}
// Test-only allocator fault injection; production code has no test switches.
void* operator new(std::size_t size) {
  bool injectable = true;
#if defined(_MSC_VER) && _ITERATOR_DEBUG_LEVEL != 0
  // MSVC's Debug STL allocates these inside noexcept _Hash_vec/vector
  // constructors. Test payload allocation rollback, not vendor proxy OOM aborts.
  // This size exclusion is test-only; Release injects every allocation size.
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

int g_Failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "          \
                << #condition << '\n';                                         \
      ++g_Failures;                                                            \
    }                                                                          \
  } while (false)

struct TrackedComponent final : Archura::Component {
  explicit TrackedComponent(int *destroyed) : destroyed(destroyed) {}
  ~TrackedComponent() override { ++*destroyed; }
  int *destroyed;
};

void TestHandlesAndLifetime() {
  static_assert(!std::is_copy_constructible<Archura::Entity>::value,
                "Entities must have a single owner");
  static_assert(!std::is_move_constructible<Archura::Entity>::value,
                "Entity addresses must remain stable");

  Archura::Scene scene("handles");
  Archura::Entity *entity = scene.CreateEntity("first");
  const Archura::EntityHandle handle = entity->GetHandle();
  CHECK(Archura::EntityHandle::FromValue(handle.Value()) == handle);
  CHECK(scene.GetEntity(handle) == entity);
  CHECK(scene.DestroyEntity(handle));
  CHECK(!scene.IsAlive(handle));
  CHECK(scene.GetEntity(handle) == nullptr);
  CHECK(!scene.DestroyEntity(handle));

  Archura::Entity *next = scene.CreateEntity("next");
  CHECK(next->GetID() != handle.id);
  CHECK(scene.GetEntity(handle) == nullptr);
}

void TestPersistentIdentityCommit() {
  Archura::Scene scene;
  auto* old = scene.CreateEntity("old");
  const auto oldHandle = old->GetHandle();
  const auto persistent = old->GetPersistentID();
  CHECK(Archura::Entity::IsValidPersistentID(persistent));
  CHECK(!Archura::Entity::IsValidPersistentID(std::string(32, '0')));
  CHECK(!Archura::Entity::IsValidPersistentID(std::string(32, 'A')));
  CHECK(!Archura::Entity::IsValidPersistentID("short"));
  auto* staged = scene.CreateEntity("staged");
  const auto stagedHandle = staged->GetHandle();
  const auto generated = staged->GetPersistentID();
  CHECK(generated != persistent);
  CHECK(!scene.CommitEntityReplacement({oldHandle}, {stagedHandle}, {"bad"}));
  CHECK(scene.GetEntity(oldHandle) == old && staged->GetPersistentID() == generated);
  CHECK(!scene.CommitEntityReplacement({oldHandle, oldHandle}, {stagedHandle}, {persistent}));
  CHECK(!scene.CommitEntityReplacement({oldHandle}, {oldHandle}, {persistent}));
  CHECK(scene.CommitEntityReplacement({oldHandle}, {stagedHandle}, {persistent}));
  CHECK(!scene.IsAlive(oldHandle));
  CHECK(scene.GetEntity(stagedHandle) == staged);
  CHECK(scene.GetEntityByPersistentID(persistent) == staged);
  CHECK(scene.GetEntityByPersistentID(generated) == nullptr);

  auto* next = scene.CreateEntity("next");
  const auto nextHandle = next->GetHandle();
  const auto nextIdentity = next->GetPersistentID();
  CHECK(nextIdentity != persistent);
  CHECK(!scene.CommitEntityReplacement({}, {nextHandle}, {persistent}));
  CHECK(next->GetPersistentID() == nextIdentity && scene.IsAlive(stagedHandle));
  scene.ForEachEntity([&](Archura::Entity&) {
    CHECK(!scene.CommitEntityReplacement({stagedHandle}, {nextHandle}, {persistent}));
  });
  CHECK(scene.GetEntities().size() == 2);
  scene.ForEachEntity([&](Archura::Entity& entity) {
    if (entity.GetHandle() == nextHandle) {
      scene.DestroyEntity(nextHandle);
      CHECK(!scene.GetEntityByPersistentID(nextIdentity));
    }
  });
  CHECK(!scene.GetEntityByPersistentID(nextIdentity));
}

void TestIdentityCommitAllocationFailures() {
  bool reachedCommit = false;
  int injectedFailures = 0;
  for (int allocation = 0; allocation < 100 && !reachedCommit; ++allocation) {
    Archura::Scene scene;
    auto* oldParent = scene.CreateEntity("old parent");
    auto* oldChild = scene.CreateEntity("old child");
    oldChild->TrySetParent(oldParent);
    auto* retained = scene.CreateEntity("retained");
    auto* newParent = scene.CreateEntity("new parent");
    auto* newChild = scene.CreateEntity("new child");
    newChild->TrySetParent(newParent);
    const auto parentID = oldParent->GetPersistentID(), childID = oldChild->GetPersistentID();
    const auto generated = newParent->GetPersistentID();
    const auto oldHandle = oldParent->GetHandle(), retainedHandle = retained->GetHandle();
    std::vector<Archura::EntityHandle> old{oldParent->GetHandle(), oldChild->GetHandle()};
    std::vector<Archura::EntityHandle> staged{newParent->GetHandle(), newChild->GetHandle()};
    std::vector<std::string> identities{parentID, childID};
    bool failed = false;
    AllocationFailure::after = allocation;
    try { reachedCommit = scene.CommitEntityReplacement(old, staged, identities); }
    catch (const std::bad_alloc&) { failed = true; ++injectedFailures; }
    AllocationFailure::after = -1;
    if (failed) {
      CHECK(scene.GetEntities().size() == 5 && scene.IsAlive(oldHandle));
      CHECK(oldChild->GetParent() == oldParent && oldParent->GetChildren().size() == 1);
      CHECK(oldParent->GetPersistentID() == parentID && oldChild->GetPersistentID() == childID);
      CHECK(newParent->GetPersistentID() == generated);
      CHECK(scene.GetEntityByPersistentID(parentID) == oldParent);
      CHECK(scene.GetEntityByPersistentID(generated) == newParent);
    } else {
      CHECK(reachedCommit && scene.GetEntities().size() == 3);
      CHECK(!scene.IsAlive(oldHandle) && scene.IsAlive(retainedHandle));
      CHECK(scene.GetEntityByPersistentID(parentID) == newParent);
      CHECK(scene.GetEntityByPersistentID(childID) == newChild);
      CHECK(newChild->GetParent() == newParent);
    }
  }
  CHECK(reachedCommit && injectedFailures > 0);
  std::cout << "Identity commit rollback verified at " << injectedFailures << " allocation failure points\n";
}

void TestComponentOwnership() {
  int destroyed = 0;
  {
    Archura::Entity entity(7, "components");
    TrackedComponent *first = entity.AddComponent<TrackedComponent>(&destroyed);
    TrackedComponent *duplicate =
        entity.AddComponent<TrackedComponent>(&destroyed);
    CHECK(first == duplicate);
    CHECK(destroyed == 0);
    CHECK(entity.RemoveComponent<TrackedComponent>());
    CHECK(destroyed == 1);
    CHECK(!entity.RemoveComponent<TrackedComponent>());
    entity.AddComponent<TrackedComponent>(&destroyed);
  }
  CHECK(destroyed == 2);
}

void TestHierarchyIntegrity() {
  Archura::Scene scene("hierarchy");
  Archura::Entity *root = scene.CreateEntity("root");
  Archura::Entity *child = scene.CreateEntity("child");
  Archura::Entity *grandchild = scene.CreateEntity("grandchild");

  CHECK(child->TrySetParent(root));
  CHECK(grandchild->TrySetParent(child));
  CHECK(!root->TrySetParent(grandchild));
  CHECK(!root->TrySetParent(root));
  CHECK(root->GetChildren().size() == 1);

  const Archura::EntityHandle childHandle = child->GetHandle();
  CHECK(scene.DestroyEntity(childHandle));
  CHECK(root->GetChildren().empty());
  CHECK(grandchild->GetParent() == nullptr);

  Archura::Scene otherScene("other");
  Archura::Entity *outsider = otherScene.CreateEntity("outsider");
  CHECK(!root->TrySetParent(outsider));
  CHECK(root->GetParent() == nullptr);
}

void TestMutationSafeSceneIteration() {
  Archura::Scene scene("iteration");
  Archura::Entity *first = scene.CreateEntity("first");
  scene.CreateEntity("second");
  const Archura::EntityHandle firstHandle = first->GetHandle();

  std::vector<Archura::EntityID> visited;
  scene.ForEachEntity([&](Archura::Entity &entity) {
    visited.push_back(entity.GetID());
    if (entity.GetHandle() == firstHandle) {
      scene.DestroyEntity(firstHandle);
      // The handle is dead immediately, but callback storage stays alive until
      // the outermost traversal exits so this reference is not dangling.
      CHECK(!scene.IsAlive(firstHandle));
      CHECK(entity.GetHandle() == firstHandle);
      scene.CreateEntity("created-during-pass");
    }
  });

  CHECK(visited.size() == 2);
  CHECK(!scene.IsAlive(firstHandle));
  CHECK(scene.GetEntities().size() == 2);
}

void TestSparseSetTickets() {
  Archura::SparseSet set;
  const Archura::ComponentTicket first = set.MakeTicket(11);
  CHECK(!set.IsTicketValid(first));
  set.Add(11);
  const Archura::ComponentTicket live = set.MakeTicket(11);
  CHECK(set.IsTicketValid(live));
  CHECK(set.Remove(11) != Archura::SparseSet::InvalidIndex);
  CHECK(!set.IsTicketValid(live));
  set.Add(11);
  CHECK(!set.IsTicketValid(live));
  CHECK(set.IsTicketValid(set.MakeTicket(11)));
}

void TestTransformStorageContracts() {
  Archura::ComponentStorage<Archura::Transform> storage;
  Archura::Transform a;
  a.position.x = 1.0f;
  Archura::Transform b;
  b.position.x = 2.0f;
  Archura::Transform c;
  c.position.x = 3.0f;

  const Archura::ComponentTicket bTicket = storage.Add(20, b);
  storage.Add(10, a);
  storage.Add(30, c);
  CHECK(storage.Size() == 3);
  storage.Remove(20);
  CHECK(!storage.IsTicketValid(bTicket));
  CHECK(storage.Contains(10));
  CHECK(storage.Contains(30));
  CHECK(std::fabs(storage.Get(30).position.x - 3.0f) < 0.0001f);

  bool missingThrew = false;
  try {
    (void)storage.Get(999);
  } catch (const std::out_of_range &) {
    missingThrew = true;
  }
  CHECK(missingThrew);

  bool mutationThrew = false;
  try {
    storage.ForEach([&](Archura::EntityID, glm::vec3 &position,
                        glm::vec3 &, glm::vec3 &) {
      position.y += 5.0f;
      storage.Remove(10);
    });
  } catch (const std::logic_error &) {
    mutationThrew = true;
  }
  CHECK(mutationThrew);
  storage.Remove(10); // guard was released during stack unwinding
  CHECK(!storage.Contains(10));
}

void TestRigidBodyStorageRoundTrip() {
  Archura::ComponentStorage<Archura::RigidBody> storage;
  Archura::RigidBody body;
  body.mass = 4.0f;
  body.restitution = 0.75f;
  body.friction = 0.25f;
  body.isKinematic = true;
  body.continuous = true;
  storage.Add(42, body);
  const Archura::RigidBody copy = storage.Get(42);
  CHECK(copy.mass == body.mass);
  CHECK(copy.restitution == body.restitution);
  CHECK(copy.friction == body.friction);
  CHECK(copy.isKinematic == body.isKinematic);
  CHECK(copy.continuous == body.continuous);
}

} // namespace

int main() {
#ifdef _MSC_VER
  // Automated failures must produce stderr/status, never block on a CRT dialog.
  _set_error_mode(_OUT_TO_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
  TestHandlesAndLifetime();
  TestPersistentIdentityCommit();
  TestIdentityCommitAllocationFailures();
  TestComponentOwnership();
  TestHierarchyIntegrity();
  TestMutationSafeSceneIteration();
  TestSparseSetTickets();
  TestTransformStorageContracts();
  TestRigidBodyStorageRoundTrip();

  if (g_Failures != 0) {
    std::cerr << g_Failures << " ECS core test(s) failed\n";
    return 1;
  }
  std::cout << "All ECS core tests passed\n";
  return 0;
}
