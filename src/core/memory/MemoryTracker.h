#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Archura::Memory {

struct MemoryStats {
  std::string name;
  const void *owner = nullptr;
  size_t capacityBytes = 0;
  size_t currentBytes = 0;
  size_t peakBytes = 0;
  size_t allocationCount = 0;
  size_t freeCount = 0;
  size_t liveAllocationCount = 0;
  float fragmentation = 0.0f;
};

class MemoryTracker {
public:
#ifdef ARCHURA_DEBUG
  static void RegisterAllocator(const void *owner, const char *name,
                                size_t capacityBytes);
  static void UnregisterAllocator(const void *owner);
  static void RecordAllocation(const void *owner, size_t bytes);
  static void RecordFree(const void *owner, size_t bytes);
  static void RecordReset(const void *owner);
  static void RecordState(const void *owner, size_t currentBytes,
                          float fragmentation);
  static std::vector<MemoryStats> Snapshot();
#else
  static void RegisterAllocator(const void *, const char *, size_t) {}
  static void UnregisterAllocator(const void *) {}
  static void RecordAllocation(const void *, size_t) {}
  static void RecordFree(const void *, size_t) {}
  static void RecordReset(const void *) {}
  static void RecordState(const void *, size_t, float) {}
  static std::vector<MemoryStats> Snapshot() { return {}; }
#endif
};

} // namespace Archura::Memory
