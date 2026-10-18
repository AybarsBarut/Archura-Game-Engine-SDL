#include "Entity.h"

#include <algorithm>
#include <stdexcept>
#include <mutex>
#include <random>
#include <unordered_set>

namespace Archura {

namespace {
std::string NewPersistentID() {
  // Random identity is distinct from predictable runtime handles. It conveys
  // no authorization; scenes validate collisions explicitly on creation/commit.
  static std::mt19937_64 random([] {
    std::random_device entropy;
    std::seed_seq seed{entropy(), entropy(), entropy(), entropy(),
                       entropy(), entropy(), entropy(), entropy()};
    return std::mt19937_64(seed);
  }());
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  constexpr char hex[] = "0123456789abcdef";
  std::string result(32, '0');
  do {
    for (size_t part = 0; part < 2; ++part) {
      const uint64_t bits = random();
      for (size_t digit = 0; digit < 16; ++digit)
        result[part * 16 + digit] = hex[(bits >> (4 * (15 - digit))) & 15];
    }
  } while (!Entity::IsValidPersistentID(result));
  return result;
}
} // namespace

bool Entity::IsValidPersistentID(std::string_view value) noexcept {
  if (value.size() != 32) return false;
  bool nonzero = false;
  for (char digit : value) {
    if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f'))) return false;
    nonzero |= digit != '0';
  }
  return nonzero;
}

Entity::Entity(EntityHandle handle, std::string name, Scene *owner)
    : m_Handle(handle), m_PersistentID(NewPersistentID()), m_Owner(owner), m_Name(std::move(name)) {
  if (!m_Handle)
    throw std::invalid_argument("Entity requires a non-zero handle");
  AddComponent<Transform>();
}

Entity::~Entity() noexcept { DetachHierarchy(); }

bool Entity::IsAncestorOf(const Entity *candidate) const noexcept {
  for (const Entity *current = candidate; current; current = current->m_Parent) {
    if (current == this)
      return true;
  }
  return false;
}

bool Entity::TrySetParent(Entity *parent) {
  // Hierarchy pointers are only valid inside one ownership domain. Allow
  // standalone entities to parent one another, but never bridge a Scene and
  // another Scene/standalone lifetime.
  if (parent == this || (parent && parent->m_Owner != m_Owner) ||
      (parent && IsAncestorOf(parent)))
    return false;
  if (m_Parent == parent)
    return true;

  // Allocate the new backlink before changing either side. A failed vector
  // growth therefore leaves the old relationship intact.
  if (parent)
    parent->m_Children.push_back(this);
  if (m_Parent) {
    auto &siblings = m_Parent->m_Children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this),
                   siblings.end());
  }
  m_Parent = parent;
  return true;
}

void Entity::DetachHierarchy() noexcept {
  if (m_Parent) {
    auto &siblings = m_Parent->m_Children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this),
                   siblings.end());
    m_Parent = nullptr;
  }
  for (Entity *child : m_Children) {
    if (child && child->m_Parent == this)
      child->m_Parent = nullptr;
  }
  m_Children.clear();
}

glm::mat4 Entity::GetWorldTransform() const {
  const auto *transform = GetComponent<Transform>();
  const glm::mat4 model =
      transform ? transform->GetModelMatrix() : glm::mat4(1.0f);
  return m_Parent ? m_Parent->GetWorldTransform() * model : model;
}

Scene::Scene(std::string name)
    : m_Name(std::move(name)), m_OwnerThread(std::this_thread::get_id()) {}

Entity *Scene::CreateEntity(const std::string &name) {
  AssertOwnerThread();
  if (m_NextEntityID == std::numeric_limits<EntityID>::max())
    throw std::overflow_error("Scene exhausted 32-bit entity identifiers");

  const EntityHandle handle{m_NextEntityID, 1};
  auto entity = std::make_unique<Entity>(handle, name, this);
  while (m_PersistentIndex.count(entity->GetPersistentID()))
    entity->m_PersistentID = NewPersistentID();
  Entity *result = entity.get();

  // Reserve both insertions before publishing either. A failed allocation does
  // not leak the entity or expose a partially indexed object.
  m_Entities.reserve(m_Entities.size() + 1);
  m_EntityIndex.reserve(m_EntityIndex.size() + 1);
  m_PersistentIndex.reserve(m_PersistentIndex.size() + 1);
  m_Entities.push_back(std::move(entity));
  try {
    m_EntityIndex.emplace(handle.id, result);
    m_PersistentIndex.emplace(result->GetPersistentID(), result);
  } catch (...) {
    m_EntityIndex.erase(handle.id);
    m_PersistentIndex.erase(result->GetPersistentID());
    m_Entities.pop_back();
    throw;
  }
  ++m_NextEntityID;
  return result;
}

Entity* Scene::GetEntityByPersistentID(const std::string& id) noexcept {
  return const_cast<Entity*>(static_cast<const Scene&>(*this).GetEntityByPersistentID(id));
}

const Entity* Scene::GetEntityByPersistentID(const std::string& id) const noexcept {
  AssertOwnerThread();
  const auto found = m_PersistentIndex.find(id);
  return found == m_PersistentIndex.end() || IsPendingDestroy(found->second->GetHandle())
      ? nullptr : found->second;
}

