#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Editor.h"
#include "../core/DeveloperConsole.h"
#include "../core/ProjectSerializer.h"
#include "../core/Window.h"
#include "../core/memory/MemoryTracker.h"
#include "../ecs/Component.h"
#include "../ecs/Entity.h"
#include "../game/Projectile.h"
#include "../game/Weapon.h"
#include "../input/Input.h"
#include "../rendering/Camera.h"
#include "../rendering/Mesh.h"
#include "../rendering/Texture.h"

#include <imgui.h>
#include "../../external/imgui/backends/imgui_impl_opengl3.h"
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Archura {

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

Editor::Editor()
    : m_Enabled(true), m_SelectedEntity(nullptr), m_Window(nullptr),
      m_Mode(EditorMode::Edit), m_EditorCamera(glm::vec3(0.0f, 8.0f, 20.0f)) {}

Editor::~Editor() { Shutdown(); }

bool Editor::Init(Window *window) {
  m_Window = window;

  // Redirect cout to console panel
  m_NewCoutBuf = std::make_unique<EditorStreamBuf>(this);
  m_OldCoutBuf = std::cout.rdbuf(m_NewCoutBuf.get());

  // Project browser root
  m_BaseProjectDir = std::filesystem::current_path();
  if (std::filesystem::exists(m_BaseProjectDir / "assets"))
    m_BaseProjectDir /= "assets";
  m_CurrentProjectDir = m_BaseProjectDir;

  ApplyDarkTheme();

  m_AssetCacheDirty = true;
  return true;
}

void Editor::Shutdown() {
  if (m_OldCoutBuf) {
    std::cout.rdbuf(m_OldCoutBuf);
    m_OldCoutBuf = nullptr;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Dark Navy Theme
// ─────────────────────────────────────────────────────────────────────────────

void Editor::ApplyDarkTheme() {
  ImGuiStyle &style = ImGui::GetStyle();

  style.WindowRounding = 6.0f;
  style.ChildRounding = 4.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 4.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.WindowPadding = ImVec2(10.0f, 8.0f);
  style.FramePadding = ImVec2(6.0f, 4.0f);
  style.ItemSpacing = ImVec2(8.0f, 5.0f);
  style.ScrollbarSize = 12.0f;
  style.GrabMinSize = 10.0f;
  style.IndentSpacing = 16.0f;

  // Navy / slate palette – cheap to render, easy on the eyes
  ImVec4 *c = style.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.13f, 0.16f, 1.00f);
  c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
  c[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.13f, 0.16f, 0.97f);
  c[ImGuiCol_Border] = ImVec4(0.25f, 0.28f, 0.34f, 1.00f);
  c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.28f, 0.35f, 1.00f);
  c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
  c[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.17f, 0.24f, 1.00f);
  c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.09f, 0.12f, 0.80f);
  c[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
  c[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
  c[ImGuiCol_ScrollbarGrab] = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
  c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.34f, 0.39f, 0.48f, 1.00f);
  c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.46f, 0.57f, 1.00f);
  c[ImGuiCol_CheckMark] = ImVec4(0.37f, 0.76f, 0.49f, 1.00f);
  c[ImGuiCol_SliderGrab] = ImVec4(0.37f, 0.55f, 0.85f, 1.00f);
  c[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.65f, 0.95f, 1.00f);
  c[ImGuiCol_Button] = ImVec4(0.20f, 0.27f, 0.40f, 1.00f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.38f, 0.56f, 1.00f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.34f, 0.46f, 0.68f, 1.00f);
  c[ImGuiCol_Header] = ImVec4(0.20f, 0.27f, 0.40f, 1.00f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.38f, 0.56f, 1.00f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.34f, 0.46f, 0.68f, 1.00f);
  c[ImGuiCol_Separator] = ImVec4(0.25f, 0.28f, 0.34f, 1.00f);
  c[ImGuiCol_Tab] = ImVec4(0.13f, 0.17f, 0.24f, 1.00f);
  c[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.38f, 0.56f, 1.00f);
  c[ImGuiCol_TabActive] = ImVec4(0.22f, 0.31f, 0.47f, 1.00f);
  c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.22f, 0.32f, 1.00f);
  c[ImGuiCol_Text] = ImVec4(0.86f, 0.89f, 0.94f, 1.00f);
  c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.49f, 0.56f, 1.00f);
  c[ImGuiCol_DragDropTarget] = ImVec4(0.37f, 0.76f, 0.49f, 1.00f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Asset cache
// ─────────────────────────────────────────────────────────────────────────────

void Editor::RefreshAssetCache() {
  m_CachedTextureFiles.clear();
  m_CachedModelFiles.clear();

  auto scanDir = [](const std::string &dir, std::vector<std::string> &out,
                    const auto &extCheck) {
    if (!std::filesystem::exists(dir))
      return;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      if (!entry.is_regular_file())
        continue;
      std::string ext = entry.path().extension().string();
      for (auto &ch : ext)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      if (extCheck(ext))
        out.push_back(entry.path().filename().string());
    }
  };

  scanDir("assets/textures", m_CachedTextureFiles, [](const std::string &e) {
    return e == ".jpg" || e == ".png" || e == ".tga" || e == ".bmp" ||
           e == ".jpeg";
  });
  scanDir("assets/models", m_CachedModelFiles, [](const std::string &e) {
    return e == ".obj" || e == ".fbx" || e == ".glb" || e == ".gltf";
  });

  m_AssetCacheDirty = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level draw
// ─────────────────────────────────────────────────────────────────────────────

bool Editor::WantCaptureMouse() const {
  if (!m_Enabled)
    return false;
  if (!ImGui::GetCurrentContext())
    return false;
  return ImGui::GetIO().WantCaptureMouse;
}

void Editor::BeginDockSpace() {
  // This project uses stock (non-docking) ImGui — docking APIs are unavailable.
  // BeginDockSpace is intentionally a no-op; layout is managed via
  // SetNextWindowPos.
}

void Editor::DrawEditorUI(Scene *scene) {
  if (!m_Enabled)
    return;

  // Lazy asset scan (only on first frame or after explicit refresh)
  if (m_AssetCacheDirty)
    RefreshAssetCache();

  DrawToolbar(scene);

  // Panel layout – first-use positioning only (user can resize)
  ImGuiViewport *vp = ImGui::GetMainViewport();
  const float W = vp->WorkSize.x;
  const float H = vp->WorkSize.y;
  const float Lw = 280.0f; // Hierarchy
  const float Rw = 300.0f; // Inspector
  const float Bh = 220.0f; // Bottom strip
  const float Tb = 36.0f;  // Toolbar height offset
  const float Py = vp->WorkPos.y + Tb;

  if (m_ShowSceneHierarchy) {
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, Py),
                            m_HierarchyLocked ? ImGuiCond_Always
                                              : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(Lw, H - Bh - Tb),
                             m_HierarchyLocked ? ImGuiCond_Always
                                               : ImGuiCond_FirstUseEver);
    DrawSceneHierarchy(scene);
  }
  if (m_ShowInspector) {
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + W - Rw, Py),
                            m_InspectorLocked ? ImGuiCond_Always
                                              : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(Rw, H - Tb), m_InspectorLocked
                                                     ? ImGuiCond_Always
                                                     : ImGuiCond_FirstUseEver);
    DrawInspector(scene);
  }
  if (m_ShowProjectPanel || m_ShowConsole) {
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + Lw, vp->WorkPos.y + H - Bh),
                            m_ConsoleLocked ? ImGuiCond_Always
                                            : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(W - Lw - Rw, Bh),
                             m_ConsoleLocked ? ImGuiCond_Always
                                             : ImGuiCond_FirstUseEver);
    if (m_ShowConsole)
      DrawConsolePanel();
  }
  if (m_ShowProjectPanel) {
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + H - Bh),
                            m_ProjectPanelLocked ? ImGuiCond_Always
                                                 : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(Lw, Bh), m_ProjectPanelLocked
                                                 ? ImGuiCond_Always
                                                 : ImGuiCond_FirstUseEver);
    DrawProjectPanel();
  }
  if (m_ShowPerformance)
    DrawPerformanceMetrics(ImGui::GetIO().DeltaTime, ImGui::GetIO().Framerate);
  if (m_ShowDemoWindow)
    DrawDemoWindow();
  if (m_ShowGameBuilder)
    m_GameBuilderPanel.Draw(scene, m_SelectedEntity);

  m_ObjectTool.Draw(scene, nullptr, m_SelectedEntity);

  if (m_ShowTutorial)
    DrawTutorialPanel();
}

