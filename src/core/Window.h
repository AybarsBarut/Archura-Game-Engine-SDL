#pragma once

#include "../rendering/GraphicsAPI.h"
#include <string>
#include <memory>

// Prevent unnecessary includes if any


// Forward declarations
struct SDL_Window;

namespace Archura {

/**
 * @brief Window sınıfı - SDL2 window yönetimi ve OpenGL context
 * 
 * GTX 1050 hedefli, yüksek performanslı window oluşturur
 */
class Window {
public:
    struct WindowProps {
        std::string title;
        unsigned int width;
        unsigned int height;
        bool vsync;
        bool fullscreen;
        GraphicsAPI graphicsAPI;

        WindowProps(
            const std::string& title = "Archura Engine",
            unsigned int width = 1920,
            unsigned int height = 1080,
            bool vsync = false,
            bool fullscreen = false,
            GraphicsAPI graphicsAPI = GraphicsAPI::OpenGL
        ) : title(title), width(width), height(height), vsync(vsync),
            fullscreen(fullscreen), graphicsAPI(graphicsAPI) {}
    };

public:
    Window(const WindowProps& props = WindowProps());
    ~Window();

    // Copy constructor ve assignment operator'ı disable et
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void Update();
    bool ShouldClose() const;
    bool IsValid() const {
        return m_Window != nullptr && m_Initialized &&
               (m_GraphicsAPI != GraphicsAPI::OpenGL || m_Context != nullptr);
    }
    
    void SetVSync(bool enabled);
    bool IsVSync() const { return m_VSync; }

    void SetFullscreen(bool enabled);

    bool IsFullscreen() const { return m_Fullscreen; }

    void SetResolution(unsigned int width, unsigned int height); // New: Resolution support

    unsigned int GetWidth() const { return m_Width; }
    unsigned int GetHeight() const { return m_Height; }
    float GetAspectRatio() const { return m_Height ? (float)m_Width / (float)m_Height : 1.0f; }
    unsigned int GetFramebufferWidth() const { return m_FramebufferWidth; }
    unsigned int GetFramebufferHeight() const { return m_FramebufferHeight; }
    void RefreshDrawableSize();

    SDL_Window* GetNativeWindow() const { return m_Window; }
    void* GetContext() const { return m_Context; }
    GraphicsAPI GetGraphicsAPI() const { return m_GraphicsAPI; }
    
    void SetShouldClose(bool value) { m_ShouldClose = value; }

    // FPS tracking
    // FPS tracking
    float GetDeltaTime() const { return m_DeltaTime; }
    float GetFPS() const { return m_FPS; }
    float GetLastFrameTime() const { return m_LastFrameTime; }

private:
    void Init(const WindowProps& props);
    void Shutdown();
    void UpdateDeltaTime();

private:
    SDL_Window* m_Window;
    void* m_Context; // SDL_GLContext
    std::string m_Title;
    unsigned int m_Width, m_Height;
    unsigned int m_FramebufferWidth = 0, m_FramebufferHeight = 0;
    bool m_VSync;
    bool m_Fullscreen;
    bool m_ShouldClose = false;
    bool m_Initialized = false;
    GraphicsAPI m_GraphicsAPI = GraphicsAPI::OpenGL;

    // Performans tracking
    float m_DeltaTime;
    float m_LastFrameTime;
    float m_FPS;
    float m_FPSTimer;
    int m_FrameCount;
};

} // namespace Archura
