#pragma once

#include "TransformUndoHistory.h"
#include "../core/WeaponData.h"
#include <type_traits>

namespace Archura {
namespace ComponentHistory {
inline bool Equal(const BoxCollider& a, const BoxCollider& b) {
    return a.size == b.size && a.center == b.center && a.isTrigger == b.isTrigger &&
           a.shape == b.shape && a.radius == b.radius;
}
inline bool Equal(const RigidBody& a, const RigidBody& b) {
    return a.velocity == b.velocity && a.force == b.force && a.mass == b.mass &&
           a.drag == b.drag && a.restitution == b.restitution && a.friction == b.friction &&
           a.useGravity == b.useGravity && a.isKinematic == b.isKinematic &&
           a.continuous == b.continuous && a.canBePushed == b.canBePushed &&
           a.angularVelocity == b.angularVelocity && a.angularDrag == b.angularDrag;
}
inline bool Equal(const ScriptComponent& a, const ScriptComponent& b) { return a.className == b.className; }
inline bool Equal(const LightComponent& a, const LightComponent& b) {
    return a.type == b.type && a.color == b.color && a.intensity == b.intensity && a.range == b.range;
}
inline bool EqualStats(const Weapon::WeaponStats& a, const Weapon::WeaponStats& b) {
    return a.damage == b.damage && a.fireRate == b.fireRate && a.range == b.range &&
        a.magSize == b.magSize && a.currentMag == b.currentMag && a.totalAmmo == b.totalAmmo &&
        a.reloadTime == b.reloadTime && a.recoilAmount == b.recoilAmount && a.isAutomatic == b.isAutomatic &&
        a.pattern.offsets == b.pattern.offsets;
}
inline bool Equal(const Weapon& a, const Weapon& b) {
    if (a.type != b.type || !EqualStats(a.stats, b.stats) || a.inventory.size() != b.inventory.size()) return false;
    auto left = a.inventory.begin(), right = b.inventory.begin();
    for (; left != a.inventory.end(); ++left, ++right)
        if (left->first != right->first || !EqualStats(left->second, right->second)) return false;
    return true;
}
inline bool Equal(const SkyboxComponent& a, const SkyboxComponent& b) { return a.facePaths == b.facePaths; }
inline void Normalize(Weapon& value) {
    value.timeSinceLastShot = value.reloadTimer = value.currentRecoil = 0;
    value.isReloading = false; value.shotsFired = 0;
}
inline void Normalize(SkyboxComponent& value) { value.shouldReload = false; }
template<class T> bool Valid(const T&) { return true; }
inline bool Valid(const Weapon& value) { return Serialization::IsValidWeapon(value); }
inline bool Valid(const SkyboxComponent& value) {
    if (value.facePaths.size() != 6) return false;
    for (const auto& face : value.facePaths) if (face.find('\0') != std::string::npos) return false;
    return true;
}
template<class T> void Publish(T& live, T&& prepared) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<T>, "Value publication must not throw");
    live = std::move(prepared);
}
inline void Publish(Weapon& live, Weapon&& prepared) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<Weapon::WeaponStats>);
    static_assert(noexcept(live.inventory.swap(prepared.inventory)));
    live.type = prepared.type;
    live.stats = std::move(prepared.stats);
    live.inventory.swap(prepared.inventory); // Preserve firing/reload transients.
}
inline void Publish(SkyboxComponent& live, SkyboxComponent&& prepared) noexcept {
    live.facePaths.swap(prepared.facePaths);
    live.shouldReload = true; // Both Undo and Redo invalidate the displayed cubemap.
}
inline void Normalize(LightComponent&) {}
inline void Normalize(BoxCollider&) {}
inline void Normalize(RigidBody&) {}
inline void Normalize(ScriptComponent& value) { value.scriptInstanceID = -1; }
template<class T> size_t Bytes(const std::optional<T>&) noexcept { return 0; }
template<> inline size_t Bytes(const std::optional<ScriptComponent>& value) noexcept {
    return value ? value->className.capacity() : 0;
}
template<> inline size_t Bytes(const std::optional<Weapon>& value) noexcept {
    if (!value) return 0;
    // Conservative allowance for map nodes and vendor container proxies.
    size_t bytes = 512 + value->stats.pattern.offsets.capacity() * sizeof(glm::vec2);
    for (const auto& entry : value->inventory) bytes += 512 + entry.second.pattern.offsets.capacity() * sizeof(glm::vec2);
    return bytes;
}
template<> inline size_t Bytes(const std::optional<SkyboxComponent>& value) noexcept {
    if (!value) return 0;
    size_t bytes = 128 + value->facePaths.capacity() * sizeof(std::string);
    for (const auto& face : value->facePaths) bytes += face.capacity();
    return bytes;
}
}

