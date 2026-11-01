#pragma once

#include "TransformUndoHistory.h"

namespace Archura {
// Stores links only for moved children and their affected sibling groups.
// Applying a prepared graph changes no entity handles or local TRS values.
class HierarchyEditCommand final : public EditorHistoryCommand {
public:
    static std::shared_ptr<HierarchyEditCommand> Create(Scene& scene,
        const std::vector<EntityHandle>& children, EntityHandle parent,
        std::vector<std::string> selection = {}, std::string primary = {});
    bool CanApply(const Scene& scene, bool undo) const override;
    ActionResult Apply(Scene& scene, bool undo) override;
    size_t ByteSize() const noexcept override;
    const char* Label() const noexcept override { return m_Unparent ? "Unparent Entities" : "Reparent Entities"; }
private:
    bool Matches(const Scene& scene, const std::vector<EntityParentLink>& links, const std::vector<size_t>& counts) const;
    Scene* m_Scene = nullptr;
    std::weak_ptr<Scene*> m_Lifetime;
    bool m_Unparent = false;
    std::vector<EntityParentLink> m_Before, m_After;
    std::vector<std::string> m_Parents;
    std::vector<size_t> m_BeforeCounts, m_AfterCounts;
    std::shared_ptr<const std::vector<std::string>> m_Selection;
    std::string m_Primary;
};
} // namespace Archura
