#include "Projectile.h"
#include "ProjectileSystem.h"
#include "PhysicsSystem.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../rendering/Mesh.h"
#include "../core/ResourceManager.h"
#include "Lifetime.h"
#include "SurfaceProperty.h"
#include "Particle.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <random>
#include <algorithm>
#include <cmath>

namespace Archura {

void ProjectileSystem::Update(float deltaTime) {
    if (!m_Scene || !m_PhysicsSystem || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        return;

    m_ProjectilesToDestroy.clear();
    m_ProjectilesToDestroy.reserve(10); // Pre-allocate for common case

    // Tum mermileri guncelle
    for (auto& entityPtr : m_Scene->GetEntities()) {
        auto* projectile = entityPtr->GetComponent<Projectile>();
        if (projectile) {
            UpdateProjectile(entityPtr.get(), projectile, deltaTime);
        }
    }

    // Vurmus veya suresi dolmus mermileri yok et
    std::sort(m_ProjectilesToDestroy.begin(), m_ProjectilesToDestroy.end(),
              [](EntityHandle lhs, EntityHandle rhs) { return lhs.Value() < rhs.Value(); });
    m_ProjectilesToDestroy.erase(
        std::unique(m_ProjectilesToDestroy.begin(), m_ProjectilesToDestroy.end()),
        m_ProjectilesToDestroy.end());
    for (EntityHandle handle : m_ProjectilesToDestroy)
        m_Scene->DestroyEntity(handle);

    // Generic Lifecycle System (Simple implementation here for now)
    std::vector<EntityHandle> expiredEntities;
    for (auto& entityPtr : m_Scene->GetEntities()) {
        auto* lifetime = entityPtr->GetComponent<Lifetime>();
        if (lifetime) {
            lifetime->remainingTime -= deltaTime;
            if (lifetime->remainingTime <= 0.0f) {
                expiredEntities.push_back(entityPtr->GetHandle());
            }
        }
    }
    for (EntityHandle handle : expiredEntities) m_Scene->DestroyEntity(handle);
}

void ProjectileSystem::UpdateProjectile(Entity* entity, Projectile* proj, float deltaTime) {
    auto* transform = entity->GetComponent<Transform>();
    if (!transform) return;

    // Omur suresi kontrolu
    proj->lifetime -= deltaTime;
    if (proj->lifetime <= 0.0f) {
        m_ProjectilesToDestroy.push_back(entity->GetHandle());
        return;
    }

    // El Bombasi Fitili
    if (proj->type == Projectile::ProjectileType::Grenade) {
        proj->fuseTimer -= deltaTime;
        if (proj->fuseTimer <= 0.0f) {
            // Patla!
            // std::cout << "BOOM! Grenade exploded." << std::endl;
            // Alan hasari mantigi burada (basitlestirilmis: sadece yok et)
            // Gercek bir uygulamada, tum varliklara olan mesafeyi kontrol ederdik
            m_ProjectilesToDestroy.push_back(entity->GetHandle());
            return;
        }
    }

    // Yercekimi
    if (proj->gravity != 0.0f) {
        proj->velocity.y += proj->gravity * deltaTime;
    }

    const glm::vec3 displacement = proj->velocity * deltaTime;
    if (glm::length(displacement) <= 1.0e-6f) return;
    PhysicsSystem::QueryFilter filter;
    filter.ignoredA = entity->GetHandle();
    filter.ignoredB = proj->owner;
    filter.includeTriggers = false;
    PhysicsSystem::ShapeCastHit hit;
    const float radius = proj->type == Projectile::ProjectileType::Grenade ? 0.15f : 0.1f;
    if (!m_PhysicsSystem->SweepSphere(transform->position, displacement, radius,
                                      hit, filter)) {
        transform->position += displacement;
        return;
    }

    transform->position += glm::normalize(displacement) * hit.distance;
    transform->position += hit.normal * 1.0e-3f;
    Entity* target = m_Scene->GetEntity(hit.entity);
    if (!target) return;
    SurfaceType surfaceType = SurfaceType::Concrete;
    if (auto* surface = target->GetComponent<SurfaceProperty>())
        surfaceType = surface->type;
    SpawnDecal(m_Scene, hit.point, hit.normal, surfaceType);

    if (proj->type == Projectile::ProjectileType::Grenade) {
        const float incomingNormalSpeed = glm::dot(proj->velocity, hit.normal);
        if (incomingNormalSpeed < 0.0f)
            proj->velocity -= (1.0f + 0.5f) * incomingNormalSpeed * hit.normal;
        const glm::vec3 normalVelocity = glm::dot(proj->velocity, hit.normal) * hit.normal;
        proj->velocity = normalVelocity + (proj->velocity - normalVelocity) * 0.8f;
        if (glm::length(proj->velocity) < 0.5f) proj->velocity = glm::vec3(0.0f);
        return;
    }

    if (auto* health = target->GetComponent<Health>()) {
        health->current = std::max(0.0f, health->current - proj->damage);
    }
    proj->hasHit = true;
    m_ProjectilesToDestroy.push_back(entity->GetHandle());
}

void ProjectileSystem::SpawnDecal(Scene* scene, const glm::vec3& position, const glm::vec3& normal, SurfaceType surfaceType) {
    if (!scene) return;

    Entity* decal = scene->CreateEntity("Decal");
    
    // Add Lifetime
    decal->AddComponent<Lifetime>(10.0f); // 10 seconds lifetime

    // Transform
    auto* transform = decal->GetComponent<Transform>();
    transform->position = position + normal * 0.02f; // Slight offset to prevent Z-fighting
    transform->scale = glm::vec3(0.2f); // 20cm decal

    // Orientation logic (Align Quad +Y to Normal)
    // Simple axis aligned handling for Euler angles
    if (glm::abs(normal.y) > 0.9f) {
        // Ceiling or Floor
        if (normal.y > 0) transform->rotation = glm::vec3(0.0f, 0.0f, 0.0f); // Up
        else transform->rotation = glm::vec3(180.0f, 0.0f, 0.0f); // Down
    } else if (glm::abs(normal.x) > 0.9f) {
        // Walls X
        if (normal.x > 0) transform->rotation = glm::vec3(0.0f, 0.0f, -90.0f);
        else transform->rotation = glm::vec3(0.0f, 0.0f, 90.0f); 
    } else {
        // Walls Z
        if (normal.z > 0) transform->rotation = glm::vec3(90.0f, 0.0f, 0.0f);
        else transform->rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
    }

    // Mesh Renderer
    auto* meshRenderer = decal->AddComponent<MeshRenderer>();
    
    // Use Plane mesh
    auto decalMesh = ResourceManager::Get().GetMeshShared("__projectile_decal");
    if (!decalMesh) {
        decalMesh = ResourceManager::Get().AddMesh(
            "__projectile_decal", Mesh::CreatePlaneShared(1.0f, 1.0f));
    }
    meshRenderer->SetMeshAsset(std::move(decalMesh));

    // Use Bullet Hole Texture if available, else black color
    // Color based on Surface Type
    glm::vec3 decalColor = glm::vec3(0.1f); // Default Dark Grey (Concrete)
    
    switch (surfaceType) {
        case SurfaceType::Flesh:
             decalColor = glm::vec3(0.8f, 0.0f, 0.0f); // Red Blood
             break;
        case SurfaceType::Wood:
             decalColor = glm::vec3(0.6f, 0.4f, 0.2f); // Brown Wood
             break;
        case SurfaceType::Metal:
             decalColor = glm::vec3(0.7f, 0.7f, 0.75f); // Bluish Grey Metal
             break;
        case SurfaceType::Dirt:
             decalColor = glm::vec3(0.4f, 0.3f, 0.2f); // Dark Brown Dirt
             break;
        case SurfaceType::Glass:
             decalColor = glm::vec3(0.5f, 0.9f, 0.9f); // Cyan Glass
             break;
        default:
             break;
    }
    
    // Randomize slightly for variety (optional, keep simple for now)
    
    // Texture logic placeholder (eventually use different textures per material)
    meshRenderer->color = decalColor;

    // --- Spawn Particles ---
    int particleCount = 5;
    float particleSpeed = 2.0f;
    bool particleGravity = true;
    float particleSize = 0.05f;

    if (surfaceType == SurfaceType::Metal) {
        particleCount = 10;
        particleSpeed = 5.0f;
        particleGravity = true;
        decalColor = glm::vec4(1.0f, 0.9f, 0.5f, 1.0f); // Sparks are bright
    } else if (surfaceType == SurfaceType::Flesh) {
        particleCount = 8;
        particleSpeed = 1.0f;
        particleGravity = true;
    } else if (surfaceType == SurfaceType::Concrete) {
        particleSpeed = 1.5f;
    }

    static std::mt19937 mt(std::random_device{}());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for(int i=0; i<particleCount; ++i) {
        Entity* p = scene->CreateEntity("Particle");
        auto* pt = p->AddComponent<Transform>();
        pt->position = position + normal * 0.1f;
        pt->scale = glm::vec3(particleSize);

        auto* par = p->AddComponent<Particle>();
        par->color = glm::vec4(decalColor, 1.0f);
        par->lifetime = 0.5f + (dist(mt) + 1.0f) * 0.2f;
        par->startLifetime = par->lifetime;

        glm::vec3 rDir = glm::vec3(dist(mt), dist(mt), dist(mt));
        if(glm::dot(rDir, normal) < 0) rDir = -rDir;
        par->velocity = glm::normalize(normal + rDir) * particleSpeed;
        if(particleGravity) par->acceleration = glm::vec3(0.0f, -9.81f, 0.0f);

        auto* pmr = p->AddComponent<MeshRenderer>();
        auto cubeMesh = ResourceManager::Get().GetMeshShared("__projectile_pixel");
        if (!cubeMesh) {
            cubeMesh = ResourceManager::Get().AddMesh(
                "__projectile_pixel", Mesh::CreateCubeShared(1.0f));
        }
        pmr->SetMeshAsset(std::move(cubeMesh));
        pmr->color = glm::vec3(decalColor);
    }
}

Entity* ProjectileSystem::SpawnProjectile(
    Scene* scene,
    const glm::vec3& position,
    const glm::vec3& direction,
    float speed,
    float damage,
    Entity* owner,
    Projectile::ProjectileType type
) {
    const float directionLength = glm::length(direction);
    if (!scene || !std::isfinite(directionLength) || directionLength <= 1.0e-6f ||
        !std::isfinite(speed) || speed < 0.0f || !std::isfinite(damage)) return nullptr;

    // Mermi varligi olustur
    Entity* projectile = scene->CreateEntity("Projectile");
    
    // Donusum
    auto* transform = projectile->GetComponent<Transform>();
    transform->position = position;
    
    auto* meshRenderer = projectile->AddComponent<MeshRenderer>();
    
    if (type == Projectile::ProjectileType::Grenade) {
        meshRenderer->SetMeshAsset(Mesh::CreateCubeShared(1.0f));
        meshRenderer->color = glm::vec3(0.0f, 0.5f, 0.0f); // Yesil El Bombasi
        transform->scale = glm::vec3(0.3f);
    } else {
        // Mermi
        auto bulletMesh = ResourceManager::Get().GetMeshShared("bullet");
        if (!bulletMesh) {
            bulletMesh = ResourceManager::Get().AddMesh(
                "bullet", Mesh::CreateSphereShared(0.5f, 8));
        }
        meshRenderer->SetMeshAsset(std::move(bulletMesh));
        meshRenderer->color = glm::vec3(1.0f, 1.0f, 0.0f); // Sari Mermi
        transform->scale = glm::vec3(0.1f, 0.1f, 0.3f);
    }

    // Yone gore rotasyon hesapla (Basit)
    glm::vec3 normalizedDir = direction / directionLength;
    
    // Mermi bileseni
    auto* proj = projectile->AddComponent<Projectile>();
    proj->velocity = normalizedDir * speed;
    proj->speed = speed;
    proj->damage = damage;
    proj->owner = owner ? owner->GetHandle() : EntityHandle{};
    proj->type = type;
    
    if (type == Projectile::ProjectileType::Grenade) {
        proj->gravity = -9.81f;
        proj->lifetime = 10.0f;
        proj->fuseTimer = 5.0f;
    } else {
        proj->gravity = 0.0f; // Mermiler duz ucar
        proj->lifetime = 5.0f;
    }

    // std::cout << "Spawned projectile at " << position.x << ", " << position.y << ", " << position.z << std::endl;

    return projectile;
}

} // namespace Archura
