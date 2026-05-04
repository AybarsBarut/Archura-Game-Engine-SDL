#include "TransformSoASystem.h"

#include "Entity.h"

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) ||             \
    defined(__i386__)
#include <immintrin.h>
#define ARCHURA_ECS_HAS_SSE 1
#else
#define ARCHURA_ECS_HAS_SSE 0
#endif

namespace Archura {

void TransformSoASystem::Init(Scene *scene) {
  System::Init(scene);
  RebuildFromScene();
}

void TransformSoASystem::Update(float deltaTime) {
  IntegrateRigidBodiesSIMD(deltaTime);
  SyncBackToScene();
}

void TransformSoASystem::Shutdown() {
  m_Transforms = ComponentStorage<Transform>();
  m_Bodies = ComponentStorage<RigidBody>();
  m_Scene = nullptr;
}

void TransformSoASystem::RebuildFromScene() {
  m_Transforms = ComponentStorage<Transform>();
  m_Bodies = ComponentStorage<RigidBody>();

  if (!m_Scene)
    return;

  const auto &entities = m_Scene->GetEntities();
  m_Transforms.Reserve(entities.size());
  m_Bodies.Reserve(entities.size());

  for (const auto &entityPtr : entities) {
    Entity *entity = entityPtr.get();
    if (auto *transform = entity->GetComponent<Transform>())
      m_Transforms.Add(entity->GetID(), *transform);
    if (auto *body = entity->GetComponent<RigidBody>())
      m_Bodies.Add(entity->GetID(), *body);
  }
}

void TransformSoASystem::SyncBackToScene() {
  if (!m_Scene)
    return;

  for (EntityID entityId : m_Transforms.Entities()) {
    Entity *entity = m_Scene->GetEntity(entityId);
    if (!entity)
      continue;
    if (auto *transform = entity->GetComponent<Transform>())
      *transform = m_Transforms.Get(entityId);
  }

  for (EntityID entityId : m_Bodies.Entities()) {
    Entity *entity = m_Scene->GetEntity(entityId);
    if (!entity)
      continue;
    if (auto *body = entity->GetComponent<RigidBody>())
      *body = m_Bodies.Get(entityId);
  }
}

void TransformSoASystem::IntegrateRigidBodiesSIMD(float deltaTime) {
  const auto &entities = m_Bodies.Entities();
  float *vx = m_Bodies.VelocityX();
  float *vy = m_Bodies.VelocityY();
  float *vz = m_Bodies.VelocityZ();
  float *drag = m_Bodies.Drag();
  uint8_t *gravity = m_Bodies.UseGravity();
  uint8_t *kinematic = m_Bodies.IsKinematic();

  constexpr float gravityY = -9.81f;

  size_t i = 0;
#if ARCHURA_ECS_HAS_SSE
  const __m128 dt = _mm_set1_ps(deltaTime);
  const __m128 one = _mm_set1_ps(1.0f);
  const __m128 gy = _mm_set1_ps(gravityY * deltaTime);

  for (; i + 4 <= entities.size(); i += 4) {
    alignas(16) float kMask[4] = {};
    alignas(16) float gMask[4] = {};
    for (int lane = 0; lane < 4; ++lane) {
      kMask[lane] = kinematic[i + lane] ? 0.0f : 1.0f;
      gMask[lane] = gravity[i + lane] ? 1.0f : 0.0f;
    }

    __m128 active = _mm_load_ps(kMask);
    __m128 useGravity = _mm_load_ps(gMask);
    __m128 vx4 = _mm_loadu_ps(vx + i);
    __m128 vy4 = _mm_loadu_ps(vy + i);
    __m128 vz4 = _mm_loadu_ps(vz + i);
    __m128 drag4 = _mm_loadu_ps(drag + i);

    vy4 = _mm_add_ps(vy4, _mm_mul_ps(gy, useGravity));

    const __m128 damping = _mm_sub_ps(one, _mm_mul_ps(drag4, dt));
    vx4 = _mm_mul_ps(vx4, damping);
    vy4 = _mm_mul_ps(vy4, damping);
    vz4 = _mm_mul_ps(vz4, damping);

    _mm_storeu_ps(vx + i, _mm_mul_ps(vx4, active));
    _mm_storeu_ps(vy + i, _mm_mul_ps(vy4, active));
    _mm_storeu_ps(vz + i, _mm_mul_ps(vz4, active));
  }
#endif

  for (; i < entities.size(); ++i) {
    if (kinematic[i])
      continue;
    if (gravity[i])
      vy[i] += gravityY * deltaTime;
    const float damping = 1.0f - drag[i] * deltaTime;
    vx[i] *= damping;
    vy[i] *= damping;
    vz[i] *= damping;
  }

  for (size_t index = 0; index < entities.size(); ++index) {
    const EntityID entity = entities[index];
    if (!m_Transforms.Contains(entity))
      continue;

    Transform transform = m_Transforms.Get(entity);
    transform.position.x += vx[index] * deltaTime;
    transform.position.y += vy[index] * deltaTime;
    transform.position.z += vz[index] * deltaTime;
    m_Transforms.Set(entity, transform);
  }
}

} // namespace Archura
