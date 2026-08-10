#include "PhysicsSystem.h"

#include "../ecs/Component.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Archura {
namespace {
constexpr float kEpsilon = 1.0e-6f;
constexpr float kPositionSlop = 1.0e-3f;
constexpr float kCorrectionPercent = 0.8f;
constexpr float kCharacterSkin = 1.0e-3f;
constexpr float kMinWalkableNormalY = 0.6427876f; // cos(50 degrees)

struct ConvexPlane {
    glm::vec3 normal{0.0f}; // outward-facing, normalized
    float distance = 0.0f; // dot(normal, point) <= distance is solid
};

bool BuildExpandedRampPlanes(const Entity& entity, const BoxCollider& collider,
                             const glm::vec3& extent,
                             std::array<ConvexPlane, 6>& planes) {
    const glm::vec3 half = glm::abs(collider.size) * 0.5f;
    if (half.x <= kEpsilon || half.y <= kEpsilon || half.z <= kEpsilon)
        return false;

    const glm::mat4 world = entity.GetWorldTransform();
    const float determinant = glm::determinant(glm::mat3(world));
    if (!std::isfinite(determinant) || std::abs(determinant) <= kEpsilon)
        return false;
    const glm::mat4 planeTransform = glm::transpose(glm::inverse(world));

    const float minY = collider.center.y - half.y;
    const float minZ = collider.center.z - half.z;
    const float maxZ = collider.center.z + half.z;
    const float riseOverRun = (2.0f * half.y) / (2.0f * half.z);
    const std::array<glm::vec4, 6> localPlanes = {
        glm::vec4( 1,  0,  0, -(collider.center.x + half.x)),
        glm::vec4(-1,  0,  0,  (collider.center.x - half.x)),
        glm::vec4( 0, -1,  0,  minY),
        glm::vec4( 0,  0, -1,  minZ),
        glm::vec4( 0,  0,  1, -maxZ),
        glm::vec4( 0,  1, riseOverRun,
                  -(minY + riseOverRun * maxZ))
    };

    for (std::size_t i = 0; i < localPlanes.size(); ++i) {
        const glm::vec4 equation = planeTransform * localPlanes[i];
        const float normalLength = glm::length(glm::vec3(equation));
        if (!std::isfinite(normalLength) || normalLength <= kEpsilon)
            return false;
        const glm::vec3 normal = glm::vec3(equation) / normalLength;
        const float distance = -equation.w / normalLength;
        const float support = glm::dot(glm::abs(normal), extent);
        if (!std::isfinite(distance) || !std::isfinite(support)) return false;
        planes[i] = ConvexPlane{normal, distance + support};
    }
    return true;
}

bool PointInsideConvex(const glm::vec3& point,
                       const std::array<ConvexPlane, 6>& planes,
                       float& exitDistance, glm::vec3& exitNormal) {
    exitDistance = std::numeric_limits<float>::max();
    bool strictlyInside = true;
    for (const ConvexPlane& plane : planes) {
        const float clearance = plane.distance - glm::dot(plane.normal, point);
        if (clearance <= kEpsilon) strictlyInside = false;
        if (clearance < exitDistance) {
            exitDistance = clearance;
            exitNormal = plane.normal;
        }
    }
    return strictlyInside;
}

bool RayConvex(const glm::vec3& origin, const glm::vec3& direction,
               const std::array<ConvexPlane, 6>& planes, float maxDistance,
               float& hitTime, glm::vec3& hitNormal) {
    float enter = -std::numeric_limits<float>::infinity();
    float exit = maxDistance;
    glm::vec3 enterNormal(0.0f);
    bool hasEnteringPlane = false;
    for (const ConvexPlane& plane : planes) {
        const float signedDistance = glm::dot(plane.normal, origin) - plane.distance;
        const float denominator = glm::dot(plane.normal, direction);
        if (std::abs(denominator) <= kEpsilon) {
            if (signedDistance > 0.0f) return false;
            continue;
        }
        const float time = -signedDistance / denominator;
        if (denominator < 0.0f) {
            if (time > enter) {
                enter = time;
                enterNormal = plane.normal;
            }
            hasEnteringPlane = true;
        } else {
            exit = std::min(exit, time);
        }
        if (enter > exit) return false;
    }
    if (!hasEnteringPlane || exit < 0.0f || enter > maxDistance)
        return false;
    hitTime = std::max(0.0f, enter);
    hitNormal = enterNormal;
    return true;
}

float Clamp01(float value) noexcept {
    return std::max(0.0f, std::min(value, 1.0f));
}

float InverseMass(const RigidBody* body) noexcept {
    if (!body || body->isKinematic || !std::isfinite(body->mass) ||
        body->mass <= kEpsilon)
        return 0.0f;
    return 1.0f / body->mass;
}

void ApplyWorldTranslation(Entity& entity, const glm::vec3& translation) {
    auto* transform = entity.GetComponent<Transform>();
    if (!transform) return;
    const Entity* parent = entity.GetParent();
    if (!parent) {
        transform->position += translation;
        return;
    }
    const glm::mat3 parentBasis(parent->GetWorldTransform());
    const float determinant = glm::determinant(parentBasis);
    if (!std::isfinite(determinant) || std::abs(determinant) <= kEpsilon) return;
    transform->position += glm::inverse(parentBasis) * translation;
}
} // namespace

