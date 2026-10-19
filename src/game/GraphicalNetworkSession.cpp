#include "GraphicalNetworkSession.h"
#include "RenderSystem.h"
#include "../rendering/Mesh.h"
#include <cmath>
#include <stdexcept>

namespace Archura {
GraphicalNetworkSession::GraphicalNetworkSession() {
    m_Cube = Mesh::CreateCubeShared();
    auto cube = [&](const char* name, glm::vec3 pos, glm::vec3 scale, glm::vec3 color) {
        auto* entity = m_Scene.CreateEntity(name);
        entity->SetOwnership(EntityOwnership::Engine);
        auto* transform = entity->GetComponent<Transform>();
        transform->position = pos; transform->scale = scale;
        auto* mesh = entity->AddComponent<MeshRenderer>();
        mesh->SetMeshAsset(m_Cube); mesh->color = color;
    };
    const float extent = Net::kArenaExtent;
    cube("Arena floor", {0, -0.5F, 0}, {extent*2, 1, extent*2}, {0.25F, 0.3F, 0.35F});
    for (const auto& box : Net::kArenaObstacles)
        cube("Arena obstacle", {(box.minX+box.maxX)/2, box.height/2, (box.minZ+box.maxZ)/2},
             {box.maxX-box.minX, box.height, box.maxZ-box.minZ}, {0.8F, 0.45F, 0.2F});
    for (float side : {-1.0F, 1.0F}) {
        cube("Arena wall X", {side*(extent+0.5F), 1.5F, 0}, {1, 3, extent*2}, {0.4F, 0.5F, 0.6F});
        cube("Arena wall Z", {0, 1.5F, side*(extent+0.5F)}, {extent*2, 3, 1}, {0.4F, 0.5F, 0.6F});
    }
    auto* sun = m_Scene.CreateEntity("Arena sun");
    sun->SetOwnership(EntityOwnership::Engine);
    sun->GetComponent<Transform>()->rotation = {-50, -30, 0};
    auto* light = sun->AddComponent<LightComponent>();
    light->type = LightComponent::Type::Directional; light->intensity = 1;
    m_Renderer = std::make_unique<RenderSystem>(&m_Camera);
    m_Renderer->Init(&m_Scene);
    if (!m_Renderer->IsInitialized()) throw std::runtime_error("Network arena renderer failed");
}
GraphicalNetworkSession::~GraphicalNetworkSession() = default;

void GraphicalNetworkSession::ResetConnection() {
    if (!m_Replica.Clear()) throw std::runtime_error("Network scene is being iterated");
    m_LocalID = 0; m_InputState.Reset();
}

bool GraphicalNetworkSession::Apply(const std::vector<std::uint8_t>& bytes, std::uint32_t localID) {
    if (localID < 2) return false;
    if (m_LocalID && localID != m_LocalID) ResetConnection();
    if (!m_Replica.Apply(m_Scene, bytes)) return false;
    m_LocalID = localID;
    bool foundSelf = false;
    for (const auto& player : Snapshot().players) {
        auto* entity = m_Replica.Resolve(m_Scene, player.id);
        if (!entity) return false;
        if (player.id == m_LocalID) {
            foundSelf = true;
            entity->RemoveComponent<MeshRenderer>();
            m_Camera.SetPosition({player.x, Net::kArenaEyeHeight, player.z});
        } else {
            auto* transform = entity->GetComponent<Transform>();
            transform->position.y = 0.9F;
            transform->scale = {Net::kArenaPlayerRadius*2, 1.8F, Net::kArenaPlayerRadius*2};
            auto* mesh = entity->GetComponent<MeshRenderer>();
            if (!mesh) mesh = entity->AddComponent<MeshRenderer>();
            mesh->SetMeshAsset(m_Cube);
            mesh->color = (player.id % 2) ? glm::vec3(0.25F, 0.8F, 0.5F) : glm::vec3(0.3F, 0.55F, 1.0F);
        }
    }
    return foundSelf;
}

Net::PlayerInputPacket GraphicalNetworkSession::Input(float x, float z, bool active) {
    return m_InputState.Next(x, z, active);
}
glm::vec2 GraphicalNetworkSession::Movement(const Camera& camera, float forward, float strafe) {
    const auto movement = Net::PlanarMovement(camera.GetFront().x, camera.GetFront().z, forward, strafe);
    return {movement[0], movement[1]};
}
std::size_t GraphicalNetworkSession::RemoteCount() const {
    std::size_t count = 0;
    for (const auto& player : Snapshot().players) if (player.id != m_LocalID) ++count;
    return count;
}
bool GraphicalNetworkSession::ValidateView() const {
    if (!m_LocalID || !Snapshot().tick) return false;
    bool self = false;
    for (const auto& player : Snapshot().players) {
        const auto* entity = m_Replica.Resolve(m_Scene, player.id);
        if (!entity) return false;
        if (player.id == m_LocalID) {
            self = true;
            if (entity->HasComponent<MeshRenderer>() || m_Camera.GetPosition() != glm::vec3(player.x, Net::kArenaEyeHeight, player.z)) return false;
        } else {
            const auto* mesh = entity->GetComponent<MeshRenderer>();
            const auto* transform = entity->GetComponent<Transform>();
            if (!mesh || !mesh->GetMesh() || !transform || transform->position != glm::vec3(player.x,0.9F,player.z)) return false;
        }
    }
    return self;
}
void GraphicalNetworkSession::AimAtRemote() {
    for (const auto& player : Snapshot().players) if (player.id != m_LocalID) {
        const auto diff = glm::vec2(player.x-m_Camera.GetPosition().x, player.z-m_Camera.GetPosition().z);
        if (glm::length(diff) > 0.1F) {
            m_Camera.SetYaw(glm::degrees(std::atan2(diff.y, diff.x)));
            m_Camera.SetPitch(-5);
        }
        break;
    }
}
} // namespace Archura
