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

void TestRepeatedWalkIntoRampEntrance() {
    Archura::Scene scene("ramp-entrance-walk");
    AddRamp(scene, "ramp", glm::vec3(0), {4, 2, 4});
    AddBox(scene, "floor", {0, -.5f, 0}, {30, 1, 30});
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    glm::vec3 position(0, .901f, 3);
    float vertical = 0;
    for (int tick = 0; tick < 256; ++tick) {
        vertical -= 9.81f/128;
        const auto move = physics.MoveKinematicAABB(position, {.3f, .9f, .3f}, {0, vertical, -4}, 1.f/128);
        position = move.position; vertical = move.velocity.y;
    }
    CHECK(position.z < -2.5f);
}

void TestRampEntranceAcrossSpeedsAndRotations() {
    for (float yaw : {0.f, 30.f, 90.f, 180.f, 270.f})
    for (float speed : {.5f, 2.f, 4.f, 8.f})
    for (float height : {.9f, .901f}) {
        Archura::Scene scene("ramp-entrance-variants");
        auto* ramp = AddRamp(scene, "ramp", glm::vec3(0), {4, 2, 4});
        ramp->GetComponent<Archura::Transform>()->rotation.y = yaw;
        AddBox(scene, "floor", {0, -.5f, 0}, {30, 1, 30});
        Archura::PhysicsSystem physics; physics.Init(&scene);
        const float angle = yaw*3.14159265f/180;
        const glm::vec3 outward(std::sin(angle), 0, std::cos(angle));
        glm::vec3 position = outward*3.f + glm::vec3(0,height,0);
        float vertical = 0;
        bool grounded = false;
        for (int tick = 0; tick < static_cast<int>(1280/speed); ++tick) {
            if (!grounded) vertical -= 9.81f/128;
            const auto move = physics.MoveKinematicAABB(position, {.3f,.9f,.3f},
                -outward*speed+glm::vec3(0,vertical,0), 1.f/128);
            position = move.position; vertical = move.velocity.y; grounded = move.grounded;
        }
        if (glm::dot(position,outward) >= -2.5f)
            std::cerr << "Stuck yaw=" << yaw << " speed=" << speed << " height=" << height
                      << " final=" << position.x << ',' << position.y << ',' << position.z << '\n';
        CHECK(glm::dot(position,outward) < -2.5f);
    }
}

