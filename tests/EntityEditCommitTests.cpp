#include "ecs/Entity.h"

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

std::vector<std::string> Order(const Scene& scene) {
  std::vector<std::string> result;
  for (const auto& entity : scene.GetEntities()) result.push_back(entity->GetPersistentID());
  return result;
}
std::string State(const Scene& scene) {
  std::ostringstream state;
  for (const auto& entity : scene.GetEntities()) {
    state << entity->GetHandle().Value() << ':' << entity->GetPersistentID() << ':'
          << entity->GetName() << ':' << static_cast<int>(entity->GetOwnership()) << ':';
    if (entity->GetParent()) state << entity->GetParent()->GetPersistentID();
    state << '[';
    for (const auto* child : entity->GetChildren()) state << child->GetPersistentID() << ',';
    state << "]\n";
  }
  return state.str();
}

void TestPartialDeleteAndRestoration() {
  Scene scene;
  auto* parent = scene.CreateEntity("parent");
  auto* first = scene.CreateEntity("first");
  auto* deleted = scene.CreateEntity("deleted");
  auto* second = scene.CreateEntity("second");
  auto* child = scene.CreateEntity("retained child");
  auto* unrelated = scene.CreateEntity("unrelated");
  deleted->SetOwnership(EntityOwnership::Engine);
  CHECK(second->TrySetParent(parent));
  CHECK(deleted->TrySetParent(parent));
  CHECK(first->TrySetParent(parent));
  CHECK(child->TrySetParent(deleted));
  const auto originalOrder = Order(scene);
  const auto parentHandle = parent->GetHandle(), childHandle = child->GetHandle();
  const auto unrelatedHandle = unrelated->GetHandle(), deletedHandle = deleted->GetHandle();
  const auto identity = deleted->GetPersistentID();
  auto afterDelete = originalOrder;
  afterDelete.erase(afterDelete.begin() + 2);
  CHECK(scene.CommitEntityEdit({deletedHandle}, {}, {}, {}, afterDelete));
  CHECK(!scene.IsAlive(deletedHandle) && scene.GetEntityByPersistentID(identity) == nullptr);
  CHECK(scene.GetEntity(parentHandle) == parent && scene.GetEntity(childHandle) == child);
  CHECK(scene.GetEntity(unrelatedHandle) == unrelated && !child->GetParent());
  CHECK(parent->GetChildren().size() == 2 && parent->GetChildren()[0] == second && parent->GetChildren()[1] == first);

  auto* restored = scene.CreateEntity("deleted");
  restored->SetOwnership(EntityOwnership::Engine);
  const auto freshHandle = restored->GetHandle();
  const auto temporaryIdentity = restored->GetPersistentID();
  CHECK(scene.CommitEntityEdit({}, {freshHandle}, {identity},
      {{identity, parent->GetPersistentID(), 1}, {child->GetPersistentID(), identity, 0}}, originalOrder));
  CHECK(Order(scene) == originalOrder);
  CHECK(!scene.IsAlive(deletedHandle) && scene.GetEntity(freshHandle) == restored);
  CHECK(scene.GetEntityByPersistentID(identity) == restored && !scene.GetEntityByPersistentID(temporaryIdentity));
  CHECK(restored->IsEngineOwned() && restored->GetParent() == parent && child->GetParent() == restored);
  CHECK(parent->GetChildren().size() == 3 && parent->GetChildren()[0] == second &&
        parent->GetChildren()[1] == restored && parent->GetChildren()[2] == first);
  CHECK(restored->GetChildren().size() == 1 && restored->GetChildren()[0] == child);
  CHECK(scene.GetEntity(unrelatedHandle) == unrelated && scene.GetEntity(childHandle) == child);
}

