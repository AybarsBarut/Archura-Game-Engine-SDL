#pragma once

namespace Archura {

    // Forward declarations
    class FPSController;
    class Window;
    class Scene;

    class PauseMenu {
    public:
        PauseMenu() = default;
        ~PauseMenu() = default;

        // Scene* eklendi: save/load için gerekli
        void Render(bool& isPaused, FPSController& controller, Window& window, Scene* scene = nullptr);

    private:
        void RenderMainMenu(bool& isPaused);
        void RenderOptions(FPSController& controller, Window& window);
        void RenderKeybinds(FPSController& controller);
        void RenderSaveGame(Scene* scene);
        void RenderLoadGame(Scene* scene);
        void RenderMultiplayerSetup();

        // Helpers
        const char* GetKeyName(int keycode);

        enum class MenuState {
            Main,
            Options,
            Keybinds,
            SaveGame,
            LoadGame,
            Multiplayer
        };

        MenuState m_CurrentState = MenuState::Main;
        int m_WaitingForKey = -1;

        // Proje kaydetme için isim göriş tamponu
        char m_ProjectNameBuf[128] = "";
    };

}
