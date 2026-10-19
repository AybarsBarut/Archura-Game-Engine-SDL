#define SDL_MAIN_HANDLED

#include "editor/EditorSelectionState.h"
#include "editor/EditorModeState.h"
#include "editor/PlayModeSession.h"
#include "editor/TransformUndoHistory.h"
#include "ecs/Component.h"
#include "ecs/Entity.h"
#include "game/FPSController.h"
#include "game/ParticleSystem.h"
#include "game/PhysicsSystem.h"
#include "game/ProjectileSystem.h"
#include "game/Weapon.h"
#include "input/Input.h"
#include "rendering/Camera.h"
#include "rendering/Mesh.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

namespace {

using namespace Archura;

int g_Failures = 0;

void Expect(bool condition, const char* expr, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << ": Expectation failed: " << expr << "\n";
        ++g_Failures;
    }
}

#define EXPECT_TRUE(expr) Expect(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define EXPECT_FALSE(expr) Expect(!static_cast<bool>(expr), "!(" #expr ")", __FILE__, __LINE__)

bool Near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

bool NearVec3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
    return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

void TestBallSettingsRestoreAcrossPlayStop() {
    Scene scene("ball authoring");
    auto* ball = scene.CreateEntity("Ball");
    const auto persistentID = ball->GetPersistentID();
    auto* collider = ball->AddComponent<BoxCollider>();
    collider->shape = BoxCollider::Shape::Sphere;
    collider->radius = 0.8f;
    auto* body = ball->AddComponent<RigidBody>();
    body->angularVelocity = {1, -2, 3};
    body->angularDrag = 0.35f;
    body->mass = 8;
    body->canBePushed = false;
    body->useGravity = false;
    PlayModeSession session;
    PlayModeContext context{};
    context.scene = &scene;
    for (int cycle = 0; cycle < 3; ++cycle) {
        EXPECT_TRUE(session.EnterPlayMode(context));
        ball = scene.GetEntityByPersistentID(persistentID);
        const auto previousHandle = ball->GetHandle();
        collider = ball->GetComponent<BoxCollider>();
        body = ball->GetComponent<RigidBody>();
        collider->shape = BoxCollider::Shape::Box;
        collider->radius = 5;
        body->angularVelocity = {9, 8, 7};
        body->angularDrag = 4;
        body->canBePushed = true;
        body->useGravity = true;
        body->mass = 20;
        EXPECT_TRUE(session.ExitPlayMode(context));
        EXPECT_FALSE(scene.IsAlive(previousHandle));
        ball = scene.GetEntityByPersistentID(persistentID);
        EXPECT_TRUE(ball != nullptr);
        if (!ball) return;
        collider = ball->GetComponent<BoxCollider>();
        body = ball->GetComponent<RigidBody>();
        EXPECT_TRUE(collider && collider->shape == BoxCollider::Shape::Sphere && Near(collider->radius, 0.8f));
        EXPECT_TRUE(body && NearVec3(body->angularVelocity, {1, -2, 3}) && Near(body->angularDrag, 0.35f) &&
                    Near(body->mass, 8) && !body->canBePushed && !body->useGravity);
    }
}

void TestRejectedSnapshotsAndWrongSceneStop() {
    struct Unhandled final : Component { int authored = 71; };
    Scene scene("guarded authoring");
    auto* authored = scene.CreateEntity("custom authored state");
    const auto originalHandle = authored->GetHandle();
    authored->AddComponent<Unhandled>();
    PlayModeSession session;
    PlayModeContext ctx{};
    ctx.scene = &scene;
    int started = 0, stopped = 0;
    ctx.onRuntimeStart = [&](Scene*) { ++started; };
    ctx.onRuntimeStop = [&](Scene*) { ++stopped; };
    EditorModeState modes;
    const bool entered = modes.Request(EditorMode::Play, [&] { return session.EnterPlayMode(ctx); });
    EXPECT_FALSE(entered);
    EXPECT_TRUE(started == 0 && !session.HasSnapshot() && !session.IsPlaying());
    EXPECT_TRUE(modes.GetMode() == EditorMode::Edit);
    if (entered) session.ExitPlayMode(ctx);
    EXPECT_TRUE(scene.IsAlive(originalHandle));
    if (scene.IsAlive(originalHandle)) scene.GetEntity(originalHandle)->RemoveComponent<Unhandled>();

    // Independent scenario: a snapshot must never replace another live scene.
    PlayModeSession boundSession;
    EXPECT_TRUE(modes.Request(EditorMode::Play, [&] { return boundSession.EnterPlayMode(ctx); }));
    Scene unrelated("unrelated authored project");
    const auto unrelatedHandle = unrelated.CreateEntity("must survive")->GetHandle();
    auto wrong = ctx;
    wrong.scene = &unrelated;
    const int stopsBefore = stopped;
    EXPECT_FALSE(modes.Request(EditorMode::Edit, [&] { return boundSession.ExitPlayMode(wrong); }));
    EXPECT_TRUE(unrelated.IsAlive(unrelatedHandle) && stopped == stopsBefore);
    EXPECT_TRUE(boundSession.IsPlaying() && boundSession.HasSnapshot());
    EXPECT_TRUE(modes.GetMode() == EditorMode::Play);
    EXPECT_FALSE(boundSession.TickSimulation(wrong, nullptr, false, 1.0f / 128.0f));
    EXPECT_TRUE(modes.Request(EditorMode::Edit, [&] { return boundSession.ExitPlayMode(ctx); }));
    EXPECT_TRUE(modes.GetMode() == EditorMode::Edit);
}

