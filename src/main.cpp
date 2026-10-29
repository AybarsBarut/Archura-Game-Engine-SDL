#define SDL_MAIN_HANDLED
#include "core/Application.h"
#include "core/GameManifest.h"
#include "core/StartupProgress.h"
#include "game/GraphicalNetworkSession.h"
#include "rendering/GraphicsAPI.h"
#include <iostream>
#include <cstdio>
#include <windows.h>
#include <memory>
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <string_view>

// Harici GPU Seçimi (NVIDIA / AMD)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {

void PrintSmokeTelemetry(const Archura::Application& app) {
    const auto cpu = app.GetFrameTelemetry().Snapshot();
    const auto pump = app.GetNetworkPumpTelemetry().Snapshot();
    const auto interval = app.GetFrameIntervalTelemetry().Snapshot();
    const auto present = app.GetPresentTelemetry().Snapshot();
    const auto pacing = app.GetPacingTelemetry().Snapshot();
    const auto& loop = app.GetGameLoopStats();
    std::cout << "LOOP_METRICS frames=" << cpu.observedFrames << " ticks=" << loop.TotalTicks()
              << " dropped_ticks=" << loop.DroppedTicks() << " dropped_seconds=" << loop.DroppedSeconds()
              << " cpu_ms_p50=" << cpu.p50CpuWorkMs << " cpu_ms_p95=" << cpu.p95CpuWorkMs
              << " cpu_ms_p99=" << cpu.p99CpuWorkMs << " pump_samples=" << pump.observedFrames
              << " pump_ms_p95=" << pump.p95CpuWorkMs
              << " frame_ms_p95=" << interval.p95CpuWorkMs
              << " swap_ms_p95=" << present.p95CpuWorkMs
              << " pacing_ms_p95=" << pacing.p95CpuWorkMs << '\n';
}

// SDL's software renderer can still use an accelerated window framebuffer.
// Preserve the caller's GL context across splash drawing and destruction.
class ScopedGLContext {
public:
    ScopedGLContext() noexcept
        : m_Window(SDL_GL_GetCurrentWindow()), m_Context(SDL_GL_GetCurrentContext()),
          m_WindowID(m_Window ? SDL_GetWindowID(m_Window) : 0) {}
    ~ScopedGLContext() {
        if (CanRestore() && SDL_GL_MakeCurrent(m_Window, m_Context) != 0)
            std::cerr << "SPLASH ERROR: Cannot restore graphics context: " << SDL_GetError() << '\n';
    }
    ScopedGLContext(const ScopedGLContext&) = delete;
    ScopedGLContext& operator=(const ScopedGLContext&) = delete;

    void Restore() {
        if (CanRestore() && SDL_GL_MakeCurrent(m_Window, m_Context) != 0)
            throw std::runtime_error(std::string("Cannot restore graphics context after splash: ") + SDL_GetError());
        m_Context = nullptr;
    }

private:
    bool CanRestore() const noexcept {
        return m_Context && m_WindowID && SDL_WasInit(SDL_INIT_VIDEO) &&
               SDL_GetWindowFromID(m_WindowID) == m_Window;
    }
    SDL_Window* m_Window;
    SDL_GLContext m_Context;
    Uint32 m_WindowID;
};

// This object owns one SDL video reference and all splash resources. Declare it
// after the Application owner so it is destroyed before Engine calls SDL_Quit.
class StartupSplash {
public:
    StartupSplash() = default;
    StartupSplash(const StartupSplash&) = delete;
    StartupSplash& operator=(const StartupSplash&) = delete;
    ~StartupSplash() { Close(); }

