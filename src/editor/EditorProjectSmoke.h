#pragma once

#include "Editor.h"
#include "SceneDocumentSnapshot.h"
#include "../core/Application.h"
#include "../core/ProjectSerializer.h"
#include "../core/ResourceManager.h"
#include "../rendering/Mesh.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace Archura {

// Native coordinator fixture for --play-stop-smoke-test. It uses the actual
// codec, GL mesh reconstruction and editor transitions; it does not simulate UI.
class EditorProjectSmoke {
public:
    static void Run(Editor& editor, Application& app, Scene* scene) {
        namespace fs = std::filesystem;
        const auto require = [](bool condition, const char* message) {
            if (!condition) throw std::runtime_error(std::string("Project smoke: ") + message);
        };
        struct TemporaryDirectory {
            fs::path path;
            fs::path parent;
            TemporaryDirectory() {
                parent = fs::canonical(fs::temp_directory_path());
                const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
                for (unsigned attempt = 0; attempt != 64; ++attempt) {
                    const auto candidate = parent / ("archura-project-smoke-" + std::to_string(stamp) + "-" + std::to_string(attempt));
                    if (fs::create_directory(candidate)) { path = candidate; return; }
                }
                throw std::runtime_error("Project smoke: cannot create fixture directory");
            }
            ~TemporaryDirectory() {
                // Remove only this exact, exclusively created fixture root.
                std::error_code ec;
                const auto actual = fs::canonical(path, ec);
                if (!ec && !path.empty() && path.parent_path() == parent && actual == path)
                    fs::remove_all(path, ec);
            }
        } temporary;
        struct RestoreDirectory {
            fs::path previous = fs::current_path();
            ~RestoreDirectory() { std::error_code ec; fs::current_path(previous, ec); }
        } restoreDirectory;
        struct RestoreResourceRoot {
            fs::path path = ResourceManager::Get().GetProjectRoot();
            bool explicitRoot = ResourceManager::Get().HasExplicitContentRoot();
            ~RestoreResourceRoot() { ResourceManager::Get().SwapProjectRoot(path, explicitRoot); }
        } restoreRoot;
        struct RestoreRecentProjects {
            std::unique_ptr<RecentProjects>& slot;
            std::unique_ptr<RecentProjects> previous;
            ~RestoreRecentProjects() { slot = std::move(previous); }
        } restoreRecent{editor.m_RecentProjects, std::move(editor.m_RecentProjects)};

        const auto originalProject = editor.m_Project;
        const auto originalDocument = editor.m_Document;
        const auto originalContents = CaptureSceneDocument(scene);
        require(scene && !editor.IsGameRunning() && originalContents.has_value(), "initial authored scene unavailable");
        const auto baseline = temporary.path / "baseline.gameproj";
        const ProjectConfig baselineConfig{"Native project fixture", "1.0", "Main"};
        require(originalProject
            ? ProjectSerializer::SaveProject(baseline.string(), baselineConfig, scene, originalProject->contentRoot)
            : ProjectSerializer::SaveProject(baseline.string(), baselineConfig, scene), "cannot preserve initial scene");

        std::string error;
        const auto preferences = temporary.path / "preferences" / "recent-projects.json";
        editor.m_RecentProjects = std::make_unique<RecentProjects>(preferences);
        require(editor.m_RecentProjects->Load(error), "cannot initialize temporary recent list");
        const auto dismissProjectError = [&] {
            editor.m_ProjectDialogOpen = false;
            editor.m_RequestProjectDialog = false;
            editor.m_ProjectIOError.clear();
            editor.m_Document.Cancel();
        };
        const auto write = [&](const fs::path& path, const std::string& content) {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            require(static_cast<bool>(file << content), "cannot write fixture file");
        };
        const std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1/1 2/2/1 3/3/1\n";
        const auto meshScene = [](const std::string& path) {
            return "{\"format_version\":1,\"project\":{\"name\":\"External\",\"version\":\"1.0\",\"start_scene\":\"Main\"},"
                "\"entities\":[{\"name\":\"WorkspaceMesh\",\"mesh_type\":\"obj\",\"has_mesh_renderer\":true,"
                "\"model_path\":\"" + path + "\"}]}";
        };

        scene->CreateEntity("UnsavedBeforeProjectCreate");
        editor.RefreshDocument(scene, true);
        const auto dirtyBeforeCreate = CaptureSceneDocument(scene);
        const auto firstFolder = temporary.path / "First Project";
        editor.RequestWorkspace(scene, firstFolder.u8string(), true, "First Project");
        require(editor.m_Document.NeedsDecision() && !fs::exists(firstFolder), "Create touched disk before the dirty decision");
        editor.m_Document.Cancel();
        require(CaptureSceneDocument(scene) == dirtyBeforeCreate && !fs::exists(firstFolder), "Cancel changed the scene or created a project");
        editor.m_CachedModelFiles.push_back("stale-model.obj");
        editor.m_CachedTextureFiles.push_back("stale-texture.png");
        editor.m_SelectedFiles.push_back(temporary.path / "stale-selection");
        editor.RequestWorkspace(scene, firstFolder.u8string(), true, "First Project");
        editor.m_Document.Discard();
        editor.ExecuteDocumentAction(scene);
        require(editor.m_Project && editor.m_Project->name == "First Project" && !editor.HasPendingDocumentAction(), "guarded Create did not establish the project");
        const auto first = *editor.m_Project;
        require(editor.m_Document.Path() == first.startScene.string() && !editor.IsDocumentDirty(), "starter scene did not establish a clean document");
        require(ResourceManager::Get().HasExplicitContentRoot() && ResourceManager::Get().GetProjectRoot() == first.contentRoot,
                "Create did not bind the explicit content root");
        require(editor.m_BaseProjectDir == first.contentRoot && editor.m_CurrentProjectDir == first.contentRoot &&
                editor.m_SelectedFiles.empty() && editor.m_CachedModelFiles.empty() && editor.m_CachedTextureFiles.empty(),
                "Create retained the old browser or asset cache");
        Entity* floor = nullptr;
        bool player = false, sun = false;
        for (const auto& entity : scene->GetEntities()) {
            if (entity->GetName() == "Floor") floor = entity.get();
            player |= entity->GetName() == "Player" && entity->GetComponent<BoxCollider>() != nullptr;
            sun |= entity->GetName() == "Sun" && entity->GetComponent<LightComponent>() != nullptr;
        }
        const auto* floorRenderer = floor ? floor->GetComponent<MeshRenderer>() : nullptr;
        require(player && sun && floorRenderer && floorRenderer->GetMesh() &&
                floorRenderer->GetMesh()->GetSource().type == MeshSourceType::Cube, "starter scene did not reconstruct its Player/Floor/Sun and GL primitive");
        require(editor.SetMode(EditorMode::Play) && editor.SetMode(EditorMode::Edit), "starter project Play/Stop failed");
        editor.RefreshDocument(scene, true);
        require(!editor.IsDocumentDirty(), "starter Play/Stop dirtied authored data");
        require(editor.m_RecentProjects->Entries().size() == 1 && editor.m_RecentProjects->Entries().front() == first.filePath,
                "successful Create was not recorded as recent");

        ProjectDescriptor second;
        require(ProjectSession::Create(temporary.path / "External Project", "External Project", second, error), "cannot prepare external fixture project");
        const auto modelPath = second.contentRoot / "assets/models/fixture.obj";
        write(modelPath, obj);
        write(second.startScene, meshScene("assets/models/fixture.obj"));
        const auto unrelated = temporary.path / "Unrelated CWD";
        fs::create_directories(unrelated / "assets/models");
        write(unrelated / "assets/models/fixture.obj", "conflicting invalid OBJ must not be loaded");
        fs::current_path(unrelated);
        editor.RequestWorkspace(scene, second.filePath.u8string());
        editor.ExecuteDocumentAction(scene);
        require(editor.m_Project && editor.m_Project->filePath == second.filePath && !editor.IsDocumentDirty(),
                "external project did not open from unrelated CWD");
        const auto* renderer = scene->GetEntities().empty() ? nullptr : scene->GetEntities().front()->GetComponent<MeshRenderer>();
        require(renderer && renderer->GetMesh() && renderer->GetMesh()->GetSource().type == MeshSourceType::OBJ &&
                std::as_const(*renderer->GetMesh()).GetVertices().size() >= 3, "external relative OBJ did not reconstruct");
        require(ResourceManager::Get().GetProjectRoot() == second.contentRoot && ResourceManager::Get().HasExplicitContentRoot() &&
                editor.m_BaseProjectDir == second.contentRoot && editor.m_CurrentProjectDir == second.contentRoot,
                "external project root or browser was not rebound");
        RecentProjects restarted(preferences);
        require(restarted.Load(error) && restarted.Entries().size() == 2 && restarted.Entries().front() == second.filePath,
                "recent projects did not survive a new store instance");

        scene->CreateEntity("UnsavedBeforeFailedProjectOpen");
        editor.RefreshDocument(scene, true);
        const auto beforeFailure = CaptureSceneDocument(scene);
        const auto previousDocumentPath = editor.m_Document.Path();
        const auto invalidDescriptor = temporary.path / "invalid.archproj";
        write(invalidDescriptor, "{\"project_version\":999}");
        editor.RequestWorkspace(scene, invalidDescriptor.u8string());
        require(editor.m_Document.NeedsDecision(), "invalid-project request skipped the dirty decision");
        editor.m_Document.Discard();
        editor.ExecuteDocumentAction(scene);
        require(CaptureSceneDocument(scene) == beforeFailure && editor.IsDocumentDirty() &&
                editor.m_Document.Path() == previousDocumentPath && editor.m_Project && editor.m_Project->filePath == second.filePath &&
                ResourceManager::Get().GetProjectRoot() == second.contentRoot && ResourceManager::Get().HasExplicitContentRoot(),
                "invalid metadata replaced the current document or project root");
        require(editor.m_RequestProjectDialog && !editor.m_ProjectIOError.empty(), "invalid metadata did not retain a visible error");
        dismissProjectError();

        ProjectDescriptor missingAsset;
        require(ProjectSession::Create(temporary.path / "Missing Asset Project", "Missing Asset", missingAsset, error), "cannot prepare missing-asset fixture");
        write(missingAsset.startScene, meshScene("assets/models/missing.obj"));
        write(unrelated / "assets/models/missing.obj", obj);
        editor.RequestWorkspace(scene, missingAsset.filePath.u8string());
        editor.m_Document.Discard();
        editor.ExecuteDocumentAction(scene);
        require(CaptureSceneDocument(scene) == beforeFailure && editor.IsDocumentDirty() &&
                editor.m_Document.Path() == previousDocumentPath && editor.m_Project && editor.m_Project->filePath == second.filePath &&
                ResourceManager::Get().GetProjectRoot() == second.contentRoot && ResourceManager::Get().HasExplicitContentRoot(),
                "missing asset fell back to CWD or replaced the active session");
        require(editor.m_RequestProjectDialog && !editor.m_ProjectIOError.empty(), "missing asset did not retain a visible error");
        dismissProjectError();
        require(editor.m_RecentProjects->Entries().size() == 2, "failed project opens polluted recents");

        const auto saveAs = temporary.path / fs::u8path(u8"outside-project-İş-游戏.gameproj");
        require(editor.SaveSceneTo(scene, saveAs.u8string()), "scene Save As failed outside the project");
        require(editor.m_Document.Path() == saveAs.u8string() && !editor.IsDocumentDirty() && editor.m_Project &&
                editor.m_Project->filePath == second.filePath && editor.m_Project->startScene == second.startScene &&
                ResourceManager::Get().GetProjectRoot() == second.contentRoot && ResourceManager::Get().HasExplicitContentRoot(),
                "scene Save As changed project identity or content root");
        require(editor.m_RecentProjects->Entries().size() == 2, "scene Save As changed project recents");

        const auto legacy = temporary.path / "legacy.scene";
        write(legacy, "{\"format_version\":1,\"entities\":[{\"name\":\"LegacyAuthored\",\"mesh_type\":\"cube\",\"has_mesh_renderer\":false}]}");
        editor.RequestOpenScene(scene, legacy.string(), true);
        editor.ExecuteDocumentAction(scene);
        require(!editor.m_Project && !ResourceManager::Get().HasExplicitContentRoot() &&
                ResourceManager::Get().GetProjectRoot() == fs::canonical(legacy.parent_path()) && editor.m_Document.Path().empty(),
                "legacy scene load did not detach the explicit project session");
        require(editor.m_BaseProjectDir == editor.m_EngineAssetDir && editor.m_CurrentProjectDir == editor.m_EngineAssetDir,
                "legacy load retained the workspace browser root");

        fs::current_path(restoreDirectory.previous);
        editor.BindProjectContext(originalProject);
        require(app.LoadEditorProject(baseline.string()), "cannot restore initial authored scene");
        editor.ResetDocument(scene, originalDocument.Path());
        editor.m_Document = originalDocument;
        require(CaptureSceneDocument(scene) == originalContents, "fixture did not restore initial scene contents");
        std::cout << "Editor project coordinator smoke passed (native starter/OBJ, explicit roots, recent list and guarded transitions; no UI clicks)\n";
    }
};

} // namespace Archura