void TestSnapshotLifetimeAndShutdownRelease() {
    PlayModeSession session;
    PlayModeContext stale{};
    std::weak_ptr<Mesh> capturedAsset;
    {
        Scene source("temporary source");
        auto* entity = source.CreateEntity("asset owner");
        auto asset = Mesh::CreateCubeShared();
        capturedAsset = asset;
        auto* mesh = entity->AddComponent<MeshRenderer>();
        mesh->mesh = asset.get();
        PlayModeContext ctx{}; ctx.scene = &source;
        EXPECT_FALSE(session.EnterPlayMode(ctx)); // Raw resources have no lifetime contract.
        mesh->SetMeshAsset(asset);
        EXPECT_TRUE(session.EnterPlayMode(ctx));
        stale = ctx;
    }
    EXPECT_FALSE(capturedAsset.expired()); // Snapshot is now the last GPU owner.
    EXPECT_FALSE(session.ExitPlayMode(stale)); // Must check lifetime before touching stale.scene.
    Scene unrelated;
    auto* survivor = unrelated.CreateEntity("authored survivor");
    const auto survivorHandle = survivor->GetHandle();
    PlayModeContext wrong{}; wrong.scene = &unrelated;
    EXPECT_FALSE(session.ExitPlayMode(wrong));
    EXPECT_TRUE(unrelated.IsAlive(survivorHandle));
    session.DiscardSnapshot();
    EXPECT_TRUE(capturedAsset.expired()); // Application discards before context shutdown.
    EXPECT_FALSE(session.IsPlaying() || session.HasSnapshot());
    EXPECT_TRUE(session.EnterPlayMode(wrong));
    EXPECT_TRUE(session.ExitPlayMode(wrong));
}

void TestEditorModeFailureAndReentrancy() {
    EditorModeState modes;
    EXPECT_FALSE(modes.Request(EditorMode::Play, {}));
    EXPECT_TRUE(modes.GetMode() == EditorMode::Edit);
    EXPECT_FALSE(modes.Request(EditorMode::Play, [&] {
        EXPECT_TRUE(modes.GetMode() == EditorMode::Edit);
        return modes.Request(EditorMode::Edit, [] { return true; });
    }));
    EXPECT_TRUE(modes.GetMode() == EditorMode::Edit);
    bool threw = false;
    try {
        modes.Request(EditorMode::Play, []() -> bool { throw std::runtime_error("runtime transition failed"); });
    } catch (const std::runtime_error&) { threw = true; }
    EXPECT_TRUE(threw && modes.GetMode() == EditorMode::Edit);
    EXPECT_TRUE(modes.Request(EditorMode::Play, [] { return true; }));
    EXPECT_FALSE(modes.Request(EditorMode::Edit, [] { return false; }));
    EXPECT_TRUE(modes.GetMode() == EditorMode::Play);
    EXPECT_TRUE(modes.Request(EditorMode::Edit, [] { return true; }));
}