std::size_t PhysicsSystem::PairHash::operator()(const Pair& pair) const noexcept {
    const std::uint64_t a = pair.a.Value();
    const std::uint64_t b = pair.b.Value();
    const std::size_t h1 = std::hash<std::uint64_t>{}(a);
    const std::size_t h2 = std::hash<std::uint64_t>{}(b);
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6U) + (h1 >> 2U));
}

void PhysicsSystem::Init(Scene* scene) {
    m_Scene = scene;
    m_Accumulator = 0.0;
    m_PreviousContacts.clear();
}

void PhysicsSystem::Shutdown() {
    m_PreviousContacts.clear();
    m_Accumulator = 0.0;
    m_Scene = nullptr;
}

void PhysicsSystem::SetGravity(const glm::vec3& gravity) noexcept {
    if (IsFinite(gravity)) m_Gravity = gravity;
}

void PhysicsSystem::SetFixedTimeStep(float seconds) noexcept {
    if (std::isfinite(seconds) && seconds >= 1.0e-4f && seconds <= 0.1f) {
        m_FixedTimeStep = seconds;
        m_Accumulator = std::min(m_Accumulator, static_cast<double>(seconds));
    }
}

void PhysicsSystem::SetMaxSubsteps(std::uint32_t count) noexcept {
    m_MaxSubsteps = std::max<std::uint32_t>(1U, std::min<std::uint32_t>(count, 32U));
}

bool PhysicsSystem::IsFinite(const glm::vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

PhysicsSystem::Pair PhysicsSystem::CanonicalPair(EntityHandle a,
                                                  EntityHandle b) noexcept {
    return a.Value() < b.Value() ? Pair{a, b} : Pair{b, a};
}

void PhysicsSystem::Update(float deltaTime) {
    if (!m_Scene || !m_Simulating || m_Updating ||
        !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    struct UpdateGuard final {
        bool& flag;
        explicit UpdateGuard(bool& value) noexcept : flag(value) { flag = true; }
        ~UpdateGuard() { flag = false; }
    } guard(m_Updating);

    // Bounded catch-up prevents a debugger pause from causing a spiral of death.
    m_Accumulator += static_cast<double>(std::min(deltaTime, 0.25f));
    std::uint32_t steps = 0;
    while (m_Accumulator >= static_cast<double>(m_FixedTimeStep) &&
           steps < m_MaxSubsteps) {
        Step(m_FixedTimeStep);
        m_Accumulator -= static_cast<double>(m_FixedTimeStep);
        ++steps;
    }
    if (steps == m_MaxSubsteps &&
        m_Accumulator >= static_cast<double>(m_FixedTimeStep))
        m_Accumulator = std::fmod(m_Accumulator,
                                  static_cast<double>(m_FixedTimeStep));
}

void PhysicsSystem::Step(float deltaTime) {
    std::uint32_t motionSubsteps = 1;
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        const auto* body = entityPtr->GetComponent<RigidBody>();
        const auto* collider = entityPtr->GetComponent<BoxCollider>();
        const auto* transform = entityPtr->GetComponent<Transform>();
        if (!body || !body->continuous || body->isKinematic || !collider || !transform)
            continue;
        if (!IsFinite(body->velocity)) continue;
        const AABB worldBounds = WorldAABB(*entityPtr, *collider);
        if (!IsFinite(worldBounds.min) || !IsFinite(worldBounds.max)) continue;
        const glm::vec3 extent = worldBounds.max - worldBounds.min;
        const float minExtent = std::max(kEpsilon, std::min(extent.x, std::min(extent.y, extent.z)));
        const float travel = glm::length(body->velocity) * deltaTime;
        if (!std::isfinite(travel)) continue;
        const float requiredFloat = std::ceil(travel / (minExtent * 0.5f));
        const auto required = static_cast<std::uint32_t>(
            std::max(1.0f, std::min(requiredFloat, static_cast<float>(m_MaxSubsteps))));
        motionSubsteps = std::max(motionSubsteps, std::min(required, m_MaxSubsteps));
    }

    const float subDelta = deltaTime / static_cast<float>(motionSubsteps);
    std::unordered_map<Pair, Contact, PairHash> stepContacts;
    for (std::uint32_t i = 0; i < motionSubsteps; ++i) {
        Integrate(subDelta);
        DetectAndResolve(stepContacts);
    }
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        if (auto* body = entityPtr->GetComponent<RigidBody>())
            body->force = glm::vec3(0.0f);
    }
    DispatchEvents(std::move(stepContacts));
}

void PhysicsSystem::Integrate(float deltaTime) {
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        auto* body = entityPtr->GetComponent<RigidBody>();
        auto* transform = entityPtr->GetComponent<Transform>();
        const float inverseMass = InverseMass(body);
        if (!body || !transform || inverseMass <= 0.0f)
            continue;

        if (!IsFinite(body->velocity)) body->velocity = glm::vec3(0.0f);
        if (!IsFinite(body->force)) body->force = glm::vec3(0.0f);
        glm::vec3 acceleration = body->force * inverseMass;
        if (body->useGravity) acceleration += m_Gravity;
        body->velocity += acceleration * deltaTime;

        // exp damping is stable for arbitrarily large non-negative drag.
        const float drag = std::isfinite(body->drag) ? std::max(0.0f, body->drag) : 0.0f;
        body->velocity *= std::exp(-drag * deltaTime);
        ApplyWorldTranslation(*entityPtr, body->velocity * deltaTime);
    }
}

