#ifndef NOMINMAX
#define NOMINMAX
#endif

// 1. System Includes (Windows/SDL first)
#include <SDL.h>

// 2. Output cleanup
#if defined(near)
#undef near
#endif
#if defined(far)
#undef far
#endif

// 3. GLM Includes (Must be clean of 'near'/'far')
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// 4. Core Engine Includes
#include "core/Application.h"
#include "core/AudioSystem.h"
#include "core/Engine.h"
#include "core/ImGuiLayer.h"
#include "core/Window.h"

// 5. ECS & Game Systems
#include "ecs/Component.h"
#include "ecs/Entity.h"

#include "editor/Editor.h"

#include "game/CommandRegistry.h"
#include "game/DevConsole.h"
#include "game/FPSController.h"
#include "game/ParticleSystem.h"
#include "game/PauseMenu.h"
#include "game/PhysicsSystem.h"
#include "game/Projectile.h"
#include "game/ProjectileSystem.h"
#include "game/RenderSystem.h"
#include "game/ScriptSystem.h"
#include "game/Weapon.h"
#include "game/FPSConsoleCommands.h"

#include "input/Input.h"
#include "network/NetworkManager.h"

#include "rendering/Camera.h"
#include "rendering/HUDRenderer.h"
#include "rendering/Mesh.h"
#include "rendering/Renderer.h"
#include "rendering/Skybox.h"

// 6. Third Party (ImGui)
#include <imgui_impl_sdl2.h>
#include <iostream>
#include <string>

namespace Archura {

Application* Application::s_Instance = nullptr;

Application::Application() {
    s_Instance = this;
}

Application::~Application() {
}

void Application::SetSensitivity(float sens) {
    if (m_FPSController) {
        m_FPSController->SetMouseSensitivity(sens);
        std::cout << "Sensitivity set to " << sens << std::endl;
    }
}

void Application::SetDevMode(bool enabled) {
    m_DevModeActive = enabled;
}

bool Application::Init() {
    Engine::EngineConfig config;
    config.windowTitle = "Archura FPS Engine - Build 0.2 (SDL2)";
    config.windowWidth = 1920;
    config.windowHeight = 1080;
    config.vsync = false;
    config.fullscreen = false;

    if (!Engine::Get().Init(config)) {
        std::cerr << "Failed to initialize engine!" << std::endl;
        return false;
    }

    FILE* f = fopen("logs/startup_log.txt", "a");
    if(f) { fprintf(f, "Engine Initialized. Initializing Subsystems...\n"); fclose(f); }

    m_Window = Engine::Get().GetWindow();

    if((f = fopen("logs/startup_log.txt", "a"))) { fprintf(f, "Initializing AudioSystem...\n"); fclose(f); }
    AudioSystem::Get().Init();
    
    if((f = fopen("logs/startup_log.txt", "a"))) { fprintf(f, "Initializing NetworkManager...\n"); fclose(f); }
    NetworkManager::Get().Init();
    
    if((f = fopen("logs/startup_log.txt", "a"))) { fprintf(f, "Initializing DevConsole...\n"); fclose(f); }
    DevConsole::Get().Init();

    if((f = fopen("logs/startup_log.txt", "a"))) { fprintf(f, "Registering Commands...\n"); fclose(f); }
    FPSConsoleCommands::RegisterAllCommands();
    CommandRegistry::Get().RegisterCommand(
        "fps_limit", [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                float limit = std::stof(args[0]);
                Application::Get().SetFPSLimit(limit);
            }
        });

    CommandRegistry::Get().RegisterCommand(
        "sensitivity", [](const std::vector<std::string>& args) {
            if (!args.empty()) {
                Application::Get().SetSensitivity(std::stof(args[0]));
            }
        });

    CommandRegistry::Get().RegisterCommand(
        "debug_mode", [](const std::vector<std::string>& args) {
             if (!args.empty()) {
                try {
                    int mode = std::stoi(args[0]);
                    Application::Get().SetDevMode(mode != 0);
                    std::cout << "Debug Mode set to: " << (mode != 0 ? "ON" : "OFF") << std::endl;
                } catch (...) {
                    std::cout << "Invalid argument for debug_mode" << std::endl;
                }
             } else {
                 bool current = Application::Get().IsDevMode();
                 Application::Get().SetDevMode(!current);
                 std::cout << "Debug Mode toggled to: " << (!current ? "ON" : "OFF") << std::endl;
             }
        });

