#include "Editor.h"
#include "../core/ResourceManager.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <iterator>
#include <vector>

namespace Archura {

void Editor::DrawSceneHierarchy(Scene *scene) {
  SanitizeSelection(scene);
  ImGuiWindowFlags window_flags = 0;
  ImGui::Begin("Scene Hierarchy", nullptr, window_flags);

  if (!scene) {
    ImGui::TextDisabled("No active scene");
    ImGui::End();
    return;
  }

  const auto &entities = scene->GetEntities();
  ImGui::TextDisabled("%zu entities", entities.size());
  ImGui::SameLine();
  if (ImGui::SmallButton("+ Add"))
    ImGui::OpenPopup("AddEntityPopup");


  if (ImGui::BeginPopup("AddEntityPopup")) {
    if (ImGui::BeginMenu("Primitives")) {
      if (ImGui::MenuItem("Cube"))
        SpawnEntity(scene, "Cube");
      if (ImGui::MenuItem("Sphere"))
        SpawnEntity(scene, "Sphere");
      if (ImGui::MenuItem("Ball (Dynamic Sphere)"))
        SpawnEntity(scene, "Ball");
      if (ImGui::MenuItem("Capsule"))
        SpawnEntity(scene, "Capsule");
      if (ImGui::MenuItem("Stairs"))
        SpawnEntity(scene, "Stairs");
      if (ImGui::MenuItem("Ramp"))
        SpawnEntity(scene, "Ramp");
      if (ImGui::MenuItem("Light"))
        SpawnEntity(scene, "Light");
      ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::TextDisabled("Models");
    for (const auto &fname : m_CachedModelFiles) {
      if (ImGui::MenuItem(fname.c_str())) {
        try {
          SpawnEntity(scene, "Model", ResourceManager::Get().ResolveAssetPath("assets/models/" + fname).string());
        } catch (const std::exception& error) {
          Log(std::string("[Assets] Model import failed: ") + error.what());
        }
      }
    }
    if (m_CachedModelFiles.empty())
      ImGui::TextDisabled("(none in assets/models)");
    ImGui::EndPopup();
  }

  ImGui::Separator();

  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##HierarchySearch", "Search entities", m_HierarchyFilter.InputBuf, IM_ARRAYSIZE(m_HierarchyFilter.InputBuf))) m_HierarchyFilter.Build();

  // Helper: icon prefix by component set
  auto entityIcon = [](Entity *e) -> const char * {
    if (e->GetComponent<LightComponent>())
      return "  ";
    if (e->GetComponent<MeshRenderer>())
      return "  ";
    return "  ";
  };

  std::vector<Entity*> flatList;
  Entity* pendingShiftSelect = nullptr;
  std::vector<EntityHandle> pendingDeleteHandles, pendingParentHandles;
  EntityHandle pendingParent;

  std::function<void(Entity *)> DrawNode = [&](Entity *entity) {
    flatList.push_back(entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanFullWidth;

    bool isSelected = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity) != m_SelectedEntities.end();
    if (isSelected)
      flags |= ImGuiTreeNodeFlags_Selected;

    if (entity->GetChildren().empty() || m_HierarchyFilter.IsActive())
      flags |= ImGuiTreeNodeFlags_Leaf;

    const bool isLookedAt = (entity == m_LookedAtEntity);

    // Alternating row tint
    ImVec2 rowMin = ImGui::GetCursorScreenPos();
    ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetContentRegionAvail().x,
                           rowMin.y + ImGui::GetTextLineHeightWithSpacing());
    float rowAlpha = (int)(entity->GetID()) % 2 == 0 ? 0.04f : 0.0f;
    ImGui::GetWindowDrawList()->AddRectFilled(
        rowMin, rowMax, IM_COL32(255, 255, 255, (int)(rowAlpha * 255)));

    if (isLookedAt)
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.37f, 0.85f, 0.52f, 1.0f));

    char label[192];
    snprintf(label, sizeof(label), "%s%s", entityIcon(entity),
             entity->GetName().c_str());
    bool opened = ImGui::TreeNodeEx((void *)(uintptr_t)entity->GetID(), flags,
                                    "%s", label);

    if (isLookedAt)
      ImGui::PopStyleColor();

