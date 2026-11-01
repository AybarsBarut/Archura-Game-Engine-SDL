#include "HierarchyEditCommand.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Archura {
namespace {
EntityParentLink Link(const Entity& entity, size_t siblingIndex) {
    EntityParentLink link{entity.GetPersistentID(), {}};
    if (const auto* parent = entity.GetParent()) {
        link.parentPersistentID = parent->GetPersistentID();
        link.siblingIndex = siblingIndex;
    }
    return link;
}
}
std::shared_ptr<HierarchyEditCommand> HierarchyEditCommand::Create(Scene& scene,
    const std::vector<EntityHandle>& children, EntityHandle parentHandle,
    std::vector<std::string> selection, std::string primary) {
    if (scene.IsIterating() || children.empty() || children.size() > 2048) return {};
    auto* parent = scene.GetEntity(parentHandle);
    if (parentHandle && !parent) return {};
    std::vector<Entity*> moved;
    std::unordered_set<std::string> targets, parents;
    if (parent) parents.insert(parent->GetPersistentID());
    for (const auto handle : children) {
        auto* entity = scene.GetEntity(handle);
        if (!entity || entity == parent) return {};
        if (entity->GetParent() == parent) continue;
        if (!targets.insert(entity->GetPersistentID()).second) continue;
        moved.push_back(entity);
        if (entity->GetParent()) parents.insert(entity->GetParent()->GetPersistentID());
    }
    if (moved.empty()) return {};
    auto command = std::shared_ptr<HierarchyEditCommand>(new HierarchyEditCommand);
    command->m_Scene = &scene;
    command->m_Lifetime = scene.GetLifetimeToken();
    command->m_Unparent = !parent;
    command->m_Primary = std::move(primary);
    command->m_Selection = std::make_shared<const std::vector<std::string>>(std::move(selection));
    command->m_Parents.assign(parents.begin(), parents.end());
    std::unordered_map<const Entity*, size_t> siblingIndices;
    for (const auto& id : command->m_Parents) {
        const auto& siblings = scene.GetEntityByPersistentID(id)->GetChildren();
        for (size_t index = 0; index < siblings.size(); ++index) siblingIndices.emplace(siblings[index], index);
    }
    // Scene order makes the captured set deterministic without depending on hash order.
    for (const auto& entity : scene.GetEntities())
        if (targets.count(entity->GetPersistentID()) ||
            (entity->GetParent() && parents.count(entity->GetParent()->GetPersistentID())))
            command->m_Before.push_back(Link(*entity, entity->GetParent() ? siblingIndices.at(entity.get()) : std::numeric_limits<size_t>::max()));
    command->m_After = command->m_Before;
    std::unordered_map<std::string, size_t> counts, positions;
    for (size_t index = 0; index < command->m_After.size(); ++index)
        positions.emplace(command->m_After[index].childPersistentID, index);
    for (const auto& id : command->m_Parents) {
        size_t position = 0;
        for (const auto* child : scene.GetEntityByPersistentID(id)->GetChildren()) {
            if (targets.count(child->GetPersistentID())) continue;
            command->m_After[positions.at(child->GetPersistentID())].siblingIndex = position++;
        }
        counts[id] = position;
    }
    for (const auto* entity : moved) {
        auto& link = command->m_After[positions.at(entity->GetPersistentID())];
        link.parentPersistentID = parent ? parent->GetPersistentID() : std::string{};
        link.siblingIndex = parent ? counts[parent->GetPersistentID()]++ : std::numeric_limits<size_t>::max();
    }
    for (const auto& id : command->m_Parents) {
        command->m_BeforeCounts.push_back(scene.GetEntityByPersistentID(id)->GetChildren().size());
        command->m_AfterCounts.push_back(counts.at(id));
    }
    if (command->ByteSize() > 8u * 1024u * 1024u) return {};
    return command;
}

bool HierarchyEditCommand::Matches(const Scene& scene, const std::vector<EntityParentLink>& links, const std::vector<size_t>& counts) const {
    // A new or removed sibling outside this journal is a chronology barrier.
    // Do not silently move it while restoring an older sibling order.
    for (size_t index = 0; index < m_Parents.size(); ++index) {
        const auto* parent = scene.GetEntityByPersistentID(m_Parents[index]);
        if (!parent || parent->GetChildren().size() != counts[index]) return false;
    }
    for (const auto& link : links) {
        const auto* child = scene.GetEntityByPersistentID(link.childPersistentID);
        if (!child) return false;
        const auto* parent = child->GetParent();
        if (!parent) { if (!link.parentPersistentID.empty()) return false; }
        else if (parent->GetPersistentID() != link.parentPersistentID ||
                 link.siblingIndex >= parent->GetChildren().size() ||
                 parent->GetChildren()[link.siblingIndex] != child) return false;
    }
    return true;
}

bool HierarchyEditCommand::CanApply(const Scene& scene, bool undo) const {
    const auto lifetime = m_Lifetime.lock();
    return &scene == m_Scene && lifetime && *lifetime == &scene &&
        lifetime == scene.GetLifetimeToken().lock() && !scene.IsIterating() &&
        Matches(scene, undo ? m_After : m_Before, undo ? m_AfterCounts : m_BeforeCounts);
}
ActionResult HierarchyEditCommand::Apply(Scene& scene, bool undo) {
    if (!CanApply(scene, undo)) return {ActionResult::Status::Blocked};
    std::vector<std::string> order;
    order.reserve(scene.GetEntities().size());
    for (const auto& entity : scene.GetEntities()) order.push_back(entity->GetPersistentID());
    if (!scene.CommitEntityEdit({}, {}, {}, undo ? m_Before : m_After, order))
        return {ActionResult::Status::Blocked};
    return {ActionResult::Status::Applied, scene.GetEntityByPersistentID(m_Primary), m_Selection};
}
size_t HierarchyEditCommand::ByteSize() const noexcept {
    size_t bytes = sizeof(*this) + m_Primary.capacity() + m_Parents.capacity() * sizeof(std::string);
    bytes += (m_BeforeCounts.capacity() + m_AfterCounts.capacity()) * sizeof(size_t);
    for (const auto& id : m_Parents) bytes += id.capacity();
    for (const auto* links : {&m_Before, &m_After}) {
        bytes += links->capacity() * sizeof(EntityParentLink);
        for (const auto& link : *links) bytes += link.childPersistentID.capacity() + link.parentPersistentID.capacity();
    }
    if (m_Selection) {
        bytes += 64 + sizeof(*m_Selection) + m_Selection->capacity() * sizeof(std::string);
        for (const auto& id : *m_Selection) bytes += id.capacity();
    }
    return bytes;
}
} // namespace Archura
