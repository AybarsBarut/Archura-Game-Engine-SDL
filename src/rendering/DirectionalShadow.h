#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Archura {

inline constexpr float DirectionalShadowDistance = 100.0f;
inline constexpr float DirectionalShadowFadeStart = 80.0f;

// A camera-centered sphere fits in this orthographic volume for every camera
// orientation. Receivers fade out before reaching its edge; the extra depth
// margin retains off-screen casters between the light and those receivers.
inline glm::mat4 BuildDirectionalShadowMatrix(const glm::vec3& cameraPosition,
                                              glm::vec3 lightDirection,
                                              unsigned int width,
                                              unsigned int height) {
    const float lengthSquared = glm::dot(lightDirection, lightDirection);
    if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f)
        lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    else
        lightDirection /= std::sqrt(lengthSquared);

    constexpr float casterMargin = 50.0f;
    constexpr float eyeDistance = DirectionalShadowDistance + casterMargin;
    const glm::vec3 up = std::abs(lightDirection.y) > 0.99f
                            ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const glm::mat4 lightView = glm::lookAt(
        cameraPosition - lightDirection * eyeDistance, cameraPosition, up);
    glm::mat4 projection = glm::ortho(
        -DirectionalShadowDistance, DirectionalShadowDistance,
        -DirectionalShadowDistance, DirectionalShadowDistance,
        1.0f, 2.0f * eyeDistance);

    // Snap the world origin to a shadow texel to avoid crawling under motion.
    const glm::vec2 resolution(width, height);
    const glm::vec2 origin = glm::vec2(projection * lightView * glm::vec4(0, 0, 0, 1))
                             * resolution * 0.5f;
    const glm::vec2 offset = (glm::round(origin) - origin) * 2.0f / resolution;
    projection[3] += glm::vec4(offset, 0.0f, 0.0f);
    return projection * lightView;
}

} // namespace Archura
