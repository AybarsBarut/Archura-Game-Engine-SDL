#pragma once
#include "TransformUndoHistory.h"
#include <cmath>

namespace Archura {
// Stores only diffuse color. Texture, mesh and shader owners remain live on
// MeshRenderer and are never copied into the journal.
class MeshColorEditCommand final : public EditorHistoryCommand {
public:
    static std::shared_ptr<MeshColorEditCommand> Create(Scene& scene, EntityHandle handle, const glm::vec3& desired) {
        const auto* entity = scene.GetEntity(handle);
        const auto* renderer = entity ? entity->GetComponent<MeshRenderer>() : nullptr;
        if (!renderer || scene.IsIterating() || renderer->color == desired ||
            !Finite(renderer->color) || !Finite(desired)) return {};
        auto command = std::make_shared<MeshColorEditCommand>();
        command->m_Scene = &scene;
        command->m_Lifetime = scene.GetLifetimeToken();
        command->m_ID = entity->GetPersistentID();
        command->m_Before = renderer->color;
        command->m_After = desired;
        return command;
    }
    bool CanApply(const Scene& scene, bool undo) const override {
        const auto lifetime = m_Lifetime.lock();
        if (&scene != m_Scene || !lifetime || *lifetime != &scene ||
            lifetime != scene.GetLifetimeToken().lock() || scene.IsIterating()) return false;
        const auto* entity = scene.GetEntityByPersistentID(m_ID);
        const auto* renderer = entity ? entity->GetComponent<MeshRenderer>() : nullptr;
        return renderer && renderer->color == (undo ? m_After : m_Before);
    }
    ActionResult Apply(Scene& scene, bool undo) override {
        if (!CanApply(scene, undo)) return {ActionResult::Status::Blocked};
        auto* entity = scene.GetEntityByPersistentID(m_ID);
        entity->GetComponent<MeshRenderer>()->color = undo ? m_Before : m_After;
        return {ActionResult::Status::Applied, entity, {}};
    }
    bool HasChanges() const noexcept override { return m_Before != m_After; }
    std::shared_ptr<EditorHistoryCommand> MergeWith(const EditorHistoryCommand& next) const override {
        const auto* delta = dynamic_cast<const MeshColorEditCommand*>(&next);
        if (!delta || m_Scene != delta->m_Scene || m_Lifetime.lock() != delta->m_Lifetime.lock() ||
            m_ID != delta->m_ID || m_After != delta->m_Before) return {};
        auto merged = std::make_shared<MeshColorEditCommand>(*this);
        merged->m_After = delta->m_After;
        return merged;
    }
    size_t ByteSize() const noexcept override { return sizeof(*this) + m_ID.capacity(); }
    const char* Label() const noexcept override { return "Change Mesh Color"; }
private:
    static bool Finite(const glm::vec3& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
    Scene* m_Scene = nullptr;
    std::weak_ptr<Scene*> m_Lifetime;
    std::string m_ID;
    glm::vec3 m_Before{1}, m_After{1};
};
} // namespace Archura
