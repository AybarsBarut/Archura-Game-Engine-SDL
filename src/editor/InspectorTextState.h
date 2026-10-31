#pragma once

#include "../ecs/Entity.h"
#include <algorithm>
#include <cstring>

namespace Archura {

// Per-inspector drafts. Never key text by a recycled ID or a raw scene address.
// Cache source values separately so an unchanged source cannot erase typing.
class InspectorTextState {
public:
    char name[128] = {};
    char script[128] = {};
    char newScript[128] = {};

    // Save/close may arrive before ImGui deactivates the field this frame.
    void CommitPending(Scene* scene) {
        const auto owner = m_Lifetime.lock();
        if (!owner || *owner != scene || m_Scene != scene) return;
        auto* entity = scene->GetEntity(m_Handle);
        if (!entity) return;
        if (entity->GetName() == m_SourceName && name[0] && !MatchesDisplayedSource(name, m_SourceName))
            entity->SetName(name);
        auto* component = entity->GetComponent<ScriptComponent>();
        if (m_HasScript && component && component->className == m_SourceScript &&
            !MatchesDisplayedSource(script, m_SourceScript))
            component->className = script;
    }

    void Sync(const Scene* scene, EntityHandle handle,
              const std::string& sourceName, const std::string* sourceScript) {
        if (!scene || !handle || !scene->IsAlive(handle)) {
            Clear();
            return;
        }
        const auto lifetime = scene->GetLifetimeToken().lock();
        const bool selectionChanged = m_Scene != scene || m_Handle != handle ||
                                      m_Lifetime.lock() != lifetime;
        const bool hasScript = sourceScript != nullptr;
        const bool presenceChanged = hasScript != m_HasScript;

        if (selectionChanged || m_SourceName != sourceName) Copy(name, sourceName);
        if (selectionChanged || presenceChanged ||
            (hasScript && m_SourceScript != *sourceScript)) {
            Copy(script, hasScript ? *sourceScript : std::string{});
        }
        if (selectionChanged || presenceChanged) newScript[0] = '\0';

        m_Scene = scene;
        m_Handle = handle;
        m_Lifetime = lifetime;
        m_SourceName = sourceName;
        m_SourceScript = hasScript ? *sourceScript : std::string{};
        m_HasScript = hasScript;
    }

private:
    template<size_t Size>
    static bool MatchesDisplayedSource(const char (&buffer)[Size], const std::string& source) {
        // Copy truncates only the displayed draft. An untouched draft must never
        // truncate the complete authored value when save/close commits fields.
        const size_t count = std::min(Size - 1, source.size());
        return buffer[count] == '\0' && std::memcmp(buffer, source.data(), count) == 0;
    }
    template<size_t Size>
    static void Copy(char (&buffer)[Size], const std::string& source) {
        const size_t count = std::min(Size - 1, source.size());
        std::memcpy(buffer, source.data(), count);
        buffer[count] = '\0';
    }
    void Clear() {
        name[0] = script[0] = newScript[0] = '\0';
        m_Scene = nullptr;
        m_Handle = {};
        m_Lifetime.reset();
        m_SourceName.clear();
        m_SourceScript.clear();
        m_HasScript = false;
    }

    const Scene* m_Scene = nullptr;
    EntityHandle m_Handle{};
    std::weak_ptr<Scene*> m_Lifetime;
    std::string m_SourceName, m_SourceScript;
    bool m_HasScript = false;
};

} // namespace Archura
