#pragma once

#include "../../ecs/Component.h"
#include "../../rendering/Mesh.h"

namespace Archura::EditorDetail {
// Shared authoring defaults used by primitive creation and Inspector actions.
inline bool FitPrimitiveCollider(BoxCollider& collider, const Mesh* mesh) {
  if (!mesh) return false;
  const auto& source = mesh->GetSource();
  if (source.type == MeshSourceType::Sphere) {
    collider.shape = BoxCollider::Shape::Sphere;
    collider.radius = source.radius;
    collider.size = glm::vec3(2.0f * source.radius);
    collider.center = glm::vec3(0.0f);
  } else if (source.type == MeshSourceType::Ramp) {
    collider.shape = BoxCollider::Shape::Ramp;
    collider.size = {source.width, source.height, source.depth};
    collider.center = {0.0f, source.height * 0.5f, 0.0f};
  } else if (source.type == MeshSourceType::Cube) {
    collider.shape = BoxCollider::Shape::Box;
    collider.size = glm::vec3(source.size);
    collider.center = glm::vec3(0.0f);
  } else {
    return false;
  }
  return true;
}

inline void ConfigureBall(RigidBody& body) {
  body.mass = 1.0f;
  body.useGravity = true;
  body.isKinematic = false;
  body.canBePushed = true;
  body.continuous = true;
  body.restitution = 0.6f;
  body.friction = 0.5f;
}
} // namespace Archura::EditorDetail