PhysicsSystem::AABB PhysicsSystem::WorldAABB(const Entity& entity,
                                             const BoxCollider& collider) const {
    const glm::mat4 world = entity.GetWorldTransform();
    const glm::vec3 localHalf = glm::abs(collider.size) * 0.5f;
    AABB result{glm::vec3(std::numeric_limits<float>::max()),
                glm::vec3(std::numeric_limits<float>::lowest())};
    for (int x : {-1, 1}) for (int y : {-1, 1}) for (int z : {-1, 1}) {
        const glm::vec3 local = collider.center + localHalf * glm::vec3(x, y, z);
        const glm::vec3 point = glm::vec3(world * glm::vec4(local, 1.0f));
        result.min = glm::min(result.min, point);
        result.max = glm::max(result.max, point);
    }
    return result;
}

std::vector<PhysicsSystem::Proxy> PhysicsSystem::BuildProxies() const {
    std::vector<Proxy> proxies;
    proxies.reserve(m_Scene->GetEntities().size());
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        const auto* collider = entityPtr->GetComponent<BoxCollider>();
        if (!collider || !entityPtr->GetComponent<Transform>()) continue;
        const AABB bounds = WorldAABB(*entityPtr, *collider);
        if (!IsFinite(bounds.min) || !IsFinite(bounds.max)) continue;
        proxies.push_back(Proxy{entityPtr->GetHandle(), bounds, collider->isTrigger});
    }
    std::sort(proxies.begin(), proxies.end(), [](const Proxy& lhs, const Proxy& rhs) {
        if (lhs.bounds.min.x != rhs.bounds.min.x) return lhs.bounds.min.x < rhs.bounds.min.x;
        return lhs.handle.Value() < rhs.handle.Value();
    });
    return proxies;
}

bool PhysicsSystem::Overlap(const AABB& a, const AABB& b, Contact& contact) {
    const glm::vec3 overlap = glm::min(a.max, b.max) - glm::max(a.min, b.min);
    if (overlap.x < 0.0f || overlap.y < 0.0f || overlap.z < 0.0f) return false;
    const glm::vec3 centerA = (a.min + a.max) * 0.5f;
    const glm::vec3 centerB = (b.min + b.max) * 0.5f;
    int axis = 0;
    if (overlap.y < overlap.x) axis = 1;
    if (overlap.z < overlap[axis]) axis = 2;
    contact.penetration = overlap[axis];
    contact.normal = glm::vec3(0.0f);
    contact.normal[axis] = centerB[axis] >= centerA[axis] ? 1.0f : -1.0f;
    contact.point = glm::clamp((centerA + centerB) * 0.5f,
                               glm::max(a.min, b.min), glm::min(a.max, b.max));
    return true;
}