    CommandRegistry::Get().RegisterCommand(
        "bind", [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                DevConsole::Get().Log("Usage: bind <key_id> <command>");
                return;
            }
            try {
                int key = std::stoi(args[0]);
                std::string command = args[1];
                for (size_t i = 2; i < args.size(); ++i)
                    command += " " + args[i];

                DevConsole::Get().Bind(key, command);
            } catch (...) {
                DevConsole::Get().Log("Invalid key ID");
            }
        });

    // Gamma/Brightness commands removed/disabled for SDL2 migration simplicity
    /*
    CommandRegistry::Get().RegisterCommand("gamma", ...);
    CommandRegistry::Get().RegisterCommand("brightness", ...);
    */

    CommandRegistry::Get().RegisterCommand(
        "windowed", [](const std::vector<std::string>&) {
             auto& window = Application::Get().GetWindow();
             window.SetFullscreen(false);
        });

    CommandRegistry::Get().RegisterCommand(
        "fullscreen", [](const std::vector<std::string>&) {
             auto& window = Application::Get().GetWindow();
             window.SetFullscreen(true);
        });

    CommandRegistry::Get().RegisterCommand(
        "quit", [](const std::vector<std::string>&) {
            Application::Get().Quit();
        });

    m_ImGuiLayer = std::make_unique<ImGuiLayer>();
    m_ImGuiLayer->Init(m_Window);

    return true;
}

