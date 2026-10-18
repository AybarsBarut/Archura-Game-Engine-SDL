#include "Terrain.h"
#include "Shader.h"
#include "RenderThread.h"
#include "../core/Logger.h"
#include "../core/ResourceManager.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <algorithm>
#include <cmath>

namespace Archura {

// ---------------------------------------------------------------------------
Terrain::~Terrain() {
    if (RenderThread::IsCurrent()) {
        if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
        if (m_VBO) glDeleteBuffers(1, &m_VBO);
        if (m_EBO) glDeleteBuffers(1, &m_EBO);
    }
    m_VAO = 0;
    m_VBO = 0;
    m_EBO = 0;
}

// ---------------------------------------------------------------------------
Terrain* Terrain::CreateFlat(float widthWorld, float depthWorld, int gridSize) {
    gridSize = std::max(2, gridSize);
    auto* t = new Terrain();
    t->m_WorldWidth = widthWorld;
    t->m_WorldDepth = depthWorld;
    t->m_MaxHeight  = 0.0f;
    t->m_MinHeight  = 0.0f;
    t->m_GridSize   = gridSize;
    t->m_Cols       = gridSize;
    t->m_Rows       = gridSize;

    // Tum yukseklikler sifir
    const int total = gridSize * gridSize;
    std::vector<float> heights(static_cast<size_t>(total), 0.0f);

    t->BuildMesh(heights, gridSize, gridSize);

    // Shader yükle (sadece aktif OpenGL context/render thread varsa)
    if (RenderThread::IsCurrent()) {
        t->m_Shader = std::make_unique<Shader>();
        if (!t->m_Shader->LoadFromFile("assets/shaders/terrain.vert",
                                        "assets/shaders/terrain.frag")) {
            ARCH_LOG_ERROR("[Terrain] terrain shader yuklenemedi!");
        }
    }

    ARCH_LOG_INFO("[Terrain] Duz arazi olusturuldu. Grid=" + std::to_string(gridSize)
                  + " Width=" + std::to_string(widthWorld)
                  + " Depth=" + std::to_string(depthWorld));
    return t;
}

// ---------------------------------------------------------------------------
Terrain* Terrain::LoadFromHeightmap(const std::string& heightmapPath,
                                     float widthWorld, float depthWorld, float maxHeight) {
    const std::filesystem::path resolvedPath =
        ResourceManager::Get().ResolveAssetPath(heightmapPath);
    const std::string loadPath =
        resolvedPath.empty() ? heightmapPath : resolvedPath.string();

    // STB ile greyscale goruntu yukle (Terrain nesnesini basarili yuklemeden once tahsis etme!)
    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load_thread(0);
    unsigned char* data = stbi_load(loadPath.c_str(), &w, &h, &channels, 1); // 1 kanal = greyscale

    if (!data || w < 2 || h < 2) {
        if (data) {
            stbi_image_free(data);
        }
        ARCH_LOG_ERROR("[Terrain] Heightmap yuklenemedi: " + heightmapPath
                       + " — Duz arazi olusturuluyor.");
        return CreateFlat(widthWorld, depthWorld, 64);
    }

    auto* t = new Terrain();
    t->m_WorldWidth = widthWorld;
    t->m_WorldDepth = depthWorld;
    t->m_MaxHeight  = maxHeight;
    t->m_MinHeight  = 0.0f;
    t->m_Cols       = w;
    t->m_Rows       = h;
    t->m_GridSize   = std::min(w, h);

    std::vector<float> heights(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int i = 0; i < w * h; ++i) {
        heights[static_cast<size_t>(i)] = (data[i] / 255.0f) * maxHeight;
    }
    stbi_image_free(data);

    t->BuildMesh(heights, w, h);

    if (RenderThread::IsCurrent()) {
        t->m_Shader = std::make_unique<Shader>();
        if (!t->m_Shader->LoadFromFile("assets/shaders/terrain.vert",
                                        "assets/shaders/terrain.frag")) {
            ARCH_LOG_ERROR("[Terrain] terrain shader yuklenemedi!");
        }
    }

    ARCH_LOG_INFO("[Terrain] Heightmap'ten arazi olusturuldu: " + heightmapPath);
    return t;
}

// ---------------------------------------------------------------------------
void Terrain::BuildMesh(const std::vector<float>& heights, int cols, int rows) {
    cols = std::max(2, cols);
    rows = std::max(2, rows);
    m_Cols = cols;
    m_Rows = rows;
    m_GridSize = std::min(cols, rows);
    m_Heights.assign(heights.begin(), heights.end());

    // Min/Max hesapla
    if (!heights.empty()) {
        m_MinHeight = *std::min_element(heights.begin(), heights.end());
        m_MaxHeight = *std::max_element(heights.begin(), heights.end());
    }

    // Vertex yapisi: position(3) + normal(3) + texcoord(2)
    // Toplam float sayisi: cols*rows * 8
    std::vector<float> verts;
    verts.reserve(static_cast<size_t>(cols) * static_cast<size_t>(rows) * 8u);

    const float cellW = m_WorldWidth / static_cast<float>(cols - 1);
    const float cellD = m_WorldDepth / static_cast<float>(rows - 1);

    auto heightAt = [&](int c, int r) -> float {
        c = std::clamp(c, 0, cols - 1);
        r = std::clamp(r, 0, rows - 1);
        return heights[static_cast<size_t>(r) * static_cast<size_t>(cols) + static_cast<size_t>(c)];
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

            float u = static_cast<float>(c) / static_cast<float>(cols - 1);
            float v = static_cast<float>(r) / static_cast<float>(rows - 1);

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
    indices.reserve(static_cast<size_t>(cols - 1) * static_cast<size_t>(rows - 1) * 6u);
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            unsigned int tl = static_cast<unsigned int>(r * cols + c);
            unsigned int tr = tl + 1;
            unsigned int bl = static_cast<unsigned int>((r + 1) * cols + c);
            unsigned int br = bl + 1;
            // Ucgen 1
            indices.push_back(tl); indices.push_back(bl); indices.push_back(tr);
            // Ucgen 2
            indices.push_back(tr); indices.push_back(bl); indices.push_back(br);
        }
    }
    m_IndexCount = static_cast<unsigned int>(indices.size());

