#include "EntityEditCommand.h"
#include "../core/GameSaveManager.h"
#include "../core/ResourceManager.h"
#include "../core/SerializationJson.h"
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <streambuf>
#include <unordered_set>

namespace Archura {
namespace {
constexpr size_t MaxBatchBytes = 4u * 1024u * 1024u;
constexpr size_t MaxTargets = 2048;
const std::string EmptyBatch = "{\"format_version\":2,\"entities\":[]}";

std::vector<std::string> Order(const Scene& scene) {
    std::vector<std::string> result;
    result.reserve(scene.GetEntities().size());
    for (const auto& entity : scene.GetEntities()) result.push_back(entity->GetPersistentID());
    return result;
}

EntityParentLink ParentLink(const Entity& entity) {
    EntityParentLink link;
    link.childPersistentID = entity.GetPersistentID();
    if (const auto* parent = entity.GetParent()) {
        link.parentPersistentID = parent->GetPersistentID();
        const auto& children = parent->GetChildren();
        const auto position = std::find(children.begin(), children.end(), &entity);
        link.siblingIndex = static_cast<size_t>(position - children.begin());
    }
    return link;
}

bool CapturePayload(Scene& scene, const std::vector<EntityHandle>& handles, std::string& result) {
    class Buffer final : public std::streambuf {
    public:
        std::string data;
    private:
        std::streamsize xsputn(const char* text, std::streamsize count) override {
            if (count < 0 || static_cast<size_t>(count) > MaxBatchBytes - data.size()) return 0;
            data.append(text, static_cast<size_t>(count));
            return count;
        }
        int_type overflow(int_type ch) override {
            if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
            if (data.size() == MaxBatchBytes) return traits_type::eof();
            data.push_back(traits_type::to_char_type(ch));
            return ch;
        }
    } buffer;
    std::ostream stream(&buffer);
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<float>::max_digits10)
           << "{\"format_version\":2,\"entities\":[";
    if (!GameSaveManager::WriteEntityBatch(stream, &scene, handles, false)) return false;
    stream << "]}";
    if (!stream) return false;
    result = std::move(buffer.data);
    return true;
}

size_t StringsBytes(const std::vector<std::string>& values) noexcept {
    size_t bytes = values.capacity() * sizeof(std::string);
    for (const auto& value : values) bytes += value.capacity();
    return bytes;
}
}

EntityEditCommand::EntityEditCommand(Scene& scene, const std::filesystem::path& documentRoot)
    : m_Scene(&scene), m_Lifetime(scene.GetLifetimeToken()), m_DocumentRoot(documentRoot),
      m_ResourceRoot(ResourceManager::Get().GetProjectRoot()),
      m_ExplicitRoot(ResourceManager::Get().HasExplicitContentRoot()) {
    // Diagnostics must not grow a published command's retained byte budget.
    m_Error.reserve(192);
}

bool EntityEditCommand::CaptureState(Scene& scene, State& state, const std::vector<EntityHandle>& targets) {
    if (targets.size() > MaxTargets || !CapturePayload(scene, targets, state.payload)) return false;
    state.present = !targets.empty();
    state.order = Order(scene);
    std::unordered_set<std::string> recorded;
    for (const auto handle : targets) {
        const auto* entity = scene.GetEntity(handle);
        if (!entity) return false;
        if (recorded.insert(entity->GetPersistentID()).second) state.links.push_back(ParentLink(*entity));
        std::vector<std::string> children;
        for (const auto* child : entity->GetChildren()) children.push_back(child->GetPersistentID());
        state.children.emplace_back(entity->GetPersistentID(), std::move(children));
    }
    // Deleting a parent detaches, rather than deletes, surviving children. Their
    // links are part of the same transaction even though their values are retained.
    for (const auto handle : targets) {
        const auto* entity = scene.GetEntity(handle);
        for (const auto* child : entity->GetChildren())
            if (recorded.insert(child->GetPersistentID()).second) state.links.push_back(ParentLink(*child));
    }
    return true;
}

std::shared_ptr<EntityEditCommand> EntityEditCommand::Creation(
    Scene& scene, std::function<void()> create, std::vector<std::string> selection,
    std::string primary, const std::filesystem::path& documentRoot, std::string& error) {
    error.clear();
    if (scene.IsIterating() || !create) { error = "Creation is unavailable during scene traversal."; return {}; }
    auto command = std::shared_ptr<EntityEditCommand>(new EntityEditCommand(scene, documentRoot));
    command->m_IsCreation = true;
    command->m_Create = std::move(create);
    command->m_Before.payload = EmptyBatch;
    command->m_Before.order = Order(scene);
    command->m_Before.selection = std::make_shared<const std::vector<std::string>>(std::move(selection));
    command->m_Before.primary = std::move(primary);
    if (command->PayloadBytes() > MaxCommandBytes) { error = "The scene order exceeds the command history limit."; return {}; }
    return command;
}

