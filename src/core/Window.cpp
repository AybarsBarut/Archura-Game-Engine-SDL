#include "Window.h"
#include <SDL.h>
#include <cstdio>
#include <glad/glad.h>
#include <iostream>
#include <algorithm>

namespace Archura {

static bool s_SDLInitialized = false;

static void LogStartup(const char *message) {
  const char *logPath = "logs/startup_detailed.txt";
  FILE *f = fopen(logPath, "a");
  if (f) {
    fprintf(f, "[%u ms] WINDOW: %s\n", SDL_GetTicks(), message);
    fflush(f);
    fclose(f);
  }
}

Window::Window(const WindowProps &props)
    : m_Window(nullptr), m_Context(nullptr), m_Width(props.width),
      m_Height(props.height), m_VSync(props.vsync), m_DeltaTime(0.0f),
      m_LastFrameTime(0.0f), m_FPS(0.0f), m_FPSTimer(0.0f), m_FrameCount(0),
      m_Fullscreen(props.fullscreen), m_ShouldClose(false) {
  Init(props);
}

Window::~Window() { Shutdown(); }

void Window::Init(const WindowProps &props) {
  LogStartup("Init started");

  m_Title = props.title;

  // Initialize SDL
  if (!s_SDLInitialized) {
    LogStartup("Initializing SDL");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
        0) {
      std::cerr << "SDL Initialization failed: " << SDL_GetError() << "\n";
      char buf[256];
      snprintf(buf, sizeof(buf), "SDL Init FAILED: %s", SDL_GetError());
      LogStartup(buf);
      return;
    }
    s_SDLInitialized = true;
    LogStartup("SDL initialized successfully");
  }

  // OpenGL Attributes
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
#ifdef ARCHURA_DEBUG
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

  // MSAA 4x
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  // Create Window
  LogStartup("Creating SDL window");

  Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                 SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN;
  if (props.fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }

  m_Window = SDL_CreateWindow(m_Title.c_str(), SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, m_Width, m_Height, flags);

  if (!m_Window) {
    std::cerr << "Failed to create SDL Window: " << SDL_GetError() << "\n";
    char buf[256];
    snprintf(buf, sizeof(buf), "SDL window creation FAILED: %s",
             SDL_GetError());
    LogStartup(buf);
    return;
  }

  LogStartup("SDL window created successfully");

  // Create OpenGL Context
  LogStartup("Creating OpenGL context");

  m_Context = SDL_GL_CreateContext(m_Window);
  if (!m_Context) {
    std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << "\n";
    char buf[256];
    snprintf(buf, sizeof(buf), "OpenGL context creation FAILED: %s",
             SDL_GetError());
    LogStartup(buf);
    return;
  }

  LogStartup("OpenGL context created successfully");

  if (SDL_GL_MakeCurrent(m_Window, m_Context) != 0) {
    std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << "\n";
    LogStartup("OpenGL make-current FAILED");
    return;
  }

  // Initialzie GLAD
  LogStartup("Initializing GLAD");

  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    FILE *f_glad = fopen("logs/startup_log.txt", "a");
    if (f_glad) {
      fprintf(f_glad, "GLAD Failed\n");
      fclose(f_glad);
    }
    std::cerr << "Failed to initialize GLAD!\n";
    LogStartup("GLAD initialization FAILED");
    return;
  }
  FILE *f_glad_init = fopen("logs/startup_log.txt", "a");
  if (f_glad_init) {
    fprintf(f_glad_init, "GLAD Initialized\n");
    fclose(f_glad_init);
  }

  LogStartup("GLAD initialized successfully");

  std::cout << "OpenGL Info:" << std::endl;
  std::cout << "  Vendor: " << glGetString(GL_VENDOR) << std::endl;
  std::cout << "  Renderer: " << glGetString(GL_RENDERER) << std::endl;
  std::cout << "  Version: " << glGetString(GL_VERSION) << std::endl;

  // VSync
  LogStartup("Setting up OpenGL state");

  SetVSync(m_VSync);

  // OpenGL state
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  RefreshDrawableSize();

  m_LastFrameTime = (float)SDL_GetTicks() / 1000.0f;
  m_Initialized = true;

  LogStartup("Init completed successfully");
}

void Window::Shutdown() {
  m_Initialized = false;
  if (m_Context) {
    if (m_Window) SDL_GL_MakeCurrent(m_Window, m_Context);
    SDL_GL_DeleteContext(m_Context);
    m_Context = nullptr;
  }
  if (m_Window) {
    SDL_DestroyWindow(m_Window);
    m_Window = nullptr;
  }

  // Note: SDL_Quit() is called only if s_SDLInitialized is true.
  // In a multi-window scenario, we should track reference counts.
  // For this engine's current architecture, we assume a single main window.
  if (s_SDLInitialized) {
    SDL_Quit();
    s_SDLInitialized = false;
    LogStartup("SDL Shutdown (Global)");
  }
}

void Window::Update() {
  // Polling is done in Application Loop now!
  if (!IsValid()) return;
  RefreshDrawableSize();
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

bool Window::ShouldClose() const { return m_ShouldClose; }

void Window::SetVSync(bool enabled) {
  if (!m_Context) return;
  // SDL: 0 immediate, 1 vsync, -1 adaptive
  if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) != 0) {
    std::cerr << "Failed to set swap interval: " << SDL_GetError() << "\n";
    return;
  }
  m_VSync = enabled;
}

void Window::SetFullscreen(bool enabled) {
  if (m_Fullscreen == enabled)
    return;

  if (!m_Window) return;
  const Uint32 mode = enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
  if (SDL_SetWindowFullscreen(m_Window, mode) != 0) {
    std::cerr << "Failed to change fullscreen mode: " << SDL_GetError() << "\n";
    return;
  }
  m_Fullscreen = enabled;
  if (enabled) {
    // Use WINDOW_FULLSCREEN_DESKTOP for borderless fullscreen (more stable
    // usually)
    int w, h;
    SDL_GetWindowSize(m_Window, &w, &h);
    m_Width = w;
    m_Height = h;
  } else {
    SDL_SetWindowSize(m_Window, 1280, 720);
    SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);

    m_Width = 1280;
    m_Height = 720;
  }
  RefreshDrawableSize();
}

void Window::SetResolution(unsigned int width, unsigned int height) {
  if (m_Fullscreen || !m_Window || width == 0 || height == 0)
    return;
  m_Width = width;
  m_Height = height;
  SDL_SetWindowSize(m_Window, width, height);
  RefreshDrawableSize();
}

void Window::RefreshDrawableSize() {
  if (!m_Window || !m_Context) return;
  int logicalWidth = 0;
  int logicalHeight = 0;
  int drawableWidth = 0;
  int drawableHeight = 0;
  SDL_GetWindowSize(m_Window, &logicalWidth, &logicalHeight);
  SDL_GL_GetDrawableSize(m_Window, &drawableWidth, &drawableHeight);
  m_Width = static_cast<unsigned int>(std::max(logicalWidth, 0));
  m_Height = static_cast<unsigned int>(std::max(logicalHeight, 0));
  m_FramebufferWidth = static_cast<unsigned int>(std::max(drawableWidth, 0));
  m_FramebufferHeight = static_cast<unsigned int>(std::max(drawableHeight, 0));
  if (drawableWidth > 0 && drawableHeight > 0) {
    glViewport(0, 0, drawableWidth, drawableHeight);
  }
}

} // namespace Archura
