#pragma once

#include "../ecs/Entity.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Archura {

struct SelectionPersistentSnapshot {
    std::string primaryPersistentID;
    std::vector<std::string> selectedPersistentIDs;
    std::string lastClickedPersistentID;
    std::string lookedAtPersistentID;
};

// Generation-checked editor selection state. Owns EntityHandles and scene
// lifetime tokens rather than trusting raw Entity* across frames or deletions.
class EditorSelectionState {
public:
    void BindScene(Scene* scene) noexcept {
        if (scene) {
            if (m_Scene && m_Scene != scene) {
                ClearHandlesOnly();
            }
            m_Scene = scene;
            m_SceneToken = scene->GetLifetimeToken();
            m_HasBoundScene = true;
        } else {
            Clear();
            m_Scene = nullptr;
            m_SceneToken.reset();
            m_HasBoundScene = false;
        }
    }

    void SetSelectedEntity(Entity* entity) {
        if (entity) {
            if (Scene* owner = entity->GetScene()) {
                BindScene(owner);
            }
            m_PrimaryHandle = entity->GetHandle();
            m_PrimaryPersistentID = entity->GetPersistentID();
            m_SelectedHandles = {m_PrimaryHandle};
            m_SelectedPersistentIDs = {m_PrimaryPersistentID};
            m_PrimaryPtr = entity;
            m_SelectedPtrs = {entity};
        } else {
            m_PrimaryHandle = {};
            m_PrimaryPersistentID.clear();
            m_SelectedHandles.clear();
            m_SelectedPersistentIDs.clear();
            m_PrimaryPtr = nullptr;
            m_SelectedPtrs.clear();
        }
        SyncLastMirrors();
    }

    void SetSelectionList(const std::vector<Entity*>& entities, Entity* primary) {
        m_SelectedHandles.clear();
        m_SelectedPersistentIDs.clear();
        m_SelectedPtrs.clear();
        for (Entity* entity : entities) {
            if (!entity) continue;
            if (Scene* owner = entity->GetScene()) {
                BindScene(owner);
            }
            const EntityHandle handle = entity->GetHandle();
            if (std::find(m_SelectedHandles.begin(), m_SelectedHandles.end(), handle) ==
                m_SelectedHandles.end()) {
                m_SelectedHandles.push_back(handle);
                m_SelectedPersistentIDs.push_back(entity->GetPersistentID());
                m_SelectedPtrs.push_back(entity);
            }
        }
        if (primary) {
            if (Scene* owner = primary->GetScene()) {
                BindScene(owner);
            }
            m_PrimaryHandle = primary->GetHandle();
            m_PrimaryPersistentID = primary->GetPersistentID();
            m_PrimaryPtr = primary;
            if (std::find(m_SelectedHandles.begin(), m_SelectedHandles.end(), m_PrimaryHandle) ==
                m_SelectedHandles.end()) {
                m_SelectedHandles.push_back(m_PrimaryHandle);
                m_SelectedPersistentIDs.push_back(m_PrimaryPersistentID);
                m_SelectedPtrs.push_back(primary);
            }
        } else if (!m_SelectedPtrs.empty()) {
            m_PrimaryPtr = m_SelectedPtrs.back();
            m_PrimaryHandle = m_PrimaryPtr->GetHandle();
            m_PrimaryPersistentID = m_PrimaryPtr->GetPersistentID();
        } else {
            m_PrimaryPtr = nullptr;
            m_PrimaryHandle = {};
            m_PrimaryPersistentID.clear();
        }
        SyncLastMirrors();
    }

    void SetLastClickedEntity(Entity* entity) noexcept {
        if (entity) {
            if (Scene* owner = entity->GetScene()) {
                BindScene(owner);
            }
            m_LastClickedHandle = entity->GetHandle();
            m_LastClickedPersistentID = entity->GetPersistentID();
            m_LastClickedPtr = entity;
        } else {
            m_LastClickedHandle = {};
            m_LastClickedPersistentID.clear();
            m_LastClickedPtr = nullptr;
        }
        m_LastSyncedLastClicked = m_LastClickedPtr;
    }

