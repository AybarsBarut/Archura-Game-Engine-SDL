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
#include "game/GameModeManager.h"
#include "core/GameSaveManager.h"

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
    const char* logPath = "logs/startup_detailed.txt";
    FILE* f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: Started\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    Engine::EngineConfig config;
    config.windowTitle = "Archura FPS Engine - Build 0.2 (SDL2)";
    config.windowWidth = 1920;
    config.windowHeight = 1080;
    config.vsync = false;
    config.fullscreen = false;

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: Calling Engine::Get().Init()\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    if (!Engine::Get().Init(config)) {
        std::cerr << "Failed to initialize engine!" << std::endl;
        f = fopen(logPath, "a");
        if(f) { 
            fprintf(f, "[%dms] APP::Init: Engine::Init() FAILED\n", SDL_GetTicks()); 
            fflush(f);
            fclose(f); 
        }
        return false;
    }

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: Engine initialized successfully\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    m_Window = Engine::Get().GetWindow();

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: Initializing AudioSystem\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    AudioSystem::Get().Init();
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: AudioSystem initialized\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: Initializing NetworkManager\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    NetworkManager::Get().Init();
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: NetworkManager initialized\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: Initializing DevConsole\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    DevConsole::Get().Init();

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: DevConsole initialized\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

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

    CommandRegistry::Get().RegisterCommand(
        "save_game", [](const std::vector<std::string>& args) {
            std::string name = args.empty() ? "QuickSave" :
                               [&]{ std::string r; for (auto& a : args) r += a + " "; if(!r.empty()) r.pop_back(); return r; }();
            Scene* scene = Application::Get().GetScene();
            if (scene) {
                GameSaveManager::Get().SaveProject(name, scene);
                std::cout << "Proje kaydedildi: " << name << std::endl;
            }
        });

    CommandRegistry::Get().RegisterCommand(
        "load_game", [](const std::vector<std::string>& args) {
            if (args.empty()) {
                // Projeleri listele
                GameSaveManager::Get().RefreshProjects();
                auto& projects = GameSaveManager::Get().GetProjects();
                if (projects.empty()) {
                    std::cout << "Kayitli proje bulunamadi!" << std::endl;
                } else {
                    for (const auto& p : projects)
                        std::cout << "  [" << p.entityCount << " obj] " << p.name << " - " << p.timestamp << std::endl;
                }
                return;
            }
            // Dosya yolu veya proje adıyla yükle
            std::string filePath = args[0];
            if (filePath.find(".scene") == std::string::npos)
                filePath = "saves/" + GameSaveManager::MakeSafeFileName(filePath) + ".scene";
            Scene* scene = Application::Get().GetScene();
            if (scene) {
                if (GameSaveManager::Get().LoadProject(filePath, scene))
                    std::cout << "Proje yuklendi: " << filePath << std::endl;
                else
                    std::cout << "Proje yuklenemedi: " << filePath << std::endl;
            }
        });

    CommandRegistry::Get().RegisterCommand(
        "game_mode", [](const std::vector<std::string>& args) {
            if (args.empty()) {
                std::cout << "Mod: " << GameModeManager::Get().GetModeString() << std::endl;
                return;
            }
            auto& gmm = GameModeManager::Get();
            if (args[0] == "sp" || args[0] == "singleplayer") {
                gmm.Disconnect();
            } else if (args[0] == "host") {
                uint16_t port = args.size() > 1 ? (uint16_t)std::stoi(args[1]) : 7777;
                gmm.StartHost("0.0.0.0", port);
            } else if (args[0] == "join") {
                std::string ip  = args.size() > 1 ? args[1] : "127.0.0.1";
                uint16_t port   = args.size() > 2 ? (uint16_t)std::stoi(args[2]) : 7777;
                gmm.ConnectToHost(ip, port);
            } else {
                std::cout << "game_mode <sp|host [port]|join [ip] [port]>" << std::endl;
            }
        });

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: Initializing ImGuiLayer\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    m_ImGuiLayer = std::make_unique<ImGuiLayer>();
    m_ImGuiLayer->Init(m_Window);

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP::Init: ImGuiLayer initialized\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    return true;
}

