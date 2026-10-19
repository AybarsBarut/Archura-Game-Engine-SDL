#include "game/SurfaceAttachment.h"
#include <iostream>
#include <limits>

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { ++failures; std::cerr << __LINE__ << ": " #x "\n"; } } while (false)
bool Near(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f) {
    return glm::length(a - b) < epsilon;
}
}

int main() {
    using namespace Archura;
    Scene scene;
    Entity* parent = scene.CreateEntity("moving scaled target");
    auto* transform = parent->GetComponent<Transform>();
    transform->position = {3, -2, 8};
    transform->rotation = {23, 71, -34};
    transform->scale = {2, 0.5f, 3};
    const glm::vec3 point(4, 1, 9);
    for (const glm::vec3 normal : {glm::vec3(0,1,0), glm::vec3(0,-1,0),
                                  glm::normalize(glm::vec3(1,2,-3))}) {
        Entity* decal = scene.CreateEntity("test mark");
        CHECK(AttachSurfaceDecal(*decal, *parent, point, normal, 0.04f));
        const glm::mat4 world = decal->GetWorldTransform();
        CHECK(Near(glm::vec3(world[3]), point + normal * .001f));
        const glm::vec3 worldNormal = glm::normalize(glm::transpose(glm::inverse(glm::mat3(world))) * glm::vec3(0,1,0));
        CHECK(Near(worldNormal, normal));
        CHECK(Near(glm::normalize(glm::cross(glm::vec3(world[2]), glm::vec3(world[0]))), normal));
        CHECK(std::abs(glm::length(glm::vec3(world[0])) - .04f) < .0001f);
        CHECK(std::abs(glm::length(glm::vec3(world[2])) - .04f) < .0001f);
        CHECK(decal->IsEngineOwned());
        CHECK(decal->GetParent() == parent);
        scene.DestroyEntity(decal->GetHandle());
    }
    transform->scale.x = -2;
    auto* reflected = scene.CreateEntity("reflected mark");
    const glm::vec3 tilted = glm::normalize(glm::vec3(3,-1,2));
    CHECK(AttachSurfaceDecal(*reflected, *parent, point, tilted, .04f));
    const auto reflectedWorld = reflected->GetWorldTransform();
    CHECK(Near(glm::normalize(glm::cross(glm::vec3(reflectedWorld[2]),
                                        glm::vec3(reflectedWorld[0]))), tilted));
    scene.DestroyEntity(reflected->GetHandle());
    transform->scale.x = 2;

    // Rolling/translation changes world position but preserves surface-local
    // attachment. A mark is never left at the old world contact position.
    Entity* decal = scene.CreateEntity("rolling mark");
    CHECK(AttachSurfaceDecal(*decal, *parent, point, glm::normalize(glm::vec3(1,2,3)), .04f));
    const auto local = decal->GetComponent<Transform>()->position;
    transform->position += glm::vec3(10, 4, -5);
    transform->rotation += glm::vec3(42, -35, 91);
    CHECK(Near(glm::vec3(decal->GetWorldTransform()[3]),
               glm::vec3(parent->GetWorldTransform() * glm::vec4(local, 1))));
    const auto decalHandle = decal->GetHandle();
    scene.DestroyEntity(parent->GetHandle());
    scene.CreateEntity("replacement must not inherit marks");
    RemoveOrphanedSurfaceDecals(scene);
    CHECK(!scene.IsAlive(decalHandle));

    // Singular and invalid input never publishes partial pose changes.
    Transform result;
    result.position = {7,8,9};
    CHECK(!SurfaceAttachmentPose(glm::mat4(0), point, {0,1,0}, .04f, result));
    CHECK(!SurfaceAttachmentPose(glm::mat4(1), point, {}, .04f, result));
    CHECK(!SurfaceAttachmentPose(glm::mat4(1), point, {0,1,0},
                                std::numeric_limits<float>::infinity(), result));
    CHECK(Near(result.position, {7,8,9}));

    Scene other;
    auto* external = other.CreateEntity("external");
    auto* detached = scene.CreateEntity("detached");
    CHECK(!AttachSurfaceDecal(*detached, *external, point, {0,1,0}, .04f));
    auto* target = scene.CreateEntity("target");
    CHECK(AttachSurfaceDecal(*detached, *target, point, {0,1,0}, .04f));
    const auto detachedHandle = detached->GetHandle();
    detached->SetParent(nullptr);
    RemoveOrphanedSurfaceDecals(scene);
    CHECK(!scene.IsAlive(detachedHandle));

    if (failures) return 1;
    std::cout << "Surface attachment transforms, rolling, scale and lifetime passed\n";
    return 0;
}
