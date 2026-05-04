#pragma once

#include "Entity.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace Archura {

struct ComponentTicket {
  EntityID entity = 0;
  uint32_t generation = 0;
};

class SparseSet {
public:
  static constexpr uint32_t InvalidIndex =
      std::numeric_limits<uint32_t>::max();

  bool Contains(EntityID entity) const {
    const uint32_t sparse = SparseIndex(entity);
    return sparse < m_Sparse.size() && m_Sparse[sparse] != InvalidIndex;
  }

  uint32_t GetDenseIndex(EntityID entity) const {
    const uint32_t sparse = SparseIndex(entity);
    if (sparse >= m_Sparse.size())
      return InvalidIndex;
    return m_Sparse[sparse];
  }

  uint32_t Add(EntityID entity) {
    const uint32_t sparse = SparseIndex(entity);
    if (sparse >= m_Sparse.size())
      m_Sparse.resize(sparse + 1, InvalidIndex);

    uint32_t &dense = m_Sparse[sparse];
    if (dense != InvalidIndex)
      return dense;

    dense = static_cast<uint32_t>(m_DenseEntities.size());
    m_DenseEntities.push_back(entity);
    return dense;
  }

  uint32_t Remove(EntityID entity) {
    const uint32_t sparse = SparseIndex(entity);
    if (sparse >= m_Sparse.size())
      return InvalidIndex;

    const uint32_t dense = m_Sparse[sparse];
    if (dense == InvalidIndex)
      return InvalidIndex;

    const uint32_t lastDense =
        static_cast<uint32_t>(m_DenseEntities.size() - 1);
    const EntityID movedEntity = m_DenseEntities[lastDense];

    m_DenseEntities[dense] = movedEntity;
    m_DenseEntities.pop_back();
    m_Sparse[sparse] = InvalidIndex;

    if (dense != lastDense)
      m_Sparse[SparseIndex(movedEntity)] = dense;

    EnsureGeneration(entity);
    m_Generations[sparse]++;
    return dense;
  }

  ComponentTicket MakeTicket(EntityID entity) {
    EnsureGeneration(entity);
    return ComponentTicket{entity, m_Generations[SparseIndex(entity)]};
  }

  bool IsTicketValid(ComponentTicket ticket) const {
    const uint32_t sparse = SparseIndex(ticket.entity);
    return sparse < m_Generations.size() &&
           m_Generations[sparse] == ticket.generation && Contains(ticket.entity);
  }

  const std::vector<EntityID> &DenseEntities() const { return m_DenseEntities; }
  size_t Size() const { return m_DenseEntities.size(); }
  void Clear() {
    for (EntityID entity : m_DenseEntities) {
      const uint32_t sparse = SparseIndex(entity);
      if (sparse < m_Sparse.size()) {
        m_Sparse[sparse] = InvalidIndex;
        EnsureGeneration(entity);
        m_Generations[sparse]++;
      }
    }
    m_DenseEntities.clear();
  }

private:
  static uint32_t SparseIndex(EntityID entity) { return entity; }

  void EnsureGeneration(EntityID entity) {
    const uint32_t sparse = SparseIndex(entity);
    if (sparse >= m_Generations.size())
      m_Generations.resize(sparse + 1, 1);
  }

  std::vector<uint32_t> m_Sparse;
  std::vector<EntityID> m_DenseEntities;
  std::vector<uint32_t> m_Generations;
};

} // namespace Archura
