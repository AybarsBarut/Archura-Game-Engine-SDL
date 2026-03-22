#include "../core/Application.h"
#include "PauseMenu.h"
#include "../../external/imgui/imgui.h" // Relative path
#include "../network/NetworkManager.h"
#include "../core/Window.h"
#include "../game/FPSController.h"
#include "../game/GameModeManager.h"
#include "../core/GameSaveManager.h"
#include "../ecs/Entity.h"

#include <string>
#include <iostream>
#include <vector>

namespace Archura {

const char* PauseMenu::GetKeyName(int scancode) {
    // Uses SDL Scancode values (SDL_SCANCODE_*)
    // These are hardware-position codes, NOT ASCII or GLFW keycodes
    switch (scancode) {
        // --- Letters (SDL_SCANCODE_A=4 through SDL_SCANCODE_Z=29) ---
        case 4:  return "A";
        case 5:  return "B";
        case 6:  return "C";
        case 7:  return "D";
        case 8:  return "E";
        case 9:  return "F";
        case 10: return "G";
        case 11: return "H";
        case 12: return "I";
        case 13: return "J";
        case 14: return "K";
        case 15: return "L";
        case 16: return "M";
        case 17: return "N";
        case 18: return "O";
        case 19: return "P";
        case 20: return "Q";
        case 21: return "R";
        case 22: return "S";
        case 23: return "T";
        case 24: return "U";
        case 25: return "V";
        case 26: return "W";
        case 27: return "X";
        case 28: return "Y";
        case 29: return "Z";
        // --- Number row (SDL_SCANCODE_1=30 .. SDL_SCANCODE_0=39) ---
        case 30: return "1";
        case 31: return "2";
        case 32: return "3";
        case 33: return "4";
        case 34: return "5";
        case 35: return "6";
        case 36: return "7";
        case 37: return "8";
        case 38: return "9";
        case 39: return "0";
        // --- Control keys ---
        case 40: return "ENTER";
        case 41: return "ESCAPE";
        case 42: return "BACKSPACE";
        case 43: return "TAB";
        case 44: return "SPACE";
        case 45: return "MINUS";
        case 46: return "EQUAL";
        case 47: return "LEFT BRACKET";
        case 48: return "RIGHT BRACKET";
        case 49: return "BACKSLASH";
        case 51: return "SEMICOLON";
        case 52: return "APOSTROPHE";
        case 53: return "GRAVE";
        case 54: return "COMMA";
        case 55: return "PERIOD";
        case 56: return "SLASH";
        case 57: return "CAPS LOCK";
        // --- Function keys ---
        case 58: return "F1";
        case 59: return "F2";
        case 60: return "F3";
        case 61: return "F4";
        case 62: return "F5";
        case 63: return "F6";
        case 64: return "F7";
        case 65: return "F8";
        case 66: return "F9";
        case 67: return "F10";
        case 68: return "F11";
        case 69: return "F12";
        // --- Navigation ---
        case 73: return "INSERT";
        case 74: return "HOME";
        case 75: return "PAGE UP";
        case 76: return "DELETE";
        case 77: return "END";
        case 78: return "PAGE DOWN";
        case 79: return "RIGHT";
        case 80: return "LEFT";
        case 81: return "DOWN";
        case 82: return "UP";
        // --- Numpad ---
        case 83: return "NUM LOCK";
        case 84: return "NUMPAD /";
        case 85: return "NUMPAD *";
        case 86: return "NUMPAD -";
        case 87: return "NUMPAD +";
        case 88: return "NUMPAD ENTER";
        case 89: return "NUMPAD 1";
        case 90: return "NUMPAD 2";
        case 91: return "NUMPAD 3";
        case 92: return "NUMPAD 4";
        case 93: return "NUMPAD 5";
        case 94: return "NUMPAD 6";
        case 95: return "NUMPAD 7";
        case 96: return "NUMPAD 8";
        case 97: return "NUMPAD 9";
        case 98: return "NUMPAD 0";
        case 99: return "NUMPAD .";
        // --- Modifier keys ---
        case 224: return "LEFT CTRL";
        case 225: return "LEFT SHIFT";
        case 226: return "LEFT ALT";
        case 228: return "RIGHT CTRL";
        case 229: return "RIGHT SHIFT";
        case 230: return "RIGHT ALT";
        // --- Extra ---
        case 72: return "PAUSE";
        case 70: return "SCROLL LOCK";
        case 71: return "PRINT SCREEN";
        default: {
            static char buf[16];
            snprintf(buf, sizeof(buf), "[%d]", scancode);
            return buf;
        }
    }
}

