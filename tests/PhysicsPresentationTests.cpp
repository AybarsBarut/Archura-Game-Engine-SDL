#include "ecs/Entity.h"
#include "game/PhysicsSystem.h"
#include "core/GameLoopHarness.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << ": " << #condition << '\n'; ++failures; \
} } while (false)

bool Near(float a, float b, float tolerance = 2.e-4f) {
    return std::isfinite(a) && std::isfinite(b) && std::abs(a-b) <= tolerance;
}
bool Near(const glm::mat4& a, const glm::mat4& b, float tolerance = 2.e-4f) {
    for (int c=0; c<4; ++c) for (int r=0; r<4; ++r)
        if (!Near(a[c][r],b[c][r],tolerance)) return false;
    return true;
}

Archura::Entity* Ball(Archura::Scene& scene, const glm::vec3& position = {}) {
    auto* entity = scene.CreateEntity("presentation ball");
    entity->GetComponent<Archura::Transform>()->position = position;
    auto* collider = entity->AddComponent<Archura::BoxCollider>();
    collider->shape = Archura::BoxCollider::Shape::Sphere;
    collider->radius = .5f;
    auto* body = entity->AddComponent<Archura::RigidBody>();
    body->useGravity = false;
    body->drag = body->angularDrag = 0;
    return entity;
}

void TestVariableRenderRates() {
    // Application advances at 128 Hz, Physics at 60 Hz, presentation at the
    // requested display rate. The trajectory is analytic, independent of the
    // interpolator's implementation, and includes rotation across Euler wraps.
    constexpr double appStep = 1. / 128.;
    glm::mat4 commonAuthority(1);
    glm::mat4 commonPresentation(1);
    bool firstRate = true;
    for (const int fps : {30,60,120,144,240}) {
        Archura::Scene scene("variable render rate");
        auto* ball = Ball(scene);
        auto* transform = ball->GetComponent<Archura::Transform>();
        auto* body = ball->GetComponent<Archura::RigidBody>();
        body->velocity = {3,0,0};
        body->angularVelocity = {0,2,0};
        Archura::PhysicsSystem physics;
        physics.Init(&scene);
        const double physicsStep = physics.GetFixedTimeStep();
        double applicationTime = 0;
        Archura::GameLoopHarness loop;
        Archura::GameLoopHarness::Callbacks callbacks;
        callbacks.fixedTick = [&](double dt) { physics.Update(static_cast<float>(dt)); };
        int interpolatedFrames = 0;
        for (int frame=1; frame<=fps*2; ++frame) {
            const double renderTime = double(frame)/fps;
            loop.RunFrame(1.0 / fps, callbacks);
            applicationTime = loop.TotalTicks() * appStep;
            CHECK(loop.DroppedTicks() == 0);
            std::array<unsigned char,sizeof(Archura::Transform)> transformBytes{};
            std::array<unsigned char,sizeof(Archura::RigidBody)> bodyBytes{};
            std::memcpy(transformBytes.data(),transform,sizeof(*transform));
            std::memcpy(bodyBytes.data(),body,sizeof(*body));
            const auto authority = ball->GetWorldTransform();
            const auto view = physics.GetRenderWorldTransform(*ball,float(loop.AccumulatorSeconds()));
            CHECK(std::memcmp(transformBytes.data(),transform,sizeof(*transform))==0);
            CHECK(std::memcmp(bodyBytes.data(),body,sizeof(*body))==0);
            CHECK(Near(authority,ball->GetWorldTransform(),0));
            const auto steps = std::floor((applicationTime+1.e-10)/physicsStep);
            const double simulatedTime = steps*physicsStep;
            CHECK(Near(authority[3].x,float(3*simulatedTime),5.e-4f));
            if (steps>0) {
                // A physics boundary can fall inside the unconsumed app
                // remainder. No extrapolation is allowed past current pose.
                const auto sampleTime = std::clamp(renderTime-physicsStep,0.,simulatedTime);
                const auto expected = glm::rotate(
                    glm::translate(glm::mat4(1),glm::vec3(float(3*sampleTime),0,0)),
                    float(2*sampleTime),glm::vec3(0,1,0));
                CHECK(Near(view,expected,7.e-4f));
                CHECK(view[3].x <= authority[3].x + 1.e-5f);
                if (view[3].x > float(3*(simulatedTime-physicsStep))+1.e-5f &&
                    view[3].x < authority[3].x-1.e-5f) ++interpolatedFrames;
            }
            if (frame==fps*2) {
                if (firstRate) { commonAuthority=authority; commonPresentation=view; }
                else { CHECK(Near(authority,commonAuthority)); CHECK(Near(view,commonPresentation)); }
            }
        }
        // At 30/60 Hz samples coincide with physics boundaries; faster displays
        // must visibly sample intermediate poses rather than repeat raw poses.
        if (fps>60) CHECK(interpolatedFrames > fps/2);
        firstRate = false;
    }
}

