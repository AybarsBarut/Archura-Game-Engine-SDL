#include "Editor.h"
#include "../core/Application.h"
#include "../core/ResourceManager.h"

#include <cstdio>
#include <cstdlib>

namespace Archura {
namespace {
std::filesystem::path RecentProjectsFile() {
#ifdef _WIN32
    if (const auto* local = _wgetenv(L"LOCALAPPDATA"))
        if (*local) return std::filesystem::path(local) / "Archura" / "Editor" / "recent_projects.json";
#else
    if (const auto* config = std::getenv("XDG_CONFIG_HOME"))
        if (*config) return std::filesystem::path(config) / "Archura" / "Editor" / "recent_projects.json";
    if (const auto* homeDir = std::getenv("HOME"))
        if (*homeDir) return std::filesystem::path(homeDir) / ".config" / "Archura" / "Editor" / "recent_projects.json";
#endif
    return std::filesystem::temp_directory_path() / "Archura" / "Editor" / "recent_projects.json";
}
}

void Editor::InitProjectSession() {
    try {
        m_RecentProjects = std::make_unique<RecentProjects>(RecentProjectsFile());
        std::string error;
        if (!m_RecentProjects->Load(error)) Log("[Projects] Recent projects unavailable: " + error);
    } catch (const std::exception& error) {
        Log(std::string("[Projects] Recent projects unavailable: ") + error.what());
    }
}

void Editor::BindProjectContext(std::optional<ProjectDescriptor> project) {
    m_Project = std::move(project);
    m_BaseProjectDir = m_Project ? m_Project->contentRoot : m_EngineAssetDir;
    m_CurrentProjectDir = m_BaseProjectDir;
    m_SelectedFiles.clear();
    m_LastClickedFile.clear();
    m_ProjectDirectoryCache.Invalidate();
    m_CachedModelFiles.clear();
    m_CachedTextureFiles.clear();
    m_AssetCacheDirty = true;
    m_GameBuilderPanel.InvalidateAssets();
}

void Editor::RequestWorkspace(Scene* scene, std::string path, bool create, std::string name) {
    if (!scene || IsGameRunning() || HasPendingDocumentAction()) return;
    RefreshDocument(scene, true);
    m_PendingProjectName = std::move(name);
    m_Document.Request(create ? SceneDocumentState::Action::CreateWorkspace :
                       SceneDocumentState::Action::OpenWorkspace, std::move(path));
}

bool Editor::OpenStartupProject(Scene* scene, const std::filesystem::path& path) {
    if (!scene || m_Project || IsGameRunning() || HasPendingDocumentAction() || IsDocumentDirty()) return false;
    RequestWorkspace(scene, path.u8string());
    ExecuteDocumentAction(scene);
    return m_Project.has_value() && !HasPendingDocumentAction();
}

void Editor::ExecuteWorkspaceAction(Scene* scene) {
    const bool create = m_Document.PendingAction() == SceneDocumentState::Action::CreateWorkspace;
    const std::string target = m_Document.Target();
    ProjectDescriptor project;
    m_ProjectIOError.clear();
    bool success = false;
    try {
        const auto path = std::filesystem::u8path(target);
        const bool prepared = create
            ? ProjectSession::Create(path, m_PendingProjectName, project, m_ProjectIOError)
            : ProjectSession::Read(path, project, m_ProjectIOError);
        if (prepared) {
            if (auto* app = Application::TryGet()) success = app->LoadEditorWorkspace(project);
            if (!success) m_ProjectIOError = "The project scene could not be opened. The current scene and project were retained.";
        }
        if (success) {
            const auto scenePath = project.startScene.u8string();
            const auto descriptorPath = project.filePath;
            BindProjectContext(std::move(project));
            ResetDocument(scene, scenePath);
            if (m_RecentProjects) {
                std::string error;
                if (!m_RecentProjects->Record(descriptorPath, error)) Log("[Projects] Could not update recent projects: " + error);
            }
            Log("[Project] " + descriptorPath.u8string());
        }
    } catch (const std::exception& error) {
        m_ProjectIOError = std::string("Project operation failed: ") + error.what();
    }
    m_Document.Cancel();
    if (!success) {
        Log("[ERROR] " + m_ProjectIOError);
        m_ProjectDialogCreate = create;
        m_RequestProjectDialog = true;
        std::snprintf(m_ProjectPathBuffer, sizeof(m_ProjectPathBuffer), "%s", target.c_str());
    }
}

void Editor::RequestProjectDialog(bool create) {
    if (IsGameRunning() || HasPendingDocumentAction()) return;
    m_ProjectDialogCreate = create;
    m_ProjectIOError.clear();
    m_RequestProjectDialog = true;
    const auto initial = m_Project ? m_Project->filePath.u8string() : std::string{};
    std::snprintf(m_ProjectPathBuffer, sizeof(m_ProjectPathBuffer), "%s", create ? "" : initial.c_str());
}

void Editor::DrawProjectDialog(Scene* scene) {
    if (m_RequestProjectDialog) {
        ImGui::OpenPopup("Project session");
        m_RequestProjectDialog = false;
        m_ProjectDialogOpen = true;
    }
    ImGui::SetNextWindowSize(ImVec2(600, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Project session", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextUnformatted(m_ProjectDialogCreate ? "Create project" : "Open project");
    ImGui::TextWrapped(m_ProjectDialogCreate
        ? "Choose a new folder in an existing directory. A starter scene and assets folders will be created."
        : "Choose a project.archproj file. To open a legacy .gameproj document, use Open Scene.");
    if (m_ProjectDialogCreate) {
        ImGui::SetNextItemWidth(550);
        ImGui::InputText("Name", m_ProjectNameBuffer, sizeof(m_ProjectNameBuffer));
    }
    ImGui::SetNextItemWidth(550);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputText(m_ProjectDialogCreate ? "Folder" : "File", m_ProjectPathBuffer, sizeof(m_ProjectPathBuffer));
    if (!m_ProjectIOError.empty()) ImGui::TextWrapped("%s", m_ProjectIOError.c_str());
    ImGui::Separator();
    ImGui::BeginDisabled(!scene || !m_ProjectPathBuffer[0] || (m_ProjectDialogCreate && !m_ProjectNameBuffer[0]));
    if (ImGui::Button(m_ProjectDialogCreate ? "Create" : "Open", ImVec2(110, 0))) {
        m_ProjectDialogOpen = false;
        ImGui::CloseCurrentPopup();
        RequestWorkspace(scene, m_ProjectPathBuffer, m_ProjectDialogCreate, m_ProjectNameBuffer);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_ProjectDialogOpen = false;
        m_Document.Cancel();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
} // namespace Archura
