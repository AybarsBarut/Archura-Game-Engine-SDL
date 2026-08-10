#include "RenderSystem.h"
#include "../core/Engine.h"
#include "../core/Window.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../rendering/Mesh.h"
#include "../rendering/Texture.h"
#include "../rendering/RenderThread.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>

namespace Archura {

RenderSystem::RenderSystem(Camera* camera)
    : m_Camera(camera)
{
}

RenderSystem::~RenderSystem() {
    Shutdown();
}

void RenderSystem::Init(Scene* scene) {
    Shutdown();
    System::Init(scene);
    
    // Varsayilan shader'i yukle
    m_DefaultShader = std::make_unique<Shader>();
    if (!m_DefaultShader->LoadFromFile("assets/shaders/basic.vert", "assets/shaders/basic.frag")) {
        std::cerr << "Failed to load default shader!\n";
        Shutdown();
        return;
    }

    // Shadow Map Init
    if (!InitShadowMap()) {
        Shutdown();
        return;
    }

    m_DepthShader = std::make_unique<Shader>();
    if (!m_DepthShader->LoadFromFile("assets/shaders/depth.vert", "assets/shaders/depth.frag")) {
        std::cerr << "Failed to load depth shader!\n";
        Shutdown();
        return;
    }

    // Debug Mesh (Cube)
    m_DebugMesh = Mesh::CreateCubeShared(1.0f);
    
    m_Skybox = std::make_unique<Skybox>();
    if (!m_Skybox->Init()) {
        Shutdown();
        return;
    }

    m_Initialized = true;

    // std::cout << "RenderSystem initialized." << std::endl;
}

bool RenderSystem::InitShadowMap() {
    if (!RenderThread::IsCurrent()) return false;
    glGenFramebuffers(1, &m_DepthMapFBO);
    
    glGenTextures(1, &m_DepthMapTexture);
    glBindTexture(GL_TEXTURE_2D, m_DepthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    
    // Bilinear percentage-closer filtering prevents visible square texels.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, m_DepthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete)
        std::cerr << "Shadow Framebuffer is not complete!\n";
        
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!complete) {
        ReleaseGPUResources();
    }
    return complete;
}

void RenderSystem::Update(float deltaTime) {
    (void)deltaTime;
    if (!m_Initialized || !RenderThread::IsCurrent() || !m_Scene || !m_Camera) return;

    // --- Shader Hot-Reload (sadece DEBUG modda dosyalar degistiyse yeniden derle) ---
#ifndef NDEBUG
    if (m_DefaultShader) m_DefaultShader->CheckAndReload();
    if (m_DepthShader)   m_DepthShader->CheckAndReload();
#endif

    // Reset State to defaults for normal rendering
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Kamera matrisleri – use EditorCamera override when injected
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 viewPos;
    Window* window = Engine::Get().GetWindow();

    if (m_HasViewOverride) {
        view       = m_ViewOverride;
        projection = m_ProjOverride;
        // Derive view position from the inverse view matrix (column 3 of inv)
        viewPos = glm::vec3(glm::inverse(view)[3]);
    } else {
        view       = m_Camera->GetViewMatrix();
        projection = m_Camera->GetProjectionMatrix(window->GetAspectRatio());
        viewPos    = m_Camera->GetPosition();
    }

    // Batch Rendering Logic
    // Gruplandirma: Mesh -> (Texture/Shader) -> Transforms
    // Simdilik Shader ve Texture tek tip varsayalim veya basitlestirelim.
    // Map: Mesh* -> List of ModelMatrices
    // Isin dogrusu: Material yapisi olmali. Simdilik MeshRenderer icindeki mesh ve texture'a gore gruplayalim.
    
    struct RenderBatch {
        Mesh* mesh;
        Shader* shader;
        Texture* texture;
        glm::vec3 color; // Color override
        std::vector<glm::mat4> instanceMatrices;
    };
    
    // Basit bir vector kullanalim, her unique (mesh, texture, shader) kombinasyonu icin bir batch
    std::vector<RenderBatch> batches;
    
    // Cull edilenler
    int culledCount = 0;
    int renderedCount = 0;
    
    // 1. Collect
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        if (m_IsolationTarget && entityPtr.get() != m_IsolationTarget) continue; // Isolation Check

        auto* meshRenderer = entityPtr->GetComponent<MeshRenderer>();
        auto* transform = entityPtr->GetComponent<Transform>();
        
        Mesh* renderMesh = meshRenderer ? meshRenderer->GetMesh() : nullptr;
        if (!meshRenderer || !transform || !renderMesh) continue;

        // Frustum Culling
        glm::vec3 entityPos = transform->position;
        float distance = glm::length(entityPos - viewPos);
        if (distance > 1000.0f) {
            culledCount++;
            continue;
        }

        // Batch bul veya olustur
        Shader* targetShader = meshRenderer->GetShader() ? meshRenderer->GetShader() : m_DefaultShader.get();
        Texture* targetTexture = meshRenderer->GetTexture();
        
        bool found = false;
        for (auto& batch : batches) {
            if (batch.mesh == renderMesh &&
                batch.shader == targetShader && 
                batch.texture == targetTexture &&
                batch.color == meshRenderer->color) { // Renk de ayni olmali
                
                batch.instanceMatrices.push_back(entityPtr->GetWorldTransform());
                found = true;
                break;
            }
        }
        
        if (!found) {
            RenderBatch newBatch;
            newBatch.mesh = renderMesh;
            newBatch.shader = targetShader;
            newBatch.texture = targetTexture;
            newBatch.color = meshRenderer->color;
            newBatch.instanceMatrices.push_back(entityPtr->GetWorldTransform());
            batches.push_back(newBatch);
        }
    }
    

