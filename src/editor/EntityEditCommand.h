#pragma once

#include "TransformUndoHistory.h"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Archura {
// CPU-only create/delete transactions. Restoration reconstructs only the named
// entities and commits hierarchy/order together, preserving unrelated handles.
class EntityEditCommand final : public EditorHistoryCommand {
public:
    static constexpr size_t MaxCommandBytes = 8u * 1024u * 1024u;
    // Callback is synchronous and create-only: it may configure new entities,
    // but must not mutate existing entity values or hierarchy.
    static std::shared_ptr<EntityEditCommand> Creation(
        Scene& scene, std::function<void()> create, std::vector<std::string> selection,
        std::string primary, const std::filesystem::path& documentRoot, std::string& error);
    static std::shared_ptr<EntityEditCommand> Deletion(
        Scene& scene, const std::vector<EntityHandle>& targets, std::vector<std::string> selection,
        std::string primary, const std::filesystem::path& documentRoot, std::string& error);

    bool CanApply(const Scene& scene, bool undo) const override;
    ActionResult Apply(Scene& scene, bool undo) override;
    size_t ByteSize() const noexcept override;
    const char* Label() const noexcept override { return m_IsCreation ? "Create Entity" : "Delete Entities"; }
    const std::string& Error() const noexcept { return m_Error; }

private:
    struct State {
        bool present = false;
        std::string payload;
        std::vector<EntityParentLink> links;
        std::vector<std::pair<std::string, std::vector<std::string>>> children;
        std::vector<std::string> order;
        std::shared_ptr<const std::vector<std::string>> selection;
        std::string primary;
    };
    EntityEditCommand(Scene& scene, const std::filesystem::path& documentRoot);
    bool CaptureState(Scene& scene, State& state, const std::vector<EntityHandle>& targets);
    bool MatchesState(const Scene& scene, const State& state) const;
    ActionResult CreateInitial(Scene& scene);
    std::vector<std::string> FinalOrder(const Scene& scene, const State& desired) const;
    ActionResult Result(Scene& scene, const State& state) const;
    size_t PayloadBytes() const noexcept;

    Scene* m_Scene;
    std::weak_ptr<Scene*> m_Lifetime;
    std::filesystem::path m_DocumentRoot, m_ResourceRoot;
    bool m_ExplicitRoot = false, m_IsCreation = false, m_Ready = false;
    std::function<void()> m_Create;
    std::vector<std::string> m_Targets;
    State m_Before, m_After;
    std::string m_Error;
};
} // namespace Archura
