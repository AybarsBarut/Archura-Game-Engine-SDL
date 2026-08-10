#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <glm/glm.hpp>
#include <string> // Added for std::string in ScriptComponent
#include <utility>

namespace Archura {

// Forward declarations
class Entity;
class System;

/**
 * @brief Component base class - Tüm component'ler bundan türer
 */
struct Component {
    virtual ~Component() = default;
};

/**
 * @brief Transform component - Pozisyon, rotasyon, scale
 */
struct Transform : public Component {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler angles (degrees)
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 GetModelMatrix() const;
};

/**
 * @brief Mesh Renderer component
 */
struct MeshRenderer : public Component {
    // Strong asset handles keep renderer resources alive across entity copies,
    // editor clipboard operations and cache replacement. The raw members remain
    // as compatibility observers for legacy/script-facing code; new native code
    // must use the Set*Asset helpers.
    std::shared_ptr<class Mesh> meshAsset;
    std::shared_ptr<class Shader> shaderAsset;
    std::shared_ptr<class Texture> textureAsset;
    class Mesh* mesh = nullptr;
    class Shader* shader = nullptr;
    class Texture* texture = nullptr; // Skin/Texture
    glm::vec3 color = glm::vec3(1.0f);

    void SetMeshAsset(std::shared_ptr<Mesh> asset) noexcept {
        meshAsset = std::move(asset);
        mesh = meshAsset.get();
    }
    void SetShaderAsset(std::shared_ptr<Shader> asset) noexcept {
        shaderAsset = std::move(asset);
        shader = shaderAsset.get();
    }
    void SetTextureAsset(std::shared_ptr<Texture> asset) noexcept {
        textureAsset = std::move(asset);
        texture = textureAsset.get();
    }
    void ClearMeshAsset() noexcept { meshAsset.reset(); mesh = nullptr; }
    void ClearShaderAsset() noexcept { shaderAsset.reset(); shader = nullptr; }
    void ClearTextureAsset() noexcept { textureAsset.reset(); texture = nullptr; }
    Mesh* GetMesh() const noexcept { return meshAsset ? meshAsset.get() : mesh; }
    Shader* GetShader() const noexcept { return shaderAsset ? shaderAsset.get() : shader; }
    Texture* GetTexture() const noexcept { return textureAsset ? textureAsset.get() : texture; }
};

/**
 * @brief Box Collider component - Fiziksel carpisma kutusu
 */
struct BoxCollider : public Component {
    enum class Shape : std::uint8_t {
        Box = 0,
        Ramp = 1
    };

    glm::vec3 size = glm::vec3(1.0f); // Boyutlar (Genislik, Yukseklik, Derinlik)
    glm::vec3 center = glm::vec3(0.0f); // Merkez ofseti
    bool isTrigger = false;
    Shape shape = Shape::Box;
};

/**
 * @brief Health component - Can ve hasar durumu
 */
struct Health : public Component {
    float current = 100.0f;
    float max = 100.0f;
    bool isDead = false;
};

/**
 * @brief RigidBody component - Fiziksel hareket ve etkilesim
 */
struct RigidBody : public Component {
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 force = glm::vec3(0.0f);
    float mass = 1.0f;
    float drag = 0.1f;
    float restitution = 0.0f;
    float friction = 0.6f;
    bool useGravity = true;
    // No RigidBody means static. Kinematic bodies are moved by gameplay and
    // have infinite mass in the solver. A non-kinematic body with mass > 0 is
    // dynamic; non-positive/invalid mass is treated as static defensively.
    bool isKinematic = false;
    // Enables adaptive motion substeps. This is bounded CCD mitigation for
    // linear AABB motion, not a general-purpose continuous convex solver.
    bool continuous = false;
};

/**
 * @brief Script component - Harici script'leri baglamak icin
 */
struct ScriptComponent : public Component {
    std::string className; // Name of the C# class
    // Placeholder for script instance ID or pointer
    int scriptInstanceID = -1;
};

/**
 * @brief Spawn Point component - Oyuncu dogus noktasi
 */
struct SpawnPoint : public Component {
    int teamId = 0; // 1 = Team A, 2 = Team B
};

/**
 * @brief Light component - Isik kaynagi
 */
struct LightComponent : public Component {
    enum class Type {
        Directional = 0,
        Point = 1,
        Circle = 2 // Ambient / Everywhere
    };

    Type type = Type::Point;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f; // Sadece Point light icin
};

/**
 * @brief Skybox component - Skybox textures
 */
struct SkyboxComponent : public Component {
    std::vector<std::string> facePaths;
    bool shouldReload = false;

    SkyboxComponent() {
        facePaths.resize(6);
    }
};

} // namespace Archura
