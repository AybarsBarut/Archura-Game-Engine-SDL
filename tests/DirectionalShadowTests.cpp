#include "rendering/DirectionalShadow.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void RequireInside(const glm::mat4& matrix, const glm::vec3& position) {
    const glm::vec4 clip = matrix * glm::vec4(position, 1.0f);
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(clip[axis]) || std::abs(clip[axis] / clip.w) >= 1.0f)
            throw std::runtime_error("nearby shadow receiver/caster clipped");
    }
}
}

int main() {
    try {
        const glm::vec3 sun(0.321394f, -0.766044f, -0.556670f);
        const glm::vec3 cameras[] = {{19.4f, 21.4f, 26.7f}, {0, 2, 3},
                                    {46.8f, 41.4f, 33.3f}, {-250, 60, 100}};
        const glm::vec3 directions[] = {sun, {0, -1, 0}, {0, 1, 0},
                                       {1, -0.001f, 0}, sun * 10.0f, {0, 0, 0},
                                       {std::numeric_limits<float>::quiet_NaN(), 0, 0}};
        for (const auto& camera : cameras) {
            for (const auto& direction : directions) {
                const glm::mat4 matrix = Archura::BuildDirectionalShadowMatrix(
                    camera, direction, 2048, 2048);
                // All nearby receivers must stay covered, regardless of the
                // light direction or camera orientation (which changes in Edit).
                for (int latitude = -9; latitude <= 9; ++latitude) {
                    const float pitch = glm::radians(latitude * 10.0f);
                    for (int longitude = 0; longitude < 36; ++longitude) {
                        const float yaw = glm::radians(longitude * 10.0f);
                        const glm::vec3 ray(std::cos(yaw) * std::cos(pitch),
                                            std::sin(pitch),
                                            std::sin(yaw) * std::cos(pitch));
                        RequireInside(matrix, camera + ray * 99.8f);
                        if (glm::dot(direction, direction) > 1.0e-8f) {
                            // An off-screen caster toward the sun must retain
                            // depth coverage for a receiver inside the fade zone.
                            RequireInside(matrix, camera + ray * 80.0f
                                - glm::normalize(direction) * 40.0f);
                        }
                    }
                }
            }
        }
        // Former holes in the user's floor image fell outside the old 90x90
        // map, though they are well within the fully shadowed camera range.
        const auto matrix = Archura::BuildDirectionalShadowMatrix(cameras[0], sun, 2048, 2048);
        RequireInside(matrix, {-47.0f, -0.5f, 47.73f});
        RequireInside(matrix, {-42.88f, -0.5f, 48.99f});
        RequireInside(matrix, {-43.21f, -0.5f, 26.18f});
        std::cout << "Directional shadow coverage passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
