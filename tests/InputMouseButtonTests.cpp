#define SDL_MAIN_HANDLED

#include "input/Input.h"

#include <climits>
#include <iostream>

namespace {

int g_Failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "          \
                << #condition << '\n';                                         \
      ++g_Failures;                                                            \
    }                                                                          \
  } while (false)

void SendButtonEvent(Archura::Input &input, Uint32 type, Uint8 button,
                     int x = 0, int y = 0) {
  SDL_Event event{};
  event.type = type;
  event.button.button = button;
  event.button.x = x;
  event.button.y = y;
  input.OnEvent(event);
}

void SendFocusLost(Archura::Input &input) {
  SDL_Event event{};
  event.type = SDL_WINDOWEVENT;
  event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
  input.OnEvent(event);
}

void CheckState(const Archura::Input &input, int button, bool pressed,
                bool down, bool released) {
  CHECK(input.IsMouseButtonPressed(button) == pressed);
  CHECK(input.IsMouseButtonDown(button) == down);
  CHECK(input.IsMouseButtonReleased(button) == released);
}

void TestNormalCrossFrameLifecycleAndRepeatedQueries() {
  Archura::Input input(nullptr);
  constexpr int button = SDL_BUTTON_LEFT;
  CHECK(input.GetMouseButtonPressPosition(button) == glm::vec2(0.0f));

  input.Update();
  CheckState(input, button, false, false, false);

  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, button);
  CheckState(input, button, true, true, false);
  CheckState(input, button, true, true, false);

  input.EndFrame();
  input.Update();
  CheckState(input, button, false, true, false);
  CheckState(input, button, false, true, false);

  SendButtonEvent(input, SDL_MOUSEBUTTONUP, button);
  CheckState(input, button, false, false, true);
  CheckState(input, button, false, false, true);

  input.EndFrame();
  input.Update();
  CheckState(input, button, false, false, false);
}

void TestSameFrameDownThenUpLatchesBothEdges() {
  Archura::Input input(nullptr);
  input.Update();

  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);

  CheckState(input, SDL_BUTTON_LEFT, true, false, true);
  CheckState(input, SDL_BUTTON_LEFT, true, false, true);
}

void TestHeldUpThenDownLatchesBothEdges() {
  Archura::Input input(nullptr);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
  input.EndFrame();
  input.Update();
  CheckState(input, SDL_BUTTON_LEFT, false, true, false);

  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);

  CheckState(input, SDL_BUTTON_LEFT, true, true, true);
  CheckState(input, SDL_BUTTON_LEFT, true, true, true);
}

void TestDuplicateEventsDoNotCreateTransitions() {
  Archura::Input input(nullptr);
  input.Update();

  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
  CheckState(input, SDL_BUTTON_LEFT, true, true, false);

  input.EndFrame();
  input.Update();
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
  CheckState(input, SDL_BUTTON_LEFT, false, true, false);

  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);
  CheckState(input, SDL_BUTTON_LEFT, false, false, true);

  input.EndFrame();
  input.Update();
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT);
  CheckState(input, SDL_BUTTON_LEFT, false, false, false);
}

void TestFocusLossReleasesHeldButtons() {
  Archura::Input input(nullptr);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT);
  input.EndFrame();
  input.Update();

  CheckState(input, SDL_BUTTON_LEFT, false, true, false);
  CheckState(input, SDL_BUTTON_RIGHT, false, true, false);

  SendFocusLost(input);
  CheckState(input, SDL_BUTTON_LEFT, false, false, true);
  CheckState(input, SDL_BUTTON_RIGHT, false, false, true);
  CheckState(input, SDL_BUTTON_LEFT, false, false, true);

  input.EndFrame();
  input.Update();
  CheckState(input, SDL_BUTTON_LEFT, false, false, false);
  CheckState(input, SDL_BUTTON_RIGHT, false, false, false);
}

void TestInvalidIndicesAreSafe() {
  Archura::Input input(nullptr);
  input.Update();
  const int invalidButtons[] = {-1, 0, 6, INT_MAX};
  for (const int button : invalidButtons) {
    CheckState(input, button, false, false, false);
    CHECK(input.GetMouseButtonPressPosition(button) == glm::vec2(0.0f));
  }

  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, 0);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, 6);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, 0);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, 6);
  for (const int button : invalidButtons)
    CheckState(input, button, false, false, false);
}

void TestCoalescedDragKeepsPressPosition() {
  Archura::Input input(nullptr);
  input.Update();
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 100, 200);
  SDL_Event motion{};
  motion.type = SDL_MOUSEMOTION;
  motion.motion.x = 170;
  motion.motion.y = 230;
  input.OnEvent(motion);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 170, 230);
  CHECK(input.GetMousePosition() == glm::vec2(170.0f, 230.0f));
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(100.0f, 200.0f));
  CheckState(input, SDL_BUTTON_LEFT, true, false, true);
  input.Update();
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(0.0f));
  // Button event coordinates also cover a complete drag with no motion event.
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 210, 220);
  CHECK(input.GetMousePosition() == glm::vec2(210.0f, 220.0f));
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 999, 999);
  CHECK(input.GetMousePosition() == glm::vec2(210.0f, 220.0f));
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 280, 220);
  CHECK(input.GetMousePosition() == glm::vec2(280.0f, 220.0f));
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(210.0f, 220.0f));
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 999, 999);
  CHECK(input.GetMousePosition() == glm::vec2(280.0f, 220.0f));
}

void TestPressPositionLifecycle() {
  Archura::Input input(nullptr);
  input.Update();
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 10, 20);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 99, 99);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 30, 40);
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(10.0f, 20.0f));
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_RIGHT) == glm::vec2(30.0f, 40.0f));
  input.Update();
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(10.0f, 20.0f));
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 70, 80);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 50, 60);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 70, 80);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 90, 100);
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(50.0f, 60.0f));
  CheckState(input, SDL_BUTTON_LEFT, true, true, true);
  SendFocusLost(input);
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(0.0f));
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_RIGHT) == glm::vec2(0.0f));
  CheckState(input, SDL_BUTTON_LEFT, true, false, true);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 110, 120);
  CHECK(input.GetMouseButtonPressPosition(SDL_BUTTON_LEFT) == glm::vec2(110.0f, 120.0f));
}

} // namespace

extern "C" int SDLCALL SDL_SetRelativeMouseMode(SDL_bool) { return 0; }
extern "C" int SDLCALL SDL_ShowCursor(int) { return 0; }

int main() {
  TestNormalCrossFrameLifecycleAndRepeatedQueries();
  TestSameFrameDownThenUpLatchesBothEdges();
  TestHeldUpThenDownLatchesBothEdges();
  TestDuplicateEventsDoNotCreateTransitions();
  TestFocusLossReleasesHeldButtons();
  TestInvalidIndicesAreSafe();
  TestCoalescedDragKeepsPressPosition();
  TestPressPositionLifecycle();

  if (g_Failures != 0) {
    std::cerr << g_Failures << " mouse-button check(s) failed\n";
    return 1;
  }

  std::cout << "Input mouse-button tests passed\n";
  return 0;
}