    void Open() {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
            throw std::runtime_error(std::string("Splash SDL init failed: ") + SDL_GetError());
        m_Video = true;
        m_Window = SDL_CreateWindow("Archura Starting...", SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED, 600, 300, SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN);
        if (!m_Window) throw std::runtime_error(std::string("Splash window failed: ") + SDL_GetError());
        // Prefer software rendering, while still guarding against SDL's
        // optional accelerated window framebuffer during every update.
        m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_SOFTWARE);
        if (!m_Renderer) throw std::runtime_error(std::string("Splash renderer failed: ") + SDL_GetError());
        if (TTF_Init() == 0) {
            m_TTF = true;
            m_TitleFont = TTF_OpenFont("assets/fonts/Roboto-Bold.ttf", 64);
            m_TextFont = TTF_OpenFont("assets/fonts/Roboto-Bold.ttf", 18);
        }
        SDL_RaiseWindow(m_Window);
        SDL_SetWindowAlwaysOnTop(m_Window, SDL_TRUE);
    }

    bool Report(Archura::StartupStage stage, bool cancelTest) {
        if (!m_Window) return true;
        ScopedGLContext context;
        if (cancelTest && stage == Archura::StartupStage::CoreReady) {
            SDL_Event close{};
            close.type = SDL_WINDOWEVENT;
            close.window.windowID = SDL_GetWindowID(m_Window);
            close.window.event = SDL_WINDOWEVENT_CLOSE;
            if (SDL_PushEvent(&close) != 1)
                throw std::runtime_error("Could not inject splash cancellation event");
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
                return false;
        }

        int completed = 0;
        const char* message = "Initializing engine...";
        switch (stage) {
        case Archura::StartupStage::Starting: break;
        case Archura::StartupStage::CoreReady:
            completed = 1; message = "Core systems ready; preparing world..."; break;
        case Archura::StartupStage::WorldReady:
            completed = 2; message = "World ready; preparing first frame..."; break;
        case Archura::StartupStage::Ready:
            completed = 3; message = "Ready"; break;
        }
        SDL_SetRenderDrawColor(m_Renderer, 30, 30, 30, 255);
        SDL_RenderClear(m_Renderer);
        DrawText(m_TitleFont, "ARCHURA", 70, true, {255, 255, 255, 255});
        DrawText(m_TextFont, message, 195, false, {180, 180, 180, 255});
        SDL_Rect background{50, 220, 500, 15};
        SDL_SetRenderDrawColor(m_Renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(m_Renderer, &background);
        SDL_Rect foreground{50, 220, 500 * completed / 3, 15};
        SDL_SetRenderDrawColor(m_Renderer, 80, 180, 255, 255);
        SDL_RenderFillRect(m_Renderer, &foreground);
        SDL_RenderPresent(m_Renderer);
        if (stage == Archura::StartupStage::Ready) Close();
        context.Restore();
        return true;
    }

private:
    void DrawText(TTF_Font* font, const char* text, int y, bool centered, SDL_Color color) {
        if (!font) return;
        SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_Renderer, surface);
        if (texture) {
            SDL_Rect destination{centered ? (600 - surface->w) / 2 : 50, y, surface->w, surface->h};
            SDL_RenderCopy(m_Renderer, texture, nullptr, &destination);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }

    void Close() noexcept {
        ScopedGLContext context;
        if (m_TitleFont) TTF_CloseFont(m_TitleFont);
        if (m_TextFont) TTF_CloseFont(m_TextFont);
        m_TitleFont = m_TextFont = nullptr;
        // Window's failed-init cleanup may already have called SDL_Quit,
        // which destroys every SDL window and renderer, including this splash.
        const bool videoAlive = SDL_WasInit(SDL_INIT_VIDEO) != 0;
        if (videoAlive && m_Renderer) SDL_DestroyRenderer(m_Renderer);
        if (videoAlive && m_Window) SDL_DestroyWindow(m_Window);
        m_Renderer = nullptr;
        m_Window = nullptr;
        if (m_TTF) TTF_Quit();
        if (m_Video && videoAlive) SDL_QuitSubSystem(SDL_INIT_VIDEO);
        m_TTF = m_Video = false;
    }

    SDL_Window* m_Window = nullptr;
    SDL_Renderer* m_Renderer = nullptr;
    TTF_Font* m_TitleFont = nullptr;
    TTF_Font* m_TextFont = nullptr;
    bool m_Video = false;
    bool m_TTF = false;
};

} // namespace

