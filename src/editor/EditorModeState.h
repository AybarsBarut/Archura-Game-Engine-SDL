#pragma once

#include <functional>

namespace Archura {

// Edit uses the editor camera with simulation paused; Play uses the FPS
// controller with full simulation and a locked cursor.
enum class EditorMode { Edit, Play };

// Publish a UI mode only after its runtime transition succeeds. Reentrant
// commands from script lifecycle callbacks cannot start a second transition.
class EditorModeState {
public:
    EditorMode GetMode() const noexcept { return m_Mode; }
    bool Request(EditorMode desired, const std::function<bool()>& transition) {
        if (m_Transitioning) return false;
        if (desired == m_Mode) return true;
        if (!transition) return false;
        struct Guard { bool& busy; ~Guard() { busy = false; } } guard{m_Transitioning};
        m_Transitioning = true;
        if (!transition()) return false;
        m_Mode = desired;
        return true;
    }
private:
    EditorMode m_Mode = EditorMode::Edit;
    bool m_Transitioning = false;
};

} // namespace Archura
