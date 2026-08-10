#include "PostProcess.h"
#include "RenderThread.h"
#include "../core/Logger.h"
#include <glad/glad.h>
#include <algorithm>
#include <iostream>


namespace Archura {

// Full-screen quad vertexleri (NDC, xy = pos, zw = texcoord)
static const float s_QuadVerts[] = {
    // positions   // texcoords
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

// ---------------------------------------------------------------------------
PostProcess::~PostProcess() {
    Shutdown();
}

bool PostProcess::Init(int width, int height) {
    if (!RenderThread::IsCurrent()) {
        ARCH_LOG_ERROR("[PostProcess] Init must run on the OpenGL render thread");
        return false;
    }
    if (width <= 0 || height <= 0) {
        ARCH_LOG_ERROR("[PostProcess] Invalid framebuffer size");
        return false;
    }
    Shutdown();
    m_Width  = width;
    m_Height = height;

    SetupQuad();
    if (!CreateFBOs(width, height)) {
        Shutdown();
        return false;
    }

    // Shader: parlak piksel cikarma
    m_ShaderExtract = std::make_unique<Shader>();
    if (!m_ShaderExtract->LoadFromFile("assets/shaders/hdr.vert",
                                       "assets/shaders/bloom_extract.frag")) {
        ARCH_LOG_ERROR("[PostProcess] bloom_extract shader yuklenemedi!");
        Shutdown();
        return false;
    }

    // Shader: Gaussian blur
    m_ShaderBlur = std::make_unique<Shader>();
    if (!m_ShaderBlur->LoadFromFile("assets/shaders/hdr.vert",
                                    "assets/shaders/bloom_blur.frag")) {
        ARCH_LOG_ERROR("[PostProcess] bloom_blur shader yuklenemedi!");
        Shutdown();
        return false;
    }

    // Shader: HDR + tone map + bloom birlesim
    m_ShaderHDR = std::make_unique<Shader>();
    if (!m_ShaderHDR->LoadFromFile("assets/shaders/hdr.vert",
                                   "assets/shaders/hdr.frag")) {
        ARCH_LOG_ERROR("[PostProcess] hdr shader yuklenemedi!");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    ARCH_LOG_INFO("[PostProcess] HDR+Bloom baslatildi. " +
                  std::to_string(width) + "x" + std::to_string(height));
    return true;
}

// ---------------------------------------------------------------------------
bool PostProcess::CreateFBOs(int w, int h) {
    // ── HDR FBO ──────────────────────────────────────────────────────────────
    glGenFramebuffers(1, &m_HDRFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFBO);

    glGenTextures(1, &m_HDRColor);
    glBindTexture(GL_TEXTURE_2D, m_HDRColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_HDRColor, 0);

    // Derinlik icin renderbuffer
    glGenRenderbuffers(1, &m_HDRDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_HDRDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_HDRDepth);

    bool complete = true;
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ARCH_LOG_ERROR("[PostProcess] HDR FBO tamamlanamadi!");
        complete = false;
    }

    // ── Bloom ping-pong FBO'lari ─────────────────────────────────────────────
    glGenFramebuffers(2, m_BloomFBO);
    glGenTextures(2, m_BloomColor);

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[i]);
        glBindTexture(GL_TEXTURE_2D, m_BloomColor[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomColor[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ARCH_LOG_ERROR("[PostProcess] Bloom FBO " + std::to_string(i) +
                           " is incomplete");
            complete = false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    if (!complete) DeleteFBOs();
    return complete;
}

void PostProcess::DeleteFBOs() {
    if (m_HDRFBO)     { glDeleteFramebuffers(1, &m_HDRFBO);       m_HDRFBO     = 0; }
    if (m_HDRColor)   { glDeleteTextures(1, &m_HDRColor);          m_HDRColor   = 0; }
    if (m_HDRDepth)   { glDeleteRenderbuffers(1, &m_HDRDepth);     m_HDRDepth   = 0; }
    glDeleteFramebuffers(2, m_BloomFBO);
    glDeleteTextures(2, m_BloomColor);
    for (int i = 0; i < 2; ++i) { m_BloomFBO[i] = 0; m_BloomColor[i] = 0; }
}

// ---------------------------------------------------------------------------
void PostProcess::SetupQuad() {
    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_QuadVerts), s_QuadVerts, GL_STATIC_DRAW);
    // Konum (location 0): vec2
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // TexCoord (location 1): vec2
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
void PostProcess::Resize(int nw, int nh) {
    if (!RenderThread::IsCurrent()) return;
    // A minimized SDL window commonly reports a zero drawable size. Preserve the
    // last valid allocations and rebuild when the drawable becomes non-zero.
    if (nw <= 0 || nh <= 0) return;
    if (nw == m_Width && nh == m_Height) return;
    const int oldWidth = m_Width;
    const int oldHeight = m_Height;
    DeleteFBOs();
    if (CreateFBOs(nw, nh)) {
        m_Width = nw;
        m_Height = nh;
        return;
    }

    ARCH_LOG_ERROR("[PostProcess] Resize failed; restoring prior framebuffer size");
    if (!CreateFBOs(oldWidth, oldHeight)) {
        m_Initialized = false;
    }
}

// ---------------------------------------------------------------------------
void PostProcess::BindHDRFramebuffer() {
    if (!m_Initialized || !RenderThread::IsCurrent()) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFBO);
    glViewport(0, 0, m_Width, m_Height);
}

// ---------------------------------------------------------------------------
void PostProcess::Render() {
    if (!m_Initialized || !RenderThread::IsCurrent()) return;

    GLint previousProgram = 0;
    GLint previousVAO = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture0 = 0;
    GLint previousTexture1 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(m_QuadVAO);

    int finalBloomIndex = 0;

    // ── 1. Bloom Extract: HDR texture'dan parlak pikselleri cikar ────────────
    if (bloomEnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT);
        m_ShaderExtract->Bind();
        m_ShaderExtract->SetInt("uScene", 0);
        m_ShaderExtract->SetFloat("uThreshold", bloomThreshold);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_HDRColor);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ── 2. Gaussian Blur (ping-pong) ──────────────────────────────────────
        m_ShaderBlur->Bind();
        m_ShaderBlur->SetInt("uImage", 0);

        bool horizontal = true;
        const int blurDraws = std::clamp(bloomPasses, 0, 64) * 2;
        for (int i = 0; i < blurDraws; ++i) {
            finalBloomIndex = horizontal ? 1 : 0;
            glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[finalBloomIndex]);
            m_ShaderBlur->SetInt("uHorizontal", horizontal ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_BloomColor[horizontal ? 0 : 1]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
        }
    }

