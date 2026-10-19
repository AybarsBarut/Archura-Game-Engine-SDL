#pragma once

#include "../ecs/Entity.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace Archura {

// Runtime only: the Scene owns both entities; the generation prevents an ID
// recycled after deletion from acquiring an old bullet mark.
struct DecalAttachment : Component {
    EntityHandle target;
};

inline bool SurfaceAttachmentPose(const glm::mat4& parentWorld,
                                  const glm::vec3& point,
                                  const glm::vec3& normal, float width,
                                  Transform& result) {
    const glm::mat3 basis(parentWorld);
    const float determinant = glm::determinant(basis);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1.e-8f ||
        !std::isfinite(width) || width <= 0.0f) return false;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(point[i]) || !std::isfinite(normal[i])) return false;
    const float normalLength = glm::length(normal);
    if (!std::isfinite(normalLength) || normalLength < 1.e-6f) return false;
    const glm::vec3 unitNormal = normal / normalLength;
    // A normal is a covector. Transforming it by transpose(parent), rather
    // than inverse(parent), keeps the local plane tangent under scaled parents.
    const glm::vec3 localNormal = glm::normalize(glm::transpose(basis) * unitNormal);
    const glm::quat orientation = glm::rotation(glm::vec3(0, 1, 0), localNormal);
    const glm::vec3 tangentX = orientation * glm::vec3(1, 0, 0);
    const glm::vec3 tangentZ = orientation * glm::vec3(0, 0, 1);
    Transform pose;
    pose.position = glm::vec3(glm::inverse(parentWorld) *
                              glm::vec4(point + unitNormal * 0.001f, 1));
    pose.rotation = glm::degrees(glm::eulerAngles(orientation));
    pose.scale = glm::vec3(width / glm::length(basis * tangentX), 1,
                          width / glm::length(basis * tangentZ));
    // Preserve the plane's visible winding for reflected/negative-scale parents.
    if (determinant < 0.0f) pose.scale.x = -pose.scale.x;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(pose.position[i]) || !std::isfinite(pose.rotation[i]) ||
            !std::isfinite(pose.scale[i])) return false;
    result = pose;
    return true;
}

inline bool AttachSurfaceDecal(Entity& decal, Entity& target,
                               const glm::vec3& point, const glm::vec3& normal,
                               float width) {
    if (!decal.GetScene() || decal.GetScene() != target.GetScene()) return false;
    Transform local;
    if (!SurfaceAttachmentPose(target.GetWorldTransform(), point, normal, width, local) ||
        !decal.TrySetParent(&target)) return false;
    *decal.GetComponent<Transform>() = local;
    decal.SetOwnership(EntityOwnership::Engine);
    decal.AddComponent<DecalAttachment>()->target = target.GetHandle();
    return true;
}

inline void RemoveOrphanedSurfaceDecals(Scene& scene) {
    scene.ForEachEntity([&](Entity& entity) {
        const auto* attachment = entity.GetComponent<DecalAttachment>();
        if (!attachment) return;
        const auto* target = scene.GetEntity(attachment->target);
        if (!target || entity.GetParent() != target)
            scene.DestroyEntity(entity.GetHandle());
    });
}

} // namespace Archura
