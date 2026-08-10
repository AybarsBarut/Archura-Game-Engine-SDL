#include "Engine.h"
#include "Window.h"
#include "../rendering/Renderer.h"
#include "../input/Input.h"
#include <iostream>

namespace Archura {

Engine& Engine::Get() {
    static Engine instance;
    return instance;
}

bool Engine::Init(const EngineConfig& config) {
    const char* logPath = "logs/startup_detailed.txt";
    FILE* f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Init started\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    m_EditorMode = config.editorMode;

    // Pencere olustur
    Window::WindowProps windowProps(
        config.windowTitle,
        config.windowWidth,
        config.windowHeight,
        config.vsync,
        config.fullscreen
    );
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Creating Window\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    m_Window = std::make_unique<Window>(windowProps);
    if (!m_Window->IsValid()) {
        std::cerr << "Failed to create window!" << std::endl;
        f = fopen(logPath, "a");
        if(f) { 
            fprintf(f, "[%dms] ENGINE: Window creation FAILED\n", SDL_GetTicks()); 
            fflush(f);
            fclose(f); 
        }
        m_Window.reset();
        return false;
    }

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Window created successfully\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    // Giris sistemi
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Creating Input system\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    m_Input = std::make_unique<Input>(m_Window->GetNativeWindow());

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Input system created\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    // Goruntu sistemi
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Initializing Renderer\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    m_Renderer = std::make_unique<Renderer>();
    if (!m_Renderer->Init()) {
        std::cerr << "Failed to initialize renderer!" << std::endl;
        f = fopen(logPath, "a");
        if(f) { 
            fprintf(f, "[%dms] ENGINE: Renderer initialization FAILED\n", SDL_GetTicks()); 
            fflush(f);
            fclose(f); 
        }
        m_Renderer.reset();
        m_Input.reset();
        m_Window.reset();
        return false;
    }

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Renderer initialized successfully\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    m_Running = true;

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] ENGINE: Init completed successfully\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    return true;
}

void Engine::Run() {


    // Ana oyun dongusu
    while (m_Running && !m_Window->ShouldClose()) {
        float deltaTime = m_Window->GetDeltaTime();
        
        // Update
        m_Input->Update();
        Update(deltaTime);

        // Render
        Render();

        // Window update
        m_Window->Update();

        // ESC tusu ile cikis
        if (m_Input->IsKeyPressed(SDL_SCANCODE_ESCAPE)) {
            m_Running = false;
        }
    }


}

void Engine::Shutdown() {
    if (m_Renderer) {
        m_Renderer->Shutdown();
    }
    
    m_Input.reset();
    m_Renderer.reset();
    m_Window.reset();
    
    m_Running = false;

}

void Engine::Update(float deltaTime) {
    // Her karede FPS bilgisini goster (konsolda spam yapmamak icin 1 saniyede bir)
    static float fpsTimer = 0.0f;
    fpsTimer += deltaTime;
    
    if (fpsTimer >= 1.0f) {
        // FPS logging removed
        fpsTimer = 0.0f;
    }

    // Burada oyun mantigi guncellemeleri yapilacak
    // - ECS update
    // - Physics update
    // - Animation update
    // vs...
}

void Engine::Render() {
    m_Renderer->BeginFrame();
    
    // Burada goruntu islemleri yapilacak
    // - Scene rendering
    // - Editor UI (ImGui)
    // vs...
    
    m_Renderer->EndFrame();
}

} // namespace Archura
