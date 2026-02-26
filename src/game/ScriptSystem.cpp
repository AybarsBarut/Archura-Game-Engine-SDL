#include "ScriptSystem.h"
#include "../scripting/ScriptEngine.h"
#include "../ecs/Component.h"
#include "DevConsole.h"
#include <iostream>

namespace Archura {

    void ScriptSystem::Init(Scene* scene) {
        m_Scene = scene;
        ScriptEngine::Init();
        DevConsole::Get().Log("Script System Initialized (Mono)");
        
        // Initialize existing entities
        for (auto& entityPtr : m_Scene->GetEntities()) {
            Entity* entity = entityPtr.get();
            if (entity->HasComponent<ScriptComponent>()) {
                 ScriptEngine::OnCreateEntity(*entity);
            }
        }
    }

    void ScriptSystem::Update(float deltaTime) {
        if (!m_Scene) return;

        // Iterate over all entities with ScriptComponent
        for (auto& entityPtr : m_Scene->GetEntities()) {
            Entity* entity = entityPtr.get();
            if (entity->HasComponent<ScriptComponent>()) {
                ScriptEngine::OnUpdateEntity(*entity, deltaTime);
            }
        }
    }

    void ScriptSystem::Shutdown() {
        ScriptEngine::Shutdown();
    }

    void ScriptSystem::ReloadScripts() {
        ScriptEngine::LoadAssembly("Resources/Scripts/ScriptCore.dll");
        // Re-initialize entities might be needed here
    }

} // namespace Archura
