#pragma once

#include "FrameTelemetry.h"
#include "Window.h"
#include "../rendering/GraphicsAPI.h"
#include <cstdint>
#include <memory>

namespace Archura {

    class Application {
    public:
        explicit Application(GraphicsLaunchOptions graphicsOptions = {});
        virtual ~Application();

        static Application& Get() { return *s_Instance; }

        void Run();
        void Quit() { m_Running = false; }

        Window& GetWindow() { return *m_Window; }

    private:
        bool Init();
        void OnEvent(); // Event polling placeholder if needed here



    public:
        // Console Command Helpers
        void SetFPSLimit(float limit);
        void SetSensitivity(float sens);
        void SetDevMode(bool enabled);
        bool IsDevMode() const { return m_DevModeActive; }
        
        class FPSController* GetFPSController() { return m_FPSController.get(); }
        class Scene* GetScene() const { return m_Scene.get(); } // Expose Scene
        class RenderSystem* GetRenderSystem() { return m_RenderSystem.get(); }
        class Camera* GetCamera() { return m_Camera.get(); } // Expose Camera
        const FrameTelemetry& GetFrameTelemetry() const { return m_FrameTelemetry; }

    private:
        static Application* s_Instance;
        Window* m_Window; // Reference to Engine's window (owned by Engine)
        std::unique_ptr<class ImGuiLayer> m_ImGuiLayer;
        bool m_Running = true;
        GraphicsLaunchOptions m_GraphicsOptions;
        
        // Tickrate Configuration (128 Hz fixed timestep)
        static constexpr float TICK_RATE = 128.0f;
        static constexpr float TICK_INTERVAL = 1.0f / TICK_RATE;  // 7.8125ms
        static constexpr int MAX_TICKS_PER_FRAME = 5;  // Spiral of death protection
        
        // Timing
        float m_Accumulator = 0.0f;
        float m_LastFrameTime = 0.0f;
        uint32_t m_TickCount = 0;  // Total ticks since start
        
        // Performance Monitoring
        double m_LastFPSUpdateTime = 0.0;
        int m_FrameCount = 0;
        float m_CurrentFPS = 0.0f;
        float m_FPSLimit = 144.0f;
        FrameTelemetry m_FrameTelemetry;

        // Game State (Moved from local Run scope)
        std::unique_ptr<class Scene> m_Scene; // Scene is now a member
        std::unique_ptr<class FPSController> m_FPSController;
        std::unique_ptr<class RenderSystem> m_RenderSystem;
        std::unique_ptr<class PhysicsSystem> m_PhysicsSystem;
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
        
        bool m_DevModeActive = true;
        bool m_IsPaused = false;
        
        // Separated update/render methods
        void ProcessInput();           // Input processing (Once per frame)
        void UpdateGameLogic(float dt);    // Game logic (128 Hz fixed)
        void RenderFrame(float alpha);     // Rendering (variable Hz)
        void LimitFrameRate(float frameStartTime);
    };

}