void Editor::Update(Scene *scene, float /*deltaTime*/, float /*fps*/) {
  if (!m_Enabled)
    return;
  DrawEditorUI(scene);
}

// ─────────────────────────────────────────────────────────────────────────────
// Toolbar (Play / Pause / Stop + stats badge)
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawToolbar(Scene * /*scene*/) {
  ImGuiViewport *vp = ImGui::GetMainViewport();
  const float tbH = 36.0f;

  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, tbH), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.09f, 0.12f, 1.0f));

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::Begin("##Toolbar", nullptr, flags);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();

  // ── Left: mode badge ───────────────────────────────────────────────────
  const bool playing = (m_Mode == EditorMode::Play);
  if (playing)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.37f, 0.85f, 0.52f, 1.0f));
  else
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.69f, 0.87f, 1.0f));
  ImGui::Text(playing ? "  PLAY MODE" : "  EDIT MODE");
  ImGui::PopStyleColor();

  // ── Center: Play / Pause / Stop ────────────────────────────────────────
  const float btnW = 70.0f;
  const float totalBtns = btnW * 2.0f + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SameLine((vp->WorkSize.x - totalBtns) * 0.5f);

  if (!playing) {
    // Play button (green tint)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.24f, 0.66f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.14f, 0.40f, 0.24f, 1.0f));
    if (ImGui::Button("  PLAY", ImVec2(btnW, 0))) {
      m_Mode = EditorMode::Play;
      if (m_OnPlay)
        m_OnPlay();
    }
    ImGui::PopStyleColor(3);
  } else {
    // Stop button (red tint)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.42f, 0.10f, 0.10f, 1.0f));
    if (ImGui::Button("  STOP", ImVec2(btnW, 0))) {
      m_Mode = EditorMode::Edit;
      if (m_OnStop)
        m_OnStop();
    }
    ImGui::PopStyleColor(3);
  }

  // ── Right: FPS stat ────────────────────────────────────────────────────
  {
    const float fps = ImGui::GetIO().Framerate;
    char stat[32];
    snprintf(stat, sizeof(stat), "%.0f FPS", fps);
    float tw = ImGui::CalcTextSize(stat).x;
    ImGui::SameLine(vp->WorkSize.x - tw - 14.0f);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          fps >= 60.0f ? ImVec4(0.37f, 0.85f, 0.52f, 1.0f)
                                       : ImVec4(0.90f, 0.55f, 0.20f, 1.0f));
    ImGui::TextUnformatted(stat);
    ImGui::PopStyleColor();
  }

  ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Menu Bar
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawMenuBar(Scene *scene) {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
      }
      if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
        if (scene) {
          ProjectConfig cfg{"ArchuraGame", "1.0", "MainScene"};
          std::filesystem::create_directories("games/ArchuraGame");
          ProjectSerializer::SaveProject("games/ArchuraGame/project.gameproj",
                                         cfg, scene);
          Log("Project saved.");
        }
      }
      if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
      }
      if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Cut", "Ctrl+X")) {
      }
      if (ImGui::MenuItem("Copy", "Ctrl+C")) {
      }
      if (ImGui::MenuItem("Paste", "Ctrl+V")) {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Scene Hierarchy", nullptr, &m_ShowSceneHierarchy);
      ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
      ImGui::MenuItem("Project", nullptr, &m_ShowProjectPanel);
      ImGui::MenuItem("Console", nullptr, &m_ShowConsole);
      ImGui::Separator();
      ImGui::MenuItem("Performance", nullptr, &m_ShowPerformance);
      ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
      ImGui::Separator();
      bool toolOpen = m_ObjectTool.IsOpen();
      if (ImGui::MenuItem("Object Manipulator", nullptr, &toolOpen))
        m_ObjectTool.SetOpen(toolOpen);
      if (ImGui::MenuItem("Game Builder", nullptr, &m_ShowGameBuilder))
        m_GameBuilderPanel.SetOpen(m_ShowGameBuilder);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Entity")) {
      if (ImGui::BeginMenu("Primitives")) {
        if (ImGui::MenuItem("Cube"))
          SpawnEntity(scene, "Cube");
        if (ImGui::MenuItem("Sphere"))
          SpawnEntity(scene, "Sphere");
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
      if (ImGui::BeginMenu("Load Model")) {
        for (const auto &fname : m_CachedModelFiles) {
          if (ImGui::MenuItem(fname.c_str()))
            SpawnEntity(scene, "Model", "assets/models/" + fname);
        }
        if (m_CachedModelFiles.empty())
          ImGui::TextDisabled("No models in assets/models");
        ImGui::EndMenu();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About Archura Engine")) {
      }
      ImGui::Separator();
      ImGui::MenuItem("Developer Tutorial", nullptr, &m_ShowTutorial);
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Overlay (camera info + axis gizmo)
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawOverlay(Scene *scene, Camera *gameCamera) {
  m_ObjectTool.OnSceneGUI(scene, gameCamera, m_SelectedEntity);

  constexpr float DIST = 10.0f;
  ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGuiIO &io = ImGui::GetIO();

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

  // Move overlay to the top-left of the 3D viewport (Left panel is 280, Toolbar is 36)
  float offsetX = m_ShowSceneHierarchy ? 280.0f : 0.0f;
  ImVec2 pos(vp->WorkPos.x + offsetX + DIST, vp->WorkPos.y + 36.0f + DIST);
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.45f);

  if (ImGui::Begin("##Overlay", nullptr, flags)) {
    // Mode ribbon
    if (m_Mode == EditorMode::Edit)
      ImGui::TextColored(ImVec4(0.55f, 0.79f, 1.0f, 1.0f), "EDITOR CAM");
    else
      ImGui::TextColored(ImVec4(0.37f, 0.85f, 0.52f, 1.0f), "PLAY CAM");
    ImGui::Separator();

    // Always show editor camera position when in Edit mode
    glm::vec3 pos3;
    float yaw, pitch;
    if (m_Mode == EditorMode::Edit) {
      pos3 = m_EditorCamera.GetPosition();
      yaw = m_EditorCamera.GetYaw();
      pitch = m_EditorCamera.GetPitch();
    } else if (gameCamera) {
      pos3 = gameCamera->GetPosition();
      yaw = gameCamera->GetYaw();
      pitch = gameCamera->GetPitch();
    } else {
      pos3 = glm::vec3(0.0f);
      yaw = pitch = 0.0f;
    }
    ImGui::Text("Pos  (%.1f, %.1f, %.1f)", pos3.x, pos3.y, pos3.z);
    ImGui::Text("Y/P  (%.1f, %.1f)", yaw, pitch);

    if (m_Mode == EditorMode::Edit)
      ImGui::TextDisabled("[RMB + WASD] to fly");

    // Axis gizmo
    ImGui::Separator();
    ImVec2 orig = ImGui::GetCursorScreenPos();
    const float sz = 32.0f;
    ImVec2 center = ImVec2(orig.x + sz + 6.0f, orig.y + sz + 6.0f);

    const glm::mat4 view = m_EditorCamera.GetViewMatrix();
    const glm::mat3 viewRot = glm::mat3(view);

    glm::vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    ImU32 colors[3] = {IM_COL32(230, 60, 60, 255), IM_COL32(60, 220, 60, 255),
                       IM_COL32(80, 110, 240, 255)};
    const char *labels[3] = {"X", "Y", "Z"};

    ImDrawList *dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < 3; ++i) {
      glm::vec3 v = viewRot * axes[i];
      ImVec2 e = ImVec2(center.x + v.x * sz, center.y - v.y * sz);
      dl->AddLine(center, e, colors[i], 2.5f);
      dl->AddText(e, colors[i], labels[i]);
    }
    ImGui::Dummy(ImVec2(sz * 2.0f + 12.0f, sz * 2.0f + 12.0f));
    ImGui::Separator();
    ImGui::Text("FPS %.1f", io.Framerate);
  }
  ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene Hierarchy
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawSceneHierarchy(Scene *scene) {
  ImGuiWindowFlags window_flags = 0;
  if (m_HierarchyLocked)
    window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoCollapse;
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

  ImGui::SameLine(ImGui::GetWindowWidth() - 35.0f);
  if (ImGui::Button("...##HierMenu"))
    ImGui::OpenPopup("HierConfig");
  if (ImGui::BeginPopup("HierConfig")) {
    if (ImGui::MenuItem("Sabit (Fixed Layout)", nullptr, m_HierarchyLocked))
      m_HierarchyLocked = !m_HierarchyLocked;
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("AddEntityPopup")) {
    if (ImGui::BeginMenu("Primitives")) {
      if (ImGui::MenuItem("Cube"))
        SpawnEntity(scene, "Cube");
      if (ImGui::MenuItem("Sphere"))
        SpawnEntity(scene, "Sphere");
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
      if (ImGui::MenuItem(fname.c_str()))
        SpawnEntity(scene, "Model", "assets/models/" + fname);
    }
    if (m_CachedModelFiles.empty())
      ImGui::TextDisabled("(none in assets/models)");
    ImGui::EndPopup();
  }

  ImGui::Separator();

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

  std::function<void(Entity *)> DrawNode = [&](Entity *entity) {
    flatList.push_back(entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanFullWidth;
                               
    bool isSelected = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity) != m_SelectedEntities.end();
    if (isSelected)
      flags |= ImGuiTreeNodeFlags_Selected;
      
    if (entity->GetChildren().empty())
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
    snprintf(label, sizeof(label), "%s%s##%u", entityIcon(entity),
             entity->GetName().c_str(), entity->GetID());
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
        } else if (io.KeyShift && m_LastClickedEntity) {
            pendingShiftSelect = entity;
        } else {
            SetSelectedEntity(entity);
            m_LastClickedEntity = entity;
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
        if (de && de != entity)
          de->SetParent(entity);
      }
      ImGui::EndDragDropTarget();
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
      bool inSelection = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity) != m_SelectedEntities.end();
      if (ImGui::MenuItem("Unparent")) {
        if (inSelection) {
            for (auto* e : m_SelectedEntities) e->SetParent(nullptr);
        } else {
            entity->SetParent(nullptr);
        }
      }
      if (ImGui::MenuItem("Delete")) {
        if (inSelection) {
            for (auto* e : m_SelectedEntities) scene->DestroyEntity(e->GetID());
            m_SelectedEntities.clear();
            m_SelectedEntity = nullptr;
            m_LastClickedEntity = nullptr;
        } else {
            scene->DestroyEntity(entity->GetID());
            if (m_SelectedEntity == entity) {
                m_SelectedEntities.clear();
                m_SelectedEntity = nullptr;
            }
        }
      }
      ImGui::EndPopup();
    }

    if (opened) {
      for (auto *child : entity->GetChildren())
        DrawNode(child);
      ImGui::TreePop();
    }
  };

  for (const auto &ePtr : entities)
    if (ePtr->GetParent() == nullptr)
      DrawNode(ePtr.get());

  if (pendingShiftSelect && m_LastClickedEntity) {
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
      if (Entity *de = scene->GetEntity(did))
        de->SetParent(nullptr);
    }
    ImGui::EndDragDropTarget();
  }

  ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Inspector
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawInspector(Scene *scene) {
  ImGuiWindowFlags window_flags = 0;
  if (m_InspectorLocked)
    window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoCollapse;
  ImGui::Begin("Inspector", nullptr, window_flags);

  auto DrawMenu = [&]() {
    ImGui::SameLine(ImGui::GetWindowWidth() - 35.0f);
    if (ImGui::Button("...##InspMenu"))
      ImGui::OpenPopup("InspConfig");
    if (ImGui::BeginPopup("InspConfig")) {
      if (ImGui::MenuItem("Sabit (Fixed Layout)", nullptr, m_InspectorLocked))
        m_InspectorLocked = !m_InspectorLocked;
      ImGui::EndPopup();
    }
  };

  if (!m_SelectedEntity) {
    ImGui::TextDisabled("Nothing selected");
    DrawMenu();
    ImGui::End();
    return;
  }

  // Name editing
  static char nameBuf[128] = "";
  if (m_SelectedEntity->GetID() != m_CachedEntityID) {
    strncpy(nameBuf, m_SelectedEntity->GetName().c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    m_CachedEntityID = m_SelectedEntity->GetID();
  }
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf),
                       ImGuiInputTextFlags_EnterReturnsTrue |
                           ImGuiInputTextFlags_AutoSelectAll))
    if (strlen(nameBuf) > 0)
      m_SelectedEntity->SetName(nameBuf);
  if (ImGui::IsItemDeactivatedAfterEdit())
    if (strlen(nameBuf) > 0)
      m_SelectedEntity->SetName(nameBuf);

  ImGui::TextDisabled("ID: %u", m_SelectedEntity->GetID());
  DrawMenu();
  ImGui::Separator();

  // ── Transform ─────────────────────────────────────────────────────────
  auto *tf = m_SelectedEntity->GetComponent<Transform>();
  if (tf &&
      ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Helper: drag + inline reset button
    auto DragVec3WithReset = [](const char *label, glm::vec3 &v,
                                float defaultVal, float speed) {
      ImGui::DragFloat3(label, &v.x, speed);
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.20f, 0.20f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.45f, 0.25f, 0.25f, 1.0f));
      char rbId[64];
      snprintf(rbId, sizeof(rbId), "R##%s", label);
      if (ImGui::SmallButton(rbId))
        v = glm::vec3(defaultVal);
      ImGui::PopStyleColor(2);
    };
    DragVec3WithReset("Position", tf->position, 0.0f, 0.1f);
    DragVec3WithReset("Rotation", tf->rotation, 0.0f, 1.0f);
    DragVec3WithReset("Scale", tf->scale, 1.0f, 0.05f);
  }

  // ── Mesh Renderer ──────────────────────────────────────────────────────
  auto *mr = m_SelectedEntity->GetComponent<MeshRenderer>();
  if (mr) {
    if (ImGui::SmallButton("Modify Mesh Geometry"))
      m_ObjectTool.SetOpen(true);
    ImGui::Spacing();
  }
  if (mr && ImGui::CollapsingHeader("Mesh Renderer")) {
    ImGui::Text("Mesh: %s", mr->mesh ? "Loaded" : "None");
    ImGui::ColorEdit3("Color", &mr->color.x);
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
          std::string p = "assets/textures/" + m_CachedTextureFiles[i];
          std::string nm = std::filesystem::path(p).stem().string();
          auto tex = TextureManager::Get().LoadShared(nm, p);
          if (tex) {
            mr->SetTextureAsset(std::move(tex));
            Log("Texture: " + p);
          }
        }
        if (isSel)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }
    if (mr->texture) {
      ImGui::Image((void *)(intptr_t)mr->texture->GetID(), ImVec2(80, 80));
      ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::Text("ID %u", mr->texture->GetID());
      if (ImGui::SmallButton("Remove"))
        mr->ClearTextureAsset();
      ImGui::EndGroup();
    }
  }

  // ── Box Collider ───────────────────────────────────────────────────────
  auto *bc = m_SelectedEntity->GetComponent<BoxCollider>();
  if (bc && ImGui::CollapsingHeader("Collider")) {
    const char *shapeNames[] = {"Box", "Ramp"};
    int shape = static_cast<int>(bc->shape);
    if (ImGui::Combo("Shape", &shape, shapeNames, 2))
      bc->shape = static_cast<BoxCollider::Shape>(shape);
    ImGui::DragFloat3("Size", &bc->size.x, 0.05f);
    ImGui::DragFloat3("Center", &bc->center.x, 0.05f);
    ImGui::Checkbox("Is Trigger", &bc->isTrigger);
  }

  // ── Weapon ─────────────────────────────────────────────────────────────
  auto *weapon = m_SelectedEntity->GetComponent<Weapon>();
  if (weapon && ImGui::CollapsingHeader("Weapon")) {
    const char *wTypes[] = {"Rifle",   "Pistol",  "Knife",
                            "Grenade", "Shotgun", "Sniper"};
    int wt = (int)weapon->type;
    if (ImGui::Combo("Type", &wt, wTypes, 6))
      weapon->SwitchWeapon((Weapon::WeaponType)wt);
    ImGui::DragFloat("Damage", &weapon->stats.damage, 1.0f, 0.0f, 200.0f);
    ImGui::DragFloat("Fire Rate", &weapon->stats.fireRate, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat("Range", &weapon->stats.range, 1.0f, 10.0f, 500.0f);
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
    int lt = (int)lc->type;
    if (ImGui::Combo("Type", &lt, ltypes, 3))
      lc->type = (LightComponent::Type)lt;
    ImGui::ColorEdit3("Color", &lc->color.x);
    ImGui::DragFloat("Intensity", &lc->intensity, 0.05f, 0.0f, 100.0f);
    if (lc->type == LightComponent::Type::Point)
      ImGui::DragFloat("Range", &lc->range, 0.5f, 0.0f, 1000.0f);
  }

  // ── Skybox ─────────────────────────────────────────────────────────────
  auto *sky = m_SelectedEntity->GetComponent<SkyboxComponent>();
  if (sky && ImGui::CollapsingHeader("Skybox")) {
    const char *faceLabels[] = {"Right",  "Left",  "Top",
                                "Bottom", "Front", "Back"};
    for (int i = 0; i < 6; ++i) {
      char buf[256];
      strncpy(buf, sky->facePaths[i].c_str(), sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      if (ImGui::InputText(faceLabels[i], buf, sizeof(buf)))
        sky->facePaths[i] = buf;
    }
    if (ImGui::Button("Reload Skybox"))
      sky->shouldReload = true;
  }

  // ── Script Component ─────────────────────────────────────────────────────
  auto *sc = m_SelectedEntity->GetComponent<ScriptComponent>();
  if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (sc) {
      // Show + edit attached script class name
      static char scBuf[128] = "";
      if (m_SelectedEntity->GetID() != m_CachedEntityID)
        strncpy(scBuf, sc->className.c_str(), sizeof(scBuf) - 1);

      ImGui::SetNextItemWidth(-60);
      ImGui::PushStyleColor(ImGuiCol_FrameBg,
                            ImVec4(0.12f, 0.22f, 0.12f, 1.0f));
      if (ImGui::InputText("Class##SC", scBuf, sizeof(scBuf),
                           ImGuiInputTextFlags_EnterReturnsTrue))
        sc->className = scBuf;
      if (ImGui::IsItemDeactivatedAfterEdit())
        sc->className = scBuf;
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.75f, 0.22f, 0.22f, 1.0f));
      if (ImGui::SmallButton("X##RemSC"))
        m_SelectedEntity->RemoveComponent<ScriptComponent>();
      ImGui::PopStyleColor(2);
    } else {
      static char newScBuf[128] = "";
      ImGui::SetNextItemWidth(-90);
      ImGui::InputText("##NewSC", newScBuf, sizeof(newScBuf));
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.40f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.22f, 0.60f, 0.22f, 1.0f));
      if (ImGui::SmallButton("+ Script") && strlen(newScBuf) > 0) {
        auto *ns = m_SelectedEntity->AddComponent<ScriptComponent>();
        ns->className = newScBuf;
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
    if (scene) {
      scene->DestroyEntity(m_SelectedEntity->GetID());
      m_SelectedEntity = nullptr;
    }
  }
  ImGui::PopStyleColor(3);

  ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Project Panel
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawProjectPanel() {
  ImGuiWindowFlags window_flags = 0;
  if (m_ProjectPanelLocked)
    window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoCollapse;
  ImGui::Begin("Project", nullptr, window_flags);

  if (m_CurrentProjectDir != m_BaseProjectDir) {
    if (ImGui::SmallButton(".."))
      m_CurrentProjectDir = m_CurrentProjectDir.parent_path();
    ImGui::SameLine();
  }
  if (ImGui::SmallButton("Refresh"))
    m_AssetCacheDirty = true;
  ImGui::SameLine();
  ImGui::TextDisabled("%s", m_CurrentProjectDir.filename().string().c_str());

  ImGui::SameLine(ImGui::GetWindowWidth() - 35.0f);
  if (ImGui::Button("...##ProjMenu"))
    ImGui::OpenPopup("ProjConfig");
  if (ImGui::BeginPopup("ProjConfig")) {
    if (ImGui::MenuItem("Sabit (Fixed Layout)", nullptr, m_ProjectPanelLocked))
      m_ProjectPanelLocked = !m_ProjectPanelLocked;
    ImGui::EndPopup();
  }

  ImGui::Separator();

  std::error_code ec;
  std::vector<std::filesystem::path> currentFiles;
  for (auto &entry : std::filesystem::directory_iterator(m_CurrentProjectDir, ec)) {
    currentFiles.push_back(entry.path());
  }
  
  // Sort files: directories first, then alphabetical
  std::sort(currentFiles.begin(), currentFiles.end(), [](const auto& a, const auto& b) {
    bool dirA = std::filesystem::is_directory(a);
    bool dirB = std::filesystem::is_directory(b);
    if (dirA != dirB) return dirA;
    return a.filename() < b.filename();
  });

  std::filesystem::path pendingShiftFile;

  for (const auto& p : currentFiles) {
    std::string name = p.filename().string();
    bool isDir = std::filesystem::is_directory(p);
    bool isSelected = std::find(m_SelectedFiles.begin(), m_SelectedFiles.end(), p) != m_SelectedFiles.end();
    
    if (isDir) {
      if (ImGui::Selectable((name + "/").c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
        if (ImGui::IsMouseDoubleClicked(0)) {
          m_CurrentProjectDir /= p.filename();
          m_SelectedFiles.clear();
        } else {
          ImGuiIO& io = ImGui::GetIO();
          if (io.KeyCtrl) {
            auto it = std::find(m_SelectedFiles.begin(), m_SelectedFiles.end(), p);
            if (it != m_SelectedFiles.end()) m_SelectedFiles.erase(it);
            else m_SelectedFiles.push_back(p);
            m_LastClickedFile = p;
          } else if (io.KeyShift && !m_LastClickedFile.empty()) {
            pendingShiftFile = p;
          } else {
            m_SelectedFiles.clear();
            m_SelectedFiles.push_back(p);
            m_LastClickedFile = p;
          }
        }
      }
    } else {
      if (ImGui::Selectable(name.c_str(), isSelected)) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl) {
          auto it = std::find(m_SelectedFiles.begin(), m_SelectedFiles.end(), p);
          if (it != m_SelectedFiles.end()) m_SelectedFiles.erase(it);
          else m_SelectedFiles.push_back(p);
          m_LastClickedFile = p;
        } else if (io.KeyShift && !m_LastClickedFile.empty()) {
          pendingShiftFile = p;
        } else {
          m_SelectedFiles.clear();
          m_SelectedFiles.push_back(p);
          m_LastClickedFile = p;
        }
      }
    }
  }

  if (!pendingShiftFile.empty() && !m_LastClickedFile.empty()) {
    auto it1 = std::find(currentFiles.begin(), currentFiles.end(), m_LastClickedFile);
    auto it2 = std::find(currentFiles.begin(), currentFiles.end(), pendingShiftFile);
    if (it1 != currentFiles.end() && it2 != currentFiles.end()) {
      size_t idx1 = std::distance(currentFiles.begin(), it1);
      size_t idx2 = std::distance(currentFiles.begin(), it2);
      size_t startIdx = std::min(idx1, idx2);
      size_t endIdx = std::max(idx1, idx2);
      
      if (!ImGui::GetIO().KeyCtrl) m_SelectedFiles.clear();
      for (size_t i = startIdx; i <= endIdx; ++i) {
        if (std::find(m_SelectedFiles.begin(), m_SelectedFiles.end(), currentFiles[i]) == m_SelectedFiles.end()) {
          m_SelectedFiles.push_back(currentFiles[i]);
        }
      }
    }
  }

  ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Console Panel
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawConsolePanel() {
  ImGuiWindowFlags window_flags = 0;
  if (m_ConsoleLocked)
    window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoCollapse;
  ImGui::Begin("Console", nullptr, window_flags);

  ImGui::TextColored(ImVec4(0.56f, 0.78f, 1.0f, 1.0f), "Command Palette");
  ImGui::SameLine();
  ImGui::TextDisabled("%zu entries", m_ConsoleLogs.size());

  ImGui::SameLine(ImGui::GetWindowWidth() - 205.0f);
  if (ImGui::SmallButton("Commands"))
    ExecuteCommand("commands");
  ImGui::SameLine();
  if (ImGui::SmallButton("Clear"))
    ClearLogs();

  ImGui::SameLine(ImGui::GetWindowWidth() - 35.0f);
  if (ImGui::Button("...##ConsMenu"))
    ImGui::OpenPopup("ConsConfig");
  if (ImGui::BeginPopup("ConsConfig")) {
    if (ImGui::MenuItem("Sabit (Fixed Layout)", nullptr, m_ConsoleLocked))
      m_ConsoleLocked = !m_ConsoleLocked;
    ImGui::EndPopup();
  }

  ImGui::Separator();
  if (ImGui::SmallButton("render.stats"))
    ExecuteCommand("render.stats");
  ImGui::SameLine();
  if (ImGui::SmallButton("scene.list"))
    ExecuteCommand("scene.list");
  ImGui::SameLine();
  if (ImGui::SmallButton("debug.cheats 1"))
    ExecuteCommand("debug.cheats 1");
  ImGui::SameLine();
  if (ImGui::SmallButton("config.save"))
    ExecuteCommand("config.save");

  const float footerH =
      ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("##ConsoleLogs", ImVec2(0, -footerH), false,
                    ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto &log : m_ConsoleLogs) {
    // Colour-code by prefix
    if (log.rfind(">", 0) == 0)
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.56f, 0.78f, 1.0f, 1.0f));
    else if (log.rfind("[WARN]", 0) == 0)
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.80f, 0.20f, 1.0f));
    else if (log.rfind("[ERR]", 0) == 0 || log.rfind("[ERROR]", 0) == 0)
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
    else if (log.rfind("[SaveManager]", 0) == 0)
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.0f, 1.0f));
    else
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.89f, 0.94f, 1.0f));
    ImGui::TextUnformatted(log.c_str());
    ImGui::PopStyleColor();
  }

  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::TextDisabled("Use dot commands: render.stats, entity.teleport, audio.reload, profile.start");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##CmdIn", m_InputBuf, sizeof(m_InputBuf),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    ExecuteCommand(m_InputBuf);
    m_InputBuf[0] = '\0';
    ImGui::SetKeyboardFocusHere(-1);
  }
  if (m_InputBuf[0] != '\0') {
    std::string needle = m_InputBuf;
    auto aliases = DeveloperConsole::GetInstance().GetAliasNames();
    int shown = 0;
    ImGui::TextDisabled("Matches:");
    ImGui::SameLine();
    for (const auto &name : aliases) {
      if (name.find(needle) != std::string::npos) {
        if (shown++ > 0)
          ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.56f, 0.78f, 1.0f, 1.0f), "%s",
                           name.c_str());
        if (shown >= 5)
          break;
      }
    }
    if (shown == 0) {
      ImGui::SameLine();
      ImGui::TextDisabled("none");
    }
  }
  ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Performance
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawPerformanceMetrics(float deltaTime, float fps) {
  ImGui::Begin("Performance");

  // Rolling frame-time graph (90 samples, static – no alloc)
  static float frameTimes[90] = {};
  static int ftIdx = 0;
  frameTimes[ftIdx] = deltaTime * 1000.0f;
  ftIdx = (ftIdx + 1) % 90;

  ImGui::Text("FPS:       %.1f", fps);
  ImGui::Text("Frame:     %.2f ms", deltaTime * 1000.0f);
  ImGui::Separator();
  ImGui::PlotLines("##FT", frameTimes, 90, ftIdx, "ms", 0.0f, 33.0f,
                   ImVec2(-1, 60));

#ifdef ARCHURA_DEBUG
  ImGui::Separator();
  ImGui::TextUnformatted("Memory");

  auto formatBytes = [](size_t bytes) {
    static char buffers[4][32];
    static int index = 0;
    char *buffer = buffers[index++ % 4];
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    if (bytes >= static_cast<size_t>(mb))
      snprintf(buffer, 32, "%.2f MB", static_cast<double>(bytes) / mb);
    else if (bytes >= static_cast<size_t>(kb))
      snprintf(buffer, 32, "%.1f KB", static_cast<double>(bytes) / kb);
    else
      snprintf(buffer, 32, "%zu B", bytes);
    return buffer;
  };

  const auto stats = Memory::MemoryTracker::Snapshot();
  if (stats.empty()) {
    ImGui::TextDisabled("No tracked allocators yet.");
  } else if (ImGui::BeginTable("##MemoryStats", 6,
                               ImGuiTableFlags_Borders |
                                   ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Allocator");
    ImGui::TableSetupColumn("Used");
    ImGui::TableSetupColumn("Peak");
    ImGui::TableSetupColumn("Cap");
    ImGui::TableSetupColumn("Allocs");
    ImGui::TableSetupColumn("Frag");
    ImGui::TableHeadersRow();

    for (const auto &item : stats) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(item.name.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(formatBytes(item.currentBytes));
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(formatBytes(item.peakBytes));
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(formatBytes(item.capacityBytes));
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%zu", item.allocationCount);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%.0f%%", item.fragmentation * 100.0f);
    }
    ImGui::EndTable();
  }
#else
  ImGui::Separator();
  ImGui::TextDisabled("Memory tracking is compiled out.");
#endif

  ImGui::End();
}

