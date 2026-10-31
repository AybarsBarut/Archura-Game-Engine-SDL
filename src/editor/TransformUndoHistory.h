#pragma once

#include "../ecs/Entity.h"
#include <deque>
#include <optional>

namespace Archura {

// Value-only transform history bound to one Scene lifetime. Capture starts an
// interaction before the first edit; the next command commits it lazily so a
// click without a change cannot consume Undo or invalidate Redo.
class TransformUndoHistory {
public:
    bool Capture(const Entity& entity) {
        const auto* owner = entity.GetScene();
        const auto* transform = entity.GetComponent<Transform>();
        if (!owner || !transform || owner->GetEntity(entity.GetHandle()) != &entity) return false;
        Snapshot before = Read(entity, *transform);
        if (!Matches(*owner)) {
            Clear();
            m_Owner = owner;
            m_Lifetime = owner->GetLifetimeToken();
        }
        FinalizePending(*owner);
        m_Pending = std::move(before);
        return true;
    }

    Entity* Undo(Scene& scene) {
        if (!Matches(scene)) { Clear(); return nullptr; }
        FinalizePending(scene);
        return Apply(scene, m_Undo, m_Redo);
    }

    Entity* Redo(Scene& scene) {
        if (!Matches(scene)) { Clear(); return nullptr; }
        FinalizePending(scene);
        return Apply(scene, m_Redo, m_Undo);
    }

    // Queries must leave an unchanged pending capture alive: a drag can activate
    // one frame before its first transform delta arrives.
    bool CanUndo(const Scene& scene) const {
        return Matches(scene) && (PendingChanged(scene) || HasApplicable(scene, m_Undo));
    }

    bool CanRedo(const Scene& scene) const {
        return Matches(scene) && !PendingChanged(scene) && HasApplicable(scene, m_Redo);
    }

    // Play/Stop replaces entity generations while preserving Scene lifetime and
    // persistent IDs. Rebind both directions, including an unfinished capture.
    void RebindRestoredScene(const Scene& scene) {
        if (!Matches(scene)) { Clear(); return; }
        const auto rebind = [&](Snapshot& snapshot) {
            const auto* entity = scene.GetEntityByPersistentID(snapshot.persistentID);
            snapshot.handle = entity && entity->GetComponent<Transform>() ? entity->GetHandle() : EntityHandle{};
        };
        for (auto& snapshot : m_Undo) rebind(snapshot);
        for (auto& snapshot : m_Redo) rebind(snapshot);
        if (m_Pending) rebind(*m_Pending);
    }

    void Clear() {
        m_Undo.clear();
        m_Redo.clear();
        m_Pending.reset();
        m_Owner = nullptr;
        m_Lifetime.reset();
    }

private:
    struct Snapshot {
        EntityHandle handle;
        std::string persistentID;
        glm::vec3 position, rotation, scale;
    };

    bool Matches(const Scene& scene) const {
        if (m_Owner != &scene) return false;
        const auto lifetime = m_Lifetime.lock();
        return lifetime && *lifetime == &scene && lifetime == scene.GetLifetimeToken().lock();
    }

    static Snapshot Read(const Entity& entity, const Transform& transform) {
        return {entity.GetHandle(), entity.GetPersistentID(), transform.position, transform.rotation, transform.scale};
    }

    static const Entity* Resolve(const Scene& scene, const Snapshot& snapshot) {
        const auto* entity = scene.GetEntity(snapshot.handle);
        return entity && entity->GetPersistentID() == snapshot.persistentID && entity->GetComponent<Transform>() ? entity : nullptr;
    }

    static bool Different(const Transform& transform, const Snapshot& snapshot) {
        return transform.position != snapshot.position || transform.rotation != snapshot.rotation || transform.scale != snapshot.scale;
    }

    bool PendingChanged(const Scene& scene) const {
        if (!m_Pending) return false;
        const auto* entity = Resolve(scene, *m_Pending);
        return entity && Different(*entity->GetComponent<Transform>(), *m_Pending);
    }

    static bool HasApplicable(const Scene& scene, const std::deque<Snapshot>& snapshots) {
        for (auto current = snapshots.rbegin(); current != snapshots.rend(); ++current) {
            const auto* entity = Resolve(scene, *current);
            if (entity && Different(*entity->GetComponent<Transform>(), *current)) return true;
        }
        return false;
    }

    static void PushBounded(std::deque<Snapshot>& snapshots, Snapshot snapshot) {
        snapshots.push_back(std::move(snapshot));
        if (snapshots.size() > 50) snapshots.pop_front();
    }

    void FinalizePending(const Scene& scene) {
        if (!m_Pending) return;
        if (PendingChanged(scene)) {
            // Copy before publishing so allocation failure leaves the pending
            // interaction available and never clears a valid Redo branch.
            PushBounded(m_Undo, *m_Pending);
            m_Redo.clear();
        }
        m_Pending.reset();
    }

    static Entity* Apply(Scene& scene, std::deque<Snapshot>& source, std::deque<Snapshot>& destination) {
        while (!source.empty()) {
            const auto& desired = source.back();
            const auto* resolved = Resolve(scene, desired);
            if (!resolved || !Different(*resolved->GetComponent<Transform>(), desired)) {
                source.pop_back();
                continue;
            }
            auto* entity = scene.GetEntity(desired.handle);
            auto* transform = entity->GetComponent<Transform>();
            // Prepare/publish the inverse before touching the scene or popping
            // the source command. Values then apply without allocation.
            PushBounded(destination, Read(*entity, *transform));
            transform->position = desired.position;
            transform->rotation = desired.rotation;
            transform->scale = desired.scale;
            source.pop_back();
            return entity;
        }
        return nullptr;
    }

    const Scene* m_Owner = nullptr;
    std::weak_ptr<Scene*> m_Lifetime;
    std::deque<Snapshot> m_Undo, m_Redo;
    std::optional<Snapshot> m_Pending;
};

} // namespace Archura
