#pragma once

#include "Component.h"
#include "SparseSet.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Archura {

template <typename T> class ComponentStorage;

struct TransformView {
  EntityID entity = 0;
  glm::vec3 &position;
  glm::vec3 &rotation;
  glm::vec3 &scale;
};

struct RigidBodyView {
  EntityID entity = 0;
  glm::vec3 &velocity;
  glm::vec3 &force;
  float &mass;
  float &drag;
  bool &useGravity;
  bool &isKinematic;
};

struct TransformBatch4 {
  EntityID entities[4] = {};
  float *positionX = nullptr;
  float *positionY = nullptr;
  float *positionZ = nullptr;
  float *rotationX = nullptr;
  float *rotationY = nullptr;
  float *rotationZ = nullptr;
  float *scaleX = nullptr;
  float *scaleY = nullptr;
  float *scaleZ = nullptr;
  size_t count = 0;
};

template <> class ComponentStorage<Transform> {
public:
  ComponentTicket Add(EntityID entity, const Transform &value = {}) {
    EnsureStructuralMutationAllowed();
    if (m_Index.Contains(entity)) {
      Set(entity, value);
      return m_Index.MakeTicket(entity);
    }

    const size_t oldSize = Size();
    try {
      m_PositionX.push_back(value.position.x);
      m_PositionY.push_back(value.position.y);
      m_PositionZ.push_back(value.position.z);
      m_RotationX.push_back(value.rotation.x);
      m_RotationY.push_back(value.rotation.y);
      m_RotationZ.push_back(value.rotation.z);
      m_ScaleX.push_back(value.scale.x);
      m_ScaleY.push_back(value.scale.y);
      m_ScaleZ.push_back(value.scale.z);
      m_Index.Add(entity);
    } catch (...) {
      ResizeArrays(oldSize);
      throw;
    }
    return m_Index.MakeTicket(entity);
  }

  void Remove(EntityID entity) {
    EnsureStructuralMutationAllowed();
    const uint32_t dense = m_Index.GetDenseIndex(entity);
    if (dense == SparseSet::InvalidIndex)
      return;

    const size_t last = Size() - 1;
    SwapAndPop(m_PositionX, dense, last);
    SwapAndPop(m_PositionY, dense, last);
    SwapAndPop(m_PositionZ, dense, last);
    SwapAndPop(m_RotationX, dense, last);
    SwapAndPop(m_RotationY, dense, last);
    SwapAndPop(m_RotationZ, dense, last);
    SwapAndPop(m_ScaleX, dense, last);
    SwapAndPop(m_ScaleY, dense, last);
    SwapAndPop(m_ScaleZ, dense, last);
    m_Index.Remove(entity);
  }

  bool Contains(EntityID entity) const { return m_Index.Contains(entity); }
  bool IsTicketValid(ComponentTicket ticket) const {
    return m_Index.IsTicketValid(ticket);
  }
  size_t Size() const { return m_Index.Size(); }
  bool Empty() const { return Size() == 0; }

  Transform Get(EntityID entity) const {
    const uint32_t index = m_Index.GetDenseIndex(entity);
    if (index == SparseSet::InvalidIndex)
      throw std::out_of_range("Transform component does not exist");
    Transform value;
    value.position = {m_PositionX[index], m_PositionY[index],
                      m_PositionZ[index]};
    value.rotation = {m_RotationX[index], m_RotationY[index],
                      m_RotationZ[index]};
    value.scale = {m_ScaleX[index], m_ScaleY[index], m_ScaleZ[index]};
    return value;
  }

  void Set(EntityID entity, const Transform &value) {
    EnsureStructuralMutationAllowed();
    const uint32_t index = m_Index.GetDenseIndex(entity);
    if (index == SparseSet::InvalidIndex)
      throw std::out_of_range("Transform component does not exist");
    m_PositionX[index] = value.position.x;
    m_PositionY[index] = value.position.y;
    m_PositionZ[index] = value.position.z;
    m_RotationX[index] = value.rotation.x;
    m_RotationY[index] = value.rotation.y;
    m_RotationZ[index] = value.rotation.z;
    m_ScaleX[index] = value.scale.x;
    m_ScaleY[index] = value.scale.y;
    m_ScaleZ[index] = value.scale.z;
  }

  template <typename Fn> void ForEach(Fn &&fn) {
    IterationGuard guard(*this);
    const auto &entities = m_Index.DenseEntities();
    for (size_t i = 0; i < entities.size(); ++i) {
      glm::vec3 position{m_PositionX[i], m_PositionY[i], m_PositionZ[i]};
      glm::vec3 rotation{m_RotationX[i], m_RotationY[i], m_RotationZ[i]};
      glm::vec3 scale{m_ScaleX[i], m_ScaleY[i], m_ScaleZ[i]};
      fn(entities[i], position, rotation, scale);
      m_PositionX[i] = position.x;
      m_PositionY[i] = position.y;
      m_PositionZ[i] = position.z;
      m_RotationX[i] = rotation.x;
      m_RotationY[i] = rotation.y;
      m_RotationZ[i] = rotation.z;
      m_ScaleX[i] = scale.x;
      m_ScaleY[i] = scale.y;
      m_ScaleZ[i] = scale.z;
    }
  }

  template <typename Fn> void ForEachBatch4(Fn &&fn) {
    IterationGuard guard(*this);
    const auto &entities = m_Index.DenseEntities();
    for (size_t i = 0; i < entities.size(); i += 4) {
      TransformBatch4 batch;
      batch.count = std::min<size_t>(4, entities.size() - i);
      for (size_t j = 0; j < batch.count; ++j)
        batch.entities[j] = entities[i + j];

      batch.positionX = m_PositionX.data() + i;
      batch.positionY = m_PositionY.data() + i;
      batch.positionZ = m_PositionZ.data() + i;
      batch.rotationX = m_RotationX.data() + i;
      batch.rotationY = m_RotationY.data() + i;
      batch.rotationZ = m_RotationZ.data() + i;
      batch.scaleX = m_ScaleX.data() + i;
      batch.scaleY = m_ScaleY.data() + i;
      batch.scaleZ = m_ScaleZ.data() + i;
      fn(batch);
    }
  }

  float *PositionX() { return m_PositionX.data(); }
  float *PositionY() { return m_PositionY.data(); }
  float *PositionZ() { return m_PositionZ.data(); }
  const std::vector<EntityID> &Entities() const { return m_Index.DenseEntities(); }

  void Reserve(size_t count) {
    m_PositionX.reserve(count);
    m_PositionY.reserve(count);
    m_PositionZ.reserve(count);
    m_RotationX.reserve(count);
    m_RotationY.reserve(count);
    m_RotationZ.reserve(count);
    m_ScaleX.reserve(count);
    m_ScaleY.reserve(count);
    m_ScaleZ.reserve(count);
  }

private:
  class IterationGuard {
  public:
    explicit IterationGuard(ComponentStorage &storage) noexcept
        : m_Storage(storage) {
      ++m_Storage.m_IterationDepth;
    }
    ~IterationGuard() { --m_Storage.m_IterationDepth; }
    IterationGuard(const IterationGuard &) = delete;
    IterationGuard &operator=(const IterationGuard &) = delete;

  private:
    ComponentStorage &m_Storage;
  };

  void EnsureStructuralMutationAllowed() const {
    if (m_IterationDepth != 0)
      throw std::logic_error(
          "Transform storage cannot be structurally mutated during iteration");
  }

  void ResizeArrays(size_t size) noexcept {
    m_PositionX.resize(size);
    m_PositionY.resize(size);
    m_PositionZ.resize(size);
    m_RotationX.resize(size);
    m_RotationY.resize(size);
    m_RotationZ.resize(size);
    m_ScaleX.resize(size);
    m_ScaleY.resize(size);
    m_ScaleZ.resize(size);
  }

  template <typename Array>
  static void SwapAndPop(Array &array, size_t index, size_t last) {
    if (index != last)
      array[index] = array[last];
    array.pop_back();
  }

  SparseSet m_Index;
  size_t m_IterationDepth = 0;
  std::vector<float> m_PositionX;
  std::vector<float> m_PositionY;
  std::vector<float> m_PositionZ;
  std::vector<float> m_RotationX;
  std::vector<float> m_RotationY;
  std::vector<float> m_RotationZ;
  std::vector<float> m_ScaleX;
  std::vector<float> m_ScaleY;
  std::vector<float> m_ScaleZ;
};

