#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#if defined(near)
#undef near
#endif
#if defined(far)
#undef far
#endif

#include <deque>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

struct SDL_Window;

#include "EditorCamera.h"
#include "ObjectManipulationTool.h"
#include "GameBuilderPanel.h"

namespace Archura {

class Scene;
class Entity;
class Window;
class Camera;
class Input;
class Mesh;
class Texture;

/**
 * @brief Edit / Play mode discriminator.
 * Edit  → editor camera active, cursor free, Game systems paused.
 * Play  → FPS controller active, cursor locked, full simulation.
 */
enum class EditorMode { Edit, Play };

/**
 * @brief ImGui Editor – main editor coordinator.
 *
 * Owns the EditorCamera and drives the Edit/Play state machine.
 * All per-frame draw functions are allocation-free on the hot path.
 */
class Editor {
public:
    Editor();
    ~Editor();

    bool Init(Window* window);
    void Shutdown();

    void BeginDockSpace();
    void DrawEditorUI(Scene* scene);
    void Update(Scene* scene, float deltaTime, float fps);
    void DrawMenuBar(Scene* scene);
    void DrawOverlay(Scene* scene, Camera* gameCamera);

    // ── Mouse Picking ────────────────────────────────────────────────────────
    /** Call every frame in Edit mode BEFORE ImGui consumes mouse clicks.
     *  Returns the picked entity (or nullptr on miss / ImGui-over). */
    Entity* PickEntityAtScreenPos(Scene* scene, Input* input, int screenW, int screenH);

    // ── 3-D Transform Gizmo ──────────────────────────────────────────────────
    /** Draws translation arrows + rotation circles.  Caches screen positions for
     *  hit-testing.  Call after ImGui::NewFrame(), before EndFrame(). */
    void DrawTransformGizmo(int screenW, int screenH);

    /** Process LMB drag on gizmo handles.  Call in ProcessInput(), before picking. */
    void ProcessGizmoInput(Input* input, int screenW, int screenH);

    /** True while a gizmo handle is hovered or being dragged (suppresses entity picking). */
    bool IsGizmoInteracting() const { return m_GizmoActive != GizmoOp::None || m_GizmoHovered != GizmoOp::None; }

    // ── Editor Keyboard Shortcuts (Ctrl+Z/C/V/D, F) ─────────────────────────
    /** Call every frame in Edit mode from ProcessInput (after gizmo). */
    void ProcessEditorShortcuts(Input* input, Scene* scene);

    /** Snapshot current transform – call before any destructive edit so Ctrl+Z can restore. */
    void PushUndoSnapshot(Entity* entity);

    // ── Visibility ──────────────────────────────────────────────────────────
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const        { return m_Enabled; }

    // ── State Machine ────────────────────────────────────────────────────────
    EditorMode  GetMode()    const { return m_Mode; }
    bool IsGameRunning()     const { return m_Mode == EditorMode::Play; }

    /** Called when toolbar Play button is clicked. */
    void RegisterOnPlay(std::function<void()> cb)  { m_OnPlay  = std::move(cb); }
    /** Called when toolbar Stop button is clicked. */
    void RegisterOnStop(std::function<void()> cb)  { m_OnStop  = std::move(cb); }

    // ── Editor Camera ────────────────────────────────────────────────────────
    EditorCamera* GetEditorCamera() { return &m_EditorCamera; }

    // ── ImGui capture query ──────────────────────────────────────────────────
    bool WantCaptureMouse() const;

    // ── Entity selection ─────────────────────────────────────────────────────
    void SetSelectedEntity(Entity* entity) { 
        m_SelectedEntity = entity; 
        m_SelectedEntities.clear();
        if (entity) m_SelectedEntities.push_back(entity);
    }
    void SetLookedAtEntity(Entity* entity) { m_LookedAtEntity = entity; }
    void SetSpawnPosition(const glm::vec3& pos) { m_SpawnPosition = pos; }

    // ── Console logging ──────────────────────────────────────────────────────
    void Log(const std::string& msg) { m_ConsoleLogs.push_back(msg); }
    void ClearLogs()                 { m_ConsoleLogs.clear(); }
    void ExecuteCommand(const char* command);

public: // ── Shared State (kept public for minimal refactor surface) ──────────
    Entity*   m_SelectedEntity = nullptr;
    std::vector<Entity*> m_SelectedEntities;
    Entity*   m_LastClickedEntity = nullptr;
    Entity*   m_LookedAtEntity = nullptr;
    glm::vec3 m_SpawnPosition  = glm::vec3(0.0f);
    Window*   m_Window         = nullptr;

    std::vector<std::string> m_ConsoleLogs;
    char     m_InputBuf[256]   = "";
    uint32_t m_CachedEntityID  = 0;

    // Project browser
    std::filesystem::path m_BaseProjectDir;
    std::filesystem::path m_CurrentProjectDir;
    std::vector<std::filesystem::path> m_SelectedFiles;
    std::filesystem::path m_LastClickedFile;

