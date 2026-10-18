#include "game/DevConsole.h"
#include "core/DeveloperConsole.h"
#include <imgui_internal.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    ImGui::CreateContext();
    try {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1280, 720);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        auto& palette = Archura::DevConsole::Get();
        palette.Init();
        palette.Toggle();
        auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiCond_Always);
            palette.Render();
            ImGui::Render();
        };
        frame();
        frame();
        auto* window = ImGui::FindWindowByName("Archura Command Palette");
        Require(window != nullptr, "palette window missing");
        const ImGuiID inputID = window->GetID("##CommandInput");
        // InputText is the last line submitted in this window. End() replaces
        // LastItemData with the title bar, so use the window's last line here.
        const ImVec2 center(window->WorkRect.GetCenter().x,
            window->DC.CursorPosPrevLine.y + ImGui::GetFrameHeight() * 0.5f);
        io.AddMousePosEvent(center.x, center.y);
        io.AddMouseButtonEvent(0, true);
        frame(); // Formerly asserted as soon as the input became active.
        Require(GImGui->ActiveId == inputID, "click did not activate command input");
        io.AddMouseButtonEvent(0, false);
        frame();
        io.AddInputCharactersUTF8("debug.cheats 1");
        frame();
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame();
        Require(Archura::DeveloperConsole::GetInstance().AreCheatsEnabled(),
                "Enter did not execute the typed command");
        io.AddKeyEvent(ImGuiKey_Enter, false);
        frame();
        const auto* state = ImGui::GetInputTextState(inputID);
        Require(state && state->CurLenA == 0, "submitted command was not cleared");
        palette.Toggle();
        ImGui::DestroyContext();
        std::cout << "Console click, typing, submission and clearing passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ImGui::DestroyContext();
        return 1;
    }
}
