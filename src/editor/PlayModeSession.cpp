#include "PlayModeSession.h"
#include "../game/FPSController.h"
#include "../game/ParticleSystem.h"
#include "../game/PhysicsSystem.h"
#include "../game/ProjectileSystem.h"
#include <unordered_map>
#include <iostream>

namespace Archura {

namespace {

Entity* ResolvePlayerInScene(Scene* scene, const std::string* persistentID, const uint64_t* handleValue) {
    if (!scene) return nullptr;
    if (persistentID && !persistentID->empty()) {
        if (Entity* byPid = scene->GetEntityByPersistentID(*persistentID)) {
            return byPid;
        }
    }
    if (handleValue && *handleValue != 0) {
        if (Entity* byHandle = scene->GetEntity(EntityHandle::FromValue(*handleValue))) {
            return byHandle;
        }
    }
    for (const auto& entityPtr : scene->GetEntities()) {
        if (entityPtr && entityPtr->GetName() == "Player") {
            return entityPtr.get();
        }
    }
    return nullptr;
}

} // namespace

bool PlayModeSession::ShouldTickSimulation(bool isPaused) const noexcept {
    return m_Mode == PlaySessionMode::Play && !isPaused;
}

bool PlayModeSession::EnterPlayMode(const PlayModeContext& ctx) {
    if (m_Mode == PlaySessionMode::Play || !ctx.scene || ctx.scene->IsIterating()) {
        return false;
    }

    // Snapshot coverage must be complete. Unknown authored components or raw
    // resources without retained owners cannot be restored safely after Play.
    for (const auto& entity : ctx.scene->GetEntities()) {
        const size_t supported = static_cast<size_t>(entity->HasComponent<Transform>()) +
            entity->HasComponent<MeshRenderer>() + entity->HasComponent<BoxCollider>() +
            entity->HasComponent<LightComponent>() + entity->HasComponent<RigidBody>() +
            entity->HasComponent<Health>() + entity->HasComponent<Weapon>() +
            entity->HasComponent<ScriptComponent>() + entity->HasComponent<SpawnPoint>() +
            entity->HasComponent<SkyboxComponent>() + entity->HasComponent<AudioSource>() +
            entity->HasComponent<AudioSourceComponent>() + entity->HasComponent<SurfaceProperty>() +
            entity->HasComponent<Lifetime>() + entity->HasComponent<Particle>() + entity->HasComponent<Projectile>();
        const auto* mesh = entity->GetComponent<MeshRenderer>();
        if (supported != entity->GetComponentCount() ||
            (mesh && ((mesh->GetMesh() && !mesh->meshAsset) ||
                      (mesh->GetShader() && !mesh->shaderAsset) ||
                      (mesh->GetTexture() && !mesh->textureAsset)))) {
            std::cerr << "[Play] Unsupported snapshot state on entity: " << entity->GetName() << '\n';
            return false;
        }
    }
    m_SnapshotScene = ctx.scene->GetLifetimeToken();

    if (ctx.selection) {
        m_SelectionSnapshot = ctx.selection->CapturePersistentSnapshot(ctx.scene);
    } else {
        m_SelectionSnapshot = {};
    }

    m_PlayerPersistentID.clear();
    if (Entity* player = ResolvePlayerInScene(ctx.scene, ctx.playerPersistentID, ctx.playerHandleValue)) {
        m_PlayerPersistentID = player->GetPersistentID();
        if (ctx.playerPersistentID) {
            *ctx.playerPersistentID = m_PlayerPersistentID;
        }
        if (ctx.playerHandleValue) {
            *ctx.playerHandleValue = player->GetHandle().Value();
        }
    }

    if (ctx.camera) {
        m_CameraSnapshot.valid = true;
        m_CameraSnapshot.position = ctx.camera->GetPosition();
        m_CameraSnapshot.yaw = ctx.camera->GetYaw();
        m_CameraSnapshot.pitch = ctx.camera->GetPitch();
        m_CameraSnapshot.roll = ctx.camera->GetRoll();
        m_CameraSnapshot.fov = ctx.camera->GetFOV();
        m_CameraSnapshot.movementSpeed = ctx.camera->GetMovementSpeed();
        m_CameraSnapshot.mouseSensitivity = ctx.camera->GetMouseSensitivity();
    } else {
        m_CameraSnapshot = {};
    }

    if (ctx.fpsController) {
        m_ControllerSnapshot.valid = true;
        m_ControllerSnapshot.walkSpeed = ctx.fpsController->GetWalkSpeed();
        m_ControllerSnapshot.runSpeed = ctx.fpsController->GetRunSpeed();
        m_ControllerSnapshot.jumpHeight = ctx.fpsController->GetJumpHeight();
        m_ControllerSnapshot.mouseSensitivity = ctx.fpsController->GetMouseSensitivity();
        m_ControllerSnapshot.gravityEnabled = ctx.fpsController->IsGravityEnabled();
        m_ControllerSnapshot.noclipEnabled = ctx.fpsController->IsNoclipEnabled();
    } else {
        m_ControllerSnapshot = {};
    }

    const auto& entities = ctx.scene->GetEntities();
    m_EntitySnapshots.clear();
    m_EntitySnapshots.reserve(entities.size());

    for (const auto& entityPtr : entities) {
        if (!entityPtr) continue;
        EntitySnapshot snap;
        snap.name = entityPtr->GetName();
        snap.persistentID = entityPtr->GetPersistentID();
        snap.parentPersistentID =
            entityPtr->GetParent() ? entityPtr->GetParent()->GetPersistentID() : std::string{};
        snap.ownership = entityPtr->GetOwnership();

        if (const auto* tf = entityPtr->GetComponent<Transform>()) {
            snap.transform = *tf;
        }
        if (const auto* mr = entityPtr->GetComponent<MeshRenderer>()) {
            MeshRendererSnapshot mrSnap;
            mrSnap.meshAsset = mr->meshAsset;
            mrSnap.shaderAsset = mr->shaderAsset;
            mrSnap.textureAsset = mr->textureAsset;
            mrSnap.rawMesh = mr->mesh;
            mrSnap.rawShader = mr->shader;
            mrSnap.rawTexture = mr->texture;
            mrSnap.color = mr->color;
            if (mr->meshAsset) {
                mrSnap.hasGeometrySnapshot = true;
                mrSnap.meshSource = mr->meshAsset->GetSource();
                mrSnap.vertices = static_cast<const Mesh&>(*mr->meshAsset).GetVertices();
                mrSnap.indices = mr->meshAsset->GetIndices();
            }
            snap.meshRenderer = std::move(mrSnap);
        }
        if (const auto* bc = entityPtr->GetComponent<BoxCollider>()) {
            snap.boxCollider = *bc;
        }
        if (const auto* lc = entityPtr->GetComponent<LightComponent>()) {
            snap.light = *lc;
        }
        if (const auto* rb = entityPtr->GetComponent<RigidBody>()) {
            snap.rigidBody = *rb;
        }
        if (const auto* hp = entityPtr->GetComponent<Health>()) {
            snap.health = *hp;
        }
        if (const auto* wp = entityPtr->GetComponent<Weapon>()) {
            snap.weapon = *wp;
        }
        if (const auto* sc = entityPtr->GetComponent<ScriptComponent>()) {
            ScriptComponent copy = *sc;
            copy.scriptInstanceID = -1;
            snap.script = std::move(copy);
        }
        if (const auto* sp = entityPtr->GetComponent<SpawnPoint>()) {
            snap.spawnPoint = *sp;
        }
        if (const auto* sky = entityPtr->GetComponent<SkyboxComponent>()) {
            snap.skybox = *sky;
        }
        if (const auto* as = entityPtr->GetComponent<AudioSource>()) {
            AudioSource copy = *as;
            copy.isPlaying = false;
            copy.runtimeAlias.clear();
            snap.audioSource = std::move(copy);
        }
        if (const auto* asc = entityPtr->GetComponent<AudioSourceComponent>()) {
            AudioSourceComponent copy = *asc;
            copy.playing = false;
            snap.audioSourceComponent = std::move(copy);
        }
        if (const auto* surf = entityPtr->GetComponent<SurfaceProperty>()) {
            snap.surfaceProperty = *surf;
        }
        if (const auto* life = entityPtr->GetComponent<Lifetime>()) {
            snap.lifetime = *life;
        }
        if (const auto* part = entityPtr->GetComponent<Particle>()) {
            snap.particle = *part;
        }
        if (const auto* proj = entityPtr->GetComponent<Projectile>()) {
            snap.projectile = *proj;
        }

        m_EntitySnapshots.push_back(std::move(snap));
    }

    if (ctx.physicsSystem) {
        ctx.physicsSystem->ResetRuntimeState();
    }
    if (ctx.projectileSystem) {
        ctx.projectileSystem->ResetRuntimeState();
    }
    if (ctx.fpsController) {
        ctx.fpsController->ResetRuntimeState();
    }

    m_HasSnapshot = true;
    m_Mode = PlaySessionMode::Play;

    if (ctx.onRuntimeStart) {
        ctx.onRuntimeStart(ctx.scene);
    }

    return true;
}

bool PlayModeSession::ExitPlayMode(const PlayModeContext& ctx) {
    if (!ctx.scene || m_Mode != PlaySessionMode::Play || !m_HasSnapshot) {
        return false;
    }
    const auto source = m_SnapshotScene.lock();
    if (!source || *source != ctx.scene) return false;
    if (ctx.scene->IsIterating()) return false;

    if (ctx.onRuntimeStop) {
        ctx.onRuntimeStop(ctx.scene);
    }
    if (ctx.physicsSystem) {
        ctx.physicsSystem->ResetRuntimeState();
    }
    if (ctx.projectileSystem) {
        ctx.projectileSystem->ResetRuntimeState();
    }
    if (ctx.fpsController) {
        ctx.fpsController->ResetRuntimeState();
    }

    std::vector<EntityHandle> replacedHandles;
    replacedHandles.reserve(ctx.scene->GetEntities().size());
    for (const auto& entityPtr : ctx.scene->GetEntities()) {
        if (entityPtr) {
            replacedHandles.push_back(entityPtr->GetHandle());
        }
    }

    std::vector<EntityHandle> stagedHandles;
    std::vector<std::string> persistentIDs;
    stagedHandles.reserve(m_EntitySnapshots.size());
    persistentIDs.reserve(m_EntitySnapshots.size());

    struct StagedRollbackGuard {
        Scene* scene = nullptr;
        std::vector<EntityHandle>& staged;
        bool committed = false;
        ~StagedRollbackGuard() {
            if (!committed && scene) {
                for (const EntityHandle handle : staged) {
                    scene->DestroyEntity(handle);
                }
            }
        }
    } rollbackGuard{ctx.scene, stagedHandles, false};

    std::unordered_map<std::string, Entity*> stagedByPersistentID;
    stagedByPersistentID.reserve(m_EntitySnapshots.size());

    for (const EntitySnapshot& snap : m_EntitySnapshots) {
        Entity* entity = ctx.scene->CreateEntity(snap.name);
        if (!entity) {
            return false;
        }
        stagedHandles.push_back(entity->GetHandle());
        persistentIDs.push_back(snap.persistentID);
        if (!snap.persistentID.empty()) {
            stagedByPersistentID.emplace(snap.persistentID, entity);
        }

        entity->SetOwnership(snap.ownership);

        if (snap.transform.has_value()) {
            if (auto* tf = entity->GetComponent<Transform>()) {
                *tf = *snap.transform;
            } else {
                *entity->AddComponent<Transform>() = *snap.transform;
            }
        } else {
            entity->RemoveComponent<Transform>();
        }

        if (snap.meshRenderer.has_value()) {
            const MeshRendererSnapshot& mrSnap = *snap.meshRenderer;
            auto* mr = entity->AddComponent<MeshRenderer>();
            if (mrSnap.meshAsset) {
                if (mrSnap.hasGeometrySnapshot) {
                    mrSnap.meshAsset->RestoreGeometrySnapshot(
                        mrSnap.vertices, mrSnap.indices, mrSnap.meshSource);
                }
                mr->SetMeshAsset(mrSnap.meshAsset);
            } else {
                mr->mesh = mrSnap.rawMesh;
            }
            if (mrSnap.shaderAsset) {
                mr->SetShaderAsset(mrSnap.shaderAsset);
            } else {
                mr->shader = mrSnap.rawShader;
            }
            if (mrSnap.textureAsset) {
                mr->SetTextureAsset(mrSnap.textureAsset);
            } else {
                mr->texture = mrSnap.rawTexture;
            }
            mr->color = mrSnap.color;
        }

        if (snap.boxCollider.has_value()) {
            *entity->AddComponent<BoxCollider>() = *snap.boxCollider;
        }
        if (snap.light.has_value()) {
            *entity->AddComponent<LightComponent>() = *snap.light;
        }
        if (snap.rigidBody.has_value()) {
            *entity->AddComponent<RigidBody>() = *snap.rigidBody;
        }
        if (snap.health.has_value()) {
            *entity->AddComponent<Health>() = *snap.health;
        }
        if (snap.weapon.has_value()) {
            *entity->AddComponent<Weapon>() = *snap.weapon;
        }
        if (snap.script.has_value()) {
            auto* sc = entity->AddComponent<ScriptComponent>();
            *sc = *snap.script;
            sc->scriptInstanceID = -1;
        }
        if (snap.spawnPoint.has_value()) {
            *entity->AddComponent<SpawnPoint>() = *snap.spawnPoint;
        }
        if (snap.skybox.has_value()) {
            *entity->AddComponent<SkyboxComponent>() = *snap.skybox;
        }
        if (snap.audioSource.has_value()) {
            auto* as = entity->AddComponent<AudioSource>();
            *as = *snap.audioSource;
            as->isPlaying = false;
            as->runtimeAlias.clear();
        }
        if (snap.audioSourceComponent.has_value()) {
            auto* asc = entity->AddComponent<AudioSourceComponent>();
            *asc = *snap.audioSourceComponent;
            asc->playing = false;
        }
        if (snap.surfaceProperty.has_value()) {
            *entity->AddComponent<SurfaceProperty>() = *snap.surfaceProperty;
        }
        if (snap.lifetime.has_value()) {
            *entity->AddComponent<Lifetime>() = *snap.lifetime;
        }
        if (snap.particle.has_value()) {
            *entity->AddComponent<Particle>() = *snap.particle;
        }
        if (snap.projectile.has_value()) {
            *entity->AddComponent<Projectile>() = *snap.projectile;
        }
    }

    for (size_t i = 0; i < m_EntitySnapshots.size(); ++i) {
        const std::string& parentPID = m_EntitySnapshots[i].parentPersistentID;
        if (parentPID.empty()) continue;
        const auto parentIt = stagedByPersistentID.find(parentPID);
        Entity* child = ctx.scene->GetEntity(stagedHandles[i]);
        if (child && parentIt != stagedByPersistentID.end()) {
            child->TrySetParent(parentIt->second);
        }
    }

    if (!ctx.scene->CommitEntityReplacement(replacedHandles, stagedHandles, persistentIDs)) {
        return false;
    }
    rollbackGuard.committed = true;

    if (m_CameraSnapshot.valid && ctx.camera) {
        ctx.camera->SetPosition(m_CameraSnapshot.position);
        ctx.camera->SetYaw(m_CameraSnapshot.yaw);
        ctx.camera->SetPitch(m_CameraSnapshot.pitch);
        ctx.camera->SetRoll(m_CameraSnapshot.roll);
        ctx.camera->SetFOV(m_CameraSnapshot.fov);
        ctx.camera->SetMovementSpeed(m_CameraSnapshot.movementSpeed);
        ctx.camera->SetMouseSensitivity(m_CameraSnapshot.mouseSensitivity);
    }

    if (m_ControllerSnapshot.valid && ctx.fpsController) {
        ctx.fpsController->SetWalkSpeed(m_ControllerSnapshot.walkSpeed);
        ctx.fpsController->SetRunSpeed(m_ControllerSnapshot.runSpeed);
        ctx.fpsController->SetJumpHeight(m_ControllerSnapshot.jumpHeight);
        ctx.fpsController->SetMouseSensitivity(m_ControllerSnapshot.mouseSensitivity);
        ctx.fpsController->SetGravityEnabled(m_ControllerSnapshot.gravityEnabled);
        ctx.fpsController->SetNoclipEnabled(m_ControllerSnapshot.noclipEnabled);
        ctx.fpsController->ResetRuntimeState();
    }

    Entity* restoredPlayer = ResolvePlayerInScene(ctx.scene, &m_PlayerPersistentID, nullptr);
    if (ctx.playerHandleValue) {
        *ctx.playerHandleValue = restoredPlayer ? restoredPlayer->GetHandle().Value() : 0ULL;
    }
    if (ctx.playerPersistentID) {
        *ctx.playerPersistentID = restoredPlayer ? restoredPlayer->GetPersistentID() : std::string{};
    }

    if (ctx.selection) {
        ctx.selection->RestorePersistentSnapshot(ctx.scene, m_SelectionSnapshot);
    }
    if (ctx.undoHistory) {
        ctx.undoHistory->RebindRestoredScene(*ctx.scene);
    }

    m_Mode = PlaySessionMode::Edit;
    m_HasSnapshot = false;
    m_EntitySnapshots.clear();
    m_SnapshotScene.reset();
    return true;
}

void PlayModeSession::DiscardSnapshot() noexcept {
    m_EntitySnapshots.clear();
    m_SnapshotScene.reset();
    m_PlayerPersistentID.clear();
    m_CameraSnapshot.valid = false;
    m_ControllerSnapshot.valid = false;
    m_HasSnapshot = false;
    m_Mode = PlaySessionMode::Edit;
}

bool PlayModeSession::TickSimulation(const PlayModeContext& ctx, Input* input, bool isPaused, float dt) {
    if (!ctx.scene || !ShouldTickSimulation(isPaused)) {
        return false;
    }
    const auto source = m_SnapshotScene.lock();
    if (!source || *source != ctx.scene) return false;

    EntityHandle playerHandle{};
    if (ctx.playerHandleValue && *ctx.playerHandleValue != 0) {
        playerHandle = EntityHandle::FromValue(*ctx.playerHandleValue);
        if (!ctx.scene->GetEntity(playerHandle)) {
            if (Entity* resolved = ResolvePlayerInScene(ctx.scene, ctx.playerPersistentID, ctx.playerHandleValue)) {
                playerHandle = resolved->GetHandle();
                *ctx.playerHandleValue = playerHandle.Value();
            } else {
                playerHandle = {};
                *ctx.playerHandleValue = 0ULL;
            }
        }
    }

    WeaponSystem weaponSystem;
    weaponSystem.Update(ctx.scene, playerHandle, input, ctx.camera, ctx.projectileSystem, dt);

    if (ctx.fpsController) {
        ctx.fpsController->Update(input, ctx.scene, dt, ctx.projectileSystem, ctx.physicsSystem);
    }
    if (ctx.projectileSystem) {
        ctx.projectileSystem->Update(dt);
    }
    if (ctx.physicsSystem) {
        ctx.physicsSystem->Update(dt);
    }
    if (ctx.particleSystem) {
        ctx.particleSystem->Update(dt);
    }
    return true;
}

} // namespace Archura
