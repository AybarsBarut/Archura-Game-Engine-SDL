#pragma once

#include "editor/Editor.h"
#include "editor/ComponentEditCommand.h"
#include "editor/SceneDocumentSnapshot.h"
#include "core/Application.h"
#include "core/ProjectSerializer.h"
#include "core/ResourceManager.h"
#include "rendering/Texture.h"
#include "game/Weapon.h"
#include <array>
#include <fstream>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace Archura {

// Native coordinator fixture for --play-stop-smoke-test. Exercises the actual
// editor history and authored Play/Stop boundary; no simulated UI clicks.
class EditorHistorySmoke {
public:
    static void Run(Editor& editor, Application& app, Scene* scene) {
        namespace fs = std::filesystem;
        const auto require = [](bool condition, const char* message) {
            if (!condition) throw std::runtime_error(std::string("History smoke: ") + message);
        };
        struct TemporaryDirectory {
            fs::path path;
            fs::path parent = fs::canonical(fs::temp_directory_path());
            TemporaryDirectory() {
                const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
                for (unsigned attempt = 0; attempt != 64; ++attempt) {
                    const auto candidate = parent / ("archura-history-smoke-" + std::to_string(stamp) + "-" + std::to_string(attempt));
                    if (fs::create_directory(candidate)) { path = candidate; return; }
                }
                throw std::runtime_error("History smoke: cannot create fixture directory");
            }
            ~TemporaryDirectory() {
                // Only the exact successfully created fixture root is owned.
                std::error_code error;
                const auto actual = fs::canonical(path, error);
                if (!error && !path.empty() && path.parent_path() == parent && actual == path)
                    fs::remove_all(path, error);
            }
        } temporary;
        struct RestoreResourceRoot {
            fs::path path = ResourceManager::Get().GetProjectRoot();
            bool explicitRoot = ResourceManager::Get().HasExplicitContentRoot();
            ~RestoreResourceRoot() { ResourceManager::Get().SwapProjectRoot(path, explicitRoot); }
        } restoreRoot;

        require(scene && !editor.IsGameRunning() && !editor.HasPendingDocumentAction(), "initial editor is not idle");
        const auto originalDocument = editor.m_Document;
        const auto originalContents = CaptureSceneDocument(scene);
        require(originalContents.has_value(), "cannot capture initial authored scene");
        const auto baselineFile = temporary.path / "baseline.gameproj";
        const ProjectConfig config{"Native history fixture", "1.0", "Main"};
        const auto* project = editor.GetProjectDescriptor();
        require(project ? ProjectSerializer::SaveProjectPath(baselineFile, config, scene, project->contentRoot)
                        : ProjectSerializer::SaveProjectPath(baselineFile, config, scene),
                "cannot preserve initial authored scene");

        editor.m_UndoHistory.Clear();
        auto* probe = scene->CreateEntity("TransformHistorySmokeProbe");
        const std::string persistentID = probe->GetPersistentID();
        const auto getTransform = [&]() -> Transform* {
            auto* entity = scene->GetEntityByPersistentID(persistentID);
            require(entity != nullptr, "authored transform probe is missing");
            auto* transform = entity->GetComponent<Transform>();
            require(transform != nullptr, "authored probe has no transform");
            return transform;
        };
        editor.ResetDocument(scene);
        const auto authoredBaseline = CaptureSceneDocument(scene);
        const glm::vec3 changedPosition(7, 8, 9), changedRotation(10, 20, 30), changedScale(2, 3, 4);

        editor.PushUndoSnapshot(probe);
        getTransform()->position = {1, 2, 3};
        getTransform()->position = changedPosition;
        getTransform()->rotation = changedRotation;
        getTransform()->scale = changedScale;
        editor.RefreshDocument(scene, true);
        require(editor.IsDocumentDirty() && editor.m_UndoHistory.CanUndo(*scene), "continuous transform edit was not captured");
        editor.UndoLastAction(scene);
        editor.RefreshDocument(scene, true);
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "single gesture Undo did not restore the saved baseline");
        require(!editor.m_UndoHistory.CanUndo(*scene) && editor.m_UndoHistory.CanRedo(*scene), "gesture produced more than one Undo operation");
        require(editor.m_SelectedEntity == scene->GetEntityByPersistentID(persistentID), "Undo did not select the valid changed entity");

