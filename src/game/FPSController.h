#pragma once

#include "../rendering/Camera.h"

namespace Archura {

class Input;

/**
 * @brief FPS Controller - First-person karakter kontrolü
 * 
 * WASD hareket, mouse look, koşma, zıplama
 */
struct KeyBindings {
    int forward;
    int backward;
    int left;
    int right;
    int jump;
    int sprint;
};

class FPSController {
public:
    FPSController(Camera* camera);
    ~FPSController() = default;

    void Update(Input* input, class Scene* scene, float deltaTime, class ProjectileSystem* projectileSystem = nullptr);

    // Ayarlar
    void SetWalkSpeed(float speed) { m_WalkSpeed = speed; }
    void SetRunSpeed(float speed) { m_RunSpeed = speed; }
    void SetJumpHeight(float height) { m_JumpHeight = height; }
    void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }
    void SetKeyBindings(const KeyBindings& bindings) { m_Bindings = bindings; }
    void SetGravityEnabled(bool enabled) { m_GravityEnabled = enabled; }
    void SetNoclipEnabled(bool enabled) { m_Noclip = enabled; }

    float GetWalkSpeed() const { return m_WalkSpeed; }
    float GetRunSpeed() const { return m_RunSpeed; }
    float GetJumpHeight() const { return m_JumpHeight; }
    float GetMouseSensitivity() const { return m_MouseSensitivity; }
    KeyBindings& GetKeyBindings() { return m_Bindings; }

    // State
    bool IsGrounded() const { return m_IsGrounded; }
    bool IsRunning() const { return m_IsRunning; }
    bool IsNoclipEnabled() const { return m_Noclip; }

    // Recoil
    // Recoil
    void AddRecoil(const glm::vec3& recoil); // Pitch, Yaw, Roll (Roll unused for now)
    void ResetRecoil();
    
    // Input Handling (Explicit public call)
    void HandleMouseLook(Input* input, float deltaTime);

private:
    void HandleMovement(Input* input, class Scene* scene, float deltaTime);
    bool CheckCollision(const glm::vec3& position, class Scene* scene, float* outGroundHeight = nullptr, float stepHeight = 0.0f);

    // Physics Helpers
    struct MoveParams {
        float max_velocity;
        float accelerate;
        float friction;
        float stop_speed;
        float delta_time;
    };

    void ApplyFriction(const MoveParams& params);
    void Accelerate(const glm::vec3& wishDir, const MoveParams& params);
    void AirAccelerate(const glm::vec3& wishDir, const MoveParams& params);

private:
    Camera* m_Camera;

    // Movement settings
    float m_WalkSpeed = 10.0f; // Kayganliktan dolayi biraz arttirabiliriz 
    float m_RunSpeed = 20.0f;
    float m_JumpHeight = 2.0f; // Bu ziplama gucu icin kullanilacak (sqrt formulunde)
    float m_MouseSensitivity = 0.1f;
    float m_Gravity = -20.0f; // Biraz daha sert duşüş
    bool m_GravityEnabled = true;
    bool m_Noclip = false;
    KeyBindings m_Bindings;

    // Physics / Source Style Movement Constants
    float m_MaxVelocity = 20.0f; // Metric scale max velocity (RunSpeed esitlendi)
    
    // Friction Tuning
    // Quake Unit vs Metric Unit sorunu vardi.
    // StopSpeed cok yuksek oldugu icin (100) surtunme tum hizi yiyordu.
    // Metric sistemde (1 unit = 1 meter) StopSpeed kucuk olmali.
    float m_StopSpeed = 4.0f; // Hızlanma durdurma eşiği (meter/sec) - Arttirildi
    float m_Friction = 6.0f;  // Surtunme arttirildi (Daha keskin durus)
    float m_Acceleration = 15.0f; // Yer ivmesi arttirildi (Daha seri kalkis)
    float m_AirAcceleration = 100.0f; // Hava kontrolü (Yuksek deger = hizli donus)
    float m_AirSpeedCap = 3.0f;  // Havada strafe hizi limiti (Dusuk deger = sadece strafe ile hizlanma)
    
    // State
    glm::vec3 m_Velocity = glm::vec3(0.0f);
    bool m_IsGrounded = true;
    bool m_IsRunning = false;

    // Mantling State
    bool m_IsMantling = false;
    glm::vec3 m_MantleStartPos;
    glm::vec3 m_MantleTargetPos;
    float m_MantleTimer = 0.0f;
    float m_MantleDuration = 0.5f;

    // Recoil State
    glm::vec3 m_CurrentRecoil = glm::vec3(0.0f);
    glm::vec3 m_TargetRecoil = glm::vec3(0.0f);
    float m_RecoilSnappiness = 16.0f;
    float m_RecoilReturnSpeed = 10.0f;

    // Strafe Tilt State
    float m_StrafeTiltAmount = 2.0f; // Derece
    float m_StrafeTiltSpeed = 5.0f;
    float m_CurrentTilt = 0.0f;

    // Bunnyhop / Air Control
    float m_MouseDeltaX = 0.0f; // Last frame's mouse X movement
    float m_MaxBunnyhopSpeed = 30.0f; // Soft cap for bunnyhop speed
};

} // namespace Archura