    // Lighting Setup
    // Isiklari topla
    struct LightData {
        glm::vec3 position;
        glm::vec3 direction; // For directional
        glm::vec3 color;
        float intensity;
        float range;
        int type; // 0 = Directional, 1 = Point
    };

    std::vector<LightData> lights;
    
    // Varsayilan isik (Gunes) eger hic isik yoksa
    bool hasLights = false;

    for (const auto& entityPtr : m_Scene->GetEntities()) {
        auto* lightComp = entityPtr->GetComponent<LightComponent>();
        auto* transform = entityPtr->GetComponent<Transform>();
        
        if (lightComp && transform) {
            LightData ld;
            ld.position = transform->position; // Point light pos
            
            // Rotation'dan direction cikarimi (Directional light icin)
            // Basitce Z ekseni rotasyonu varsayalim veya transform forward
            // Simdilik (0, -1, 0) varsayip rotation ile cevirmemiz lazim ama 
            // karmasiklastirmamak icin sadece position kullanalim.
            // Directional light icin position = direction origin gibi dusunulebilir simdilik.
            // Calculate direction from rotation
            // Assuming default direction is DOWN (0, -1, 0) for Directional Light
            // or FORWARD (0, 0, -1) depending on convention. 
            // Let's use the Transform's orientation.
            glm::mat4 model = entityPtr->GetWorldTransform();
            // In OpenGL, Forward is usually -Z. But for a "Sun" pointing down, 
            // we often rotate a Forward(-Z) or Down(-Y) vector.
            // Let's assume the light shines in the direction of the object's local Forward (-Z).
            // glm's rotation matrix columns: 0=Right, 1=Up, 2=Backward(+Z)
            // So Forward is -model[2]. 
            ld.direction = glm::normalize(glm::vec3(-model[2])); 

            ld.color = lightComp->color;
            ld.intensity = lightComp->intensity;
            ld.range = lightComp->range;
            ld.type = (int)lightComp->type;
            
            lights.push_back(ld);
            hasLights = true;
            
            // Simdilik sadece ilk 4 isigi alalim (Shader siniri)
            if (lights.size() >= 4) break; 
        }
    }


    // Eger hic isik yoksa varsayilan bir isik ekle
    if (!hasLights) {
        LightData defaultLight;
        defaultLight.position = glm::vec3(5.0f, 10.0f, 5.0f);
        defaultLight.color = glm::vec3(1.0f);
        defaultLight.intensity = 1.0f;
        defaultLight.range = 100.0f;
        defaultLight.type = 1; // Point
        lights.push_back(defaultLight);
    }
    