void TestHierarchyAndExternalEdits() {
    Archura::Scene scene("attached decal");
    auto* ball = Ball(scene);
    ball->GetComponent<Archura::RigidBody>()->velocity = {6,0,0};
    auto* decal = scene.CreateEntity("surface mark");
    auto* local = decal->GetComponent<Archura::Transform>();
    local->position = {.2f,.3f,.4f};
    CHECK(decal->TrySetParent(ball));
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    physics.Update(physics.GetFixedTimeStep()*1.5f);
    const auto view = physics.GetRenderWorldTransform(*ball);
    CHECK(view[3].x > 0 && view[3].x < ball->GetWorldTransform()[3].x);
    CHECK(Near(physics.GetRenderWorldTransform(*decal),view*local->GetModelMatrix()));
    CHECK(Near(decal->GetWorldTransform(),ball->GetWorldTransform()*local->GetModelMatrix()));

    // Teleports, scale edits and reparenting must never fly through the scene.
    auto* transform = ball->GetComponent<Archura::Transform>();
    transform->position = {100,2,3};
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    CHECK(Near(physics.GetRenderWorldTransform(*decal),decal->GetWorldTransform()));
    physics.Update(physics.GetFixedTimeStep());
    transform->scale = {2,3,4};
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    auto* parent = scene.CreateEntity("new parent");
    CHECK(ball->TrySetParent(parent));
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    physics.Update(physics.GetFixedTimeStep());
    parent->GetComponent<Archura::Transform>()->position.x = -40;
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
}

void TestLifetimeModesAndReset() {
    Archura::Scene scene("lifetime");
    auto* ball = Ball(scene,{10,0,0});
    auto* body = ball->GetComponent<Archura::RigidBody>();
    body->velocity = {6,0,0};
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    physics.Update(physics.GetFixedTimeStep()*1.5f);
    body->isKinematic = true;
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    body->isKinematic = false;
    body->mass = 0;
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    body->mass = 1;
    physics.ResetRuntimeState();
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    physics.Update(physics.GetFixedTimeStep()*1.5f);
    physics.SetFixedTimeStep(1.f/120);
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    physics.Update(physics.GetFixedTimeStep()*1.5f);
    const auto removed = ball->GetHandle();
    CHECK(scene.DestroyEntity(removed));
    ball = Ball(scene,{-50,0,0});
    CHECK(ball->GetHandle()!=removed);
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    physics.Update(physics.GetFixedTimeStep()*1.5f);
    physics.Shutdown();
    CHECK(Near(physics.GetRenderWorldTransform(*ball),ball->GetWorldTransform()));
    // A distinct scene can reuse the numeric ID; Init must clear old history.
    Archura::Scene other("replacement");
    auto* replacement = Ball(other,{200,0,0});
    physics.Init(&other);
    CHECK(Near(physics.GetRenderWorldTransform(*replacement),replacement->GetWorldTransform()));
}

