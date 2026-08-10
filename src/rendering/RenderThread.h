#pragma once

namespace Archura {

// OpenGL 3.3 contexts are thread-affine. This registry makes that ownership an
// explicit engine contract instead of relying on undocumented call-site habit.
class RenderThread final {
public:
    static void AttachCurrent() noexcept {
        s_IsRenderThread = true;
    }

    static void Detach() noexcept {
        s_IsRenderThread = false;
    }

    static bool IsCurrent() noexcept {
        return s_IsRenderThread;
    }

private:
    inline static thread_local bool s_IsRenderThread = false;
};

} // namespace Archura
