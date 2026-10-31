#pragma once

#include "Editor.h"
#include "SceneDocumentSnapshot.h"
#include "../core/Application.h"
#include "../core/ResourceManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <SDL.h>
#include <vector>

namespace Archura {

// Native integration fixture, invoked only by --play-stop-smoke-test. Exercises
// the real editor, codec, application rebinding and Play session with a live GL
// context. It does not synthesize or claim coverage of modal button clicks.
class EditorDocumentSmoke {
public:
    static void Run(Editor& editor, Application& app, Scene* scene) {
        struct RestoreRoot {
            std::filesystem::path path = ResourceManager::Get().GetProjectRoot();
            ~RestoreRoot() { ResourceManager::Get().SetProjectRoot(path); }
        } restoreRoot;
        const auto require = [](bool condition, const char* message) {
            if (!condition) throw std::runtime_error(std::string("Document smoke: ") + message);
        };
        struct TemporaryDirectory {
            std::filesystem::path path;
            TemporaryDirectory() {
                const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
                const auto parent = std::filesystem::temp_directory_path();
                for (unsigned attempt = 0; attempt != 64; ++attempt) {
                    const auto candidate = parent / ("archura-document-smoke-" + std::to_string(stamp) + "-" + std::to_string(attempt));
                    if (std::filesystem::create_directory(candidate)) {
                        path = candidate;
                        return;
                    }
                }
                throw std::runtime_error("Document smoke: could not create temporary directory");
            }
            ~TemporaryDirectory() {
                // Only the exact, successfully created fixture directory is owned.
                std::error_code error;
                if (!path.empty()) std::filesystem::remove_all(path, error);
            }
        } temporary;

        const std::string originalPath = editor.m_Document.Path();
        const std::string baseline = (temporary.path / "baseline.gameproj").string();
        const std::string saveAs = (temporary.path / "save-as.gameproj").string();
        const std::string missing = (temporary.path / "missing.gameproj").string();
        editor.ResetDocument(scene);
        const auto original = CaptureSceneDocument(scene);
        require(original.has_value(), "initial scene cannot be captured");
        require(editor.SaveSceneTo(scene, baseline), "baseline save failed");
        require(!editor.IsDocumentDirty(), "successful save remained dirty");

        editor.RequestClose(scene);
        require(editor.ConsumeCloseApproval(), "clean close was not approved");
        require(!editor.ConsumeCloseApproval(), "close approval was not consumed");

        scene->CreateEntity("DocumentSmokeAuthoredEdit");
        editor.RefreshDocument(scene, true);
        require(editor.IsDocumentDirty(), "authored entity creation was not detected");
        editor.RequestClose(scene);
        require(editor.m_Document.NeedsDecision(), "dirty close did not request a decision");
        require(!editor.ConsumeCloseApproval(), "dirty close was approved");
        require(!editor.SetMode(EditorMode::Play), "Play accepted a pending close");
        editor.m_Document.Cancel();
        require(!editor.HasPendingDocumentAction() && editor.IsDocumentDirty(), "Cancel lost dirty state");

        editor.RequestClose(scene);
        editor.m_Document.Discard();
        require(editor.IsDocumentDirty(), "Discard incorrectly marked the document clean");
        editor.ExecuteDocumentAction(scene);
        require(editor.ConsumeCloseApproval() && editor.IsDocumentDirty(), "Discard did not authorize only the pending close");

        editor.RequestOpenScene(scene, missing);
        require(editor.m_Document.NeedsDecision(), "dirty open did not request a decision");
        const auto beforeFailedSave = CaptureSceneDocument(scene);
        const std::string invalidSave = (temporary.path / "absent-parent" / "failure.gameproj").string();
        require(!editor.SaveSceneTo(scene, invalidSave), "invalid destination unexpectedly saved");
        require(editor.m_Document.Path() == baseline && editor.m_Document.Target() == missing &&
                editor.m_Document.NeedsDecision() && editor.IsDocumentDirty(), "failed save lost pending action or identity");
        require(CaptureSceneDocument(scene) == beforeFailedSave, "failed save mutated authored contents");
        require(editor.SaveSceneTo(scene, saveAs), "Save As failed");
        require(editor.m_Document.Path() == saveAs && editor.m_Document.Target() == missing &&
                editor.m_Document.HasPending() && !editor.m_Document.NeedsDecision(), "Save As replaced the pending open target");
        editor.m_Document.Cancel();

        scene->CreateEntity("DocumentSmokeSecondEdit");
        editor.RefreshDocument(scene, true);
        const auto beforeFailedOpen = CaptureSceneDocument(scene);
        editor.RequestOpenScene(scene, missing);
        editor.m_Document.Discard();
        editor.ExecuteDocumentAction(scene);
        require(CaptureSceneDocument(scene) == beforeFailedOpen && editor.IsDocumentDirty() &&
                editor.m_Document.Path() == saveAs, "failed open replaced contents or dirty identity");
        require(editor.m_RequestSceneDialog && !editor.m_SceneIOError.empty(), "failed open did not retain a visible error");
        // Dismiss the scheduled error/file dialog without fabricating UI input.
        editor.m_RequestSceneDialog = false;
        editor.m_SceneDialogOpen = false;
        editor.m_SceneIOError.clear();

        editor.RequestOpenScene(scene, baseline);
        editor.m_Document.Discard();
        editor.ExecuteDocumentAction(scene);
        require(!editor.IsDocumentDirty() && editor.m_Document.Path() == baseline &&
                CaptureSceneDocument(scene) == original, "successful open did not establish the loaded baseline");

        require(editor.SetMode(EditorMode::Play), "clean Play rejected");
        scene->CreateEntity("DocumentSmokeRuntimeOnly");
        editor.RefreshDocument(scene, true);
        require(!editor.IsDocumentDirty(), "runtime mutation dirtied authored contents");
        require(editor.SetMode(EditorMode::Edit), "clean Stop rejected");
        editor.RefreshDocument(scene, true);
        require(!editor.IsDocumentDirty() && CaptureSceneDocument(scene) == original, "Stop did not restore clean authored contents");

        scene->CreateEntity("DocumentSmokeDirtyBeforePlay");
        editor.RefreshDocument(scene, true);
        const auto dirtyAuthored = CaptureSceneDocument(scene);
        require(editor.IsDocumentDirty() && editor.SetMode(EditorMode::Play), "dirty authored Play rejected");
        scene->CreateEntity("DocumentSmokeAnotherRuntimeOnly");
        editor.RefreshDocument(scene, true);
        require(editor.IsDocumentDirty(), "Play cleared pre-existing dirty state");
        app.SetDevMode(false);
        SDL_Event close{};
        close.type = SDL_WINDOWEVENT;
        close.window.event = SDL_WINDOWEVENT_CLOSE;
        const auto mainWindowID = SDL_GetWindowID(app.GetWindow().GetNativeWindow());
        close.window.windowID = mainWindowID + 1;
        require(SDL_PushEvent(&close) == 1, "could not queue secondary-window close");
        app.ProcessInput();
        require(editor.IsGameRunning() && !editor.HasPendingDocumentAction() && app.m_Running,
                "secondary-window close reached the document guard");
        close.window.windowID = mainWindowID;
        require(SDL_PushEvent(&close) == 1, "could not queue main-window close");
        app.ProcessInput();
        require(!editor.IsGameRunning() && app.IsDevMode() && app.m_Running &&
                editor.m_Document.NeedsDecision(), "hidden Play close did not Stop and prompt");
        app.SetDevMode(false);
        require(app.IsDevMode(), "pending decision allowed the editor to hide");
        editor.m_Document.Cancel();
        SDL_Event quit{};
        quit.type = SDL_QUIT;
        require(SDL_PushEvent(&quit) == 1, "could not queue SDL quit");
        app.ProcessInput();
        require(app.m_Running && editor.m_Document.NeedsDecision(), "SDL quit bypassed dirty guard");
        editor.m_Document.Cancel();
        editor.RefreshDocument(scene, true);
        require(editor.IsDocumentDirty() && CaptureSceneDocument(scene) == dirtyAuthored, "Stop lost the dirty authored boundary");

        editor.RequestNewScene(scene);
        require(editor.m_Document.NeedsDecision(), "New Scene skipped the dirty decision");
        editor.m_Document.Cancel();
        require(CaptureSceneDocument(scene) == dirtyAuthored && editor.IsDocumentDirty(), "New Scene Cancel lost authored edits");
        std::vector<std::string> engineIDs;
        for (const auto& entity : scene->GetEntities())
            if (entity->IsEngineOwned()) engineIDs.push_back(entity->GetPersistentID());
        editor.RequestNewScene(scene);
        editor.m_Document.Discard();
        editor.ExecuteDocumentAction(scene);
        require(!editor.HasPendingDocumentAction() && !editor.IsDocumentDirty() &&
                editor.m_Document.Path().empty(), "New Scene did not establish an untitled baseline");
        require(scene->GetEntities().size() == engineIDs.size(), "New Scene retained user entities");
        for (const auto& id : engineIDs) {
            const auto* entity = scene->GetEntityByPersistentID(id);
            require(entity && entity->IsEngineOwned(), "New Scene lost an engine-owned entity");
        }

        require(app.LoadEditorProject(baseline), "fixture restore failed");
        editor.ResetDocument(scene, originalPath);
        require(CaptureSceneDocument(scene) == original && !editor.IsDocumentDirty(), "fixture did not restore initial scene");
        std::cout << "Editor document coordinator smoke passed (native codec and Play/Stop; no UI clicks)\n";
    }
};

} // namespace Archura
