#include "MemoryTracker.h"

#ifdef ARCHURA_DEBUG

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace Archura::Memory {
namespace {

std::mutex g_MemoryTrackerMutex;
std::unordered_map<const void *, MemoryStats> g_MemoryStats;

float ClampFragmentation(float value) {
  if (value < 0.0f)
    return 0.0f;
  if (value > 1.0f)
    return 1.0f;
  return value;
}

} // namespace

void MemoryTracker::RegisterAllocator(const void *owner, const char *name,
                                      size_t capacityBytes) {
  if (!owner)
    return;

  std::lock_guard<std::mutex> lock(g_MemoryTrackerMutex);
  MemoryStats &stats = g_MemoryStats[owner];
  stats.name = (name && name[0] != '\0') ? name : "UnnamedAllocator";
  stats.owner = owner;
  stats.capacityBytes = capacityBytes;
  stats.currentBytes = 0;
  stats.peakBytes = 0;
  stats.allocationCount = 0;
  stats.freeCount = 0;
  stats.liveAllocationCount = 0;
  stats.fragmentation = 0.0f;
}

void MemoryTracker::UnregisterAllocator(const void *owner) {
  if (!owner)
    return;

  std::lock_guard<std::mutex> lock(g_MemoryTrackerMutex);
  g_MemoryStats.erase(owner);
}

void MemoryTracker::RecordAllocation(const void *owner, size_t bytes) {
  if (!owner)
    return;

  std::lock_guard<std::mutex> lock(g_MemoryTrackerMutex);
  auto it = g_MemoryStats.find(owner);
  if (it == g_MemoryStats.end())
    return;

  MemoryStats &stats = it->second;
  stats.currentBytes += bytes;
  stats.peakBytes = std::max(stats.peakBytes, stats.currentBytes);
  stats.allocationCount++;
  stats.liveAllocationCount++;
}

void MemoryTracker::RecordFree(const void *owner, size_t bytes) {
  if (!owner)
    return;

  std::lock_guard<std::mutex> lock(g_MemoryTrackerMutex);
  auto it = g_MemoryStats.find(owner);
  if (it == g_MemoryStats.end())
    return;

  MemoryStats &stats = it->second;
  stats.currentBytes =
      (bytes > stats.currentBytes) ? 0 : (stats.currentBytes - bytes);
  stats.freeCount++;
  if (stats.liveAllocationCount > 0)
    stats.liveAllocationCount--;
}

void MemoryTracker::RecordReset(const void *owner) {
  if (!owner)
    return;

  std::lock_guard<std::mutex> lock(g_MemoryTrackerMutex);
  auto it = g_MemoryStats.find(owner);
  if (it == g_MemoryStats.end())
    return;

  MemoryStats &stats = it->second;
  stats.currentBytes = 0;
  stats.liveAllocationCount = 0;
  stats.fragmentation = 0.0f;
}

void MemoryTracker::RecordState(const void *owner, size_t currentBytes,
                                float fragmentation) {
  if (!owner)
    return;

  std::lock_guard<std::mutex> lock(g_MemoryTrackerMutex);
  auto it = g_MemoryStats.find(owner);
  if (it == g_MemoryStats.end())
    return;

  MemoryStats &stats = it->second;
  stats.currentBytes = currentBytes;
  stats.peakBytes = std::max(stats.peakBytes, stats.currentBytes);
  stats.fragmentation = ClampFragmentation(fragmentation);
}

std::vector<MemoryStats> MemoryTracker::Snapshot() {
  std::lock_guard<std::mutex> lock(g_MemoryTrackerMutex);
  std::vector<MemoryStats> result;
  result.reserve(g_MemoryStats.size());
  for (const auto &pair : g_MemoryStats)
    result.push_back(pair.second);

  std::sort(result.begin(), result.end(),
            [](const MemoryStats &a, const MemoryStats &b) {
              return a.name < b.name;
            });
  return result;
}

} // namespace Archura::Memory

#endif // ARCHURA_DEBUG