void Application::Run() {
    const char* logPath = "logs/startup_detailed.txt";
    FILE* f = fopen(logPath, "a"); 
    if(f) { 
        fprintf(f, "[%dms] APP: Run() started\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: Calling Init()\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    if (!Init()) {
        f = fopen(logPath, "a");
        if(f) { 
            fprintf(f, "[%dms] APP: Init() FAILED\n", SDL_GetTicks()); 
            fflush(f);
            fclose(f); 
        }
        fprintf(stderr, "Application::Init Failed\n");
        return;
    }
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: Init() completed successfully\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: Creating Scene\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    m_Scene = std::make_unique<Scene>("Demo Scene");
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: Scene created\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    // Initialize Camera as member variable
    m_Camera = std::make_unique<Camera>(glm::vec3(0.0f, 5.0f, 10.0f));

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: Creating FPSController\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    m_FPSController = std::make_unique<FPSController>(m_Camera.get());
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: FPSController created\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: Initializing RenderSystem\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }
    
    m_RenderSystem = std::make_unique<RenderSystem>(m_Camera.get());
    m_RenderSystem->Init(m_Scene.get());
    
    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: RenderSystem initialized\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

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

    // Initialize HUDRenderer as member
    m_HUDRenderer = std::make_unique<HUDRenderer>();
    m_HUDRenderer->Init();

    // Initialize Editor as member
    m_Editor = std::make_unique<Editor>();
    m_Editor->Init(m_Window);
    m_Editor->SetEnabled(true);

    // ── Play / Stop callbacks ────────────────────────────────────────────
    m_Editor->RegisterOnPlay([this]() {
        m_Input->SetCursorMode(2);
        m_IsPaused = false;
        std::cout << "[Editor] Entered Play mode" << std::endl;
    });
    m_Editor->RegisterOnStop([this]() {
        m_Input->SetCursorMode(0);
        std::cout << "[Editor] Returned to Edit mode" << std::endl;
    });

    // Create Player entity (store as member)
    m_Player = m_Scene->CreateEntity("Player");
    auto* weapon = m_Player->AddComponent<Weapon>();
    weapon->InitInventory();
    auto* playerHealth = m_Player->AddComponent<Health>();
    playerHealth->max = 100.0f;
    playerHealth->current = 100.0f;

    m_PhysicsSystem = std::make_unique<PhysicsSystem>();
    m_PhysicsSystem->Init(m_Scene.get());
    m_ScriptSystem = std::make_unique<ScriptSystem>();
    m_ScriptSystem->Init(m_Scene.get());
    m_ParticleSystem = std::make_unique<ParticleSystem>();
    m_ParticleSystem->Init(m_Scene.get());
    m_ProjectileSystem = std::make_unique<ProjectileSystem>();
    m_ProjectileSystem->Init(m_Scene.get());

    f = fopen(logPath, "a");
    if(f) { 
        fprintf(f, "[%dms] APP: All systems initialized. Entering main loop\n", SDL_GetTicks()); 
        fflush(f);
        fclose(f); 
    }

    m_EngineWindow = Engine::Get().GetWindow();
    m_Input = Engine::Get().GetInput();
    m_Renderer = Engine::Get().GetRenderer();

    // --- SETUP ROBUST MAP ---
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
        col->size = w.s;
        col->isTrigger = false;
    }

    // Initialize PauseMenu as member
    m_PauseMenu = std::make_unique<PauseMenu>();

    // Set initial cursor mode: free in editor, locked in Play mode
    m_Input->SetCursorMode(m_DevModeActive ? 0 : 2);


    std::cout << "DEBUG: Window ShouldClose: " << m_EngineWindow->ShouldClose() << ", Running: " << m_Running << std::endl;

    // Initialize timing for fixed timestep
    m_LastFrameTime = SDL_GetTicks() / 1000.0f;
    m_Accumulator = 0.0f;
    m_TickCount = 0;

    std::cout << "Starting 128 tickrate main loop..." << std::endl;

    try {
        while (!m_EngineWindow->ShouldClose() && m_Running) {
            // Calculate frame time
            float currentTime = SDL_GetTicks() / 1000.0f;
            float frameTime = currentTime - m_LastFrameTime;
            m_LastFrameTime = currentTime;
            
            // Cap frame time to prevent spiral of death
            if (frameTime > 0.25f) {
                frameTime = 0.25f;  // Max 4 FPS minimum
            }
            
            m_Accumulator += frameTime;
            
            // Fixed timestep update loop (128 Hz)
            // Process input and events (ONCE PER FRAME)
            ProcessInput();

            // Fixed timestep update loop (128 Hz)
            int ticksThisFrame = 0;
            while (m_Accumulator >= TICK_INTERVAL && ticksThisFrame < MAX_TICKS_PER_FRAME) {
                
                // Update game logic with fixed timestep
                UpdateGameLogic(TICK_INTERVAL);

                m_Accumulator -= TICK_INTERVAL;
                ticksThisFrame++;
                m_TickCount++;
            }
            
            // Calculate interpolation alpha for smooth rendering
            float alpha = static_cast<float>(m_Accumulator / TICK_INTERVAL);
            
            // Render with interpolation
            RenderFrame(alpha);
            
            // FPS Counter (every second)
            m_FrameCount++;
            if (currentTime - m_LastFPSUpdateTime >= 1.0f) {
                m_CurrentFPS = static_cast<float>(m_FrameCount / (currentTime - m_LastFPSUpdateTime));
                // std::cout << "FPS: " << m_CurrentFPS << " | Ticks: " << m_TickCount << std::endl;
                m_FrameCount = 0;
                m_LastFPSUpdateTime = currentTime;
            }
        }
        std::cout << "DEBUG: Exited Loop. ShouldClose: " << m_EngineWindow->ShouldClose() << ", Running: " << m_Running << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown Exception" << std::endl;
    }
    std::cout << "DEBUG: Exiting Application::Run" << std::endl;
}

