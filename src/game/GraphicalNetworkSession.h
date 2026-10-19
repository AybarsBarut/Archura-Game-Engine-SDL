#pragma once
#include "../network/NetworkSceneReplica.h"
#include "../network/NetworkInputState.h"
#include "../network/SnapshotInterpolation.h"
#include "../rendering/Camera.h"
#include <memory>

namespace Archura {
class Mesh;
class RenderSystem;
struct NetworkLaunchOptions {
    std::string host;
    int port = 27015;
    bool smoke = false;
    float moveX = 1, moveZ = 0;
    int durationMs = 4200;
};

// Render-owned, disposable view of the authoritative arena. The authored Scene,
// FPS camera and gameplay systems are never rebound to remote entities.
class GraphicalNetworkSession {
public:
    GraphicalNetworkSession();
    ~GraphicalNetworkSession();
    bool Apply(const std::vector<std::uint8_t>& bytes, std::uint32_t localID);
    void PrepareView();
    void ResetConnection();
    Net::PlayerInputPacket Input(float worldX, float worldZ, bool active);
    static glm::vec2 Movement(const Camera& camera, float forward, float strafe);
    Camera& GetCamera() { return m_Camera; }
    RenderSystem& GetRenderer() { return *m_Renderer; }
    const Net::WorldSnapshot& Snapshot() const { return m_Replica.Snapshot(); }
    const Net::WorldSnapshot& View() const { return m_View; }
    std::uint32_t LocalID() const { return m_LocalID; }
    std::size_t RemoteCount() const;
    bool ValidateView() const;
    void AimAtRemote();
private:
    Scene m_Scene{"Network arena"};
    Camera m_Camera{glm::vec3(0, Net::kArenaEyeHeight, 0)};
    Net::NetworkSceneReplica m_Replica;
    Net::SnapshotInterpolation m_Interpolation;
    Net::WorldSnapshot m_View;
    std::shared_ptr<Mesh> m_Cube;
    std::unique_ptr<RenderSystem> m_Renderer;
    std::uint32_t m_LocalID = 0;
    Net::NetworkInputState m_InputState;
};
} // namespace Archura
