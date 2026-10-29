#include "Editor.h"
#include "../core/Application.h"
#include "../core/ProjectSerializer.h"

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

void Editor::RequestSceneDialog(bool save) {
    if (IsGameRunning()) {
        Log("[Editor] Stop Play before opening or saving an authored scene.");
        return;
    }
    m_SceneDialogSave = save;
    m_RequestSceneDialog = true;
    m_ConfirmSceneAction = false;
    m_SceneIOError.clear();
    const auto& initial = m_CurrentScenePath.empty()
        ? std::string("ArchuraProject.gameproj") : m_CurrentScenePath;
    std::snprintf(m_ScenePathBuffer, sizeof(m_ScenePathBuffer), "%s", initial.c_str());
}

void Editor::SaveCurrentScene(Scene* scene) {
    if (IsGameRunning() || !scene) return;
    if (m_CurrentScenePath.empty()) {
        RequestSceneDialog(true);
        return;
    }
    try {
        std::filesystem::path path;
        std::string error;
        const bool valid = ValidateScenePath(m_CurrentScenePath.c_str(), true, path, error);
        const ProjectConfig config{path.stem().string(), "1.0", "MainScene"};
        if (valid && ProjectSerializer::SaveProject(path.string(), config, scene)) {
            Log("[Save] " + path.string());
            return;
        }
        RequestSceneDialog(true);
        m_SceneIOError = valid
            ? "Save failed. Check file permissions, assets and scene data; the previous file was retained."
            : error;
    } catch (const std::exception& error) {
        RequestSceneDialog(true);
        m_SceneIOError = std::string("Save failed: ") + error.what();
    }
    Log("[ERROR] " + m_SceneIOError);
}

void Editor::DrawSceneDialog(Scene* scene) {
    if (m_RequestSceneDialog) {
        ImGui::OpenPopup("Scene file");
        m_RequestSceneDialog = false;
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
    if (!m_SceneIOError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.50f, 0.40f, 1.0f));
        ImGui::PushTextWrapPos(520);
        ImGui::TextUnformatted(m_SceneIOError.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    if (m_ConfirmSceneAction) {
        ImGui::TextWrapped(m_SceneDialogSave
            ? "This file already exists. Confirm to replace it."
            : "Opening replaces the current authored scene. Save any edits first, or confirm to discard them.");
    }
    if (IsGameRunning()) ImGui::TextDisabled("Stop Play before changing scene files.");
    ImGui::Separator();
    ImGui::BeginDisabled(IsGameRunning() || !scene);
    const char* action = m_ConfirmSceneAction ? "Confirm" : (m_SceneDialogSave ? "Save" : "Open");
    if (ImGui::Button(action, ImVec2(110, 0))) {
        try {
            m_SceneIOError.clear();
            std::filesystem::path path;
            if (ValidateScenePath(m_ScenePathBuffer, m_SceneDialogSave, path, m_SceneIOError)) {
                std::error_code ec;
                const bool exists = std::filesystem::exists(path, ec);
                if (ec) {
                    m_SceneIOError = "Cannot inspect the selected file: " + ec.message();
                } else if (!m_ConfirmSceneAction && (!m_SceneDialogSave || exists)) {
                    m_ConfirmSceneAction = true;
                } else {
                    bool success = false;
                    if (m_SceneDialogSave) {
                        const ProjectConfig config{path.stem().string(), "1.0", "MainScene"};
                        success = ProjectSerializer::SaveProject(path.string(), config, scene);
                    } else if (auto* app = Application::TryGet()) {
                        success = app->LoadEditorProject(path.string());
                    }
                    if (success) {
                        m_CurrentScenePath = path.string();
                        m_SceneIOError.clear();
                        m_ConfirmSceneAction = false;
                        Log(std::string(m_SceneDialogSave ? "[Save] " : "[Open] ") + m_CurrentScenePath);
                        ImGui::CloseCurrentPopup();
                    } else {
                        m_SceneIOError = m_SceneDialogSave
                            ? "Save failed. Check permissions, assets and scene data; the previous file was retained."
                            : "Open failed. Check the file format and referenced assets; the current scene was retained.";
                        m_ConfirmSceneAction = false;
                        Log("[ERROR] " + m_SceneIOError);
                    }
                }
            }
        } catch (const std::exception& error) {
            m_SceneIOError = std::string("Scene operation failed: ") + error.what();
            m_ConfirmSceneAction = false;
            Log("[ERROR] " + m_SceneIOError);
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_ConfirmSceneAction = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
} // namespace Archura
