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
        void StartRuntime(Scene* scene);
        void StopRuntime();
        void SetScene(Scene* scene) noexcept { m_Scene = scene; }
        bool IsRuntimeActive() const noexcept { return m_RuntimeActive; }
        void Update(float deltaTime);
        void Shutdown();

        void ReloadScripts();

    private:
        Scene* m_Scene = nullptr;
        bool m_RuntimeActive = false;
        std::unordered_set<uint64_t> m_ActiveScriptEntities;
    };

} // namespace Archura
