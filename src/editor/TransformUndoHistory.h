#pragma once

#include "../ecs/Entity.h"
#include <deque>
#include <exception>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace Archura {

struct ActionResult {
    enum class Status { Applied, Blocked, Empty };
    Status status = Status::Empty;
    Entity* selectedEntity = nullptr;
    // Prepared before mutation by a command; null means use selectedEntity.
    // An empty list explicitly clears selection. Shared CPU data survives the
    // command moving between history queues without allocating after commit.
    std::shared_ptr<const std::vector<std::string>> selectionIDs;
};

// Implementations own CPU values/recipes and persistent IDs, never Scene/entity
// or GPU ownership. A failed/throwing Apply must leave the Scene unchanged.
// ByteSize is an upper bound for all owned payload. The initial Apply may
// shrink a provisional reservation; after first publication it remains stable.
class EditorHistoryCommand {
public:
    virtual ~EditorHistoryCommand() = default;
    virtual bool CanApply(const Scene& scene, bool undo) const = 0;
    virtual ActionResult Apply(Scene& scene, bool undo) = 0;
    virtual size_t ByteSize() const noexcept = 0;
    virtual const char* Label() const noexcept = 0;
    virtual bool HasChanges() const noexcept { return true; }
    virtual std::shared_ptr<EditorHistoryCommand> MergeWith(const EditorHistoryCommand&) const { return {}; }
};

// Compatibility name: transform captures and entity commands share one bounded
// chronological journal. A lazy transform capture groups an entire interaction
// and does not discard Redo until a real transform delta has been committed.
class TransformUndoHistory {
public:
    static constexpr size_t MaxHistoryBytes = 32u * 1024u * 1024u;
    static constexpr size_t MaxEntries = 50;
    static constexpr size_t MaxGestureBytes = 1024u * 1024u;

    bool Capture(const Entity& entity) {
        const auto* owner = entity.GetScene();
        const auto* transform = entity.GetComponent<Transform>();
        if (!owner || !transform || owner->GetEntity(entity.GetHandle()) != &entity) return false;
        Snapshot before = Read(entity, *transform);
        if (SnapshotBytes(before) > PendingReserveBytes) return false;
        if (!Matches(*owner)) Bind(*owner);
        FinalizeGesture();
        FinalizePending(*owner);
        m_Pending = std::move(before);
        return true;
    }

    ActionResult Execute(Scene& scene, std::shared_ptr<EditorHistoryCommand> command) {
        if (!command || CommandBytes(*command) > CompletedBudget) return Blocked();
        try {
            if (!command->CanApply(scene, false)) return Blocked();
            // All journal storage and pending-transform publication are prepared
            // in a candidate. Failure preserves the actual queues and binding.
            TransformUndoHistory candidate(*this);
            if (!candidate.Matches(scene)) candidate.Bind(scene);
            candidate.FinalizeGesture();
            candidate.FinalizePending(scene);
            candidate.m_Undo.emplace_back(command);
            ActionResult result = command->Apply(scene, false);
            if (result.status != ActionResult::Status::Applied) return Blocked();
            candidate.m_Undo.back().bytes = CommandBytes(*command);
            candidate.m_Redo.clear();
            candidate.RebindRestoredScene(scene);
            candidate.Trim();
            Swap(candidate);
            return result;
        } catch (const std::exception&) {
            return Blocked();
        }
    }

    // Live deltas are applied transactionally while the immutable before/after
    // recipe remains pending. A return to the original value keeps Redo intact.
    ActionResult ExecuteGesture(Scene& scene, std::shared_ptr<EditorHistoryCommand> delta, uint64_t key) {
        if (!key || !delta || CommandBytes(*delta) > MaxGestureBytes) return Blocked();
        try {
            if (!delta->CanApply(scene, false)) return Blocked();
            TransformUndoHistory candidate(*this);
            if (!candidate.Matches(scene)) candidate.Bind(scene);
            if (candidate.m_Gesture && candidate.m_GestureKey != key) candidate.FinalizeGesture();
            candidate.FinalizePending(scene);
            auto combined = candidate.m_Gesture ? candidate.m_Gesture->MergeWith(*delta) : delta;
            if (!combined || CommandBytes(*combined) > MaxGestureBytes) return Blocked();
            const auto result = delta->Apply(scene, false);
            if (result.status != ActionResult::Status::Applied) return Blocked();
            candidate.m_Gesture = std::move(combined);
            candidate.m_GestureKey = key;
            Swap(candidate);
            return result;
        } catch (const std::exception&) { return Blocked(); }
    }
    bool EndGesture(const Scene& scene, uint64_t key = 0) {
        if (!Matches(scene) || (key && key != m_GestureKey)) return false;
        try {
            TransformUndoHistory candidate(*this);
            candidate.FinalizeGesture();
            Swap(candidate);
            return true;
        } catch (const std::exception&) { return false; }
    }

