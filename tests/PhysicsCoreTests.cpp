#include "ecs/Entity.h"
#include "game/PhysicsSystem.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int gFailures = 0;

#define CHECK(condition) do {                                                   \
    if (!(condition)) {                                                        \
        std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "         \
                  << #condition << '\n';                                       \
        ++gFailures;                                                           \
    }                                                                          \
} while (false)

bool Near(float a, float b, float epsilon = 1.0e-3f) {
    return std::abs(a - b) <= epsilon;
}

Archura::Entity* AddBox(Archura::Scene& scene, const char* name,
                        const glm::vec3& position, const glm::vec3& size,
                        bool dynamic = false) {
    Archura::Entity* entity = scene.CreateEntity(name);
    entity->GetComponent<Archura::Transform>()->position = position;
    entity->AddComponent<Archura::BoxCollider>()->size = size;
    if (dynamic) {
        auto* body = entity->AddComponent<Archura::RigidBody>();
        body->useGravity = false;
        body->drag = 0.0f;
    }
    return entity;
}

Archura::Entity* AddRamp(Archura::Scene& scene, const char* name,
                         const glm::vec3& position, const glm::vec3& size) {
    Archura::Entity* entity = scene.CreateEntity(name);
    entity->GetComponent<Archura::Transform>()->position = position;
    auto* collider = entity->AddComponent<Archura::BoxCollider>();
    collider->size = size;
    collider->center = {0.0f, size.y * 0.5f, 0.0f};
    collider->shape = Archura::BoxCollider::Shape::Ramp;
    return entity;
}

void TestWorldBoundsAndQueries() {
    Archura::Scene scene("queries");
    Archura::Entity* box = AddBox(scene, "negative-scale", {5.0f, 0.0f, 0.0f},
                                  {1.0f, 2.0f, 2.0f});
    auto* transform = box->GetComponent<Archura::Transform>();
    transform->scale = {-2.0f, 1.0f, 1.0f};
    box->GetComponent<Archura::BoxCollider>()->center = {1.0f, 0.0f, 0.0f};

    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    Archura::Entity* hit = reinterpret_cast<Archura::Entity*>(1);
    glm::vec3 point(99.0f);
    CHECK(physics.Raycast({0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f},
                          10.0f, &hit, &point));
    CHECK(hit == box);
    CHECK(Near(point.x, 2.0f));

    CHECK(physics.RaycastSphere({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                10.0f, 0.5f, &hit, &point));
    CHECK(Near(point.x, 1.5f));
    CHECK(physics.RaycastSphere({0.0f, 1.4f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                10.0f, 0.5f, &hit, &point));
    CHECK(Near(point.x, 1.7f, 2.0e-3f)); // rounded corner, not expanded-box x=1.5

    hit = reinterpret_cast<Archura::Entity*>(1);
    CHECK(!physics.Raycast({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
                           10.0f, &hit, &point));
    CHECK(hit == nullptr);
    CHECK(!physics.Raycast({0.0f, 0.0f, 0.0f},
                           {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f},
                           10.0f, &hit, &point));
    CHECK(!physics.RaycastSphere(glm::vec3(0.0f), {1.0f, 0.0f, 0.0f},
                                 10.0f, -1.0f, &hit, &point));
}

void TestRotatedNegativeScaledParentBounds() {
    Archura::Scene scene("parented-bounds");
    Archura::Entity* parent = scene.CreateEntity("parent");
    auto* parentTransform = parent->GetComponent<Archura::Transform>();
    parentTransform->position = {10.0f, 0.0f, 0.0f};
    parentTransform->rotation = {0.0f, 0.0f, 90.0f};
    parentTransform->scale = {2.0f, 3.0f, 1.0f};
    Archura::Entity* child = AddBox(scene, "child", {1.0f, 0.0f, 0.0f},
                                    {2.0f, 2.0f, 2.0f});
    child->GetComponent<Archura::Transform>()->scale = {-1.0f, 1.0f, 1.0f};
    child->GetComponent<Archura::BoxCollider>()->center = {0.5f, 0.0f, 0.0f};
    CHECK(child->TrySetParent(parent));

    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    Archura::Entity* hit = nullptr;
    glm::vec3 point(0.0f);
    CHECK(physics.Raycast({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                          20.0f, &hit, &point));
    CHECK(hit == child);
    CHECK(Near(point.x, 7.0f, 2.0e-3f));
}

void TestIntegrationAndImpulse() {
    Archura::Scene scene("solver");
    Archura::Entity* left = AddBox(scene, "left", {-0.75f, 0.0f, 0.0f},
                                   {2.0f, 2.0f, 2.0f}, true);
    Archura::Entity* right = AddBox(scene, "right", {0.75f, 0.0f, 0.0f},
                                    {2.0f, 2.0f, 2.0f}, true);
    auto* leftBody = left->GetComponent<Archura::RigidBody>();
    auto* rightBody = right->GetComponent<Archura::RigidBody>();
    leftBody->mass = 1.0f;
    rightBody->mass = 3.0f;
    leftBody->velocity.x = 1.0f;
    rightBody->velocity.x = -1.0f;
    leftBody->restitution = rightBody->restitution = 1.0f;

    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    int enterCount = 0;
    int stayCount = 0;
    physics.SetOnCollisionEnter([&](const Archura::CollisionEvent& event) {
        ++enterCount;
        CHECK(event.phase == Archura::CollisionPhase::Enter);
        CHECK(event.entityA.Value() < event.entityB.Value());
    });
    physics.SetOnCollisionStay([&](const Archura::CollisionEvent& event) {
        ++stayCount;
        CHECK(event.phase == Archura::CollisionPhase::Stay);
    });
    physics.Update(1.0f / 60.0f);
    CHECK(enterCount == 1);
    CHECK(leftBody->velocity.x < 0.0f);
    CHECK(rightBody->velocity.x > -1.0f);
    const float leftCorrection = std::abs(left->GetComponent<Archura::Transform>()->position.x + 0.75f);
    const float rightCorrection = std::abs(right->GetComponent<Archura::Transform>()->position.x - 0.75f);
    CHECK(leftCorrection > rightCorrection * 2.5f); // inverse mass weighting
    physics.Update(1.0f / 60.0f);
    CHECK(stayCount == 1);
}

void TestForceDragAndFixedStep() {
    Archura::Scene scene("integration");
    Archura::Entity* bodyEntity = AddBox(scene, "body", glm::vec3(0.0f),
                                         glm::vec3(1.0f), true);
    auto* body = bodyEntity->GetComponent<Archura::RigidBody>();
    body->mass = 2.0f;
    body->drag = 0.0f;
    body->force = {12.0f, 0.0f, 0.0f};
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    physics.SetFixedTimeStep(0.1f);
    physics.Update(0.05f);
    CHECK(Near(bodyEntity->GetComponent<Archura::Transform>()->position.x, 0.0f));
    physics.Update(0.05f);
    CHECK(Near(body->velocity.x, 0.6f));
    CHECK(Near(bodyEntity->GetComponent<Archura::Transform>()->position.x, 0.06f));
    CHECK(Near(glm::length(body->force), 0.0f));

    body->drag = 100000.0f;
    body->velocity = {10.0f, 0.0f, 0.0f};
    physics.Update(0.1f);
    CHECK(std::isfinite(body->velocity.x));
    CHECK(body->velocity.x >= 0.0f);
}

void TestTriggersAndDestroyedExit() {
    Archura::Scene scene("events");
    Archura::Entity* trigger = AddBox(scene, "trigger", glm::vec3(0.0f),
                                      glm::vec3(2.0f));
    trigger->GetComponent<Archura::BoxCollider>()->isTrigger = true;
    Archura::Entity* body = AddBox(scene, "body", {0.5f, 0.0f, 0.0f},
                                   glm::vec3(2.0f), true);
    const glm::vec3 initial = body->GetComponent<Archura::Transform>()->position;
    const Archura::EntityHandle destroyed = trigger->GetHandle();

    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    std::vector<Archura::CollisionEvent> exits;
    int enters = 0;
    physics.SetOnCollisionEnter([&](const Archura::CollisionEvent& event) {
        ++enters;
        CHECK(event.isTrigger);
    });
    physics.SetOnCollisionExit([&](const Archura::CollisionEvent& event) {
        exits.push_back(event);
    });
    physics.Update(1.0f / 60.0f);
    CHECK(enters == 1);
    CHECK(body->GetComponent<Archura::Transform>()->position == initial);
    CHECK(scene.DestroyEntity(destroyed));
    physics.Update(1.0f / 60.0f);
    CHECK(exits.size() == 1);
    CHECK(exits[0].phase == Archura::CollisionPhase::Exit);
    CHECK(exits[0].entityA == destroyed || exits[0].entityB == destroyed);
    CHECK(scene.GetEntity(destroyed) == nullptr); // event remains safe to retain
}

void TestAuthoritativeCharacterSweep() {
    Archura::Scene scene("character");
    AddBox(scene, "wall", {2.0f, 0.0f, 0.0f}, {0.5f, 4.0f, 4.0f});
    AddBox(scene, "floor", {0.0f, -1.0f, 0.0f}, {10.0f, 1.0f, 10.0f});
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    const auto move = physics.MoveKinematicAABB(
        {0.0f, 0.01f, 0.0f}, {0.3f, 0.5f, 0.3f},
        {100.0f, -1.0f, 4.0f}, 0.1f);
    CHECK(move.position.x < 1.46f);
    CHECK(Near(move.velocity.x, 0.0f));
    CHECK(move.position.z > 0.1f); // slides instead of cancelling all motion
    CHECK(move.grounded);
}

void TestRampShapeQueriesAndCharacterClimb() {
    Archura::Scene scene("ramp-character");
    Archura::Entity* ramp = AddRamp(scene, "ramp", glm::vec3(0.0f),
                                    {2.0f, 1.0f, 4.0f});
    AddBox(scene, "floor", {0.0f, -0.5f, 0.0f}, {20.0f, 1.0f, 20.0f});
    Archura::PhysicsSystem physics;
    physics.Init(&scene);

    Archura::Entity* hit = nullptr;
    glm::vec3 point(0.0f);
    CHECK(physics.Raycast({0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
                          10.0f, &hit, &point));
    CHECK(hit == ramp);
    CHECK(Near(point.y, 0.5f));

    // A box collider would hit the front at z=2. The wedge must not be hit
    // until the ray reaches the actual inclined surface at z=-1.2.
    CHECK(physics.Raycast({0.0f, 0.8f, 3.0f}, {0.0f, 0.0f, -1.0f},
                          10.0f, &hit, &point));
    CHECK(hit == ramp);
    CHECK(Near(point.z, -1.2f, 2.0e-3f));

    const auto move = physics.MoveKinematicAABB(
        {0.0f, 0.901f, 2.6f}, {0.3f, 0.9f, 0.3f},
        {0.0f, -1.0f, -2.0f}, 0.5f);
    CHECK(move.position.z < 2.2f);
    CHECK(move.position.y > 0.95f);
    CHECK(move.velocity.y > 0.0f);
    CHECK(move.grounded);

    const auto idle = physics.MoveKinematicAABB(
        move.position, {0.3f, 0.9f, 0.3f}, glm::vec3(0.0f), 1.0f / 60.0f);
    CHECK(idle.grounded);
}

void TestDynamicBodyResolvesAgainstRampSurface() {
    Archura::Scene scene("ramp-dynamic");
    AddRamp(scene, "ramp", glm::vec3(0.0f), {2.0f, 1.0f, 4.0f});
    Archura::Entity* bodyEntity = AddBox(scene, "body", {0.0f, 0.55f, 0.0f},
                                         {0.2f, 0.2f, 0.2f}, true);
    auto* body = bodyEntity->GetComponent<Archura::RigidBody>();
    body->velocity = {0.0f, -1.0f, 0.0f};

    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    physics.Update(1.0f / 60.0f);

    const glm::vec3 position =
        bodyEntity->GetComponent<Archura::Transform>()->position;
    CHECK(position.y > 0.55f);
    CHECK(body->velocity.y > -1.0f);
}

void TestCharacterInitialPenetrationAndSurfaceDeparture() {
    Archura::Scene scene("character-overlap");
    AddBox(scene, "wall", {0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, 4.0f});
    Archura::PhysicsSystem physics;
    physics.Init(&scene);

    const auto depenetrated = physics.MoveKinematicAABB(
        {0.9f, 0.0f, 0.0f}, {0.25f, 0.5f, 0.25f},
        {2.0f, 0.0f, 0.0f}, 0.1f);
    CHECK(depenetrated.position.x > 1.25f);
    CHECK(depenetrated.position.x > 0.9f);

    const auto departing = physics.MoveKinematicAABB(
        {1.25f, 0.0f, 0.0f}, {0.25f, 0.5f, 0.25f},
        {2.0f, 0.0f, 0.0f}, 0.1f);
    CHECK(departing.position.x > 1.4f);
}

void TestStaticBodyVelocityDoesNotInjectImpulse() {
    Archura::Scene scene("defensive-static");
    Archura::Entity* fixed = AddBox(scene, "fixed", {0.0f, 0.0f, 0.0f},
                                    {2.0f, 2.0f, 2.0f}, true);
    Archura::Entity* dynamic = AddBox(scene, "dynamic", {1.5f, 0.0f, 0.0f},
                                      {2.0f, 2.0f, 2.0f}, true);
    auto* fixedBody = fixed->GetComponent<Archura::RigidBody>();
    auto* dynamicBody = dynamic->GetComponent<Archura::RigidBody>();
    fixedBody->mass = 0.0f;
    fixedBody->velocity = {100.0f, 0.0f, 0.0f};
    dynamicBody->velocity = {0.0f, 0.0f, 0.0f};

    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    physics.Update(1.0f / 60.0f);
    CHECK(Near(dynamicBody->velocity.x, 0.0f));
}

void TestDestroyDuringCollisionCallback() {
    Archura::Scene scene("destroy-in-callback");
    Archura::Entity* trigger = AddBox(scene, "trigger", glm::vec3(0.0f),
                                      glm::vec3(2.0f));
    trigger->GetComponent<Archura::BoxCollider>()->isTrigger = true;
    AddBox(scene, "body", {0.5f, 0.0f, 0.0f}, glm::vec3(2.0f), true);
    const Archura::EntityHandle destroyed = trigger->GetHandle();
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    int exits = 0;
    physics.SetOnCollisionEnter([&](const Archura::CollisionEvent&) {
        CHECK(scene.DestroyEntity(destroyed));
    });
    physics.SetOnCollisionExit([&](const Archura::CollisionEvent& event) {
        ++exits;
        CHECK(event.entityA == destroyed || event.entityB == destroyed);
    });
    physics.Update(1.0f / 60.0f);
    CHECK(scene.GetEntity(destroyed) == nullptr);
    physics.Update(1.0f / 60.0f);
    CHECK(exits == 1);
}

void TestNonFiniteContinuousVelocityIsQuarantined() {
    Archura::Scene scene("non-finite-ccd");
    Archura::Entity* entity = AddBox(scene, "body", glm::vec3(0.0f),
                                     glm::vec3(1.0f), true);
    auto* body = entity->GetComponent<Archura::RigidBody>();
    body->continuous = true;
    body->velocity.x = std::numeric_limits<float>::infinity();
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    physics.Update(1.0f / 60.0f);
    CHECK(std::isfinite(body->velocity.x));
    CHECK(std::isfinite(entity->GetComponent<Archura::Transform>()->position.x));
}

void TestContinuousSubstepsDispatchOncePerFixedTick() {
    Archura::Scene scene("ccd-event-rate");
    Archura::Entity* trigger = AddBox(scene, "trigger", {0.0f, 0.0f, 0.0f},
                                      {20.0f, 20.0f, 20.0f});
    trigger->GetComponent<Archura::BoxCollider>()->isTrigger = true;
    Archura::Entity* bodyEntity = AddBox(scene, "continuous", glm::vec3(0.0f),
                                         glm::vec3(0.1f), true);
    auto* body = bodyEntity->GetComponent<Archura::RigidBody>();
    body->continuous = true;
    body->velocity = {10.0f, 0.0f, 0.0f};
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    int enters = 0;
    int stays = 0;
    physics.SetOnCollisionEnter([&](const Archura::CollisionEvent&) { ++enters; });
    physics.SetOnCollisionStay([&](const Archura::CollisionEvent&) { ++stays; });
    physics.Update(1.0f / 60.0f);
    CHECK(enters == 1);
    CHECK(stays == 0);
    physics.Update(1.0f / 60.0f);
    CHECK(stays == 1);
}

void TestDeterministicCanonicalPairOrder() {
    Archura::Scene scene("pair-order");
    std::vector<Archura::EntityHandle> handles;
    for (int i = 0; i < 3; ++i) {
        Archura::Entity* entity = AddBox(scene, "trigger", glm::vec3(0.0f),
                                         glm::vec3(2.0f));
        entity->GetComponent<Archura::BoxCollider>()->isTrigger = true;
        handles.push_back(entity->GetHandle());
    }
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> pairs;
    physics.SetOnCollisionEnter([&](const Archura::CollisionEvent& event) {
        pairs.emplace_back(event.entityA.Value(), event.entityB.Value());
    });
    physics.Update(1.0f / 60.0f);
    CHECK(pairs.size() == 3);
    CHECK(std::is_sorted(pairs.begin(), pairs.end()));
    for (const auto& pair : pairs) CHECK(pair.first < pair.second);
}

void TestSpiralBoundDropsExcessTime() {
    Archura::Scene scene("spiral-bound");
    Archura::Entity* entity = AddBox(scene, "body", glm::vec3(0.0f),
                                     glm::vec3(1.0f), true);
    auto* body = entity->GetComponent<Archura::RigidBody>();
    body->velocity = {1.0f, 0.0f, 0.0f};
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    physics.SetFixedTimeStep(0.01f);
    physics.SetMaxSubsteps(2);
    physics.Update(100.0f);
    CHECK(Near(entity->GetComponent<Archura::Transform>()->position.x, 0.02f));
    physics.Update(0.01f);
    CHECK(Near(entity->GetComponent<Archura::Transform>()->position.x, 0.03f));
}

void TestProjectileSweepFilteringAndNearestHit() {
    Archura::Scene scene("projectile-sweep");
    Archura::Entity* owner = AddBox(scene, "owner", {2.0f, 0.0f, 0.0f},
                                    {1.0f, 1.0f, 1.0f});
    Archura::Entity* trigger = AddBox(scene, "trigger", {3.0f, 0.0f, 0.0f},
                                      {0.1f, 2.0f, 2.0f});
    trigger->GetComponent<Archura::BoxCollider>()->isTrigger = true;
    Archura::Entity* nearest = AddBox(scene, "thin-nearest", {5.0f, 0.0f, 0.0f},
                                      {0.05f, 2.0f, 2.0f});
    AddBox(scene, "far", {8.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 2.0f});
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    Archura::PhysicsSystem::QueryFilter filter;
    filter.ignoredA = owner->GetHandle();
    Archura::PhysicsSystem::ShapeCastHit hit;
    CHECK(physics.SweepSphere({0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f},
                              0.1f, hit, filter));
    CHECK(hit.entity == nearest->GetHandle());
    CHECK(hit.distance > 4.8f && hit.distance < 5.0f);
    CHECK(hit.normal.x < -0.9f);
    filter.includeTriggers = true;
    CHECK(physics.SweepSphere({0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f},
                              0.1f, hit, filter));
    CHECK(hit.entity == trigger->GetHandle());
}
} // namespace

int main() {
    TestWorldBoundsAndQueries();
    TestRotatedNegativeScaledParentBounds();
    TestIntegrationAndImpulse();
    TestForceDragAndFixedStep();
    TestTriggersAndDestroyedExit();
    TestAuthoritativeCharacterSweep();
    TestRampShapeQueriesAndCharacterClimb();
    TestDynamicBodyResolvesAgainstRampSurface();
    TestCharacterInitialPenetrationAndSurfaceDeparture();
    TestStaticBodyVelocityDoesNotInjectImpulse();
    TestDestroyDuringCollisionCallback();
    TestNonFiniteContinuousVelocityIsQuarantined();
    TestContinuousSubstepsDispatchOncePerFixedTick();
    TestDeterministicCanonicalPairOrder();
    TestSpiralBoundDropsExcessTime();
    TestProjectileSweepFilteringAndNearestHit();
    if (gFailures != 0) {
        std::cerr << gFailures << " physics test(s) failed\n";
        return 1;
    }
    std::cout << "Physics core tests passed\n";
    return 0;
}