std::shared_ptr<EntityEditCommand> EntityEditCommand::Deletion(
    Scene& scene, const std::vector<EntityHandle>& targets, std::vector<std::string> selection,
    std::string primary, const std::filesystem::path& documentRoot, std::string& error) {
    error.clear();
    if (scene.IsIterating() || targets.empty() || targets.size() > MaxTargets) {
        error = "Choose live entities outside scene traversal (at most 2048 per command).";
        return {};
    }
    auto command = std::shared_ptr<EntityEditCommand>(new EntityEditCommand(scene, documentRoot));
    if (!command->CaptureState(scene, command->m_Before, targets)) {
        error = "Cannot capture these entities for Undo: unsupported scene data or command size limit.";
        return {};
    }
    for (const auto handle : targets) command->m_Targets.push_back(scene.GetEntity(handle)->GetPersistentID());
    command->m_Before.selection = std::make_shared<const std::vector<std::string>>(selection);
    command->m_Before.primary = std::move(primary);
    const std::unordered_set<std::string> removed(command->m_Targets.begin(), command->m_Targets.end());
    command->m_After.payload = EmptyBatch;
    for (const auto& id : command->m_Before.order) if (!removed.count(id)) command->m_After.order.push_back(id);
    for (const auto& link : command->m_Before.links) {
        if (removed.count(link.childPersistentID)) continue;
        command->m_After.links.push_back({link.childPersistentID, {}, std::numeric_limits<size_t>::max()});
    }
    selection.erase(std::remove_if(selection.begin(), selection.end(), [&](const auto& id) { return removed.count(id) != 0; }), selection.end());
    command->m_After.selection = std::make_shared<const std::vector<std::string>>(std::move(selection));
    if (command->m_After.selection && !command->m_After.selection->empty()) command->m_After.primary = command->m_After.selection->back();
    command->m_Ready = true;
    if (command->PayloadBytes() > MaxCommandBytes) { error = "The entity command exceeds its 8 MiB history limit."; return {}; }
    return command;
}

bool EntityEditCommand::MatchesState(const Scene& scene, const State& state) const {
    for (const auto& id : m_Targets)
        if ((scene.GetEntityByPersistentID(id) != nullptr) != state.present) return false;
    for (const auto& link : state.links) {
        const auto* child = scene.GetEntityByPersistentID(link.childPersistentID);
        if (!child) return false;
        const auto* parent = child->GetParent();
        if ((parent ? parent->GetPersistentID() : std::string{}) != link.parentPersistentID) return false;
        if (parent) {
            const auto& siblings = parent->GetChildren();
            if (link.siblingIndex >= siblings.size() || siblings[link.siblingIndex] != child) return false;
        }
    }
    for (const auto& children : state.children) {
        const auto* parent = scene.GetEntityByPersistentID(children.first);
        if (!parent || parent->GetChildren().size() != children.second.size()) return false;
        for (size_t index = 0; index < children.second.size(); ++index)
            if (parent->GetChildren()[index]->GetPersistentID() != children.second[index]) return false;
    }
    return true;
}

bool EntityEditCommand::CanApply(const Scene& scene, bool undo) const {
    const auto lifetime = m_Lifetime.lock();
    if (&scene != m_Scene || !lifetime || *lifetime != &scene ||
        lifetime != scene.GetLifetimeToken().lock() || scene.IsIterating() ||
        ResourceManager::Get().GetProjectRoot() != m_ResourceRoot ||
        ResourceManager::Get().HasExplicitContentRoot() != m_ExplicitRoot) return false;
    if (!m_Ready) return m_IsCreation && !undo && static_cast<bool>(m_Create);
    return MatchesState(scene, undo ? m_After : m_Before);
}

ActionResult EntityEditCommand::Result(Scene& scene, const State& state) const {
    return {ActionResult::Status::Applied, scene.GetEntityByPersistentID(state.primary), state.selection};
}