    if (ImGui::IsItemClicked()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl) {
            auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
            if (it != m_SelectedEntities.end()) {
                m_SelectedEntities.erase(it);
                if (m_SelectedEntity == entity) {
                    m_SelectedEntity = m_SelectedEntities.empty() ? nullptr : m_SelectedEntities.back();
                }
            } else {
                m_SelectedEntities.push_back(entity);
                m_SelectedEntity = entity;
            }
            m_LastClickedEntity = entity;
            m_SelectionState.SetSelectionList(m_SelectedEntities, m_SelectedEntity);
            m_SelectionState.SetLastClickedEntity(m_LastClickedEntity);
            SanitizeSelection(scene);
        } else if (io.KeyShift && m_LastClickedEntity) {
            pendingShiftSelect = entity;
        } else {
            SetSelectedEntity(entity);
            m_LastClickedEntity = entity;
            m_SelectionState.SetLastClickedEntity(entity);
            SanitizeSelection(scene);
        }
    }

    // Drag & drop
    if (ImGui::BeginDragDropSource()) {
      EntityID id = entity->GetID();
      ImGui::SetDragDropPayload("ENTITY_DRAG", &id, sizeof(EntityID));
      ImGui::Text("%s", entity->GetName().c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload *pl =
              ImGui::AcceptDragDropPayload("ENTITY_DRAG")) {
        EntityID did = *(const EntityID *)pl->Data;
        Entity *de = scene->GetEntity(did);
        if (de && de != entity) {
          pendingParentHandles = {de->GetHandle()};
          pendingParent = entity->GetHandle();
        }
      }
      ImGui::EndDragDropTarget();
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
      bool inSelection = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity) != m_SelectedEntities.end();
      if (ImGui::MenuItem("Unparent")) {
        if (inSelection) {
            pendingParentHandles = m_SelectionState.GetSelectedHandles();
            pendingParent = {};
        } else {
            pendingParentHandles = {entity->GetHandle()};
            pendingParent = {};
        }
      }
      if (ImGui::MenuItem("Delete")) {
        if (inSelection) {
            const auto handles = m_SelectionState.GetSelectedHandles();
            pendingDeleteHandles.insert(pendingDeleteHandles.end(), handles.begin(), handles.end());
        } else {
            pendingDeleteHandles.push_back(entity->GetHandle());
        }
      }
      ImGui::EndPopup();
    }

    if (opened) {
      if (!m_HierarchyFilter.IsActive())
        for (auto *child : entity->GetChildren())
          DrawNode(child);
      ImGui::TreePop();
    }
  };

  for (const auto &ePtr : entities)
    if (m_HierarchyFilter.IsActive() ? m_HierarchyFilter.PassFilter(ePtr->GetName().c_str()) : ePtr->GetParent() == nullptr)
      DrawNode(ePtr.get());

  if (!pendingDeleteHandles.empty()) {
      DeleteEntities(scene, pendingDeleteHandles);
  } else if (pendingShiftSelect && m_LastClickedEntity) {
      auto it1 = std::find(flatList.begin(), flatList.end(), m_LastClickedEntity);
      auto it2 = std::find(flatList.begin(), flatList.end(), pendingShiftSelect);
      if (it1 != flatList.end() && it2 != flatList.end()) {
          size_t idx1 = std::distance(flatList.begin(), it1);
          size_t idx2 = std::distance(flatList.begin(), it2);
          size_t startIdx = std::min(idx1, idx2);
          size_t endIdx = std::max(idx1, idx2);

          if (!ImGui::GetIO().KeyCtrl) m_SelectedEntities.clear();

          for (size_t i = startIdx; i <= endIdx; ++i) {
              if (std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), flatList[i]) == m_SelectedEntities.end()) {
                  m_SelectedEntities.push_back(flatList[i]);
              }
          }
          m_SelectedEntity = pendingShiftSelect;
          m_SelectionState.SetSelectionList(m_SelectedEntities, m_SelectedEntity);
          SanitizeSelection(scene);
      }
  }

  // Empty-space drop target for un-parenting
  ImVec2 avail = ImGui::GetContentRegionAvail();
  if (avail.y < 50.0f)
    avail.y = 50.0f;
  ImGui::InvisibleButton("##HierEmpty", avail);
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload *pl = ImGui::AcceptDragDropPayload("ENTITY_DRAG")) {
      EntityID did = *(const EntityID *)pl->Data;
      if (Entity *de = scene->GetEntity(did)) {
        pendingParentHandles = {de->GetHandle()};
        pendingParent = {};
      }
    }
    ImGui::EndDragDropTarget();
  }

  if (pendingDeleteHandles.empty() && !pendingParentHandles.empty())
    ReparentEntities(scene, pendingParentHandles, pendingParent);
  ImGui::End();
}

} // namespace Archura
