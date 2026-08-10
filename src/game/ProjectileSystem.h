#pragma once

#include "../ecs/System.h"
#include "Projectile.h"
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace Archura {

class Scene;
class Entity;
class PhysicsSystem;

class ProjectileSystem : public System {
public:
    ProjectileSystem();
    ~ProjectileSystem() = default;

    void Init(Scene* scene, PhysicsSystem* physicsSystem);
    void Update(float deltaTime) override;

    void UpdateProjectile(Entity* entity, Projectile* proj, float deltaTime);
    void SpawnDecal(Scene* scene, const glm::vec3& position, const glm::vec3& normal, SurfaceType surfaceType);
    
    Entity* SpawnProjectile(Scene* scene, const glm::vec3& position, const glm::vec3& direction, 
                            float speed, float damage, Entity* owner, Projectile::ProjectileType type);

private:
    Scene* m_Scene;
    PhysicsSystem* m_PhysicsSystem = nullptr;
    std::vector<EntityHandle> m_ProjectilesToDestroy;
};

} // namespace Archura
