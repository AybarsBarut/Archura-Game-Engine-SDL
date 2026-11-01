#pragma once

#include "Editor.h"
#include "SceneDocumentSnapshot.h"
#include "../core/Application.h"
#include "../core/ProjectSerializer.h"
#include "../core/ResourceManager.h"

#include <chrono>
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