void TestRampDepartureWallsAndCeiling() {
    Archura::Scene scene("ramp-boundaries");
    AddRamp(scene, "ramp", glm::vec3(0), {4, 2, 4});
    AddBox(scene, "floor", {0,-.5f,0}, {30,1,30});
    Archura::PhysicsSystem physics; physics.Init(&scene);
    const glm::vec3 extent(.3f,.9f,.3f);
    const auto back = physics.MoveKinematicAABB({0,.901f,-3}, extent, {0,0,4}, 1);
    CHECK(back.position.z <= -2.299f);
    const auto side = physics.MoveKinematicAABB({3,.901f,-1}, extent, {-4,0,0}, 1);
    CHECK(side.position.x >= 2.299f);
    // Start exactly on the expanded slope: tangent and separating motions must
    // not report the low-end face behind the character as a zero-time wall.
    const auto tangent = physics.MoveKinematicAABB({0,2.05f,0}, extent, {0,1,-2}, .1f);
    CHECK(tangent.position.z < -.15f);
    const auto jump = physics.MoveKinematicAABB({0,2.05f,0}, extent, {0,4,0}, .1f);
    CHECK(jump.position.y > 2.4f);

    glm::vec3 position(0,2.801f,-1.5f);
    float vertical = 0;
    bool grounded = false;
    for (int i = 0; i < 192; ++i) {
        if (!grounded) vertical -= 9.81f/128;
        const auto move = physics.MoveKinematicAABB(position, extent, {0,vertical,4}, 1.f/128);
        position = move.position; vertical = move.velocity.y; grounded = move.grounded;
    }
    CHECK(position.z > 3.5f);
    CHECK(position.y >= .899f && position.y < 1.2f);

    AddBox(scene, "ceiling", {0,2.6f,0}, {6,1,6});
    position = {0,.901f,3}; vertical = 0; grounded = false;
    bool hitCeiling = false;
    for (int i = 0; i < 128; ++i) {
        if (!grounded) vertical -= 9.81f/128;
        const auto move = physics.MoveKinematicAABB(position, extent, {0,vertical,-4}, 1.f/128);
        position = move.position; vertical = move.velocity.y; grounded = move.grounded;
        hitCeiling |= move.hitCeiling;
        CHECK(position.y <= 1.202f);
    }
    CHECK(hitCeiling);
    CHECK(position.z > 1.65f);
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

Archura::Entity* AddSphere(Archura::Scene& scene, const char* name,
                          glm::vec3 position, float radius, bool dynamic = false) {
    auto* entity = AddBox(scene, name, position, glm::vec3(2 * radius), dynamic);
    auto* collider = entity->GetComponent<Archura::BoxCollider>();
    collider->shape = Archura::BoxCollider::Shape::Sphere;
    collider->radius = radius;
    return entity;
}

void Advance(Archura::PhysicsSystem& physics, int ticks) {
    for (int i = 0; i < ticks; ++i) physics.Update(1.f / 60);
}

void TestSphereQueriesAndRoundedCharacterCorners() {
    Archura::Scene scene("sphere-queries");
    auto* sphere = AddSphere(scene, "sphere", {3, 0, 0}, 1);
    Archura::PhysicsSystem physics; physics.Init(&scene);
    Archura::Entity* hit = nullptr;
    glm::vec3 point(0);
    CHECK(physics.Raycast({0, .6f, 0}, {1, 0, 0}, 10, &hit, &point));
    CHECK(hit == sphere);
    CHECK(Near(point.x, 2.2f));
    CHECK(!physics.Raycast({0, .8f, .8f}, {1, 0, 0}, 10, &hit, &point));
    CHECK(hit == nullptr);
    CHECK(physics.RaycastSphere({0, 0, 0}, {1, 0, 0}, 10, .2f, &hit, &point));
    CHECK(Near(point.x, 1.8f));
    CHECK(!physics.RaycastSphere({0, .9f, .9f}, {1, 0, 0}, 10, .2f, &hit, &point));
    Archura::PhysicsSystem::ShapeCastHit cast;
    CHECK(physics.SweepSphere({0, .6f, 0}, {10, 0, 0}, .2f, cast));
    CHECK(cast.entity == sphere->GetHandle());
    CHECK(Near(cast.distance, 3 - std::sqrt(1.44f - .36f)));
    CHECK(cast.normal.x < -.8f && cast.normal.y > .4f);
    // This path crosses the sphere's bounding cube but misses its rounded edge.
    const auto corner = physics.MoveKinematicAABB({0, 1.05f, 1.05f}, glm::vec3(.1f), {6, 0, 0}, 1);
    CHECK(Near(corner.position.x, 6));
    const auto blocked = physics.MoveKinematicAABB({0, 0, 0}, glm::vec3(.1f), {6, 0, 0}, 1);
    CHECK(blocked.position.x > 1.89f && blocked.position.x < 1.91f);
}

void TestSphereBoxAndSphereSphereRoundedContacts() {
    {
        Archura::Scene scene("sphere-box-corner");
        AddSphere(scene, "sphere", {0, 0, 0}, 1);
        auto* box = AddBox(scene, "corner", {.9f, .9f, 0}, glm::vec3(.2f), true);
        Archura::PhysicsSystem physics; physics.Init(&scene);
        int contacts = 0;
        physics.SetOnCollisionEnter([&](const Archura::CollisionEvent&) { ++contacts; });
        physics.Update(1.f / 60);
        CHECK(contacts == 0);
        CHECK(box->GetComponent<Archura::Transform>()->position == glm::vec3(.9f, .9f, 0));
        box->GetComponent<Archura::Transform>()->position = {.75f, 0, 0};
        physics.Update(1.f / 60);
        CHECK(contacts == 1);
        CHECK(box->GetComponent<Archura::Transform>()->position.x > .9f);
    }
    {
        Archura::Scene scene("sphere-sphere-corner");
        AddSphere(scene, "fixed", {0, 0, 0}, 1);
        auto* sphere = AddSphere(scene, "diagonal", {1.5f, 1.5f, 0}, 1, true);
        Archura::PhysicsSystem physics; physics.Init(&scene);
        physics.Update(1.f / 60);
        CHECK(physics.GetActiveContactCount() == 0);
        CHECK(sphere->GetComponent<Archura::Transform>()->position == glm::vec3(1.5f, 1.5f, 0));
    }
}

void TestSphereMassMomentumAndBounce() {
    Archura::Scene scene("sphere-momentum");
    auto* left = AddSphere(scene, "light", {-.49f, 0, 0}, .5f, true);
    auto* right = AddSphere(scene, "heavy", {.49f, 0, 0}, .5f, true);
    auto* a = left->GetComponent<Archura::RigidBody>();
    auto* b = right->GetComponent<Archura::RigidBody>();
    a->mass = 1; b->mass = 3;
    a->velocity.x = 2; b->velocity.x = 0;
    a->friction = b->friction = 0;
    a->restitution = b->restitution = 1;
    Archura::PhysicsSystem physics; physics.Init(&scene);
    physics.Update(1.f / 60);
    CHECK(a->velocity.x < -.9f);
    CHECK(b->velocity.x > .9f);
    CHECK(Near(a->mass * a->velocity.x + b->mass * b->velocity.x, 2, .01f));
    CHECK(Near(a->mass * a->velocity.x * a->velocity.x +
               b->mass * b->velocity.x * b->velocity.x, 4, .03f));
}

void TestSphereGravityToggleRestAndMaterialBounce() {
    {
        Archura::Scene scene("sphere-gravity");
        AddBox(scene, "floor", {0, -.5f, 0}, {30, 1, 30});
        auto* sphere = AddSphere(scene, "ball", {0, 3, 0}, .5f, true);
        auto* body = sphere->GetComponent<Archura::RigidBody>();
        auto* transform = sphere->GetComponent<Archura::Transform>();
        Archura::PhysicsSystem physics; physics.Init(&scene);
        Advance(physics, 30);
        CHECK(Near(transform->position.y, 3));
        body->useGravity = true;
        Advance(physics, 20);
        CHECK(transform->position.y < 2.7f);
        CHECK(body->velocity.y < -2);
        Advance(physics, 160);
        CHECK(transform->position.y > .48f && transform->position.y < .52f);
        CHECK(std::abs(body->velocity.y) < .2f);
    }
    for (bool bouncyFloor : {false, true}) {
        Archura::Scene scene("sphere-bounce-material");
        auto* floor = AddBox(scene, "floor", {0, -.5f, 0}, {30, 1, 30}, bouncyFloor);
        if (bouncyFloor) {
            auto* body = floor->GetComponent<Archura::RigidBody>();
            body->isKinematic = true;
            body->restitution = .8f;
        }
        auto* ball = AddSphere(scene, "ball", {0, .51f, 0}, .5f, true);
        auto* body = ball->GetComponent<Archura::RigidBody>();
        body->restitution = bouncyFloor ? 0 : .8f;
        body->velocity.y = -3;
        Archura::PhysicsSystem physics; physics.Init(&scene);
        physics.Update(1.f / 60);
        CHECK(body->velocity.y > 2.3f);
    }
}

void TestSphereRollingAndRampDescent() {
    {
        Archura::Scene scene("sphere-rolling");
        AddBox(scene, "floor", {0, -.5f, 0}, {30, 1, 30});
        auto* sphere = AddSphere(scene, "rolling", {0, .5f, 0}, .5f, true);
        auto* body = sphere->GetComponent<Archura::RigidBody>();
        body->useGravity = true;
        body->velocity.x = 3;
        body->angularDrag = 0;
        Archura::PhysicsSystem physics; physics.Init(&scene);
        Advance(physics, 60);
        CHECK(sphere->GetComponent<Archura::Transform>()->position.x > 1);
        CHECK(body->angularVelocity.z < -1);
        CHECK(glm::length(sphere->GetComponent<Archura::Transform>()->rotation) > 1);
        // With no contact friction, sliding does not invent a rolling spin.
        auto* slider = AddSphere(scene, "frictionless", {0, .5f, 3}, .5f, true);
        auto* sliding = slider->GetComponent<Archura::RigidBody>();
        sliding->useGravity = true; sliding->friction = 0; sliding->velocity.x = 3;
        Advance(physics, 30);
        CHECK(glm::length(sliding->angularVelocity) < .001f);
    }
    {
        Archura::Scene scene("sphere-ramp-descent");
        AddRamp(scene, "ramp", {0, 0, 0}, {4, 2, 8});
        AddBox(scene, "floor", {0, -.5f, 0}, {30, 1, 30});
        auto* sphere = AddSphere(scene, "ball", {0, 2.2f, -2}, .5f, true);
        auto* body = sphere->GetComponent<Archura::RigidBody>();
        body->useGravity = true; body->continuous = true;
        Archura::PhysicsSystem physics; physics.Init(&scene);
        Advance(physics, 180);
        const auto position = sphere->GetComponent<Archura::Transform>()->position;
        CHECK(position.z > 1);
        CHECK(position.y > .45f);
        CHECK(glm::length(body->angularVelocity) > .1f);
        CHECK(std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z));
    }
}

