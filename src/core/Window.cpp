#include "Window.h"
#include <SDL.h>
#include <glad/glad.h>
#include <iostream>

namespace Archura {

static bool s_SDLInitialized = false;

Window::Window(const WindowProps& props)
    : m_Window(nullptr)
    , m_Context(nullptr)
    , m_Width(props.width)
    , m_Height(props.height)
    , m_VSync(props.vsync)
    , m_DeltaTime(0.0f)
    , m_LastFrameTime(0.0f)
    , m_FPS(0.0f)
    , m_FPSTimer(0.0f)
    , m_FrameCount(0)
    , m_Fullscreen(props.fullscreen)
    , m_ShouldClose(false)
{
    Init(props);
}

Window::~Window() {
    Shutdown();
}

void Window::Init(const WindowProps& props) {
    m_Title = props.title;

    // Initialize SDL
    if (!s_SDLInitialized) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
            std::cerr << "SDL Initialization failed: " << SDL_GetError() << "\n";
            return;
        }
        s_SDLInitialized = true;
    }

    // OpenGL Attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    // MSAA 4x
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    // Create Window
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN;
    if (props.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    m_Window = SDL_CreateWindow(
        m_Title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_Width,
        m_Height,
        flags
    );
    
    if (!m_Window) {
        std::cerr << "Failed to create SDL Window: " << SDL_GetError() << "\n";
        return;
    }

    // Create OpenGL Context
    m_Context = SDL_GL_CreateContext(m_Window);
    if (!m_Context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << "\n";
        return;
    }

    SDL_GL_MakeCurrent(m_Window, m_Context);

    // Initialzie GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        FILE* f = fopen("startup_log.txt", "a"); if(f) { fprintf(f, "GLAD Failed\n"); fclose(f); }
        std::cerr << "Failed to initialize GLAD!\n";
        return;
    }
    FILE* f = fopen("startup_log.txt", "a"); if(f) { fprintf(f, "GLAD Initialized\n"); fclose(f); }

    std::cout << "OpenGL Info:" << std::endl;
    std::cout << "  Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "  Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "  Version: " << glGetString(GL_VERSION) << std::endl;

    // VSync
    SetVSync(m_VSync);

    // OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glViewport(0, 0, m_Width, m_Height);

    m_LastFrameTime = (float)SDL_GetTicks() / 1000.0f;
}

void Window::Shutdown() {
    if (m_Context) {
        SDL_GL_DeleteContext(m_Context);
        m_Context = nullptr;
    }
    if (m_Window) {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }
    
    // Note: Usually one would SDL_Quit() at end of program, 
    // but if we have multiple windows, we might wait.
    // For now, assuming single window life-cycle.
    if (s_SDLInitialized) {
        SDL_Quit();
        s_SDLInitialized = false;
    }
}

void Window::Update() {
    // Polling is done in Application Loop now!
    SDL_GL_SwapWindow(m_Window);
    UpdateDeltaTime();
}

void Window::UpdateDeltaTime() {
    float currentTime = (float)SDL_GetTicks() / 1000.0f;
    m_DeltaTime = currentTime - m_LastFrameTime;
    m_LastFrameTime = currentTime;

    // FPS
    m_FrameCount++;
    m_FPSTimer += m_DeltaTime;
    
    if (m_FPSTimer >= 1.0f) {
        m_FPS = m_FrameCount / m_FPSTimer;
        m_FrameCount = 0;
        m_FPSTimer = 0.0f;
    }
}

bool Window::ShouldClose() const {
    return m_ShouldClose;
}

void Window::SetVSync(bool enabled) {
    m_VSync = enabled;
    // SDL: 0 immediate, 1 vsync, -1 adaptive
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}

void Window::SetFullscreen(bool enabled) {
    if (m_Fullscreen == enabled) return;
    
    m_Fullscreen = enabled;
    if (enabled) {
        // Use WINDOW_FULLSCREEN_DESKTOP for borderless fullscreen (more stable usually)
        SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        
        int w, h;
        SDL_GetWindowSize(m_Window, &w, &h);
        m_Width = w;
        m_Height = h;
    } else {
        SDL_SetWindowFullscreen(m_Window, 0);
        SDL_SetWindowSize(m_Window, 1280, 720);
        SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        
        m_Width = 1280;
        m_Height = 720;
    }
    glViewport(0, 0, m_Width, m_Height);
}

void Window::SetResolution(unsigned int width, unsigned int height) {
    if (m_Fullscreen) return;
    m_Width = width;
    m_Height = height;
    SDL_SetWindowSize(m_Window, width, height);
    glViewport(0, 0, width, height);
}

} // namespace Archura
