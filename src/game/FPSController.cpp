#include "FPSController.h"
#include "../input/Input.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../game/Weapon.h"
#include "../game/Projectile.h"
#include "../game/PhysicsSystem.h" // Added for physics raycast
#include <algorithm>

namespace Archura {

FPSController::FPSController(Camera* camera)
    : m_Camera(camera)
{
    // Varsayilan tus atamalari
    m_Bindings.forward = SDL_SCANCODE_W;
    m_Bindings.backward = SDL_SCANCODE_S;
    m_Bindings.left = SDL_SCANCODE_A;
    m_Bindings.right = SDL_SCANCODE_D;
    m_Bindings.jump = SDL_SCANCODE_SPACE;
    m_Bindings.sprint = SDL_SCANCODE_LSHIFT;
}

void FPSController::Update(Input* input, Scene* scene, float deltaTime, ProjectileSystem* projectileSystem, PhysicsSystem* physicsSystem) {
    // Mouse Look is now handled externally in Application::ProcessInput()
        
    HandleMovement(input, deltaTime, physicsSystem);

    // --- STRAFE TILT (Kamera Yatma) --- DISABLED for better FPS experience
    /*
    float targetTilt = 0.0f;
    if (input->IsKeyDown(m_Bindings.left)) {
        targetTilt = m_StrafeTiltAmount;
    } else if (input->IsKeyDown(m_Bindings.right)) {
        targetTilt = -m_StrafeTiltAmount;
    }

    // Smooth transition (Lerp)
    m_CurrentTilt = m_CurrentTilt + (targetTilt - m_CurrentTilt) * m_StrafeTiltSpeed * deltaTime;
    m_Camera->SetRoll(m_CurrentTilt);
    */
    
    // Kamera roll'u sıfırla (düz tut)
    m_Camera->SetRoll(0.0f);

    // --- GERİ TEPME İYİLEŞTİRMESİ ---
    if (glm::length(m_CurrentRecoil) > 0.001f) {
        glm::vec3 recovery = m_CurrentRecoil * m_RecoilReturnSpeed * deltaTime;
        
        // Aşırı iyileştirme yapma (over-recover)
        if (glm::length(recovery) > glm::length(m_CurrentRecoil)) {
            recovery = m_CurrentRecoil;
        }

        m_CurrentRecoil -= recovery;
        
        // Kamerayı düzelt (aşağı eğ)
        // Not: m_CurrentRecoil pozitiftir (yukarı vuruş), bu yüzden çıkararak (aşağı bakarak) telafi ediyoruz.
        // Ama ProcessMouseMovement (x, y) alır ve y eğimdir (pitch).
        // Eğer geri tepme için +Y (Yukarı) eklediysek, düzeltmek için -Y ekleriz.
        m_Camera->ProcessMouseMovement(-recovery.y, -recovery.x, true);
    }

    // --- ATIŞ MANTIĞI ---
    if (input && input->IsCursorLocked()) {
        Entity* player = nullptr;
        if (scene) {
            // No structural mutation occurs in this lookup. Keep the fallback
            // compatible with the lightweight controller test target.
            for (const auto& entity : scene->GetEntities()) {
                if (entity->GetName() == "Player") {
                    player = entity.get();
                    break;
                }
            }
        }

        auto* weapon = player ? player->GetComponent<Weapon>() : nullptr;
        const bool fireInput = weapon &&
            (weapon->stats.isAutomatic
                 ? input->IsMouseButtonDown(SDL_BUTTON_LEFT)
                 : input->IsMouseButtonPressed(SDL_BUTTON_LEFT));

        if (fireInput && projectileSystem) {
            // Genel sistem sarmalayıcısını örneklendir
            WeaponSystem ws;
            if (ws.TryShoot(weapon, player, scene, m_Camera, projectileSystem)) {
                // Eğim (X ekseni dönüşü) genellikle Y fare hareketidir.
                float rX = ((rand() % 100) / 100.0f - 0.5f) * weapon->stats.recoilAmount * 0.5f;
                float rY = weapon->stats.recoilAmount;
                AddRecoil(glm::vec3(rY, rX, 0.0f));
            }
        }
    }

    // --- OBJECT GRABBING LOGIC ---
    if (physicsSystem && input->IsCursorLocked()) {
        // We will use Right Mouse Button (SDL_BUTTON_RIGHT is usually 3) or E key for grabbing
        bool grabInput = input->IsMouseButtonDown(3) || input->IsKeyDown(SDL_SCANCODE_E);
        
        if (grabInput) {
            if (!m_IsGrabbing) {
                // Try to grab
                Entity* hitEntity = nullptr;
                glm::vec3 hitPoint;
                if (physicsSystem->Raycast(m_Camera->GetPosition(), m_Camera->GetFront(), 15.0f, &hitEntity, &hitPoint)) {
                    // Check if entity is valid and has a transform
                    if (hitEntity && hitEntity->HasComponent<Transform>() && hitEntity->GetName() != "Player" && hitEntity->GetName() != "Floor") {
                        m_GrabbedEntity = hitEntity->GetHandle();
                        m_GrabDistance = glm::distance(m_Camera->GetPosition(), hitPoint);
                        // Make it slightly closer than the actual hit point to avoid clipping
                        if (m_GrabDistance < 2.0f) m_GrabDistance = 2.0f;
                        m_IsGrabbing = true;
                    }
                }
            } else {
                // Currently grabbing, update position
                Entity* grabbedEntity = scene->GetEntity(m_GrabbedEntity);
                if (grabbedEntity && grabbedEntity->HasComponent<Transform>()) {
                    auto* transform = grabbedEntity->GetComponent<Transform>();
                    
                    // Calculate target position
                    glm::vec3 targetPos = m_Camera->GetPosition() + m_Camera->GetFront() * m_GrabDistance;
                    
                    // If it has a rigidbody, zero out its velocity so it doesn't fall while grabbed
                    if (grabbedEntity->HasComponent<RigidBody>()) {
                        auto* rb = grabbedEntity->GetComponent<RigidBody>();
                        rb->velocity = glm::vec3(0.0f); // Hold still
                    }

                    // Lerp position for smooth movement
                    float grabSpeed = 15.0f;
                    transform->position = transform->position + (targetPos - transform->position) * grabSpeed * deltaTime;
                } else {
                    // Entity might have been destroyed
                    m_IsGrabbing = false;
                    m_GrabbedEntity = {};
                }
            }
        } else {
            // Released button
            if (m_IsGrabbing) {
                m_IsGrabbing = false;
                m_GrabbedEntity = {};
            }
        }
    }
}

// Physics Helper: Apply Friction
// Physics Helper: Apply Friction
void FPSController::ApplyFriction(const MoveParams& params) {
    if (!m_IsGrounded) return; // No friction in air

    glm::vec3 vel = m_Velocity;
    // Ignora vertical velocity for friction (Gravity is separate)
    vel.y = 0.0f;
    
    float speed = glm::length(vel);
    
    // If speed is too low, snap to 0 to prevent sliding
    if (speed < 0.1f) {
        m_Velocity.x = 0.0f;
        m_Velocity.z = 0.0f;
        return;
    }

    float drop = 0.0f;
    
    // Apply friction based on speed or stop speed (whichever is larger)
    // control determines the amount of friction applied
    float control = speed < params.stop_speed ? params.stop_speed : speed;
    drop += control * params.friction * params.delta_time;

    float newSpeed = speed - drop;
    if (newSpeed < 0.0f) newSpeed = 0.0f;
    
    if (speed > 0.0f) newSpeed /= speed;

    m_Velocity.x *= newSpeed;
    m_Velocity.z *= newSpeed;
}

// Physics Helper: Accelerate
// Physics Helper: Accelerate
// The projection logic enables air strafing:
// Even if current speed > max_velocity, if the projection of velocity onto wishDir
// is less than max_velocity, we can still add acceleration in that specific direction.
// This allows changing direction (strafing) without losing overall speed in the air.
void FPSController::Accelerate(const glm::vec3& wishDir, const MoveParams& params) {
    // Project current velocity onto the wish direction
    float currentSpeed = glm::dot(m_Velocity, wishDir);
    
    // precise calculation for addSpeed
    float addSpeed = params.max_velocity - currentSpeed;
    
    // If we're already moving faster than max_velocity in the wish direction, don't accelerate
    if (addSpeed <= 0.0f) return;
    
    // Calculate acceleration speed to add this frame
    float accelSpeed = params.accelerate * params.delta_time * params.max_velocity;
    
    // Cap accelSpeed so we don't exceed max_velocity in the wish direction
    if (accelSpeed > addSpeed) accelSpeed = addSpeed;
    
    m_Velocity.x += accelSpeed * wishDir.x;
    m_Velocity.y += accelSpeed * wishDir.y;
    m_Velocity.z += accelSpeed * wishDir.z;
}

// Physics Helper: Air Accelerate (Separate for clarity and future distinct logic)
// Physics Helper: Air Accelerate (Separate for clarity and future distinct logic)
void FPSController::AirAccelerate(const glm::vec3& wishDir, const MoveParams& params) {
    float currentSpeed = glm::dot(m_Velocity, wishDir);
    float addSpeed = params.max_velocity - currentSpeed;
    
    if (addSpeed <= 0.0f) return;
    
    float accelSpeed = params.accelerate * params.delta_time * params.max_velocity;
    if (accelSpeed > addSpeed) accelSpeed = addSpeed;
    
    m_Velocity.x += accelSpeed * wishDir.x;
    m_Velocity.y += accelSpeed * wishDir.y;
    m_Velocity.z += accelSpeed * wishDir.z;
}

void FPSController::HandleMovement(Input* input, float deltaTime,
                                   PhysicsSystem* physicsSystem) {
    // 1. Durum Kontrolu (Grounded?)
    // Basit raycast kontrolu Update sonunda yapiliyor, burada flag kullaniyoruz.
    
    // 2. Giris Yonunu Hesapla (Wish Direction)
    m_IsRunning = input->IsKeyDown(m_Bindings.sprint);
    
    glm::vec3 forward = m_Camera->GetFront();
    glm::vec3 right = m_Camera->GetRight();
    
    forward.y = 0.0f;
    right.y = 0.0f;
    
    if (glm::length(forward) > 0.001f) forward = glm::normalize(forward);
    if (glm::length(right) > 0.001f) right = glm::normalize(right);
    
    glm::vec3 wishDir = glm::vec3(0.0f);
    if (input->IsCursorLocked()) {
        if (input->IsKeyDown(m_Bindings.forward)) wishDir += forward;
        if (input->IsKeyDown(m_Bindings.backward)) wishDir -= forward;
        if (input->IsKeyDown(m_Bindings.left)) wishDir -= right;
        if (input->IsKeyDown(m_Bindings.right)) wishDir += right;
    }
    
    if (glm::length(wishDir) > 0.001f) wishDir = glm::normalize(wishDir);
    
    
    if (glm::length(wishDir) > 0.001f) wishDir = glm::normalize(wishDir);
    
    // --- BUNNYHOPPING LOGIC ---
    // Ziplama kontrolunu surtunmeden ONCE yapmaliyiz.
    // Eger ziplama basarili olursa, m_IsGrounded false olur ve surtunme uygulanmaz.
    // Boylece hiz korunur.
    
    if (input->IsCursorLocked() && input->IsKeyDown(m_Bindings.jump)) {
        if (m_IsGrounded) {
             m_Velocity.y = sqrt(m_JumpHeight * 2.0f * -m_Gravity); 
             m_IsGrounded = false;
        }
    }

    // 3. Fizik Uygula
    if (m_Noclip) {
        // --- NOCLIP MODE ---
        // Gravity devre disi, carpisma devre disi.
        // Tamamen kamera yonunde hareket.
        
        float currentSpeed = m_IsRunning ? m_RunSpeed * 4.0f : m_RunSpeed * 2.0f;
        
        // Kamera yonlendirmesi (3D)
        glm::vec3 flyDir = glm::vec3(0.0f);
        if (input->IsKeyDown(m_Bindings.forward)) flyDir += m_Camera->GetFront();
        if (input->IsKeyDown(m_Bindings.backward)) flyDir -= m_Camera->GetFront();
        if (input->IsKeyDown(m_Bindings.right)) flyDir += m_Camera->GetRight();
        if (input->IsKeyDown(m_Bindings.left)) flyDir -= m_Camera->GetRight();
        
        // Space / Ctrl ile dikey hareket (World Up/Down)
        if (input->IsCursorLocked()) {
            if (input->IsKeyDown(m_Bindings.jump)) flyDir += glm::vec3(0.0f, 1.0f, 0.0f);
            if (input->IsKeyDown(SDL_SCANCODE_LCTRL)) flyDir -= glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (glm::length(flyDir) > 0.001f) {
            flyDir = glm::normalize(flyDir);
            m_Velocity = flyDir * currentSpeed;
        } else {
            // Hizli durus
            m_Velocity = glm::vec3(0.0f);
        }

        // Pozisyonu guncelle (Carpisma kontrolu YOK)
        glm::vec3 currentPos = m_Camera->GetPosition();
        glm::vec3 targetPos = currentPos + m_Velocity * deltaTime;
        m_Camera->SetPosition(targetPos);
        
        return; // Diger fizik hesaplamalarini atla
    }

    if (m_IsGrounded && m_GravityEnabled) {
        // Yerde Hareket
        MoveParams groundParams;
        groundParams.max_velocity = m_IsRunning ? m_RunSpeed : m_WalkSpeed;
        groundParams.accelerate = m_Acceleration;
        groundParams.friction = m_Friction;
        groundParams.stop_speed = m_StopSpeed;
        groundParams.delta_time = deltaTime;

        ApplyFriction(groundParams);
        Accelerate(wishDir, groundParams);
    } else {
        // Havada Hareket (Bunnyhop / Air Control) OR Flying Mode (Gravity Disabled but NOT Noclip)
        
        if (!m_GravityEnabled) {
            // FLY MODE: Simple accelerated flight (but with collisions)
            // Manual friction for 3D dampening (ApplyFriction ignores Y and air)
            
            float flyFriction = m_Friction * 0.5f;
            float currentSpeed = glm::length(m_Velocity);
            
            if (currentSpeed > 0.0f) {
                float drop = currentSpeed * flyFriction * deltaTime;
                float newSpeed = currentSpeed - drop;
                if (newSpeed < 0.0f) newSpeed = 0.0f;
                m_Velocity *= (newSpeed / currentSpeed);
            }

            MoveParams flyParams;
            flyParams.max_velocity = (m_IsRunning ? m_RunSpeed : m_WalkSpeed) * 2.0f; // Faster fly
            flyParams.accelerate = m_Acceleration;
            flyParams.friction = 0.0f; // Already applied manually above
            flyParams.stop_speed = m_StopSpeed;
            flyParams.delta_time = deltaTime;
            
            // Allow vertical movement with Jump/Duck keys (Apply Acceleration directly vs Impulse)
            if (input->IsCursorLocked()) {
                if (input->IsKeyDown(m_Bindings.jump)) {
                    m_Velocity.y += flyParams.accelerate * deltaTime * flyParams.max_velocity * 0.5f; 
                }
                if (input->IsKeyDown(SDL_SCANCODE_LCTRL)) {
                    m_Velocity.y -= flyParams.accelerate * deltaTime * flyParams.max_velocity * 0.5f;
                }
            }

            // Cap Vertical Speed manually to avoid infinite buildup
            if (m_Velocity.y > flyParams.max_velocity) m_Velocity.y = flyParams.max_velocity;
            if (m_Velocity.y < -flyParams.max_velocity) m_Velocity.y = -flyParams.max_velocity;

            Accelerate(wishDir, flyParams);

        } else {
            // Normal Air Physics
            MoveParams airParams;
            airParams.max_velocity = m_AirSpeedCap; 
            airParams.accelerate = m_AirAcceleration;
            airParams.friction = 0.0f; 
            airParams.stop_speed = 0.0f;
            airParams.delta_time = deltaTime;

            int strafeDir = 0;
            if (input->IsKeyDown(m_Bindings.right)) strafeDir = 1;
            else if (input->IsKeyDown(m_Bindings.left)) strafeDir = -1;

            int mouseDir = 0;
            if (m_MouseDeltaX > 1.0f) mouseDir = 1;
            else if (m_MouseDeltaX < -1.0f) mouseDir = -1;

            bool shouldAccelerate = true;
            if (strafeDir != 0) {
                 if (strafeDir == mouseDir) shouldAccelerate = true;
                 else shouldAccelerate = false;
            } 

            if (shouldAccelerate) {
                AirAccelerate(wishDir, airParams);
            }

            // Gravity
            m_Velocity.y += m_Gravity * deltaTime;

            // Bunnyhop Cap
            float horizontalSpeed = sqrt(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);
            if (horizontalSpeed > m_MaxBunnyhopSpeed) {
                float scale = m_MaxBunnyhopSpeed / horizontalSpeed;
                m_Velocity.x *= scale;
                m_Velocity.z *= scale;
            }
        }
    }
    
    // PhysicsSystem is the single collision authority. The camera position is
    // eye height, while the character sweep consumes its AABB center.
    constexpr float playerRadius = 0.3f;
    constexpr float playerHeight = 1.8f;
    const glm::vec3 eyePosition = m_Camera->GetPosition();
    const glm::vec3 center = eyePosition - glm::vec3(0.0f, playerHeight * 0.5f, 0.0f);
    if (physicsSystem) {
        const auto move = physicsSystem->MoveKinematicAABB(
            center, glm::vec3(playerRadius, playerHeight * 0.5f, playerRadius),
            m_Velocity, deltaTime);
        m_Velocity = move.velocity;
        m_IsGrounded = move.grounded;
        m_Camera->SetPosition(move.position + glm::vec3(0.0f, playerHeight * 0.5f, 0.0f));
    } else {
        // Explicit no-physics fallback for tools/tests that do not own a world.
        m_Camera->SetPosition(eyePosition + m_Velocity * deltaTime);
        m_IsGrounded = false;
    }

}

void FPSController::HandleMouseLook(Input* input, float deltaTime) {
    (void)deltaTime;
    // Imlec kilitli ise (FPS modu) kamerayi dondur
    if (input->IsCursorLocked()) {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        
        // Bunnyhop icin mouse delta'yi sakla
        m_MouseDeltaX = mouseDelta.x;

        m_Camera->ProcessMouseMovement(mouseDelta.x * m_MouseSensitivity, 
                                      -mouseDelta.y * m_MouseSensitivity); // Y eksenini ters cevir

        // Scroll is accumulated once per render frame, so consume it only in
        // this per-frame gameplay input path. Fixed ticks must not replay it.
        const float scrollDelta = input->GetMouseScrollDelta();
        if (scrollDelta != 0.0f)
            m_Camera->ProcessMouseScroll(scrollDelta);
    }

    // Sol tik ile imleci kilitleme mantigi Application.cpp'ye tasindi.
    // Artik burada UI durumu kontrol edilmeden kilitlenmeyecek.
    
    // ESC ile imleci serbest birak (Gecici cikis)
    // Not: Ana dongude ESC cikis yapiyor olabilir, bunu kontrol etmeliyiz.
    // Simdilik sadece kilitli degilse serbest birakma mantigi kalsin.
}

void FPSController::AddRecoil(const glm::vec3& recoil) {
    // Anlık vuruş ekle
    m_CurrentRecoil += recoil;

    // Kameraya hemen uygula (Yukarı Vur)
    m_Camera->ProcessMouseMovement(recoil.y, recoil.x, true);
}

void FPSController::ResetRecoil() {
    m_CurrentRecoil = glm::vec3(0.0f);
}

} // namespace Archura