ActionResult EntityEditCommand::CreateInitial(Scene& scene) {
    // Only synchronous, create-only editor callbacks are accepted. Save live
    // handles before invoking one, so every partial creation can be removed
    // without allocating even when construction or snapshot capture throws.
    std::unordered_set<uint64_t> original;
    original.reserve(scene.GetEntities().size());
    for (const auto& entity : scene.GetEntities()) original.insert(entity->GetHandle().Value());
    const auto rollback = [&]() noexcept {
        for (size_t index = scene.GetEntities().size(); index > 0; --index) {
            const auto handle = scene.GetEntities()[index - 1]->GetHandle();
            if (!original.count(handle.Value()))
                scene.DestroyEntity(handle);
        }
    };
    try {
        m_Create();
        std::vector<EntityHandle> created;
        for (const auto& entity : scene.GetEntities())
            if (!original.count(entity->GetHandle().Value()))
                created.push_back(entity->GetHandle());
        State after;
        if (created.empty() || !CaptureState(scene, after, created))
            throw std::runtime_error("Creation cannot be captured for Undo.");
        std::vector<std::string> targets;
        for (const auto handle : created) targets.push_back(scene.GetEntity(handle)->GetPersistentID());
        after.selection = std::make_shared<const std::vector<std::string>>(targets);
        after.primary = targets.back();
        m_Targets.swap(targets);
        std::swap(m_After, after);
        if (PayloadBytes() > MaxCommandBytes) {
            std::swap(m_After, after);
            m_Targets.swap(targets);
            throw std::runtime_error("Creation exceeds the 8 MiB history limit.");
        }
        m_Ready = true;
        m_Create = {};
        return Result(scene, m_After);
    } catch (...) {
        rollback();
        throw;
    }
}

ActionResult EntityEditCommand::Apply(Scene& scene, bool undo) {
    if (!CanApply(scene, undo)) { m_Error = "The entities, hierarchy or project context changed outside this command."; return {ActionResult::Status::Blocked}; }
    if (!m_Ready) return CreateInitial(scene);
    const auto& expected = undo ? m_After : m_Before;
    const auto& desired = undo ? m_Before : m_After;
    std::vector<EntityHandle> replaced;
    for (const auto& id : m_Targets) if (const auto* entity = scene.GetEntityByPersistentID(id)) replaced.push_back(entity->GetHandle());
    if (expected.present) {
        std::string current;
        if (!CapturePayload(scene, replaced, current) || current != expected.payload) {
            m_Error = "These entity values changed outside the command; Undo/Redo was retained.";
            return {ActionResult::Status::Blocked};
        }
    }
    const auto order = FinalOrder(scene, desired);
    Serialization::Json root;
    if (!Serialization::JsonParser(desired.payload).Parse(root) ||
        !GameSaveManager::LoadEntityBatch(root, &scene, replaced, desired.links, order, m_DocumentRoot)) {
        m_Error = "Entity reconstruction failed. Check referenced assets; the current scene and history were retained.";
        return {ActionResult::Status::Blocked};
    }
    return Result(scene, desired);
}

std::vector<std::string> EntityEditCommand::FinalOrder(const Scene& scene, const State& desired) const {
    const std::unordered_set<std::string> targets(m_Targets.begin(), m_Targets.end());
    std::vector<std::string> result;
    result.reserve(scene.GetEntities().size() + (desired.present ? m_Targets.size() : 0));
    for (const auto& entity : scene.GetEntities())
        if (!targets.count(entity->GetPersistentID())) result.push_back(entity->GetPersistentID());
    if (desired.present) {
        for (size_t index = 0; index < desired.order.size(); ++index)
            if (targets.count(desired.order[index])) result.insert(result.begin() + std::min(index, result.size()), desired.order[index]);
    }
    return result;
}

size_t EntityEditCommand::PayloadBytes() const noexcept {
    size_t bytes = sizeof(*this) + StringsBytes(m_Targets) + m_Error.capacity();
    bytes += (m_DocumentRoot.native().capacity() + m_ResourceRoot.native().capacity()) * sizeof(std::filesystem::path::value_type);
    for (const State* state : {&m_Before, &m_After}) {
        bytes += state->payload.capacity() + state->primary.capacity() + StringsBytes(state->order);
        bytes += state->links.capacity() * sizeof(EntityParentLink);
        for (const auto& link : state->links) bytes += link.childPersistentID.capacity() + link.parentPersistentID.capacity();
        bytes += state->children.capacity() * sizeof(decltype(state->children)::value_type);
        for (const auto& children : state->children) bytes += children.first.capacity() + StringsBytes(children.second);
        if (state->selection) bytes += sizeof(std::vector<std::string>) + 64 + StringsBytes(*state->selection);
    }
    return bytes;
}

size_t EntityEditCommand::ByteSize() const noexcept { return m_Ready ? PayloadBytes() : MaxCommandBytes; }
} // namespace Archura
