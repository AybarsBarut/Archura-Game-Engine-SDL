#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace Archura::Net {
constexpr float kArenaExtent = 256.0F;
constexpr float kArenaPlayerRadius = 0.35F;
constexpr float kArenaEyeHeight = 1.6F;
struct ArenaObstacle { float minX, maxX, minZ, maxZ, height; };
inline constexpr std::array<ArenaObstacle, 2> kArenaObstacles{{
    {3, 5, -1, 1, 2}, {-4, -2, -5, -3, 3}
}};

// Axis-separated swept AABB footprint. Rendering uses these same obstacle
// bounds; there is no client-side collision simulation or client-provided dt.
inline void MoveArenaPlayer(float& x, float& z, float dx, float dz) {
    auto move = [](float start, float other, float delta, bool xAxis) {
        const float limit = kArenaExtent - kArenaPlayerRadius;
        float end = std::clamp(start + delta, -limit, limit);
        for (const auto& obstacle : kArenaObstacles) {
            const float nearOther = (xAxis ? obstacle.minZ : obstacle.minX) - kArenaPlayerRadius;
            const float farOther = (xAxis ? obstacle.maxZ : obstacle.maxX) + kArenaPlayerRadius;
            if (other <= nearOther || other >= farOther) continue;
            const float low = (xAxis ? obstacle.minX : obstacle.minZ) - kArenaPlayerRadius;
            const float high = (xAxis ? obstacle.maxX : obstacle.maxZ) + kArenaPlayerRadius;
            if (delta > 0 && start <= low && end > low) end = low;
            if (delta < 0 && start >= high && end < high) end = high;
        }
        return end;
    };
    x = move(x, z, dx, true);
    z = move(z, x, dz, false);
}
} // namespace Archura::Net