    // --- 1. Pass: Render to Shadow Map (Directional Light only) ---
    // En yakin directional isigi bul (Gunes)
    // Simdilik list'teki type=0 olan ilk isigi alalim
    
    glm::vec3 lightPos = glm::vec3(0.0f);
    glm::vec3 lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    bool hasDirLight = false;
    
    for (const auto& l : lights) {
        if (l.type == 0) { // Directional
             // "Sun" moves securely with the player to keep shadows high-res implies "Cascaded Shadow Maps"
             // But here we use a simple large ortho map.
             // Position is important for the View Matrix of the light.
             lightPos = l.position;
             lightDirection = l.direction;
             // Ensure the "Light Position" is far enough back along the inverse direction vector
             // so it doesn't clip scene objects behind it if we use it as "Eye" pos.
             // But for Ortho, Z matters less except for Near/Far plane clipping.
             // We'll keep l.position as the "Virtual Source".
             hasDirLight = true;
             break;
        }
    }
    
    // Eger directional light yoksa, golge pass'ini deaktif edebiliriz ama
    // shader texture bekledigi icin bos texture bind etmemiz gerekebilir.
    // Simdilik directional light varsa render edelim.
    
    if (hasDirLight && m_DepthShader) {
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, m_DepthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        // Light Space Matrix
        constexpr float near_plane = 1.0f;
        constexpr float far_plane = 140.0f;
        // Preserve useful texel density around the active camera.
        constexpr float orthoSize = 45.0f;
        glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, near_plane, far_plane);
        
        // Build a non-degenerate directional-light view. Using the entity
        // position as the eye and world-up directly produced NaNs whenever the
        // sun sat above the origin (forward parallel to up).
        if (glm::dot(lightDirection, lightDirection) < 1.0e-8f) {
            lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
        } else {
            lightDirection = glm::normalize(lightDirection);
        }
        const glm::mat4 inverseView = glm::inverse(view);
        glm::vec3 cameraForward = -glm::vec3(inverseView[2]);
        if (glm::dot(cameraForward, cameraForward) < 1.0e-8f)
            cameraForward = glm::vec3(0.0f, 0.0f, -1.0f);
        else
            cameraForward = glm::normalize(cameraForward);
        const glm::vec3 shadowCenter = viewPos + cameraForward * 12.0f;
        lightPos = shadowCenter - lightDirection * 65.0f;
        const glm::vec3 up = std::abs(glm::dot(lightDirection, glm::vec3(0, 1, 0))) > 0.99f
                                 ? glm::vec3(0, 0, 1)
                                 : glm::vec3(0, 1, 0);
        glm::mat4 lightView = glm::lookAt(lightPos, shadowCenter, up);
        
        m_LightSpaceMatrix = lightProjection * lightView;

        // Stabilize the map so sub-texel camera motion cannot make shadows crawl.
        glm::vec4 shadowOrigin = m_LightSpaceMatrix * glm::vec4(0, 0, 0, 1);
        shadowOrigin *= static_cast<float>(SHADOW_WIDTH) * 0.5f;
        const glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = (roundedOrigin - shadowOrigin) *
                                (2.0f / static_cast<float>(SHADOW_WIDTH));
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;
        lightProjection[3] += roundOffset;
        m_LightSpaceMatrix = lightProjection * lightView;
        
        m_DepthShader->Bind();
        m_DepthShader->SetMat4("lightSpaceMatrix", m_LightSpaceMatrix);
        