void PhysicsSystem::DetectAndResolve(
    std::unordered_map<Pair, Contact, PairHash>& stepContacts) {
    const std::vector<Proxy> proxies = BuildProxies();
    stepContacts.reserve(stepContacts.size() + proxies.size());

    // Sweep-and-prune on X; Y/Z are tested only while intervals remain active.
    for (std::size_t i = 0; i < proxies.size(); ++i) {
        for (std::size_t j = i + 1; j < proxies.size(); ++j) {
            if (proxies[j].bounds.min.x > proxies[i].bounds.max.x) break;
            Entity* entityA = m_Scene->GetEntity(proxies[i].handle);
            Entity* entityB = m_Scene->GetEntity(proxies[j].handle);
            if (!entityA || !entityB) continue;
            auto* bodyA = entityA->GetComponent<RigidBody>();
            auto* bodyB = entityB->GetComponent<RigidBody>();
            const float invA = InverseMass(bodyA);
            const float invB = InverseMass(bodyB);
            const bool trigger = proxies[i].trigger || proxies[j].trigger;
            if (!trigger && invA + invB <= 0.0f) continue;

            Contact contact;
            const auto* colliderA = entityA->GetComponent<BoxCollider>();
            const auto* colliderB = entityB->GetComponent<BoxCollider>();
            const bool rampA = colliderA && colliderA->shape == BoxCollider::Shape::Ramp;
            const bool rampB = colliderB && colliderB->shape == BoxCollider::Shape::Ramp;
            if (rampA != rampB) {
                Entity* rampEntity = rampA ? entityA : entityB;
                const BoxCollider* rampCollider = rampA ? colliderA : colliderB;
                const AABB& otherBounds = rampA ? proxies[j].bounds : proxies[i].bounds;
                const glm::vec3 otherCenter = (otherBounds.min + otherBounds.max) * 0.5f;
                const glm::vec3 otherExtent = (otherBounds.max - otherBounds.min) * 0.5f;
                std::array<ConvexPlane, 6> planes;
                if (!BuildExpandedRampPlanes(*rampEntity, *rampCollider,
                                             otherExtent, planes))
                    continue;
                float penetration = std::numeric_limits<float>::max();
                glm::vec3 outwardNormal(0.0f);
                bool overlapping = true;
                for (const ConvexPlane& plane : planes) {
                    const float clearance = plane.distance -
                                            glm::dot(plane.normal, otherCenter);
                    if (clearance < -kEpsilon) {
                        overlapping = false;
                        break;
                    }
                    if (clearance < penetration) {
                        penetration = std::max(0.0f, clearance);
                        outwardNormal = plane.normal;
                    }
                }
                if (!overlapping) continue;
                const float support = glm::dot(glm::abs(outwardNormal), otherExtent);
                contact.penetration = penetration;
                contact.normal = rampA ? outwardNormal : -outwardNormal;
                contact.point = otherCenter - outwardNormal * (penetration + support);
            } else if (!Overlap(proxies[i].bounds, proxies[j].bounds, contact)) {
                continue;
            }
            const glm::vec3 solverNormal = contact.normal;
            contact.pair = CanonicalPair(proxies[i].handle, proxies[j].handle);
            if (contact.pair.a != proxies[i].handle) contact.normal = -contact.normal;
            contact.trigger = trigger;
            stepContacts.emplace(contact.pair, contact);
            if (trigger) continue;

            auto* transformA = entityA->GetComponent<Transform>();
            auto* transformB = entityB->GetComponent<Transform>();
            const float inverseMassSum = invA + invB;
            if (!transformA || !transformB || inverseMassSum <= 0.0f) continue;

            const float correctionMagnitude =
                std::max(contact.penetration - kPositionSlop, 0.0f) *
                kCorrectionPercent / inverseMassSum;
            const glm::vec3 correction = correctionMagnitude * solverNormal;
            ApplyWorldTranslation(*entityA, -correction * invA);
            ApplyWorldTranslation(*entityB, correction * invB);

            // Invalid/non-positive-mass bodies are defensive statics, not moving
            // kinematics. Only dynamic and explicitly kinematic bodies contribute
            // surface velocity to the contact solve.
            const glm::vec3 velocityA = bodyA && (invA > 0.0f || bodyA->isKinematic) &&
                                                IsFinite(bodyA->velocity)
                                            ? bodyA->velocity : glm::vec3(0.0f);
            const glm::vec3 velocityB = bodyB && (invB > 0.0f || bodyB->isKinematic) &&
                                                IsFinite(bodyB->velocity)
                                            ? bodyB->velocity : glm::vec3(0.0f);
            glm::vec3 relativeVelocity = velocityB - velocityA;
            const float normalSpeed = glm::dot(relativeVelocity, solverNormal);
            if (normalSpeed >= 0.0f) continue;

            const float restitutionA = bodyA ? Clamp01(bodyA->restitution) : 0.0f;
            const float restitutionB = bodyB ? Clamp01(bodyB->restitution) : 0.0f;
            const float restitution = std::min(restitutionA, restitutionB);
            const float normalImpulseMagnitude =
                -(1.0f + restitution) * normalSpeed / inverseMassSum;
            const glm::vec3 normalImpulse = normalImpulseMagnitude * solverNormal;
            if (bodyA) bodyA->velocity -= normalImpulse * invA;
            if (bodyB) bodyB->velocity += normalImpulse * invB;

            relativeVelocity =
                (bodyB && (invB > 0.0f || bodyB->isKinematic) && IsFinite(bodyB->velocity)
                     ? bodyB->velocity : glm::vec3(0.0f)) -
                (bodyA && (invA > 0.0f || bodyA->isKinematic) && IsFinite(bodyA->velocity)
                     ? bodyA->velocity : glm::vec3(0.0f));
            glm::vec3 tangent = relativeVelocity -
                                glm::dot(relativeVelocity, solverNormal) * solverNormal;
            const float tangentLength = glm::length(tangent);
            if (tangentLength <= kEpsilon) continue;
            tangent /= tangentLength;
            float tangentImpulseMagnitude = -glm::dot(relativeVelocity, tangent) /
                                            inverseMassSum;
            const float frictionA = bodyA && std::isfinite(bodyA->friction)
                                        ? std::max(0.0f, bodyA->friction) : 0.6f;
            const float frictionB = bodyB && std::isfinite(bodyB->friction)
                                        ? std::max(0.0f, bodyB->friction) : 0.6f;
            const float friction = std::sqrt(frictionA * frictionB);
            const float maxFriction = normalImpulseMagnitude * friction;
            tangentImpulseMagnitude = std::max(-maxFriction,
                                               std::min(tangentImpulseMagnitude, maxFriction));
            const glm::vec3 tangentImpulse = tangentImpulseMagnitude * tangent;
            if (bodyA) bodyA->velocity -= tangentImpulse * invA;
            if (bodyB) bodyB->velocity += tangentImpulse * invB;
        }
    }
}

