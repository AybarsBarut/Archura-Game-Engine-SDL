#include "ImGuiLayer.h"

#include <glad/glad.h>
#include <imgui.h>
#include "../../external/imgui/backends/imgui_impl_sdl2.h"
#include "../../external/imgui/backends/imgui_impl_opengl3.h"
#include <SDL.h>
#include <iostream>

namespace Archura {

    ImGuiLayer::ImGuiLayer() {
    }

    ImGuiLayer::~ImGuiLayer() {
        Shutdown();
    }

    void ImGuiLayer::Init(Window* window) {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;        // Enable Docking
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;      // Enable Multi-Viewport

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        // Note: SDL2_InitForOpenGL expects SDL_Window* and SDL_GLContext
        ImGui_ImplSDL2_InitForOpenGL(window->GetNativeWindow(), window->GetContext());
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void ImGuiLayer::Shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::BeginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::EndFrame() {
        ImGui::Render();

        // Dear ImGui style/vertex colors are already authored for display.
        // Do not run them through the framebuffer's linear-to-sRGB conversion.
        // Preserve the caller's state so a future linear scene pass can still
        // opt into GL_FRAMEBUFFER_SRGB without brightening the editor UI.
        const GLboolean framebufferSrgbWasEnabled =
            glIsEnabled(GL_FRAMEBUFFER_SRGB);
        if (framebufferSrgbWasEnabled) {
            glDisable(GL_FRAMEBUFFER_SRGB);
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (framebufferSrgbWasEnabled) {
            glEnable(GL_FRAMEBUFFER_SRGB);
        }
    }

}
