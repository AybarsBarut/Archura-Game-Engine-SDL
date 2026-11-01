#include "Editor.h"
#include "internal/EditorPrimitives.h"
#include "../game/Projectile.h"
#include "../game/Weapon.h"
#include "../rendering/Texture.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <type_traits>
#include <utility>

namespace Archura {

void Editor::DrawInspector(Scene *scene) {
  SanitizeSelection(scene);
  ImGuiWindowFlags window_flags = 0;
  ImGui::Begin("Inspector", nullptr, window_flags);

  if (!m_SelectedEntity) {
    CommitInspectorDrafts(scene);
    ImGui::Spacing();
    ImGui::TextUnformatted("Select an object");
    ImGui::TextWrapped("Choose an entity in the Hierarchy or click it in the Scene to inspect its components.");
    ImGui::Separator();
    ImGui::TextDisabled("Create: Entity > Primitives");
    ImGui::TextDisabled("Frame selection: F");
    ImGui::TextDisabled("Duplicate: Ctrl+D");
    ImGui::End();
    return;
  }

  auto *sc = m_SelectedEntity->GetComponent<ScriptComponent>();
  if (!m_InspectorText.IsBoundTo(scene, m_SelectedEntity->GetHandle())) CommitInspectorDrafts(scene);
  m_InspectorText.Sync(scene, m_SelectedEntity->GetHandle(), m_SelectedEntity->GetName(), sc ? &sc->className : nullptr);
  auto& nameBuf = m_InspectorText.name;
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf),
                       ImGuiInputTextFlags_EnterReturnsTrue |
                           ImGuiInputTextFlags_AutoSelectAll))
    CommitInspectorDrafts(scene);
  if (ImGui::IsItemDeactivatedAfterEdit())
    CommitInspectorDrafts(scene);

  ImGui::TextDisabled("ID: %u", m_SelectedEntity->GetID());
  ImGui::Separator();

  // ── Transform ─────────────────────────────────────────────────────────
  auto *tf = m_SelectedEntity->GetComponent<Transform>();
  if (tf &&
      ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Helper: drag + inline reset button
    auto DragVec3WithReset = [this](const char *label, glm::vec3 &v,
                                float defaultVal, float speed) {
      const glm::vec3 before = v;
      ImGui::DragFloat3(label, &v.x, speed);
      if (ImGui::IsItemActivated()) {
        const glm::vec3 updated = v;
        v = before;
        PushUndoSnapshot(m_SelectedEntity);
        v = updated;
      }
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.20f, 0.20f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.45f, 0.25f, 0.25f, 1.0f));
      char rbId[64];
      snprintf(rbId, sizeof(rbId), "R##%s", label);
      if (ImGui::SmallButton(rbId)) {
        PushUndoSnapshot(m_SelectedEntity);
        v = glm::vec3(defaultVal);
      }
      ImGui::PopStyleColor(2);
    };
    DragVec3WithReset("Position", tf->position, 0.0f, 0.1f);
    DragVec3WithReset("Rotation", tf->rotation, 0.0f, 1.0f);
    DragVec3WithReset("Scale", tf->scale, 1.0f, 0.05f);
  }

  auto TrackProperty = [&](bool changed, auto apply) {
    const auto widget = ImGui::GetItemID();
    const bool activated = ImGui::IsItemActivated(), deactivated = ImGui::IsItemDeactivated();
    if (activated || (changed && (!m_PropertyGesture || m_PropertyGestureWidget != widget))) {
      if (++m_PropertyGestureSerial == 0) ++m_PropertyGestureSerial;
      m_PropertyGesture = m_PropertyGestureSerial;
      m_PropertyGestureWidget = widget;
    }
    if (changed) apply(m_PropertyGesture);
    if (deactivated && m_PropertyGestureWidget == widget) {
      m_UndoHistory.EndGesture(*scene, m_PropertyGesture);
      m_PropertyGestureWidget = 0;
      m_PropertyGesture = 0;
      RefreshDocument(scene, true);
    }
  };
  auto EditProperty = [&](auto& component, auto draw) {
    using T = std::decay_t<decltype(component)>;
    T desired = component;
    const bool changed = draw(desired);
    if (IsGameRunning()) { if (changed) component = std::move(desired); }
    else TrackProperty(changed, [&](uint64_t key) {
      SetComponentValue<T>(scene, m_SelectedEntity->GetHandle(), std::move(desired), key);
    });
  };

  // ── Mesh Renderer ──────────────────────────────────────────────────────
  auto *mr = m_SelectedEntity->GetComponent<MeshRenderer>();
  if (mr) {
    if (ImGui::SmallButton("Modify Mesh Geometry"))
      m_ObjectTool.SetOpen(true);
    ImGui::Spacing();
  }
  if (mr && ImGui::CollapsingHeader("Mesh Renderer")) {
    ImGui::Text("Mesh: %s", mr->mesh ? "Loaded" : "None");
    glm::vec3 desired = mr->color;
    const bool changed = ImGui::ColorEdit3("Color##Mesh", &desired.x);
    if (IsGameRunning()) { if (changed) mr->color = desired; }
    else TrackProperty(changed, [&](uint64_t key) { SetMeshColorValue(scene, m_SelectedEntity->GetHandle(), desired, key); });
  }

  // ── Texture ────────────────────────────────────────────────────────────
  if (mr && ImGui::CollapsingHeader("Texture")) {
    if (ImGui::SmallButton("Refresh##Tex"))
      m_AssetCacheDirty = true;
    ImGui::SameLine();
    ImGui::TextDisabled("%zu files", m_CachedTextureFiles.size());

    static int selTex = -1;
    if (ImGui::BeginListBox("##Textures", ImVec2(-1, 120.0f))) {
      for (int i = 0; i < (int)m_CachedTextureFiles.size(); ++i) {
        const bool isSel = (selTex == i);
        if (ImGui::Selectable(m_CachedTextureFiles[i].c_str(), isSel)) {
          selTex = i;
          try {
            ChangeTexture(scene, m_SelectedEntity->GetHandle(), "assets/textures/" + m_CachedTextureFiles[i], !IsGameRunning());
          } catch (const std::exception& error) {
            Log(std::string("[Assets] Texture import failed: ") + error.what());
          }
        }
        if (isSel)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }
    if (mr->texture) {
      if (mr->textureAsset) m_TexturePreviewOwners.push_back(mr->textureAsset);
      ImGui::Image((void *)(intptr_t)mr->texture->GetID(), ImVec2(80, 80));
      ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::Text("ID %u", mr->texture->GetID());
      if (ImGui::SmallButton("Remove##Texture"))
        ChangeTexture(scene, m_SelectedEntity->GetHandle(), {}, !IsGameRunning());
      ImGui::EndGroup();
    }
  }

  // Collider configuration is separate from body motion: no body means static.
  auto *bc = m_SelectedEntity->GetComponent<BoxCollider>();
  if (!bc && ImGui::Button("Add Collider")) {
    BoxCollider desired;
    EditorDetail::FitPrimitiveCollider(desired, mr ? mr->GetMesh() : nullptr);
    if (IsGameRunning()) *m_SelectedEntity->AddComponent<BoxCollider>() = desired;
    else SetComponentValue<BoxCollider>(scene, m_SelectedEntity->GetHandle(), desired);
    bc = m_SelectedEntity->GetComponent<BoxCollider>();
  }
  if (bc && ImGui::CollapsingHeader("Collider")) {
    const char *shapeNames[] = {"Box", "Ramp", "Sphere"};
    EditProperty(*bc, [&](BoxCollider& value) {
      int shape = static_cast<int>(value.shape);
      if (!ImGui::Combo("Shape", &shape, shapeNames, 3)) return false;
      value.shape = static_cast<BoxCollider::Shape>(shape);
      return true;
    });
    if (bc->shape == BoxCollider::Shape::Sphere) {
      EditProperty(*bc, [&](BoxCollider& value) { return ImGui::DragFloat("Radius", &value.radius, 0.01f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
      ImGui::TextDisabled("World radius uses the largest scale axis.");
    } else {
      EditProperty(*bc, [&](BoxCollider& value) { return ImGui::DragFloat3("Size", &value.size.x, 0.05f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
    }
    EditProperty(*bc, [&](BoxCollider& value) { return ImGui::DragFloat3("Center", &value.center.x, 0.05f); });
    EditProperty(*bc, [&](BoxCollider& value) { return ImGui::Checkbox("Is Trigger", &value.isTrigger); });
    if (mr && mr->mesh) {
      const auto type = mr->mesh->GetSource().type;
      if ((type == MeshSourceType::Cube || type == MeshSourceType::Sphere || type == MeshSourceType::Ramp) &&
          ImGui::Button("Fit Primitive Collider")) {
        BoxCollider desired = *bc;
        EditorDetail::FitPrimitiveCollider(desired, mr->GetMesh());
        if (IsGameRunning()) *bc = desired;
        else SetComponentValue<BoxCollider>(scene, m_SelectedEntity->GetHandle(), desired);
      }
    }
  }

  if (bc && ImGui::Button("Remove Collider")) {
    if (IsGameRunning()) m_SelectedEntity->RemoveComponent<BoxCollider>();
    else SetComponentValue<BoxCollider>(scene, m_SelectedEntity->GetHandle(), std::nullopt);
    bc = m_SelectedEntity->GetComponent<BoxCollider>();
  }

  if (ImGui::CollapsingHeader("Physics")) {
    auto* body = m_SelectedEntity->GetComponent<RigidBody>();
    int motion = !body ? 0 : (body->isKinematic ? 2 : 1);
    const char* motionNames[] = {"Static", "Dynamic", "Kinematic"};
    if (ImGui::Combo("Motion", &motion, motionNames, 3)) {
      std::optional<RigidBody> desired;
      if (motion != 0) {
        desired = body ? *body : RigidBody{};
        desired->isKinematic = motion == 2;
        if (!(desired->mass > 0.0f)) desired->mass = 1.0f;
        desired->velocity = desired->force = desired->angularVelocity = glm::vec3(0.0f);
      }
      if (IsGameRunning()) {
        if (desired) *m_SelectedEntity->AddComponent<RigidBody>() = *desired;
        else m_SelectedEntity->RemoveComponent<RigidBody>();
      } else SetComponentValue<RigidBody>(scene, m_SelectedEntity->GetHandle(), std::move(desired));
      body = m_SelectedEntity->GetComponent<RigidBody>();
    }
    if (!bc) ImGui::TextWrapped("Add a Collider for solid contacts and player pushing.");
    if (body) {
      if (body->isKinematic) ImGui::TextDisabled("Moved by gameplay; infinite collision mass.");
      else {
        EditProperty(*body, [&](RigidBody& value) { return ImGui::DragFloat("Mass (kg)", &value.mass, 0.1f, 0.001f, 1000000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
        EditProperty(*body, [&](RigidBody& value) { return ImGui::Checkbox("Use Gravity", &value.useGravity); });
        EditProperty(*body, [&](RigidBody& value) { return ImGui::Checkbox("Pushable by Player", &value.canBePushed); });
      }
      EditProperty(*body, [&](RigidBody& value) { return ImGui::DragFloat("Bounce", &value.restitution, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp); });
      EditProperty(*body, [&](RigidBody& value) { return ImGui::DragFloat("Friction", &value.friction, 0.01f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp); });
      EditProperty(*body, [&](RigidBody& value) { return ImGui::DragFloat("Linear Drag", &value.drag, 0.01f, 0.0f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp); });
      if (bc && bc->shape == BoxCollider::Shape::Sphere)
        EditProperty(*body, [&](RigidBody& value) { return ImGui::DragFloat("Angular Drag", &value.angularDrag, 0.01f, 0.0f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp); });
      EditProperty(*body, [&](RigidBody& value) { return ImGui::Checkbox("Continuous Collision", &value.continuous); });
    }
    if (bc && bc->shape == BoxCollider::Shape::Sphere && ImGui::Button("Apply Ball Physics")) {
      RigidBody desired = body ? *body : RigidBody{};
      EditorDetail::ConfigureBall(desired);
      if (IsGameRunning()) *m_SelectedEntity->AddComponent<RigidBody>() = desired;
      else SetComponentValue<RigidBody>(scene, m_SelectedEntity->GetHandle(), desired);
    }
  }

  // ── Weapon ─────────────────────────────────────────────────────────────
  auto *weapon = m_SelectedEntity->GetComponent<Weapon>();
  if (weapon && ImGui::CollapsingHeader("Weapon")) {
    const char *wTypes[] = {"Rifle",   "Pistol",  "Knife",
                            "Grenade", "Shotgun", "Sniper"};
    EditProperty(*weapon, [&](Weapon& value) {
      int type = static_cast<int>(value.type);
      if (!ImGui::Combo("Type##Weapon", &type, wTypes, 6)) return false;
      value.SwitchWeapon(static_cast<Weapon::WeaponType>(type));
      return true;
    });
    EditProperty(*weapon, [&](Weapon& value) { return ImGui::DragFloat("Damage", &value.stats.damage, 1.0f, 0.0f, 200.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
    EditProperty(*weapon, [&](Weapon& value) { return ImGui::DragFloat("Fire Rate", &value.stats.fireRate, 0.01f, 0.01f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
    EditProperty(*weapon, [&](Weapon& value) { return ImGui::DragFloat("Range##Weapon", &value.stats.range, 1.0f, 10.0f, 500.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
    ImGui::Separator();
    ImGui::Text("Ammo: %d/%d  Total: %d", weapon->stats.currentMag,
                weapon->stats.magSize, weapon->stats.totalAmmo);
  }

  // ── Projectile ─────────────────────────────────────────────────────────
  auto *proj = m_SelectedEntity->GetComponent<Projectile>();
  if (proj && ImGui::CollapsingHeader("Projectile")) {
    ImGui::Text("Speed %.2f  Damage %.2f  Life %.2f", proj->speed, proj->damage,
                proj->lifetime);
    ImGui::DragFloat("Gravity", &proj->gravity, 0.1f, -20.0f, 0.0f);
  }

  // ── Light ──────────────────────────────────────────────────────────────
  auto *lc = m_SelectedEntity->GetComponent<LightComponent>();
  if (lc && ImGui::CollapsingHeader("Light")) {
    const char *ltypes[] = {"Directional", "Point", "Ambient"};
    EditProperty(*lc, [&](LightComponent& value) {
      int type = static_cast<int>(value.type);
      if (!ImGui::Combo("Type", &type, ltypes, 3)) return false;
      value.type = static_cast<LightComponent::Type>(type);
      return true;
    });
    EditProperty(*lc, [&](LightComponent& value) { return ImGui::ColorEdit3("Color", &value.color.x); });
    EditProperty(*lc, [&](LightComponent& value) { return ImGui::DragFloat("Intensity", &value.intensity, 0.05f, 0.0f, 100.0f); });
    if (lc->type == LightComponent::Type::Point)
      EditProperty(*lc, [&](LightComponent& value) { return ImGui::DragFloat("Range", &value.range, 0.5f, 0.0f, 1000.0f); });
  }

  // ── Skybox ─────────────────────────────────────────────────────────────
  auto *sky = m_SelectedEntity->GetComponent<SkyboxComponent>();
  if (sky && sky->facePaths.size() == 6 && ImGui::CollapsingHeader("Skybox")) {
    const char *faceLabels[] = {"Right",  "Left",  "Top",
                                "Bottom", "Front", "Back"};
    for (int i = 0; i < 6; ++i) {
      auto& buffer = m_InspectorText.skybox[i];
      if (ImGui::InputText(faceLabels[i], buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) CommitInspectorDrafts(scene);
      if (ImGui::IsItemDeactivatedAfterEdit()) CommitInspectorDrafts(scene);
    }
    if (ImGui::Button("Reload Skybox"))
      sky->shouldReload = true;
  }

  // ── Script Component ─────────────────────────────────────────────────────
  if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (sc) {
      // Show + edit attached script class name
      auto& scBuf = m_InspectorText.script;

      ImGui::SetNextItemWidth(-60);
      ImGui::PushStyleColor(ImGuiCol_FrameBg,
                            ImVec4(0.12f, 0.22f, 0.12f, 1.0f));
      if (ImGui::InputText("Class##SC", scBuf, sizeof(scBuf),
                           ImGuiInputTextFlags_EnterReturnsTrue))
        CommitInspectorDrafts(scene);
      if (ImGui::IsItemDeactivatedAfterEdit())
        CommitInspectorDrafts(scene);
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.75f, 0.22f, 0.22f, 1.0f));
      if (ImGui::SmallButton("X##RemSC")) {
        if (IsGameRunning()) m_SelectedEntity->RemoveComponent<ScriptComponent>();
        else SetComponentValue<ScriptComponent>(scene, m_SelectedEntity->GetHandle(), std::nullopt);
      }
      ImGui::PopStyleColor(2);
    } else {
      auto& newScBuf = m_InspectorText.newScript;
      ImGui::SetNextItemWidth(-90);
      ImGui::InputText("##NewSC", newScBuf, sizeof(newScBuf));
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.40f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.22f, 0.60f, 0.22f, 1.0f));
      if (ImGui::SmallButton("+ Script") && strlen(newScBuf) > 0) {
        ScriptComponent desired;
        desired.className = newScBuf;
        if (IsGameRunning()) *m_SelectedEntity->AddComponent<ScriptComponent>() = std::move(desired);
        else SetComponentValue<ScriptComponent>(scene, m_SelectedEntity->GetHandle(), std::move(desired));
        newScBuf[0] = '\0';
      }
      ImGui::PopStyleColor(2);
      ImGui::SameLine();
      ImGui::TextDisabled("(C# class name)");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // ── Delete ─────────────────────────────────────────────────────────────
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.82f, 0.22f, 0.22f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
  if (ImGui::Button("Delete Entity", ImVec2(-1, 0))) {
    if (scene && m_SelectedEntity) {
      DeleteEntities(scene, {m_SelectedEntity->GetHandle()});
    }
  }
  ImGui::PopStyleColor(3);

  ImGui::End();
}

} // namespace Archura
