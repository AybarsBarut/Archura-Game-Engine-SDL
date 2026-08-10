#include "Entity.h"

#include <algorithm>
#include <stdexcept>

namespace Archura {

Entity::Entity(EntityHandle handle, std::string name, Scene *owner)
    : m_Handle(handle), m_Owner(owner), m_Name(std::move(name)) {
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
  Entity *result = entity.get();

  // Reserve both insertions before publishing either. A failed allocation does
  // not leak the entity or expose a partially indexed object.
  m_Entities.reserve(m_Entities.size() + 1);
  m_EntityIndex.reserve(m_EntityIndex.size() + 1);
  m_Entities.push_back(std::move(entity));
  try {
    m_EntityIndex.emplace(handle.id, result);
  } catch (...) {
    m_Entities.pop_back();
    throw;
  }
  ++m_NextEntityID;
  return result;
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
