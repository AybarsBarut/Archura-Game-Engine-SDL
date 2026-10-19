#include "ecs/Entity.h"
#include "game/PhysicsSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>

namespace {
int failures = 0;
constexpr float dt = 1.f / 128.f;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << ": " << #condition << '\n'; ++failures; \
} } while (false)

bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1.e-4f) {
    return glm::length(a-b) <= tolerance;
}

Archura::Entity* Ball(Archura::Scene& scene, float mass = 2.f,
                       const glm::vec3& position = {}) {
    auto* entity = scene.CreateEntity("pullable ball");
    entity->GetComponent<Archura::Transform>()->position = position;
    auto* collider = entity->AddComponent<Archura::BoxCollider>();
    collider->shape = Archura::BoxCollider::Shape::Sphere;
    collider->radius = .5f;
    auto* body = entity->AddComponent<Archura::RigidBody>();
    body->mass = mass;
    body->drag = body->angularDrag = 0;
    body->useGravity = false;
    body->continuous = true;
    return entity;
}

void TestEligibilityAndInvalidInput() {
    Archura::Scene scene("grab eligibility");
    auto* ball = Ball(scene);
    auto* body = ball->GetComponent<Archura::RigidBody>();
    auto* collider = ball->GetComponent<Archura::BoxCollider>();
    const auto handle = ball->GetHandle();
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    const auto reject = [&] {
        glm::vec3 center(99);
        const auto velocity = body->velocity;
        CHECK(!physics.GetPullableBodyCenter(handle,center));
        CHECK(center == glm::vec3(99));
        CHECK(!physics.PullBodyToward(handle,{4,0,0},dt));
        CHECK(body->velocity == velocity);
    };
    glm::vec3 center;
    CHECK(physics.GetPullableBodyCenter(handle,center));
    CHECK(center == glm::vec3(0));
    body->isKinematic = true; reject(); body->isKinematic = false;
    body->canBePushed = false; reject(); body->canBePushed = true;
    collider->isTrigger = true; reject(); collider->isTrigger = false;
    for (const float mass : {0.f,-1.f,std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
        body->mass = mass; reject();
    }
    body->mass = 2;
    auto* wall = scene.CreateEntity("static");
    wall->AddComponent<Archura::BoxCollider>();
    CHECK(!physics.GetPullableBodyCenter(wall->GetHandle(),center));
    CHECK(!physics.PullBodyToward(wall->GetHandle(),{4,0,0},dt));
    auto* noCollider = scene.CreateEntity("no collider");
    noCollider->AddComponent<Archura::RigidBody>();
    CHECK(!physics.GetPullableBodyCenter(noCollider->GetHandle(),center));
    CHECK(!physics.PullBodyToward(noCollider->GetHandle(),{4,0,0},dt));
    for (const float invalid : {0.f,-dt,.1f,std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::quiet_NaN()}) {
        CHECK(!physics.PullBodyToward(handle,{4,0,0},invalid));
        CHECK(body->velocity == glm::vec3(0));
    }
    for (int axis=0; axis<3; ++axis) {
        glm::vec3 target(0); target[axis] = std::numeric_limits<float>::quiet_NaN();
        CHECK(!physics.PullBodyToward(handle,target,dt));
        target[axis] = std::numeric_limits<float>::infinity();
        CHECK(!physics.PullBodyToward(handle,target,dt));
        CHECK(body->velocity == glm::vec3(0));
    }
    physics.SetSimulating(false);
    CHECK(!physics.PullBodyToward(handle,{4,0,0},dt));
    CHECK(body->velocity == glm::vec3(0));
    physics.SetSimulating(true);
    CHECK(physics.PullBodyToward(handle,{4,0,0},dt));
}

void TestImpulseNonmutationAndMassBounds() {
    float lightResponse = 0;
    for (const float mass : {.0001f,2.f,1000.f}) {
        Archura::Scene scene("mass limited pull");
        auto* ball = Ball(scene,mass);
        auto* body = ball->GetComponent<Archura::RigidBody>();
        body->angularVelocity = {0,1,0};
        auto* transform = ball->GetComponent<Archura::Transform>();
        std::array<unsigned char,sizeof(Archura::Transform)> before{};
        std::memcpy(before.data(),transform,sizeof(*transform));
        Archura::PhysicsSystem physics; physics.Init(&scene);
        CHECK(physics.PullBodyToward(ball->GetHandle(),{100,0,0},dt));
        CHECK(std::memcmp(before.data(),transform,sizeof(*transform)) == 0);
        CHECK(body->angularVelocity == glm::vec3(0,1,0)); // center impulse adds no spin
        CHECK(body->mass == mass && !body->useGravity && body->continuous);
        CHECK(body->canBePushed && !body->isKinematic && body->drag == 0);
        CHECK(body->force == glm::vec3(0));
        CHECK(body->velocity.x > 0 && body->velocity.x <= 80*dt+1.e-5f);
        CHECK(body->velocity.x*mass <= 500*dt+1.e-4f);
        if (mass == 2) lightResponse = body->velocity.x;
        if (mass == 1000) CHECK(body->velocity.x < lightResponse*.02f);
        for (int i=0; i<128; ++i) {
            CHECK(physics.PullBodyToward(ball->GetHandle(),{100,0,0},dt));
            physics.Update(dt);
            CHECK(std::isfinite(body->velocity.x));
            CHECK(body->velocity.x <= 8.001f);
        }
        CHECK(ball->GetComponent<Archura::Transform>()->position.x > 0);
    }
}

void TestHoldAndRelease() {
    Archura::Scene scene("gravity hold");
    auto* ball = Ball(scene,2,{0,1,0});
    auto* body = ball->GetComponent<Archura::RigidBody>();
    body->useGravity = true;
    Archura::PhysicsSystem physics; physics.Init(&scene);
    const glm::vec3 target(2,3,-1);
    for (int i=0; i<768; ++i) {
        CHECK(physics.PullBodyToward(ball->GetHandle(),target,dt));
        physics.Update(dt);
    }
    glm::vec3 center;
    CHECK(physics.GetPullableBodyCenter(ball->GetHandle(),center));
    CHECK(Near(center,target,.08f));
    CHECK(glm::length(body->velocity) < .3f);
    CHECK(body->useGravity && body->mass == 2 && !body->isKinematic);
    const float heldHeight = center.y;
    // Releasing consists of stopping pull requests: no hidden constraint remains.
    for (int i=0; i<64; ++i) physics.Update(dt);
    CHECK(ball->GetWorldTransform()[3].y < heldHeight-.8f);
    CHECK(body->velocity.y < -4.f);

    body->useGravity = false;
    body->velocity = {2,0,0};
    const float start = ball->GetWorldTransform()[3].x;
    for (int i=0; i<64; ++i) physics.Update(dt);
    CHECK(std::abs(ball->GetWorldTransform()[3].x-start-1.f) < .04f);
    CHECK(body->velocity == glm::vec3(2,0,0));

    auto* heavy = Ball(scene,1000,{10,3,0});
    heavy->GetComponent<Archura::RigidBody>()->useGravity = true;
    for (int i=0; i<128; ++i) {
        CHECK(physics.PullBodyToward(heavy->GetHandle(),{10,3,0},dt));
        physics.Update(dt);
    }
    CHECK(heavy->GetWorldTransform()[3].y < 0); // 500N cannot suspend 1000kg
}

void TestWallRemainsAuthoritative() {
    Archura::Scene scene("pull into solid wall");
    auto* ball = Ball(scene);
    auto* wall = scene.CreateEntity("wall");
    wall->GetComponent<Archura::Transform>()->position = {2,0,0};
    wall->AddComponent<Archura::BoxCollider>()->size = {.2f,10,10};
    Archura::PhysicsSystem physics; physics.Init(&scene);
    int contacts = 0;
    physics.SetOnCollisionEnter([&](const Archura::CollisionEvent&){ ++contacts; });
    float maximum = 0;
    for (int i=0; i<512; ++i) {
        CHECK(physics.PullBodyToward(ball->GetHandle(),{5,0,0},dt));
        physics.Update(dt);
        maximum = std::max(maximum,ball->GetWorldTransform()[3].x);
    }
    CHECK(contacts > 0);
    CHECK(maximum > 1.3f && maximum < 1.43f); // wall near face1.9 minus radius.5
    CHECK(wall->GetWorldTransform()[3].x == 2);
}

void TestParentCentersAndRuntimeLifetime() {
    for (const auto shape : {Archura::BoxCollider::Shape::Sphere,
                             Archura::BoxCollider::Shape::Box}) {
        Archura::Scene scene("parent and collider offset");
        auto* parent = scene.CreateEntity("parent");
        auto* p = parent->GetComponent<Archura::Transform>();
        p->position = {10,2,-3}; p->rotation = {0,0,90}; p->scale = {2,3,1};
        auto* ball = Ball(scene,2,{1,0,0});
        auto* collider = ball->GetComponent<Archura::BoxCollider>();
        collider->shape = shape; collider->center = {.5f,.25f,0};
        CHECK(ball->TrySetParent(parent));
        const auto expected = glm::vec3(ball->GetWorldTransform()*glm::vec4(collider->center,1));
        Archura::PhysicsSystem physics; physics.Init(&scene);
        glm::vec3 center;
        CHECK(physics.GetPullableBodyCenter(ball->GetHandle(),center));
        CHECK(Near(center,expected));
        CHECK(physics.PullBodyToward(ball->GetHandle(),center+glm::vec3(1,0,0),dt));
        CHECK(ball->GetComponent<Archura::RigidBody>()->velocity.x > 0);
        CHECK(ball->GetComponent<Archura::RigidBody>()->angularVelocity == glm::vec3(0));
        physics.ResetRuntimeState();
        CHECK(physics.GetPullableBodyCenter(ball->GetHandle(),center));
        const auto stale = ball->GetHandle();
        CHECK(scene.DestroyEntity(stale));
        auto* replacement = Ball(scene);
        const auto velocity = replacement->GetComponent<Archura::RigidBody>()->velocity;
        CHECK(!physics.GetPullableBodyCenter(stale,center));
        CHECK(!physics.PullBodyToward(stale,{10,0,0},dt));
        CHECK(replacement->GetComponent<Archura::RigidBody>()->velocity == velocity);
        physics.Shutdown();
        CHECK(!physics.GetPullableBodyCenter(replacement->GetHandle(),center));
        CHECK(!physics.PullBodyToward(replacement->GetHandle(),{10,0,0},dt));
    }
    // Reusing exactly the Scene address and initial handle cannot revive the
    // old scene binding. The weak lifetime token must guard both public APIs.
    alignas(Archura::Scene) unsigned char storage[sizeof(Archura::Scene)];
    auto* first = new (storage) Archura::Scene("old scene");
    const auto handle = Ball(*first)->GetHandle();
    Archura::PhysicsSystem physics; physics.Init(first);
    first->~Scene();
    auto* second = new (storage) Archura::Scene("new scene at same address");
    auto* replacement = Ball(*second);
    glm::vec3 center(99);
    CHECK(!physics.GetPullableBodyCenter(handle,center));
    CHECK(center == glm::vec3(99));
    CHECK(!physics.PullBodyToward(handle,{10,0,0},dt));
    CHECK(replacement->GetComponent<Archura::RigidBody>()->velocity == glm::vec3(0));
    physics.Init(second);
    CHECK(physics.GetPullableBodyCenter(replacement->GetHandle(),center));
    second->~Scene();
}
} // namespace

int main() {
    TestEligibilityAndInvalidInput();
    TestImpulseNonmutationAndMassBounds();
    TestHoldAndRelease();
    TestWallRemainsAuthoritative();
    TestParentCentersAndRuntimeLifetime();
    if (failures) { std::cerr << failures << " grab regression failure(s)\n"; return 1; }
    std::cout << "Physics pull eligibility, mass bounds, hold/release and collision tests passed\n";
}