void Editor::DrawDemoWindow() { ImGui::ShowDemoWindow(&m_ShowDemoWindow); }

// ─────────────────────────────────────────────────────────────────────────────
// Developer Tutorial Panel
// ─────────────────────────────────────────────────────────────────────────────

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
      Row("Ctrl + Z", "Undo last transform");
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
      Section("Box Collider",
              "Defines the AABB used for physics and mouse picking. "
              "'Is Trigger' objects generate events but don't block movement.");
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

// ─────────────────────────────────────────────────────────────────────────────
// Console command dispatch
// ─────────────────────────────────────────────────────────────────────────────

void Editor::ExecuteCommand(const char *command) {
  if (!command || command[0] == '\0')
    return;
  Log(std::string("> ") + command);
  DeveloperConsole::GetInstance().ExecuteCommand(std::string(command));
}

// ─────────────────────────────────────────────────────────────────────────────
// Entity spawning
// ─────────────────────────────────────────────────────────────────────────────

void Editor::SpawnEntity(Scene *scene, const std::string &type,
                         const std::string &path) {
  if (!scene)
    return;

  static int counter = 0;
  std::string name = type + "_" + std::to_string(++counter);
  Entity *e = scene->CreateEntity(name);

  auto *tf = e->GetComponent<Transform>();
  tf->position = m_SpawnPosition;

  auto *mr = e->AddComponent<MeshRenderer>();
  mr->color = glm::vec3(1.0f);
  auto *col = e->AddComponent<BoxCollider>();
  col->size = glm::vec3(1.0f);

  if (type == "Cube")
    mr->SetMeshAsset(Mesh::CreateCubeShared());
  else if (type == "Sphere") {
    mr->SetMeshAsset(Mesh::CreateSphereShared(0.5f));
  } else if (type == "Capsule") {
    mr->SetMeshAsset(Mesh::CreateCapsuleShared(0.5f, 2.0f));
    col->size = {1, 2, 1};
  } else if (type == "Stairs") {
    mr->SetMeshAsset(Mesh::CreateStairsShared(1, 1, 2, 5));
    col->size = {1, 1, 2};
  } else if (type == "Ramp") {
    mr->SetMeshAsset(Mesh::CreateRampShared(1, 1, 2));
    col->size = {1, 1, 2};
    col->center = {0, 0.5f, 0};
    col->shape = BoxCollider::Shape::Ramp;
  } else if (type == "Light") {
    e->RemoveComponent<MeshRenderer>();
    e->AddComponent<LightComponent>();
    col->isTrigger = true;
    col->size = glm::vec3(0.5f);
  } else if (type == "Model" && !path.empty()) {
    std::string ext = std::filesystem::path(path).extension().string();
    for (auto &c : ext)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".obj")
      mr->SetMeshAsset(Mesh::LoadFromOBJShared(path));
    else if (ext == ".fbx")
      mr->SetMeshAsset(Mesh::LoadFromFBXShared(path));
    else {
      Log("[WARN] Unsupported model format: " + ext);
      mr->SetMeshAsset(Mesh::CreateSphereShared(0.5f));
      mr->color = glm::vec3(1.0f, 0.0f, 1.0f);
    }
    if (name.find("Model_") == 0)
      e->SetName(std::filesystem::path(path).stem().string());
  }

  Log("Spawned " + type);
  m_SelectedEntity = e;
  m_AssetCacheDirty = false; // spawn doesn't invalidate asset cache
}