void PhysicsSystem::DispatchEvents(
    std::unordered_map<Pair, Contact, PairHash>&& contacts) {
    std::vector<Contact> ordered;
    ordered.reserve(contacts.size());
    for (const auto& item : contacts) ordered.push_back(item.second);
    std::sort(ordered.begin(), ordered.end(), [](const Contact& lhs, const Contact& rhs) {
        if (lhs.pair.a.Value() != rhs.pair.a.Value())
            return lhs.pair.a.Value() < rhs.pair.a.Value();
        return lhs.pair.b.Value() < rhs.pair.b.Value();
    });
    for (const Contact& contact : ordered) {
        const bool existed = m_PreviousContacts.find(contact.pair) != m_PreviousContacts.end();
        CollisionEvent event{contact.pair.a, contact.pair.b, contact.point,
                             contact.normal, contact.penetration,
                             existed ? CollisionPhase::Stay : CollisionPhase::Enter,
                             contact.trigger};
        if (existed) {
            const CollisionCallback callback = m_OnStay;
            if (callback) callback(event);
        } else {
            const CollisionCallback callback = m_OnEnter;
            if (callback) callback(event);
        }
    }

    std::vector<Contact> exits;
    for (const auto& item : m_PreviousContacts)
        if (contacts.find(item.first) == contacts.end()) exits.push_back(item.second);
    std::sort(exits.begin(), exits.end(), [](const Contact& lhs, const Contact& rhs) {
        if (lhs.pair.a.Value() != rhs.pair.a.Value())
            return lhs.pair.a.Value() < rhs.pair.a.Value();
        return lhs.pair.b.Value() < rhs.pair.b.Value();
    });
    for (const Contact& contact : exits) {
        CollisionEvent event{contact.pair.a, contact.pair.b, contact.point,
                             contact.normal, 0.0f, CollisionPhase::Exit,
                             contact.trigger};
        const CollisionCallback callback = m_OnExit;
        if (callback) callback(event);
    }
    m_PreviousContacts = std::move(contacts);
}

bool PhysicsSystem::RayAABB(const glm::vec3& origin, const glm::vec3& direction,
                            const AABB& box, float maxDistance, float& hitTime,
                            glm::vec3* hitNormal) {
    float nearTime = 0.0f;
    float farTime = maxDistance;
    glm::vec3 nearNormal(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) <= kEpsilon) {
            if (origin[axis] < box.min[axis] || origin[axis] > box.max[axis]) return false;
            continue;
        }
        float t1 = (box.min[axis] - origin[axis]) / direction[axis];
        float t2 = (box.max[axis] - origin[axis]) / direction[axis];
        float sign = -1.0f;
        if (t1 > t2) { std::swap(t1, t2); sign = 1.0f; }
        if (t1 >= nearTime) {
            nearTime = t1;
            nearNormal = glm::vec3(0.0f);
            nearNormal[axis] = sign;
        }
        farTime = std::min(farTime, t2);
        if (nearTime > farTime) return false;
    }
    if (farTime < 0.0f || nearTime > maxDistance) return false;
    hitTime = std::max(0.0f, nearTime);
    if (hitNormal) *hitNormal = nearNormal;
    return true;
}

bool PhysicsSystem::SweepSphereAABB(const glm::vec3& origin,
                                    const glm::vec3& direction, float radius,
                                    const AABB& box, float maxDistance,
                                    float& hitTime) {
    // Squared distance from a moving point to an AABB is piecewise quadratic.
    // Split at slab crossings, then solve the exact quadratic on each interval.
    std::array<float, 8> breaks{};
    std::size_t count = 0;
    breaks[count++] = 0.0f;
    breaks[count++] = maxDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) <= kEpsilon) continue;
        for (float plane : {box.min[axis], box.max[axis]}) {
            const float time = (plane - origin[axis]) / direction[axis];
            if (time > 0.0f && time < maxDistance) breaks[count++] = time;
        }
    }
    std::sort(breaks.begin(), breaks.begin() + count);
    count = static_cast<std::size_t>(std::unique(breaks.begin(), breaks.begin() + count,
        [](float lhs, float rhs) { return std::abs(lhs - rhs) <= kEpsilon; }) - breaks.begin());
    const float radiusSquared = radius * radius;
    for (std::size_t interval = 0; interval + 1 < count; ++interval) {
        const float begin = breaks[interval];
        const float end = breaks[interval + 1];
        const float sample = (begin + end) * 0.5f;
        float quadratic = 0.0f;
        float linear = 0.0f;
        float constant = -radiusSquared;
        for (int axis = 0; axis < 3; ++axis) {
            const float samplePosition = origin[axis] + direction[axis] * sample;
            float boundary = 0.0f;
            bool outside = false;
            if (samplePosition < box.min[axis]) { boundary = box.min[axis]; outside = true; }
            else if (samplePosition > box.max[axis]) { boundary = box.max[axis]; outside = true; }
            if (!outside) continue;
            const float offset = origin[axis] - boundary;
            quadratic += direction[axis] * direction[axis];
            linear += 2.0f * offset * direction[axis];
            constant += offset * offset;
        }
        const auto evaluate = [&](float time) {
            return (quadratic * time + linear) * time + constant;
        };
        if (evaluate(begin) <= kEpsilon) { hitTime = begin; return true; }
        if (quadratic <= kEpsilon) continue;
        const float discriminant = linear * linear - 4.0f * quadratic * constant;
        if (discriminant < 0.0f) continue;
        const float root = (-linear - std::sqrt(std::max(0.0f, discriminant))) /
                           (2.0f * quadratic);
        if (root >= begin - kEpsilon && root <= end + kEpsilon) {
            hitTime = std::max(0.0f, root);
            return true;
        }
    }
    // Handles maxDistance == 0 and contact exactly at the final endpoint.
    glm::vec3 closest = glm::clamp(origin + direction * maxDistance, box.min, box.max);
    if (glm::dot(origin + direction * maxDistance - closest,
                 origin + direction * maxDistance - closest) <= radiusSquared + kEpsilon) {
        hitTime = maxDistance;
        return true;
    }
    return false;
}

