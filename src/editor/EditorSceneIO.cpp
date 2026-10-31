#include "Editor.h"
#include "../core/Application.h"
#include "../core/ProjectSerializer.h"
#include "SceneDocumentSnapshot.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <system_error>

namespace Archura {
namespace {
bool ValidateScenePath(const char* text, bool saving,
                       std::filesystem::path& path, std::string& error) {
    if (!text || !*text) {
        error = "Enter a .gameproj file path.";
        return false;
    }
    path = std::filesystem::path(text).lexically_normal();
    if (saving && !path.has_extension()) path += ".gameproj";
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (extension != ".gameproj") {
        error = "Choose a .gameproj file. Legacy .scene files use load_game in the console.";
        return false;
    }
    std::error_code ec;
    // A bare relative file otherwise has no document root in ProjectSerializer,
    // allowing a previously opened project's asset root to leak into this load.
    // Store the same absolute identity for Save, Open and later Ctrl+S.
    path = std::filesystem::absolute(path, ec).lexically_normal();
    if (ec || !path.is_absolute()) {
        error = "Cannot resolve the scene path against the working directory.";
        if (ec) error += " " + ec.message();
        return false;
    }
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
    if (!std::filesystem::is_directory(parent, ec) || ec) {
        error = "The parent directory does not exist or is inaccessible. Choose an existing directory.";
        return false;
    }
    if (!saving && (!std::filesystem::is_regular_file(path, ec) || ec)) {
        error = "The scene file does not exist or is inaccessible.";
        return false;
    }
    const bool exists = std::filesystem::exists(path, ec);
    if (saving && exists && !ec && std::filesystem::is_directory(path, ec)) {
        error = "The selected path is a directory. Enter a file name.";
        return false;
    }
    if (ec) {
        error = "Cannot inspect the selected path: " + ec.message();
        return false;
    }
    return true;
}
}

void Editor::ResetDocument(Scene* scene, std::string path) {
    m_DocumentScene = scene;
    m_DocumentLifetime = scene ? scene->GetLifetimeToken() : std::weak_ptr<Scene*>{};
    m_Document.Reset(CaptureSceneDocument(scene), std::move(path));
    m_NextDocumentCheck = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
}

void Editor::RefreshDocument(Scene* scene, bool force) {
    if (!scene || IsGameRunning()) return;
    const auto lifetime = m_DocumentLifetime.lock();
    if (!m_Document.Initialized() || m_DocumentScene != scene ||
        !lifetime || lifetime != scene->GetLifetimeToken().lock()) {
        ResetDocument(scene);
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (!force && now < m_NextDocumentCheck) return;
    if (force) m_InspectorText.CommitPending(scene);
    m_Document.Observe(CaptureSceneDocument(scene));
    m_NextDocumentCheck = now + std::chrono::milliseconds(250);
}

bool Editor::ConsumeCloseApproval() {
    const bool approved = m_CloseApproved;
    m_CloseApproved = false;
    return approved;
}

void Editor::RequestClose(Scene* scene) {
    if (HasPendingDocumentAction()) return;
    RefreshDocument(scene, true);
    if (m_Document.Request(SceneDocumentState::Action::Close) && !m_Document.NeedsDecision())
        ExecuteDocumentAction(scene);
}

void Editor::RequestOpenScene(Scene* scene, std::string path, bool legacy) {
    if (HasPendingDocumentAction() || IsGameRunning()) return;
    RefreshDocument(scene, true);
    m_Document.Request(legacy ? SceneDocumentState::Action::OpenLegacy :
                       SceneDocumentState::Action::OpenProject, std::move(path));
}

void Editor::RequestNewScene(Scene* scene) {
    if (HasPendingDocumentAction() || IsGameRunning()) return;
    RefreshDocument(scene, true);
    m_Document.Request(SceneDocumentState::Action::NewScene);
}

void Editor::ExecuteDocumentAction(Scene* scene) {
    if (!m_Document.HasPending() || m_Document.NeedsDecision()) return;
    const auto action = m_Document.PendingAction();
    const std::string target = m_Document.Target();
    if (action == SceneDocumentState::Action::Close) {
        m_CloseApproved = true;
        m_Document.Cancel();
        return;
    }
    bool success = false;
    try {
        if (auto* app = Application::TryGet()) {
            if (action == SceneDocumentState::Action::NewScene) success = app->NewEditorScene();
            else success = action == SceneDocumentState::Action::OpenLegacy
                ? app->LoadEditorLegacy(target) : app->LoadEditorProject(target);
        }
    } catch (const std::exception& error) {
        m_SceneIOError = std::string("Open failed: ") + error.what();
    }
    m_Document.Cancel();
    if (success) {
        ResetDocument(scene, action == SceneDocumentState::Action::OpenProject ? target : std::string{});
        Log("[Open] " + target);
    } else {
        if (m_SceneIOError.empty()) m_SceneIOError = "Open failed. The current scene was retained; check the file and its assets.";
        Log("[ERROR] " + m_SceneIOError);
        // Keep the failure visible without silently retrying the destructive action.
        const std::string error = m_SceneIOError;
        RequestSceneDialog(false);
        m_SceneIOError = error;
        std::snprintf(m_ScenePathBuffer, sizeof(m_ScenePathBuffer), "%s", target.c_str());
    }
}

void Editor::RequestSceneDialog(bool save) {
    if (IsGameRunning() || m_SceneDialogOpen || m_RequestSceneDialog) return;
    m_SceneDialogSave = save;
    m_RequestSceneDialog = true;
    m_ConfirmSceneAction = false;
    m_SceneIOError.clear();
    const std::string initial = m_Document.Path().empty() ? "ArchuraProject.gameproj" : m_Document.Path();
    std::snprintf(m_ScenePathBuffer, sizeof(m_ScenePathBuffer), "%s", initial.c_str());
}

bool Editor::SaveSceneTo(Scene* scene, const std::string& target) {
    if (!scene || IsGameRunning()) return false;
    RefreshDocument(scene, true);
    try {
        std::filesystem::path path;
        if (!ValidateScenePath(target.c_str(), true, path, m_SceneIOError)) return false;
        // Preflight before touching the file; unknown/oversized scenes stay dirty.
        auto snapshot = CaptureSceneDocument(scene);
        if (!snapshot) {
            m_SceneIOError = "Save failed: this scene contains unsupported data or exceeds the document limit.";
            return false;
        }
        const ProjectConfig config{path.stem().string(), "1.0", "MainScene"};
        if (ProjectSerializer::SaveProject(path.string(), config, scene)) {
            m_Document.Saved(std::move(snapshot), path.string());
            m_SceneIOError.clear();
            Log("[Save] " + path.string());
            return true;
        }
        m_SceneIOError = "Save failed. Check permissions, assets and scene data; the previous file was retained.";
    } catch (const std::exception& error) {
        m_SceneIOError = std::string("Save failed: ") + error.what();
    }
    Log("[ERROR] " + m_SceneIOError);
    return false;
}

void Editor::SaveCurrentScene(Scene* scene) {
    if (!scene || IsGameRunning()) return;
    if (!m_Document.Path().empty() && SaveSceneTo(scene, m_Document.Path())) return;
    const std::string error = m_SceneIOError;
    RequestSceneDialog(true);
    m_SceneIOError = error;
}

void Editor::DrawDocumentGuard(Scene* scene) {
    if (m_Document.NeedsDecision() && !m_SceneDialogOpen && !m_RequestSceneDialog)
        ImGui::OpenPopup("Unsaved scene changes");
    if (ImGui::BeginPopupModal("Unsaved scene changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Save changes to the current scene before continuing?");
        if (ImGui::Button("Save", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
            SaveCurrentScene(scene);
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(100, 0))) {
            m_Document.Discard();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_Document.Cancel();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!m_SceneDialogOpen && !m_RequestSceneDialog) ExecuteDocumentAction(scene);
}

void Editor::DrawSceneDialog(Scene* scene) {
    if (m_RequestSceneDialog) {
        ImGui::OpenPopup("Scene file");
        m_RequestSceneDialog = false;
        m_SceneDialogOpen = true;
    }
    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Scene file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextUnformatted(m_SceneDialogSave ? "Save scene project" : "Open scene project");
    ImGui::TextWrapped("Enter an absolute path or a path relative to the engine working directory.");
    ImGui::SetNextItemWidth(520);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##ScenePath", m_ScenePathBuffer, sizeof(m_ScenePathBuffer))) {
        m_ConfirmSceneAction = false;
        m_SceneIOError.clear();
    }
    if (!m_SceneIOError.empty()) ImGui::TextWrapped("%s", m_SceneIOError.c_str());
    if (m_ConfirmSceneAction) ImGui::TextWrapped("This file already exists. Confirm to replace it.");
    ImGui::Separator();
    ImGui::BeginDisabled(IsGameRunning() || !scene);
    if (ImGui::Button(m_ConfirmSceneAction ? "Confirm" : (m_SceneDialogSave ? "Save" : "Open"), ImVec2(110, 0))) {
        try {
            m_SceneIOError.clear();
            std::filesystem::path path;
            if (ValidateScenePath(m_ScenePathBuffer, m_SceneDialogSave, path, m_SceneIOError)) {
                std::error_code ec;
                const bool exists = std::filesystem::exists(path, ec);
                if (ec) m_SceneIOError = "Cannot inspect the selected file: " + ec.message();
                else if (m_SceneDialogSave && exists && !m_ConfirmSceneAction) m_ConfirmSceneAction = true;
                else if (m_SceneDialogSave) {
                    if (SaveSceneTo(scene, path.string())) {
                        m_SceneDialogOpen = false;
                        m_ConfirmSceneAction = false;
                        ImGui::CloseCurrentPopup();
                    } else m_ConfirmSceneAction = false;
                } else {
                    m_SceneDialogOpen = false;
                    ImGui::CloseCurrentPopup();
                    RequestOpenScene(scene, path.string());
                }
            }
        } catch (const std::exception& error) {
            m_SceneIOError = std::string("Scene operation failed: ") + error.what();
            m_ConfirmSceneAction = false;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_ConfirmSceneAction = false;
        m_SceneDialogOpen = false;
        m_Document.Cancel();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
} // namespace Archura
