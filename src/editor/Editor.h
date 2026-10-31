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
#include <imgui.h>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

struct SDL_Window;

#include "EditorCamera.h"
#include "EditorModeState.h"
#include "EditorSelectionState.h"
#include "ObjectManipulationTool.h"
#include "GameBuilderPanel.h"
#include "TransformUndoHistory.h"
#include "ProjectDirectoryCache.h"
#include "InspectorTextState.h"
#include "SceneDocumentState.h"
#include <chrono>

namespace Archura {

class Scene;
class Entity;
class Window;
class Camera;
class Input;
class Mesh;
class Texture;

/**
 * @brief ImGui Editor – main editor coordinator.
 *
 * Owns the EditorCamera and drives the Edit/Play state machine.
 */
class Editor {
    friend class EditorDocumentSmoke;
public:
    Editor();
    ~Editor();

    bool Init(Window* window);
    void Shutdown();

    void BeginDockSpace();
    void CaptureSceneFrame();
    float GetSceneAspect(float fallback) const;
    bool IsSceneHovered() const { return m_SceneHovered; }
    bool CanStartSceneInteraction(glm::vec2 mouse) const;
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
    bool IsGizmoInteracting() const { return m_GizmoConsumedClick || m_GizmoActive != GizmoOp::None || m_GizmoHovered != GizmoOp::None; }

    // ── Editor Keyboard Shortcuts (Ctrl+Z/C/V/D, F) ─────────────────────────
    /** Call every frame in Edit mode from ProcessInput (after gizmo). */
    void ProcessEditorShortcuts(Input* input, Scene* scene);

    /** Snapshot current transform – call before any destructive edit so Ctrl+Z can restore. */
    void PushUndoSnapshot(Entity* entity);

    // ── Visibility ──────────────────────────────────────────────────────────
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const        { return m_Enabled; }

    // ── State Machine ────────────────────────────────────────────────────────
    EditorMode  GetMode()    const { return m_ModeState.GetMode(); }
    bool IsGameRunning()     const { return GetMode() == EditorMode::Play; }
    bool SetMode(EditorMode mode);

    /** Called when toolbar Play button is clicked. */
    void RegisterOnPlay(std::function<void()> cb) {
        m_OnPlay = [cb = std::move(cb)] { if (!cb) return false; cb(); return true; };
    }
    /** Called when toolbar Stop button is clicked. */
    void RegisterOnStop(std::function<void()> cb) {
        m_OnStop = [cb = std::move(cb)] { if (!cb) return false; cb(); return true; };
    }
    void RegisterPlayTransition(std::function<bool()> cb) { m_OnPlay = std::move(cb); }
    void RegisterStopTransition(std::function<bool()> cb) { m_OnStop = std::move(cb); }

    // ── Editor Camera ────────────────────────────────────────────────────────
    EditorCamera* GetEditorCamera() { return &m_EditorCamera; }

    // ── ImGui capture query ──────────────────────────────────────────────────
    bool WantCaptureMouse() const;

    // ── Entity selection ─────────────────────────────────────────────────────
    void SetSelectedEntity(Entity* entity);
    void SetLookedAtEntity(Entity* entity);
    void ClearSelection();
    bool SanitizeSelection(Scene* scene = nullptr);
    EditorSelectionState& GetSelectionState() noexcept { return m_SelectionState; }
    const EditorSelectionState& GetSelectionState() const noexcept { return m_SelectionState; }
    TransformUndoHistory& GetUndoHistory() noexcept { return m_UndoHistory; }
    void SetSpawnPosition(const glm::vec3& pos) { m_SpawnPosition = pos; }

    // ── Console logging ──────────────────────────────────────────────────────
    void Log(const std::string& msg) { m_ConsoleLogs.push_back(msg); }
    void ClearLogs()                 { m_ConsoleLogs.clear(); }
    void ExecuteCommand(const char* command);

    void ResetDocument(Scene* scene, std::string path = {});
    void RefreshDocument(Scene* scene, bool force = false);
    bool IsDocumentDirty() const { return m_Document.IsDirty(); }
    bool HasPendingDocumentAction() const { return m_Document.HasPending() || m_SceneDialogOpen || m_RequestSceneDialog; }
    void RequestClose(Scene* scene);
    void RequestOpenScene(Scene* scene, std::string path, bool legacy = false);
    void RequestNewScene(Scene* scene);
    bool ConsumeCloseApproval();

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
    ProjectDirectoryCache m_ProjectDirectoryCache;
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
    void DrawSceneViewport();
    void RequestSceneDialog(bool save);
    void DrawSceneDialog(Scene* scene);
    void SaveCurrentScene(Scene* scene);
    bool SaveSceneTo(Scene* scene, const std::string& path);
    void DrawDocumentGuard(Scene* scene);
    void ExecuteDocumentAction(Scene* scene);

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
    bool m_ResetWorkspace = false;
    bool m_SceneHovered = false;
    bool m_SceneVisible = false;
    glm::vec2 m_SceneOrigin = {}, m_SceneSize = {};
    ImDrawList* m_SceneDrawList = nullptr;
    unsigned int m_SceneTexture = 0;
    int m_SceneTextureWidth = 0, m_SceneTextureHeight = 0;
    SceneDocumentState m_Document;
    Scene* m_DocumentScene = nullptr;
    std::weak_ptr<Scene*> m_DocumentLifetime;
    std::chrono::steady_clock::time_point m_NextDocumentCheck{};
    bool m_CloseApproved = false;
    bool m_SceneDialogOpen = false;
    char m_ScenePathBuffer[1024] = {};
    std::string m_SceneIOError;
    bool m_SceneDialogSave = false;
    bool m_RequestSceneDialog = false;
    bool m_ConfirmSceneAction = false;
    ImGuiTextFilter m_AssetFilter;
    ImGuiTextFilter m_HierarchyFilter;
    InspectorTextState m_InspectorText;
    bool m_GizmoConsumedClick = false;
    EditorModeState m_ModeState;

    std::function<bool()> m_OnPlay;
    std::function<bool()> m_OnStop;

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
    EntityHandle m_GizmoTargetHandle = {};

    // ── Selection & Undo System ─────────────────────────────────────────────
    EditorSelectionState m_SelectionState;
    TransformUndoHistory m_UndoHistory;

    void UndoLastAction(Scene* scene);

    // ── Clipboard (Copy / Paste) ───────────────────────────────────────────────
    struct EntityClipboard {
        bool        valid     = false;
        std::string name;
        glm::vec3   pos       = {}, rot = {}, scale = {1,1,1};
        glm::vec3   color     = {1,1,1};
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Texture> texture;
        bool        hasBc     = false;
        glm::vec3   bcSize    = {1,1,1}, bcCenter = {};
        bool        bcTrigger = false;
        int         bcShape   = 0;
        float       bcRadius  = 0.5f;
        bool        hasBody   = false;
        glm::vec3   bodyVelocity = {}, bodyForce = {}, bodyAngularVelocity = {};
        float       bodyMass = 1.0f, bodyDrag = 0.1f, bodyBounce = 0.0f;
        float       bodyFriction = 0.6f, bodyAngularDrag = 0.1f;
        bool        bodyGravity = true, bodyKinematic = false;
        bool        bodyContinuous = false, bodyPushable = true;
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
