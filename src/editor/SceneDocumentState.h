#pragma once

#include <optional>
#include <string>
#include <utility>

namespace Archura {

// Owns document identity and an immutable destructive-action target. Runtime
// scene access and file operations remain with the editor coordinator.
class SceneDocumentState {
public:
    enum class Action { None, Close, OpenProject, OpenLegacy, NewScene };

    void Reset(std::optional<std::string> snapshot, std::string path = {}) {
        m_Baseline = std::move(snapshot);
        m_Path = std::move(path);
        m_Initialized = true;
        m_Dirty = !m_Baseline;
        Cancel();
    }

    bool Initialized() const noexcept { return m_Initialized; }
    void Observe(const std::optional<std::string>& snapshot) {
        m_Dirty = !m_Initialized || !m_Baseline || !snapshot || *m_Baseline != *snapshot;
    }
    bool IsDirty() const noexcept { return m_Dirty; }
    const std::string& Path() const noexcept { return m_Path; }

    bool Request(Action action, std::string target = {}) {
        if (action == Action::None || HasPending()) return false;
        m_Action = action;
        m_Target = std::move(target);
        m_NeedsDecision = m_Dirty;
        return true;
    }
    bool HasPending() const noexcept { return m_Action != Action::None; }
    bool NeedsDecision() const noexcept { return HasPending() && m_NeedsDecision; }
    Action PendingAction() const noexcept { return m_Action; }
    const std::string& Target() const noexcept { return m_Target; }

    // Discard authorizes only this action; it never marks unsaved contents clean.
    void Discard() noexcept { m_NeedsDecision = false; }
    void Cancel() noexcept {
        m_Action = Action::None;
        m_Target.clear();
        m_NeedsDecision = false;
    }

    bool Saved(std::optional<std::string> snapshot, std::string path) {
        if (!snapshot || path.empty()) return false;
        m_Baseline = std::move(snapshot);
        m_Path = std::move(path);
        m_Initialized = true;
        m_Dirty = false;
        m_NeedsDecision = false;
        return true;
    }

private:
    std::optional<std::string> m_Baseline;
    std::string m_Path;
    std::string m_Target;
    Action m_Action = Action::None;
    bool m_Initialized = false;
    bool m_Dirty = true;
    bool m_NeedsDecision = false;
};

} // namespace Archura