        // Tum sahneyi depth icin render et
        // Batch mantigini burada da kullanabiliriz ama basitlik icin direkt loop
        // (Sadece MeshRenderer olanlari)
        for (const auto& batch : batches) {
             // Exclude Player from Shadow Map to prevent self-shadowing artifacts ("Black Circle")
             // This is a naive check; ideally use a "CastShadows" flag.
             // We check the first entity in the instance list (or logic needs to be per-entity if batching mixed types)
             // Since we batch by mesh/material, if the player uses a unique mesh/material, it might be in its own batch?
             // Actually, batching merges entities. If "Player" shares a mesh with others (e.g. Cube), it's part of the batch.
             // This structure makes excluding one specific entity hard IF it's batched.
             // However, Player usually has a unique mesh or just a collider in this engine state.
             // Let's assume for now we disable shadows for batches containing the player OR skip this loop optimization
             // and filter during batch creation? 
             // Better: Modify the Batch collection loop to SKIP 'Player' entity for Shadow Batches?
             // No, let's just do a simple filter here if possible, otherwise we risk performance.
             // Given the limited complexity, let's just Render EVERYTHING for now, BUT increasing Bias in shader might help.
             // OR, since the previous code didn't check names, let's try to Fix the Ambient first, and maybe the "Black Circle"
             // is NOT the player shadow but the Flashlight/PointLight logic I suspected earlier.
             // Wait, if I can't easily exclude Player, I should check if Player has a mesh.
             // Application.cpp doesn't show Player having a mesh.
             // Weapon? Weapon has a mesh?
             // If Weapon casts shadow, it's small.
             
             // Let's INCREASE AMBIENT first in Lines 326.
             batch.mesh->DrawInstanced(m_DepthShader.get(), batch.instanceMatrices);
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        // No directional light, reset matrix to identity or keep zero
        // Maybe clear texture to white (depth 1.0) so everything is lit
        glBindFramebuffer(GL_FRAMEBUFFER, m_DepthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // --- 2. Pass: Normal Rendering ---
    
    // Reset Viewport
    glViewport(0, 0, window->GetFramebufferWidth(), window->GetFramebufferHeight());
    
    // 4. Draw Skybox first (optimized via Depth Funcl inside Skybox::Draw)
    // Find Skybox Component
    for (const auto& entityPtr : m_Scene->GetEntities()) {
        auto* skyComp = entityPtr->GetComponent<SkyboxComponent>();
        if (skyComp) {
            if (skyComp->shouldReload) {
                if (m_Skybox->LoadCubemap(skyComp->facePaths)) {
                    skyComp->shouldReload = false;
                }
            }
             m_Skybox->Draw(*m_Camera, Engine::Get().GetWindow()->GetAspectRatio());
             break; // Only one skybox
        }
    }

    for (const auto& batch : batches) {
        if (batch.instanceMatrices.empty()) continue;
        
        Shader* shader = batch.shader;
        shader->Bind();
        
        shader->SetMat4("uView", view);
        shader->SetMat4("uProjection", projection);
        shader->SetVec3("uViewPos", viewPos);

        // Shadow Map Uniforms
        shader->SetMat4("uLightSpaceMatrix", m_LightSpaceMatrix);
        shader->SetInt("uShadowMap", 1); // Texture Unit 1
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_DepthMapTexture);

        // Pass Lights to Shader
        // Note: Shader uniform array desteklemeli: uniform Light uLights[4]; ve int uLightCount;
        shader->SetInt("uLightCount", (int)lights.size());
        
        for (size_t i = 0; i < lights.size(); i++) {
            std::string base = "uLights[" + std::to_string(i) + "]";
            shader->SetVec3(base + ".position", lights[i].position);
            shader->SetVec3(base + ".direction", lights[i].direction);
            shader->SetVec3(base + ".color", lights[i].color);
            shader->SetFloat(base + ".intensity", lights[i].intensity);
            shader->SetFloat(base + ".range", lights[i].range);
            shader->SetInt(base + ".type", lights[i].type);
        }
        
        // Material
        if (batch.texture) {
            batch.texture->Bind(0); // Unit 0
            shader->SetInt("uTexture", 0);
            shader->SetInt("uUseTexture", 1);
            shader->SetVec3("uDiffuse", glm::vec3(1.0f));
        } else {
            shader->SetInt("uUseTexture", 0);
            shader->SetVec3("uDiffuse", batch.color); 
        }

        // Global Ambient (could be uniform or derived from a Light)
        // Set higher base ambient to prevent "Gray/Black" walls
        shader->SetVec3("uAmbient",   glm::vec3(0.4f));
        shader->SetVec3("uSpecular",  glm::vec3(0.0f));   // no specular for rough surfaces
        shader->SetFloat("uShininess", 32.0f);             // safe value (not 0)
        
        // Tek seferde ciz (Instanced)
        batch.mesh->DrawInstanced(shader, batch.instanceMatrices);
        
        renderedCount += static_cast<int>(batch.instanceMatrices.size());
    }

    // Hata ayiklama: her 60 karede bir culling istatistiklerini yazdir
    static int frameCount = 0;
    if (++frameCount >= 60) {
        // std::cout << "Rendered: " << renderedCount << " | Culled: " << culledCount << std::endl;
        frameCount = 0;
    }
}



void RenderSystem::DrawColliders() {
    if (!m_Scene || !m_Camera || !m_DebugMesh || !m_DefaultShader) return;
    if (m_DefaultShader->GetProgramID() == 0) return;

    // Save previous state (optional but good practice)
    GLint polygonMode[2];
    glGetIntegerv(GL_POLYGON_MODE, polygonMode);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE); 
    glDisable(GL_DEPTH_TEST);