    void PauseMenu::Render(bool& isPaused, FPSController& controller, Window& window, Scene* scene) {
        if (!isPaused) {
            m_CurrentState = MenuState::Main; // Reset to main when closed
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.85f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                                 ImGuiWindowFlags_NoMove | 
                                 ImGuiWindowFlags_NoResize | 
                                 ImGuiWindowFlags_NoSavedSettings | 
                                 ImGuiWindowFlags_NoBringToFrontOnFocus;

        if (ImGui::Begin("PauseMenu", nullptr, flags)) {
            
            // Common Center Layout Calculation
            float centerX = io.DisplaySize.x * 0.5f;
            float centerY = io.DisplaySize.y * 0.5f;
            float btnWidth = 300.0f;

            // Title
            ImGui::SetWindowFontScale(3.0f);
            const char* title = "ARCHURA PAUSED";
            if (m_CurrentState == MenuState::Options) title = "OPTIONS";
            if (m_CurrentState == MenuState::Keybinds) title = "CONTROLS";

            float titleW = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPos(ImVec2(centerX - titleW * 0.5f, centerY - 250.0f));
            ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), title);
            ImGui::SetWindowFontScale(1.0f);

            // Menu Content
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            
            // Layout Container - Calculate height approximate to center vertically
            float contentHeight = 400.0f; // Approximate
            ImGui::SetCursorPos(ImVec2(centerX - btnWidth * 0.5f, centerY - contentHeight * 0.4f));
            ImGui::BeginGroup();

            switch (m_CurrentState) {
                case MenuState::Main:
                    RenderMainMenu(isPaused);
                    break;
                case MenuState::Options:
                    RenderOptions(controller, window);
                    break;
                case MenuState::Keybinds:
                    RenderKeybinds(controller);
                    break;
                case MenuState::SaveGame:
                    RenderSaveGame(scene);
                    break;
                case MenuState::LoadGame:
                    RenderLoadGame(scene);
                    break;
                case MenuState::Multiplayer:
                    RenderMultiplayerSetup();
                    break;
            }

            ImGui::EndGroup();
            ImGui::PopStyleVar();
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    void PauseMenu::RenderMainMenu(bool& isPaused) {
        float btnWidth = 300.0f;
        float btnHeight = 50.0f;
        float spacing = 15.0f;

        if (ImGui::Button("OYUNA DEVAM (RESUME)", ImVec2(btnWidth, btnHeight))) {
            isPaused = false;
        }

        ImGui::Dummy(ImVec2(0, spacing));

        if (ImGui::Button("PROJEYI KAYDET (Save Project)", ImVec2(btnWidth, btnHeight))) {
            m_CurrentState = MenuState::SaveGame;
            GameSaveManager::Get().RefreshProjects();
        }

        ImGui::Dummy(ImVec2(0, spacing / 2));

        if (ImGui::Button("PROJEYI YUKLE (Load Project)", ImVec2(btnWidth, btnHeight))) {
            m_CurrentState = MenuState::LoadGame;
            GameSaveManager::Get().RefreshProjects();
        }

        ImGui::Dummy(ImVec2(0, spacing));

        if (ImGui::Button("MULTIPLAYER", ImVec2(btnWidth, btnHeight))) {
            m_CurrentState = MenuState::Multiplayer;
        }

        ImGui::Dummy(ImVec2(0, spacing));

        if (ImGui::Button("SECENEKLER (OPTIONS)", ImVec2(btnWidth, btnHeight))) {
            m_CurrentState = MenuState::Options;
        }

        ImGui::Dummy(ImVec2(0, spacing));

        if (ImGui::Button("CIKIS (QUIT)", ImVec2(btnWidth, btnHeight))) {
            Application::Get().Quit();
        }
    }

    void PauseMenu::RenderOptions(FPSController& controller, Window& window) {
        float width = 300.0f;
        
        // --- SENSITIVITY ---
        ImGui::Text("Mouse Sensitivity");
        ImGui::SetNextItemWidth(width);
        float sens = controller.GetMouseSensitivity();
        if (ImGui::SliderFloat("##sens", &sens, 0.01f, 1.0f, "%.2f")) {
            controller.SetMouseSensitivity(sens);
        }
        ImGui::Dummy(ImVec2(0, 10));

        // --- RESOLUTION ---
        ImGui::Text("Resolution");
        const char* resolutions[] = { "1280x720", "1600x900", "1920x1080", "2560x1440" };
        static int currentRes = 2; // Default to 1920x1080 check?
        
        // Sync combo with actual window size
        if (window.GetWidth() == 1280) currentRes = 0;
        else if (window.GetWidth() == 1600) currentRes = 1;
        else if (window.GetWidth() == 1920) currentRes = 2;
        else if (window.GetWidth() == 2560) currentRes = 3;

        ImGui::SetNextItemWidth(width);
        if (ImGui::Combo("##res", &currentRes, resolutions, IM_ARRAYSIZE(resolutions))) {
            switch (currentRes) {
                case 0: window.SetResolution(1280, 720); break;
                case 1: window.SetResolution(1600, 900); break;
                case 2: window.SetResolution(1920, 1080); break;
                case 3: window.SetResolution(2560, 1440); break;
            }
        }
        ImGui::SameLine();
        bool fullscreen = window.IsFullscreen();
        if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
            window.SetFullscreen(fullscreen);
        }

        ImGui::Dummy(ImVec2(0, 10));

        // --- NETWORK (Game Host) ---
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Game Host / Network");
        
        static int port = 27015;
        static char ip[32] = "127.0.0.1";

        ImGui::Text("Port");
        ImGui::SetNextItemWidth(width);
        ImGui::InputInt("##port", &port, 0); // STEP=0 removes +/- buttons for cleaner look, or keep them? 
                                             // Default is 1, 100. Let's keep formatted input or default. 
                                             // User asked to shorten. InputInt defaults with buttons.
        
        if (ImGui::Button("HOST GAME", ImVec2(width, 30))) {
            NetworkManager::Get().StartServer(port);
        }

        if (ImGui::Button("STOP HOSTING", ImVec2(width, 30))) {
            NetworkManager::Get().Shutdown();
        }

        ImGui::Text("IP Address");
        ImGui::SetNextItemWidth(width);
        ImGui::InputText("##ip", ip, 32);

        if (ImGui::Button("JOIN GAME", ImVec2(width, 30))) {
            NetworkManager::Get().Connect(ip, port);
        }
        
        // Status
        if (NetworkManager::Get().IsServer()) ImGui::TextColored(ImVec4(0,1,0,1), "Hosting on port %d", port);
        if (NetworkManager::Get().IsConnected() && !NetworkManager::Get().IsServer()) ImGui::TextColored(ImVec4(0,1,0,1), "Connected to server");

        ImGui::Dummy(ImVec2(0, 20));
        
        // --- BUTTONS ---
        if (ImGui::Button("KEY BINDINGS", ImVec2(width, 40))) {
            m_CurrentState = MenuState::Keybinds;
        }

        ImGui::Dummy(ImVec2(0, 10));

        if (ImGui::Button("BACK", ImVec2(width, 40))) {
            m_CurrentState = MenuState::Main;
        }
    }