    ActionResult UndoAction(Scene& scene) { return ApplyAction(scene, true); }
    ActionResult RedoAction(Scene& scene) { return ApplyAction(scene, false); }
    Entity* Undo(Scene& scene) { return UndoAction(scene).selectedEntity; }
    Entity* Redo(Scene& scene) { return RedoAction(scene).selectedEntity; }

    // Queries must not finalize an activation frame before a drag's first delta.
    // A blocked generic command is a chronology barrier, not a skippable entry.
    bool CanUndo(const Scene& scene) const {
        if (!Matches(scene)) return false;
        if (GestureChanged()) return m_Gesture->CanApply(scene, true);
        return PendingChanged(scene) || CanApply(scene, m_Undo, true);
    }
    bool CanRedo(const Scene& scene) const {
        return Matches(scene) && !GestureChanged() && !PendingChanged(scene) && CanApply(scene, m_Redo, false);
    }
    const char* UndoLabel(const Scene& scene) const {
        if (!Matches(scene)) return "";
        if (GestureChanged()) return m_Gesture->Label();
        return PendingChanged(scene) ? "Transform" : Label(scene, m_Undo);
    }
    const char* RedoLabel(const Scene& scene) const {
        return Matches(scene) && !GestureChanged() && !PendingChanged(scene) ? Label(scene, m_Redo) : "";
    }
    size_t RetainedBytes() const noexcept {
        return QueueBytes(m_Undo) + QueueBytes(m_Redo) + (m_Pending ? SnapshotBytes(*m_Pending) : 0) +
               (m_Gesture ? CommandBytes(*m_Gesture) : 0);
    }

    void RebindRestoredScene(const Scene& scene) {
        if (!Matches(scene)) { Clear(); return; }
        const auto rebind = [&](Snapshot& snapshot) {
            const auto* entity = scene.GetEntityByPersistentID(snapshot.persistentID);
            snapshot.handle = entity && entity->GetComponent<Transform>() ? entity->GetHandle() : EntityHandle{};
        };
        for (auto& entry : m_Undo) if (auto* snapshot = std::get_if<Snapshot>(&entry.value)) rebind(*snapshot);
        for (auto& entry : m_Redo) if (auto* snapshot = std::get_if<Snapshot>(&entry.value)) rebind(*snapshot);
        if (m_Pending) rebind(*m_Pending);
    }

    void Clear() {
        m_Undo.clear();
        m_Redo.clear();
        m_Pending.reset();
        m_Gesture.reset();
        m_GestureKey = 0;
        m_Owner = nullptr;
        m_Lifetime.reset();
    }

private:
    struct Snapshot {
        EntityHandle handle;
        std::string persistentID;
        glm::vec3 position, rotation, scale;
    };
    struct Entry {
        std::variant<Snapshot, std::shared_ptr<EditorHistoryCommand>> value;
        size_t bytes;
        explicit Entry(Snapshot snapshot)
            : value(std::move(snapshot)), bytes(sizeof(Entry) + std::get<Snapshot>(value).persistentID.capacity()) {}
        explicit Entry(std::shared_ptr<EditorHistoryCommand> command)
            : value(std::move(command)), bytes(CommandBytes(*std::get<std::shared_ptr<EditorHistoryCommand>>(value))) {}
    };
    // Reserve pending transform + immutable component gesture storage. Neither
    // activation nor a return-to-origin gesture may evict the existing Redo branch.
    static constexpr size_t PendingReserveBytes = 1024;
    static constexpr size_t CompletedBudget = MaxHistoryBytes - PendingReserveBytes - MaxGestureBytes;