bool PhysicsSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction,
                            float maxDistance, Entity** outEntity,
                            glm::vec3* outHitPoint) const {
    if (outEntity) *outEntity = nullptr;
    if (outHitPoint) *outHitPoint = glm::vec3(0.0f);
    if (!m_Scene || !IsFinite(origin) || !IsFinite(direction) ||
        !std::isfinite(maxDistance) || maxDistance < 0.0f) return false;
    const float length = glm::length(direction);
    if (!std::isfinite(length) || length <= kEpsilon) return false;
    const glm::vec3 rayDirection = direction / length;
    float closest = maxDistance;
    Entity* hit = nullptr;
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        const auto* collider = entityPtr->GetComponent<BoxCollider>();
        if (!collider) continue;
        if (collider->shape == BoxCollider::Shape::Ramp) {
            std::array<ConvexPlane, 6> planes;
            if (!BuildExpandedRampPlanes(*entityPtr, *collider,
                                         glm::vec3(0.0f), planes))
                continue;
            float time = 0.0f;
            glm::vec3 normal(0.0f);
            if (RayConvex(origin, rayDirection, planes, closest, time, normal) &&
                (!hit || time < closest ||
                 (time == closest && entityPtr->GetID() < hit->GetID()))) {
                closest = time;
                hit = entityPtr.get();
            }
            continue;
        }
        const AABB bounds = WorldAABB(*entityPtr, *collider);
        if (!IsFinite(bounds.min) || !IsFinite(bounds.max)) continue;
        float time = 0.0f;
        if (RayAABB(origin, rayDirection, bounds,
                    closest, time) && (!hit || time < closest ||
                    (time == closest && entityPtr->GetID() < hit->GetID()))) {
            closest = time;
            hit = entityPtr.get();
        }
    }
    if (!hit) return false;
    if (outEntity) *outEntity = hit;
    if (outHitPoint) *outHitPoint = origin + rayDirection * closest;
    return true;
}

bool PhysicsSystem::RaycastSphere(const glm::vec3& origin,
                                  const glm::vec3& direction,
                                  float maxDistance, float sphereRadius,
                                  Entity** outEntity,
                                  glm::vec3* outHitPoint) const {
    if (outEntity) *outEntity = nullptr;
    if (outHitPoint) *outHitPoint = glm::vec3(0.0f);
    if (!m_Scene || !IsFinite(origin) || !IsFinite(direction) ||
        !std::isfinite(maxDistance) || maxDistance < 0.0f ||
        !std::isfinite(sphereRadius) || sphereRadius < 0.0f) return false;
    const float length = glm::length(direction);
    if (!std::isfinite(length) || length <= kEpsilon) return false;
    ShapeCastHit hit;
    QueryFilter filter;
    filter.includeTriggers = true; // preserve legacy query semantics
    if (!SweepSphere(origin, direction / length * maxDistance, sphereRadius,
                     hit, filter)) return false;
    Entity* entity = m_Scene->GetEntity(hit.entity);
    if (!entity) return false;
    if (outEntity) *outEntity = entity;
    // This is the sphere center at time of impact, the conventional sweep result.
    if (outHitPoint) *outHitPoint = origin + direction / length * hit.distance;
    return true;
}