    void SetLookedAtEntity(Entity* entity) noexcept {
        if (entity) {
            if (Scene* owner = entity->GetScene()) {
                BindScene(owner);
            }
            m_LookedAtHandle = entity->GetHandle();
            m_LookedAtPersistentID = entity->GetPersistentID();
            m_LookedAtPtr = entity;
        } else {
            m_LookedAtHandle = {};
            m_LookedAtPersistentID.clear();
            m_LookedAtPtr = nullptr;
        }
        m_LastSyncedLookedAt = m_LookedAtPtr;
    }

    void Clear() noexcept {
        ClearHandlesOnly();
        SyncLastMirrors();
    }

    bool Sanitize(Scene* explicitScene = nullptr) {
        Scene* activeScene = explicitScene;
        if (activeScene) {
            BindScene(activeScene);
        } else if (m_HasBoundScene) {
            auto locked = m_SceneToken.lock();
            if (!locked || *locked == nullptr) {
                Clear();
                m_Scene = nullptr;
                m_SceneToken.reset();
                m_HasBoundScene = false;
                return false;
            }
            activeScene = *locked;
        }

        if (!activeScene) {
            Clear();
            return false;
        }

        std::vector<EntityHandle> validHandles;
        std::vector<std::string> validPersistentIDs;
        std::vector<Entity*> validPtrs;
        validHandles.reserve(m_SelectedHandles.size());
        validPersistentIDs.reserve(m_SelectedHandles.size());
        validPtrs.reserve(m_SelectedHandles.size());

        for (const EntityHandle handle : m_SelectedHandles) {
            Entity* live = activeScene->GetEntity(handle);
            if (!live) continue;
            if (std::find(validHandles.begin(), validHandles.end(), live->GetHandle()) !=
                validHandles.end()) {
                continue;
            }
            validHandles.push_back(live->GetHandle());
            validPersistentIDs.push_back(live->GetPersistentID());
            validPtrs.push_back(live);
        }

        Entity* primaryLive = activeScene->GetEntity(m_PrimaryHandle);
        if (!primaryLive && !validPtrs.empty()) {
            primaryLive = validPtrs.back();
        }
        if (primaryLive) {
            m_PrimaryHandle = primaryLive->GetHandle();
            m_PrimaryPersistentID = primaryLive->GetPersistentID();
            m_PrimaryPtr = primaryLive;
            if (std::find(validHandles.begin(), validHandles.end(), m_PrimaryHandle) ==
                validHandles.end()) {
                validHandles.push_back(m_PrimaryHandle);
                validPersistentIDs.push_back(m_PrimaryPersistentID);
                validPtrs.push_back(primaryLive);
            }
        } else {
            m_PrimaryHandle = {};
            m_PrimaryPersistentID.clear();
            m_PrimaryPtr = nullptr;
        }

        m_SelectedHandles = std::move(validHandles);
        m_SelectedPersistentIDs = std::move(validPersistentIDs);
        m_SelectedPtrs = std::move(validPtrs);

        if (Entity* lastClickedLive = activeScene->GetEntity(m_LastClickedHandle)) {
            m_LastClickedHandle = lastClickedLive->GetHandle();
            m_LastClickedPersistentID = lastClickedLive->GetPersistentID();
            m_LastClickedPtr = lastClickedLive;
        } else {
            m_LastClickedHandle = {};
            m_LastClickedPersistentID.clear();
            m_LastClickedPtr = nullptr;
        }

        if (Entity* lookedAtLive = activeScene->GetEntity(m_LookedAtHandle)) {
            m_LookedAtHandle = lookedAtLive->GetHandle();
            m_LookedAtPersistentID = lookedAtLive->GetPersistentID();
            m_LookedAtPtr = lookedAtLive;
        } else {
            m_LookedAtHandle = {};
            m_LookedAtPersistentID.clear();
            m_LookedAtPtr = nullptr;
        }

        SyncLastMirrors();
        return m_PrimaryPtr != nullptr;
    }

