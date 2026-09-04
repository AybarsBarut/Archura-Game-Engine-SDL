#define SDL_MAIN_HANDLED

#include "ecs/Entity.h"
#include "game/ProjectileSystem.h"
#include "game/Weapon.h"
#include "input/Input.h"
#include "rendering/Camera.h"

#include <cmath>
#include <iostream>

namespace {

constexpr float kTick = 1.0f / 128.0f;
int g_Failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "          \
                << #condition << '\n';                                         \
      ++g_Failures;                                                            \
    }                                                                          \
  } while (false)

bool Near(float actual, float expected) {
  return std::fabs(actual - expected) <= 0.000001f;
}

void Tick(Archura::WeaponSystem &system, Archura::Entity &player) {
  system.Update(&player, nullptr, nullptr, nullptr, nullptr, kTick);
}

void TestFixedTickFireCadence() {
  Archura::Entity player(1, "Player");
  auto *weapon = player.AddComponent<Archura::Weapon>();
  weapon->stats.fireRate = 3.0f * kTick;
  weapon->stats.magSize = 8;
  weapon->stats.currentMag = 8;
  weapon->stats.totalAmmo = 16;

  Archura::WeaponSystem system;

  Tick(system, player);
  CHECK(Near(weapon->timeSinceLastShot, kTick));
  CHECK(!system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));

  Tick(system, player);
  CHECK(Near(weapon->timeSinceLastShot, 2.0f * kTick));
  CHECK(!system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));

  Tick(system, player);
  CHECK(Near(weapon->timeSinceLastShot, weapon->stats.fireRate));
  CHECK(system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));
  CHECK(weapon->stats.currentMag == 7);
  CHECK(Near(weapon->timeSinceLastShot, 0.0f));

  CHECK(!system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));
  Tick(system, player);
  CHECK(!system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));
  Tick(system, player);
  CHECK(!system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));
  Tick(system, player);
  CHECK(system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));
  CHECK(weapon->stats.currentMag == 6);
}

void TestHeldAutomaticFireRemainsPeriodic() {
  Archura::Entity player(2, "Player");
  auto *weapon = player.AddComponent<Archura::Weapon>();
  weapon->stats.isAutomatic = true;
  weapon->stats.fireRate = 2.0f * kTick;
  weapon->stats.magSize = 10;
  weapon->stats.currentMag = 10;
  weapon->stats.totalAmmo = 20;

  Archura::WeaponSystem system;
  int shots = 0;
  for (int tick = 0; tick < 6; ++tick) {
    Tick(system, player);
    if (system.TryShoot(weapon, &player, nullptr, nullptr, nullptr))
      ++shots;
  }

  CHECK(shots == 3);
  CHECK(weapon->stats.currentMag == 7);
}

void TestDefaultRifleBoundary() {
  Archura::Entity player(4, "Player");
  auto *weapon = player.AddComponent<Archura::Weapon>();
  weapon->InitInventory();

  Archura::WeaponSystem system;
  for (int tick = 0; tick < 12; ++tick)
    Tick(system, player);
  CHECK(!system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));

  Tick(system, player);
  CHECK(system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));
  CHECK(weapon->stats.currentMag == 29);
}

void TestReloadDurationAndAmmoConservation() {
  Archura::Entity player(3, "Player");
  auto *weapon = player.AddComponent<Archura::Weapon>();
  weapon->stats.fireRate = kTick;
  weapon->stats.magSize = 5;
  weapon->stats.currentMag = 2;
  weapon->stats.totalAmmo = 7;
  weapon->stats.reloadTime = 4.0f * kTick;

  Archura::WeaponSystem system;
  const int initialAmmo = weapon->stats.currentMag + weapon->stats.totalAmmo;
  system.Reload(weapon);
  CHECK(weapon->isReloading);

  for (int tick = 0; tick < 3; ++tick) {
    Tick(system, player);
    CHECK(weapon->isReloading);
    CHECK(weapon->stats.currentMag == 2);
    CHECK(weapon->stats.totalAmmo == 7);
    CHECK(!system.TryShoot(weapon, &player, nullptr, nullptr, nullptr));
  }

  Tick(system, player);
  CHECK(!weapon->isReloading);
  CHECK(Near(weapon->reloadTimer, 0.0f));
  CHECK(weapon->stats.currentMag == 5);
  CHECK(weapon->stats.totalAmmo == 4);
  CHECK(weapon->stats.currentMag + weapon->stats.totalAmmo == initialAmmo);
}

void TestDestroyedPlayerHandleIsSafe() {
  Archura::Scene scene("weapon-lifetime");
  Archura::Entity *player = scene.CreateEntity("Player");
  auto *weapon = player->AddComponent<Archura::Weapon>();
  weapon->stats.fireRate = kTick;
  const Archura::EntityHandle handle = player->GetHandle();

  Archura::WeaponSystem system;
  system.Update(&scene, handle, nullptr, nullptr, nullptr, kTick);
  CHECK(Near(weapon->timeSinceLastShot, kTick));
  CHECK(scene.DestroyEntity(handle));
  CHECK(scene.GetEntity(handle) == nullptr);

  // A stale generation-checked handle resolves to null instead of becoming a
  // dangling raw pointer inside the fixed-tick weapon scheduler.
  system.Update(&scene, handle, nullptr, nullptr, nullptr, kTick);
  CHECK(scene.GetEntity(handle) == nullptr);
}

} // namespace

// These focused tests intentionally replace non-state integrations. Weapon.cpp
// still supplies the production timing, reload, and shot-consumption logic.
namespace Archura {

bool Input::IsKeyPressed(int) const { return false; }

void Camera::ProcessMouseMovement(float, float, bool) {}

Entity *ProjectileSystem::SpawnProjectile(
    Scene *, const glm::vec3 &, const glm::vec3 &, float, float, Entity *,
    Projectile::ProjectileType) {
  return nullptr;
}

} // namespace Archura

int main() {
  TestFixedTickFireCadence();
  TestHeldAutomaticFireRemainsPeriodic();
  TestDefaultRifleBoundary();
  TestReloadDurationAndAmmoConservation();
  TestDestroyedPlayerHandleIsSafe();

  if (g_Failures != 0) {
    std::cerr << g_Failures << " weapon progression check(s) failed\n";
    return 1;
  }

  std::cout << "Weapon progression tests passed\n";
  return 0;
}