    void PauseMenu::RenderKeybinds(FPSController& controller) {
        // Section header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.80f, 1.0f, 1.0f));
        ImGui::Text("Key Bindings Configuration");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));

        KeyBindings& binds = controller.GetKeyBindings();

        const float labelW = 110.0f;
        const float keyBtnW = 190.0f;
        const float rowH = 32.0f;

        // Table-style header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Text("%-12s", "Action");
        ImGui::SameLine(labelW);
        ImGui::Text("Key");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        // Key button style – muted blue
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.30f, 0.48f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.42f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.52f, 0.78f, 1.0f));

        struct BindRow { const char* label; int key; };
        BindRow rows[] = {
            { "Forward",  binds.forward  },
            { "Backward", binds.backward },
            { "Left",     binds.left     },
            { "Right",    binds.right    },
            { "Jump",     binds.jump     },
            { "Sprint",   binds.sprint   },
        };

        for (auto& row : rows) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(row.label);
            ImGui::SameLine(labelW);
            // Non-interactive display button (click-to-rebind could be added later)
            char btnId[64];
            snprintf(btnId, sizeof(btnId), "%s##bind_%s", GetKeyName(row.key), row.label);
            ImGui::Button(btnId, ImVec2(keyBtnW, rowH));
            ImGui::Spacing();
        }

        ImGui::PopStyleColor(3);
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));

        if (ImGui::Button("BACK", ImVec2(300, 40))) {
            m_CurrentState = MenuState::Options;
        }
    }

    void PauseMenu::RenderSaveGame(Scene* scene) {
        float btnWidth = 380.0f;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "PROJEYI KAYDET");
        ImGui::TextDisabled("Sahnenizi bir proje dosyasi olarak kaydedin.");
        ImGui::TextDisabled("Kaydedilen: tum entity'ler, texture'lar, isimleri ve pozisyonlari.");
        ImGui::TextDisabled("NOT: Floor, Wall, Player gibi sistem entity'leri kaydedilmez.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto& mgr = GameSaveManager::Get();

        // Proje adı giriş alanı
        ImGui::Text("Proje Adi:");
        ImGui::SetNextItemWidth(btnWidth);
        ImGui::InputText("##projname", m_ProjectNameBuf, sizeof(m_ProjectNameBuf));

        if (strlen(m_ProjectNameBuf) == 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(" (bos birakilirsa 'Unnamed' olur)");
        }

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.1f, 0.6f, 0.1f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.8f, 0.2f, 1.f));
        if (ImGui::Button("KAYDET", ImVec2(btnWidth, 50))) {
            if (scene) {
                std::string name = (strlen(m_ProjectNameBuf) > 0)
                    ? std::string(m_ProjectNameBuf)
                    : "Unnamed";
                mgr.SaveProject(name, scene);
                memset(m_ProjectNameBuf, 0, sizeof(m_ProjectNameBuf));
            }
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Mevcut kayıtlı projeleri listele
        const auto& projects = mgr.GetProjects();
        if (!projects.empty()) {
            ImGui::TextDisabled("Mevcut kayitli projeler:");
            for (const auto& proj : projects) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.f), "  %s", proj.name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled(" - %s", proj.timestamp.c_str());
            }
        } else {
            ImGui::TextDisabled("Henuz kayitli proje yok.");
        }

        ImGui::Spacing();
        ImGui::Dummy(ImVec2(0, 10));
        if (ImGui::Button("GERI (BACK)", ImVec2(btnWidth, 40))) {
            m_CurrentState = MenuState::Main;
        }
    }

    void PauseMenu::RenderLoadGame(Scene* scene) {
        float btnWidth = 380.0f;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "PROJEYI YUKLE");
        ImGui::TextDisabled("Kayitli bir projeyi sahneye yukleyin.");
        ImGui::TextDisabled("Mevcut kullanici entity'leri silinir ve proje entity'leri eklenir.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto& mgr = GameSaveManager::Get();
        if (ImGui::Button("Listeyi Yenile", ImVec2(btnWidth, 30))) {
            mgr.RefreshProjects();
        }
        ImGui::Spacing();

        const auto& projects = mgr.GetProjects();

        if (projects.empty()) {
            ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1.f), "Kayitli proje bulunamadi.");
            ImGui::TextDisabled("'Projeyi Kaydet' ile once bir proje kaydedin.");
        } else {
            // Kaydırılabilir liste
            ImGui::BeginChild("##projlist", ImVec2(btnWidth, 300), true);
            for (const auto& proj : projects) {
                ImGui::PushID(proj.filePath.c_str());

                // Proje butonu
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.4f, 0.7f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.9f, 1.f));

                std::string btnLabel = proj.name + "  [" + std::to_string(proj.entityCount) + " obje]";
                if (ImGui::Button(btnLabel.c_str(), ImVec2(btnWidth - 50, 48))) {
                    if (mgr.LoadProject(proj.filePath, scene)) {
                        m_CurrentState = MenuState::Main;
                    }
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine();

                // Silme butonu
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.7f, 0.1f, 0.1f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.f));
                if (ImGui::Button("X", ImVec2(36, 48))) {
                    mgr.DeleteProject(proj.filePath);
                }
                ImGui::PopStyleColor(2);

                // Alt bilgi
                ImGui::TextDisabled("  %s", proj.timestamp.c_str());
                ImGui::Spacing();

                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::Spacing();
        if (ImGui::Button("GERI (BACK)", ImVec2(btnWidth, 40))) {
            m_CurrentState = MenuState::Main;
        }
    }

    void PauseMenu::RenderMultiplayerSetup() {
        float width = 300.0f;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "MULTIPLAYER KURULUM");
        ImGui::Separator();
        ImGui::Spacing();

        auto& gmm = GameModeManager::Get();
        ImGui::Text("Mevcut Mod: ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", gmm.GetModeString());
        ImGui::Spacing();

        // Single Player
        bool isSP = gmm.GetMode() == GameMode::SinglePlayer;
        if (isSP) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.1f, 1.f));
        if (ImGui::Button("TEK OYUNCU (Single Player)", ImVec2(width, 44))) {
            gmm.Disconnect();
        }
        if (isSP) ImGui::PopStyleColor();

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // IP & Port
        ImGui::Text("IP Adresi:");
        ImGui::SetNextItemWidth(width);
        ImGui::InputText("##mp_ip", gmm.m_HostIP, sizeof(gmm.m_HostIP));

        ImGui::Text("Port:");
        int portVal = (int)gmm.m_Port;
        ImGui::SetNextItemWidth(width);
        if (ImGui::InputInt("##mp_port", &portVal, 1, 100)) {
            if (portVal < 1024) portVal = 1024;
            if (portVal > 65535) portVal = 65535;
            gmm.m_Port = (uint16_t)portVal;
        }

        ImGui::Spacing();

        // Host
        bool isHost = gmm.GetMode() == GameMode::MultiplayerHost;
        if (isHost) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.3f, 0.7f, 1.f));
        if (ImGui::Button("SUNUCU BASLAT (Host)", ImVec2(width, 44))) {
            gmm.StartHost(gmm.m_HostIP, gmm.m_Port);
        }
        if (isHost) {
            ImGui::PopStyleColor();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Hosting on port %d", (int)gmm.m_Port);
            if (ImGui::Button("SUNUCUYU DURDUR", ImVec2(width, 34))) {
                gmm.Disconnect();
            }
        }

        ImGui::Spacing();

        // Join
        bool isClient = gmm.GetMode() == GameMode::MultiplayerClient;
        if (isClient) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.5f, 1.f));
        if (ImGui::Button("OYUNA KATIL (Join)", ImVec2(width, 44))) {
            gmm.ConnectToHost(gmm.m_HostIP, gmm.m_Port);
        }
        if (isClient) {
            ImGui::PopStyleColor();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Baglandi: %s:%d", gmm.m_HostIP, (int)gmm.m_Port);
            if (ImGui::Button("BAGLANTIY\xC4\xB0 KES", ImVec2(width, 34))) {
                gmm.Disconnect();
            }
        }

        ImGui::Dummy(ImVec2(0, 20));
        if (ImGui::Button("GERI (BACK)", ImVec2(width, 40))) {
            m_CurrentState = MenuState::Main;
        }
    }

}