    // Reconciles any direct mutations made to public compatibility pointer
    // fields on Editor, validating pointer ownership in `scene` without
    // dereferencing unowned addresses, then sanitizing via EntityHandles.
    bool ReconcileMirrors(Scene* explicitScene,
                          Entity*& ioPrimary,
                          std::vector<Entity*>& ioSelected,
                          Entity*& ioLastClicked,
                          Entity*& ioLookedAt) {
        Scene* activeScene = explicitScene;
        if (!activeScene && m_HasBoundScene) {
            if (auto locked = m_SceneToken.lock()) {
                activeScene = *locked;
            }
        }

        const bool primaryMutated = (ioPrimary != m_LastSyncedPrimary);
        const bool selectedMutated = (ioSelected != m_LastSyncedSelected);
        const bool lastClickedMutated = (ioLastClicked != m_LastSyncedLastClicked);
        const bool lookedAtMutated = (ioLookedAt != m_LastSyncedLookedAt);

        if (activeScene && (primaryMutated || selectedMutated)) {
            if (selectedMutated) {
                std::vector<Entity*> ownedSelected;
                ownedSelected.reserve(ioSelected.size());
                for (Entity* candidate : ioSelected) {
                    if (activeScene->OwnsEntityPointer(candidate)) {
                        ownedSelected.push_back(candidate);
                    }
                }
                Entity* ownedPrimary = activeScene->OwnsEntityPointer(ioPrimary)
                                           ? ioPrimary
                                           : (ownedSelected.empty() ? nullptr : ownedSelected.back());
                SetSelectionList(ownedSelected, ownedPrimary);
            } else if (primaryMutated) {
                Entity* ownedPrimary = activeScene->OwnsEntityPointer(ioPrimary) ? ioPrimary : nullptr;
                SetSelectedEntity(ownedPrimary);
            }
        } else if (!activeScene && (primaryMutated || selectedMutated)) {
            Clear();
        }

        if (lastClickedMutated) {
            Entity* ownedLastClicked =
                (activeScene && activeScene->OwnsEntityPointer(ioLastClicked)) ? ioLastClicked : nullptr;
            SetLastClickedEntity(ownedLastClicked);
        }
        if (lookedAtMutated) {
            Entity* ownedLookedAt =
                (activeScene && activeScene->OwnsEntityPointer(ioLookedAt)) ? ioLookedAt : nullptr;
            SetLookedAtEntity(ownedLookedAt);
        }

        const bool hasSelection = Sanitize(activeScene);
        ioPrimary = m_PrimaryPtr;
        ioSelected = m_SelectedPtrs;
        ioLastClicked = m_LastClickedPtr;
        ioLookedAt = m_LookedAtPtr;
        SyncLastMirrors();
        return hasSelection;
    }

    SelectionPersistentSnapshot CapturePersistentSnapshot(Scene* scene = nullptr) {
        Sanitize(scene);
        SelectionPersistentSnapshot snapshot;
        snapshot.primaryPersistentID = m_PrimaryPersistentID;
        snapshot.selectedPersistentIDs = m_SelectedPersistentIDs;
        snapshot.lastClickedPersistentID = m_LastClickedPersistentID;
        snapshot.lookedAtPersistentID = m_LookedAtPersistentID;
        return snapshot;
    }