    // ── 3. Tone Map + Bloom Birlesim → Backbuffer ─────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT);
    m_ShaderHDR->Bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_HDRColor);
    m_ShaderHDR->SetInt("uHDRTexture", 0);

    // Bloom texture (son blur sonucu m_BloomColor[0] veya [1])
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomEnabled ? m_BloomColor[finalBloomIndex] : 0);
    m_ShaderHDR->SetInt("uBloomTexture", 1);

    m_ShaderHDR->SetFloat("uExposure", exposure);
    m_ShaderHDR->SetFloat("uBloomStrength", bloomEnabled ? bloomStrength : 0.0f);
    m_ShaderHDR->SetInt("uHDREnabled", hdrEnabled ? 1 : 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(previousVAO));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------
void PostProcess::Shutdown() {
    if ((m_Initialized || m_HDRFBO || m_QuadVAO) && !RenderThread::IsCurrent()) {
        ARCH_LOG_ERROR("[PostProcess] Destruction outside render thread; GPU deletion skipped");
        m_HDRFBO = m_HDRColor = m_HDRDepth = m_QuadVAO = m_QuadVBO = 0;
        m_BloomFBO[0] = m_BloomFBO[1] = 0;
        m_BloomColor[0] = m_BloomColor[1] = 0;
        m_ShaderExtract.reset();
        m_ShaderBlur.reset();
        m_ShaderHDR.reset();
        m_Initialized = false;
        return;
    }
    DeleteFBOs();
    if (m_QuadVAO) { glDeleteVertexArrays(1, &m_QuadVAO); m_QuadVAO = 0; }
    if (m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }
    m_ShaderExtract.reset();
    m_ShaderBlur.reset();
    m_ShaderHDR.reset();
    m_Initialized = false;
}

} // namespace Archura
