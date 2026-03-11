#pragma once

#include "../rendering/Shader.h"
#include <memory>

namespace Archura {

/**
 * @brief HDR Framebuffer + Bloom post-process sistemi.
 *
 * Kullanim - her frame:
 * @code
 *   // 1. Normal sahne HDR FBO'ya render edilir:
 *   postProcess.BindHDRFramebuffer();
 *   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 *   // ... tum scene draw cagrilari buraya ...
 *
 *   // 2. Post-process gecisleri:
 *   postProcess.Render(); // Bloom + Tone Mapping
 * @endcode
 *
 * ImGui ayarlari:
 * @code
 *   postProcess.hdrEnabled    = true;
 *   postProcess.bloomEnabled  = true;
 *   postProcess.exposure      = 1.2f;
 *   postProcess.bloomThreshold = 0.8f;
 *   postProcess.bloomStrength  = 0.4f;
 *   postProcess.bloomPasses    = 5;
 * @endcode
 */
class PostProcess {
public:
    PostProcess() = default;
    ~PostProcess();

    /// Ekran boyutland. Init'ten once window boyutunu bilmek gerekiyor.
    bool Init(int width, int height);

    /// Pencere boyutu degistiyse FBO'lari yeniden olustur.
    void Resize(int newWidth, int newHeight);

    /// Sahne renderina basmadan once cagir — HDR FBO'yu aktiflestirir.
    void BindHDRFramebuffer();

    /// Sahne bittikten sonra cagir — Bloom + Tone Map gerceklestirir.
    void Render();

    void Shutdown();

    // ── Ayarlar (ImGui'den veya koddan degistirilebilir) ──────────────────
    bool  hdrEnabled      = true;
    bool  bloomEnabled    = true;
    float exposure        = 1.0f;
    float bloomThreshold  = 1.0f;  // 1.0 = HDR araliginda parlak piksel esigi
    float bloomStrength   = 0.3f;
    int   bloomPasses     = 5;     // Daha fazla = daha yaygin bloom (daha pahali)

private:
    void CreateFBOs(int w, int h);
    void DeleteFBOs();
    void SetupQuad();

    int m_Width = 1280, m_Height = 720;
    bool m_Initialized = false;

    // HDR FBO (sahne GL_RGBA16F olarak render edilir)
    unsigned int m_HDRFBO      = 0;
    unsigned int m_HDRColor    = 0; // GL_RGBA16F texture
    unsigned int m_HDRDepth    = 0; // Depth renderbuffer

    // Bloom ping-pong FBO'lari (iki gecis icin iki FBO)
    unsigned int m_BloomFBO[2]   = {0, 0};
    unsigned int m_BloomColor[2] = {0, 0};

    // Full-screen quad
    unsigned int m_QuadVAO = 0;
    unsigned int m_QuadVBO = 0;

    // Shader'lar
    std::unique_ptr<Shader> m_ShaderExtract; // Parlak piksel cikarma
    std::unique_ptr<Shader> m_ShaderBlur;    // Gaussian blur
    std::unique_ptr<Shader> m_ShaderHDR;     // Tone map + birlesim
};

} // namespace Archura