bool Scene::CommitEntityReplacement(const std::vector<EntityHandle>& replaced,
                                    const std::vector<EntityHandle>& staged,
                                    const std::vector<std::string>& persistentIDs) {
  AssertOwnerThread();
  if (IsIterating() || persistentIDs.size() != staged.size()) return false;
  // MSVC Debug vector move construction allocates an iterator proxy inside a
  // noexcept constructor. Take a reference and copy in this throwing preflight
  // scope so allocation failure is recoverable rather than std::terminate.
  std::vector<std::string> preparedIDs(persistentIDs);
  std::unordered_set<EntityID> oldIDs;
  for (auto handle : replaced)
    if (!GetEntity(handle) || !oldIDs.insert(handle.id).second) return false;
  std::unordered_set<EntityID> newIDs;
  for (auto handle : staged)
    if (!GetEntity(handle) || oldIDs.count(handle.id) || !newIDs.insert(handle.id).second) return false;

  // Include every retained entity. Imported identities may equal old replaced
  // identities, but must never alias a retained service or another staged item.
  std::unordered_map<std::string, Entity*> finalIndex;
  finalIndex.reserve(m_Entities.size() - replaced.size());
  for (const auto& entity : m_Entities)
    if (!oldIDs.count(entity->GetID()) && !newIDs.count(entity->GetID()))
      if (!finalIndex.emplace(entity->GetPersistentID(), entity.get()).second) return false;
  for (size_t i = 0; i < staged.size(); ++i) {
    if (preparedIDs[i].empty()) preparedIDs[i] = GetEntity(staged[i])->GetPersistentID();
    if (!Entity::IsValidPersistentID(preparedIDs[i]) ||
        !finalIndex.emplace(preparedIDs[i], GetEntity(staged[i])).second) return false;
  }

  // All allocating/validating work is finished. Handles of removed entities
  // remain invalid; persistent identity never changes their runtime generation.
  for (auto handle : replaced) DestroyEntityImmediate(handle);
  for (size_t i = 0; i < staged.size(); ++i)
    GetEntity(staged[i])->m_PersistentID.swap(preparedIDs[i]);
  static_assert(noexcept(m_PersistentIndex.swap(finalIndex)), "Scene identity commit must not throw");
  m_PersistentIndex.swap(finalIndex);
  return true;
}

bool Scene::DestroyEntity(EntityHandle handle) {
  AssertOwnerThread();
  Entity *entity = GetEntity(handle);
  if (!entity)
    return false;
  if (m_IterationDepth != 0) {
    m_PendingDestroy.push_back(handle);
    return true;
  }
  return DestroyEntityImmediate(handle);
}

bool Scene::DestroyEntity(EntityID id) {
  AssertOwnerThread();
  Entity *entity = GetEntity(id);
  return entity ? DestroyEntity(entity->GetHandle()) : false;
}

bool Scene::DestroyEntityImmediate(EntityHandle handle) noexcept {
  const auto indexed = m_EntityIndex.find(handle.id);
  if (indexed == m_EntityIndex.end() ||
      indexed->second->GetHandle() != handle)
    return false;

  Entity *target = indexed->second;
  const auto it = std::find_if(
      m_Entities.begin(), m_Entities.end(),
      [target](const std::unique_ptr<Entity> &entity) {
        return entity.get() == target;
      });
  assert(it != m_Entities.end());
  if (it == m_Entities.end())
    return false;
  m_PersistentIndex.erase(target->GetPersistentID());
  m_EntityIndex.erase(indexed);
  m_Entities.erase(it);
  return true;
}

bool Scene::IsPendingDestroy(EntityHandle handle) const noexcept {
  return std::find(m_PendingDestroy.begin(), m_PendingDestroy.end(), handle) !=
         m_PendingDestroy.end();
}

void Scene::FlushPendingDestroy() noexcept {
  while (!m_PendingDestroy.empty()) {
    const EntityHandle handle = m_PendingDestroy.back();
    m_PendingDestroy.pop_back();
    (void)DestroyEntityImmediate(handle);
  }
}

Entity *Scene::GetEntity(EntityHandle handle) noexcept {
  AssertOwnerThread();
  if (!handle || IsPendingDestroy(handle))
    return nullptr;
  const auto it = m_EntityIndex.find(handle.id);
  if (it == m_EntityIndex.end() || it->second->GetHandle() != handle)
    return nullptr;
  return it->second;
}

const Entity *Scene::GetEntity(EntityHandle handle) const noexcept {
  AssertOwnerThread();
  if (!handle || IsPendingDestroy(handle))
    return nullptr;
  const auto it = m_EntityIndex.find(handle.id);
  if (it == m_EntityIndex.end() || it->second->GetHandle() != handle)
    return nullptr;
  return it->second;
}

Entity *Scene::GetEntity(EntityID id) noexcept {
  AssertOwnerThread();
  const auto it = m_EntityIndex.find(id);
  return it == m_EntityIndex.end() ||
                 IsPendingDestroy(it->second->GetHandle())
             ? nullptr
             : it->second;
}

const Entity *Scene::GetEntity(EntityID id) const noexcept {
  AssertOwnerThread();
  const auto it = m_EntityIndex.find(id);
  return it == m_EntityIndex.end() ||
                 IsPendingDestroy(it->second->GetHandle())
             ? nullptr
             : it->second;
}

} // namespace Archura
