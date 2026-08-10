#pragma once

#include "../ecs/Entity.h"
#include "CollisionEvent.h"

#include <glm/glm.hpp>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Archura {

class Scene;

class PhysicsSystem final {
public:
    struct CharacterMoveResult {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        bool grounded = false;
        bool hitCeiling = false;
    };
    struct QueryFilter {
        EntityHandle ignoredA{};
        EntityHandle ignoredB{};
        bool includeTriggers = false;
    };
    struct ShapeCastHit {
        EntityHandle entity{};
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        float distance = 0.0f;
    };

    void Init(Scene* scene);
    void Update(float deltaTime);
    void Shutdown();

    void SetGravity(const glm::vec3& gravity) noexcept;
    const glm::vec3& GetGravity() const noexcept { return m_Gravity; }
    void SetSimulating(bool enabled) noexcept { m_Simulating = enabled; }
    bool IsSimulating() const noexcept { return m_Simulating; }
    void SetDebugDraw(bool enabled) noexcept { m_DebugDraw = enabled; }
    bool GetDebugDraw() const noexcept { return m_DebugDraw; }

    void SetFixedTimeStep(float seconds) noexcept;
    float GetFixedTimeStep() const noexcept { return m_FixedTimeStep; }
    void SetMaxSubsteps(std::uint32_t count) noexcept;

    void SetOnCollisionEnter(CollisionCallback cb) { m_OnEnter = std::move(cb); }
    void SetOnCollisionStay(CollisionCallback cb) { m_OnStay = std::move(cb); }
    void SetOnCollisionExit(CollisionCallback cb) { m_OnExit = std::move(cb); }

    bool Raycast(const glm::vec3& origin, const glm::vec3& direction,
                 float maxDistance, Entity** outEntity = nullptr,
                 glm::vec3* outHitPoint = nullptr) const;

    // Sweeps a sphere against scene box colliders (Minkowski-expanded AABBs).
    bool RaycastSphere(const glm::vec3& origin, const glm::vec3& direction,
                       float maxDistance, float sphereRadius,
                       Entity** outEntity = nullptr,
                       glm::vec3* outHitPoint = nullptr) const;

    // Authoritative continuous scene query. Projectile gameplay excludes its
    // own and owner handles and ignores triggers unless explicitly requested.
    bool SweepSphere(const glm::vec3& origin, const glm::vec3& displacement,
                     float sphereRadius, ShapeCastHit& outHit,
                     const QueryFilter& filter = {}) const;

    // Authoritative kinematic character query used by FPSController. Position
    // is the character AABB center; velocity is projected along hit planes.
    CharacterMoveResult MoveKinematicAABB(const glm::vec3& position,
                                          const glm::vec3& halfExtent,
                                          const glm::vec3& velocity,
                                          float deltaTime,
                                          EntityHandle ignored = {}) const;

private:
    struct AABB { glm::vec3 min{0.0f}; glm::vec3 max{0.0f}; };
    struct Proxy {
        EntityHandle handle{};
        AABB bounds{};
        bool trigger = false;
    };
    struct Pair {
        EntityHandle a{};
        EntityHandle b{};
        friend bool operator==(const Pair& lhs, const Pair& rhs) noexcept {
            return lhs.a == rhs.a && lhs.b == rhs.b;
        }
    };
    struct PairHash {
        std::size_t operator()(const Pair& pair) const noexcept;
    };
    struct Contact {
        Pair pair{};
        glm::vec3 point{0.0f};
        glm::vec3 normal{0.0f};
        float penetration = 0.0f;
        bool trigger = false;
    };

    void Step(float deltaTime);
    void Integrate(float deltaTime);
    void DetectAndResolve(std::unordered_map<Pair, Contact, PairHash>& stepContacts);
    void DispatchEvents(std::unordered_map<Pair, Contact, PairHash>&& contacts);
    std::vector<Proxy> BuildProxies() const;
    AABB WorldAABB(const Entity& entity, const BoxCollider& collider) const;
    static bool Overlap(const AABB& a, const AABB& b, Contact& contact);
    static bool RayAABB(const glm::vec3& origin, const glm::vec3& direction,
                        const AABB& box, float maxDistance, float& hitTime,
                        glm::vec3* hitNormal = nullptr);
    static bool SweepSphereAABB(const glm::vec3& origin,
                                const glm::vec3& direction,
                                float radius, const AABB& box,
                                float maxDistance, float& hitTime);
    static bool IsFinite(const glm::vec3& value) noexcept;
    static Pair CanonicalPair(EntityHandle a, EntityHandle b) noexcept;

    Scene* m_Scene = nullptr;
    glm::vec3 m_Gravity{0.0f, -9.81f, 0.0f};
    bool m_Simulating = true;
    bool m_DebugDraw = false;
    float m_FixedTimeStep = 1.0f / 60.0f;
    double m_Accumulator = 0.0;
    std::uint32_t m_MaxSubsteps = 8;
    bool m_Updating = false;
    std::unordered_map<Pair, Contact, PairHash> m_PreviousContacts;
    CollisionCallback m_OnEnter;
    CollisionCallback m_OnStay;
    CollisionCallback m_OnExit;
};

} // namespace Archura
