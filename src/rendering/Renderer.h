#pragma once

#include "GraphicsAPI.h"
#include <glm/glm.hpp>
#include <memory>
#include <thread>

namespace Archura {

class Shader;

/**
 * @brief Ana Renderer sınıfı - OpenGL rendering yönetimi
 * 
 * GTX 1050 için optimize edilmiş rendering pipeline
 */
class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    bool Init(GraphicsAPI api = GraphicsAPI::OpenGL);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void SetClearColor(const glm::vec4& color);
    void Clear();
    void SetViewport(unsigned int width, unsigned int height);
    bool IsOnRenderThread() const;
    GraphicsAPI GetGraphicsAPI() const { return m_GraphicsAPI; }

    // Stats
    struct RenderStats {
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t vertices = 0;
        
        void Reset() {
            drawCalls = 0;
            triangles = 0;
            vertices = 0;
        }
    };

    const RenderStats& GetStats() const { return m_Stats; }

private:
    glm::vec4 m_ClearColor = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f); // Koyu gri-mavi
    RenderStats m_Stats;
    std::thread::id m_RenderThread;
    unsigned int m_ViewportWidth = 0;
    unsigned int m_ViewportHeight = 0;
    bool m_Initialized = false;
    GraphicsAPI m_GraphicsAPI = GraphicsAPI::OpenGL;
};

} // namespace Archura
