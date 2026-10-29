#pragma once

#include <glm/vec2.hpp>
#include <cmath>

namespace Archura {

// Logical ImGui/SDL coordinates, independent of drawable pixel density.
// Screen Y grows down; NDC Y grows up. The far edges belong to adjacent UI.
struct SceneViewport {
    glm::vec2 origin{};
    glm::vec2 size{};

    bool Contains(glm::vec2 screen) const {
        if (!Valid() || !Finite(screen)) return false;
        const glm::vec2 local = screen - origin;
        return local.x >= 0 && local.y >= 0 && local.x < size.x && local.y < size.y;
    }

    float Aspect(float fallback) const {
        return Valid() ? size.x / size.y
            : (std::isfinite(fallback) && fallback > 0 ? fallback : 1.0f);
    }

    // Call Contains before initiating interaction. Conversion intentionally also
    // permits points outside the image so an existing drag can continue there.
    // Invalid rectangles/inputs return zero rather than NaN or infinity.
    glm::vec2 ToNdc(glm::vec2 screen) const {
        if (!Valid() || !Finite(screen)) return {};
        const glm::vec2 local = (screen - origin) / size;
        const glm::vec2 ndc(2.0f * local.x - 1.0f, 1.0f - 2.0f * local.y);
        return Finite(ndc) ? ndc : glm::vec2{};
    }

    glm::vec2 ToScreen(glm::vec2 ndc) const {
        if (!Valid() || !Finite(ndc)) return {};
        const glm::vec2 screen = origin + size * glm::vec2((ndc.x + 1.0f) * 0.5f,
                                                         (1.0f - ndc.y) * 0.5f);
        return Finite(screen) ? screen : glm::vec2{};
    }

private:
    static bool Finite(glm::vec2 value) {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }
    bool Valid() const {
        return Finite(origin) && Finite(size) && size.x > 1.0f && size.y > 1.0f;
    }
};

} // namespace Archura
