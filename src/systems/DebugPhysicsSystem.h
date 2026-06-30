#pragma once

#include "../ecs/System.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Archura {

class FPSController;
class PhysicsSystem;
class Shader;

class DebugPhysicsSystem : public System {
public:
    DebugPhysicsSystem() = default;
    ~DebugPhysicsSystem() override;

    void Init(Scene* scene) override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    void SetDependencies(FPSController* fpsController, PhysicsSystem* physicsSystem);
    void Toggle();
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_Enabled; }

    void Render(const glm::mat4& view, const glm::mat4& projection);
    void DrawOverlay() const;

private:
    struct LineVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    void BuildColliderLines(std::vector<LineVertex>& vertices);
    void AppendBox(std::vector<LineVertex>& vertices,
                   const glm::mat4& model,
                   const glm::vec3& color) const;
    void CapturePlayerState();
    void RestorePlayerState();

private:
    FPSController* m_FPSController = nullptr;
    PhysicsSystem* m_PhysicsSystem = nullptr;
    std::unique_ptr<Shader> m_Shader;

    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;

    bool m_Enabled = false;
    bool m_HasCapturedPlayerState = false;
    bool m_PreviousNoclip = false;
};

} // namespace Archura
