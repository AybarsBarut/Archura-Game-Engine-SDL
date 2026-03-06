#include "EditorCamera.h"
#include "../input/Input.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <SDL.h>

namespace Archura {

EditorCamera::EditorCamera(const glm::vec3& position, float yaw, float pitch)
    : m_Pos(position), m_Yaw(yaw), m_Pitch(pitch)
{
    RebuildVectors();
}

void EditorCamera::Update(Input* input, float dt) {
    if (!input) return;

    // ── Speed adjustment via scroll (not per-frame alloc – just float math) ──
    float scroll = input->GetMouseScrollDelta();
    if (scroll != 0.0f) {
        m_SpeedMult = std::clamp(m_SpeedMult + scroll * 0.2f, 0.1f, 20.0f);
    }

    // ── Activate look mode only while RMB held ──
    const bool rmbHeld = input->IsMouseButtonDown(SDL_BUTTON_RIGHT);

    if (rmbHeld && !m_LookActive) {
        m_LookActive = true;
        input->SetCursorMode(2); // Locked/Relative mode gives infinite mouse movement
    } else if (!rmbHeld && m_LookActive) {
        m_LookActive = false;
        input->SetCursorMode(0); // Normal cursor
    }

    if (rmbHeld) {
        glm::vec2 delta = input->GetMouseDelta();

        // Only process when there is actual motion to avoid jitter on first frame
        if (delta.x != 0.0f || delta.y != 0.0f) {
            m_Yaw   += delta.x * m_Sensitivity;
            m_Pitch -= delta.y * m_Sensitivity;   // Inverted Y is standard for 3D fly-cam
            m_Pitch  = std::clamp(m_Pitch, -89.0f, 89.0f);
            RebuildVectors();
        }

        // ── WASD / QE movement (only when looking) ──
        const bool shiftHeld = input->IsKeyDown(SDL_SCANCODE_LSHIFT) ||
                               input->IsKeyDown(SDL_SCANCODE_RSHIFT);
        const float speed = m_BaseSpeed * m_SpeedMult * (shiftHeld ? 3.0f : 1.0f) * dt;

        if (input->IsKeyDown(SDL_SCANCODE_W)) m_Pos += m_Front  * speed;
        if (input->IsKeyDown(SDL_SCANCODE_S)) m_Pos -= m_Front  * speed;
        if (input->IsKeyDown(SDL_SCANCODE_A)) m_Pos -= m_Right  * speed;
        if (input->IsKeyDown(SDL_SCANCODE_D)) m_Pos += m_Right  * speed;
        if (input->IsKeyDown(SDL_SCANCODE_Q)) m_Pos -= m_WorldUp * speed;
        if (input->IsKeyDown(SDL_SCANCODE_E)) m_Pos += m_WorldUp * speed;
    }
}

glm::mat4 EditorCamera::GetViewMatrix() const {
    return glm::lookAt(m_Pos, m_Pos + m_Front, m_Up);
}

glm::mat4 EditorCamera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(m_FOV), aspectRatio, 0.1f, 5000.0f);
}

void EditorCamera::RebuildVectors() {
    glm::vec3 front;
    front.x = std::cos(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
    front.y = std::sin(glm::radians(m_Pitch));
    front.z = std::sin(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
    m_Front = glm::normalize(front);
    m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
    m_Up    = glm::normalize(glm::cross(m_Right, m_Front));
}

} // namespace Archura
