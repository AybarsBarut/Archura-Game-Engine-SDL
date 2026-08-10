#include "HUDRenderer.h"
#include "Shader.h"
#include "RenderThread.h"
#include "Texture.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

namespace Archura {

HUDRenderer::HUDRenderer()
    : m_QuadVAO(0)
    , m_QuadVBO(0)
    , m_QuadEBO(0)
    , m_ScreenWidth(1920.0f)
    , m_ScreenHeight(1080.0f)
{
}

HUDRenderer::~HUDRenderer() {
    Shutdown();
}

bool HUDRenderer::Init() {
    if (!RenderThread::IsCurrent()) return false;
    // HUD shader (basit 2D shader)
    m_HUDShader = std::make_unique<Shader>();
    
    std::string vertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoords;
        
        out vec2 TexCoords;
        
        uniform mat4 uProjection;
        uniform mat4 uModel;
        
        void main() {
            TexCoords = aTexCoords;
            gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
        }
    )";
    
    std::string fragmentSrc = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 FragColor;
        
        uniform vec4 uColor;
        uniform sampler2D uTexture;
        uniform bool uUseTexture;
        
        void main() {
            if (uUseTexture) {
                FragColor = texture(uTexture, TexCoords) * uColor;
            } else {
                FragColor = uColor;
            }
        }
    )";
    
    if (!m_HUDShader->LoadFromSource(vertexSrc, fragmentSrc)) {
        std::cerr << "Failed to create HUD shader!\n";
        return false;
    }

    CreateQuadMesh();
    

    return true;
}

void HUDRenderer::Shutdown() {
    if ((m_QuadVAO || m_QuadVBO || m_QuadEBO) && !RenderThread::IsCurrent()) {
        m_QuadVAO = m_QuadVBO = m_QuadEBO = 0;
        m_HUDShader.reset();
        return;
    }
    if (m_QuadVAO) { glDeleteVertexArrays(1, &m_QuadVAO); m_QuadVAO = 0; }
    if (m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }
    if (m_QuadEBO) { glDeleteBuffers(1, &m_QuadEBO); m_QuadEBO = 0; }
    
    m_HUDShader.reset();
}