    if (!RenderThread::IsCurrent()) {
        return;
    }

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
    if (!RenderThread::IsCurrent() || !m_Shader || m_VAO == 0) return;

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
    m_Shader->SetFloat("uMinHeight", m_MinHeight * scale.y + position.y);
    m_Shader->SetFloat("uMaxHeight", m_MaxHeight * scale.y + position.y);
    m_Shader->SetFloat("uTexScale", m_TexScale);

    // Doku birimlerine atama
    auto bindTex = [](unsigned int id, int unit, unsigned int fallback) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, id ? id : fallback);
    };

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
    const int cols = m_Cols;
    const int rows = m_Rows;
    if (m_Heights.empty() || cols < 2 || rows < 2 ||
        static_cast<size_t>(cols) * static_cast<size_t>(rows) > m_Heights.size()) {
        return 0.0f;
    }
    if (std::abs(scale.x) <= 1.0e-6f || std::abs(scale.z) <= 1.0e-6f ||
        m_WorldWidth <= 1.0e-6f || m_WorldDepth <= 1.0e-6f) {
        return 0.0f;
    }

    // Dunya koordinatini yatay olcek (scale.x, scale.z) dikkate alarak yerel arazi uzayina cevir
    const float unscaledX = (worldX - position.x) / scale.x;
    const float unscaledZ = (worldZ - position.z) / scale.z;
    const float localX = unscaledX + m_WorldWidth * 0.5f;
    const float localZ = unscaledZ + m_WorldDepth * 0.5f;

    // Arazi siniri disindaysa 0.0f doner
    if (localX < 0.0f || localX > m_WorldWidth ||
        localZ < 0.0f || localZ > m_WorldDepth) {
        return 0.0f;
    }

    const float cellW = m_WorldWidth / static_cast<float>(cols - 1);
    const float cellD = m_WorldDepth / static_cast<float>(rows - 1);

    const float gc = std::clamp(localX / cellW, 0.0f, static_cast<float>(cols - 1));
    const float gr = std::clamp(localZ / cellD, 0.0f, static_cast<float>(rows - 1));

    const int c0 = std::clamp(static_cast<int>(gc), 0, cols - 2);
    const int r0 = std::clamp(static_cast<int>(gr), 0, rows - 2);
    const int c1 = c0 + 1;
    const int r1 = r0 + 1;

    const float tx = std::clamp(gc - static_cast<float>(c0), 0.0f, 1.0f);
    const float tz = std::clamp(gr - static_cast<float>(r0), 0.0f, 1.0f);

    // Bilinear interpolasyon
    const float h00 = m_Heights[static_cast<size_t>(r0) * static_cast<size_t>(cols) + static_cast<size_t>(c0)];
    const float h10 = m_Heights[static_cast<size_t>(r0) * static_cast<size_t>(cols) + static_cast<size_t>(c1)];
    const float h01 = m_Heights[static_cast<size_t>(r1) * static_cast<size_t>(cols) + static_cast<size_t>(c0)];
    const float h11 = m_Heights[static_cast<size_t>(r1) * static_cast<size_t>(cols) + static_cast<size_t>(c1)];

    const float h0 = h00 + tx * (h10 - h00);
    const float h1 = h01 + tx * (h11 - h01);
    return (h0 + tz * (h1 - h0)) * scale.y + position.y;
}

} // namespace Archura