void TestStagedLinksAndFinalIdentities() {
  Scene scene;
  auto* retained = scene.CreateEntity("retained");
  auto* parent = scene.CreateEntity("staged parent");
  auto* child = scene.CreateEntity("staged child");
  parent->SetOwnership(EntityOwnership::Engine);
  CHECK(parent->TrySetParent(retained));
  CHECK(child->TrySetParent(parent));
  const std::string parentID(32, 'a'), childID(32, 'b');
  const auto retainedHandle = retained->GetHandle();
  CHECK(scene.CommitEntityEdit({}, {parent->GetHandle(), child->GetHandle()}, {parentID, childID}, {},
        {childID, retained->GetPersistentID(), parentID}));
  CHECK(scene.GetEntityByPersistentID(parentID) == parent && scene.GetEntityByPersistentID(childID) == child);
  CHECK(scene.GetEntity(retainedHandle) == retained && parent->IsEngineOwned());
  CHECK(child->GetParent() == parent && parent->GetParent() == retained);
  CHECK(scene.GetEntities()[0].get() == child && retained->GetChildren()[0] == parent);

  // A valid final graph must not be rejected because applying it edge by edge
  // would encounter a temporary cycle against the old graph.
  CHECK(scene.CommitEntityEdit({}, {}, {}, {{retained->GetPersistentID(), childID}, {parentID, ""}}, Order(scene)));
  CHECK(!parent->GetParent() && retained->GetParent() == child && child->GetParent() == parent);
}

void TestSiblingOverrides() {
  Scene scene;
  auto* parent = scene.CreateEntity("parent");
  auto* a = scene.CreateEntity("a");
  auto* b = scene.CreateEntity("b");
  auto* x = scene.CreateEntity("x");
  auto* y = scene.CreateEntity("y");
  CHECK(a->TrySetParent(parent)); CHECK(b->TrySetParent(parent));
  CHECK(x->TrySetParent(parent)); CHECK(y->TrySetParent(parent));
  const auto id = parent->GetPersistentID();
  CHECK(scene.CommitEntityEdit({}, {}, {}, {{a->GetPersistentID(), id, 0}, {b->GetPersistentID(), id, 0}}, Order(scene)));
  CHECK(parent->GetChildren() == std::vector<Entity*>({a, b, x, y}));
  CHECK(scene.CommitEntityEdit({}, {}, {}, {{a->GetPersistentID(), id}, {b->GetPersistentID(), id}}, Order(scene)));
  CHECK(parent->GetChildren() == std::vector<Entity*>({x, y, a, b}));
}

void TestValidationAndIteration() {
  Scene scene;
  auto* parent = scene.CreateEntity("parent");
  auto* child = scene.CreateEntity("child");
  auto* staged = scene.CreateEntity("staged");
  CHECK(child->TrySetParent(parent));
  const auto before = State(scene);
  const auto order = Order(scene);
  const auto p = parent->GetPersistentID(), c = child->GetPersistentID(), s = staged->GetPersistentID();
  const auto reject = [&](const std::vector<EntityHandle>& replaced, const std::vector<EntityHandle>& incoming,
                          const std::vector<std::string>& ids, const std::vector<EntityParentLink>& links,
                          const std::vector<std::string>& desiredOrder) {
    CHECK(!scene.CommitEntityEdit(replaced, incoming, ids, links, desiredOrder));
    CHECK(State(scene) == before);
  };
  reject({parent->GetHandle(), parent->GetHandle()}, {}, {}, {}, order);
  reject({}, {staged->GetHandle()}, {}, {}, order);
  reject({staged->GetHandle()}, {staged->GetHandle()}, {s}, {}, order);
  reject({}, {staged->GetHandle()}, {p}, {}, order);
  reject({}, {staged->GetHandle()}, {"bad"}, {}, order);
  reject({}, {}, {}, {{s, std::string(32, 'c')}}, order);
  reject({}, {}, {}, {{std::string(32, 'c'), p}}, order);
  reject({}, {}, {}, {{s, p}, {s, ""}}, order);
  reject({}, {}, {}, {{p, c}}, order);
  reject({}, {}, {}, {{s, s}}, order);
  reject({}, {}, {}, {{s, p, 99}}, order);
  reject({}, {}, {}, {}, {p, c});
  reject({}, {}, {}, {}, {p, c, c});
  reject({}, {}, {}, {}, {p, c, std::string(32, 'd')});
  reject({EntityHandle{parent->GetID(), parent->GetHandle().generation + 1}}, {}, {}, {}, order);
  scene.ForEachEntity([&](Entity&) { reject({}, {}, {}, {}, order); });
}