int main(int argc, char** argv) {
    bool smokeTest = false;
    bool startupCancelTest = false;
    bool startupSplashTest = false;
    bool playStopSmokeTest = false;
    Archura::NetworkLaunchOptions network;
    std::filesystem::path gamePath;
    try {
    for (int i = 1; i < argc; ++i) {
        const std::string_view option = argv[i] ? argv[i] : "";
        auto next = [&]() -> std::string {
            if (++i >= argc || !argv[i] || !argv[i][0]) throw std::invalid_argument("missing argument value");
            return argv[i];
        };
        if (option == "--startup-smoke-test") { startupSplashTest = smokeTest = true; continue; }
        if (option == "--startup-cancel-test") { startupCancelTest = true; continue; }
        if (option == "--connect") { network.host = next(); continue; }
        if (option == "--port") {
            const auto value = next(); std::size_t end = 0;
            network.port = std::stoi(value, &end);
            if (end != value.size() || network.port < 1 || network.port > 65535) throw std::invalid_argument("invalid port");
            continue;
        }
        if (option == "--network-smoke-test") { network.smoke = true; continue; }
        if (option == "--network-move-x") { network.moveX = std::stof(next()); continue; }
        if (option == "--network-move-z") { network.moveZ = std::stof(next()); continue; }
        if (argv[i] && std::string_view(argv[i]) == "--game") {
            if (++i == argc || !argv[i] || std::string_view(argv[i]).empty()) {
                std::cerr << "--game requires a manifest path\n"; return 1;
            }
            gamePath = argv[i];
        }
        if (argv[i] && std::string_view(argv[i]) == "--smoke-test") smokeTest = true;
        if (argv[i] && std::string_view(argv[i]) == "--play-stop-smoke-test")
            smokeTest = playStopSmokeTest = true;
    }
    if (startupSplashTest && (playStopSmokeTest || !gamePath.empty() || !network.host.empty()))
        throw std::invalid_argument("startup splash smoke requires normal editor startup");
    if (startupCancelTest && (smokeTest || !gamePath.empty() || !network.host.empty()))
        throw std::invalid_argument("startup cancel test requires normal editor startup");
    if (network.smoke && (network.host.empty() || smokeTest || playStopSmokeTest || !gamePath.empty() ||
        !Archura::Net::ValidPlayerInput({1, network.moveX, network.moveZ})))
        throw std::invalid_argument("network smoke requires --connect, valid axes, and no other smoke/game mode");
    } catch (const std::exception& error) { std::cerr << "LAUNCH ERROR: " << error.what() << '\n'; return 1; }
    wchar_t executableName[32768];
    const DWORD executableLength = GetModuleFileNameW(nullptr, executableName, 32768);
    if (!executableLength || executableLength >= 32768) return 1;
    const std::filesystem::path executablePath(executableName);
    if (gamePath.empty() && executablePath.stem() == "ArchuraGame")
        gamePath = executablePath.parent_path() / "game.json";
    Archura::GameManifest game;
    const bool standalone = !gamePath.empty();
    if (standalone && (startupCancelTest || startupSplashTest)) {
        std::cerr << "Startup splash tests require editor startup\n"; return 1;
    }
    if (standalone) {
        std::string error;
        if (!Archura::GameManifest::Load(gamePath, game, error)) {
            std::cerr << "GAME CONFIG ERROR: " << error << '\n'; return 1;
        }
        std::error_code directoryError;
        std::filesystem::current_path(game.root, directoryError);
        if (directoryError) { std::cerr << "Cannot enter game root\n"; return 1; }
    }
    // Tests and normal startup must have a writable diagnostic directory.
    std::error_code logError;
    std::filesystem::create_directories("logs", logError);
    if (logError) std::cerr << "Cannot create logs directory: " << logError.message() << '\n';
    Archura::GraphicsLaunchOptions graphicsOptions;
    std::string graphicsError;
    if (!Archura::ResolveGraphicsLaunchOptions(
            argc, argv, "config/graphics_api.cfg", graphicsOptions,
            graphicsError)) {
        std::cerr << "GRAPHICS CONFIG ERROR: " << graphicsError << "\n";
        return -1;
    }

    try {
        std::unique_ptr<Archura::Application> app;
        StartupSplash splash;
        Archura::StartupProgressCallback progress;
        bool coreReady = false;
        bool startupReady = false;
        if ((!smokeTest || startupSplashTest) && !standalone && network.host.empty()) {
            splash.Open();
            progress = [&](Archura::StartupStage stage) {
                if (stage == Archura::StartupStage::CoreReady) coreReady = true;
                const bool keepGoing = splash.Report(stage, startupCancelTest);
                if (stage == Archura::StartupStage::Ready && keepGoing) startupReady = true;
                return keepGoing;
            };
        }
        app = std::make_unique<Archura::Application>(graphicsOptions);
        try {
            app->Run(smokeTest ? 16 : 0, playStopSmokeTest, standalone ? &game : nullptr,
                     network.host.empty() ? nullptr : &network, progress);
        } catch (const Archura::StartupCancelled&) {
            if (startupSplashTest) throw std::runtime_error("Startup splash smoke was interrupted");
            if (startupCancelTest && (!coreReady || app->GetFrameTelemetry().Snapshot().observedFrames != 0))
                throw std::runtime_error("Startup cancellation did not occur before the first frame");
            // Leave this scope to close splash before Application/SDL teardown.
            if (startupCancelTest) std::cout << "Startup cancellation completed before first frame\n";
            return 0;
        }
        if (startupSplashTest && !startupReady) throw std::runtime_error("Startup splash did not reach ready");
        if (startupCancelTest) throw std::runtime_error("Startup cancellation test did not cancel");
        if (network.smoke) {
            const auto frames = app->GetFrameTelemetry().Snapshot().observedFrames;
            PrintSmokeTelemetry(*app);
            app.reset();
            if (frames < 16) throw std::runtime_error("Graphical network smoke rendered too few frames");
            std::cout << "Graphical network smoke completed and shutdown\n";
        }
        if (smokeTest) {
            const auto frames = app->GetFrameTelemetry().Snapshot().observedFrames;
            PrintSmokeTelemetry(*app);
            app.reset(); // Include orderly shutdown in the smoke-test result.
            if (frames != 16) {
                std::cerr << "Startup smoke interrupted after " << frames << " frames\n";
                return 1;
            }
            std::cout << "Startup smoke completed: 16 rendered frames and shutdown\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
        return -1;
    }

    return 0;
}
