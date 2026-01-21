#include "Input.h"
#include <cstring> // for memset

namespace Archura {

Input::Input(SDL_Window* window)
    : m_Window(window)
    , m_MousePosition(0.0f)
    , m_LastMousePosition(0.0f)
    , m_MouseDelta(0.0f)
    , m_ScrollDelta(0.0f)
    , m_FirstMouse(true)
    , m_CursorLocked(false)
{
    std::memset(m_Keys, 0, sizeof(m_Keys));
    std::memset(m_PreviousKeys, 0, sizeof(m_PreviousKeys));
    std::memset(m_MouseButtons, 0, sizeof(m_MouseButtons));
}

void Input::Update() {
    // Reset per-frame deltas
    m_MouseDelta = glm::vec2(0.0f);
    m_ScrollDelta = 0.0f;
}

void Input::OnEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.repeat == 0) {
            m_Keys[event.key.keysym.scancode] = 1;
        }
    }
    else if (event.type == SDL_KEYUP) {
        m_Keys[event.key.keysym.scancode] = 0;
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button < 6)
            m_MouseButtons[event.button.button] = true;
    }
    else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button < 6)
            m_MouseButtons[event.button.button] = false;
    }
    else if (event.type == SDL_MOUSEWHEEL) {
        m_ScrollDelta += event.wheel.y; // SDL2 mouse wheel y is scroll amount
    }
    else if (event.type == SDL_MOUSEMOTION) {
        m_MousePosition.x = (float)event.motion.x;
        m_MousePosition.y = (float)event.motion.y;

        if (m_CursorLocked) {
            m_MouseDelta.x += (float)event.motion.xrel;
            m_MouseDelta.y += (float)event.motion.yrel;
        }
    }
}

void Input::EndFrame() {
    // Copy current state to previous state
    std::memcpy(m_PreviousKeys, m_Keys, sizeof(m_Keys));
}

bool Input::IsKeyPressed(int keycode) const {
    if (keycode < 0 || keycode >= SDL_NUM_SCANCODES) return false;
    return m_Keys[keycode] != 0;
}

bool Input::IsKeyJustPressed(int keycode) const {
    if (keycode < 0 || keycode >= SDL_NUM_SCANCODES) return false;
    return m_Keys[keycode] != 0 && m_PreviousKeys[keycode] == 0;
}

bool Input::IsKeyDown(int keycode) const {
    return IsKeyPressed(keycode);
}

bool Input::IsKeyReleased(int keycode) const {
    if (keycode < 0 || keycode >= SDL_NUM_SCANCODES) return false;
    return m_Keys[keycode] == 0 && m_PreviousKeys[keycode] != 0;
}

bool Input::IsMouseButtonPressed(int button) const {
    if (button < 0 || button >= 6) return false;
    return m_MouseButtons[button];
}

bool Input::IsMouseButtonDown(int button) const {
    return IsMouseButtonPressed(button);
}

bool Input::IsMouseButtonReleased(int button) const {
    if (button < 0 || button >= 6) return false;
    return !m_MouseButtons[button];
}

void Input::SetCursorMode(int mode) {
    if (mode == 2) { // Locked / Disabled
        SDL_SetRelativeMouseMode(SDL_TRUE);
        m_CursorLocked = true;
        m_FirstMouse = true;
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        m_CursorLocked = false;
    }
}

} // namespace Archura