void TestSelectionClearedWhenSelectedEntityDestroyed() {
    Scene scene("SelectionDestroyScene");
    EditorSelectionState selection;
    selection.BindScene(&scene);

    Entity* crate = scene.CreateEntity("Crate");
    Entity* beacon = scene.CreateEntity("Beacon");
    selection.SetSelectedEntity(crate);
    selection.SetLastClickedEntity(crate);
    selection.SetLookedAtEntity(beacon);

    EXPECT_TRUE(selection.GetSelectedEntity() == crate);
    EXPECT_TRUE(selection.GetSelectedEntities().size() == 1U);
    EXPECT_TRUE(selection.GetLookedAtEntity() == beacon);

    EXPECT_TRUE(scene.DestroyEntity(crate->GetHandle()));
    EXPECT_FALSE(selection.Sanitize(&scene));
    EXPECT_TRUE(selection.GetSelectedEntity() == nullptr);
    EXPECT_TRUE(selection.GetSelectedEntities().empty());
    EXPECT_TRUE(selection.GetLastClickedEntity() == nullptr);
    EXPECT_FALSE(static_cast<bool>(selection.GetSelectedEntityHandle()));
    EXPECT_TRUE(selection.GetLookedAtEntity() == beacon);

    EXPECT_TRUE(scene.DestroyEntity(beacon->GetHandle()));
    EXPECT_FALSE(selection.Sanitize(&scene));
    EXPECT_TRUE(selection.GetLookedAtEntity() == nullptr);
}

void TestSelectionRejectsReplacementEntityAfterDestroy() {
    Scene scene("SelectionReplacementScene");
    EditorSelectionState selection;
    selection.BindScene(&scene);

    Entity* original = scene.CreateEntity("Original");
    const EntityHandle originalHandle = original->GetHandle();
    selection.SetSelectedEntity(original);

    EXPECT_TRUE(scene.DestroyEntity(originalHandle));
    Entity* replacement = scene.CreateEntity("Replacement");
    EXPECT_TRUE(replacement != nullptr);
    EXPECT_TRUE(replacement->GetHandle() != originalHandle);

    EXPECT_FALSE(selection.Sanitize(&scene));
    EXPECT_TRUE(selection.GetSelectedEntity() == nullptr);
    EXPECT_TRUE(selection.GetSelectedEntities().empty());
}

void TestMultiSelectionPrunesDestroyedAndPromotesRemainingPrimary() {
    Scene scene("MultiSelectScene");
    EditorSelectionState selection;
    selection.BindScene(&scene);

    Entity* a = scene.CreateEntity("EntityA");
    Entity* b = scene.CreateEntity("EntityB");
    Entity* c = scene.CreateEntity("EntityC");

    selection.SetSelectionList({a, b, c}, c);
    selection.SetLastClickedEntity(c);
    selection.SetLookedAtEntity(b);

    EXPECT_TRUE(selection.GetSelectedEntity() == c);
    EXPECT_TRUE(selection.GetSelectedEntities().size() == 3U);

    // Destroy middle entity B; primary C remains, LookedAt(B) clears.
    EXPECT_TRUE(scene.DestroyEntity(b->GetHandle()));
    EXPECT_TRUE(selection.Sanitize(&scene));
    EXPECT_TRUE(selection.GetSelectedEntity() == c);
    EXPECT_TRUE(selection.GetSelectedEntities().size() == 2U);
    EXPECT_TRUE(selection.GetLookedAtEntity() == nullptr);
    EXPECT_TRUE(selection.GetLastClickedEntity() == c);

    // Destroy primary entity C; remaining entity A is promoted to primary!
    Entity* danglingC = c;
    EXPECT_TRUE(scene.DestroyEntity(c->GetHandle()));
    EXPECT_TRUE(selection.Sanitize(&scene));
    EXPECT_TRUE(selection.GetSelectedEntity() == a);
    EXPECT_TRUE(selection.GetSelectedEntities().size() == 1U);
    EXPECT_TRUE(selection.GetLastClickedEntity() == nullptr);

    // Verify ReconcileMirrors rejects an unowned/dangling raw pointer write.
    Entity* primaryMirror = danglingC;
    std::vector<Entity*> selectedMirror = {danglingC};
    Entity* lastClickedMirror = danglingC;
    Entity* lookedAtMirror = danglingC;
    EXPECT_FALSE(selection.ReconcileMirrors(
        &scene, primaryMirror, selectedMirror, lastClickedMirror, lookedAtMirror));
    EXPECT_TRUE(primaryMirror == nullptr);
    EXPECT_TRUE(selectedMirror.empty());
    EXPECT_TRUE(lastClickedMirror == nullptr);
    EXPECT_TRUE(lookedAtMirror == nullptr);
}

