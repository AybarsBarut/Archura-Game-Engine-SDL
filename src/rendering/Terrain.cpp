#include "Terrain.h"
#include "Shader.h"
#include "../core/Logger.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <algorithm>
#include <cmath>

namespace Archura {

// ---------------------------------------------------------------------------
Terrain::~Terrain() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
}

// ---------------------------------------------------------------------------
Terrain* Terrain::CreateFlat(float widthWorld, float depthWorld, int gridSize) {
    auto* t = new Terrain();
    t->m_WorldWidth = widthWorld;
    t->m_WorldDepth = depthWorld;
    t->m_MaxHeight  = 0.0f;
    t->m_MinHeight  = 0.0f;
    t->m_GridSize   = gridSize;

    // Tum yukseklikler sifir
    int total = gridSize * gridSize;
    std::vector<float> heights(total, 0.0f);

    t->BuildMesh(heights, gridSize, gridSize);

    // Shader yükle
    t->m_Shader = std::make_unique<Shader>();
    if (!t->m_Shader->LoadFromFile("assets/shaders/terrain.vert",
                                    "assets/shaders/terrain.frag")) {
        ARCH_LOG_ERROR("[Terrain] terrain shader yuklenemedi!");
    }

    ARCH_LOG_INFO("[Terrain] Duz arazi olusturuldu. Grid=" + std::to_string(gridSize)
                  + " Width=" + std::to_string(widthWorld)
                  + " Depth=" + std::to_string(depthWorld));
    return t;
}

// ---------------------------------------------------------------------------
Terrain* Terrain::LoadFromHeightmap(const std::string& heightmapPath,
                                     float widthWorld, float depthWorld, float maxHeight) {
    auto* t = new Terrain();
    t->m_WorldWidth = widthWorld;
    t->m_WorldDepth = depthWorld;
    t->m_MaxHeight  = maxHeight;
    t->m_MinHeight  = 0.0f;

    // STB ile greyscale goruntu yukle
    int w, h, channels;
    stbi_set_flip_vertically_on_load_thread(0);
    unsigned char* data = stbi_load(heightmapPath.c_str(), &w, &h, &channels, 1); // 1 kanal = greyscale

    if (!data) {
        ARCH_LOG_ERROR("[Terrain] Heightmap yuklenemedi: " + heightmapPath
                       + " — Duz arazi olusturuluyor.");
        return CreateFlat(widthWorld, depthWorld, 64);
    }

    t->m_GridSize = std::min(w, h); // Kare olarak kes

    std::vector<float> heights(w * h);
    for (int i = 0; i < w * h; ++i) {
        heights[i] = (data[i] / 255.0f) * maxHeight;
    }
    stbi_image_free(data);

    t->BuildMesh(heights, w, h);

    t->m_Shader = std::make_unique<Shader>();
    if (!t->m_Shader->LoadFromFile("assets/shaders/terrain.vert",
                                    "assets/shaders/terrain.frag")) {
        ARCH_LOG_ERROR("[Terrain] terrain shader yuklenemedi!");
    }

    ARCH_LOG_INFO("[Terrain] Heightmap'ten arazi olusturuldu: " + heightmapPath);
    return t;
}

// ---------------------------------------------------------------------------
void Terrain::BuildMesh(const std::vector<float>& heights, int cols, int rows) {
    m_Heights.assign(heights.begin(), heights.end());

    // Min/Max hesapla
    if (!heights.empty()) {
        m_MinHeight = *std::min_element(heights.begin(), heights.end());
        m_MaxHeight = *std::max_element(heights.begin(), heights.end());
    }

    // Vertex yapisi: position(3) + normal(3) + texcoord(2)
    // Toplam float sayisi: cols*rows * 8
    std::vector<float> verts;
    verts.reserve(cols * rows * 8);

    float cellW = m_WorldWidth / (cols - 1);
    float cellD = m_WorldDepth / (rows - 1);

    auto heightAt = [&](int c, int r) -> float {
        c = std::clamp(c, 0, cols - 1);
        r = std::clamp(r, 0, rows - 1);
        return heights[r * cols + c];
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = c * cellW - m_WorldWidth * 0.5f;
            float y = heightAt(c, r);
            float z = r * cellD - m_WorldDepth * 0.5f;

            // Normal: komsu vertex farkindan yuzey normali
            float hL = heightAt(c - 1, r);
            float hR = heightAt(c + 1, r);
            float hD = heightAt(c, r - 1);
            float hU = heightAt(c, r + 1);
            glm::vec3 nx = glm::normalize(glm::vec3(hL - hR, 2.0f * cellW, hD - hU));

            float u = (float)c / (cols - 1);
            float v = (float)r / (rows - 1);

            // Pozisyon
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
            // Normal
            verts.push_back(nx.x); verts.push_back(nx.y); verts.push_back(nx.z);
            // TexCoord
            verts.push_back(u); verts.push_back(v);
        }
    }

    // Indeksler (quad -> 2 ucgen)
    std::vector<unsigned int> indices;
    indices.reserve((cols - 1) * (rows - 1) * 6);
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            unsigned int tl = r * cols + c;
            unsigned int tr = tl + 1;
            unsigned int bl = (r + 1) * cols + c;
            unsigned int br = bl + 1;
            // Ucgen 1
            indices.push_back(tl); indices.push_back(bl); indices.push_back(tr);
            // Ucgen 2
            indices.push_back(tr); indices.push_back(bl); indices.push_back(br);
        }
    }
    m_IndexCount = static_cast<unsigned int>(indices.size());

    // GPU'ya yukle
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    int stride = 8 * sizeof(float);
    // Pos (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // Normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    // TexCoord (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