void TestDepthLimit() {
  Scene scene;
  Entity* parent = nullptr;
  for (int index = 0; index < 128; ++index) {
    auto* entity = scene.CreateEntity("node");
    CHECK(entity->TrySetParent(parent));
    parent = entity;
  }
  CHECK(scene.CommitEntityEdit({}, {}, {}, {}, Order(scene)));
  auto* excessive = scene.CreateEntity("excessive");
  CHECK(excessive->TrySetParent(parent));
  const auto before = State(scene);
  CHECK(!scene.CommitEntityEdit({}, {}, {}, {}, Order(scene)));
  CHECK(State(scene) == before);
}

void TestAllocationFailures() {
  int injectedFailures = 0, completed = 0;
  // Continue beyond the first success: stable_sort can handle a temporary
  // allocation failure internally, and later preflight points still need testing.
  for (int allocation = 0; allocation < 180; ++allocation) {
    Scene scene;
    auto* retained = scene.CreateEntity("retained parent");
    auto* old = scene.CreateEntity("replaced");
    auto* child = scene.CreateEntity("retained child");
    auto* staged = scene.CreateEntity("replacement");
    CHECK(old->TrySetParent(retained)); CHECK(child->TrySetParent(old));
    const auto identity = old->GetPersistentID();
    const auto oldHandle = old->GetHandle(), retainedHandle = retained->GetHandle(), childHandle = child->GetHandle();
    const auto before = State(scene);
    const std::vector<EntityHandle> replaced{oldHandle}, incoming{staged->GetHandle()};
    const std::vector<std::string> ids{identity};
    const std::vector<EntityParentLink> links{{identity, retained->GetPersistentID(), 0}, {child->GetPersistentID(), identity, 0}};
    const std::vector<std::string> desiredOrder{retained->GetPersistentID(), identity, child->GetPersistentID()};
    bool success = false, threw = false;
    AllocationFailure::after = allocation;
    try { success = scene.CommitEntityEdit(replaced, incoming, ids, links, desiredOrder); }
    catch (const std::bad_alloc&) { threw = true; ++injectedFailures; }
    AllocationFailure::after = -1;
    if (threw) {
      CHECK(State(scene) == before);
      CHECK(scene.GetEntity(oldHandle) == old && scene.GetEntityByPersistentID(identity) == old);
    } else {
      CHECK(success); ++completed;
      CHECK(!scene.IsAlive(oldHandle) && scene.GetEntityByPersistentID(identity) == staged);
      CHECK(scene.GetEntity(retainedHandle) == retained && scene.GetEntity(childHandle) == child);
      CHECK(child->GetParent() == staged && staged->GetParent() == retained);
      CHECK(Order(scene) == desiredOrder);
    }
  }
  CHECK(injectedFailures > 0 && completed > 0);
  std::cout << "Entity edit rollback checked at " << injectedFailures << " allocation points\n";
}
} // namespace

int main() {
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  TestPartialDeleteAndRestoration();
  TestStagedLinksAndFinalIdentities();
  TestSiblingOverrides();
  TestValidationAndIteration();
  TestDepthLimit();
  TestAllocationFailures();
  if (failures) return 1;
  std::cout << "Entity edits: partial identity, hierarchy, order, validation and allocation rollback passed\n";
  return 0;
}
