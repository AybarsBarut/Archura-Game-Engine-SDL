#include "Weapon.h"
#include "Projectile.h"
#include "ProjectileSystem.h"
#include "../ecs/Entity.h"
#include "../input/Input.h"
#include "../rendering/Camera.h"

#include <iostream>
#include <cmath>
#include <algorithm>

namespace Archura {

void WeaponSystem::Update(Scene* scene, EntityHandle entity, Input* input,
                          Camera* camera,
                          ProjectileSystem* projectileSystem,
                          float deltaTime) {
    Update(scene ? scene->GetEntity(entity) : nullptr, input, scene, camera,
           projectileSystem, deltaTime);
}

void WeaponSystem::Update(Entity* entity, Input* input, Scene* scene, Camera* camera, ProjectileSystem* projectileSystem, float deltaTime) {
    (void)scene;
    (void)camera;
    (void)projectileSystem;
    if (!entity) return;

    auto* weapon = entity->GetComponent<Weapon>();
    if (!weapon) return;

    // Zamanlayicilari guncelle
    weapon->timeSinceLastShot += deltaTime;

    // Sarjor degistirme durumu
    if (weapon->isReloading) {
        weapon->reloadTimer += deltaTime;
        if (weapon->reloadTimer >= weapon->stats.reloadTime) {
            // Sarjor degistirme tamamlandi
            int ammoNeeded = weapon->stats.magSize - weapon->stats.currentMag;
            int ammoToReload = std::min(ammoNeeded, weapon->stats.totalAmmo);
            
            weapon->stats.currentMag += ammoToReload;
            weapon->stats.totalAmmo -= ammoToReload;
            
            weapon->isReloading = false;
            weapon->reloadTimer = 0.0f;
            
            // std::cout << "Reload complete! Mag: " << weapon->stats.currentMag << "/" << weapon->stats.magSize << std::endl; 
            //           << " | Total: " << weapon->stats.totalAmmo << std::endl;
        }
        return; // Sarjor degistirme sirasinda ates edilemez
    }



    if (input) {
        // Manuel sarjor degistirme (R tusu)
        if (input->IsKeyPressed(SDL_SCANCODE_R)) {
            if (weapon->stats.currentMag < weapon->stats.magSize && weapon->stats.totalAmmo > 0 && !weapon->isReloading) {
                Reload(weapon);
            }
        }

        // Silah degistirme (1-4 tuslari)
        if (input->IsKeyPressed(SDL_SCANCODE_1)) weapon->SwitchWeapon(Weapon::WeaponType::Rifle);
        if (input->IsKeyPressed(SDL_SCANCODE_2)) weapon->SwitchWeapon(Weapon::WeaponType::Pistol);
        if (input->IsKeyPressed(SDL_SCANCODE_3)) weapon->SwitchWeapon(Weapon::WeaponType::Knife);
        if (input->IsKeyPressed(SDL_SCANCODE_4)) weapon->SwitchWeapon(Weapon::WeaponType::Grenade);
    }
}

bool WeaponSystem::TryShoot(Weapon* weapon, Entity* entity, Scene* scene, Camera* camera,
                             ProjectileSystem* projectileSystem) {
    if (!weapon) return false;

    // Update() owns reload progression; firing must remain blocked until it
    // completes, even though the controller evaluates held-fire later in the
    // same fixed tick.
    if (weapon->isReloading) return false;

    // Ates hizi kontrolu
    if (weapon->timeSinceLastShot < weapon->stats.fireRate) {
        return false;
    }

    // Mermi kontrolu (Bicak haric)
    if (weapon->type != Weapon::WeaponType::Knife) {
        if (weapon->stats.currentMag <= 0) {
            // Otomatik sarjor degistirme (Bicak ve El Bombasi haric)
            if (weapon->type != Weapon::WeaponType::Grenade && weapon->stats.totalAmmo > 0 && !weapon->isReloading) {
                Reload(weapon);
            }
            return false;
        }
    }

    // Ates et
    if (weapon->type != Weapon::WeaponType::Knife) {
        weapon->stats.currentMag--;
    }
    
    weapon->timeSinceLastShot = 0.0f;
    
    // Geri tepme ekle
    weapon->currentRecoil = weapon->stats.recoilAmount;

    // Mermi olustur
    if (projectileSystem && scene && camera) {
        glm::vec3 spawnPos = camera->GetPosition() + camera->GetFront() * 0.5f; // Kamera onunde olustur
        glm::vec3 direction = camera->GetFront();
        
        if (weapon->type == Weapon::WeaponType::Knife) {
            Entity* target = nullptr;
            float closestDistance = weapon->stats.range;
            if (scene) {
                for (const auto& candidatePtr : scene->GetEntities()) {
                    Entity* candidate = candidatePtr.get();
                    if (!candidate || candidate == entity) continue;
                    auto* targetTransform = candidate->GetComponent<Transform>();
                    if (!candidate->GetComponent<Health>() || !targetTransform) continue;
                    const glm::vec3 toTarget = targetTransform->position - spawnPos;
                    const float distance = glm::length(toTarget);
                    if (distance <= 1.0e-4f || distance > closestDistance) continue;
                    if (glm::dot(glm::normalize(toTarget), direction) < 0.75f) continue;
                    target = candidate;
                    closestDistance = distance;
                }
            }
            if (target) {
                if (auto* health = target->GetComponent<Health>()) {
                    health->current = std::max(0.0f,
                                               health->current - weapon->stats.damage);
                    health->isDead = health->current <= 0.0f;
                }
            }
        } 
        else if (weapon->type == Weapon::WeaponType::Grenade) {
            projectileSystem->SpawnProjectile(
                scene,
                spawnPos,
                direction,
                weapon->stats.range, // Firlatma hizi
                weapon->stats.damage,
                entity,
                Projectile::ProjectileType::Grenade
            );
        }
        else {
            // Silahlar
            projectileSystem->SpawnProjectile(
                scene,
                spawnPos,
                direction,
                weapon->stats.range * 2.0f, // Mermi Hizi
                weapon->stats.damage,
                entity,
                Projectile::ProjectileType::Bullet
            );
        }
    }

    return true;
}

void WeaponSystem::Reload(Weapon* weapon) {
    if (!weapon || weapon->isReloading) return;
    
    if (weapon->stats.currentMag >= weapon->stats.magSize) {
        // std::cout << "Magazine is full!" << std::endl;
        return;
    }
    
    if (weapon->stats.totalAmmo <= 0) {
        // std::cout << "No ammo left!" << std::endl;
        return;
    }

    weapon->isReloading = true;
    weapon->reloadTimer = 0.0f;
    // std::cout << "Reloading..." << std::endl;
}

void WeaponSystem::UpdateRecoil(Weapon* weapon, Camera* camera, float deltaTime) {
    if (!weapon || !camera) return;

    // Geri tepme toparlanmasi
    if (weapon->currentRecoil > 0.0f) {
        weapon->currentRecoil -= deltaTime * 10.0f; // Toparlanma hizi
        weapon->currentRecoil = std::max(0.0f, weapon->currentRecoil);
    }
}

void WeaponSystem::ApplyRecoil(Weapon* weapon, Camera* camera) {
    if (!weapon || !camera) return;

    // Kamerayi yukari hareket ettir (geri tepme simulasyonu)
    float recoilPitch = weapon->currentRecoil * -1.0f;
    camera->ProcessMouseMovement(0.0f, recoilPitch);
}

} // namespace Archura
