#include "ecs/ComponentStorage.h"
#include "ecs/Entity.h"
#include "ecs/ScriptComponent.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

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
  TestHandlesAndLifetime();
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