void Editor::DrawModelDirectory(Scene *scene,
                                const std::filesystem::path &dir) {
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
    if (entry.is_directory(ec)) {
      if (ImGui::BeginMenu(entry.path().filename().string().c_str())) {
        DrawModelDirectory(scene, entry.path());
        ImGui::EndMenu();
      }
    } else {
      std::string ext = entry.path().extension().string();
      for (auto &c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (ext == ".obj" || ext == ".fbx" || ext == ".glb" || ext == ".gltf") {
        if (ImGui::MenuItem(entry.path().filename().string().c_str()))
          SpawnEntity(scene, "Model", entry.path().string());
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse Picking  ─ Ray-AABB intersection, returns best-hit Entity
// ─────────────────────────────────────────────────────────────────────────────

Entity *Editor::PickEntityAtScreenPos(Scene *scene, Input *input, int screenW,
                                      int screenH) {
  if (!scene || !input)
    return nullptr;
  if (screenW <= 0 || screenH <= 0)
    return nullptr;

  // Only fire on the frame the LMB is pressed (not while held)
  if (!input->IsMouseButtonPressed(SDL_BUTTON_LEFT))
    return nullptr;

  // If any ImGui window wants the mouse (e.g. panel clicked), skip
  if (ImGui::GetIO().WantCaptureMouse)
    return nullptr;

  // ── Build ray from mouse position in NDC ──────────────────────────────────
  glm::vec2 mp = input->GetMousePosition();
  // Convert to NDC [-1, 1]
  float ndcX = (2.0f * mp.x) / screenW - 1.0f;
  float ndcY = 1.0f - (2.0f * mp.y) / screenH;

  const float aspect = (screenH > 0) ? (float)screenW / (float)screenH : 1.777f;
  glm::mat4 proj = m_EditorCamera.GetProjectionMatrix(aspect);
  glm::mat4 view = m_EditorCamera.GetViewMatrix();
  glm::mat4 invVP = glm::inverse(proj * view);

  // Unproject near/far clip points
  glm::vec4 nearP = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
  glm::vec4 farP = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
  nearP /= nearP.w;
  farP /= farP.w;

  glm::vec3 rayOrigin = glm::vec3(nearP);
  glm::vec3 rayDir = glm::normalize(glm::vec3(farP) - rayOrigin);

  // ── Test every entity's AABB (world-space)
  // ──────────────────────────────────
  Entity *bestEntity = nullptr;
  float bestT = std::numeric_limits<float>::max();

  auto rayAABB = [](const glm::vec3 &ro, const glm::vec3 &rd,
                    const glm::vec3 &bmin, const glm::vec3 &bmax,
                    float &tOut) -> bool {
    glm::vec3 invD(1.0f / rd.x, 1.0f / rd.y, 1.0f / rd.z);
    glm::vec3 t0 = (bmin - ro) * invD;
    glm::vec3 t1 = (bmax - ro) * invD;
    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);
    float tNear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tFar = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
    if (tFar < 0.0f || tNear > tFar)
      return false;
    tOut = (tNear > 0.0f) ? tNear : tFar;
    return true;
  };

  for (const auto &ePtr : scene->GetEntities()) {
    Entity *ent = ePtr.get();
    auto *tf = ent->GetComponent<Transform>();
    auto *bc = ent->GetComponent<BoxCollider>();
    auto *mr = ent->GetComponent<MeshRenderer>();

    // Skip entities with no spatial presence (no transform placed yet)
    if (!tf)
      continue;

    // Build AABB from BoxCollider size, or fall back to 1x1x1 if mesh present
    glm::vec3 halfSize(0.5f);
    if (bc) {
      halfSize = bc->size * 0.5f;
    } else if (mr) {
      halfSize = glm::vec3(0.5f) * tf->scale;
    } else {
      continue; // No renderable – skip
    }

    // Transform AABB by scale + position (ignores rotation, but fast)
    halfSize *= tf->scale;
    glm::vec3 worldCenter =
        tf->position + (bc ? bc->center * tf->scale : glm::vec3(0.0f));
    glm::vec3 bmin = worldCenter - halfSize;
    glm::vec3 bmax = worldCenter + halfSize;

    float t = 0.0f;
    if (rayAABB(rayOrigin, rayDir, bmin, bmax, t) && t < bestT) {
      bestT = t;
      bestEntity = ent;
    }
  }

  return bestEntity;
}

// ─────────────────────────────────────────────────────────────────────────────
// Transform Gizmo  ─ X/Y/Z axis arrows drawn on top of the scene
// ─────────────────────────────────────────────────────────────────────────────

void Editor::DrawTransformGizmo(int screenW, int screenH) {
  // Reset cache
  m_GizmoSSValid = false;
  for (int i = 0; i < 3; ++i)
    m_GizmoRotArcSSValid[i] = false;

  if (!m_SelectedEntity)
    return;
  auto *tf = m_SelectedEntity->GetComponent<Transform>();
  if (!tf)
    return;

  const float asp = (screenH > 0) ? (float)screenW / (float)screenH : 1.777f;
  glm::mat4 vp =
      m_EditorCamera.GetProjectionMatrix(asp) * m_EditorCamera.GetViewMatrix();

  glm::vec3 origin = tf->position;
  float dist = glm::length(m_EditorCamera.GetPosition() - origin);
  float arrowLen = glm::clamp(dist * 0.12f, 0.5f, 8.0f);
  float rotR = arrowLen * 0.75f;

  // Project world → screen into glm::vec2 (for cache)
  auto proj2ss = [&](const glm::vec3 &wp, glm::vec2 &out) -> bool {
    glm::vec4 c = vp * glm::vec4(wp, 1.0f);
    if (c.w <= 0.001f)
      return false;
    glm::vec3 ndc = glm::vec3(c) / c.w;
    if (std::abs(ndc.x) > 1.5f || std::abs(ndc.y) > 1.5f)
      return false;
    out = {(ndc.x + 1.0f) * 0.5f * screenW, (1.0f - ndc.y) * 0.5f * screenH};
    return true;
  };
  auto toImVec = [](const glm::vec2 &v) { return ImVec2(v.x, v.y); };

  glm::vec2 oss;
  if (!proj2ss(origin, oss))
    return;
  m_GizmoOriginSS = oss;
  m_GizmoSSValid = true;

  // Translation tips (world)
  glm::vec3 tipW[3] = {
      origin + glm::vec3(arrowLen, 0, 0),
      origin + glm::vec3(0, arrowLen, 0),
      origin + glm::vec3(0, 0, arrowLen),
  };

  ImU32 tNorm[3] = {IM_COL32(230, 60, 60, 255), IM_COL32(60, 220, 60, 255),
                    IM_COL32(80, 110, 240, 255)};
  ImU32 rNorm[3] = {IM_COL32(200, 60, 60, 160), IM_COL32(60, 200, 60, 160),
                    IM_COL32(80, 90, 220, 160)};
  ImU32 hotT = IM_COL32(255, 235, 80, 255);
  ImU32 hotR = IM_COL32(255, 235, 80, 200);
  const char *lbl[3] = {"X", "Y", "Z"};

  ImDrawList *dl = ImGui::GetForegroundDrawList();

  // ── Translation arrows ────────────────────────────────────────────────────
  for (int i = 0; i < 3; ++i) {
    glm::vec2 tss;
    if (!proj2ss(tipW[i], tss))
      continue;
    m_GizmoTransTipSS[i] = tss;
    glm::vec2 dir = tss - oss;
    float len = glm::length(dir);
    if (len > 0.001f)
      m_GizmoAxisDir2D[i] = dir / len;

    bool hot = (m_GizmoActive == (GizmoOp)((int)GizmoOp::TransX + i) ||
                m_GizmoHovered == (GizmoOp)((int)GizmoOp::TransX + i));
    ImU32 col = hot ? hotT : tNorm[i];
    float lw = hot ? 4.5f : 3.0f;

    dl->AddLine(toImVec(oss), toImVec(tss), col, lw);
    if (len > 0.001f) {
      glm::vec2 nd = dir / len, perp(-nd.y, nd.x);
      const float R = 5.5f;
      dl->AddTriangleFilled(
          ImVec2(tss.x + nd.x * R * 1.5f, tss.y + nd.y * R * 1.5f),
          ImVec2(tss.x - perp.x * R, tss.y - perp.y * R),
          ImVec2(tss.x + perp.x * R, tss.y + perp.y * R), col);
    }
    float sx = tss.x > oss.x ? 1.f : -1.f;
    float sy = tss.y > oss.y ? 1.f : -1.f;
    dl->AddText(ImVec2(tss.x + 10.f * sx - 3.f, tss.y + 10.f * sy - 6.f), col,
                lbl[i]);
  }

  // ── Rotation circles ──────────────────────────────────────────────────────
  static constexpr float TWO_PI = 6.28318530f;
  for (int ax = 0; ax < 3; ++ax) {
    bool hot = (m_GizmoActive == (GizmoOp)((int)GizmoOp::RotX + ax) ||
                m_GizmoHovered == (GizmoOp)((int)GizmoOp::RotX + ax));
    ImU32 col = hot ? hotR : rNorm[ax];
    float lw = hot ? 2.8f : 1.8f;

    bool anyOk = false;
    glm::vec2 prev;
    bool prevOk = false;
    for (int s = 0; s <= ROT_ARC_SEGS; ++s) {
      float t = (float)s / (float)ROT_ARC_SEGS * TWO_PI;
      float ct = std::cos(t), st = std::sin(t);
      glm::vec3 wp;
      if (ax == 0)
        wp = origin + glm::vec3(0, rotR * ct, rotR * st);
      else if (ax == 1)
        wp = origin + glm::vec3(rotR * ct, 0, rotR * st);
      else
        wp = origin + glm::vec3(rotR * ct, rotR * st, 0);
      glm::vec2 ss;
      bool ok = proj2ss(wp, ss);
      if (s < ROT_ARC_SEGS) {
        m_GizmoRotArcSS[ax][s] = ss;
        if (ok)
          anyOk = true;
      }
      if (ok && prevOk)
        dl->AddLine(toImVec(prev), toImVec(ss), col, lw);
      prev = ss;
      prevOk = ok;
    }
    m_GizmoRotArcSSValid[ax] = anyOk;
  }

  // Center dot
  dl->AddCircleFilled(toImVec(oss), 4.5f, IM_COL32(255, 255, 255, 210));
  dl->AddCircle(toImVec(oss), 4.5f, IM_COL32(0, 0, 0, 160), 12, 1.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
// ProcessGizmoInput  ─ hover + LMB drag for translate / rotate
// ─────────────────────────────────────────────────────────────────────────────

void Editor::ProcessGizmoInput(Input *input, int screenW, int screenH) {
  m_GizmoHovered = GizmoOp::None;
  if (!m_SelectedEntity || !input || !m_GizmoSSValid) {
    m_GizmoActive = GizmoOp::None;
    return;
  }
  auto *tf = m_SelectedEntity->GetComponent<Transform>();
  if (!tf) {
    m_GizmoActive = GizmoOp::None;
    return;
  }

  glm::vec2 mp = input->GetMousePosition();
  bool lmbDown = input->IsMouseButtonDown(SDL_BUTTON_LEFT);
  bool lmbPressed = input->IsMouseButtonPressed(SDL_BUTTON_LEFT);

  auto distSeg = [](const glm::vec2 &p, const glm::vec2 &a,
                    const glm::vec2 &b) -> float {
    glm::vec2 ab = b - a, ap = p - a;
    float t =
        glm::clamp(glm::dot(ap, ab) / (glm::dot(ab, ab) + 1e-6f), 0.0f, 1.0f);
    return glm::length(p - (a + t * ab));
  };

  // Hover detection
  if (m_GizmoActive == GizmoOp::None) {
    for (int i = 0; i < 3; ++i) {
      if (distSeg(mp, m_GizmoOriginSS, m_GizmoTransTipSS[i]) < 10.0f) {
        m_GizmoHovered = (GizmoOp)((int)GizmoOp::TransX + i);
        break;
      }
    }
    if (m_GizmoHovered == GizmoOp::None) {
      for (int axis = 0; axis < 3; ++axis) {
        if (!m_GizmoRotArcSSValid[axis])
          continue;
        float minD = 1e9f;
        for (int s = 0; s < ROT_ARC_SEGS; ++s)
          minD = std::min(
              minD, distSeg(mp, m_GizmoRotArcSS[axis][s],
                            m_GizmoRotArcSS[axis][(s + 1) % ROT_ARC_SEGS]));
        if (minD < 9.0f) {
          m_GizmoHovered = (GizmoOp)((int)GizmoOp::RotX + axis);
          break;
        }
      }
    }
  }

  // Begin drag
  if (lmbPressed && !ImGui::GetIO().WantCaptureMouse &&
      m_GizmoHovered != GizmoOp::None) {
    PushUndoSnapshot(m_SelectedEntity); // ← save before-state for Ctrl+Z
    m_GizmoActive = m_GizmoHovered;
    m_GizmoDragMouseStart = mp;
    m_GizmoDragMousePrev = mp;
    m_GizmoDragPosStart = tf->position;
    float d = glm::length(m_EditorCamera.GetPosition() - tf->position);
    m_GizmoDragScale =
        (2.0f * d * std::tan(glm::radians(m_EditorCamera.GetFOV()) * 0.5f)) /
        (float)screenH;
  }

  // Active drag
  if (m_GizmoActive != GizmoOp::None && lmbDown) {
    int opIdx = (int)m_GizmoActive;
    if (opIdx >= (int)GizmoOp::TransX && opIdx <= (int)GizmoOp::TransZ) {
      int axis = opIdx - (int)GizmoOp::TransX;
      float proj2d =
          glm::dot(mp - m_GizmoDragMouseStart, m_GizmoAxisDir2D[axis]);
      glm::vec3 pos = m_GizmoDragPosStart;
      pos[axis] += proj2d * m_GizmoDragScale;
      tf->position = pos;
    } else {
      int axis = opIdx - (int)GizmoOp::RotX;
      glm::vec2 delta = mp - m_GizmoDragMousePrev;
      m_GizmoDragMousePrev = mp;
      float angleDeg = 0.0f;
      if (axis == 0)
        angleDeg = -delta.y * 0.35f; // X: vertical drag
      else if (axis == 1)
        angleDeg = delta.x * 0.35f; // Y: horizontal drag
      else
        angleDeg = -delta.x * 0.35f; // Z: horizontal drag
      tf->rotation[axis] += angleDeg;
    }
  }

  if (!lmbDown)
    m_GizmoActive = GizmoOp::None;
}

// ─────────────────────────────────────────────────────────────────────────────
// Undo / Copy / Paste / Shortcuts
// ─────────────────────────────────────────────────────────────────────────────

void Editor::PushUndoSnapshot(Entity *entity) {
  if (!entity)
    return;
  auto *tf = entity->GetComponent<Transform>();
  if (!tf)
    return;
  if ((int)m_UndoStack.size() >= UNDO_LIMIT)
    m_UndoStack.pop_front();
  UndoSnapshot snap;
  snap.entity = entity;
  snap.id = entity->GetID();
  snap.pos = tf->position;
  snap.rot = tf->rotation;
  snap.scale = tf->scale;
  m_UndoStack.push_back(snap);
}

void Editor::UndoLastAction(Scene * /*scene*/) {
  if (m_UndoStack.empty()) {
    Log("[Undo] Yok.");
    return;
  }
  auto &snap = m_UndoStack.back();
  if (snap.entity) {
    auto *tf = snap.entity->GetComponent<Transform>();
    if (tf) {
      tf->position = snap.pos;
      tf->rotation = snap.rot;
      tf->scale = snap.scale;
      m_SelectedEntity = snap.entity; // re-select
      Log("[Undo] Geri alindi: " + snap.entity->GetName());
    }
  }
  m_UndoStack.pop_back();
}

void Editor::CopySelected() {
  if (!m_SelectedEntity)
    return;
  auto *tf = m_SelectedEntity->GetComponent<Transform>();
  auto *mr = m_SelectedEntity->GetComponent<MeshRenderer>();
  auto *bc = m_SelectedEntity->GetComponent<BoxCollider>();
  auto *sc = m_SelectedEntity->GetComponent<ScriptComponent>();

  m_Clipboard = {}; // reset
  m_Clipboard.valid = true;
  m_Clipboard.name = m_SelectedEntity->GetName() + "_Kopya";
  if (tf) {
    m_Clipboard.pos = tf->position;
    m_Clipboard.rot = tf->rotation;
    m_Clipboard.scale = tf->scale;
  }
  if (mr) {
    m_Clipboard.mesh = mr->meshAsset;
    m_Clipboard.texture = mr->textureAsset;
    m_Clipboard.color = mr->color;
  }
  if (bc) {
    m_Clipboard.hasBc = true;
    m_Clipboard.bcSize = bc->size;
    m_Clipboard.bcCenter = bc->center;
    m_Clipboard.bcTrigger = bc->isTrigger;
    m_Clipboard.bcShape = static_cast<int>(bc->shape);
  }
  if (sc) {
    m_Clipboard.hasScript = true;
    m_Clipboard.scriptClass = sc->className;
  }
  Log("[Kopyala] " + m_SelectedEntity->GetName());
}

void Editor::PasteClipboard(Scene *scene) {
  if (!scene || !m_Clipboard.valid)
    return;
  Entity *e = scene->CreateEntity(m_Clipboard.name);
  auto *tf = e->GetComponent<Transform>();
  if (tf) {
    tf->position =
        m_Clipboard.pos + glm::vec3(1.0f, 0.0f, 1.0f); // slight offset
    tf->rotation = m_Clipboard.rot;
    tf->scale = m_Clipboard.scale;
  }
  if (m_Clipboard.mesh) {
    auto *mr = e->AddComponent<MeshRenderer>();
    mr->SetMeshAsset(m_Clipboard.mesh);
    mr->SetTextureAsset(m_Clipboard.texture);
    mr->color = m_Clipboard.color;
  }
  if (m_Clipboard.hasBc) {
    auto *bc = e->AddComponent<BoxCollider>();
    bc->size = m_Clipboard.bcSize;
    bc->center = m_Clipboard.bcCenter;
    bc->isTrigger = m_Clipboard.bcTrigger;
    bc->shape = static_cast<BoxCollider::Shape>(m_Clipboard.bcShape);
  }
  if (m_Clipboard.hasScript) {
    auto *ns = e->AddComponent<ScriptComponent>();
    ns->className = m_Clipboard.scriptClass;
  }
  m_SelectedEntity = e;
  Log("[Yapistir] " + m_Clipboard.name);
}

void Editor::ProcessEditorShortcuts(Input *input, Scene *scene) {
  if (!input)
    return;
  if (ImGui::GetIO().WantTextInput)
    return; // typing in an ImGui field – don't steal keys

  const bool ctrl = input->IsKeyDown(SDL_SCANCODE_LCTRL) ||
                    input->IsKeyDown(SDL_SCANCODE_RCTRL);

  // Ctrl+Z – Undo
  if (ctrl && input->IsKeyJustPressed(SDL_SCANCODE_Z))
    UndoLastAction(scene);

  // Ctrl+C – Copy
  if (ctrl && input->IsKeyJustPressed(SDL_SCANCODE_C))
    CopySelected();

  // Ctrl+V – Paste
  if (ctrl && input->IsKeyJustPressed(SDL_SCANCODE_V))
    PasteClipboard(scene);

  // Ctrl+D – Duplicate (copy + paste immediately)
  if (ctrl && input->IsKeyJustPressed(SDL_SCANCODE_D)) {
    CopySelected();
    PasteClipboard(scene);
  }

  // Ctrl+A – Deselect All
  if (ctrl && input->IsKeyJustPressed(SDL_SCANCODE_A)) {
    m_SelectedEntity = nullptr;
    m_SelectedEntities.clear();
    m_LastClickedEntity = nullptr;
  }

  // Delete key – Delete selected entities
  if (!ctrl && input->IsKeyJustPressed(SDL_SCANCODE_DELETE)) {
    if (!m_SelectedEntities.empty() && scene) {
      for (auto *e : m_SelectedEntities) {
        scene->DestroyEntity(e->GetID());
      }
      m_SelectedEntities.clear();
      m_SelectedEntity = nullptr;
      m_LastClickedEntity = nullptr;
      Log("[Sil] Secili varliklar silindi.");
    } else if (m_SelectedEntity && scene) {
      scene->DestroyEntity(m_SelectedEntity->GetID());
      m_SelectedEntity = nullptr;
      m_LastClickedEntity = nullptr;
      Log("[Sil] Varlik silindi.");
    }
  }

  // F – Focus / Frame selected entity (like Unity)
  if (!ctrl && input->IsKeyJustPressed(SDL_SCANCODE_F) && m_SelectedEntity) {
    auto *tf = m_SelectedEntity->GetComponent<Transform>();
    if (tf) {
      float dist = glm::length(m_EditorCamera.GetPosition() - tf->position);
      float pullback = glm::clamp(dist * 0.5f, 5.0f, 40.0f);
      // Move camera toward the entity along its current forward direction
      glm::vec3 dir =
          glm::normalize(m_EditorCamera.GetPosition() - tf->position);
      m_EditorCamera.SetPosition(tf->position + dir * pullback);
      Log("[Focus] " + m_SelectedEntity->GetName());
    }
  }
}

} // namespace Archura