void TestStallAndRenderInputBounds() {
    Archura::Scene scene("stall");
    auto* ball = Ball(scene);
    ball->GetComponent<Archura::RigidBody>()->velocity = {3,0,0};
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    physics.SetMaxSubsteps(2);
    physics.Update(10); // bounded catch-up drops excess elapsed time
    const float current = ball->GetWorldTransform()[3].x;
    CHECK(current>0 && current<.11f);
    const auto before = ball->GetWorldTransform();
    for (const float additional : {-10.f,0.f,10.f,
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
        const auto view = physics.GetRenderWorldTransform(*ball,additional);
        CHECK(std::isfinite(view[3].x));
        CHECK(view[3].x >= current - 3*physics.GetFixedTimeStep()-1.e-5f);
        CHECK(view[3].x <= current+1.e-5f);
        CHECK(Near(before,ball->GetWorldTransform(),0));
    }
}

void TestPointImpulses() {
    Archura::Scene scene("bullet impulse");
    auto* ball = Ball(scene);
    auto* body = ball->GetComponent<Archura::RigidBody>();
    body->mass = 2;
    Archura::PhysicsSystem physics;
    physics.Init(&scene);
    CHECK(physics.ApplyImpulse(ball->GetHandle(),{4,0,0},{0,.5f,0}));
    CHECK(Near(body->velocity.x,2));
    CHECK(Near(body->angularVelocity.z,-10)); // I=.4*m*r^2=.2, torque=-2
    CHECK(ball->GetComponent<Archura::Transform>()->position==glm::vec3(0));
    const auto velocity = body->velocity;
    const auto angular = body->angularVelocity;
    CHECK(!physics.ApplyImpulse(ball->GetHandle(),{std::numeric_limits<float>::quiet_NaN(),0,0},{0,0,0}));
    CHECK(body->velocity==velocity && body->angularVelocity==angular);
    body->isKinematic = true;
    CHECK(!physics.ApplyImpulse(ball->GetHandle(),{4,0,0},{0,0,0}));
    body->isKinematic = false;
    body->mass = 0;
    CHECK(!physics.ApplyImpulse(ball->GetHandle(),{4,0,0},{0,0,0}));
    auto* wall = scene.CreateEntity("static wall");
    wall->AddComponent<Archura::BoxCollider>();
    CHECK(!physics.ApplyImpulse(wall->GetHandle(),{4,0,0},{0,0,0}));
    const auto stale = ball->GetHandle();
    scene.DestroyEntity(stale);
    CHECK(!physics.ApplyImpulse(stale,{4,0,0},{0,0,0}));
}

void TestPausePresentationTransition() {
    Archura::Scene scene("pause render transition");
    auto* ball=Ball(scene);
    ball->GetComponent<Archura::RigidBody>()->velocity={3,0,0};
    Archura::PhysicsSystem physics; physics.Init(&scene);
    physics.Update(.025f);
    const auto current=ball->GetWorldTransform();
    CHECK(physics.GetRenderWorldTransform(*ball)[3].x<current[3].x);
    const auto accumulator=physics.GetAccumulator();
    // Application switches to raw pose while paused; a resume before the next
    // physics tick must not reuse the previous pair and jump backward.
    physics.ResetRenderPoses();
    CHECK(Near(physics.GetRenderWorldTransform(*ball),current,0));
    CHECK(physics.GetAccumulator()==accumulator);
    physics.ResetRenderPoses();
    CHECK(Near(physics.GetRenderWorldTransform(*ball),current,0));
    physics.Update(1.f/60);
    CHECK(physics.GetRenderWorldTransform(*ball)[3].x>=current[3].x);
}
} // namespace

int main() {
    TestVariableRenderRates();
    TestHierarchyAndExternalEdits();
    TestLifetimeModesAndReset();
    TestStallAndRenderInputBounds();
    TestPointImpulses();
    TestPausePresentationTransition();
    if (failures) { std::cerr << failures << " presentation failure(s)\n"; return 1; }
    std::cout << "Physics presentation and point impulse tests passed\n";
}