void Application::Run() {
    const char* logPath = "C:/Users/4rchura/Documents/code_snippets/Archura-Game-Engine-SDL/logs/startup_log.txt";
    FILE* f = fopen(logPath, "w"); 
    if(f) { fprintf(f, "Application::Run Started\n"); fclose(f); }
    else { fprintf(stderr, "Failed to open %s\n", logPath); }
    
    printf("DEBUG: Application::Run invoked\n");

    if (!Init()) {
        fprintf(stderr, "Application::Init Failed\n");
        return;
    }
    printf("DEBUG: Application::Init Success\n");

    m_Scene = std::make_unique<Scene>("Demo Scene");
    Camera camera(glm::vec3(0.0f, 5.0f, 10.0f));
    
    printf("DEBUG: Scene Created\n");

    m_FPSController = std::make_unique<FPSController>(&camera);
    
    printf("DEBUG: FPSController Created\n");
    
    m_RenderSystem = std::make_unique<RenderSystem>(&camera);
    m_RenderSystem->Init(m_Scene.get());
    
    printf("DEBUG: RenderSystem Initialized\n");

    // 4. Skybox Entity
    Entity* skyboxEntity = m_Scene->CreateEntity("Skybox");
    auto* skyComp = skyboxEntity->AddComponent<SkyboxComponent>();
    skyComp->facePaths = {
        "assets/skybox/right.jpg",
        "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",
        "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg",
        "assets/skybox/back.jpg"
    };
    skyComp->shouldReload = true;

    printf("DEBUG: Skybox Initialized\n");

    HUDRenderer hudRenderer;
    hudRenderer.Init();
    
    printf("DEBUG: HUDRenderer Initialized\n");

    Editor editor;
    editor.Init(m_Window); 
    editor.SetEnabled(true);
    
    printf("DEBUG: Editor Initialized\n");

    Entity* player = m_Scene->CreateEntity("Player");
    auto* weapon = player->AddComponent<Weapon>();
    weapon->InitInventory();

    printf("DEBUG: Player & Weapon Initialized\n");

    auto* playerHealth = player->AddComponent<Health>();
    playerHealth->max = 100.0f;
    playerHealth->current = 100.0f;

    PhysicsSystem physicsSystem;
    physicsSystem.Init(m_Scene.get());
    
    printf("DEBUG: PhysicsSystem Initialized\n");

    ScriptSystem scriptSystem;
    scriptSystem.Init(m_Scene.get());

    ParticleSystem particleSystem;
    particleSystem.Init(m_Scene.get());

    ProjectileSystem projectileSystem;
    projectileSystem.Init(m_Scene.get());

    printf("DEBUG: All Systems Initialized. Entering Loop.\n");

    // --- SETUP ROBUST MAP (V2) ---
    // 1. Sun
    Entity* light = m_Scene->CreateEntity("Sun");
    auto* lightComp = light->AddComponent<LightComponent>();
    lightComp->type = LightComponent::Type::Directional;
    lightComp->color = glm::vec3(1.0f, 0.98f, 0.9f);
    lightComp->intensity = 1.0f;
    light->GetComponent<Transform>()->rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
    light->GetComponent<Transform>()->position = glm::vec3(0.0f, 50.0f, 0.0f);

    // 1.5 Ambient Light (Fill)
    Entity* ambientLight = m_Scene->CreateEntity("AmbientLight");
    auto* ambientComp = ambientLight->AddComponent<LightComponent>();
    ambientComp->type = LightComponent::Type::Circle;
    ambientComp->color = glm::vec3(0.6f, 0.6f, 0.7f);
    ambientComp->intensity = 0.2f;

    // 2. Floor
    Entity* floor = m_Scene->CreateEntity("Floor");
    {
        auto* mesh = floor->AddComponent<MeshRenderer>();
        mesh->mesh = Mesh::CreateCube();
        mesh->color = glm::vec3(0.4f, 0.4f, 0.45f);

        auto* trans = floor->GetComponent<Transform>();
        trans->position = glm::vec3(0.0f, -1.0f, 0.0f);
        trans->scale = glm::vec3(100.0f, 1.0f, 100.0f);

        auto* col = floor->AddComponent<BoxCollider>();
        col->size = glm::vec3(1.0f, 1.0f, 1.0f);
        col->isTrigger = false;
        col->center = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    // 3. Walls
    float mapSize = 100.0f;
    float wallHeight = 15.0f;
    float wallThick = 2.0f;
    float offset = mapSize * 0.5f;
    float wallY = (wallHeight * 0.5f) - 1.0f;

    struct WallDef { std::string name; glm::vec3 p; glm::vec3 s; glm::vec3 c; };
    std::vector<WallDef> walls = {
        { "Wall_North", {0, wallY, -offset}, {mapSize, wallHeight, wallThick}, {0.8f, 0.4f, 0.4f} },
        { "Wall_South", {0, wallY, offset},  {mapSize, wallHeight, wallThick}, {0.4f, 0.8f, 0.4f} },
        { "Wall_East",  {offset, wallY, 0},  {wallThick, wallHeight, mapSize}, {0.4f, 0.4f, 0.8f} },
        { "Wall_West",  {-offset, wallY, 0}, {wallThick, wallHeight, mapSize}, {0.8f, 0.8f, 0.4f} }
    };

    for(const auto& w : walls) {
        Entity* wall = m_Scene->CreateEntity(w.name);
        auto* mesh = wall->AddComponent<MeshRenderer>();
        mesh->mesh = Mesh::CreateCube();
        mesh->color = w.c;
        wall->GetComponent<Transform>()->position = w.p;
        wall->GetComponent<Transform>()->scale = w.s;
        
        auto* col = wall->AddComponent<BoxCollider>();
        col->size = w.s;  // Collider'ı duvarın gerçek boyutuna ayarla
        col->isTrigger = false;
    }
    // ---------------------------
    


    PauseMenu pauseMenu;

    auto* window = Engine::Get().GetWindow();
    auto* input = Engine::Get().GetInput();
    auto* renderer = Engine::Get().GetRenderer();



    // 0 = Normal, 1 = Hidden, 2 = Locked/Relative
    input->SetCursorMode(2); // Locked



    std::cout << "DEBUG: Window ShouldClose: " << window->ShouldClose() << ", Running: " << m_Running << std::endl;

    try {
        while (!window->ShouldClose() && m_Running) {
            // std::cout << "Loop Tik" << std::endl; // Too spammy, maybe once
            
            float time = (float)SDL_GetTicks() / 1000.0f;
            float deltaTime = time - (float)m_LastFrameTime;
            m_LastFrameTime = time;

            // 1. Poll Events
            input->Update(); 
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                 ImGui_ImplSDL2_ProcessEvent(&event);
                 input->OnEvent(event);
                 if (event.type == SDL_QUIT) window->SetShouldClose(true);
                 if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) window->SetShouldClose(true);
            }

            // [Input Handling Code...]
            if (input->IsKeyJustPressed(SDL_SCANCODE_TAB)) { SetDevMode(!m_DevModeActive); }
            if (input->IsKeyJustPressed(SDL_SCANCODE_ESCAPE)) { m_IsPaused = !m_IsPaused; input->SetCursorMode(m_IsPaused ? 0 : 2); }
            if (input->IsKeyJustPressed(SDL_SCANCODE_GRAVE)) { DevConsole::Get().Toggle(); }

            // 2. Game Logic
            if (!m_IsPaused) {
                m_FPSController->Update(input, m_Scene.get(), deltaTime, &projectileSystem);
                projectileSystem.Update(deltaTime);
                physicsSystem.Update(deltaTime);
                scriptSystem.Update(deltaTime);
                particleSystem.Update(deltaTime);
                AudioSystem::Get().Update(m_Scene.get(), &camera);
            }

            // 3. Rendering
            renderer->BeginFrame(); 
            m_ImGuiLayer->BeginFrame(); 

 
            m_RenderSystem->Update(deltaTime);
            
            if (m_DevModeActive) {
                editor.BeginDockSpace();
                editor.DrawMenuBar(m_Scene.get());
                editor.DrawEditorUI(m_Scene.get());
                editor.DrawOverlay(m_Scene.get(), &camera);
            }
            
            DevConsole::Get().Render();
            pauseMenu.Render(m_IsPaused, *m_FPSController, *window);

            m_ImGuiLayer->EndFrame(); 
            renderer->EndFrame(); 
            
            window->Update(); 
            input->EndFrame(); 
        }
        std::cout << "DEBUG: Exited Loop. ShouldClose: " << window->ShouldClose() << ", Running: " << m_Running << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown Exception" << std::endl;
    }
    std::cout << "DEBUG: Exiting Application::Run" << std::endl;
}

} // namespace Archura
