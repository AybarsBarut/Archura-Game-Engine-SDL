#pragma once

#include "../ecs/Entity.h"
#include <deque>

namespace Archura {

// Transform history owns values, never entity pointers. Resolve the handle at
// undo time because scripts and editor deletion may destroy a captured entity.
class TransformUndoHistory {
public:
    bool Capture(const Entity& entity) {
        const auto* transform = entity.GetComponent<Transform>();
        if (!transform) return false;
        Snapshot snapshot{entity.GetScene(), entity.GetHandle(), transform->position,
                          transform->rotation, transform->scale};
        m_History.push_back(snapshot);
        if (m_History.size() > 50) m_History.pop_front();
        return true;
    }

    Entity* Undo(Scene& scene) {
        if (m_History.empty()) return nullptr;
        const Snapshot snapshot = m_History.back();
        m_History.pop_back();
        if (snapshot.owner != &scene) return nullptr;
        Entity* entity = scene.GetEntity(snapshot.handle);
        if (!entity) return nullptr;
        auto* transform = entity->GetComponent<Transform>();
        if (!transform) return nullptr;
        transform->position = snapshot.position;
        transform->rotation = snapshot.rotation;
        transform->scale = snapshot.scale;
        return entity;
    }

    void Clear() { m_History.clear(); }

private:
    struct Snapshot {
        const Scene* owner;
        EntityHandle handle;
        glm::vec3 position, rotation, scale;
    };
    std::deque<Snapshot> m_History;
};

} // namespace Archura