void HUDRenderer::BeginHUD(float width, float height) {
    if (m_InHUDPass || width <= 0.0f || height <= 0.0f ||
        !RenderThread::IsCurrent()) return;
    m_ScreenWidth = width;
    m_ScreenHeight = height;

    glGetIntegerv(GL_CURRENT_PROGRAM, &m_PreviousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_PreviousVAO);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &m_PreviousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_PreviousTexture0);
    glActiveTexture(static_cast<GLenum>(m_PreviousActiveTexture));
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_BlendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_BlendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_BlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &m_BlendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_BlendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_BlendEquationAlpha);
    m_DepthWasEnabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    m_CullWasEnabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    m_BlendWasEnabled = glIsEnabled(GL_BLEND) == GL_TRUE;
    m_InHUDPass = true;

    // 2D cizim icin derinlik testini kapat
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // HUD cizimi icin kirpmayi kapat
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void HUDRenderer::EndHUD() {
    if (!m_InHUDPass || !RenderThread::IsCurrent()) return;
    if (m_DepthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (m_CullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (m_BlendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFuncSeparate(static_cast<GLenum>(m_BlendSrcRGB),
                        static_cast<GLenum>(m_BlendDstRGB),
                        static_cast<GLenum>(m_BlendSrcAlpha),
                        static_cast<GLenum>(m_BlendDstAlpha));
    glBlendEquationSeparate(static_cast<GLenum>(m_BlendEquationRGB),
                            static_cast<GLenum>(m_BlendEquationAlpha));
    glUseProgram(static_cast<GLuint>(m_PreviousProgram));
    glBindVertexArray(static_cast<GLuint>(m_PreviousVAO));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_PreviousTexture0));
    glActiveTexture(static_cast<GLenum>(m_PreviousActiveTexture));
    m_InHUDPass = false;
}

void HUDRenderer::DrawRect(float x, float y, float width, float height, const glm::vec4& color) {
    if (!RenderThread::IsCurrent() || !m_HUDShader || m_QuadVAO == 0) return;
    // Ortografik projeksiyon (ekran uzayi)
    glm::mat4 projection = glm::ortho(0.0f, m_ScreenWidth, 0.0f, m_ScreenHeight);
    
    // Model matrisi (pozisyon ve olcek)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(width, height, 1.0f));
    
    m_HUDShader->Bind();
    m_HUDShader->SetMat4("uProjection", projection);
    m_HUDShader->SetMat4("uModel", model);
    m_HUDShader->SetVec4("uColor", color);
    m_HUDShader->SetInt("uUseTexture", 0);
    
    glBindVertexArray(m_QuadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void HUDRenderer::DrawTexture(Texture* texture, float x, float y, float width, float height) {
    if (!RenderThread::IsCurrent() || !texture || !texture->IsLoaded() ||
        !m_HUDShader || m_QuadVAO == 0) return;

    glm::mat4 projection = glm::ortho(0.0f, m_ScreenWidth, 0.0f, m_ScreenHeight);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(width, height, 1.0f));
    
    m_HUDShader->Bind();
    m_HUDShader->SetMat4("uProjection", projection);
    m_HUDShader->SetMat4("uModel", model);
    m_HUDShader->SetVec4("uColor", glm::vec4(1.0f));
    m_HUDShader->SetInt("uUseTexture", 1);
    m_HUDShader->SetInt("uTexture", 0);
    
    texture->Bind(0);
    
    glBindVertexArray(m_QuadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void HUDRenderer::DrawCrosshair(float size, const glm::vec4& color) {
    float centerX = m_ScreenWidth * 0.5f;
    float centerY = m_ScreenHeight * 0.5f;
    
    // Fixed size small crosshair
    float crossSize = std::max(size * 0.5f, 1.0f);
    float thickness = std::max(size * 0.1f, 1.0f);
    
    // Yatay
    DrawRect(centerX - crossSize, centerY - thickness * 0.5f, crossSize * 2.0f, thickness, color);
    
    // Dikey
    DrawRect(centerX - thickness * 0.5f, centerY - crossSize, thickness, crossSize * 2.0f, color);
}

void HUDRenderer::DrawAmmoCounter(int current, int total, float x, float y) {
    // Basit mermi gostergesi (simdilik metin cizimi yok, sadece cubuk)
    float barWidth = 200.0f;
    float barHeight = 20.0f;
    
    // Arkaplan
    DrawRect(x, y, barWidth, barHeight, glm::vec4(0.2f, 0.2f, 0.2f, 0.8f));
    
    // Mevcut mermi cubugu
    float fillRatio = total > 0 ? std::clamp((float)current / (float)total, 0.0f, 1.0f) : 0.0f;
    glm::vec4 ammoColor = fillRatio > 0.3f ? glm::vec4(0.3f, 0.8f, 0.3f, 0.9f) : glm::vec4(0.8f, 0.2f, 0.2f, 0.9f);
    DrawRect(x + 2, y + 2, (barWidth - 4) * fillRatio, barHeight - 4, ammoColor);
}

void HUDRenderer::DrawHealthBar(float health, float maxHealth, float x, float y, float width, float height) {
    // Arkaplan
    DrawRect(x, y, width, height, glm::vec4(0.2f, 0.2f, 0.2f, 0.8f));
    
    // Can cubugu
    float fillRatio = maxHealth > 0.0f ? std::clamp(health / maxHealth, 0.0f, 1.0f) : 0.0f;
    glm::vec4 healthColor;
    
    if (fillRatio > 0.6f) healthColor = glm::vec4(0.2f, 0.8f, 0.2f, 0.9f); // Yesil
    else if (fillRatio > 0.3f) healthColor = glm::vec4(0.8f, 0.8f, 0.2f, 0.9f); // Sari
    else healthColor = glm::vec4(0.8f, 0.2f, 0.2f, 0.9f); // Kirmizi
    
    DrawRect(x + 2, y + 2, (width - 4) * fillRatio, height - 4, healthColor);
}

void HUDRenderer::SetScreenSize(float width, float height) {
    m_ScreenWidth = width;
    m_ScreenHeight = height;
}

void HUDRenderer::CreateQuadMesh() {
    if (!RenderThread::IsCurrent()) return;
    // Basit dortgen (ekran uzayi koordinatlari 0-1)
    float vertices[] = {
        // Poz      // TexCoords
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f
    };
    
    unsigned int indices[] = {
        0, 1, 2,
        0, 3, 1
    };
    
    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glGenBuffers(1, &m_QuadEBO);
    
    glBindVertexArray(m_QuadVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // TexCoords
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
}

} // namespace Archura