    m_DefaultShader->Bind();
    
    // Camera uniforms
    glm::mat4 view = m_Camera->GetViewMatrix();
    glm::mat4 projection = m_Camera->GetProjectionMatrix(Engine::Get().GetWindow()->GetAspectRatio());

    m_DefaultShader->SetMat4("uView", view);
    m_DefaultShader->SetMat4("uProjection", projection);
    m_DefaultShader->SetInt("uUseTexture", 0);
    // Set minimal lighting for wireframe debug rendering
    m_DefaultShader->SetVec3("uAmbient", glm::vec3(1.0f));
    m_DefaultShader->SetVec3("uDiffuse", glm::vec3(1.0f));
    m_DefaultShader->SetVec3("uSpecular", glm::vec3(0.0f));
    m_DefaultShader->SetFloat("uShininess", 1.0f);
    m_DefaultShader->SetInt("uLightCount", 0); // No lights for debug wireframe
 

    for (const auto& entityPtr : m_Scene->GetEntities()) {
        auto* collider = entityPtr->GetComponent<BoxCollider>();
        auto* transform = entityPtr->GetComponent<Transform>();
        
        if (!collider || !transform) continue;

        // Color based on isTrigger
        glm::vec3 color = collider->isTrigger ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        m_DefaultShader->SetVec3("uDiffuse", color);

        // Calculate Transform: EntityModel * ColliderOffset * ColliderSize
        glm::mat4 model = entityPtr->GetWorldTransform();
        model = glm::translate(model, collider->center);
        model = glm::scale(model, collider->size);
        
        m_DefaultShader->SetMat4("uModel", model);
        
        m_DebugMesh->Draw(m_DefaultShader.get());
    }

    // Restore state
    glPolygonMode(GL_FRONT, polygonMode[0]);
    glPolygonMode(GL_BACK, polygonMode[1]);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
}

void RenderSystem::Shutdown() {
    if (!RenderThread::IsCurrent()) {
        if (m_Initialized || m_DepthMapFBO || m_DepthMapTexture || m_DebugMesh) {
            std::cerr << "RenderSystem::Shutdown must run on the render thread\n";
        }
        return;
    }
    m_Initialized = false;
    m_Skybox.reset();
    m_DepthShader.reset();
    m_DefaultShader.reset();
    m_DebugMesh.reset();
    ReleaseGPUResources();
    // std::cout << "RenderSystem shut down." << std::endl;
}

void RenderSystem::ReleaseGPUResources() noexcept {
    if (m_DepthMapTexture) {
        glDeleteTextures(1, &m_DepthMapTexture);
        m_DepthMapTexture = 0;
    }
    if (m_DepthMapFBO) {
        glDeleteFramebuffers(1, &m_DepthMapFBO);
        m_DepthMapFBO = 0;
    }
}

} // namespace Archura
