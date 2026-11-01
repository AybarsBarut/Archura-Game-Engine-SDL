#include "Editor.h"
#include "ComponentEditCommand.h"
#include "EntityEditCommand.h"
#include "EntityNameEditCommand.h"
#include "HierarchyEditCommand.h"
#include "MeshColorEditCommand.h"
#include "TextureEditCommand.h"

#include <imgui_internal.h>
#include <algorithm>
#include <filesystem>
#include <utility>

namespace Archura {

void Editor::PushUndoSnapshot(Entity *entity) {
  if (entity && !IsGameRunning() && !HasPendingDocumentAction()) m_UndoHistory.Capture(*entity);
}

void Editor::ApplyHistorySelection(Scene* scene, const ActionResult& result) {
  if (result.status != ActionResult::Status::Applied) return;
  m_GizmoActive = GizmoOp::None;
  m_GizmoHovered = GizmoOp::None;
  if (ImGui::GetCurrentContext()) ImGui::ClearActiveID();
  if (result.selectionIDs) {
    ClearSelection();
    std::vector<Entity*> selection;
    for (const auto& id : *result.selectionIDs)
      if (auto* entity = scene->GetEntityByPersistentID(id)) selection.push_back(entity);
    m_SelectionState.SetSelectionList(selection, result.selectedEntity);
    m_SelectedEntity = m_SelectionState.GetSelectedEntity();
    m_SelectedEntities = m_SelectionState.GetSelectedEntities();
    SanitizeSelection(scene);
  } else SetSelectedEntity(result.selectedEntity);
  RefreshDocument(scene, true);
}

bool Editor::ExecuteCreation(Scene* scene, const std::function<void()>& create) {
  if (!scene || IsGameRunning() || HasPendingDocumentAction()) return false;
  RefreshDocument(scene, true);
  SanitizeSelection(scene);
  const auto selection = m_SelectionState.CapturePersistentSnapshot();
  std::string error;
  auto command = EntityEditCommand::Creation(*scene, create, selection.selectedPersistentIDs,
      selection.primaryPersistentID, std::filesystem::u8path(m_Document.Path()).parent_path(), error);
  const auto result = m_UndoHistory.Execute(*scene, command);
  if (result.status != ActionResult::Status::Applied) {
    SanitizeSelection(scene);
    Log("[Create] " + (error.empty() ? "Creation failed; scene and history retained." : error));
    return false;
  }
  ApplyHistorySelection(scene, result);
  return true;
}

bool Editor::DeleteEntities(Scene* scene, const std::vector<EntityHandle>& handles) {
  if (!scene || IsGameRunning() || HasPendingDocumentAction()) return false;
  RefreshDocument(scene, true);
  SanitizeSelection(scene);
  const auto selection = m_SelectionState.CapturePersistentSnapshot();
  std::vector<EntityHandle> unique;
  for (const auto handle : handles)
    if (scene->IsAlive(handle) && std::find(unique.begin(), unique.end(), handle) == unique.end()) unique.push_back(handle);
  std::string error;
  auto command = EntityEditCommand::Deletion(*scene, unique, selection.selectedPersistentIDs,
      selection.primaryPersistentID, std::filesystem::u8path(m_Document.Path()).parent_path(), error);
  const auto result = m_UndoHistory.Execute(*scene, command);
  if (result.status != ActionResult::Status::Applied) {
    Log("[Delete] " + (!error.empty() ? error : command ? command->Error() : "Deletion unavailable."));
    return false;
  }
  ApplyHistorySelection(scene, result);
  return true;
}

bool Editor::ReparentEntities(Scene* scene, const std::vector<EntityHandle>& children, EntityHandle parent) {
  if (!scene || IsGameRunning() || HasPendingDocumentAction()) return false;
  RefreshDocument(scene, true);
  SanitizeSelection(scene);
  const auto selection = m_SelectionState.CapturePersistentSnapshot();
  auto command = HierarchyEditCommand::Create(*scene, children, parent,
      selection.selectedPersistentIDs, selection.primaryPersistentID);
  if (!command) return false; // Includes a no-op: keep the existing Redo branch.
  const auto result = m_UndoHistory.Execute(*scene, command);
  ApplyHistorySelection(scene, result);
  if (result.status == ActionResult::Status::Blocked) Log("[Hierarchy] Invalid or changed hierarchy; scene and history retained.");
  return result.status == ActionResult::Status::Applied;
}

bool Editor::SetTextureValue(Scene* scene, EntityHandle handle, const std::string& path) {
  return ChangeTexture(scene, handle, path, true);
}
bool Editor::ChangeTexture(Scene* scene, EntityHandle handle, const std::string& path, bool record) {
  if (!scene || HasPendingDocumentAction() || record == IsGameRunning()) return false;
  if (record) RefreshDocument(scene, true);
  std::string error;
  auto command = TextureEditCommand::Create(*scene, handle, path, std::filesystem::u8path(m_Document.Path()).parent_path(), error);
  if (!command) { if (!error.empty()) Log("[Texture] " + error); return false; }
  const auto result = record ? m_UndoHistory.Execute(*scene, command) : command->Apply(*scene, false);
  if (record) ApplyHistorySelection(scene, result);
  if (result.status == ActionResult::Status::Blocked) Log("[Texture] " + command->Error());
  return result.status == ActionResult::Status::Applied;
}
bool Editor::SetMeshColorValue(Scene* scene, EntityHandle handle, const glm::vec3& color, uint64_t gesture) {
  if (!scene || IsGameRunning() || HasPendingDocumentAction()) return false;
  if (!gesture) RefreshDocument(scene, true);
  auto command = MeshColorEditCommand::Create(*scene, handle, color);
  if (!command) return false;
  const auto result = gesture ? m_UndoHistory.ExecuteGesture(*scene, command, gesture) : m_UndoHistory.Execute(*scene, command);
  if (gesture) RefreshDocument(scene); else ApplyHistorySelection(scene, result);
  return result.status == ActionResult::Status::Applied;
}

void Editor::CommitInspectorDrafts(Scene* scene) {
  if (!scene) return;
  if (IsGameRunning()) { m_InspectorText.CommitPending(scene); return; }
  if (HasPendingDocumentAction()) return;
  m_InspectorText.CommitPending(scene,
      [&](Entity& entity, const char* value) {
        auto command = EntityNameEditCommand::Create(*scene, entity.GetHandle(), value);
        if (command) m_UndoHistory.Execute(*scene, command);
      },
      [&](Entity& entity, const char* value) {
        ScriptComponent desired = *entity.GetComponent<ScriptComponent>();
        desired.className = value;
        auto command = ComponentEditCommand<ScriptComponent>::Create(*scene, entity.GetHandle(), desired,
            {entity.GetPersistentID()}, entity.GetPersistentID());
        if (command) m_UndoHistory.Execute(*scene, command);
      },
      [&](Entity& entity, SkyboxComponent&& desired) {
        auto command = ComponentEditCommand<SkyboxComponent>::Create(*scene, entity.GetHandle(), std::move(desired),
            {entity.GetPersistentID()}, entity.GetPersistentID());
        if (command) m_UndoHistory.Execute(*scene, command);
      });
}

template<class T> bool Editor::SetComponentValue(Scene* scene, EntityHandle handle, std::optional<T> desired, uint64_t gesture) {
  if (!scene || IsGameRunning() || HasPendingDocumentAction()) return false;
  if (!gesture) RefreshDocument(scene, true);
  SanitizeSelection(scene);
  const auto selection = m_SelectionState.CapturePersistentSnapshot();
  auto command = ComponentEditCommand<T>::Create(*scene, handle, std::move(desired),
      selection.selectedPersistentIDs, selection.primaryPersistentID);
  if (!command) return false;
  const auto result = gesture ? m_UndoHistory.ExecuteGesture(*scene, command, gesture) : m_UndoHistory.Execute(*scene, command);
  if (gesture) RefreshDocument(scene);
  else ApplyHistorySelection(scene, result);
  if (result.status == ActionResult::Status::Blocked) Log("[Component] Component changed; scene and history retained.");
  return result.status == ActionResult::Status::Applied;
}
template bool Editor::SetComponentValue<BoxCollider>(Scene*, EntityHandle, std::optional<BoxCollider>, uint64_t);
template bool Editor::SetComponentValue<RigidBody>(Scene*, EntityHandle, std::optional<RigidBody>, uint64_t);
template bool Editor::SetComponentValue<ScriptComponent>(Scene*, EntityHandle, std::optional<ScriptComponent>, uint64_t);
template bool Editor::SetComponentValue<LightComponent>(Scene*, EntityHandle, std::optional<LightComponent>, uint64_t);
template bool Editor::SetComponentValue<Weapon>(Scene*, EntityHandle, std::optional<Weapon>, uint64_t);
template bool Editor::SetComponentValue<SkyboxComponent>(Scene*, EntityHandle, std::optional<SkyboxComponent>, uint64_t);

void Editor::UndoLastAction(Scene *scene) {
  if (!scene || IsGameRunning() || HasPendingDocumentAction()) return;
  RefreshDocument(scene, true);
  const auto result = m_UndoHistory.UndoAction(*scene);
  ApplyHistorySelection(scene, result);
  if (result.status == ActionResult::Status::Blocked) Log("[Undo] Entity data or assets changed; history retained.");
}

void Editor::RedoLastAction(Scene *scene) {
  if (!scene || IsGameRunning() || HasPendingDocumentAction()) return;
  RefreshDocument(scene, true);
  const auto result = m_UndoHistory.RedoAction(*scene);
  ApplyHistorySelection(scene, result);
  if (result.status == ActionResult::Status::Blocked) Log("[Redo] Entity data or assets changed; history retained.");
}

} // namespace Archura