void Terrain::Draw(const glm::mat4& view,
                   const glm::mat4& projection,
                   const glm::mat4& lightSpaceMatrix,
                   unsigned int     shadowMapTexId,
                   const glm::vec3& lightDir,
                   const glm::vec3& lightColor,
                   float            lightIntensity,
                   const glm::vec3& viewPos) {
    if (!m_Shader || m_VAO == 0) return;

    m_Shader->Bind();

    // Transform
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::scale(model, scale);

    m_Shader->SetMat4("uModel", model);
    m_Shader->SetMat4("uView", view);
    m_Shader->SetMat4("uProjection", projection);
    m_Shader->SetMat4("uLightSpaceMatrix", lightSpaceMatrix);

    // Isik
    m_Shader->SetVec3("uLightDir", lightDir);
    m_Shader->SetVec3("uLightColor", lightColor);
    m_Shader->SetFloat("uLightIntensity", lightIntensity);
    m_Shader->SetVec3("uViewPos", viewPos);

    // Yukseklik bilgisi (doku karismasi icin)
    m_Shader->SetFloat("uMinHeight", m_MinHeight + position.y);
    m_Shader->SetFloat("uMaxHeight", m_MaxHeight + position.y);
    m_Shader->SetFloat("uTexScale", m_TexScale);

    // Doku birimlerine atama
    auto bindTex = [](unsigned int id, int unit, unsigned int fallback) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, id ? id : fallback);
    };

    // Yedek: varsayilan beyaz doku (OpenGL 0 ID = gecersiz)
    // Eger doku yoksa sadece birim renk kullaniriz
    bindTex(m_TexGrass, 0, 0);
    bindTex(m_TexRock,  1, 0);
    bindTex(m_TexSnow,  2, 0);

    m_Shader->SetInt("uTexGrass", 0);
    m_Shader->SetInt("uTexRock",  1);
    m_Shader->SetInt("uTexSnow",  2);

    // Golge haritasi
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowMapTexId);
    m_Shader->SetInt("uShadowMap", 3);

    // Ciz
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    m_Shader->Unbind();
}

// ---------------------------------------------------------------------------
float Terrain::GetHeightAt(float worldX, float worldZ) const {
    if (m_Heights.empty()) return 0.0f;

    // Dunya koordinatini grid koordinatina cevir
    float localX = worldX - position.x + m_WorldWidth  * 0.5f;
    float localZ = worldZ - position.z + m_WorldDepth  * 0.5f;

    int cols = m_GridSize;
    int rows = (int)m_Heights.size() / cols;

    float cellW = m_WorldWidth / (cols - 1);
    float cellD = m_WorldDepth / (rows - 1);

    float gc = localX / cellW;
    float gr = localZ / cellD;

    int c0 = std::clamp((int)gc, 0, cols - 2);
    int r0 = std::clamp((int)gr, 0, rows - 2);
    int c1 = c0 + 1;
    int r1 = r0 + 1;

    float tx = gc - c0;
    float tz = gr - r0;

    // Bilinear interpolasyon
    float h00 = m_Heights[r0 * cols + c0];
    float h10 = m_Heights[r0 * cols + c1];
    float h01 = m_Heights[r1 * cols + c0];
    float h11 = m_Heights[r1 * cols + c1];

    float h0 = h00 + tx * (h10 - h00);
    float h1 = h01 + tx * (h11 - h01);
    return (h0 + tz * (h1 - h0)) * scale.y + position.y;
}

} // namespace Archura
