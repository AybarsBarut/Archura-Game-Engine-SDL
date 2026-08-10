#pragma once

#include <string>
#include <memory>
#include <glm/glm.hpp>

namespace Archura {

class Shader;
class Texture;

/**
 * @brief HUD Renderer - 2D overlay rendering (crosshair, ammo, health, etc.)
 */
class HUDRenderer {
public:
    HUDRenderer();
    ~HUDRenderer();

    bool Init();
    void Shutdown();

    // HUD rendering
    void BeginHUD(float width, float height);
    void EndHUD();

    // Primitive shapes
    void DrawRect(float x, float y, float width, float height, const glm::vec4& color);
    void DrawTexture(Texture* texture, float x, float y, float width, float height);
    
    // HUD elements
    void DrawCrosshair(float size = 20.0f, const glm::vec4& color = glm::vec4(1.0f));
    void DrawAmmoCounter(int current, int total, float x, float y);
    void DrawHealthBar(float health, float maxHealth, float x, float y, float width, float height);

    void SetScreenSize(float width, float height);

private:
    void CreateQuadMesh();

private:
    std::unique_ptr<Shader> m_HUDShader;
    unsigned int m_QuadVAO, m_QuadVBO, m_QuadEBO;
    float m_ScreenWidth, m_ScreenHeight;
    int m_PreviousProgram = 0;
    int m_PreviousVAO = 0;
    int m_PreviousActiveTexture = 0;
    int m_PreviousTexture0 = 0;
    int m_BlendSrcRGB = 0, m_BlendDstRGB = 0;
    int m_BlendSrcAlpha = 0, m_BlendDstAlpha = 0;
    int m_BlendEquationRGB = 0, m_BlendEquationAlpha = 0;
    bool m_DepthWasEnabled = false;
    bool m_CullWasEnabled = false;
    bool m_BlendWasEnabled = false;
    bool m_InHUDPass = false;
};

} // namespace Archura