bool PhysicsSystem::SweepSphere(const glm::vec3& origin,
                                const glm::vec3& displacement,
                                float sphereRadius, ShapeCastHit& outHit,
                                const QueryFilter& filter) const {
    outHit = ShapeCastHit{};
    if (!m_Scene || !IsFinite(origin) || !IsFinite(displacement) ||
        !std::isfinite(sphereRadius) || sphereRadius < 0.0f) return false;
    const float maxDistance = glm::length(displacement);
    if (!std::isfinite(maxDistance) || maxDistance <= kEpsilon) return false;
    const glm::vec3 direction = displacement / maxDistance;
    float closest = maxDistance;
    Entity* hit = nullptr;
    AABB hitBounds{};
    glm::vec3 hitNormal(0.0f);
    bool hitRamp = false;
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        const EntityHandle handle = entityPtr->GetHandle();
        if (handle == filter.ignoredA || handle == filter.ignoredB) continue;
        const auto* collider = entityPtr->GetComponent<BoxCollider>();
        if (!collider || (collider->isTrigger && !filter.includeTriggers)) continue;
        if (collider->shape == BoxCollider::Shape::Ramp) {
            std::array<ConvexPlane, 6> planes;
            if (!BuildExpandedRampPlanes(*entityPtr, *collider,
                                         glm::vec3(0.0f), planes))
                continue;
            for (ConvexPlane& plane : planes) plane.distance += sphereRadius;
            float time = 0.0f;
            glm::vec3 normal(0.0f);
            if (RayConvex(origin, direction, planes, closest, time, normal) &&
                (!hit || time < closest ||
                 (time == closest &&
                  handle.Value() < hit->GetHandle().Value()))) {
                closest = time;
                hit = entityPtr.get();
                hitNormal = normal;
                hitRamp = true;
            }
            continue;
        }
        const AABB bounds = WorldAABB(*entityPtr, *collider);
        if (!IsFinite(bounds.min) || !IsFinite(bounds.max)) continue;
        float time = 0.0f;
        if (SweepSphereAABB(origin, direction, sphereRadius, bounds,
                            closest, time) &&
            (!hit || time < closest ||
             (time == closest && handle.Value() < hit->GetHandle().Value()))) {
            closest = time;
            hit = entityPtr.get();
            hitBounds = bounds;
            hitRamp = false;
        }
    }
    if (!hit) return false;
    const glm::vec3 centerAtImpact = origin + direction * closest;
    if (hitRamp) {
        outHit = ShapeCastHit{hit->GetHandle(),
                              centerAtImpact - hitNormal * sphereRadius,
                              hitNormal, closest};
        return true;
    }
    const glm::vec3 point = glm::clamp(centerAtImpact, hitBounds.min, hitBounds.max);
    glm::vec3 normal = centerAtImpact - point;
    const float normalLength = glm::length(normal);
    normal = normalLength > kEpsilon ? normal / normalLength : -direction;
    outHit = ShapeCastHit{hit->GetHandle(), point, normal, closest};
    return true;
}

