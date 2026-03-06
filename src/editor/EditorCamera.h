#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Archura {

class Input;

/**
 * @brief Unity-style editor fly-cam.
 *
 * Controls:
 *   Hold RMB  → look mode (pitch/yaw via mouse delta)
 *   W/A/S/D   → move forward/left/back/right (camera-relative)
 *   Q / E     → move down / up (world Y)
 *   Scroll    → vary base move speed
 *   Shift     → 3× speed boost while held
 *
 * No gravity.  No collision.  Zero heap allocations per frame.
 * Completely independent of the game Camera; pass GetViewMatrix()
 * to RenderSystem when editor mode is active.
 */
class EditorCamera {
public:
    EditorCamera() = default;
    explicit EditorCamera(const glm::vec3& position,
                          float yaw   = -90.0f,
                          float pitch =   0.0f);

    // Call once per frame from Application::ProcessInput while in Edit mode.
    void Update(Input* input, float dt);

    glm::mat4 GetViewMatrix()       const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    // Accessors
    const glm::vec3& GetPosition()  const { return m_Pos; }
    float            GetYaw()       const { return m_Yaw; }
    float            GetPitch()     const { return m_Pitch; }
    float            GetFOV()       const { return m_FOV; }

    // Setters
    void SetPosition(const glm::vec3& p)  { m_Pos = p;   }
    void SetSensitivity(float s)           { m_Sensitivity = s; }
    void SetBaseSpeed(float s)             { m_BaseSpeed   = s; }

private:
    void RebuildVectors();

    glm::vec3 m_Pos       { 0.0f, 5.0f, 15.0f };
    glm::vec3 m_Front     { 0.0f, 0.0f, -1.0f };
    glm::vec3 m_Right     { 1.0f, 0.0f,  0.0f };
    glm::vec3 m_Up        { 0.0f, 1.0f,  0.0f };
    glm::vec3 m_WorldUp   { 0.0f, 1.0f,  0.0f };

    float m_Yaw         = -90.0f;
    float m_Pitch       =   0.0f;
    float m_FOV         =  60.0f;

    float m_Sensitivity = 0.12f;
    float m_BaseSpeed   = 12.0f;   // units / sec
    float m_SpeedMult   =  1.0f;   // modified by scroll

    bool  m_LookActive  = false;   // true while RMB held
};

} // namespace Archura
