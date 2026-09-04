#define SDL_MAIN_HANDLED

#include "ecs/Entity.h"
#include "game/FPSController.h"
#include "game/PhysicsSystem.h"
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

void AddScroll(Archura::Input &input, int amount) {
  SDL_Event event{};
  event.type = SDL_MOUSEWHEEL;
  event.wheel.y = amount;
  input.OnEvent(event);
}

float RunScrolledFrame(int fixedTicks) {
  Archura::Camera camera;
  Archura::FPSController controller(&camera);
  Archura::Input input(nullptr);
  input.SetCursorMode(2);

  input.Update();
  AddScroll(input, 1);
  controller.HandleMouseLook(&input, 0.0f);
  for (int tick = 0; tick < fixedTicks; ++tick)
    controller.Update(&input, nullptr, kTick, nullptr, nullptr);

  return camera.GetFOV();
}

void TestScrollIsIndependentOfFixedTickCount() {
  CHECK(Near(RunScrolledFrame(0), 74.0f));
  CHECK(Near(RunScrolledFrame(5), 74.0f));
}

void TestNextFrameResetDoesNotRepeatScroll() {
  Archura::Camera camera;
  Archura::FPSController controller(&camera);
  Archura::Input input(nullptr);
  input.SetCursorMode(2);

  input.Update();
  AddScroll(input, 1);
  controller.HandleMouseLook(&input, 0.0f);
  CHECK(Near(camera.GetFOV(), 74.0f));

  input.Update();
  controller.HandleMouseLook(&input, 0.0f);
  for (int tick = 0; tick < 5; ++tick)
    controller.Update(&input, nullptr, kTick, nullptr, nullptr);
  CHECK(Near(camera.GetFOV(), 74.0f));
}

void TestUnlockedCursorDoesNotApplyGameplayScroll() {
  Archura::Camera camera;
  Archura::FPSController controller(&camera);
  Archura::Input input(nullptr);
  input.SetCursorMode(0);

  input.Update();
  AddScroll(input, 1);
  controller.HandleMouseLook(&input, 0.0f);
  for (int tick = 0; tick < 5; ++tick)
    controller.Update(&input, nullptr, kTick, nullptr, nullptr);

  CHECK(Near(camera.GetFOV(), 75.0f));
}

} // namespace

// The test exercises production Input/FPSController/Camera code and replaces
// only unrelated OS, weapon, scene, and physics integrations.
extern "C" int SDLCALL SDL_SetRelativeMouseMode(SDL_bool) { return 0; }
extern "C" int SDLCALL SDL_ShowCursor(int) { return 0; }

namespace Archura {

bool WeaponSystem::TryShoot(Weapon *, Entity *, Scene *, Camera *,
                            ProjectileSystem *) {
  return false;
}

bool PhysicsSystem::Raycast(const glm::vec3 &, const glm::vec3 &, float,
                            Entity **, glm::vec3 *) const {
  return false;
}

PhysicsSystem::CharacterMoveResult PhysicsSystem::MoveKinematicAABB(
    const glm::vec3 &position, const glm::vec3 &, const glm::vec3 &velocity,
    float, EntityHandle) const {
  return CharacterMoveResult{position, velocity, false, false};
}

Entity *Scene::GetEntity(EntityHandle) noexcept { return nullptr; }

} // namespace Archura

int main() {
  TestScrollIsIndependentOfFixedTickCount();
  TestNextFrameResetDoesNotRepeatScroll();
  TestUnlockedCursorDoesNotApplyGameplayScroll();

  if (g_Failures != 0) {
    std::cerr << g_Failures << " gameplay scroll check(s) failed\n";
    return 1;
  }

  std::cout << "Gameplay scroll tests passed\n";
  return 0;
}
