#pragma once

#include "../ecs/System.h"
#include "../rendering/Camera.h"
#include "../rendering/Shader.h"
#include <memory>
#include "../rendering/Skybox.h"

namespace Archura {

class Entity; // Forward declaration

/**
 * @brief Rendering System - Tüm entity'leri render eder
 */
class RenderSystem : public System {
public:
    RenderSystem(Camera* camera);
    ~RenderSystem() override;

    void Init(Scene* scene) override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    void DrawColliders();

    void SetCamera(Camera* camera) { m_Camera = camera; }
    Camera* GetCamera() const { return m_Camera; }

    /**
     * @brief Override the view + projection matrices for one frame.
     *
     * Call before Update() each frame from the editor when the EditorCamera
     * is active. RenderSystem will use these matrices instead of deriving them
     * from m_Camera.  Call ClearViewOverride() to revert to m_Camera.
     */
    void SetViewOverride(const glm::mat4& view, const glm::mat4& proj) {
        m_ViewOverride       = view;
        m_ProjOverride       = proj;
        m_HasViewOverride    = true;
    }
    void ClearViewOverride() { m_HasViewOverride = false; }

    void SetIsolatedEntity(Entity* entity) { m_IsolationTarget = entity; }

private:
    Entity* m_IsolationTarget = nullptr;
    Camera* m_Camera;
    std::unique_ptr<Shader> m_DefaultShader;
    std::shared_ptr<class Mesh> m_DebugMesh;

    // Editor camera view override (set per-frame when EditorCamera is active)
    glm::mat4 m_ViewOverride     = glm::mat4(1.0f);
    glm::mat4 m_ProjOverride     = glm::mat4(1.0f);
    bool      m_HasViewOverride  = false;
    
    // Lighting
    // Lighting
    // Dynamic lighting handling in Update()

    // Shadows
    bool InitShadowMap();
    void ReleaseGPUResources() noexcept;
    unsigned int m_DepthMapFBO = 0;
    unsigned int m_DepthMapTexture = 0;
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
    std::unique_ptr<Shader> m_DepthShader;
    glm::mat4 m_LightSpaceMatrix;

    std::unique_ptr<class Skybox> m_Skybox;
    bool m_Initialized = false;
};

} // namespace Archura
