#pragma once

#include "Component.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Archura {

using EntityID = uint32_t;
using EntityGeneration = uint32_t;

class Scene;

// A serializable, generation-checked entity reference. EntityID remains available
// for legacy native call sites, but persistent/external references must use this
// handle so that a destroyed entity can never alias a later entity.
struct EntityHandle final {
  EntityID id = 0;
  EntityGeneration generation = 0;

  constexpr explicit operator bool() const noexcept {
    return id != 0 && generation != 0;
  }
  constexpr uint64_t Value() const noexcept {
    return (static_cast<uint64_t>(generation) << 32U) | id;
  }
  static constexpr EntityHandle FromValue(uint64_t value) noexcept {
    return EntityHandle{static_cast<EntityID>(value),
                        static_cast<EntityGeneration>(value >> 32U)};
  }

  friend constexpr bool operator==(EntityHandle lhs,
                                   EntityHandle rhs) noexcept {
    return lhs.id == rhs.id && lhs.generation == rhs.generation;
  }
  friend constexpr bool operator!=(EntityHandle lhs,
                                   EntityHandle rhs) noexcept {
    return !(lhs == rhs);
  }
};

class Entity final {
public:
  Entity(EntityHandle handle, std::string name = "Entity",
         Scene *owner = nullptr);
  Entity(EntityID id, const std::string &name = "Entity")
      : Entity(EntityHandle{id, 1}, name) {}
  ~Entity() noexcept;

  Entity(const Entity &) = delete;
  Entity &operator=(const Entity &) = delete;
  Entity(Entity &&) = delete;
  Entity &operator=(Entity &&) = delete;

  EntityID GetID() const noexcept { return m_Handle.id; }
  EntityHandle GetHandle() const noexcept { return m_Handle; }
  const std::string &GetName() const noexcept { return m_Name; }
  void SetName(std::string name) { m_Name = std::move(name); }

  // Hierarchy is non-owning. Cycles, self-parenting and duplicate children are
  // rejected. The Scene owns all Entity lifetimes.
  bool TrySetParent(Entity *parent);
  void SetParent(Entity *parent) { (void)TrySetParent(parent); }
  Entity *GetParent() const noexcept { return m_Parent; }
  const std::vector<Entity *> &GetChildren() const noexcept { return m_Children; }

  glm::mat4 GetWorldTransform() const;

  template <typename T, typename... Args> T *AddComponent(Args &&...args) {
    static_assert(std::is_base_of<Component, T>::value,
                  "ECS components must derive from Archura::Component");
    const std::type_index type = typeid(T);
    const auto existing = m_Components.find(type);
    if (existing != m_Components.end())
      return static_cast<T *>(existing->second.get());

    // Construct before mutating the map. If construction or insertion throws,
    // the entity remains unchanged (strong exception guarantee).
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T *result = component.get();
    m_Components.emplace(type, std::move(component));
    return result;
  }

  template <typename T> T *GetComponent() noexcept {
    static_assert(std::is_base_of<Component, T>::value,
                  "ECS components must derive from Archura::Component");
    const auto it = m_Components.find(std::type_index(typeid(T)));
    return it == m_Components.end() ? nullptr
                                    : static_cast<T *>(it->second.get());
  }

  template <typename T> const T *GetComponent() const noexcept {
    static_assert(std::is_base_of<Component, T>::value,
                  "ECS components must derive from Archura::Component");
    const auto it = m_Components.find(std::type_index(typeid(T)));
    return it == m_Components.end()
               ? nullptr
               : static_cast<const T *>(it->second.get());
  }

  template <typename T> bool HasComponent() const noexcept {
    return GetComponent<T>() != nullptr;
  }

  template <typename T> bool RemoveComponent() noexcept {
    static_assert(std::is_base_of<Component, T>::value,
                  "ECS components must derive from Archura::Component");
    return m_Components.erase(std::type_index(typeid(T))) != 0;
  }

private:
  bool IsAncestorOf(const Entity *candidate) const noexcept;
  void DetachHierarchy() noexcept;

  EntityHandle m_Handle;
  Scene *m_Owner = nullptr;
  std::string m_Name;
  Entity *m_Parent = nullptr;
  std::vector<Entity *> m_Children;
  std::unordered_map<std::type_index, std::unique_ptr<Component>> m_Components;
};

// Scene structural mutation and component access are deliberately main-thread
// affine. Worker systems may operate on their own snapshots, then publish on the
// owner thread. This matches the existing renderer/Mono call graph and avoids
// pretending that returning raw component pointers can be made internally safe.
class Scene final {
public:
  explicit Scene(std::string name = "Default Scene");
  ~Scene() = default;

  Scene(const Scene &) = delete;
  Scene &operator=(const Scene &) = delete;
  Scene(Scene &&) = delete;
  Scene &operator=(Scene &&) = delete;

  Entity *CreateEntity(const std::string &name = "Entity");
  bool DestroyEntity(EntityHandle handle);
  bool DestroyEntity(EntityID id);
  Entity *GetEntity(EntityHandle handle) noexcept;
  const Entity *GetEntity(EntityHandle handle) const noexcept;
  Entity *GetEntity(EntityID id) noexcept;
  const Entity *GetEntity(EntityID id) const noexcept;
  bool IsAlive(EntityHandle handle) const noexcept {
    return GetEntity(handle) != nullptr;
  }

  // Snapshot iteration is stable under CreateEntity/DestroyEntity from the
  // callback. New entities are visited next pass and destroyed handles skipped.
  template <typename Fn> void ForEachEntity(Fn &&fn) {
    AssertOwnerThread();
    std::vector<EntityHandle> snapshot;
    snapshot.reserve(m_Entities.size());
    for (const auto &entity : m_Entities)
      snapshot.push_back(entity->GetHandle());
    ++m_IterationDepth;
    try {
      for (const EntityHandle handle : snapshot) {
        if (Entity *entity = GetEntity(handle))
          fn(*entity);
      }
    } catch (...) {
      --m_IterationDepth;
      if (m_IterationDepth == 0)
        FlushPendingDestroy();
      throw;
    }
    --m_IterationDepth;
    if (m_IterationDepth == 0)
      FlushPendingDestroy();
  }

  // Legacy traversal is read-only and invalidated by structural mutation. New
  // systems should prefer ForEachEntity.
  const std::vector<std::unique_ptr<Entity>> &GetEntities() const noexcept {
    AssertOwnerThread();
    return m_Entities;
  }

  bool IsOwnerThread() const noexcept {
    return std::this_thread::get_id() == m_OwnerThread;
  }

private:
  void AssertOwnerThread() const noexcept {
    assert(IsOwnerThread());
    if (!IsOwnerThread())
      std::terminate();
  }
  bool IsPendingDestroy(EntityHandle handle) const noexcept;
  bool DestroyEntityImmediate(EntityHandle handle) noexcept;
  void FlushPendingDestroy() noexcept;

  std::string m_Name;
  EntityID m_NextEntityID = 1;
  const std::thread::id m_OwnerThread;
  std::vector<std::unique_ptr<Entity>> m_Entities;
  std::unordered_map<EntityID, Entity *> m_EntityIndex;
  size_t m_IterationDepth = 0;
  std::vector<EntityHandle> m_PendingDestroy;
};

} // namespace Archura