void TestSphereScaledParentAndCenterPolicy() {
    Archura::Scene scene("sphere-world-scale");
    auto* parent = scene.CreateEntity("parent");
    auto* transform = parent->GetComponent<Archura::Transform>();
    transform->position = {10, 0, 0};
    transform->rotation.z = 90;
    transform->scale = {-2, 3, 1};
    auto* sphere = AddSphere(scene, "child", {1, 0, 0}, .5f);
    sphere->GetComponent<Archura::BoxCollider>()->center = {.5f, 0, 0};
    CHECK(sphere->TrySetParent(parent));
    // Transformed center (10,-3,0), uniform world radius .5 * max(2,3,1).
    Archura::PhysicsSystem physics; physics.Init(&scene);
    Archura::Entity* hit = nullptr; glm::vec3 point;
    CHECK(physics.Raycast({0, -3, 0}, {1, 0, 0}, 20, &hit, &point));
    CHECK(hit == sphere && Near(point.x, 8.5f));
    CHECK(physics.Raycast({10, -3, -5}, {0, 0, 1}, 20, &hit, &point));
    CHECK(Near(point.z, -1.5f)); // remains a sphere, not a scaled ellipsoid
}

void TestCharacterPushMassAndOptOut() {
    auto pushVelocity = [](float mass, bool push, bool enabled, bool kinematic) {
        Archura::Scene scene("character-push");
        auto* sphere = AddSphere(scene, "ball", {2, 0, 0}, .5f, true);
        auto* body = sphere->GetComponent<Archura::RigidBody>();
        body->mass = mass; body->canBePushed = enabled; body->isKinematic = kinematic;
        Archura::PhysicsSystem physics; physics.Init(&scene);
        const auto result = physics.MoveKinematicAABB({0, 0, 0}, {.3f, .9f, .3f}, {4, 0, 0}, 1, {}, push, 80);
        CHECK(result.position.x < 1.21f); // contacts never teleport either participant
        CHECK(Near(sphere->GetComponent<Archura::Transform>()->position.x, 2));
        return body->velocity.x;
    };
    const float light = pushVelocity(1, true, true, false);
    const float heavy = pushVelocity(800, true, true, false);
    CHECK(light > 3 && light <= 4);
    CHECK(heavy > 0 && heavy < light * .2f);
    CHECK(Near(pushVelocity(1, false, true, false), 0));
    CHECK(Near(pushVelocity(1, true, false, false), 0));
    CHECK(Near(pushVelocity(1, true, true, true), 0));
    CHECK(Near(pushVelocity(0, true, true, false), 0));
    {
        Archura::Scene boxes("push-box-and-static");
        auto* movable = AddBox(boxes, "movable", {2, 0, 0}, glm::vec3(1), true);
        auto* fixed = AddBox(boxes, "static", {2, 0, 3}, glm::vec3(1));
        Archura::PhysicsSystem boxPhysics; boxPhysics.Init(&boxes);
        const auto pushed = boxPhysics.MoveKinematicAABB({0, 0, 0}, {.3f, .9f, .3f}, {4, 0, 0}, 1, {}, true);
        const auto blocked = boxPhysics.MoveKinematicAABB({0, 0, 3}, {.3f, .9f, .3f}, {4, 0, 0}, 1, {}, true);
        CHECK(pushed.position.x < 1.21f && blocked.position.x < 1.21f);
        CHECK(movable->GetComponent<Archura::RigidBody>()->velocity.x > 3);
        CHECK(fixed->GetComponent<Archura::Transform>()->position == glm::vec3(2, 0, 3));
        boxPhysics.Update(1.f / 60);
        CHECK(movable->GetComponent<Archura::Transform>()->position.x > 2.04f);
    }
    Archura::Scene scene("push-ball-wall");
    AddBox(scene, "wall", {3, 0, 0}, {.2f, 4, 4});
    auto* ball = AddSphere(scene, "ball", {2, 0, 0}, .5f, true);
    ball->GetComponent<Archura::RigidBody>()->continuous = true;
    Archura::PhysicsSystem physics; physics.Init(&scene);
    glm::vec3 character(0);
    for (int i = 0; i < 120; ++i) {
        character = physics.MoveKinematicAABB(character, {.3f, .9f, .3f}, {6, 0, 0}, 1.f / 60, {}, true).position;
        physics.Update(1.f / 60);
        CHECK(ball->GetComponent<Archura::Transform>()->position.x < 2.43f);
        CHECK(character.x < 1.66f);
    }
    CHECK(ball->GetComponent<Archura::Transform>()->position.x > 2.3f);
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
    TestRepeatedWalkIntoRampEntrance();
    TestRampEntranceAcrossSpeedsAndRotations();
    TestRampDepartureWallsAndCeiling();
    TestCharacterInitialPenetrationAndSurfaceDeparture();
    TestStaticBodyVelocityDoesNotInjectImpulse();
    TestDestroyDuringCollisionCallback();
    TestNonFiniteContinuousVelocityIsQuarantined();
    TestContinuousSubstepsDispatchOncePerFixedTick();
    TestDeterministicCanonicalPairOrder();
    TestSpiralBoundDropsExcessTime();
    TestProjectileSweepFilteringAndNearestHit();
    TestSphereQueriesAndRoundedCharacterCorners();
    TestSphereBoxAndSphereSphereRoundedContacts();
    TestSphereMassMomentumAndBounce();
    TestSphereGravityToggleRestAndMaterialBounce();
    TestSphereRollingAndRampDescent();
    TestSphereScaledParentAndCenterPolicy();
    TestCharacterPushMassAndOptOut();
    if (gFailures != 0) {
        std::cerr << gFailures << " physics test(s) failed\n";
        return 1;
    }
    std::cout << "Physics core tests passed\n";
    return 0;
}
