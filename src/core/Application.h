#pragma once

#include "FrameTelemetry.h"
#include "GameLoopHarness.h"
#include "StartupProgress.h"
#include "Window.h"
#include "../editor/PlayModeSession.h"
#include "../rendering/GraphicsAPI.h"
#include <cstdint>
#include <memory>

namespace Archura {

    class Application {
    public:
        explicit Application(GraphicsLaunchOptions graphicsOptions = {});
        virtual ~Application();

        static Application& Get() { return *s_Instance; }
        static Application* TryGet() noexcept { return s_Instance; }

        void Run(uint32_t frameLimit = 0, bool playStopSmokeTest = false,
                 const struct GameManifest* game = nullptr, const struct NetworkLaunchOptions* network = nullptr,
                 const StartupProgressCallback& progress = {});
        bool JoinNetworkGame(const std::string& host, int port);
        void LeaveNetworkGame();
        void Quit() { m_Running = false; }
        bool LoadEditorProject(const std::string& path);

        Window& GetWindow() { return *m_Window; }

    private:
        bool Init();
        void InitializeGame(const GameManifest& game);
        bool PumpNetworkGame();
        bool IsNetworkViewActive() const;
        void OnEvent(); // Event polling placeholder if needed here



    public:
        // Console Command Helpers
        void SetFPSLimit(float limit);
        void SetSensitivity(float sens);
        void SetDevMode(bool enabled);
        bool IsDevMode() const { return m_DevModeActive; }
        
        class FPSController* GetFPSController() { return m_FPSController.get(); }
        class Scene* GetScene() const { return m_Scene.get(); } // Expose Scene
        class RenderSystem* GetRenderSystem();
        class Camera* GetCamera();
        const FrameTelemetry& GetFrameTelemetry() const { return m_FrameTelemetry; }
        const FrameTelemetry& GetNetworkPumpTelemetry() const { return m_NetworkPumpTelemetry; }
        const FrameTelemetry& GetFrameIntervalTelemetry() const { return m_FrameIntervalTelemetry; }
        const FrameTelemetry& GetPresentTelemetry() const { return m_PresentTelemetry; }
        const FrameTelemetry& GetPacingTelemetry() const { return m_PacingTelemetry; }
        const GameLoopHarness& GetGameLoopStats() const { return m_GameLoop; }

    private:
        static Application* s_Instance;
        Window* m_Window = nullptr; // Reference to Engine's window (owned by Engine)
        std::unique_ptr<class ImGuiLayer> m_ImGuiLayer;
        bool m_Running = true;
        GraphicsLaunchOptions m_GraphicsOptions;
        
        // Tickrate Configuration (128 Hz fixed timestep)
        static constexpr float TICK_INTERVAL = static_cast<float>(GameLoopHarness::TickInterval);
        
        // Timing
        GameLoopHarness m_GameLoop;
        
        // Performance Monitoring
        double m_LastFPSUpdateTime = 0.0;
        int m_FrameCount = 0;
        float m_CurrentFPS = 0.0f;
        float m_FPSLimit = 144.0f;
        FrameTelemetry m_FrameTelemetry;
        FrameTelemetry m_NetworkPumpTelemetry;
        // Reuse the bounded duration ring; samples are milliseconds for each
        // named scope, independently of the CPU-work ring's observation count.
        FrameTelemetry m_FrameIntervalTelemetry;
        FrameTelemetry m_PresentTelemetry;
        FrameTelemetry m_PacingTelemetry;

        // Game State (Moved from local Run scope)
        std::unique_ptr<class Scene> m_Scene; // Scene is now a member
        std::unique_ptr<class FPSController> m_FPSController;
        std::unique_ptr<class RenderSystem> m_RenderSystem;
        std::unique_ptr<class PhysicsSystem> m_PhysicsSystem;
        bool m_LocalPresentationActive = false;
        std::unique_ptr<class ScriptSystem> m_ScriptSystem;
        std::unique_ptr<class ParticleSystem> m_ParticleSystem;
        std::unique_ptr<class ProjectileSystem> m_ProjectileSystem;
        std::unique_ptr<class HUDRenderer> m_HUDRenderer;
        std::unique_ptr<class Editor> m_Editor;
        std::unique_ptr<class PauseMenu> m_PauseMenu;
        std::unique_ptr<class Camera> m_Camera;
#ifdef ARCHURA_DEBUG_PHYSICS
        std::unique_ptr<class DebugPhysicsSystem> m_DebugPhysicsSystem;
#endif
#ifdef ARCHURA_OPENAL
        std::unique_ptr<class OpenALAudioSystem> m_OpenALAudioSystem;
#endif
        
        // Cached pointers to engine subsystems
        class Window* m_EngineWindow = nullptr;
        class Input* m_Input = nullptr;
        class Renderer* m_Renderer = nullptr;
        
        // Generation-checked player identity. Resolve through Scene before use;
        // editor deletion must never leave a cached raw pointer behind.
        uint64_t m_PlayerHandleValue = 0;
        std::string m_PlayerPersistentID;
        PlayModeSession m_PlaySession;
        std::unique_ptr<class GraphicalNetworkSession> m_NetworkGame;
        const NetworkLaunchOptions* m_NetworkLaunch = nullptr;
        bool m_NetworkSnapshotFailed = false;
        std::uint32_t m_NetworkStartedAt = 0;
        std::uint64_t m_LastNetworkLoggedTick = 0;
        
        bool m_DevModeActive = true;
        bool m_Standalone = false;
        std::string m_GameTitle;
        bool m_IsPaused = false;
        
        // Separated update/render methods
        void ProcessInput();           // Input processing (Once per frame)
        void UpdateGameLogic(float dt);    // Game logic (128 Hz fixed)
        void RenderFrame(float alpha);     // Rendering (variable Hz)
        void LimitFrameRate(double frameStartTime);
    };

}