    static ActionResult Blocked() { return {ActionResult::Status::Blocked, nullptr, {}}; }
    void Bind(const Scene& scene) {
        Clear();
        m_Owner = &scene;
        m_Lifetime = scene.GetLifetimeToken();
    }
    bool Matches(const Scene& scene) const {
        if (m_Owner != &scene) return false;
        const auto lifetime = m_Lifetime.lock();
        return lifetime && *lifetime == &scene && lifetime == scene.GetLifetimeToken().lock();
    }
    static Snapshot Read(const Entity& entity, const Transform& transform) {
        return {entity.GetHandle(), entity.GetPersistentID(), transform.position, transform.rotation, transform.scale};
    }
    static size_t SnapshotBytes(const Snapshot& snapshot) noexcept { return sizeof(Snapshot) + snapshot.persistentID.capacity(); }
    static size_t CommandBytes(const EditorHistoryCommand& command) noexcept {
        const size_t payload = command.ByteSize();
        return payload > MaxHistoryBytes - sizeof(Entry) ? MaxHistoryBytes + 1 : sizeof(Entry) + payload;
    }
    static size_t QueueBytes(const std::deque<Entry>& entries) noexcept {
        size_t total = 0;
        for (const auto& entry : entries) total += entry.bytes;
        return total;
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
    static const Entry* Next(const Scene& scene, const std::deque<Entry>& entries) {
        for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
            if (const auto* snapshot = std::get_if<Snapshot>(&entry->value)) {
                const auto* entity = Resolve(scene, *snapshot);
                if (entity && Different(*entity->GetComponent<Transform>(), *snapshot)) return &*entry;
            } else return &*entry;
        }
        return nullptr;
    }
    static bool CanApply(const Scene& scene, const std::deque<Entry>& entries, bool undo) {
        const auto* entry = Next(scene, entries);
        if (!entry) return false;
        if (const auto* command = std::get_if<std::shared_ptr<EditorHistoryCommand>>(&entry->value)) {
            try { return (*command)->CanApply(scene, undo); }
            catch (const std::exception&) { return false; }
        }
        return true;
    }
    static const char* Label(const Scene& scene, const std::deque<Entry>& entries) {
        const auto* entry = Next(scene, entries);
        if (!entry) return "";
        const auto* command = std::get_if<std::shared_ptr<EditorHistoryCommand>>(&entry->value);
        return command ? (*command)->Label() : "Transform";
    }
    void Trim() noexcept {
        while (m_Undo.size() + m_Redo.size() > MaxEntries || QueueBytes(m_Undo) + QueueBytes(m_Redo) > CompletedBudget) {
            if (!m_Undo.empty()) m_Undo.pop_front();
            else if (!m_Redo.empty()) m_Redo.pop_front();
            else break;
        }
    }
    bool GestureChanged() const noexcept { return m_Gesture && m_Gesture->HasChanges(); }
    void FinalizeGesture() {
        if (GestureChanged()) {
            m_Undo.emplace_back(m_Gesture);
            m_Redo.clear();
        }
        m_Gesture.reset();
        m_GestureKey = 0;
        Trim();
    }
    void FinalizePending(const Scene& scene) {
        if (!m_Pending) return;
        if (PendingChanged(scene)) {
            m_Undo.emplace_back(*m_Pending);
            m_Redo.clear();
        }
        m_Pending.reset();
        Trim();
    }
    void Swap(TransformUndoHistory& other) noexcept {
        static_assert(noexcept(m_Undo.swap(other.m_Undo)), "Journal publication must not allocate");
        static_assert(noexcept(m_Pending.swap(other.m_Pending)), "Pending publication must not allocate");
        m_Undo.swap(other.m_Undo);
        m_Redo.swap(other.m_Redo);
        m_Pending.swap(other.m_Pending);
        m_Gesture.swap(other.m_Gesture);
        std::swap(m_GestureKey, other.m_GestureKey);
        std::swap(m_Owner, other.m_Owner);
        m_Lifetime.swap(other.m_Lifetime);
    }
    ActionResult ApplyAction(Scene& scene, bool undo) {
        if (!Matches(scene)) { Clear(); return {}; }
        try {
            TransformUndoHistory candidate(*this);
            candidate.FinalizeGesture();
            candidate.FinalizePending(scene);
            ActionResult result = candidate.Apply(scene, undo);
            if (result.status == ActionResult::Status::Blocked) return result;
            candidate.Trim();
            Swap(candidate);
            return result;
        } catch (const std::exception&) {
            return Blocked();
        }
    }
    ActionResult Apply(Scene& scene, bool undo) {
        auto& source = undo ? m_Undo : m_Redo;
        auto& destination = undo ? m_Redo : m_Undo;
        while (!source.empty()) {
            const auto& entry = source.back();
            if (const auto* command = std::get_if<std::shared_ptr<EditorHistoryCommand>>(&entry.value)) {
                if (!(*command)->CanApply(scene, undo)) return Blocked();
                destination.push_back(entry); // Allocate before any scene change.
                ActionResult result = (*command)->Apply(scene, undo);
                if (result.status != ActionResult::Status::Applied) return Blocked();
                source.pop_back();
                RebindRestoredScene(scene);
                return result;
            }
            const auto& desired = std::get<Snapshot>(entry.value);
            const auto* resolved = Resolve(scene, desired);
            if (!resolved || !Different(*resolved->GetComponent<Transform>(), desired)) {
                source.pop_back();
                continue;
            }
            auto* entity = scene.GetEntity(desired.handle);
            auto* transform = entity->GetComponent<Transform>();
            destination.emplace_back(Read(*entity, *transform));
            transform->position = desired.position;
            transform->rotation = desired.rotation;
            transform->scale = desired.scale;
            source.pop_back();
            return {ActionResult::Status::Applied, entity, {}};
        }
        return {};
    }

    const Scene* m_Owner = nullptr;
    std::weak_ptr<Scene*> m_Lifetime;
    std::deque<Entry> m_Undo, m_Redo;
    std::optional<Snapshot> m_Pending;
    std::shared_ptr<EditorHistoryCommand> m_Gesture;
    uint64_t m_GestureKey = 0;
};

} // namespace Archura
