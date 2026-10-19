#pragma once
#include "AuthoritativeWorld.h"
#include "../ecs/Entity.h"
#include <map>

namespace Archura::Net {
// Dedicated transient entities; generation checks preserve unrelated scene data.
class NetworkSceneReplica {
public:
    bool Apply(Scene& scene, const std::vector<std::uint8_t>& bytes) {
        if (scene.IsIterating()) return false;
        auto token = m_Scene.lock();
        if (token && *token != &scene) return false;
        if (!token) { m_Entities.clear(); m_World.Reset(); m_Scene = scene.GetLifetimeToken(); }
        if (!m_World.Apply(bytes)) return false;
        std::map<std::uint32_t, bool> present;
        for (const auto& player : m_World.Snapshot().players) {
            present.emplace(player.id, true);
            Entity* entity = scene.GetEntity(m_Entities[player.id]);
            if (!entity) {
                entity = scene.CreateEntity("NetworkPlayer_" + std::to_string(player.id));
                entity->SetOwnership(EntityOwnership::Engine);
                m_Entities[player.id] = entity->GetHandle();
            }
            auto* transform = entity->GetComponent<Transform>();
            if (!transform) transform = entity->AddComponent<Transform>();
            transform->position = {player.x, 0.0F, player.z};
        }
        for (auto it = m_Entities.begin(); it != m_Entities.end();) {
            if (!present.count(it->first)) {
                scene.DestroyEntity(it->second); it = m_Entities.erase(it);
            } else ++it;
        }
        return true;
    }
    const WorldSnapshot& Snapshot() const noexcept { return m_World.Snapshot(); }
    const Entity* Resolve(const Scene& scene, std::uint32_t id) const noexcept {
        const auto source = m_Scene.lock();
        const auto it = m_Entities.find(id);
        return source && *source == &scene && it != m_Entities.end() ? scene.GetEntity(it->second) : nullptr;
    }
    Entity* Resolve(Scene& scene, std::uint32_t id) const noexcept {
        return const_cast<Entity*>(Resolve(static_cast<const Scene&>(scene), id));
    }
    bool Clear() {
        const auto source = m_Scene.lock();
        if (source && *source) {
            if ((*source)->IsIterating()) return false;
            for (const auto& entry : m_Entities) (*source)->DestroyEntity(entry.second);
        }
        m_Entities.clear(); m_World.Reset(); m_Scene.reset();
        return true;
    }
private:
    ClientWorld m_World;
    std::weak_ptr<Scene*> m_Scene;
    std::map<std::uint32_t, EntityHandle> m_Entities;
};
} // namespace Archura::Net
