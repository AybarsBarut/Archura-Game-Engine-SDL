#include "ProjectileSystem.h"
#include "../core/Application.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include <iostream>

namespace Archura {

ProjectileSystem::ProjectileSystem() : m_Scene(nullptr) {
}

void ProjectileSystem::Init(Scene* scene, PhysicsSystem* physicsSystem) {
    m_Scene = scene;
    m_PhysicsSystem = physicsSystem;
}

// Detailed implementations are in Projectile.cpp
// This file serves as the ProjectileSystem definition only

} // namespace Archura