PhysicsSystem::CharacterMoveResult PhysicsSystem::MoveKinematicAABB(
    const glm::vec3& position, const glm::vec3& halfExtent,
    const glm::vec3& velocity, float deltaTime, EntityHandle ignored) const {
    CharacterMoveResult result{position, velocity, false, false};
    if (!m_Scene || !IsFinite(position) || !IsFinite(halfExtent) ||
        !IsFinite(velocity) || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return result;
    const glm::vec3 extent = glm::max(glm::abs(halfExtent), glm::vec3(kCharacterSkin));
    // Resolve an initial overlap before sweeping. A point ray that starts in a
    // Minkowski-expanded obstacle has no unique entry normal and would otherwise
    // consume every slide iteration without moving.
    constexpr int kMaxDepenetrationIterations = 8;
    for (int iteration = 0; iteration < kMaxDepenetrationIterations; ++iteration) {
        float bestDistance = std::numeric_limits<float>::max();
        glm::vec3 bestNormal(0.0f);
        EntityID bestID = std::numeric_limits<EntityID>::max();
        bool foundOverlap = false;
        for (const auto& entityPtr : m_Scene->GetEntities()) {
            if (entityPtr->GetHandle() == ignored) continue;
            const auto* collider = entityPtr->GetComponent<BoxCollider>();
            if (!collider || collider->isTrigger) continue;
            if (collider->shape == BoxCollider::Shape::Ramp) {
                std::array<ConvexPlane, 6> planes;
                if (!BuildExpandedRampPlanes(*entityPtr, *collider, extent, planes))
                    continue;
                float distance = 0.0f;
                glm::vec3 normal(0.0f);
                if (!PointInsideConvex(result.position, planes, distance, normal))
                    continue;
                if (distance < bestDistance ||
                    (distance == bestDistance && entityPtr->GetID() < bestID)) {
                    bestDistance = distance;
                    bestNormal = normal;
                    bestID = entityPtr->GetID();
                    foundOverlap = true;
                }
                continue;
            }
            AABB expanded = WorldAABB(*entityPtr, *collider);
            if (!IsFinite(expanded.min) || !IsFinite(expanded.max)) continue;
            expanded.min -= extent;
            expanded.max += extent;
            bool strictlyInside = true;
            for (int axis = 0; axis < 3; ++axis)
                strictlyInside = strictlyInside && result.position[axis] > expanded.min[axis] &&
                                 result.position[axis] < expanded.max[axis];
            if (!strictlyInside) continue;

            for (int axis = 0; axis < 3; ++axis) {
                const float toMin = result.position[axis] - expanded.min[axis];
                const float toMax = expanded.max[axis] - result.position[axis];
                const float distance = std::min(toMin, toMax);
                const glm::vec3 normal = axis == 0
                    ? glm::vec3(toMin <= toMax ? -1.0f : 1.0f, 0.0f, 0.0f)
                    : axis == 1
                        ? glm::vec3(0.0f, toMin <= toMax ? -1.0f : 1.0f, 0.0f)
                        : glm::vec3(0.0f, 0.0f, toMin <= toMax ? -1.0f : 1.0f);
                if (distance < bestDistance ||
                    (distance == bestDistance && entityPtr->GetID() < bestID)) {
                    bestDistance = distance;
                    bestNormal = normal;
                    bestID = entityPtr->GetID();
                    foundOverlap = true;
                }
            }
        }
        if (!foundOverlap) break;
        result.position += bestNormal * (bestDistance + kCharacterSkin);
        const float intoSurface = glm::dot(result.velocity, bestNormal);
        if (intoSurface < 0.0f) result.velocity -= bestNormal * intoSurface;
        if (bestNormal.y > 0.5f) result.grounded = true;
        if (bestNormal.y < -0.5f) result.hitCeiling = true;
    }
    glm::vec3 remaining = result.velocity * deltaTime;
    constexpr int kMaxSlideIterations = 4;
    for (int iteration = 0; iteration < kMaxSlideIterations; ++iteration) {
        const float distance = glm::length(remaining);
        if (distance <= kEpsilon) break;
        const glm::vec3 direction = remaining / distance;
        float closest = distance;
        glm::vec3 hitNormal(0.0f);
        EntityID hitID = std::numeric_limits<EntityID>::max();
        bool found = false;
        for (const auto& entityPtr : m_Scene->GetEntities()) {
            if (entityPtr->GetHandle() == ignored) continue;
            const auto* collider = entityPtr->GetComponent<BoxCollider>();
            if (!collider || collider->isTrigger) continue;
            if (collider->shape == BoxCollider::Shape::Ramp) {
                std::array<ConvexPlane, 6> planes;
                if (!BuildExpandedRampPlanes(*entityPtr, *collider, extent, planes))
                    continue;
                float time = 0.0f;
                glm::vec3 normal(0.0f);
                if (RayConvex(result.position, direction, planes, closest,
                              time, normal) &&
                    !(time <= kEpsilon &&
                      glm::dot(direction, normal) >= -kEpsilon) &&
                    (time < closest || (!found && time == closest) ||
                     (time == closest && entityPtr->GetID() < hitID))) {
                    closest = time;
                    hitNormal = normal;
                    hitID = entityPtr->GetID();
                    found = true;
                }
                continue;
            }
            AABB expanded = WorldAABB(*entityPtr, *collider);
            if (!IsFinite(expanded.min) || !IsFinite(expanded.max)) continue;
            expanded.min -= extent;
            expanded.max += extent;
            float time = 0.0f;
            glm::vec3 normal(0.0f);
            if (RayAABB(result.position, direction, expanded, closest, time, &normal) &&
                !(time <= kEpsilon && glm::dot(direction, normal) >= -kEpsilon) &&
                (time < closest || (!found && time == closest) ||
                 (time == closest && entityPtr->GetID() < hitID))) {
                closest = time;
                hitNormal = normal;
                hitID = entityPtr->GetID();
                found = true;
            }
        }
        if (!found) { result.position += remaining; break; }
        if (hitNormal.y > 0.0f && hitNormal.y < kMinWalkableNormalY) {
            const glm::vec2 horizontal(hitNormal.x, hitNormal.z);
            const float horizontalLength = glm::length(horizontal);
            if (horizontalLength > kEpsilon)
                hitNormal = glm::vec3(horizontal.x / horizontalLength, 0.0f,
                                      horizontal.y / horizontalLength);
        }
        const float safeDistance = std::max(0.0f, closest - kCharacterSkin);
        result.position += direction * safeDistance;
        const glm::vec3 untraveled = direction * (distance - safeDistance);
        if (hitNormal.y > 0.5f) result.grounded = true;
        if (hitNormal.y < -0.5f) result.hitCeiling = true;
        const float intoSurface = glm::dot(result.velocity, hitNormal);
        if (intoSurface < 0.0f) result.velocity -= hitNormal * intoSurface;
        remaining = untraveled - hitNormal * std::min(0.0f, glm::dot(untraveled, hitNormal));
    }

    // Stable ground probe allows zero-horizontal-motion frames to remain grounded.
    if (!result.grounded) {
        const glm::vec3 down(0.0f, -1.0f, 0.0f);
        for (const auto& entityPtr : m_Scene->GetEntities()) {
            if (entityPtr->GetHandle() == ignored) continue;
            const auto* collider = entityPtr->GetComponent<BoxCollider>();
            if (!collider || collider->isTrigger) continue;
            if (collider->shape == BoxCollider::Shape::Ramp) {
                std::array<ConvexPlane, 6> planes;
                if (!BuildExpandedRampPlanes(*entityPtr, *collider, extent, planes))
                    continue;
                float time = 0.0f;
                glm::vec3 normal(0.0f);
                if (RayConvex(result.position, down, planes,
                              2.0f * kCharacterSkin, time, normal) &&
                    normal.y >= kMinWalkableNormalY) {
                    result.grounded = true;
                    break;
                }
                continue;
            }
            AABB expanded = WorldAABB(*entityPtr, *collider);
            if (!IsFinite(expanded.min) || !IsFinite(expanded.max)) continue;
            expanded.min -= extent;
            expanded.max += extent;
            float time = 0.0f;
            glm::vec3 normal(0.0f);
            if (RayAABB(result.position, down, expanded, 2.0f * kCharacterSkin,
                        time, &normal) && normal.y > 0.5f) {
                result.grounded = true;
                break;
            }
        }
    }
    return result;
}

} // namespace Archura
