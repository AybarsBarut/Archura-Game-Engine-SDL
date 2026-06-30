#include "core/memory/Allocators.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

namespace {

struct ParticleLike {
  float position[3] = {};
  float velocity[3] = {};
  float color[4] = {};
  float lifetime = 0.0f;
};

template <typename Fn> double MeasureMs(Fn &&fn) {
  const auto begin = std::chrono::high_resolution_clock::now();
  fn();
  const auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

int main() {
  constexpr size_t kIterations = 1000000;
  std::vector<ParticleLike *> pointers(kIterations, nullptr);

  const double newDeleteMs = MeasureMs([&]() {
    for (size_t i = 0; i < kIterations; ++i)
      pointers[i] = new ParticleLike();
    for (ParticleLike *ptr : pointers)
      delete ptr;
  });

  auto pool =
      std::make_unique<Archura::Memory::PoolAllocator<ParticleLike, kIterations>>(
          "BenchmarkParticlePool");

  const double poolMs = MeasureMs([&]() {
    for (size_t i = 0; i < kIterations; ++i)
      pointers[i] = pool->Create();
    for (ParticleLike *ptr : pointers)
      pool->Destroy(ptr);
  });

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "new/delete:      " << newDeleteMs << " ms\n";
  std::cout << "PoolAllocator:   " << poolMs << " ms\n";
  std::cout << "Speedup:         " << (newDeleteMs / poolMs) << "x\n";
  return 0;
}
