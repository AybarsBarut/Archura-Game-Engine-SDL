#pragma once

#include "../ecs/Entity.h"
#include <glm/glm.hpp>
#include <functional>

namespace Archura {

enum class CollisionPhase { Enter, Stay, Exit };

/**
 * @brief Fizik carpismasi event yapisi.
 *
 * PhysicsSystem::OnCollisionEnter/OnCollisionExit callback'leri bu struct'i dondurur.
 */
struct CollisionEvent {
    // Generation-checked handles make queued/copied events safe after an
    // entity is destroyed. Consumers resolve them through Scene::GetEntity.
    EntityHandle entityA{};
    EntityHandle entityB{};
    glm::vec3 contactPoint = glm::vec3(0.0f);  ///< Carpisma noktasi (yakl.)
    glm::vec3 normal       = glm::vec3(0.0f);  ///< A'dan B'ye carpisma normali
    float     penetration  = 0.0f;             ///< Nufuz derinligi
    CollisionPhase phase = CollisionPhase::Enter;
    bool isTrigger = false;
};

/// Carpisme olay tipik callback imzasi
using CollisionCallback = std::function<void(const CollisionEvent&)>;

} // namespace Archura
