#pragma once

#include "EditorSelectionState.h"
#include "TransformUndoHistory.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../ecs/components/AudioSourceComponent.h"
#include "../game/AudioSource.h"
#include "../game/SurfaceProperty.h"
#include "../game/Weapon.h"
#include "../game/Projectile.h"
#include "../game/Particle.h"
#include "../game/Lifetime.h"
#include "../rendering/Camera.h"
#include "../rendering/Mesh.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Archura {

class FPSController;
class Input;
class ParticleSystem;
class PhysicsSystem;
class ProjectileSystem;

enum class PlaySessionMode : uint8_t {
    Edit = 0,
    Play = 1
};

struct PlayModeContext {
    Scene* scene = nullptr;
    Camera* camera = nullptr;
    FPSController* fpsController = nullptr;
    PhysicsSystem* physicsSystem = nullptr;
    ProjectileSystem* projectileSystem = nullptr;
    ParticleSystem* particleSystem = nullptr;
    EditorSelectionState* selection = nullptr;
    TransformUndoHistory* undoHistory = nullptr;
    uint64_t* playerHandleValue = nullptr;
    std::string* playerPersistentID = nullptr;
    std::function<void(Scene*)> onRuntimeStart;
    std::function<void(Scene*)> onRuntimeStop;
};

class PlayModeSession {
public:
    PlaySessionMode GetMode() const noexcept { return m_Mode; }
    bool IsPlaying() const noexcept { return m_Mode == PlaySessionMode::Play; }
    bool HasSnapshot() const noexcept { return m_HasSnapshot; }

    bool ShouldTickSimulation(bool isPaused) const noexcept;

    bool EnterPlayMode(const PlayModeContext& ctx);
    bool ExitPlayMode(const PlayModeContext& ctx);
    // Shutdown/unloaded-scene cleanup: release captured GPU asset owners while
    // the context still exists. Does not restore or run runtime callbacks.
    void DiscardSnapshot() noexcept;

    bool TickSimulation(const PlayModeContext& ctx, Input* input, bool isPaused, float dt);

private:
    struct MeshRendererSnapshot {
        std::shared_ptr<Mesh> meshAsset;
        std::shared_ptr<Shader> shaderAsset;
        std::shared_ptr<Texture> textureAsset;
        Mesh* rawMesh = nullptr;
        Shader* rawShader = nullptr;
        Texture* rawTexture = nullptr;
        glm::vec3 color{1.0f};
        bool hasGeometrySnapshot = false;
        MeshSource meshSource{};
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
    };

    struct EntitySnapshot {
        std::string name;
        std::string persistentID;
        std::string parentPersistentID;
        EntityOwnership ownership = EntityOwnership::Authored;
        std::optional<Transform> transform;
        std::optional<MeshRendererSnapshot> meshRenderer;
        std::optional<BoxCollider> boxCollider;
        std::optional<LightComponent> light;
        std::optional<RigidBody> rigidBody;
        std::optional<Health> health;
        std::optional<Weapon> weapon;
        std::optional<ScriptComponent> script;
        std::optional<SpawnPoint> spawnPoint;
        std::optional<SkyboxComponent> skybox;
        std::optional<AudioSource> audioSource;
        std::optional<AudioSourceComponent> audioSourceComponent;
        std::optional<SurfaceProperty> surfaceProperty;
        std::optional<Lifetime> lifetime;
        std::optional<Particle> particle;
        std::optional<Projectile> projectile;
    };

    struct CameraSnapshot {
        bool valid = false;
        glm::vec3 position{0.0f, 0.0f, 3.0f};
        float yaw = -90.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
        float fov = 75.0f;
        float movementSpeed = 5.0f;
        float mouseSensitivity = 0.1f;
    };

    struct ControllerSnapshot {
        bool valid = false;
        float walkSpeed = 10.0f;
        float runSpeed = 20.0f;
        float jumpHeight = 2.0f;
        float mouseSensitivity = 0.1f;
        bool gravityEnabled = true;
        bool noclipEnabled = false;
    };

    PlaySessionMode m_Mode = PlaySessionMode::Edit;
    bool m_HasSnapshot = false;
    std::vector<EntitySnapshot> m_EntitySnapshots;
    CameraSnapshot m_CameraSnapshot;
    ControllerSnapshot m_ControllerSnapshot;
    SelectionPersistentSnapshot m_SelectionSnapshot;
    std::string m_PlayerPersistentID;
    std::weak_ptr<Scene*> m_SnapshotScene;
};

} // namespace Archura
