#include "Renderer.h"
#include "RenderThread.h"
#include "../core/Logger.h"
#include <glad/glad.h>
#include <algorithm>
#include <iostream>
#include <limits>

namespace Archura {

bool Renderer::Init() {
    if (m_Initialized) return true;
    if (glGetString(GL_VERSION) == nullptr) {
        ARCH_LOG_ERROR("Renderer initialization requires a current OpenGL context");
        return false;
    }

    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 3 || (major == 3 && minor < 3)) {
        ARCH_LOG_ERROR("Archura requires OpenGL 3.3 or newer");
        return false;
    }
    m_RenderThread = std::this_thread::get_id();
    RenderThread::AttachCurrent();
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    m_ViewportWidth = static_cast<unsigned int>(std::max(viewport[2], 0));
    m_ViewportHeight = static_cast<unsigned int>(std::max(viewport[3], 0));

    
    // OpenGL durum ayarlari
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Yuz kirpma - performans icin
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // MSAA - GTX 1050'de sorunsuz calisir
    glEnable(GL_MULTISAMPLE);
    // The current material and UI colors are authored in display (sRGB-like)
    // space and the post-process path is not part of the active frame graph.
    // Enabling automatic framebuffer conversion globally therefore applies a
    // second transfer function to both the scene and ImGui. Keep the legacy
    // display-space pipeline explicit until every render pass is linearized.
    glDisable(GL_FRAMEBUFFER_SRGB);
    
    m_Initialized = true;
    SetClearColor(m_ClearColor);
    return true;
}

void Renderer::Shutdown() {
    if (!m_Initialized) return;
    if (!IsOnRenderThread()) {
        ARCH_LOG_ERROR("Renderer::Shutdown called from a non-render thread");
        return;
    }
    glUseProgram(0);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_Initialized = false;
    RenderThread::Detach();
}

void Renderer::BeginFrame() {
    if (!m_Initialized || !IsOnRenderThread()) return;
    // Keep frame state deterministic even if an optional pass changed it.
    glDisable(GL_FRAMEBUFFER_SRGB);
    m_Stats.Reset();
    if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
        glViewport(0, 0, static_cast<GLsizei>(m_ViewportWidth),
                   static_cast<GLsizei>(m_ViewportHeight));
    }
    Clear();
}

void Renderer::EndFrame() {
    if (!m_Initialized || !IsOnRenderThread()) return;
    // Kare sonu islemleri
    // - UI rendering (ImGui)
    // - Son isleme efektleri
    // vs...
}

void Renderer::SetClearColor(const glm::vec4& color) {
    m_ClearColor = color;
    if (!m_Initialized || !IsOnRenderThread()) return;
    glClearColor(color.r, color.g, color.b, color.a);
}

void Renderer::SetViewport(unsigned int width, unsigned int height) {
    constexpr unsigned int kGLsizeiMax =
        static_cast<unsigned int>(std::numeric_limits<GLsizei>::max());
    m_ViewportWidth = std::min(width, kGLsizeiMax);
    m_ViewportHeight = std::min(height, kGLsizeiMax);
    if (m_Initialized && IsOnRenderThread() && width > 0 && height > 0) {
        glViewport(0, 0, static_cast<GLsizei>(m_ViewportWidth),
                   static_cast<GLsizei>(m_ViewportHeight));
    }
}

bool Renderer::IsOnRenderThread() const {
    return m_RenderThread == std::this_thread::get_id();
}

void Renderer::Clear() {
    if (!m_Initialized || !IsOnRenderThread()) return;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

} // namespace Archura