void TestSelectionClearedWhenOwningSceneDestroyed() {
    EditorSelectionState selection;
    {
        Scene transientScene("TransientScene");
        Entity* temp = transientScene.CreateEntity("Temp");
        selection.BindScene(&transientScene);
        selection.SetSelectedEntity(temp);
        EXPECT_TRUE(selection.GetSelectedEntity() == temp);
    }
    EXPECT_TRUE(selection.GetSelectedEntity() == nullptr);
    EXPECT_TRUE(selection.GetSelectedEntities().empty());
}

void TestEditModeBlocksSimulationSystems() {
    Scene scene("EditModeGateScene");
    Camera camera(glm::vec3(0.0f, 5.0f, 10.0f));
    FPSController controller(&camera);
    PhysicsSystem physics;
    physics.Init(&scene);
    ProjectileSystem projectiles;
    projectiles.Init(&scene, &physics);
    ParticleSystem particles;
    particles.Init(&scene);

    Entity* floor = scene.CreateEntity("Floor");
    floor->SetOwnership(EntityOwnership::Engine);
    floor->GetComponent<Transform>()->position = glm::vec3(0.0f, -1.0f, 0.0f);
    floor->GetComponent<Transform>()->scale = glm::vec3(50.0f, 1.0f, 50.0f);
    floor->AddComponent<BoxCollider>()->size = glm::vec3(1.0f, 1.0f, 1.0f);

    Entity* player = scene.CreateEntity("Player");
    player->SetOwnership(EntityOwnership::Engine);
    player->GetComponent<Transform>()->position = glm::vec3(0.0f, 2.0f, 0.0f);
    auto* weapon = player->AddComponent<Weapon>();
    weapon->InitInventory();
    weapon->isReloading = true;
    weapon->reloadTimer = 1.0f;

    Entity* crate = scene.CreateEntity("DynamicCrate");
    crate->GetComponent<Transform>()->position = glm::vec3(0.0f, 8.0f, 0.0f);
    crate->AddComponent<BoxCollider>()->size = glm::vec3(1.0f);
    auto* rb = crate->AddComponent<RigidBody>();
    rb->useGravity = true;
    rb->velocity = glm::vec3(2.0f, -3.0f, 0.0f);

    uint64_t playerHandleValue = player->GetHandle().Value();
    std::string playerPersistentID = player->GetPersistentID();

    PlayModeSession session;
    PlayModeContext ctx{};
    ctx.scene = &scene;
    ctx.camera = &camera;
    ctx.fpsController = &controller;
    ctx.physicsSystem = &physics;
    ctx.projectileSystem = &projectiles;
    ctx.particleSystem = &particles;
    ctx.playerHandleValue = &playerHandleValue;
    ctx.playerPersistentID = &playerPersistentID;

    EXPECT_TRUE(session.GetMode() == PlaySessionMode::Edit);
    EXPECT_FALSE(session.ShouldTickSimulation(false));

    for (int i = 0; i < 60; ++i) {
        EXPECT_FALSE(session.TickSimulation(ctx, nullptr, false, 1.0f / 128.0f));
    }

    EXPECT_TRUE(NearVec3(crate->GetComponent<Transform>()->position, glm::vec3(0.0f, 8.0f, 0.0f)));
    EXPECT_TRUE(NearVec3(rb->velocity, glm::vec3(2.0f, -3.0f, 0.0f)));
    EXPECT_TRUE(weapon->isReloading);
    EXPECT_TRUE(Near(weapon->reloadTimer, 1.0f));
}