void Application::ProcessInput() {
    // 1. Poll Events
    m_Input->Update();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        m_Input->OnEvent(event);
        if (event.type == SDL_QUIT) 
            m_EngineWindow->SetShouldClose(true);
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) 
            m_EngineWindow->SetShouldClose(true);
    }

    // 2. Handle special input (pause, dev mode, console)
    if (m_Input->IsKeyJustPressed(SDL_SCANCODE_TAB)) {
        bool entering = !m_DevModeActive;
        SetDevMode(entering);
        // In editor mode: free cursor. In play-through: respect game state.
        if (entering) {
            m_Input->SetCursorMode(0); // Free – ImGui usable
        } else {
            m_Input->SetCursorMode(m_IsPaused ? 0 : 2);
        }
    }
    if (m_Input->IsKeyJustPressed(SDL_SCANCODE_ESCAPE)) {
        if (!m_DevModeActive || (m_Editor && m_Editor->GetMode() == EditorMode::Play)) {
            m_IsPaused = !m_IsPaused;
            m_Input->SetCursorMode(m_IsPaused ? 0 : 2);
        }
    }
    if (m_Input->IsKeyJustPressed(SDL_SCANCODE_GRAVE)) {
        DevConsole::Get().Toggle();
    }

    // 3. Mouse / Camera look routing
    const bool inEditMode = m_DevModeActive &&
                            m_Editor &&
                            m_Editor->GetMode() == EditorMode::Edit;
    if (inEditMode) {
        // Editor fly-cam: update reads Input directly inside EditorCamera::Update
        float frameDt = ImGui::GetIO().DeltaTime;
        if (frameDt <= 0.0f) frameDt = 0.016f;
        m_Editor->GetEditorCamera()->Update(m_Input, frameDt);

        // --- Gizmo input (runs BEFORE picking so handle drags take priority) ---
        int w = 0, h = 0;
        if (m_Window) {
            w = static_cast<int>(m_Window->GetWidth());
            h = static_cast<int>(m_Window->GetHeight());
        }
        m_Editor->ProcessGizmoInput(m_Input, w, h);

        // --- Mouse picking: LMB selects entity (skipped while gizmo is active) ---
        if (!m_Editor->IsGizmoInteracting()) {
            Entity* picked = m_Editor->PickEntityAtScreenPos(m_Scene.get(), m_Input, w, h);
            if (picked) m_Editor->SetSelectedEntity(picked);
        }

        // --- Keyboard Shortcuts (Undo, Copy/Paste, Focus) ---
        m_Editor->ProcessEditorShortcuts(m_Input, m_Scene.get());
    } else if (!m_IsPaused && m_FPSController) {
        m_FPSController->HandleMouseLook(m_Input, 0.0f);
    }
}

void Application::UpdateGameLogic(float dt) {
    if (!m_IsPaused) {
        m_FPSController->Update(m_Input, m_Scene.get(), dt, m_ProjectileSystem.get());
        m_ProjectileSystem->Update(dt);
        m_PhysicsSystem->Update(dt);
        m_ScriptSystem->Update(dt);
        m_ParticleSystem->Update(dt);
        AudioSystem::Get().Update(m_Scene.get(), m_Camera.get());
    }
}

void Application::RenderFrame(float /*alpha*/) {
    m_Renderer->BeginFrame();
    m_ImGuiLayer->BeginFrame();

    // If editor is active and in Edit mode, inject the editor camera view/proj
    // so the 3D viewport shows the fly-cam perspective, not the game camera.
    const bool editorEditMode = m_DevModeActive &&
                                m_Editor &&
                                m_Editor->GetMode() == EditorMode::Edit;
    if (editorEditMode) {
        EditorCamera* ec = m_Editor->GetEditorCamera();
        const float aspect = (m_Window && m_Window->GetHeight() > 0)
            ? m_Window->GetAspectRatio() : 1.777f;
        m_RenderSystem->SetViewOverride(
            ec->GetViewMatrix(),
            ec->GetProjectionMatrix(aspect));
    } else {
        m_RenderSystem->ClearViewOverride();
    }

    m_RenderSystem->Update(TICK_INTERVAL);

    if (m_DevModeActive) {
        m_Editor->BeginDockSpace();
        m_Editor->DrawMenuBar(m_Scene.get());
        m_Editor->DrawEditorUI(m_Scene.get());
        m_Editor->DrawOverlay(m_Scene.get(), m_Camera.get());

        // Draw 3-D transform gizmo when an entity is selected in Edit mode
        if (editorEditMode && m_Window) {
            m_Editor->DrawTransformGizmo(
                static_cast<int>(m_Window->GetWidth()),
                static_cast<int>(m_Window->GetHeight()));
        }
    }
    
    DevConsole::Get().Render();
    m_PauseMenu->Render(m_IsPaused, *m_FPSController, *m_EngineWindow, m_Scene.get());

    m_ImGuiLayer->EndFrame();
    m_Renderer->EndFrame();
    
    m_EngineWindow->Update();
    m_Input->EndFrame();
}

} // namespace Archura

