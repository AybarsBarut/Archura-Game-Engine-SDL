#include "PhysicsSystem.h"
#include "../ecs/Entity.h"
#include "../core/Logger.h"
#include <algorithm>
#include <limits>
#include <cmath>

namespace Archura {

    // ---------------------------------------------------------------------------
    void PhysicsSystem::Init(Scene* scene) {
        m_Scene = scene;
    }

    void PhysicsSystem::Update(float deltaTime) {
        if (!m_Scene) return;
        if (!m_Simulating) return;

        Integrate(deltaTime);
        ResolveCollisions();
    }

    void PhysicsSystem::Shutdown() { }

    // ---------------------------------------------------------------------------
    void PhysicsSystem::Integrate(float deltaTime) {
        for (auto& entityPtr : m_Scene->GetEntities()) {
            Entity* entity = entityPtr.get();
            auto* rb        = entity->GetComponent<RigidBody>();
            auto* transform = entity->GetComponent<Transform>();

            if (rb && transform && !rb->isKinematic) {
                if (rb->useGravity) {
                    rb->velocity += m_Gravity * deltaTime;
                }
                rb->velocity *= (1.0f - rb->drag * deltaTime);
                transform->position += rb->velocity * deltaTime;
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Yardimci: iki AABB overlap mi?
    bool PhysicsSystem::CheckAABB(const glm::vec3& posA, const glm::vec3& sizeA,
                                  const glm::vec3& posB, const glm::vec3& sizeB) {
        glm::vec3 halfA = sizeA * 0.5f;
        glm::vec3 halfB = sizeB * 0.5f;
        return std::abs(posA.x - posB.x) <= (halfA.x + halfB.x) &&
               std::abs(posA.y - posB.y) <= (halfA.y + halfB.y) &&
               std::abs(posA.z - posB.z) <= (halfA.z + halfB.z);
    }

    // ---------------------------------------------------------------------------
    // Ray-AABB kesisim: slab yontemi
    // invDir = vec3(1/dir.x, 1/dir.y, 1/dir.z) onceden hesaplanmis olmali
    bool PhysicsSystem::RayAABB(const glm::vec3& origin, const glm::vec3& invDir,
                                 const glm::vec3& boxMin, const glm::vec3& boxMax,
                                 float& tMin, float& tMax) {
        glm::vec3 t0 = (boxMin - origin) * invDir;
        glm::vec3 t1 = (boxMax - origin) * invDir;

        glm::vec3 tSmall = glm::min(t0, t1);
        glm::vec3 tLarge = glm::max(t0, t1);

        tMin = glm::max(glm::max(tSmall.x, tSmall.y), tSmall.z);
        tMax = glm::min(glm::min(tLarge.x, tLarge.y), tLarge.z);

        return tMax >= glm::max(tMin, 0.0f);
    }

    // ---------------------------------------------------------------------------
    void PhysicsSystem::ResolveCollisions() {
        m_CurrCollisions.clear();

        auto& entities = m_Scene->GetEntities();
        for (size_t i = 0; i < entities.size(); ++i) {
            Entity* entityA = entities[i].get();
            auto* rbA    = entityA->GetComponent<RigidBody>();
            auto* colA   = entityA->GetComponent<BoxCollider>();
            auto* transA = entityA->GetComponent<Transform>();

            if (!rbA || !colA || !transA || rbA->isKinematic) continue;

            for (size_t j = 0; j < entities.size(); ++j) {
                if (i == j) continue;

                Entity* entityB = entities[j].get();
                auto* colB   = entityB->GetComponent<BoxCollider>();
                auto* transB = entityB->GetComponent<Transform>();

                if (!colB || !transB) continue;
                if (colA->isTrigger || colB->isTrigger) continue;

                if (CheckAABB(transA->position, colA->size * transA->scale,
                              transB->position, colB->size * transB->scale)) {

                    CollisionPair pair{ entityA->GetID(), entityB->GetID() };
                    m_CurrCollisions.push_back(pair);

                    // Temel Y ekseni ayirma
                    float yOverlap = (colA->size.y * transA->scale.y * 0.5f +
                                      colB->size.y * transB->scale.y * 0.5f) -
                                     std::abs(transA->position.y - transB->position.y);
                    if (yOverlap > 0 && rbA->velocity.y < 0 &&
                        transA->position.y > transB->position.y) {
                        transA->position.y += yOverlap;
                        rbA->velocity.y = 0.0f;
                    }
                }
            }
        }

        // ── Callback: OnCollisionEnter ────────────────────────────────────────
        if (m_OnEnter) {
            for (const auto& curr : m_CurrCollisions) {
                bool wasPresent = false;
                for (const auto& prev : m_PrevCollisions) {
                    if (curr == prev) { wasPresent = true; break; }
                }
                if (!wasPresent) {
                    // Yeni carpisme
                    Entity* eA = m_Scene->GetEntity(curr.a);
                    Entity* eB = m_Scene->GetEntity(curr.b);
                    if (eA && eB) {
                        CollisionEvent ev;
                        ev.entityA = eA;
                        ev.entityB = eB;
                        m_OnEnter(ev);
                    }
                }
            }
        }

        // ── Callback: OnCollisionExit ─────────────────────────────────────────
        if (m_OnExit) {
            for (const auto& prev : m_PrevCollisions) {
                bool stillPresent = false;
                for (const auto& curr : m_CurrCollisions) {
                    if (curr == prev) { stillPresent = true; break; }
                }
                if (!stillPresent) {
                    Entity* eA = m_Scene->GetEntity(prev.a);
                    Entity* eB = m_Scene->GetEntity(prev.b);
                    if (eA && eB) {
                        CollisionEvent ev;
                        ev.entityA = eA;
                        ev.entityB = eB;
                        m_OnExit(ev);
                    }
                }
            }
        }

        m_PrevCollisions = m_CurrCollisions;
    }

    // ---------------------------------------------------------------------------
    bool PhysicsSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction,
                                 float maxDistance, Entity** outEntity,
                                 glm::vec3* outHitPoint) {
        if (!m_Scene) return false;

        // Guvende kalsin: 0 veya cok kucuk direction
        float len = glm::length(direction);
        if (len < 1e-6f) return false;
        glm::vec3 dir = direction / len;

        // Sifir bolunmesinden kac
        glm::vec3 invDir = glm::vec3(
            std::abs(dir.x) > 1e-9f ? 1.0f / dir.x : std::numeric_limits<float>::max(),
            std::abs(dir.y) > 1e-9f ? 1.0f / dir.y : std::numeric_limits<float>::max(),
            std::abs(dir.z) > 1e-9f ? 1.0f / dir.z : std::numeric_limits<float>::max()
        );

        float   closestT  = maxDistance;
        Entity* hitEntity = nullptr;

        for (const auto& entityPtr : m_Scene->GetEntities()) {
            Entity* entity = entityPtr.get();
            auto* col   = entity->GetComponent<BoxCollider>();
            auto* trans = entity->GetComponent<Transform>();
            if (!col || !trans) continue;

            glm::vec3 half    = col->size * trans->scale * 0.5f;
            glm::vec3 boxMin  = trans->position + col->center - half;
            glm::vec3 boxMax  = trans->position + col->center + half;

            float tMin, tMax;
            if (RayAABB(origin, invDir, boxMin, boxMax, tMin, tMax)) {
                if (tMin < closestT && tMin >= 0.0f) {
                    closestT  = tMin;
                    hitEntity = entity;
                }
            }
        }

        if (hitEntity) {
            if (outEntity)   *outEntity   = hitEntity;
            if (outHitPoint) *outHitPoint = origin + dir * closestT;
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------------------------
    bool PhysicsSystem::RaycastSphere(const glm::vec3& origin, const glm::vec3& direction,
                                       float maxDistance, float sphereRadius,
                                       Entity** outEntity, glm::vec3* outHitPoint) {
        if (!m_Scene) return false;

        glm::vec3 dir = glm::normalize(direction);
        float   closestT  = maxDistance;
        Entity* hitEntity = nullptr;

        for (const auto& entityPtr : m_Scene->GetEntities()) {
            Entity* entity = entityPtr.get();
            auto* trans = entity->GetComponent<Transform>();
            if (!trans) continue;

            // Kure merkezi = entity pozisyonu, yaricar = sphereRadius + entity scale ortalaması
            glm::vec3 center = trans->position;
            float     radius = sphereRadius;

            // Ray-Sphere: |origin + t*dir - center|^2 = r^2
            glm::vec3 oc = origin - center;
            float a  = glm::dot(dir, dir);
            float hb = glm::dot(oc, dir);
            float c  = glm::dot(oc, oc) - radius * radius;
            float disc = hb * hb - a * c;

            if (disc < 0.0f) continue;
            float t = (-hb - std::sqrt(disc)) / a;
            if (t < 0.0f) t = (-hb + std::sqrt(disc)) / a;
            if (t >= 0.0f && t < closestT) {
                closestT  = t;
                hitEntity = entity;
            }
        }

        if (hitEntity) {
            if (outEntity)   *outEntity   = hitEntity;
            if (outHitPoint) *outHitPoint = origin + dir * closestT;
            return true;
        }
        return false;
    }

} // namespace Archura