void TestPlayStopRestoresAuthoredSceneAndCleansRuntimeState() {
    Scene scene("PlayStopRestoreScene");
    Camera camera(glm::vec3(1.0f, 6.0f, 12.0f), glm::vec3(0.0f, 1.0f, 0.0f), -85.0f, -12.0f);
    camera.SetFOV(78.0f);
    FPSController controller(&camera);
    PhysicsSystem physics;
    physics.Init(&scene);
    ProjectileSystem projectiles;
    projectiles.Init(&scene, &physics);
    ParticleSystem particles;
    particles.Init(&scene);
    EditorSelectionState selection;
    selection.BindScene(&scene);
    TransformUndoHistory undoHistory;

    Entity* floor = scene.CreateEntity("Floor");
    floor->SetOwnership(EntityOwnership::Engine);
    floor->GetComponent<Transform>()->position = glm::vec3(0.0f, -1.0f, 0.0f);
    floor->GetComponent<Transform>()->scale = glm::vec3(100.0f, 1.0f, 100.0f);
    floor->AddComponent<BoxCollider>()->size = glm::vec3(1.0f);

    Entity* player = scene.CreateEntity("Player");
    player->SetOwnership(EntityOwnership::Engine);
    player->GetComponent<Transform>()->position = glm::vec3(0.0f, 2.0f, 0.0f);
    auto* playerHealth = player->AddComponent<Health>();
    playerHealth->max = 100.0f;
    playerHealth->current = 100.0f;
    auto* playerWeapon = player->AddComponent<Weapon>();
    playerWeapon->InitInventory();

    Entity* tower = scene.CreateEntity("Tower");
    tower->GetComponent<Transform>()->position = glm::vec3(10.0f, 0.0f, -5.0f);
    // Record an Edit-mode undo snapshot on Tower before moving it to its final authored pos.
    EXPECT_TRUE(undoHistory.Capture(*tower));
    tower->GetComponent<Transform>()->position = glm::vec3(12.0f, 0.0f, -5.0f);

    Entity* turret = scene.CreateEntity("Turret");
    turret->SetParent(tower);
    turret->GetComponent<Transform>()->position = glm::vec3(0.0f, 3.0f, 0.0f);
    auto* turretHealth = turret->AddComponent<Health>();
    turretHealth->max = 80.0f;
    turretHealth->current = 80.0f;
    auto* turretScript = turret->AddComponent<ScriptComponent>();
    turretScript->className = "TurretController";
    turretScript->scriptInstanceID = -1;
    auto* turretAudio = turret->AddComponent<AudioSource>();
    turretAudio->clipName = "turret_hum.mp3";
    turretAudio->volume = 0.75f;
    turretAudio->isPlaying = false;
    auto* turretAudioComp = turret->AddComponent<AudioSourceComponent>();
    turretAudioComp->clip_path = "assets/audio/turret.wav";
    turretAudioComp->playing = false;
    turret->AddComponent<SurfaceProperty>(SurfaceType::Metal);

    Entity* crate = scene.CreateEntity("Crate");
    crate->GetComponent<Transform>()->position = glm::vec3(0.0f, 3.0f, 0.0f);
    crate->AddComponent<BoxCollider>()->size = glm::vec3(1.0f);
    auto* crateRb = crate->AddComponent<RigidBody>();
    crateRb->useGravity = true;
    crateRb->velocity = glm::vec3(0.0f);
    auto* crateMr = crate->AddComponent<MeshRenderer>();
    auto crateMesh = Mesh::CreateCubeShared(1.0f);
    crateMr->SetMeshAsset(crateMesh);
    crateMr->color = glm::vec3(0.8f, 0.6f, 0.2f);
    const glm::vec3 originalFirstVertexPos =
        static_cast<const Mesh&>(*crateMesh).GetVertices().front().position;

    const std::string playerPersistentIDOrig = player->GetPersistentID();
    const std::string towerPersistentID = tower->GetPersistentID();
    const std::string turretPersistentID = turret->GetPersistentID();
    const std::string cratePersistentID = crate->GetPersistentID();

    selection.SetSelectedEntity(turret);

    uint64_t playerHandleValue = player->GetHandle().Value();
    std::string playerPersistentID = playerPersistentIDOrig;
    int runtimeStartCount = 0;
    int runtimeStopCount = 0;

    PlayModeSession session;
    PlayModeContext ctx{};
    ctx.scene = &scene;
    ctx.camera = &camera;
    ctx.fpsController = &controller;
    ctx.physicsSystem = &physics;
    ctx.projectileSystem = &projectiles;
    ctx.particleSystem = &particles;
    ctx.selection = &selection;
    ctx.undoHistory = &undoHistory;
    ctx.playerHandleValue = &playerHandleValue;
    ctx.playerPersistentID = &playerPersistentID;
    ctx.onRuntimeStart = [&](Scene*) { ++runtimeStartCount; };
    ctx.onRuntimeStop = [&](Scene*) { ++runtimeStopCount; };

    EXPECT_TRUE(session.EnterPlayMode(ctx));
    EXPECT_TRUE(session.IsPlaying());
    EXPECT_TRUE(runtimeStartCount == 1);

    // Mutate state heavily during Play mode.
    for (int step = 0; step < 45; ++step) {
        EXPECT_TRUE(session.TickSimulation(ctx, nullptr, false, 1.0f / 60.0f));
    }
    EXPECT_TRUE(crate->GetComponent<Transform>()->position.y < 2.9f);

    player->GetComponent<Transform>()->position = glm::vec3(25.0f, 4.0f, -18.0f);
    playerHealth->current = 35.0f;
    playerWeapon->SwitchWeapon(Weapon::WeaponType::Pistol);
    playerWeapon->stats.currentMag = 3;
    playerWeapon->stats.totalAmmo = 11;
    playerWeapon->isReloading = true;
    playerWeapon->reloadTimer = 0.9f;

    camera.SetPosition(glm::vec3(25.0f, 5.7f, -18.0f));
    camera.SetYaw(42.0f);
    camera.SetPitch(19.0f);
    camera.SetRoll(6.0f);
    camera.SetFOV(105.0f);
    controller.AddRecoil(glm::vec3(3.0f, 1.5f, 0.0f));

    // Deform mesh vertices during Play mode.
    crateMesh->GetVertices().front().position += glm::vec3(5.0f, 5.0f, 5.0f);

    // Mutate and then destroy Turret during Play mode!
    turretScript->scriptInstanceID = 99;
    turretAudio->isPlaying = true;
    turretAudioComp->playing = true;
    EXPECT_TRUE(scene.DestroyEntity(turret->GetHandle()));

    // Spawn runtime entities (projectiles, particles, script spawns).
    Entity* bullet = projectiles.SpawnProjectile(
        &scene, glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
        60.0f, 25.0f, player, Projectile::ProjectileType::Bullet);
    EXPECT_TRUE(bullet != nullptr);
    const EntityHandle bulletHandle = bullet->GetHandle();
    particles.EmitBurst(&scene, glm::vec3(0.0f, 1.0f, 0.0f),
                        glm::vec3(0.0f, 1.0f, 0.0f), 4,
                        glm::vec4(1.0f, 0.5f, 0.0f, 1.0f), 5.0f, 0.1f, 2.0f);
    Entity* runtimeEnemy = scene.CreateEntity("RuntimeEnemy");
    const EntityHandle runtimeEnemyHandle = runtimeEnemy->GetHandle();
    selection.SetSelectedEntity(runtimeEnemy);

    EXPECT_TRUE(scene.GetEntities().size() > 5U);

    // Stop Play mode and verify transactional restoration.
    EXPECT_TRUE(session.ExitPlayMode(ctx));
    EXPECT_FALSE(session.IsPlaying());
    EXPECT_TRUE(runtimeStopCount == 1);

    EXPECT_TRUE(scene.GetEntities().size() == 5U);
    EXPECT_TRUE(scene.GetEntity(bulletHandle) == nullptr);
    EXPECT_TRUE(scene.GetEntity(runtimeEnemyHandle) == nullptr);

    Entity* restoredPlayer = scene.GetEntityByPersistentID(playerPersistentIDOrig);
    Entity* restoredTower = scene.GetEntityByPersistentID(towerPersistentID);
    Entity* restoredTurret = scene.GetEntityByPersistentID(turretPersistentID);
    Entity* restoredCrate = scene.GetEntityByPersistentID(cratePersistentID);

    EXPECT_TRUE(restoredPlayer != nullptr);
    EXPECT_TRUE(restoredTower != nullptr);
    EXPECT_TRUE(restoredTurret != nullptr);
    EXPECT_TRUE(restoredCrate != nullptr);

    if (restoredPlayer && restoredTower && restoredTurret && restoredCrate) {
        EXPECT_TRUE(scene.GetEntity(EntityHandle::FromValue(playerHandleValue)) == restoredPlayer);
        EXPECT_TRUE(NearVec3(restoredPlayer->GetComponent<Transform>()->position, glm::vec3(0.0f, 2.0f, 0.0f)));
        EXPECT_TRUE(Near(restoredPlayer->GetComponent<Health>()->current, 100.0f));
        const auto* rw = restoredPlayer->GetComponent<Weapon>();
        EXPECT_TRUE(rw != nullptr);
        if (rw) {
            EXPECT_TRUE(rw->type == Weapon::WeaponType::Rifle);
            EXPECT_TRUE(rw->stats.currentMag == 30);
            EXPECT_TRUE(rw->stats.totalAmmo == 120);
            EXPECT_FALSE(rw->isReloading);
            EXPECT_TRUE(Near(rw->reloadTimer, 0.0f));
        }

        EXPECT_TRUE(restoredTurret->GetParent() == restoredTower);
        EXPECT_TRUE(NearVec3(restoredTurret->GetComponent<Transform>()->position, glm::vec3(0.0f, 3.0f, 0.0f)));
        EXPECT_TRUE(Near(restoredTurret->GetComponent<Health>()->current, 80.0f));
        EXPECT_TRUE(restoredTurret->GetComponent<ScriptComponent>()->scriptInstanceID == -1);
        EXPECT_FALSE(restoredTurret->GetComponent<AudioSource>()->isPlaying);
        EXPECT_FALSE(restoredTurret->GetComponent<AudioSourceComponent>()->playing);
        EXPECT_TRUE(restoredTurret->GetComponent<SurfaceProperty>()->type == SurfaceType::Metal);

        EXPECT_TRUE(NearVec3(restoredCrate->GetComponent<Transform>()->position, glm::vec3(0.0f, 3.0f, 0.0f)));
        EXPECT_TRUE(NearVec3(restoredCrate->GetComponent<RigidBody>()->velocity, glm::vec3(0.0f)));
        EXPECT_TRUE(NearVec3(
            static_cast<const Mesh&>(*restoredCrate->GetComponent<MeshRenderer>()->meshAsset)
                .GetVertices()
                .front()
                .position,
            originalFirstVertexPos));
        EXPECT_TRUE(restoredCrate->GetComponent<MeshRenderer>()->meshAsset->GetSource().type ==
                    MeshSourceType::Cube);

        // Selection rebound to restoredTurret (which was selected before Play).
        EXPECT_TRUE(selection.GetSelectedEntity() == restoredTurret);

        // Pre-Play Edit-mode Undo history rebound to restoredTower.
        Entity* undone = undoHistory.Undo(scene);
        EXPECT_TRUE(undone == restoredTower);
        EXPECT_TRUE(NearVec3(restoredTower->GetComponent<Transform>()->position, glm::vec3(10.0f, 0.0f, -5.0f)));
    }

    EXPECT_TRUE(NearVec3(camera.GetPosition(), glm::vec3(1.0f, 6.0f, 12.0f)));
    EXPECT_TRUE(Near(camera.GetYaw(), -85.0f));
    EXPECT_TRUE(Near(camera.GetPitch(), -12.0f));
    EXPECT_TRUE(Near(camera.GetRoll(), 0.0f));
    EXPECT_TRUE(Near(camera.GetFOV(), 78.0f));
}