// Intentionally restricted to asset-free authoring values. Script runtime
// instance IDs never enter history, including when a Play/Stop replaces handles.
template<class T>
class ComponentEditCommand final : public EditorHistoryCommand {
    static_assert(std::is_same_v<T, BoxCollider> || std::is_same_v<T, RigidBody> ||
                  std::is_same_v<T, ScriptComponent> || std::is_same_v<T, LightComponent> ||
                  std::is_same_v<T, Weapon> || std::is_same_v<T, SkyboxComponent>, "Unsupported component recipe");
public:
    static std::shared_ptr<ComponentEditCommand> Create(Scene& scene, EntityHandle handle,
        std::optional<T> desired, std::vector<std::string> selection = {}, std::string primary = {}) {
        auto* entity = scene.GetEntity(handle);
        if (!entity || scene.IsIterating()) return {};
        std::optional<T> before;
        if (const auto* value = entity->GetComponent<T>()) before = *value;
        if (before) ComponentHistory::Normalize(*before);
        if (desired) ComponentHistory::Normalize(*desired);
        if ((before && !ComponentHistory::Valid(*before)) || (desired && !ComponentHistory::Valid(*desired)) ||
            Same(before ? &*before : nullptr, desired)) return {};
        auto command = std::shared_ptr<ComponentEditCommand>(new ComponentEditCommand);
        command->m_Scene = &scene;
        command->m_Lifetime = scene.GetLifetimeToken();
        command->m_ID = entity->GetPersistentID();
        command->m_Before = std::move(before);
        command->m_After = std::move(desired);
        command->m_Selection = std::make_shared<const std::vector<std::string>>(std::move(selection));
        command->m_Primary = std::move(primary);
        if (command->ByteSize() > 1024u * 1024u) return {};
        return command;
    }
    bool CanApply(const Scene& scene, bool undo) const override {
        const auto lifetime = m_Lifetime.lock();
        if (&scene != m_Scene || !lifetime || *lifetime != &scene ||
            lifetime != scene.GetLifetimeToken().lock() || scene.IsIterating()) return false;
        const auto* entity = scene.GetEntityByPersistentID(m_ID);
        return entity && Same(entity->GetComponent<T>(), undo ? m_After : m_Before);
    }
    ActionResult Apply(Scene& scene, bool undo) override {
        if (!CanApply(scene, undo)) return {ActionResult::Status::Blocked};
        auto* entity = scene.GetEntityByPersistentID(m_ID);
        const auto& desired = undo ? m_Before : m_After;
        if (desired) {
            T prepared(*desired); // All string allocation precedes live mutation.
            ComponentHistory::Normalize(prepared);
            if (auto* live = entity->GetComponent<T>()) {
                ComponentHistory::Publish(*live, std::move(prepared));
            } else {
                auto* added = entity->AddComponent<T>(std::move(prepared));
                if constexpr (std::is_same_v<T, SkyboxComponent>) added->shouldReload = true;
            }
        } else entity->RemoveComponent<T>();
        return {ActionResult::Status::Applied, scene.GetEntityByPersistentID(m_Primary), m_Selection};
    }
    bool HasChanges() const noexcept override { return !Same(m_Before ? &*m_Before : nullptr, m_After); }
    std::shared_ptr<EditorHistoryCommand> MergeWith(const EditorHistoryCommand& next) const override {
        const auto* delta = dynamic_cast<const ComponentEditCommand*>(&next);
        if (!delta || m_Scene != delta->m_Scene || m_Lifetime.lock() != delta->m_Lifetime.lock() ||
            m_ID != delta->m_ID || !Same(m_After ? &*m_After : nullptr, delta->m_Before)) return {};
        auto combined = std::make_shared<ComponentEditCommand>(*this);
        combined->m_After = delta->m_After;
        return combined;
    }
    size_t ByteSize() const noexcept override {
        size_t bytes = sizeof(*this) + m_ID.capacity() + m_Primary.capacity() +
                       ComponentHistory::Bytes(m_Before) + ComponentHistory::Bytes(m_After);
        if (m_Selection) {
            bytes += 64 + sizeof(*m_Selection) + m_Selection->capacity() * sizeof(std::string);
            for (const auto& id : *m_Selection) bytes += id.capacity();
        }
        return bytes;
    }
    const char* Label() const noexcept override {
        if constexpr (std::is_same_v<T, BoxCollider>)
            return !m_Before ? "Add Collider" : !m_After ? "Remove Collider" : "Change Collider";
        else if constexpr (std::is_same_v<T, RigidBody>)
            return !m_Before ? "Add RigidBody" : !m_After ? "Remove RigidBody" : "Change RigidBody";
        else if constexpr (std::is_same_v<T, LightComponent>) return "Change Light";
        else if constexpr (std::is_same_v<T, Weapon>) return "Change Weapon";
        else if constexpr (std::is_same_v<T, SkyboxComponent>) return "Change Skybox Faces";
        else return !m_Before ? "Add Script" : !m_After ? "Remove Script" : "Change Script";
    }
private:
    static bool Same(const T* live, const std::optional<T>& expected) {
        return live ? expected && ComponentHistory::Equal(*live, *expected) : !expected;
    }
    Scene* m_Scene = nullptr;
    std::weak_ptr<Scene*> m_Lifetime;
    std::string m_ID, m_Primary;
    std::optional<T> m_Before, m_After;
    std::shared_ptr<const std::vector<std::string>> m_Selection;
};
} // namespace Archura