    void RestorePersistentSnapshot(Scene* scene, const SelectionPersistentSnapshot& snapshot) {
        Clear();
        if (!scene) return;
        BindScene(scene);

        std::vector<Entity*> restoredList;
        restoredList.reserve(snapshot.selectedPersistentIDs.size());
        for (const std::string& pid : snapshot.selectedPersistentIDs) {
            if (pid.empty()) continue;
            if (Entity* entity = scene->GetEntityByPersistentID(pid)) {
                restoredList.push_back(entity);
            }
        }
        Entity* restoredPrimary = snapshot.primaryPersistentID.empty()
                                      ? nullptr
                                      : scene->GetEntityByPersistentID(snapshot.primaryPersistentID);
        SetSelectionList(restoredList, restoredPrimary);

        if (!snapshot.lastClickedPersistentID.empty()) {
            SetLastClickedEntity(scene->GetEntityByPersistentID(snapshot.lastClickedPersistentID));
        }
        if (!snapshot.lookedAtPersistentID.empty()) {
            SetLookedAtEntity(scene->GetEntityByPersistentID(snapshot.lookedAtPersistentID));
        }
        Sanitize(scene);
    }

    Entity* GetSelectedEntity() noexcept {
        if (m_HasBoundScene) {
            Sanitize(nullptr);
        }
        return m_PrimaryPtr;
    }

    const std::vector<Entity*>& GetSelectedEntities() noexcept {
        if (m_HasBoundScene) {
            Sanitize(nullptr);
        }
        return m_SelectedPtrs;
    }

    Entity* GetLastClickedEntity() noexcept {
        if (m_HasBoundScene) {
            Sanitize(nullptr);
        }
        return m_LastClickedPtr;
    }

    Entity* GetLookedAtEntity() noexcept {
        if (m_HasBoundScene) {
            Sanitize(nullptr);
        }
        return m_LookedAtPtr;
    }

    EntityHandle GetSelectedEntityHandle() const noexcept { return m_PrimaryHandle; }
    const std::vector<EntityHandle>& GetSelectedEntityHandles() const noexcept { return m_SelectedHandles; }
    const std::vector<EntityHandle>& GetSelectedHandles() const noexcept { return m_SelectedHandles; }
    Scene* GetBoundScene() const noexcept {
        if (!m_HasBoundScene) return nullptr;
        auto locked = m_SceneToken.lock();
        return (locked && *locked) ? *locked : nullptr;
    }

private:
    void ClearHandlesOnly() noexcept {
        m_PrimaryPtr = nullptr;
        m_SelectedPtrs.clear();
        m_LastClickedPtr = nullptr;
        m_LookedAtPtr = nullptr;
        m_PrimaryHandle = {};
        m_PrimaryPersistentID.clear();
        m_SelectedHandles.clear();
        m_SelectedPersistentIDs.clear();
        m_LastClickedHandle = {};
        m_LastClickedPersistentID.clear();
        m_LookedAtHandle = {};
        m_LookedAtPersistentID.clear();
    }

    void SyncLastMirrors() noexcept {
        m_LastSyncedPrimary = m_PrimaryPtr;
        m_LastSyncedSelected = m_SelectedPtrs;
        m_LastSyncedLastClicked = m_LastClickedPtr;
        m_LastSyncedLookedAt = m_LookedAtPtr;
    }

    Scene* m_Scene = nullptr;
    std::weak_ptr<Scene*> m_SceneToken;
    bool m_HasBoundScene = false;

    Entity* m_PrimaryPtr = nullptr;
    std::vector<Entity*> m_SelectedPtrs;
    Entity* m_LastClickedPtr = nullptr;
    Entity* m_LookedAtPtr = nullptr;

    Entity* m_LastSyncedPrimary = nullptr;
    std::vector<Entity*> m_LastSyncedSelected;
    Entity* m_LastSyncedLastClicked = nullptr;
    Entity* m_LastSyncedLookedAt = nullptr;

    EntityHandle m_PrimaryHandle{};
    std::string m_PrimaryPersistentID;
    std::vector<EntityHandle> m_SelectedHandles;
    std::vector<std::string> m_SelectedPersistentIDs;
    EntityHandle m_LastClickedHandle{};
    std::string m_LastClickedPersistentID;
    EntityHandle m_LookedAtHandle{};
    std::string m_LookedAtPersistentID;
};

} // namespace Archura
