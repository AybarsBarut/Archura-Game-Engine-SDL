#include "core/memory/Allocators.h"

#include <unordered_map>
#include <vector>

namespace Archura::Examples {
namespace {

struct RenderCommand {
  float matrix[16] = {};
  unsigned int meshId = 0;
  unsigned int materialId = 0;
};

struct Particle {
  float position[3] = {};
  float velocity[3] = {};
  float lifetime = 0.0f;
};

} // namespace

void FrameAllocatorExample() {
  Memory::LinearAllocator frameAllocator(2 * 1024 * 1024,
                                         "RenderFrameAllocator");

  auto *commands = frameAllocator.AllocateArray<RenderCommand>(2048);
  if (commands) {
    commands[0].meshId = 1;
    commands[0].materialId = 7;
  }

  frameAllocator.Reset();
}

void ParticlePoolExample() {
  Memory::PoolAllocator<Particle, 4096> particlePool("ParticlePool");

  Particle *particle = particlePool.Create();
  if (particle) {
    particle->position[1] = 2.0f;
    particle->velocity[1] = 8.0f;
    particle->lifetime = 1.5f;
  }

  particlePool.Destroy(particle);
}

void StackScratchExample() {
  Memory::StackAllocator scratch(256 * 1024, "JobScratchStack");

  {
    Memory::StackAllocator::ScopedMarker marker(scratch);
    auto *tmpPositions = static_cast<float *>(
        scratch.Allocate(sizeof(float) * 3 * 1024, alignof(float)));
    if (tmpPositions)
      tmpPositions[0] = 1.0f;
  }
}

void STLPoolAllocatorExample() {
  using IntVectorAllocator = Memory::PoolStdAllocator<int, 4096, 64>;
  IntVectorAllocator vectorAllocator;
  std::vector<int, IntVectorAllocator> values(vectorAllocator);
  values.reserve(512);
  values.push_back(42);

  using MapValue = std::pair<const int, int>;
  using MapAllocator = Memory::PoolStdAllocator<MapValue, 256, 1024>;
  MapAllocator mapAllocator;
  std::unordered_map<int, int, std::hash<int>, std::equal_to<int>,
                     MapAllocator>
      table(0, std::hash<int>{}, std::equal_to<int>{}, mapAllocator);
  table.emplace(1, 100);
}

} // namespace Archura::Examples
