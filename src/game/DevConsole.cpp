#include "DevConsole.h"
#include "../core/Application.h"
#include "../core/AudioSystem.h"
#include "../input/Input.h"
#include "CommandRegistry.h"
#include "../core/DeveloperConsole.h" // Logic System
#include <imgui.h>

namespace Archura {

DevConsole &DevConsole::Get() {
  static DevConsole instance;
  return instance;
}

void DevConsole::Init() {
  // Logic System Output'u UI'ya yonlendir
  DeveloperConsole::GetInstance().AddPrintCallback([this](const std::string& msg) {
      this->Log(msg);
  });

  // Eski CommandRegistry komutlari yerine DeveloperConsole (FPSConsoleCommands) kullaniliyor.
  // Ancak eski komutlari (noclip vs) DeveloperConsole da zaten var.
  // Playsound komutunu DeveloperConsole'a ekleyebiliriz veya burada ozel handle edebiliriz.
  // Simdilik DeveloperConsole ana sistem olacak.

  Log("[Archura] Command palette ready.");
  Log("[Tip] Try 'commands', 'render.stats', 'scene.list' or 'debug.cheats 1'.");
}

void DevConsole::Toggle() { m_IsOpen = !m_IsOpen; }

void DevConsole::Log(const std::string &message) {
  m_Logs.push_back(message);
  m_ScrollToBottom = true;
}

void DevConsole::Render() {
  if (!m_IsOpen)
    return;

  ImGui::SetNextWindowSize(ImVec2(720, 460), ImGuiCond_FirstUseEver);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
  if (!ImGui::Begin("Archura Command Palette", &m_IsOpen)) {
    ImGui::PopStyleVar();
    ImGui::End();
    return;
  }
  ImGui::PopStyleVar();

  ImGui::TextColored(ImVec4(0.56f, 0.78f, 1.0f, 1.0f), "ARCHURA");
  ImGui::SameLine();
  ImGui::TextDisabled("developer surface");
  ImGui::SameLine(ImGui::GetWindowWidth() - 185.0f);
  ImGui::TextDisabled(m_DevMode ? "Developer mode: on" : "Developer mode: off");
  ImGui::Separator();

  if (ImGui::SmallButton("Commands")) {
      DeveloperConsole::GetInstance().ExecuteCommand("commands");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Render Stats")) {
      DeveloperConsole::GetInstance().ExecuteCommand("render.stats");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Debug On")) {
      DeveloperConsole::GetInstance().ExecuteCommand("debug.cheats 1");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Clear")) {
      m_Logs.clear();
  }

  // Output area
  const float footer_height_to_reserve =
      ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() * 2.0f;
  ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve),
                    true, ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto &log : m_Logs) {
    ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    if (log.find("Error") != std::string::npos || log.find("[ERR]") != std::string::npos)
      color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
    else if (log.find("ENABLED") != std::string::npos || log.find("ready") != std::string::npos)
      color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    else if (log.rfind(">", 0) == 0)
      color = ImVec4(0.56f, 0.78f, 1.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(log.c_str());
    ImGui::PopStyleColor();
  }

  if (m_ScrollToBottom || (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
    ImGui::SetScrollHereY(1.0f);
  m_ScrollToBottom = false;

  ImGui::EndChild();

  // Input area
  ImGui::Separator();
  bool reclaim_focus = false;
  ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                    ImGuiInputTextFlags_CallbackCompletion |
                                    ImGuiInputTextFlags_CallbackHistory;

  ImGui::TextDisabled("Command");
  ImGui::SameLine();
  ImGui::TextDisabled("namespace style: render.stats, scene.open, config.save");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputText("##CommandInput", m_InputBuf, IM_ARRAYSIZE(m_InputBuf),
                       input_flags)) {
    std::string cmd = m_InputBuf;
    if (!cmd.empty()) {
      Log("> " + cmd);
      // Main Logic System
      DeveloperConsole::GetInstance().ExecuteCommand(cmd);
      m_InputBuf[0] = '\0';
    }
    reclaim_focus = true;
  }

  ImGui::SetItemDefaultFocus();
  if (reclaim_focus)
    ImGui::SetKeyboardFocusHere(-1);

  if (m_InputBuf[0] != '\0') {
      std::string needle = m_InputBuf;
      auto aliases = DeveloperConsole::GetInstance().GetAliasNames();
      int shown = 0;
      ImGui::TextDisabled("Matches:");
      ImGui::SameLine();
      for (const auto& name : aliases) {
          if (name.find(needle) != std::string::npos) {
              if (shown++ > 0) ImGui::SameLine();
              ImGui::TextColored(ImVec4(0.56f, 0.78f, 1.0f, 1.0f), "%s", name.c_str());
              if (shown >= 4) break;
          }
      }
      if (shown == 0) {
          ImGui::SameLine();
          ImGui::TextDisabled("none");
      }
  }

  ImGui::End();
}

void DevConsole::Bind(int key, const std::string& command) {
    m_KeyBinds[key] = command;
    Log("Bound key " + std::to_string(key) + " to command: " + command);
}

void DevConsole::CheckBinds(Input* input) {
    if (m_IsOpen) return; // Don't trigger binds when console is typing
    
    for (const auto& [key, cmd] : m_KeyBinds) {
        if (input->IsKeyJustPressed(key)) {
            DeveloperConsole::GetInstance().ExecuteCommand(cmd);
        }
    }
}

} // namespace Archura
