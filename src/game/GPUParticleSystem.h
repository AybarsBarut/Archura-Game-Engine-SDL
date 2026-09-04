#pragma once

#include "../ecs/System.h"
#include "../rendering/Shader.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Archura {

/**
 * @brief GPU'ya siddet yuklenebilen tek bir parcacigin CPU tarafindaki verisi.
 *        Shader'a instanced olarak gonderilir.
 */
struct ParticleInstance {
    glm::vec3 position;   // layout 4
    float     _pad0 = 0;  // std140 hizalama
    glm::vec4 color;      // layout 5 (rgba + initial alpha)
    float     size;       // layout 6
    float     lifeFrac;   // layout 7 — [0..1] kalan omur orani
    float     _pad1 = 0;
    float     _pad2 = 0;
};

/**
 * @brief CPU tarafindaki parcacik fizik verisi (GPU'ya gitmez).
 */
struct ParticleCPU {
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float     lifetime;       // saniye cinsinden kalan omur
    float     startLifetime;  // baslangic omru (lifeFrac hesaplamak icin)
    bool      alive = true;
};

/**
 * @brief Emitter konfigurasyonu - tek bir patlama veya surekli yayin icin.
 */
struct EmitterConfig {
    glm::vec3 position    = glm::vec3(0.0f);
    glm::vec3 normal      = glm::vec3(0.0f, 1.0f, 0.0f); // Yayilim yonu
    glm::vec4 colorStart  = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f); // Turuncu
    glm::vec4 colorEnd    = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);  // Kirmizi + seffaf
    float     speed       = 5.0f;
    float     speedVariance = 2.0f;
    float     size        = 0.15f;
    float     lifetime    = 1.5f;
    float     lifetimeVariance = 0.5f;
    bool      useGravity  = true;
    int       count       = 50;  // Bir seferde kac parcacik
};

/**
 * @brief GPU Instancing tabanli parcacik sistemi.
 *
 * - CPU physics guncelleme: velocity/gravity/lifetime
 * - GPU: tek DrawArraysInstanced cagrisi ile tum parcaciklar
 * - Billboard vertex shader ile her parcacik ekrana bakar
 *
 * Kullanimi:
 * @code
 *   gpuParticles.Init();
 *   gpuParticles.Emit(config);
 *   // Her frame:
 *   gpuParticles.Update(dt);
 *   gpuParticles.Render(view, proj);
 * @endcode
 */
class GPUParticleSystem {
public:
    GPUParticleSystem() = default;
    ~GPUParticleSystem();

    /// OpenGL kaynaklarini hazirla (Init oncesi cagirilmamali)
    void Init(int maxParticles = 5000);

    /// Parcacik sahnesine yeni parcaciklar ekle
    void Emit(const EmitterConfig& config);

    /// Fizik guncellemesi (lifetime, velocity, gravity)
    void Update(float deltaTime);

    /// GPU'ya yukleme ve cizim
    void Render(const glm::mat4& view, const glm::mat4& projection);

    void Shutdown();

    int GetAliveCount() const { return static_cast<int>(m_Alive.size()); }
    int GetMaxParticles() const { return m_MaxParticles; }

private:
    void UploadToGPU();

    int    m_MaxParticles = 5000;
    bool   m_Initialized  = false;

    // CPU verisi
    std::vector<ParticleInstance> m_GPUData;   // GPU'ya gidecek buffer
    std::vector<ParticleCPU>      m_CPUData;   // Fizik icin CPU yan verisi
    std::vector<int>              m_Alive;      // Yasayan parcacik indeksleri (pool)
    std::vector<int>              m_FreeIndices; // Reusable dead-particle slots

    // OpenGL kaynaklar
    unsigned int m_QuadVAO    = 0;
    unsigned int m_QuadVBO    = 0; // Quad geometrisi
    unsigned int m_InstVBO    = 0; // Instance verisi
    
    std::unique_ptr<Shader> m_Shader;
};

} // namespace Archura
