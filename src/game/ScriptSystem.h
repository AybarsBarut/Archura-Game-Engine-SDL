#pragma once

#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace Archura {

    class Scene;

    class ScriptSystem {
    public:
        ~ScriptSystem() noexcept { Shutdown(); }
        void Init(Scene* scene);
        void Update(float deltaTime);
        void Shutdown();

        void ReloadScripts();

    private:
        Scene* m_Scene = nullptr;
        std::unordered_set<uint64_t> m_ActiveScriptEntities;
        
        // In a real implementation, this would hold the Mono Domain / Assembly
        // void* m_MonoDomain; 
    };

} // namespace Archura
