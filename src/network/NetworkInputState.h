#pragma once
#include "NetworkProtocol.h"
#include <array>
#include <cmath>

namespace Archura::Net {
class NetworkInputState {
public:
    PlayerInputPacket Next(float x, float z, bool active) {
        if (!active || !std::isfinite(x) || !std::isfinite(z)) x = z = 0;
        const auto length = std::hypot(x, z);
        if (length > 1) { x /= length; z /= length; }
        PlayerInputPacket packet{m_Next, x, z};
        if (++m_Next == 0) m_Next = 1;
        return packet;
    }
    void Reset() { m_Next = 1; }
private:
    std::uint32_t m_Next = 1;
};
inline std::array<float, 2> PlanarMovement(float facingX, float facingZ, float forward, float strafe) {
    const auto length = std::hypot(facingX, facingZ);
    if (length < 0.001F || !std::isfinite(length)) return {};
    facingX /= length; facingZ /= length;
    return {facingX*forward-facingZ*strafe, facingZ*forward+facingX*strafe};
}
} // namespace Archura::Net
