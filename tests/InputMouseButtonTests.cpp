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

void SendButtonEvent(Archura::Input &input, Uint32 type, Uint8 button) {
  SDL_Event event{};
  event.type = type;
  event.button.button = button;
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
  for (const int button : invalidButtons)
    CheckState(input, button, false, false, false);

  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, 0);
  SendButtonEvent(input, SDL_MOUSEBUTTONDOWN, 6);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, 0);
  SendButtonEvent(input, SDL_MOUSEBUTTONUP, 6);
  for (const int button : invalidButtons)
    CheckState(input, button, false, false, false);
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

  if (g_Failures != 0) {
    std::cerr << g_Failures << " mouse-button check(s) failed\n";
    return 1;
  }

  std::cout << "Input mouse-button tests passed\n";
  return 0;
}
