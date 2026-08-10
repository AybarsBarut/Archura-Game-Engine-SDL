#include "ScriptSystem.h"
#include "../scripting/ScriptEngine.h"
#include "../ecs/Component.h"
#include "DevConsole.h"
#include <iostream>

namespace Archura {

    void ScriptSystem::Init(Scene* scene) {
        m_Scene = scene;
        ScriptEngine::Init();
        ScriptEngine::OnRuntimeStart(scene);
        DevConsole::Get().Log("Script System Initialized (Mono)");

        m_ActiveScriptEntities.clear();
        if (!m_Scene) return;
        m_Scene->ForEachEntity([this](Entity& entity) {
            if (!entity.HasComponent<ScriptComponent>()) return;
            const uint64_t handle = entity.GetHandle().Value();
            if (ScriptEngine::OnCreateEntity(entity)) {
                m_ActiveScriptEntities.insert(handle);
                if (!m_Scene->IsAlive(EntityHandle::FromValue(handle))) {
                    ScriptEngine::OnDestroyEntity(EntityHandle::FromValue(handle));
                    m_ActiveScriptEntities.erase(handle);
                }
            }
        });
    }

    void ScriptSystem::Update(float deltaTime) {
        if (!m_Scene) return;

        std::unordered_set<uint64_t> seen;
        m_Scene->ForEachEntity([this, deltaTime, &seen](Entity& entity) {
            if (!entity.HasComponent<ScriptComponent>()) return;
            const uint64_t handle = entity.GetHandle().Value();
            if (m_ActiveScriptEntities.find(handle) ==
                    m_ActiveScriptEntities.end() &&
                ScriptEngine::OnCreateEntity(entity))
                m_ActiveScriptEntities.insert(handle);
            if (m_ActiveScriptEntities.find(handle) ==
                m_ActiveScriptEntities.end())
                return;
            seen.insert(handle);

            Entity* current = m_Scene->GetEntity(EntityHandle::FromValue(handle));
            if (!current || !current->HasComponent<ScriptComponent>()) {
                seen.erase(handle);
                ScriptEngine::OnDestroyEntity(EntityHandle::FromValue(handle));
                return;
            }
            if (!ScriptEngine::OnUpdateEntity(*current, deltaTime)) {
                seen.erase(handle);
                ScriptEngine::OnDestroyEntity(EntityHandle::FromValue(handle));
                return;
            }

            current = m_Scene->GetEntity(EntityHandle::FromValue(handle));
            if (!current || !current->HasComponent<ScriptComponent>()) {
                seen.erase(handle);
                ScriptEngine::OnDestroyEntity(EntityHandle::FromValue(handle));
            }
        });

        for (const uint64_t handle : m_ActiveScriptEntities) {
            if (seen.find(handle) == seen.end())
                ScriptEngine::OnDestroyEntity(EntityHandle::FromValue(handle));
        }
        m_ActiveScriptEntities = std::move(seen);
    }

    void ScriptSystem::Shutdown() {
        for (const uint64_t handle : m_ActiveScriptEntities)
            ScriptEngine::OnDestroyEntity(EntityHandle::FromValue(handle));
        m_ActiveScriptEntities.clear();
        ScriptEngine::OnRuntimeStop();
        ScriptEngine::Shutdown();
        m_Scene = nullptr;
    }

    void ScriptSystem::ReloadScripts() {
        ScriptEngine::LoadAssembly("Resources/Scripts/ScriptCore.dll");
        // Re-initialize entities might be needed here
    }

} // namespace Archura
