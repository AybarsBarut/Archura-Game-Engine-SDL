#include <SDL.h>
#include <glm/glm.hpp>
#include <vector>

namespace Archura {

class Input {
public:
    Input(SDL_Window* window); // Keeps SDL_Window*
    ~Input() = default;

    void Update(); // Called once per frame (mouse delta calculation)
    void OnEvent(const SDL_Event& event); // Process events
    void EndFrame(); // Cleanup (PreviousKeys update)

    // Keyboard
    bool IsKeyPressed(int keycode) const; // Uses SDL_Scancode as int
    bool IsKeyJustPressed(int keycode) const; 
    bool IsKeyDown(int keycode) const;
    bool IsKeyReleased(int keycode) const;

    // Mouse
    bool IsMouseButtonPressed(int button) const; // 1: Left, 2: Middle, 3: Right (SDL_BUTTON_LEFT...)
    bool IsMouseButtonDown(int button) const;
    bool IsMouseButtonReleased(int button) const;

    glm::vec2 GetMousePosition() const { return m_MousePosition; }
    glm::vec2 GetMouseDelta() const { return m_MouseDelta; }
    float GetMouseScrollDelta() const { return m_ScrollDelta; }

    // Cursor
    void SetCursorMode(int mode); // 0: Normal, 1: Hidden, 2: Locked/Relative
    bool IsCursorLocked() const { return m_CursorLocked; }

private:
    SDL_Window* m_Window;
    
    // State
    glm::vec2 m_MousePosition;
    glm::vec2 m_LastMousePosition;
    glm::vec2 m_MouseDelta;
    float m_ScrollDelta;
    bool m_FirstMouse;
    bool m_CursorLocked;
    
    // Keyboard State (using Scancodes)
    Uint8 m_Keys[SDL_NUM_SCANCODES];
    Uint8 m_PreviousKeys[SDL_NUM_SCANCODES];

    // Mouse Button State (1..5)
    bool m_MouseButtons[6];
};

} // namespace Archura