    // Stream redirect
    std::streambuf*                m_OldCoutBuf = nullptr;
    std::unique_ptr<std::streambuf> m_NewCoutBuf;

private:
    // ── Layout / Theme ───────────────────────────────────────────────────────
    void ApplyDarkTheme();

    // ── Panel Draw ───────────────────────────────────────────────────────────
    void DrawToolbar(Scene* scene);
    void DrawSceneHierarchy(Scene* scene);
    void DrawInspector(Scene* scene);
    void DrawProjectPanel();
    void DrawConsolePanel();
    void DrawPerformanceMetrics(float deltaTime, float fps);
    void DrawDemoWindow();
    void DrawTutorialPanel();
    void SpawnEntity(Scene* scene, const std::string& type, const std::string& path = "");
    void DrawModelDirectory(Scene* scene, const std::filesystem::path& directory);

    // ── Asset cache helpers ──────────────────────────────────────────────────
    void RefreshAssetCache();

private:
    bool       m_Enabled = true;
    EditorMode m_Mode    = EditorMode::Edit;

    std::function<void()> m_OnPlay;
    std::function<void()> m_OnStop;

    EditorCamera         m_EditorCamera;
    ObjectManipulationTool m_ObjectTool;
    GameBuilderPanel     m_GameBuilderPanel;

    // ── Panel visibility flags ───────────────────────────────────────────────
    bool m_ShowSceneHierarchy = true;
    bool m_ShowInspector      = true;
    bool m_ShowProjectPanel   = true;
    bool m_ShowConsole        = true;
    bool m_ShowPerformance    = false;
    bool m_ShowDemoWindow     = false;
    bool m_ShowGameBuilder    = false;
    bool m_ShowTutorial       = false;

    // ── Panel locking states (fixed/Unity-style layout) ──────────────────────
    bool m_HierarchyLocked    = true;
    bool m_InspectorLocked    = true;
    bool m_ProjectPanelLocked = true;
    bool m_ConsoleLocked      = true;
    bool m_PerformanceLocked  = true;

    // ── Asset cache (dirty-flagged, never scanned per frame) ─────────────────
    std::vector<std::string> m_CachedTextureFiles;
    std::vector<std::string> m_CachedModelFiles;
    bool m_AssetCacheDirty = true;

    // ── Interactive Transform Gizmo ──────────────────────────────────────────
    enum class GizmoOp { None, TransX, TransY, TransZ, RotX, RotY, RotZ };

    GizmoOp   m_GizmoActive  = GizmoOp::None;
    GizmoOp   m_GizmoHovered = GizmoOp::None;

    // Screen-space cache filled by DrawTransformGizmo, read next frame in ProcessGizmoInput
    glm::vec2  m_GizmoOriginSS      = {};
    glm::vec2  m_GizmoTransTipSS[3] = {};
    glm::vec2  m_GizmoAxisDir2D[3]  = {};
    bool       m_GizmoSSValid       = false;

    static constexpr int ROT_ARC_SEGS = 32;
    glm::vec2  m_GizmoRotArcSS[3][ROT_ARC_SEGS];
    bool       m_GizmoRotArcSSValid[3] = {};

    // Per-drag state
    glm::vec2  m_GizmoDragMouseStart = {};
    glm::vec3  m_GizmoDragPosStart   = {};
    glm::vec2  m_GizmoDragMousePrev  = {};
    float      m_GizmoDragScale      = 1.0f;

    // ── Undo System ────────────────────────────────────────────────────────
    struct UndoSnapshot {
        Entity*   entity = nullptr; // raw ptr – valid while scene lives
        uint32_t  id     = 0;
        glm::vec3 pos    = {};
        glm::vec3 rot    = {};
        glm::vec3 scale  = {1,1,1};
    };
    static constexpr int UNDO_LIMIT = 50;
    std::deque<UndoSnapshot> m_UndoStack;

    void UndoLastAction(Scene* scene);

    // ── Clipboard (Copy / Paste) ───────────────────────────────────────────────
    struct EntityClipboard {
        bool        valid     = false;
        std::string name;
        glm::vec3   pos       = {}, rot = {}, scale = {1,1,1};
        glm::vec3   color     = {1,1,1};
        Mesh*       mesh      = nullptr;
        Texture*    texture   = nullptr;
        bool        hasBc     = false;
        glm::vec3   bcSize    = {1,1,1}, bcCenter = {};
        bool        bcTrigger = false;
        bool        hasScript = false;
        std::string scriptClass;
    };
    EntityClipboard m_Clipboard;

    void CopySelected();
    void PasteClipboard(Scene* scene);
};

// ── cout redirect streambuf ──────────────────────────────────────────────────
class EditorStreamBuf : public std::streambuf {
public:
    explicit EditorStreamBuf(Editor* editor) : m_Editor(editor) {}

protected:
    int_type overflow(int_type c) override {
        if (c != EOF) {
            char ch = static_cast<char>(c);
            if (ch == '\n') {
                m_Editor->Log(m_Buffer);
                m_Buffer.clear();
            } else {
                m_Buffer += ch;
            }
        }
        return c;
    }

private:
    Editor*     m_Editor;
    std::string m_Buffer;
};

} // namespace Archura
