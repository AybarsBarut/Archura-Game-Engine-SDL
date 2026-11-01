#include "Editor.h"

#include <imgui.h>

namespace Archura {

void Editor::DrawTutorialPanel() {
  ImGuiViewport *vp = ImGui::GetMainViewport();
  const float W = vp->WorkSize.x;
  const float H = vp->WorkSize.y;
  const float panelW = 560.0f;
  const float panelH = 520.0f;

  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + (W - panelW) * 0.5f,
                                 vp->WorkPos.y + (H - panelH) * 0.5f),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(440, 380), ImVec2(800, 900));

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
  if (!ImGui::Begin("  Developer Tutorial", &m_ShowTutorial, flags)) {
    ImGui::End();
    return;
  }

  // ── Tab bar ───────────────────────────────────────────────────────────────
  if (ImGui::BeginTabBar("##TutTabs")) {

    // ── TAB 1: Controls ───────────────────────────────────────────────────
    if (ImGui::BeginTabItem("  Controls")) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f),
                         "Editor Camera (Edit Mode)");
      ImGui::Separator();
      ImGui::Spacing();

      auto Row = [](const char *key, const char *desc) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.40f, 1.0f));
        ImGui::Text("  %-24s", key);
        ImGui::PopStyleColor();
        ImGui::SameLine(220.0f);
        ImGui::TextUnformatted(desc);
      };

      Row("RMB + WASD", "Fly camera (hold RMB to look)");
      Row("RMB + Q / E", "Move camera up / down");
      Row("Mouse Scroll", "Zoom FOV");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f), "Selection & Gizmo");
      ImGui::Separator();
      ImGui::Spacing();
      Row("LMB click (scene)", "Select entity");
      Row("LMB drag  X/Y/Z arrow", "Translate on axis");
      Row("LMB drag  circle", "Rotate on axis");
      Row("F", "Focus / frame selected entity");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f),
                         "Keyboard Shortcuts");
      ImGui::Separator();
      ImGui::Spacing();
      Row("Ctrl + Z", "Undo last action");
      Row("Ctrl + C", "Copy selected entity");
      Row("Ctrl + V", "Paste entity");
      Row("Ctrl + D", "Duplicate selected entity");
      Row("Ctrl + A", "Deselect all");
      Row("Delete", "Delete selected entity");
      Row("Ctrl + S", "Save project");
      ImGui::EndTabItem();
    }

    // ── TAB 2: Inspector ──────────────────────────────────────────────────
    if (ImGui::BeginTabItem("  Inspector")) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f), "Inspector Panel");
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::TextWrapped(
          "Select any entity in the Scene Hierarchy to inspect and edit its "
          "components. Click the name field at the top to rename the entity.");
      ImGui::Spacing();

      auto Section = [](const char *title, const char *body) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.95f, 0.60f, 1.0f));
        ImGui::BulletText("%s", title);
        ImGui::PopStyleColor();
        ImGui::Indent(14.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.78f, 0.84f, 1.0f));
        ImGui::TextWrapped("%s", body);
        ImGui::PopStyleColor();
        ImGui::Unindent(14.f);
        ImGui::Spacing();
      };

      Section("Transform",
              "Position / Rotation / Scale each have a drag field. "
              "Press the red 'R' button to reset to defaults (0,0,0 or 1,1,1 "
              "for scale).");
      Section(
          "Mesh Renderer",
          "Displays the loaded mesh and its diffuse color. "
          "Use 'Modify Mesh Geometry' to open the Object Manipulation Tool.");
      Section(
          "Texture",
          "Lists textures found in assets/textures/. "
          "Click a name to apply it. Press 'Refresh' to rescan the folder.");
      Section("Collider",
              "Choose Box, Ramp, or Sphere; tune size or sphere radius and center. "
              "Fit Primitive Collider matches an unchanged cube, sphere, or ramp mesh. "
              "'Is Trigger' objects generate events but don't block movement.");
      Section("Physics",
              "Static objects stay fixed. Dynamic objects have mass and optional gravity; "
              "enable Pushable by Player for player contact impulses. Kinematic objects "
              "are moved by gameplay. Use Apply Ball Physics on a sphere for a bouncy, "
              "pushable body, or spawn a Ball from Create / Game Builder.");
      Section("Script Component",
              "Attaches a C# class name. The scripting backend will look up "
              "and execute the matching class at runtime.");
      Section(
          "Light",
          "Directional, Point, or Ambient. Drag Intensity and Range sliders "
          "to tune falloff. Color is an RGB picker.");
      ImGui::EndTabItem();
    }

    // ── TAB 3: Console Commands ───────────────────────────────────────────
    if (ImGui::BeginTabItem("  Console Cmds")) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f), "Developer Console");
      ImGui::Separator();
      ImGui::TextWrapped(
          "Type commands in the Console panel (bottom strip). "
          "Commands are case-sensitive. Use [Tab] for autocomplete.");
      ImGui::Spacing();

      auto Cmd = [](const char *cmd, const char *desc) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.40f, 1.0f));
        ImGui::Text("  %-28s", cmd);
        ImGui::PopStyleColor();
        ImGui::SameLine(240.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.78f, 0.84f, 1.0f));
        ImGui::TextUnformatted(desc);
        ImGui::PopStyleColor();
      };

      ImGui::TextColored(ImVec4(0.80f, 0.95f, 0.60f, 1.0f), "Rendering");
      Cmd("r_stats", "Show render statistics");
      Cmd("r_reload_shaders", "Hot-reload all shaders");
      Cmd("r_texture_reload", "Reload textures from disk");
      Cmd("r_dump_statistics", "Save stats to file");
      Cmd("r_clear_cache", "Free texture/shader cache");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.80f, 0.95f, 0.60f, 1.0f), "Gameplay");
      Cmd("gravity [0|1]", "Toggle gravity");
      Cmd("teleport [e] [x y z]", "Teleport entity");
      Cmd("bind [key] [cmd]", "Bind key to command");
      Cmd("bindlist", "List all bindings");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.80f, 0.95f, 0.60f, 1.0f),
                         "Cheats (sv_cheats 1)");
      Cmd("god", "Invincibility toggle");
      Cmd("noclip", "No-clip fly mode");
      Cmd("give [item]", "Give item to player");
      Cmd("sv_wireframe", "Toggle wireframe render");
      Cmd("sv_hitbox_debug", "Visualise hitboxes");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.80f, 0.95f, 0.60f, 1.0f), "Network");
      Cmd("net_ping", "Current ping to server");
      Cmd("net_stats", "Full network statistics");
      Cmd("connect [IP:Port]", "Connect to server");
      Cmd("disconnect", "Disconnect from server");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.80f, 0.95f, 0.60f, 1.0f),
                         "System / Profiling");
      Cmd("sys_info", "Show system hardware info");
      Cmd("sys_benchmark", "Run performance benchmark");
      Cmd("profile_start", "Begin profiling session");
      Cmd("profile_stop", "End profiling + print results");
      ImGui::EndTabItem();
    }

    // ── TAB 4: Scene Hierarchy ────────────────────────────────────────────
    if (ImGui::BeginTabItem("  Hierarchy")) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f),
                         "Scene Hierarchy Panel");
      ImGui::Separator();
      ImGui::Spacing();

      auto Tip = [](const char *action, const char *result) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.95f, 0.60f, 1.0f));
        ImGui::BulletText("%s", action);
        ImGui::PopStyleColor();
        ImGui::Indent(14.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.78f, 0.84f, 1.0f));
        ImGui::TextWrapped("%s", result);
        ImGui::PopStyleColor();
        ImGui::Unindent(14.f);
        ImGui::Spacing();
      };

      Tip("+ Add button",
          "Spawns a primitive (Cube, Sphere, Capsule, Stairs, Ramp, Light) "
          "or loads a model from assets/models/.");
      Tip("Left-click entity",
          "Selects the entity and shows its components in the Inspector.");
      Tip("Drag entity → entity",
          "Parents the dragged entity under the drop target. "
          "Child transforms become relative to the parent.");
      Tip("Drag entity → empty space",
          "Un-parents the entity; it becomes a root entity.");
      Tip("Right-click entity → Unparent",
          "Removes the entity from its parent.");
      Tip("Right-click entity → Delete",
          "Destroys the entity and all its children.");
      Tip("... button (top-right)",
          "Toggle 'Fixed Layout' to lock the panel in place.");
      ImGui::EndTabItem();
    }

    // ── TAB 5: Project Panel ──────────────────────────────────────────────
    if (ImGui::BeginTabItem("  Project")) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f),
                         "Project / Asset Panel");
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::TextWrapped(
          "The Project panel (bottom-left) shows your assets/textures and "
          "assets/models directories.");
      ImGui::Spacing();

      auto Tip = [](const char *action, const char *result) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.95f, 0.60f, 1.0f));
        ImGui::BulletText("%s", action);
        ImGui::PopStyleColor();
        ImGui::Indent(14.f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.78f, 0.84f, 1.0f));
        ImGui::TextWrapped("%s", result);
        ImGui::PopStyleColor();
        ImGui::Unindent(14.f);
        ImGui::Spacing();
      };

      Tip("Supported texture formats",
          ".jpg  .jpeg  .png  .tga  .bmp\n"
          "Drop them into assets/textures/, then hit Refresh in the Inspector "
          "Texture section to pick them up.");
      Tip("Supported model formats",
          ".obj  .fbx  (partial .glb / .gltf)\n"
          "Put models in assets/models/. Use Entity > Load Model to spawn.");
      Tip("Refresh button",
          "Rescans asset directories without restarting the engine.");
      Tip("Double-click a folder", "Navigates into that sub-folder.");
      Tip(".. button", "Goes up to the parent directory.");
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f), "Saving & Loading");
      ImGui::Separator();
      ImGui::Spacing();
      Tip("Ctrl + S  /  File > Save Project",
          "Serialises the current scene to "
          "games/ArchuraGame/project.gameproj.");
      Tip("In-game Save (ESC → Save Project)",
          "Saves using GameSaveManager (JSON format in saves/ folder).");
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}

} // namespace Archura