        editor.RedoLastAction(scene);
        editor.RefreshDocument(scene, true);
        require(getTransform()->position == changedPosition && getTransform()->rotation == changedRotation &&
                getTransform()->scale == changedScale && editor.IsDocumentDirty(), "Redo did not restore all transform fields and dirty state");
        require(editor.m_SelectedEntity == scene->GetEntityByPersistentID(persistentID), "Redo did not select the valid changed entity");

        editor.UndoLastAction(scene);
        probe = scene->GetEntityByPersistentID(persistentID);
        editor.PushUndoSnapshot(probe); // Activated and deactivated without changing a value.
        require(editor.m_UndoHistory.CanRedo(*scene), "no-op capture removed Redo availability");
        editor.RedoLastAction(scene);
        require(getTransform()->position == changedPosition && getTransform()->rotation == changedRotation &&
                getTransform()->scale == changedScale, "no-op capture corrupted the Redo operation");

        editor.UndoLastAction(scene);
        editor.PushUndoSnapshot(probe);
        const glm::vec3 branchPosition(30, 40, 50), branchRotation(5, 15, 25), branchScale(3, 4, 5);
        getTransform()->position = branchPosition;
        getTransform()->rotation = branchRotation;
        getTransform()->scale = branchScale;
        require(editor.m_UndoHistory.CanUndo(*scene) && !editor.m_UndoHistory.CanRedo(*scene), "a new actual edit did not invalidate the previous Redo branch");
        editor.UndoLastAction(scene);
        editor.RefreshDocument(scene, true);
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty() && editor.m_UndoHistory.CanRedo(*scene),
                "new branch Undo did not leave the authored baseline and its own Redo operation");

        const auto originalProbeHandle = probe->GetHandle();
        require(editor.SetMode(EditorMode::Play), "Play rejected the authored Redo state");
        // Inspector activation in Play must never capture into authored history.
        editor.PushUndoSnapshot(scene->GetEntityByPersistentID(persistentID));
        getTransform()->position = {900, 800, 700};
        require(editor.SetMode(EditorMode::Edit), "Stop failed to restore the authored scene");
        probe = scene->GetEntityByPersistentID(persistentID);
        require(probe && !scene->IsAlive(originalProbeHandle) && probe->GetHandle() != originalProbeHandle,
                "Play/Stop did not replace the probe runtime generation");
        require(CaptureSceneDocument(scene) == authoredBaseline && editor.m_UndoHistory.CanRedo(*scene),
                "Play/Stop lost the authored Redo operation");
        editor.RedoLastAction(scene);
        editor.RefreshDocument(scene, true);
        require(getTransform()->position == branchPosition && getTransform()->rotation == branchRotation &&
                getTransform()->scale == branchScale && editor.m_SelectedEntity == probe && editor.IsDocumentDirty(),
                "Redo did not rebind to the restored persistent entity");
        const auto verifyPendingIsReadOnly = [&] {
            const auto before = CaptureSceneDocument(scene);
            const bool couldUndo = editor.m_UndoHistory.CanUndo(*scene);
            const bool couldRedo = editor.m_UndoHistory.CanRedo(*scene);
            editor.RequestClose(scene);
            require(editor.m_Document.NeedsDecision(), "dirty close did not acquire document input");
            editor.PushUndoSnapshot(probe);
            editor.UndoLastAction(scene);
            require(CaptureSceneDocument(scene) == before, "pending document decision allowed Undo to mutate the scene");
            editor.RedoLastAction(scene);
            require(CaptureSceneDocument(scene) == before, "pending document decision allowed Redo to mutate the scene");
            require(editor.m_UndoHistory.CanUndo(*scene) == couldUndo &&
                    editor.m_UndoHistory.CanRedo(*scene) == couldRedo,
                    "pending document decision consumed authored transform history");
            editor.m_Document.Cancel();
        };
        verifyPendingIsReadOnly(); // Undo is available on the edited branch.
        editor.UndoLastAction(scene);
        editor.RefreshDocument(scene, true);
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "Undo after Play/Stop did not restore the saved authored baseline");
        const std::string probeName = probe->GetName();
        probe->SetName("TransformHistorySmokePendingDecision");
        editor.RefreshDocument(scene, true);
        verifyPendingIsReadOnly(); // A non-transform edit is dirty while Redo is available.
        probe->SetName(probeName);
        editor.RefreshDocument(scene, true);
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty() &&
                editor.m_UndoHistory.CanRedo(*scene), "cancelled document decision lost the preserved Redo operation");

        editor.m_UndoHistory.Clear();
        editor.SetSelectedEntity(probe);
        editor.SpawnEntity(scene, "Ball");
        require(editor.m_SelectedEntity && editor.m_SelectedEntity->GetComponent<RigidBody>(), "Spawn did not select a dynamic ball");
        const auto ballID = editor.m_SelectedEntity->GetPersistentID();
        editor.CopySelected();
        editor.PasteClipboard(scene);
        const auto copyID = editor.m_SelectedEntity->GetPersistentID();
        require(copyID != ballID && editor.IsDocumentDirty(), "Paste did not publish a distinct dirty entity");
        editor.UndoLastAction(scene);
        require(!scene->GetEntityByPersistentID(copyID) && editor.m_SelectedEntity == scene->GetEntityByPersistentID(ballID),
                "Undo Paste did not restore previous selection");
        editor.RedoLastAction(scene);
        require(editor.m_SelectedEntity == scene->GetEntityByPersistentID(copyID), "Redo Paste lost selection");
        const auto entityCount = scene->GetEntities().size();
        editor.RequestClose(scene);
        require(editor.HasPendingDocumentAction(), "command fixture did not acquire document decision");
        require(!editor.ExecuteCreation(scene, [&] { scene->CreateEntity("must not run"); }) &&
                !editor.DeleteEntities(scene, {scene->GetEntityByPersistentID(ballID)->GetHandle()}) &&
                scene->GetEntities().size() == entityCount, "pending decision allowed entity command");
        editor.m_Document.Cancel();
        require(editor.DeleteEntities(scene, {scene->GetEntityByPersistentID(ballID)->GetHandle(), scene->GetEntityByPersistentID(copyID)->GetHandle()}),
                "multi-delete failed");
        require(!scene->GetEntityByPersistentID(ballID) && !scene->GetEntityByPersistentID(copyID) && !editor.m_SelectedEntity &&
                CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "delete did not restore clean baseline");
        editor.UndoLastAction(scene);
        require(scene->GetEntityByPersistentID(ballID) && editor.m_SelectedEntity == scene->GetEntityByPersistentID(copyID),
                "undo deletion failed to reconstruct entities/selection");
        require(editor.SetMode(EditorMode::Play), "command Play failed");
        require(!editor.ExecuteCreation(scene, [&] { scene->CreateEntity("runtime command"); }) &&
                !editor.DeleteEntities(scene, {scene->GetEntityByPersistentID(ballID)->GetHandle()}), "Play accepted authored command");
        require(editor.SetMode(EditorMode::Edit), "command Stop failed");
        editor.RedoLastAction(scene);
        require(!scene->GetEntityByPersistentID(ballID) && !scene->GetEntityByPersistentID(copyID), "redo deletion after Stop failed");
        editor.UndoLastAction(scene);
        editor.UndoLastAction(scene); // Paste
        editor.UndoLastAction(scene); // Ball
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "mixed command history did not restore authored baseline");
        probe = scene->GetEntityByPersistentID(persistentID);
        require(probe && editor.m_SelectedEntity == probe, "creation undo did not restore the original selection after Stop");

        editor.m_UndoHistory.Clear();
        std::string parentID, childID;
        require(editor.ExecuteCreation(scene, [&] {
            parentID = scene->CreateEntity("HierarchyHistoryParent")->GetPersistentID();
            childID = scene->CreateEntity("ComponentHistoryChild")->GetPersistentID();
        }), "structural fixture creation failed");
        auto* structuralParent = scene->GetEntityByPersistentID(parentID);
        auto* structuralChild = scene->GetEntityByPersistentID(childID);
        const auto childHandle = structuralChild->GetHandle();
        auto* childTransform = structuralChild->GetComponent<Transform>();
        require(editor.ReparentEntities(scene, {childHandle}, structuralParent->GetHandle()) && structuralChild->GetParent() == structuralParent,
                "editor reparent failed");
        BoxCollider collider;
        collider.shape = BoxCollider::Shape::Sphere;
        collider.radius = 2.5f;
        require(editor.SetComponentValue<BoxCollider>(scene, childHandle, collider), "editor add collider failed");
        RigidBody body;
        body.mass = 8; body.isKinematic = true;
        require(editor.SetComponentValue<RigidBody>(scene, childHandle, body), "editor add body failed");
        ScriptComponent script;
        script.className = std::string(300, 'S');
        script.scriptInstanceID = 123;
        require(editor.SetComponentValue<ScriptComponent>(scene, childHandle, script) &&
                structuralChild->GetComponent<ScriptComponent>()->scriptInstanceID == -1, "editor add script failed");
        require(scene->GetEntity(childHandle) == structuralChild && structuralChild->GetComponent<Transform>() == childTransform,
                "component command replaced entity or unrelated component");
        require(editor.SetComponentValue<ScriptComponent>(scene, childHandle, std::nullopt), "editor remove script failed");
        editor.UndoLastAction(scene);
        require(structuralChild->GetComponent<ScriptComponent>()->className == script.className, "script undo lost full class name");
        require(!editor.SetComponentValue<ScriptComponent>(scene, childHandle, script) && editor.m_UndoHistory.CanRedo(*scene),
                "no-op component edit consumed Redo");
        editor.RequestClose(scene);
        require(editor.HasPendingDocumentAction() &&
                !editor.ReparentEntities(scene, {childHandle}) &&
                !editor.SetComponentValue<BoxCollider>(scene, childHandle, std::nullopt), "pending decision allowed structural edit");
        editor.m_Document.Cancel();
        require(editor.SetMode(EditorMode::Play), "structural fixture Play failed");
        structuralChild = scene->GetEntityByPersistentID(childID);
        require(!editor.ReparentEntities(scene, {structuralChild->GetHandle()}) &&
                !editor.SetComponentValue<BoxCollider>(scene, structuralChild->GetHandle(), std::nullopt), "Play accepted authored structural command");
        require(editor.SetMode(EditorMode::Edit), "structural fixture Stop failed");
        editor.RedoLastAction(scene);
        structuralChild = scene->GetEntityByPersistentID(childID);
        structuralParent = scene->GetEntityByPersistentID(parentID);
        require(!structuralChild->HasComponent<ScriptComponent>() && structuralChild->GetParent() == structuralParent,
                "component redo after Stop failed");
        require(editor.DeleteEntities(scene, {structuralParent->GetHandle(), structuralChild->GetHandle()}) &&
                CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "structural fixture deletion did not restore baseline");
        editor.UndoLastAction(scene); // Restore deleted entities with fresh generations.
        editor.UndoLastAction(scene); // Restore removed Script.
        structuralChild = scene->GetEntityByPersistentID(childID);
        require(structuralChild && structuralChild->GetComponent<ScriptComponent>()->className == script.className,
                "component command failed after deletion reconstruction");
        editor.UndoLastAction(scene); // Add Script
        editor.UndoLastAction(scene); // Add RigidBody
        editor.UndoLastAction(scene); // Add Collider
        structuralChild = scene->GetEntityByPersistentID(childID);
        require(!structuralChild->HasComponent<ScriptComponent>() && !structuralChild->HasComponent<RigidBody>() &&
                !structuralChild->HasComponent<BoxCollider>(), "component rollback left added values");
        editor.UndoLastAction(scene); // Reparent
        require(!structuralChild->GetParent(), "hierarchy command failed after deletion reconstruction");
        editor.UndoLastAction(scene); // Create
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "mixed structural journal did not restore baseline");
        probe = scene->GetEntityByPersistentID(persistentID);

        editor.m_UndoHistory.Clear();
        const auto propertyHandle = probe->GetHandle();
        RigidBody propertyBody;
        require(editor.SetComponentValue<RigidBody>(scene, propertyHandle, propertyBody), "property fixture add body failed");
        for (float mass : {2.0f, 4.0f, 6.0f}) {
            propertyBody.mass = mass;
            require(editor.SetComponentValue<RigidBody>(scene, propertyHandle, propertyBody, 10001), "property delta failed");
        }
        require(editor.m_UndoHistory.EndGesture(*scene, 10001), "property gesture did not finalize");
        editor.UndoLastAction(scene);
        require(probe->GetComponent<RigidBody>()->mass == 1, "continuous mass gesture did not undo in one step");
        editor.RedoLastAction(scene);
        require(probe->GetComponent<RigidBody>()->mass == 6, "continuous mass gesture did not redo full value");
        editor.UndoLastAction(scene);
        propertyBody.mass = 9;
        require(editor.SetComponentValue<RigidBody>(scene, propertyHandle, propertyBody, 10002), "cancelled property delta failed");
        propertyBody.mass = 1;
        require(editor.SetComponentValue<RigidBody>(scene, propertyHandle, propertyBody, 10002) &&
                editor.m_UndoHistory.EndGesture(*scene, 10002) && editor.m_UndoHistory.CanRedo(*scene), "return-to-origin gesture consumed Redo");
        editor.UndoLastAction(scene); // Add body
        require(!probe->HasComponent<RigidBody>() && CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(),
                "property history did not restore baseline");
        editor.m_UndoHistory.Clear();
        editor.m_InspectorText.Sync(scene, propertyHandle, probe->GetName(), nullptr);
        std::strcpy(editor.m_InspectorText.name, "HistoryRenamedProbe");
        editor.RefreshDocument(scene, true);
        require(probe->GetName() == "HistoryRenamedProbe" && editor.IsDocumentDirty(), "forced name draft did not enter journal");
        editor.UndoLastAction(scene);
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "name Undo replayed stale draft");
        editor.RedoLastAction(scene);
        require(probe->GetName() == "HistoryRenamedProbe", "name Redo failed");
        editor.UndoLastAction(scene);
        ScriptComponent textScript;
        textScript.className = std::string(400, 'L');
        require(editor.SetComponentValue<ScriptComponent>(scene, propertyHandle, textScript), "text fixture script add failed");
        auto* textComponent = probe->GetComponent<ScriptComponent>();
        editor.m_InspectorText.Sync(scene, propertyHandle, probe->GetName(), &textComponent->className);
        std::strcpy(editor.m_InspectorText.script, "ChangedScript");
        editor.RefreshDocument(scene, true);
        require(textComponent->className == "ChangedScript", "script draft commit failed");
        editor.UndoLastAction(scene);
        require(textComponent->className == textScript.className && textComponent->scriptInstanceID == -1 && editor.m_SelectedEntity == probe,
                "script Undo truncated original text or replayed draft");
        editor.UndoLastAction(scene); // Add script
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "text history did not restore baseline");

        editor.m_UndoHistory.Clear();
        const auto materialRoot = ResourceManager::Get().GetProjectRoot();
        const bool materialExplicit = ResourceManager::Get().HasExplicitContentRoot();
        ResourceManager::Get().SetProjectRoot(temporary.path, true);
        const auto writeTexture = [&](const char* name, unsigned char value) {
            const auto path = temporary.path / "assets" / "textures" / name;
            fs::create_directories(path.parent_path());
            std::array<unsigned char, 70> bytes{};
            bytes[0]='B'; bytes[1]='M'; bytes[2]=70; bytes[10]=54; bytes[14]=40;
            bytes[18]=2; bytes[22]=2; bytes[26]=1; bytes[28]=24; bytes[34]=16;
            for (size_t row = 0; row < 2; ++row)
                for (size_t column = 0; column < 6; ++column) bytes[54 + row*8 + column] = value;
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            require(static_cast<bool>(out), "cannot write material texture fixture");
            return path;
        };
        const auto textureA = writeTexture("history-a.bmp", 64);
        writeTexture("history-b.bmp", 192);
        const std::string pathA = "assets/textures/history-a.bmp", pathB = "assets/textures/history-b.bmp";
        std::string materialID;
        require(editor.ExecuteCreation(scene, [&] {
            auto* entity = scene->CreateEntity("MaterialHistoryProbe");
            materialID = entity->GetPersistentID();
            entity->AddComponent<MeshRenderer>()->SetMeshAsset(Mesh::CreateCubeShared());
        }), "material fixture creation failed");
        auto* materialEntity = scene->GetEntityByPersistentID(materialID);
        auto* materialRenderer = materialEntity->GetComponent<MeshRenderer>();
        require(editor.m_GameBuilderPanel.AssignTextureToEntity(materialEntity, "history-a.bmp") &&
                materialRenderer->GetTexture()->IsLoaded() && materialRenderer->GetTexture()->GetPath() == pathA,
                "Game Builder texture command did not upload/select file texture");
        std::weak_ptr<Texture> originalTextureOwner = materialRenderer->textureAsset;
        require(editor.SetMeshColorValue(scene, materialEntity->GetHandle(), {0.5f,0.6f,0.7f}, 11001) &&
                editor.SetMeshColorValue(scene, materialEntity->GetHandle(), {0.2f,0.3f,0.4f}, 11001) &&
                editor.m_UndoHistory.EndGesture(*scene, 11001), "mesh color gesture failed");
        require(editor.SetTextureValue(scene, materialEntity->GetHandle(), pathB) && originalTextureOwner.expired(),
                "texture command retained old GPU ownership");
        const auto* protectedTexture = materialRenderer->GetTexture();
        fs::remove(textureA);
        editor.UndoLastAction(scene);
        require(materialRenderer->GetTexture() == protectedTexture && materialRenderer->GetTexture()->GetPath() == pathB,
                "missing texture Undo changed live renderer");
        writeTexture("history-a.bmp", 64);
        editor.UndoLastAction(scene);
        require(materialRenderer->GetTexture()->IsLoaded() && materialRenderer->GetTexture()->GetPath() == pathA &&
                editor.m_UndoHistory.CanRedo(*scene), "restored texture file did not allow Undo retry");
        editor.RequestClose(scene);
        require(editor.HasPendingDocumentAction() &&
                !editor.SetTextureValue(scene, materialEntity->GetHandle(), {}) &&
                !editor.SetMeshColorValue(scene, materialEntity->GetHandle(), {1,1,1}), "pending decision allowed material edit");
        editor.m_Document.Cancel();
        require(editor.SetMode(EditorMode::Play), "material fixture Play failed");
        materialEntity = scene->GetEntityByPersistentID(materialID);
        require(!editor.SetTextureValue(scene, materialEntity->GetHandle(), {}) &&
                editor.ChangeTexture(scene, materialEntity->GetHandle(), {}, false), "runtime texture edit did not stay outside authored history");
        require(editor.SetMode(EditorMode::Edit), "material fixture Stop failed");
        materialEntity = scene->GetEntityByPersistentID(materialID);
        materialRenderer = materialEntity->GetComponent<MeshRenderer>();
        require(materialRenderer->GetTexture()->IsLoaded() && materialRenderer->GetTexture()->GetPath() == pathA && editor.m_UndoHistory.CanRedo(*scene),
                "Stop did not restore authored texture and Redo");
        editor.RedoLastAction(scene);
        require(materialRenderer->GetTexture()->GetPath() == pathB && materialRenderer->color == glm::vec3(0.2f,0.3f,0.4f),
                "texture Redo altered color or lost assignment");
        require(editor.DeleteEntities(scene, {materialEntity->GetHandle()}), "material fixture delete failed");
        editor.UndoLastAction(scene); // Delete, reconstructs a new renderer generation.
        editor.UndoLastAction(scene); // Assign B
        editor.UndoLastAction(scene); // Entire color gesture
        materialEntity = scene->GetEntityByPersistentID(materialID);
        materialRenderer = materialEntity->GetComponent<MeshRenderer>();
        require(materialRenderer->color == glm::vec3(1) && materialRenderer->GetTexture()->GetPath() == pathA,
                "mixed material Undo failed after entity reconstruction");
        editor.UndoLastAction(scene); // Assign A
        require(!materialRenderer->GetTexture(), "texture Undo did not clear original empty assignment");
        editor.UndoLastAction(scene); // Create
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "material history did not restore clean baseline");
        ResourceManager::Get().SetProjectRoot(materialRoot, materialExplicit);
        probe = scene->GetEntityByPersistentID(persistentID);

        editor.m_UndoHistory.Clear();
        std::string authoringID;
        const std::string originalFace(400, 'S');
        require(editor.ExecuteCreation(scene, [&] {
            auto* entity = scene->CreateEntity("WeaponSkyboxHistoryProbe");
            authoringID = entity->GetPersistentID();
            auto* value = entity->AddComponent<Weapon>(); value->InitInventory();
            value->stats.pattern.offsets = {{1,2},{3,4}};
            entity->AddComponent<SkyboxComponent>()->facePaths[0] = originalFace;
        }), "authoring fixture creation failed");
        auto* authoringEntity = scene->GetEntityByPersistentID(authoringID);
        auto* authoringWeapon = authoringEntity->GetComponent<Weapon>();
        authoringWeapon->isReloading = true; authoringWeapon->reloadTimer = 0.5f; authoringWeapon->shotsFired = 8;
        const Weapon initialWeapon = *authoringWeapon;
        for (float damage : {45.0f, 60.0f}) {
            Weapon value = *authoringWeapon; value.stats.damage = damage;
            require(editor.SetComponentValue<Weapon>(scene, authoringEntity->GetHandle(), value, 12001), "weapon property gesture failed");
        }
        require(editor.m_UndoHistory.EndGesture(*scene, 12001), "weapon gesture finalization failed");
        Weapon switchedWeapon = *authoringWeapon; switchedWeapon.SwitchWeapon(Weapon::WeaponType::Pistol);
        require(editor.SetComponentValue<Weapon>(scene, authoringEntity->GetHandle(), switchedWeapon) &&
                authoringWeapon->isReloading && authoringWeapon->reloadTimer == 0.5f && authoringWeapon->shotsFired == 8,
                "weapon authoring command changed runtime counters");
        editor.m_InspectorText.Sync(scene, authoringEntity->GetHandle(), authoringEntity->GetName(), nullptr);
        std::strcpy(editor.m_InspectorText.skybox[0], "assets/skybox/edited.bmp");
        editor.RefreshDocument(scene, true);
        auto* authoringSky = authoringEntity->GetComponent<SkyboxComponent>();
        require(authoringSky->facePaths[0] == "assets/skybox/edited.bmp" && authoringSky->shouldReload,
                "forced skybox draft did not enter history and request reload");
        editor.RequestClose(scene);
        require(editor.HasPendingDocumentAction() &&
                !editor.SetComponentValue<Weapon>(scene, authoringEntity->GetHandle(), initialWeapon), "pending decision allowed weapon edit");
        editor.m_Document.Cancel();
        require(editor.SetMode(EditorMode::Play), "authoring fixture Play failed");
        authoringEntity = scene->GetEntityByPersistentID(authoringID);
        require(!editor.SetComponentValue<Weapon>(scene, authoringEntity->GetHandle(), initialWeapon), "Play accepted authored weapon command");
        authoringEntity->GetComponent<Weapon>()->stats.damage = 900;
        editor.m_InspectorText.Sync(scene, authoringEntity->GetHandle(), authoringEntity->GetName(), nullptr);
        std::strcpy(editor.m_InspectorText.skybox[0], "runtime-only.bmp");
        editor.CommitInspectorDrafts(scene);
        require(authoringEntity->GetComponent<SkyboxComponent>()->facePaths[0] == "runtime-only.bmp", "Play skybox draft did not edit runtime state");
        require(editor.SetMode(EditorMode::Edit), "authoring fixture Stop failed");
        authoringEntity = scene->GetEntityByPersistentID(authoringID);
        require(authoringEntity->GetComponent<Weapon>()->stats.damage == switchedWeapon.stats.damage &&
                authoringEntity->GetComponent<SkyboxComponent>()->facePaths[0] == "assets/skybox/edited.bmp", "Stop lost authored weapon/skybox values");
        require(editor.DeleteEntities(scene, {authoringEntity->GetHandle()}), "authoring fixture deletion failed");
        editor.UndoLastAction(scene); // Delete
        editor.UndoLastAction(scene); // Skybox text
        authoringEntity = scene->GetEntityByPersistentID(authoringID);
        require(authoringEntity->GetComponent<SkyboxComponent>()->facePaths[0] == originalFace &&
                authoringEntity->GetComponent<SkyboxComponent>()->shouldReload && editor.m_SelectedEntity == authoringEntity, "skybox Undo lost long source or reload after reconstruction");
        editor.UndoLastAction(scene); // Weapon type, inventory and active stats
        authoringWeapon = authoringEntity->GetComponent<Weapon>();
        require(authoringWeapon->type == Weapon::WeaponType::Rifle && authoringWeapon->stats.damage == 60,
                "weapon switch Undo did not restore active stats");
        editor.UndoLastAction(scene); // Whole damage gesture
        require(ComponentHistory::Equal(*authoringWeapon, initialWeapon), "weapon gesture Undo lost inventory or recoil arrays");
        editor.UndoLastAction(scene); // Create
        require(CaptureSceneDocument(scene) == authoredBaseline && !editor.IsDocumentDirty(), "weapon/skybox history did not rewind to clean baseline");
        probe = scene->GetEntityByPersistentID(persistentID);

        editor.m_UndoHistory.Clear();
        editor.PushUndoSnapshot(probe);
        getTransform()->position = {11, 12, 13};
        require(editor.m_UndoHistory.CanUndo(*scene), "deleted-target fixture did not capture a change");
        scene->DestroyEntity(probe->GetHandle());
        editor.SanitizeSelection(scene);
        auto* replacement = scene->CreateEntity("TransformHistorySmokeProbe");
        replacement->GetComponent<Transform>()->position = {88, 89, 90};
        editor.UndoLastAction(scene);
        require(replacement->GetComponent<Transform>()->position == glm::vec3(88, 89, 90), "deleted target Undo changed a replacement entity");
        require(!editor.m_SelectedEntity || scene->OwnsEntityPointer(editor.m_SelectedEntity), "failed Undo retained a stale selection");

        editor.m_UndoHistory.Clear();
        editor.PushUndoSnapshot(replacement);
        replacement->GetComponent<Transform>()->position = {1, 1, 1};
        editor.UndoLastAction(scene);
        require(editor.m_UndoHistory.CanRedo(*scene), "scene-boundary fixture did not create Redo history");
        require(app.NewEditorScene(), "New Scene rejected the history-boundary fixture");
        require(!editor.m_UndoHistory.CanUndo(*scene) && !editor.m_UndoHistory.CanRedo(*scene), "New Scene retained transform Undo/Redo history");

        require(app.LoadEditorProject(baselineFile.u8string()), "cannot restore initial authored scene");
        require(!editor.m_UndoHistory.CanUndo(*scene) && !editor.m_UndoHistory.CanRedo(*scene), "scene load retained transform Undo/Redo history");
        editor.ResetDocument(scene, originalDocument.Path());
        editor.m_Document = originalDocument;
        editor.m_UndoHistory.Clear();
        require(CaptureSceneDocument(scene) == originalContents, "fixture did not restore initial authored contents");
        std::cout << "Editor transform/entity/hierarchy/component history coordinator smoke passed (Undo/Redo, dirty baseline, Play/Stop and scene boundaries; no UI clicks)\n";
    }
};

} // namespace Archura