template <> class ComponentStorage<RigidBody> {
public:
  ComponentTicket Add(EntityID entity, const RigidBody &value = {}) {
    EnsureStructuralMutationAllowed();
    if (m_Index.Contains(entity)) {
      Set(entity, value);
      return m_Index.MakeTicket(entity);
    }

    const size_t oldSize = Size();
    try {
      m_VelocityX.push_back(value.velocity.x);
      m_VelocityY.push_back(value.velocity.y);
      m_VelocityZ.push_back(value.velocity.z);
      m_ForceX.push_back(value.force.x);
      m_ForceY.push_back(value.force.y);
      m_ForceZ.push_back(value.force.z);
      m_Mass.push_back(value.mass);
      m_Drag.push_back(value.drag);
      m_Restitution.push_back(value.restitution);
      m_Friction.push_back(value.friction);
      m_UseGravity.push_back(value.useGravity ? 1 : 0);
      m_IsKinematic.push_back(value.isKinematic ? 1 : 0);
      m_Continuous.push_back(value.continuous ? 1 : 0);
      m_Index.Add(entity);
    } catch (...) {
      ResizeArrays(oldSize);
      throw;
    }
    return m_Index.MakeTicket(entity);
  }

  void Remove(EntityID entity) {
    EnsureStructuralMutationAllowed();
    const uint32_t dense = m_Index.GetDenseIndex(entity);
    if (dense == SparseSet::InvalidIndex)
      return;

    const size_t last = Size() - 1;
    SwapAndPop(m_VelocityX, dense, last);
    SwapAndPop(m_VelocityY, dense, last);
    SwapAndPop(m_VelocityZ, dense, last);
    SwapAndPop(m_ForceX, dense, last);
    SwapAndPop(m_ForceY, dense, last);
    SwapAndPop(m_ForceZ, dense, last);
    SwapAndPop(m_Mass, dense, last);
    SwapAndPop(m_Drag, dense, last);
    SwapAndPop(m_Restitution, dense, last);
    SwapAndPop(m_Friction, dense, last);
    SwapAndPop(m_UseGravity, dense, last);
    SwapAndPop(m_IsKinematic, dense, last);
    SwapAndPop(m_Continuous, dense, last);
    m_Index.Remove(entity);
  }

  bool Contains(EntityID entity) const { return m_Index.Contains(entity); }
  bool IsTicketValid(ComponentTicket ticket) const {
    return m_Index.IsTicketValid(ticket);
  }
  size_t Size() const { return m_Index.Size(); }
  bool Empty() const { return Size() == 0; }

  RigidBody Get(EntityID entity) const {
    const uint32_t index = m_Index.GetDenseIndex(entity);
    if (index == SparseSet::InvalidIndex)
      throw std::out_of_range("RigidBody component does not exist");
    RigidBody value;
    value.velocity = {m_VelocityX[index], m_VelocityY[index],
                      m_VelocityZ[index]};
    value.force = {m_ForceX[index], m_ForceY[index], m_ForceZ[index]};
    value.mass = m_Mass[index];
    value.drag = m_Drag[index];
    value.restitution = m_Restitution[index];
    value.friction = m_Friction[index];
    value.useGravity = m_UseGravity[index] != 0;
    value.isKinematic = m_IsKinematic[index] != 0;
    value.continuous = m_Continuous[index] != 0;
    return value;
  }

  void Set(EntityID entity, const RigidBody &value) {
    EnsureStructuralMutationAllowed();
    const uint32_t index = m_Index.GetDenseIndex(entity);
    if (index == SparseSet::InvalidIndex)
      throw std::out_of_range("RigidBody component does not exist");
    m_VelocityX[index] = value.velocity.x;
    m_VelocityY[index] = value.velocity.y;
    m_VelocityZ[index] = value.velocity.z;
    m_ForceX[index] = value.force.x;
    m_ForceY[index] = value.force.y;
    m_ForceZ[index] = value.force.z;
    m_Mass[index] = value.mass;
    m_Drag[index] = value.drag;
    m_Restitution[index] = value.restitution;
    m_Friction[index] = value.friction;
    m_UseGravity[index] = value.useGravity ? 1 : 0;
    m_IsKinematic[index] = value.isKinematic ? 1 : 0;
    m_Continuous[index] = value.continuous ? 1 : 0;
  }

  float *VelocityX() { return m_VelocityX.data(); }
  float *VelocityY() { return m_VelocityY.data(); }
  float *VelocityZ() { return m_VelocityZ.data(); }
  float *Drag() { return m_Drag.data(); }
  float *Restitution() { return m_Restitution.data(); }
  float *Friction() { return m_Friction.data(); }
  uint8_t *UseGravity() { return m_UseGravity.data(); }
  uint8_t *IsKinematic() { return m_IsKinematic.data(); }
  uint8_t *Continuous() { return m_Continuous.data(); }
  const std::vector<EntityID> &Entities() const { return m_Index.DenseEntities(); }

  void Reserve(size_t count) {
    m_VelocityX.reserve(count);
    m_VelocityY.reserve(count);
    m_VelocityZ.reserve(count);
    m_ForceX.reserve(count);
    m_ForceY.reserve(count);
    m_ForceZ.reserve(count);
    m_Mass.reserve(count);
    m_Drag.reserve(count);
    m_Restitution.reserve(count);
    m_Friction.reserve(count);
    m_UseGravity.reserve(count);
    m_IsKinematic.reserve(count);
    m_Continuous.reserve(count);
  }

private:
  void EnsureStructuralMutationAllowed() const {
    if (m_IterationDepth != 0)
      throw std::logic_error(
          "RigidBody storage cannot be structurally mutated during iteration");
  }

  void ResizeArrays(size_t size) noexcept {
    m_VelocityX.resize(size);
    m_VelocityY.resize(size);
    m_VelocityZ.resize(size);
    m_ForceX.resize(size);
    m_ForceY.resize(size);
    m_ForceZ.resize(size);
    m_Mass.resize(size);
    m_Drag.resize(size);
    m_Restitution.resize(size);
    m_Friction.resize(size);
    m_UseGravity.resize(size);
    m_IsKinematic.resize(size);
    m_Continuous.resize(size);
  }

  template <typename Array>
  static void SwapAndPop(Array &array, size_t index, size_t last) {
    if (index != last)
      array[index] = array[last];
    array.pop_back();
  }

  SparseSet m_Index;
  size_t m_IterationDepth = 0;
  std::vector<float> m_VelocityX;
  std::vector<float> m_VelocityY;
  std::vector<float> m_VelocityZ;
  std::vector<float> m_ForceX;
  std::vector<float> m_ForceY;
  std::vector<float> m_ForceZ;
  std::vector<float> m_Mass;
  std::vector<float> m_Drag;
  std::vector<float> m_Restitution;
  std::vector<float> m_Friction;
  std::vector<uint8_t> m_UseGravity;
  std::vector<uint8_t> m_IsKinematic;
  std::vector<uint8_t> m_Continuous;
};

using TransformComponent = Transform;
using PhysicsBodyComponent = RigidBody;

} // namespace Archura
