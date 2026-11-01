#pragma once
#include "TransformUndoHistory.h"

namespace Archura {
class EntityNameEditCommand final : public EditorHistoryCommand {
public:
    static std::shared_ptr<EntityNameEditCommand> Create(Scene& scene, EntityHandle handle, std::string desired) {
        const auto* entity = scene.GetEntity(handle);
        if (!entity || scene.IsIterating() || desired.empty() || desired == entity->GetName()) return {};
        auto command = std::make_shared<EntityNameEditCommand>();
        command->m_Scene = &scene;
        command->m_Lifetime = scene.GetLifetimeToken();
        command->m_ID = entity->GetPersistentID();
        command->m_Before = entity->GetName();
        command->m_After = std::move(desired);
        if (command->ByteSize() > TransformUndoHistory::MaxGestureBytes) return {};
        return command;
    }
    bool CanApply(const Scene& scene, bool undo) const override {
        const auto lifetime = m_Lifetime.lock();
        if (&scene != m_Scene || !lifetime || *lifetime != &scene ||
            lifetime != scene.GetLifetimeToken().lock() || scene.IsIterating()) return false;
        const auto* entity = scene.GetEntityByPersistentID(m_ID);
        return entity && entity->GetName() == (undo ? m_After : m_Before);
    }
    ActionResult Apply(Scene& scene, bool undo) override {
        if (!CanApply(scene, undo)) return {ActionResult::Status::Blocked};
        std::string prepared = undo ? m_Before : m_After;
        auto* entity = scene.GetEntityByPersistentID(m_ID);
        entity->SetName(std::move(prepared));
        return {ActionResult::Status::Applied, entity, {}};
    }
    size_t ByteSize() const noexcept override {
        return sizeof(*this) + m_ID.capacity() + m_Before.capacity() + m_After.capacity();
    }
    const char* Label() const noexcept override { return "Rename Entity"; }
private:
    Scene* m_Scene = nullptr;
    std::weak_ptr<Scene*> m_Lifetime;
    std::string m_ID, m_Before, m_After;
};
} // namespace Archura
