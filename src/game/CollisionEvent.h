#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <string>

namespace Archura {

class Entity;

/**
 * @brief Fizik carpismasi event yapisi.
 *
 * PhysicsSystem::OnCollisionEnter/OnCollisionExit callback'leri bu struct'i dondurur.
 */
struct CollisionEvent {
    Entity* entityA = nullptr;   ///< Carpisan taraf A
    Entity* entityB = nullptr;   ///< Carpisan taraf B
    glm::vec3 contactPoint = glm::vec3(0.0f);  ///< Carpisma noktasi (yakl.)
    glm::vec3 normal       = glm::vec3(0.0f);  ///< A'dan B'ye carpisma normali
    float     penetration  = 0.0f;             ///< Nufuz derinligi

    /// Carpisan entitylerden birinin ismini dondurur
    std::string ToString() const;
};

/// Carpisme olay tipik callback imzasi
using CollisionCallback = std::function<void(const CollisionEvent&)>;

} // namespace Archura
