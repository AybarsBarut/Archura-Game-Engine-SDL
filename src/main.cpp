#define SDL_MAIN_HANDLED
#include "core/Application.h"
#include "rendering/GraphicsAPI.h"
#include <iostream>
#include <cstdio>
#include <windows.h>
#include <memory>
#include <SDL.h>
#include <SDL_ttf.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

// Harici GPU Seçimi (NVIDIA / AMD)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Global thread-safe degiskenler
std::atomic<bool> g_LoadingFinished(false);
std::atomic<int> g_ProgressPercent(0);
std::mutex g_LoadingMutex;
std::string g_LoadingMessage = "Initializing Archura Engine...";

void LoadingWorker() {
    auto SetProgress = [](int percent, const std::string& msg) {
        g_ProgressPercent = percent;
        std::lock_guard<std::mutex> lock(g_LoadingMutex);
        g_LoadingMessage = msg;
    };

    // --- YAPAY YUKLEME SIMULASYONU ---
    
    // 1. Core Systems
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    SetProgress(15, "Loading Core Systems...");
    
    // 2. Audio & Input
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    SetProgress(30, "Initializing Audio & Input...");
    
    // 3. Renderer & Shaders
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    SetProgress(50, "Compiling Shaders...");
    
    // 4. Physics Engine
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    SetProgress(75, "Loading Physics Engine...");
    
    // 5. Assets & UI
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    SetProgress(95, "Preloading Assets...");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    SetProgress(100, "Starting Engine...");

    g_LoadingFinished = true;
}

int main(int argc, char** argv) {
    Archura::GraphicsLaunchOptions graphicsOptions;
    std::string graphicsError;
    if (!Archura::ResolveGraphicsLaunchOptions(
            argc, argv, "config/graphics_api.cfg", graphicsOptions,
            graphicsError)) {
        std::cerr << "GRAPHICS CONFIG ERROR: " << graphicsError << "\n";
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SPLASH ERROR: SDL_Init Failed: " << SDL_GetError() << "\n";
        return -1;
    }

    if (TTF_Init() == -1) {
        std::cerr << "SPLASH ERROR: TTF_Init Failed: " << TTF_GetError() << "\n";
        SDL_Quit();
        return -1;
    }

    // Splash Screen Window & Renderer
    SDL_Window* splashWindow = SDL_CreateWindow(
        "Archura Starting...",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        600, 300,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN
    );

    if (!splashWindow) {
        std::cerr << "SPLASH ERROR: Failed to create splash window.\n";
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Konsol penceresinin önüne getirilmesi için
    SDL_RaiseWindow(splashWindow);
    SDL_SetWindowAlwaysOnTop(splashWindow, SDL_TRUE);

    SDL_Renderer* splashRenderer = SDL_CreateRenderer(splashWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!splashRenderer) {
        std::cerr << "SPLASH ERROR: Failed to create splash renderer.\n";
        SDL_DestroyWindow(splashWindow);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load Font
    TTF_Font* titleFont = TTF_OpenFont("assets/fonts/Roboto-Bold.ttf", 64);
    TTF_Font* subFont = TTF_OpenFont("assets/fonts/Roboto-Bold.ttf", 18);

    if (!titleFont || !subFont) {
        std::cerr << "SPLASH WARNING: Failed to load font: " << TTF_GetError() << "\n";
        // Font could not be loaded, but we can still draw the progress bar.
    }

    // Colors
    SDL_Color bgColor = { 30, 30, 30, 255 };      // #1E1E1E
    SDL_Color titleColor = { 255, 255, 255, 255 }; 
    SDL_Color textColor = { 180, 180, 180, 255 };
    SDL_Color barBgColor = { 50, 50, 50, 255 };
    SDL_Color barFgColor = { 80, 180, 255, 255 }; // Light Blue

    // Start Worker Thread
    std::thread loadingThread(LoadingWorker);

    // Splash Loop (Main Thread)
    bool quitSplash = false;
    SDL_Event e;
    
    while (!quitSplash && !g_LoadingFinished) {
        // Handle events to prevent window from "Not Responding"
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                // If user forces close during load
                exit(0); 
            }
        }

        // Draw Background
        SDL_SetRenderDrawColor(splashRenderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderClear(splashRenderer);

        // Draw Title "ARCHURA"
        if (titleFont) {
            SDL_Surface* titleSurf = TTF_RenderText_Blended(titleFont, "ARCHURA", titleColor);
            if (titleSurf) {
                SDL_Texture* titleTex = SDL_CreateTextureFromSurface(splashRenderer, titleSurf);
                SDL_Rect titleRect = { (600 - titleSurf->w) / 2, 70, titleSurf->w, titleSurf->h };
                SDL_RenderCopy(splashRenderer, titleTex, NULL, &titleRect);
                SDL_DestroyTexture(titleTex);
                SDL_FreeSurface(titleSurf);
            }
        }

        // Fetch Thread-Safe Progress / UI Info
        int currentProgress = g_ProgressPercent.load();
        std::string currentMessage;
        {
            std::lock_guard<std::mutex> lock(g_LoadingMutex);
            currentMessage = g_LoadingMessage;
        }

        // Draw Progress Bar Background
        SDL_Rect barBg = { 50, 220, 500, 15 };
        SDL_SetRenderDrawColor(splashRenderer, barBgColor.r, barBgColor.g, barBgColor.b, barBgColor.a);
        SDL_RenderFillRect(splashRenderer, &barBg);

        // Draw Progress Bar Foreground
        int barWidth = (int)((currentProgress / 100.0f) * 500.0f);
        SDL_Rect barFg = { 50, 220, barWidth, 15 };
        SDL_SetRenderDrawColor(splashRenderer, barFgColor.r, barFgColor.g, barFgColor.b, barFgColor.a);
        SDL_RenderFillRect(splashRenderer, &barFg);

        // Draw Sub Text
        if (subFont) {
            SDL_Surface* txtSurf = TTF_RenderText_Blended(subFont, currentMessage.c_str(), textColor);
            if (txtSurf) {
                SDL_Texture* txtTex = SDL_CreateTextureFromSurface(splashRenderer, txtSurf);
                SDL_Rect txtRect = { 50, 195, txtSurf->w, txtSurf->h }; // Align left to the bar
                SDL_RenderCopy(splashRenderer, txtTex, NULL, &txtRect);
                SDL_DestroyTexture(txtTex);
                SDL_FreeSurface(txtSurf);
            }
        }

        SDL_RenderPresent(splashRenderer);
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup Splash Screen
    loadingThread.join();

    if (titleFont) TTF_CloseFont(titleFont);
    if (subFont) TTF_CloseFont(subFont);
    SDL_DestroyRenderer(splashRenderer);
    SDL_DestroyWindow(splashWindow);
    TTF_Quit();
    // Keep SDL initialized for Application
    
    // ------------------------------------
    // START ENGINE
    // ------------------------------------
    
    // Clear any SDL_QUIT events that were generated by closing the splash window
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_QUIT);

    try {
        auto app = std::make_unique<Archura::Application>(graphicsOptions);
        app->Run();
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
        return -1;
    }

    return 0;
}
