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
    char skybox[6][256] = {};

    // Save/close may arrive before ImGui deactivates the field this frame.
    void CommitPending(Scene* scene) {
        CommitPending(scene, [](Entity& entity, const char* value) { entity.SetName(value); },
            [](Entity& entity, const char* value) { entity.GetComponent<ScriptComponent>()->className = value; });
    }
    bool IsBoundTo(const Scene* scene, EntityHandle handle) const {
        return m_Scene == scene && m_Handle == handle && scene && m_Lifetime.lock() == scene->GetLifetimeToken().lock();
    }
    template<class NameSink, class ScriptSink>
    void CommitPending(Scene* scene, NameSink&& nameSink, ScriptSink&& scriptSink) {
        CommitPending(scene, std::forward<NameSink>(nameSink), std::forward<ScriptSink>(scriptSink),
            [](Entity& entity, SkyboxComponent&& desired) {
                auto* live = entity.GetComponent<SkyboxComponent>();
                live->facePaths.swap(desired.facePaths); live->shouldReload = true;
            });
    }
    template<class NameSink, class ScriptSink, class SkySink>
    void CommitPending(Scene* scene, NameSink&& nameSink, ScriptSink&& scriptSink, SkySink&& skySink) {
        const auto owner = m_Lifetime.lock();
        if (!owner || *owner != scene || m_Scene != scene) return;
        auto* entity = scene->GetEntity(m_Handle);
        if (!entity) return;
        if (entity->GetName() == m_SourceName && name[0] && !MatchesDisplayedSource(name, m_SourceName)) {
            std::string prepared(name);
            nameSink(*entity, name);
            if (entity->GetName() == prepared) m_SourceName.swap(prepared);
        }
        auto* component = entity->GetComponent<ScriptComponent>();
        if (m_HasScript && component && component->className == m_SourceScript &&
            !MatchesDisplayedSource(script, m_SourceScript)) {
            std::string prepared(script);
            scriptSink(*entity, script);
            if (component->className == prepared) m_SourceScript.swap(prepared);
        }
        auto* sky = entity->GetComponent<SkyboxComponent>();
        if (m_HasSkybox && sky && sky->facePaths.size() == 6) {
            SkyboxComponent desired = *sky;
            bool changed = false;
            for (size_t index = 0; index < 6; ++index)
                if (sky->facePaths[index] == m_SourceFaces[index] && !MatchesDisplayedSource(skybox[index], m_SourceFaces[index])) {
                    desired.facePaths[index] = skybox[index]; changed = true;
                }
            if (changed) {
                auto prepared = desired.facePaths;
                skySink(*entity, std::move(desired));
                if (sky->facePaths == prepared) m_SourceFaces.swap(prepared);
            }
        }
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

        const auto* sky = scene->GetEntity(handle)->GetComponent<SkyboxComponent>();
        const bool hasSky = sky && sky->facePaths.size() == 6;
        if (hasSky) {
            for (size_t index = 0; index < 6; ++index)
                if (selectionChanged || !m_HasSkybox || m_SourceFaces[index] != sky->facePaths[index]) Copy(skybox[index], sky->facePaths[index]);
            m_SourceFaces = sky->facePaths;
        } else {
            for (auto& face : skybox) face[0] = '\0';
            m_SourceFaces.clear();
        }
        m_HasSkybox = hasSky;
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
        m_HasScript = m_HasSkybox = false;
        m_SourceFaces.clear();
        for (auto& face : skybox) face[0] = '\0';
    }

    const Scene* m_Scene = nullptr;
    EntityHandle m_Handle{};
    std::weak_ptr<Scene*> m_Lifetime;
    std::string m_SourceName, m_SourceScript;
    bool m_HasScript = false, m_HasSkybox = false;
    std::vector<std::string> m_SourceFaces;
};

} // namespace Archura
