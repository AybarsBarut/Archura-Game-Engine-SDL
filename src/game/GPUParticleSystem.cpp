#include "GPUParticleSystem.h"
#include "../core/Logger.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <algorithm>
#include <cmath>

namespace Archura {

// ---------------------------------------------------------------------------
// Billboard quad: iki ucgen, XY duzleminde -0.5..0.5
// ---------------------------------------------------------------------------
static const float s_QuadVertices[] = {
    // position (x,y,0)   texcoord
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
     0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
     0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
    -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
};

// ---------------------------------------------------------------------------
GPUParticleSystem::~GPUParticleSystem() {
    Shutdown();
}

void GPUParticleSystem::Init(int maxParticles) {
    if (m_Initialized)
        Shutdown();
    if (maxParticles <= 0) {
        ARCH_LOG_ERROR("[GPUParticleSystem] maxParticles must be positive");
        return;
    }
    m_MaxParticles = maxParticles;

    // Shader yükle
    m_Shader = std::make_unique<Shader>();
    if (!m_Shader->LoadFromFile("assets/shaders/particle.vert",
                                "assets/shaders/particle.frag")) {
        ARCH_LOG_ERROR("[GPUParticleSystem] Parcacik shader yuklenemedi!");
        return;
    }

    // Quad VAO / VBO
    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glBindVertexArray(m_QuadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_QuadVertices), s_QuadVertices, GL_STATIC_DRAW);

    // location 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // location 2: texcoord (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // Instance VBO (boslukla olustur, her frame guncellenir)
    glGenBuffers(1, &m_InstVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_InstVBO);
    glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(ParticleInstance), nullptr, GL_DYNAMIC_DRAW);

    // location 4: position (vec3) — instance
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, position));
    glVertexAttribDivisor(4, 1);

    // location 5: color (vec4) — instance
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, color));
    glVertexAttribDivisor(5, 1);

    // location 6: size (float) — instance
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, size));
    glVertexAttribDivisor(6, 1);

    // location 7: lifeFrac (float) — instance
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, lifeFrac));
    glVertexAttribDivisor(7, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_GPUData.reserve(maxParticles);
    m_CPUData.reserve(maxParticles);
    m_Alive.reserve(maxParticles);
    m_FreeIndices.reserve(maxParticles);

    m_Initialized = true;
    ARCH_LOG_INFO("[GPUParticleSystem] Baslatildi. MaxParticles=" + std::to_string(maxParticles));
}

// ---------------------------------------------------------------------------
static std::mt19937 s_Rng{std::random_device{}()};
static float RandomFloat(float lo, float hi) {
    return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(s_Rng);
}

void GPUParticleSystem::Emit(const EmitterConfig& cfg) {
    if (!m_Initialized) return;

    for (int i = 0; i < cfg.count; ++i) {
        int index = -1;
        if (!m_FreeIndices.empty()) {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
        } else {
            if (static_cast<int>(m_GPUData.size()) >= m_MaxParticles) break;
            index = static_cast<int>(m_GPUData.size());
            m_GPUData.emplace_back();
            m_CPUData.emplace_back();
        }

        // Rastgele yayilim yonu (normal etrafinda)
        glm::vec3 rnd = glm::normalize(glm::vec3(
            RandomFloat(-1.0f, 1.0f),
            RandomFloat(-1.0f, 1.0f),
            RandomFloat(-1.0f, 1.0f)
        ));
        if (glm::dot(rnd, cfg.normal) < 0.0f) rnd = -rnd;
        glm::vec3 dir = glm::normalize(cfg.normal * 0.6f + rnd * 0.4f);

        float speed    = cfg.speed + RandomFloat(-cfg.speedVariance, cfg.speedVariance);
        float lifetime = cfg.lifetime + RandomFloat(-cfg.lifetimeVariance, cfg.lifetimeVariance);
        lifetime = glm::max(lifetime, 0.1f);

        ParticleInstance inst;
        inst.position = cfg.position;
        inst.color    = cfg.colorStart;
        inst.size     = cfg.size;
        inst.lifeFrac = 1.0f;

        ParticleCPU cpu;
        cpu.velocity      = dir * speed;
        cpu.acceleration  = cfg.useGravity ? glm::vec3(0.0f, -9.81f, 0.0f) : glm::vec3(0.0f);
        cpu.lifetime      = lifetime;
        cpu.startLifetime = lifetime;
        cpu.alive         = true;

        m_GPUData[static_cast<size_t>(index)] = inst;
        m_CPUData[static_cast<size_t>(index)] = cpu;
        m_Alive.push_back(index);
    }
}

void GPUParticleSystem::Update(float deltaTime) {
    if (!m_Initialized) return;

    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

    for (size_t aliveIndex = 0; aliveIndex < m_Alive.size();) {
        const int idx = m_Alive[aliveIndex];
        auto& cpu = m_CPUData[static_cast<size_t>(idx)];
        auto& gpu = m_GPUData[static_cast<size_t>(idx)];

        cpu.lifetime -= deltaTime;
        if (cpu.lifetime <= 0.0f) {
            cpu.alive = false;
            m_FreeIndices.push_back(idx);
            m_Alive[aliveIndex] = m_Alive.back();
            m_Alive.pop_back();
            continue;
        }

        // Fizik
        cpu.velocity += cpu.acceleration * deltaTime;
        gpu.position += cpu.velocity * deltaTime;

        // Yasam orani
        gpu.lifeFrac = glm::clamp(cpu.lifetime / cpu.startLifetime, 0.0f, 1.0f);

        ++aliveIndex;
    }
}

void GPUParticleSystem::UploadToGPU() {
    if (m_Alive.empty()) return;

    // Sadece yasayan parcaciklarin verisini sirastir
    std::vector<ParticleInstance> upload;
    upload.reserve(m_Alive.size());
    for (int idx : m_Alive) {
        upload.push_back(m_GPUData[idx]);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_InstVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    upload.size() * sizeof(ParticleInstance),
                    upload.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GPUParticleSystem::Render(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_Initialized || m_Alive.empty() || !m_Shader) return;

    UploadToGPU();

    // Alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Depth yazmayı durdur (parcaciklar birbirini karartmasin)

    m_Shader->Bind();
    m_Shader->SetMat4("uView", view);
    m_Shader->SetMat4("uProjection", projection);
    m_Shader->SetInt("uUseTexture", 0);

    glBindVertexArray(m_QuadVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(m_Alive.size()));
    glBindVertexArray(0);

    // State'i geri al
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    m_Shader->Unbind();
}

void GPUParticleSystem::Shutdown() {
    if (m_QuadVAO) { glDeleteVertexArrays(1, &m_QuadVAO); m_QuadVAO = 0; }
    if (m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }
    if (m_InstVBO) { glDeleteBuffers(1, &m_InstVBO); m_InstVBO = 0; }
    m_Shader.reset();
    m_GPUData.clear();
    m_CPUData.clear();
    m_Alive.clear();
    m_FreeIndices.clear();
    m_Initialized = false;
}

} // namespace Archura