void TestTwentyConsecutivePlayStopCyclesPreserveInvariants() {
    Scene scene("TwentyCyclesScene");
    Camera camera(glm::vec3(0.0f, 4.0f, 8.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -10.0f);
    FPSController controller(&camera);
    PhysicsSystem physics;
    physics.Init(&scene);
    ProjectileSystem projectiles;
    projectiles.Init(&scene, &physics);
    ParticleSystem particles;
    particles.Init(&scene);
    EditorSelectionState selection;
    selection.BindScene(&scene);

    Entity* floor = scene.CreateEntity("Floor");
    floor->SetOwnership(EntityOwnership::Engine);
    floor->GetComponent<Transform>()->position = glm::vec3(0.0f, -1.0f, 0.0f);
    floor->GetComponent<Transform>()->scale = glm::vec3(100.0f, 1.0f, 100.0f);
    floor->AddComponent<BoxCollider>()->size = glm::vec3(1.0f);

    Entity* player = scene.CreateEntity("Player");
    player->SetOwnership(EntityOwnership::Engine);
    player->GetComponent<Transform>()->position = glm::vec3(0.0f, 2.0f, 0.0f);
    player->AddComponent<Health>();
    player->AddComponent<Weapon>()->InitInventory();

    Entity* box = scene.CreateEntity("AuthoredBox");
    box->GetComponent<Transform>()->position = glm::vec3(3.0f, 5.0f, -2.0f);
    box->AddComponent<BoxCollider>()->size = glm::vec3(1.0f);
    box->AddComponent<RigidBody>()->useGravity = true;
    box->AddComponent<Health>()->current = 95.0f;

    const std::string playerPID = player->GetPersistentID();
    const std::string boxPID = box->GetPersistentID();
    selection.SetSelectedEntity(box);

    uint64_t playerHandleValue = player->GetHandle().Value();
    std::string playerPersistentID = playerPID;

    PlayModeSession session;
    PlayModeContext ctx{};
    ctx.scene = &scene;
    ctx.camera = &camera;
    ctx.fpsController = &controller;
    ctx.physicsSystem = &physics;
    ctx.projectileSystem = &projectiles;
    ctx.particleSystem = &particles;
    ctx.selection = &selection;
    ctx.playerHandleValue = &playerHandleValue;
    ctx.playerPersistentID = &playerPersistentID;

    for (int cycle = 0; cycle < 20; ++cycle) {
        EXPECT_TRUE(session.EnterPlayMode(ctx));

        // Simulate ticks, including a paused tick.
        for (int t = 0; t < 10; ++t) {
            EXPECT_TRUE(session.TickSimulation(ctx, nullptr, false, 1.0f / 60.0f));
        }
        EXPECT_FALSE(session.TickSimulation(ctx, nullptr, true, 1.0f / 60.0f));

        Entity* livePlayer = scene.GetEntity(EntityHandle::FromValue(playerHandleValue));
        EXPECT_TRUE(livePlayer != nullptr);
        if (livePlayer) {
            livePlayer->GetComponent<Transform>()->position += glm::vec3(static_cast<float>(cycle + 1));
            livePlayer->GetComponent<Health>()->current = 10.0f;
            livePlayer->GetComponent<Weapon>()->stats.currentMag = 5;
        }

        // Spawn runtime objects and on odd cycles destroy the selected box.
        Entity* spawned = scene.CreateEntity("SpawnedInPlay_" + std::to_string(cycle));
        selection.SetSelectedEntity(spawned);
        if ((cycle % 2) == 1) {
            if (Entity* liveBox = scene.GetEntityByPersistentID(boxPID)) {
                scene.DestroyEntity(liveBox->GetHandle());
            }
        }

        EXPECT_TRUE(session.ExitPlayMode(ctx));

        EXPECT_TRUE(scene.GetEntities().size() == 3U);
        Entity* restoredPlayer = scene.GetEntityByPersistentID(playerPID);
        Entity* restoredBox = scene.GetEntityByPersistentID(boxPID);
        EXPECT_TRUE(restoredPlayer != nullptr);
        EXPECT_TRUE(restoredBox != nullptr);
        if (restoredPlayer && restoredBox) {
            EXPECT_TRUE(scene.GetEntity(EntityHandle::FromValue(playerHandleValue)) == restoredPlayer);
            EXPECT_TRUE(NearVec3(restoredPlayer->GetComponent<Transform>()->position, glm::vec3(0.0f, 2.0f, 0.0f)));
            EXPECT_TRUE(Near(restoredPlayer->GetComponent<Health>()->current, 100.0f));
            EXPECT_TRUE(restoredPlayer->GetComponent<Weapon>()->stats.currentMag == 30);
            EXPECT_TRUE(NearVec3(restoredBox->GetComponent<Transform>()->position, glm::vec3(3.0f, 5.0f, -2.0f)));
            EXPECT_TRUE(NearVec3(restoredBox->GetComponent<RigidBody>()->velocity, glm::vec3(0.0f)));
            EXPECT_TRUE(Near(restoredBox->GetComponent<Health>()->current, 95.0f));
            EXPECT_TRUE(selection.GetSelectedEntity() == restoredBox);
        }
    }
}

} // namespace

extern "C" int SDLCALL SDL_SetRelativeMouseMode(SDL_bool) { return 0; }
extern "C" int SDLCALL SDL_ShowCursor(int) { return 0; }

int main() {
    TestBallSettingsRestoreAcrossPlayStop();
    TestRejectedSnapshotsAndWrongSceneStop();
    TestSnapshotLifetimeAndShutdownRelease();
    TestEditorModeFailureAndReentrancy();
    TestSelectionClearedWhenSelectedEntityDestroyed();
    TestSelectionRejectsReplacementEntityAfterDestroy();
    TestMultiSelectionPrunesDestroyedAndPromotesRemainingPrimary();
    TestSelectionClearedWhenOwningSceneDestroyed();
    TestEditModeBlocksSimulationSystems();
    TestPlayStopRestoresAuthoredSceneAndCleansRuntimeState();
    TestTwentyConsecutivePlayStopCyclesPreserveInvariants();

    if (g_Failures != 0) {
        std::cerr << g_Failures << " test expectation(s) failed.\n";
        return 1;
    }
    return 0;
}
