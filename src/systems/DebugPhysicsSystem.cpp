#include "DebugPhysicsSystem.h"

#include "../ecs/Component.h"
#include "../ecs/Entity.h"
#include "../game/FPSController.h"
#include "../game/PhysicsSystem.h"
#include "../rendering/Shader.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <array>
#include <iostream>

namespace Archura {

DebugPhysicsSystem::~DebugPhysicsSystem() { Shutdown(); }

void DebugPhysicsSystem::Init(Scene* scene) {
    System::Init(scene);

    m_Shader = std::make_unique<Shader>();
    if (!m_Shader->LoadFromFile("assets/shaders/debug_wireframe.vert",
                                "assets/shaders/debug_wireframe.frag")) {
        std::cerr << "[DebugPhysics] Failed to load debug wireframe shader"
                  << std::endl;
    }

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(offsetof(LineVertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(offsetof(LineVertex, color)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void DebugPhysicsSystem::Update(float deltaTime) { (void)deltaTime; }

void DebugPhysicsSystem::Shutdown() {
    if (m_Enabled) {
        RestorePlayerState();
        if (m_PhysicsSystem) {
            m_PhysicsSystem->SetDebugDraw(false);
        }
        m_Enabled = false;
    }

    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    m_Shader.reset();
}

void DebugPhysicsSystem::SetDependencies(FPSController* fpsController,
                                         PhysicsSystem* physicsSystem) {
    m_FPSController = fpsController;
    m_PhysicsSystem = physicsSystem;
}

void DebugPhysicsSystem::Toggle() { SetEnabled(!m_Enabled); }

void DebugPhysicsSystem::SetEnabled(bool enabled) {
    if (enabled == m_Enabled) {
        return;
    }

    if (enabled) {
        CapturePlayerState();
        if (m_FPSController) {
            m_FPSController->SetNoclipEnabled(true);
        }
        if (m_PhysicsSystem) {
            m_PhysicsSystem->SetDebugDraw(true);
        }
        std::cout << "[DebugPhysics] ON: player noclip enabled" << std::endl;
    } else {
        RestorePlayerState();
        if (m_PhysicsSystem) {
            m_PhysicsSystem->SetDebugDraw(false);
        }
        std::cout << "[DebugPhysics] OFF: player physics restored" << std::endl;
    }

    m_Enabled = enabled;
}

void DebugPhysicsSystem::Render(const glm::mat4& view,
                                const glm::mat4& projection) {
    if (!m_Enabled || !m_Scene || !m_Shader || m_Shader->GetProgramID() == 0 ||
        m_VAO == 0 || m_VBO == 0) {
        return;
    }

    std::vector<LineVertex> vertices;
    BuildColliderLines(vertices);
    if (vertices.empty()) {
        return;
    }

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;
    GLfloat previousLineWidth = 1.0f;
    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    glGetFloatv(GL_LINE_WIDTH, &previousLineWidth);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(1.5f);

    m_Shader->Bind();
    m_Shader->SetMat4("uView", view);
    m_Shader->SetMat4("uProjection", projection);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(LineVertex)),
                 vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));

    glBindBuffer(GL_ARRAY_BUFFER, static_cast<unsigned int>(previousArrayBuffer));
    glBindVertexArray(static_cast<unsigned int>(previousVao));
    glUseProgram(static_cast<unsigned int>(previousProgram));
    glLineWidth(previousLineWidth);

    if (cullWasEnabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
    if (depthWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void DebugPhysicsSystem::DrawOverlay() const {
    if (!m_Enabled) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    ImVec2 pos(viewport->WorkPos.x + viewport->WorkSize.x - 12.0f,
               viewport->WorkPos.y + 52.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.58f);

    if (ImGui::Begin("##DebugPhysicsOverlay", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.20f, 1.0f, 0.62f, 1.0f),
                           "[PHYSICS: OFF - NOCLIP]");
    }
    ImGui::End();
}

void DebugPhysicsSystem::BuildColliderLines(std::vector<LineVertex>& vertices) {
    if (!m_Scene) {
        return;
    }

    vertices.reserve(m_Scene->GetEntities().size() * 24);

    for (const auto& entityPtr : m_Scene->GetEntities()) {
        Entity* entity = entityPtr.get();
        auto* collider = entity->GetComponent<BoxCollider>();
        auto* transform = entity->GetComponent<Transform>();
        if (!collider || !transform) {
            continue;
        }

        glm::mat4 model = entity->GetWorldTransform();
        model = glm::translate(model, collider->center);
        model = glm::scale(model, collider->size);

        const glm::vec3 color =
            collider->isTrigger ? glm::vec3(1.0f, 0.82f, 0.20f)
                                : glm::vec3(0.18f, 1.0f, 0.58f);
        if (collider->shape == BoxCollider::Shape::Ramp)
            AppendRamp(vertices, model, color);
        else
            AppendBox(vertices, model, color);
    }
}

void DebugPhysicsSystem::AppendRamp(std::vector<LineVertex>& vertices,
                                    const glm::mat4& model,
                                    const glm::vec3& color) const {
    static const std::array<glm::vec3, 6> corners = {
        glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(0.5f,  0.5f, -0.5f),
        glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(0.5f, -0.5f,  0.5f)
    };
    static const std::array<unsigned int, 18> edges = {
        0, 1, 1, 3, 3, 2, 2, 0,
        0, 4, 1, 5, 4, 5, 4, 2, 5, 3
    };
    for (unsigned int index : edges) {
        vertices.push_back({glm::vec3(model * glm::vec4(corners[index], 1.0f)),
                            color});
    }
}

void DebugPhysicsSystem::AppendBox(std::vector<LineVertex>& vertices,
                                   const glm::mat4& model,
                                   const glm::vec3& color) const {
    static const std::array<glm::vec3, 8> corners = {
        glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.5f, -0.5f, -0.5f),
        glm::vec3(0.5f, 0.5f, -0.5f),   glm::vec3(-0.5f, 0.5f, -0.5f),
        glm::vec3(-0.5f, -0.5f, 0.5f),  glm::vec3(0.5f, -0.5f, 0.5f),
        glm::vec3(0.5f, 0.5f, 0.5f),    glm::vec3(-0.5f, 0.5f, 0.5f),
    };
    static const std::array<unsigned int, 24> edges = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
    };

    std::array<glm::vec3, 8> worldCorners;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        worldCorners[i] = glm::vec3(model * glm::vec4(corners[i], 1.0f));
    }

    for (std::size_t i = 0; i < edges.size(); i += 2) {
        vertices.push_back({worldCorners[edges[i]], color});
        vertices.push_back({worldCorners[edges[i + 1]], color});
    }
}

void DebugPhysicsSystem::CapturePlayerState() {
    if (!m_FPSController) {
        m_HasCapturedPlayerState = false;
        return;
    }

    m_PreviousNoclip = m_FPSController->IsNoclipEnabled();
    m_HasCapturedPlayerState = true;
}

void DebugPhysicsSystem::RestorePlayerState() {
    if (m_FPSController && m_HasCapturedPlayerState) {
        m_FPSController->SetNoclipEnabled(m_PreviousNoclip);
    }
    m_HasCapturedPlayerState = false;
}

} // namespace Archura
