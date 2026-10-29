#include "editor/SceneViewport.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << ": failed: " << #condition << '\n'; ++failures; } } while (false)
bool Near(float a, float b, float tolerance = 0.0001f) {
    return std::fabs(a - b) < tolerance;
}
bool Near(glm::vec2 a, glm::vec2 b) { return Near(a.x, b.x) && Near(a.y, b.y); }

void TestTranslatedFractionalRectangle() {
    const Archura::SceneViewport viewport{{237.25f, 113.5f}, {641.75f, 359.25f}};
    const glm::vec2 center = viewport.origin + viewport.size * 0.5f;
    CHECK(viewport.Contains(center));
    CHECK(Near(viewport.ToNdc(center), glm::vec2(0)));
    CHECK(Near(viewport.ToNdc(viewport.origin), glm::vec2(-1, 1)));
    CHECK(Near(viewport.ToNdc(viewport.origin + viewport.size), glm::vec2(1, -1)));
    CHECK(Near(viewport.ToScreen(glm::vec2(0)), center));
    CHECK(Near(viewport.Aspect(9.0f), 641.75f / 359.25f));
    CHECK(viewport.Contains(viewport.origin));
    CHECK(!viewport.Contains(viewport.origin + glm::vec2(viewport.size.x, 1)));
    CHECK(!viewport.Contains(viewport.origin + glm::vec2(1, viewport.size.y)));
    CHECK(!viewport.Contains(viewport.origin - glm::vec2(0.01f, 0)));
    CHECK(!viewport.Contains(viewport.origin - glm::vec2(0, 0.01f)));
    for (const glm::vec2 ndc : {glm::vec2(-1, 1), glm::vec2(-0.37f, 0.52f),
                                glm::vec2(0.91f, -0.77f), glm::vec2(1.5f, -1.4f)}) {
        CHECK(Near(viewport.ToNdc(viewport.ToScreen(ndc)), ndc));
    }
}

void TestInvalidRectanglesAndInputs() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const glm::vec2 size : {glm::vec2(0), glm::vec2(-20, 30), glm::vec2(30, 1),
                                 glm::vec2(0.5f, 30), glm::vec2(nan, 10), glm::vec2(20, inf)}) {
        const Archura::SceneViewport viewport{{50, 60}, size};
        CHECK(!viewport.Contains({50, 60}));
        CHECK(Near(viewport.Aspect(1.7f), 1.7f));
        CHECK(Near(viewport.ToNdc({50, 60}), glm::vec2(0)));
        CHECK(Near(viewport.ToScreen({0, 0}), glm::vec2(0)));
    }
    const Archura::SceneViewport invalidOrigin{{nan, 0}, {200, 100}};
    CHECK(!invalidOrigin.Contains({20, 20}));
    CHECK(Near(invalidOrigin.Aspect(inf), 1.0f));
    const Archura::SceneViewport valid{{50, 60}, {200, 100}};
    CHECK(!valid.Contains({nan, 60}));
    CHECK(!valid.Contains({50, inf}));
    CHECK(Near(valid.ToNdc({inf, 60}), glm::vec2(0)));
    CHECK(Near(valid.ToScreen({nan, 0}), glm::vec2(0)));
}

// The renderer and picking ray must agree after moving/resizing a docked image.
// Project actual 3D points, map through logical image coordinates, then unproject.
void TestCameraProjectionAndPickingAgreement() {
    const glm::vec3 camera(3, 4, 9);
    const glm::mat4 view = glm::lookAt(camera, glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
    for (const Archura::SceneViewport viewport : {
             Archura::SceneViewport{{0, 0}, {1280, 720}},
             Archura::SceneViewport{{287.5f, 103.25f}, {711.25f, 483.5f}},
             Archura::SceneViewport{{40, 170}, {300, 650}}}) {
        const glm::mat4 matrix = glm::perspective(glm::radians(55.0f), viewport.Aspect(1),
                                                  0.1f, 500.0f) * view;
        const glm::mat4 inverse = glm::inverse(matrix);
        for (const glm::vec3 world : {glm::vec3(0, 1, 0), glm::vec3(1, 2, -1), glm::vec3(-1, 0, 1)}) {
            const glm::vec4 clip = matrix * glm::vec4(world, 1);
            const glm::vec3 expectedNdc = glm::vec3(clip) / clip.w;
            const glm::vec2 screen = viewport.ToScreen(glm::vec2(expectedNdc));
            const glm::vec2 pickingNdc = viewport.ToNdc(screen);
            const glm::vec4 homogeneous = inverse * glm::vec4(pickingNdc, expectedNdc.z, 1);
            const glm::vec3 restored = glm::vec3(homogeneous) / homogeneous.w;
            CHECK(glm::length(restored - world) < 0.002f);
            glm::vec4 nearPoint = inverse * glm::vec4(pickingNdc, -1, 1);
            glm::vec4 farPoint = inverse * glm::vec4(pickingNdc, 1, 1);
            nearPoint /= nearPoint.w;
            farPoint /= farPoint.w;
            const glm::vec3 ray = glm::normalize(glm::vec3(farPoint - nearPoint));
            const glm::vec3 toObject = glm::normalize(world - glm::vec3(nearPoint));
            CHECK(glm::dot(ray, toObject) > 0.99999f);
        }
    }
}
}

int main() {
    TestTranslatedFractionalRectangle();
    TestInvalidRectanglesAndInputs();
    TestCameraProjectionAndPickingAgreement();
    if (failures) return 1;
    std::cout << "Scene viewport: fractional bounds, invalid input and camera/picking agreement passed\n";
    return 0;
}
