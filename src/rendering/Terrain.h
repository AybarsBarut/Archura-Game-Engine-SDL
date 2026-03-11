#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace Archura {

class Mesh;
class Shader;
class Texture;

/**
 * @brief Heightmap tabanli ya da prosedurel arazi sinifi.
 *
 * Kullanim ornekleri:
 * @code
 *   // 1. Prosedurel duz arazi:
 *   auto* t = Terrain::CreateFlat(200.0f, 200.0f, 64);
 *
 *   // 2. Heightmap'ten:
 *   auto* t = Terrain::LoadFromHeightmap("assets/textures/heightmap.png",
 *                                         200.0f, 200.0f, 30.0f);
 *
 *   // 3. Her frame ciz:
 *   t->Draw(shader, view, projection, lightSpaceMatrix, shadowMapTexId);
 *
 *   // 4. Belli bir dunya pozisyonunun yuksekligini sorgu:
 *   float h = t->GetHeightAt(worldX, worldZ);
 * @endcode
 */
class Terrain {
public:
    Terrain() = default;
    ~Terrain();

    // Kopya yasak (GPU kaynaklar icin)
    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    // ── Factory metodlar ────────────────────────────────────────────────────

    /**
     * @brief Tamamen duz, grid tabanli arazi olustur.
     * @param widthWorld  Dunya uzayinda X boyutu (metre)
     * @param depthWorld  Dunya uzayinda Z boyutu (metre)
     * @param gridSize    Her eksende vertex sayisi (daha buyuk = daha detayli)
     */
    static Terrain* CreateFlat(float widthWorld, float depthWorld, int gridSize = 64);

    /**
     * @brief Gri tonlu PNG heightmap'ten arazi olustur.
     * @param heightmapPath  Greyscale PNG dosya yolu
     * @param widthWorld     Dunya uzayinda X boyutu
     * @param depthWorld     Dunya uzayinda Z boyutu
     * @param maxHeight      En beyaz pikselin dunya yuksekligi (metre)
     */
    static Terrain* LoadFromHeightmap(const std::string& heightmapPath,
                                       float widthWorld  = 200.0f,
                                       float depthWorld  = 200.0f,
                                       float maxHeight   = 30.0f);

    // ── Render ──────────────────────────────────────────────────────────────

    /**
     * @brief Araziyi ciz. Shader bind/unbind bu metod icinde yapilir.
     * @param view             Kamera view matrisi
     * @param projection       Projeksiyon matrisi
     * @param lightSpaceMatrix Golge matrisi (shadow map icin)
     * @param shadowMapTexId   Shadow map texture ID'si (0 ise golge yok)
     */
    void Draw(const glm::mat4& view,
              const glm::mat4& projection,
              const glm::mat4& lightSpaceMatrix,
              unsigned int     shadowMapTexId,
              const glm::vec3& lightDir      = glm::vec3(-0.5f, -1.0f, -0.5f),
              const glm::vec3& lightColor    = glm::vec3(1.0f),
              float            lightIntensity = 1.0f,
              const glm::vec3& viewPos        = glm::vec3(0.0f));

    // ── Doku atama ──────────────────────────────────────────────────────────
    void SetGrassTexture(unsigned int texId) { m_TexGrass = texId; }
    void SetRockTexture (unsigned int texId) { m_TexRock  = texId; }
    void SetSnowTexture (unsigned int texId) { m_TexSnow  = texId; }
    void SetTextureScale(float scale)         { m_TexScale = scale; }

    // ── Yukseklik sorgulama ─────────────────────────────────────────────────

    /**
     * @brief Verilen dunya X,Z koordinatindaki arazinin yuksekligini dondur.
     *        Arazi siniri disindaysa 0.0f doner.
     */
    float GetHeightAt(float worldX, float worldZ) const;

    // ── Bilgi ───────────────────────────────────────────────────────────────
    float GetWidth()     const { return m_WorldWidth;  }
    float GetDepth()     const { return m_WorldDepth;  }
    float GetMaxHeight() const { return m_MaxHeight;   }
    int   GetGridSize()  const { return m_GridSize;    }

    // Dunyada konumlamak icin transform
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale    = glm::vec3(1.0f);

private:
    void BuildMesh(const std::vector<float>& heights, int cols, int rows);
    void SetupGPU();

    // Geometri
    std::vector<float>        m_Heights;  // Ham yukseklik verileri (cols*rows)
    int                       m_GridSize = 64;
    float                     m_WorldWidth = 100.0f;
    float                     m_WorldDepth = 100.0f;
    float                     m_MaxHeight  = 20.0f;
    float                     m_MinHeight  = 0.0f;

    // OpenGL
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;
    unsigned int m_IndexCount = 0;

    // Dokular
    unsigned int m_TexGrass = 0;
    unsigned int m_TexRock  = 0;
    unsigned int m_TexSnow  = 0;
    float        m_TexScale = 20.0f;

    // Shader
    std::unique_ptr<Shader> m_Shader;
};

} // namespace Archura
