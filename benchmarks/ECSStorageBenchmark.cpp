#include "ecs/ComponentStorage.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) ||             \
    defined(__i386__)
#include <immintrin.h>
#define ARCHURA_BENCH_HAS_SSE 1
#else
#define ARCHURA_BENCH_HAS_SSE 0
#endif

namespace {

struct AoSTransform {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 rotation = glm::vec3(0.0f);
  glm::vec3 scale = glm::vec3(1.0f);
  glm::vec3 velocity = glm::vec3(1.0f, 0.5f, -0.25f);
};

template <typename Fn> double MeasureMs(Fn &&fn) {
  const auto begin = std::chrono::high_resolution_clock::now();
  fn();
  const auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

} // namespace

int main() {
  constexpr size_t kEntityCount = 250000;
  constexpr int kIterations = 128;
  constexpr float kDt = 1.0f / 128.0f;

  std::vector<AoSTransform> aos(kEntityCount);
  for (size_t i = 0; i < aos.size(); ++i) {
    aos[i].position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
    aos[i].velocity = glm::vec3(1.0f, 0.5f, -0.25f);
  }

  Archura::ComponentStorage<Archura::Transform> soa;
  soa.Reserve(kEntityCount);
  std::vector<float> velocityX(kEntityCount, 1.0f);
  std::vector<float> velocityY(kEntityCount, 0.5f);
  std::vector<float> velocityZ(kEntityCount, -0.25f);

  for (size_t i = 0; i < kEntityCount; ++i) {
    Archura::Transform transform;
    transform.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
    soa.Add(static_cast<Archura::EntityID>(i + 1), transform);
  }

  const double aosMs = MeasureMs([&]() {
    for (int iteration = 0; iteration < kIterations; ++iteration) {
      for (AoSTransform &transform : aos)
        transform.position += transform.velocity * kDt;
    }
  });

  const double soaMs = MeasureMs([&]() {
    float *positionX = soa.PositionX();
    float *positionY = soa.PositionY();
    float *positionZ = soa.PositionZ();

#if ARCHURA_BENCH_HAS_SSE
    const __m128 dt = _mm_set1_ps(kDt);
#endif
    for (int iteration = 0; iteration < kIterations; ++iteration) {
      size_t i = 0;
#if ARCHURA_BENCH_HAS_SSE
      for (; i + 4 <= kEntityCount; i += 4) {
        __m128 px = _mm_loadu_ps(positionX + i);
        __m128 py = _mm_loadu_ps(positionY + i);
        __m128 pz = _mm_loadu_ps(positionZ + i);
        __m128 vx = _mm_loadu_ps(velocityX.data() + i);
        __m128 vy = _mm_loadu_ps(velocityY.data() + i);
        __m128 vz = _mm_loadu_ps(velocityZ.data() + i);

        _mm_storeu_ps(positionX + i, _mm_add_ps(px, _mm_mul_ps(vx, dt)));
        _mm_storeu_ps(positionY + i, _mm_add_ps(py, _mm_mul_ps(vy, dt)));
        _mm_storeu_ps(positionZ + i, _mm_add_ps(pz, _mm_mul_ps(vz, dt)));
      }
#endif
      for (; i < kEntityCount; ++i) {
        positionX[i] += velocityX[i] * kDt;
        positionY[i] += velocityY[i] * kDt;
        positionZ[i] += velocityZ[i] * kDt;
      }
    }
  });

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "AoS transform update: " << aosMs << " ms\n";
  std::cout << "SoA batch update:     " << soaMs << " ms\n";
  std::cout << "Speedup:              " << (aosMs / soaMs) << "x\n";
  return 0;
}
