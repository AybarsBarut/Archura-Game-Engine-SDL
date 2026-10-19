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
#include "core/ResourceManager.h"
#include "core/Window.h"

// 5. ECS & Game Systems
#include "ecs/Component.h"
#include "ecs/Entity.h"

#include "editor/Editor.h"

#include "core/GameSaveManager.h"
#include "core/GameManifest.h"
#include "scripting/ScriptEngine.h"
#include "game/CommandRegistry.h"
#include "game/DevConsole.h"
#include "game/FPSConsoleCommands.h"
#include "game/FPSController.h"
#include "game/GameModeManager.h"
#include "game/GraphicalNetworkSession.h"
#include "game/ParticleSystem.h"
#include "game/PauseMenu.h"
#include "game/PhysicsSystem.h"
#include "game/Projectile.h"
#include "game/ProjectileSystem.h"
#include "game/RenderSystem.h"
#include "game/ScriptSystem.h"
#include "game/Weapon.h"

#ifdef ARCHURA_DEBUG_PHYSICS
#include "systems/DebugPhysicsSystem.h"
#endif

#ifdef ARCHURA_OPENAL
#include "audio/OpenALAudioSystem.h"
#endif

#include "input/Input.h"
#include "network/NetworkManager.h"

#include "rendering/Camera.h"
#include "rendering/HUDRenderer.h"
#include "rendering/Mesh.h"
#include "rendering/Renderer.h"
#include "rendering/Texture.h"

// 6. Third Party (ImGui)
#include "../../external/imgui/backends/imgui_impl_sdl2.h"

#include <cstdio>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <stdexcept>

namespace Archura {

static void LogStartup(const char *message) {
  const char *logPath = "logs/startup_detailed.txt";
  FILE *f = fopen(logPath, "a");
  if (f) {
    fprintf(f, "[%u ms] APP: %s\n", SDL_GetTicks(), message);
    fflush(f);
    fclose(f);
  }
}

Application *Application::s_Instance = nullptr;

Application::Application(GraphicsLaunchOptions graphicsOptions)
    : m_GraphicsOptions(graphicsOptions) {
  s_Instance = this;
}

Application::~Application() {
  LeaveNetworkGame(); // Drop callbacks/GPU network owners before context teardown.
  // A Play snapshot can own the last references to GL assets deleted in Play.
  // Release those owners before Engine::Shutdown destroys the graphics context.
  m_PlaySession.DiscardSnapshot();
  // Every owner that can issue glDelete* must die while the main context is
  // current. Engine::Shutdown destroys that context, so explicit ordering here
  // is part of the renderer lifetime contract.
  m_PauseMenu.reset();
  m_Editor.reset();
#ifdef ARCHURA_DEBUG_PHYSICS
  m_DebugPhysicsSystem.reset();
#endif
#ifdef ARCHURA_OPENAL
  m_OpenALAudioSystem.reset();
#endif
  m_HUDRenderer.reset();
  m_ProjectileSystem.reset();
  m_ParticleSystem.reset();
  m_ScriptSystem.reset();
  m_PhysicsSystem.reset();
  m_RenderSystem.reset();
  m_FPSController.reset();
  m_Scene.reset();
  m_Camera.reset();
  m_ImGuiLayer.reset();
  TextureManager::Get().Clear();
  ResourceManager::Get().Clear();

  m_PlayerHandleValue = 0;
  m_PlayerPersistentID.clear();
  m_Window = nullptr;
  m_EngineWindow = nullptr;
  m_Input = nullptr;
  m_Renderer = nullptr;
  Engine::Get().Shutdown();
  s_Instance = nullptr;
}

void Application::SetSensitivity(float sens) {
  if (m_FPSController) {
    m_FPSController->SetMouseSensitivity(sens);
    std::cout << "Sensitivity set to " << sens << std::endl;
  }
}

void Application::SetFPSLimit(float limit) {
  if (!std::isfinite(limit) || limit < 0.0f) {
    std::cout << "[FPS] Invalid limit. Use 0 to disable or a positive value."
              << std::endl;
    return;
  }

  m_FPSLimit = limit;
  if (m_FPSLimit == 0.0f) {
    std::cout << "[FPS] FPS limit disabled" << std::endl;
  } else {
    std::cout << "[FPS] FPS limit set to " << m_FPSLimit << std::endl;
  }
}

void Application::SetDevMode(bool enabled) { m_DevModeActive = !m_Standalone && enabled; }

bool Application::IsNetworkViewActive() const {
  return m_NetworkGame && GameModeManager::Get().GetMode() == GameMode::MultiplayerClient &&
      (!m_Editor || m_Editor->GetMode() == EditorMode::Play);
}
RenderSystem* Application::GetRenderSystem() {
  return IsNetworkViewActive() ? &m_NetworkGame->GetRenderer() : m_RenderSystem.get();
}
Camera* Application::GetCamera() {
  return IsNetworkViewActive() ? &m_NetworkGame->GetCamera() : m_Camera.get();
}
bool Application::JoinNetworkGame(const std::string& host, int port) {
  if (port < 1 || port > 65535 || host.empty()) return false;
  const bool enteredPlay = m_Editor && m_Editor->GetMode() == EditorMode::Edit;
  if (m_Editor && !m_Editor->SetMode(EditorMode::Play)) return false;
  LeaveNetworkGame();
  if (GameModeManager::Get().ConnectToHost(host, static_cast<std::uint16_t>(port))) return true;
  if (enteredPlay) m_Editor->SetMode(EditorMode::Edit);
  m_IsPaused = true;
  if (m_Input) m_Input->SetCursorMode(0);
  return false;
}
void Application::LeaveNetworkGame() {
  if (m_NetworkGame) {
    NetworkManager::Get().SetOnWorldState({});
    m_NetworkGame.reset();
  }
  m_NetworkSnapshotFailed = false;
  if (GameModeManager::Get().GetMode() == GameMode::MultiplayerClient)
    GameModeManager::Get().Disconnect();
}

bool Application::PumpNetworkGame() {
  auto& network = NetworkManager::Get();
  auto state = network.GetState();
  const bool client = GameModeManager::Get().GetMode() == GameMode::MultiplayerClient;
  if (!client || (state != ConnectionState::Handshaking && state != ConnectionState::Connected)) {
    if (m_NetworkGame) LeaveNetworkGame();
    if (network.IsServer()) network.UpdateServer(); // Legacy engine-host transport.
    else if (!client && (state == ConnectionState::Handshaking || state == ConnectionState::Connected)) network.UpdateClient();
    return false;
  }
  if (!m_NetworkGame) {
    m_NetworkGame = std::make_unique<GraphicalNetworkSession>();
    m_LastNetworkLoggedTick = 0;
    m_NetworkStartedAt = SDL_GetTicks();
    network.SetOnWorldState([this](const auto& bytes) {
      if (!m_NetworkGame || !m_NetworkGame->Apply(bytes, NetworkManager::Get().GetLocalPeerId()))
        m_NetworkSnapshotFailed = true;
    });
  }
  if (state == ConnectionState::Handshaking) {
    m_NetworkGame->ResetConnection(); // Includes server restart with reused peer IDs/ticks.
    m_NetworkStartedAt = SDL_GetTicks();
  }
  network.UpdateClient();
  if (!m_NetworkGame->Snapshot().tick && SDL_GetTicks()-m_NetworkStartedAt > 5000)
    m_NetworkSnapshotFailed = true;
  if (m_NetworkSnapshotFailed || network.GetState() == ConnectionState::Failed) {
    const auto error = network.GetLastError();
    LeaveNetworkGame();
    m_IsPaused = true;
    m_Input->SetCursorMode(0);
    if (m_NetworkLaunch && m_NetworkLaunch->smoke) throw std::runtime_error("Graphical network failed: " + error);
    std::cerr << "[Network] Session ended: invalid/missing arena snapshot or " << error << '\n';
    return true; // Resume local simulation only on a subsequent tick.
  }
  if (network.IsConnected()) {
    const bool active = IsNetworkViewActive() && !m_IsPaused &&
        (m_Input->IsCursorLocked() || (m_NetworkLaunch && m_NetworkLaunch->smoke)) &&
        !DevConsole::Get().IsOpen();
    glm::vec2 axes{};
    if (m_NetworkLaunch && m_NetworkLaunch->smoke) axes = {m_NetworkLaunch->moveX, m_NetworkLaunch->moveZ};
    else {
      const auto& bindings = m_FPSController->GetKeyBindings();
      axes = GraphicalNetworkSession::Movement(m_NetworkGame->GetCamera(),
          float(m_Input->IsKeyDown(bindings.forward))-float(m_Input->IsKeyDown(bindings.backward)),
          float(m_Input->IsKeyDown(bindings.right))-float(m_Input->IsKeyDown(bindings.left)));
    }
    if (!network.SendPlayerInput(m_NetworkGame->Input(axes.x, axes.y, active))) {
      LeaveNetworkGame();
      if (m_NetworkLaunch && m_NetworkLaunch->smoke) throw std::runtime_error("Network input queue failed");
    }
  }
  return true; // Authored scene scripts/physics/weapons are suspended throughout the network session.
}

bool Application::Init() {
  LogStartup("Init: Started");

  Engine::EngineConfig config;
  config.windowTitle = "Archura FPS Engine - Build 0.2 (SDL2)";
  if (m_Standalone) config.windowTitle = m_GameTitle;
  config.editorMode = !m_Standalone;
  config.windowWidth = 1920;
  config.windowHeight = 1080;
  config.vsync = false;
  config.fullscreen = false;
  config.graphicsAPI = m_GraphicsOptions.requestedAPI;
  config.allowGraphicsFallback = m_GraphicsOptions.allowFallback;

  config.fullscreen = false;

  LogStartup("Init: Calling Engine::Get().Init()");

  if (!Engine::Get().Init(config)) {
    std::cerr << "Failed to initialize engine!" << std::endl;
    LogStartup("Init: Engine::Init() FAILED");
    return false;
  }

  LogStartup("Init: Engine initialized successfully");

  m_Window = Engine::Get().GetWindow();

  LogStartup("Init: Initializing AudioSystem");
  AudioSystem::Get().Init();

  LogStartup("Init: AudioSystem initialized");

  LogStartup("Init: Initializing NetworkManager");
  NetworkManager::Get().Init();

  LogStartup("Init: NetworkManager initialized");

  if (m_Standalone) {
    // Gameplay pause UI can use ImGui; no editor, console or authoring commands.
    m_ImGuiLayer = std::make_unique<ImGuiLayer>();
    m_ImGuiLayer->Init(m_Window);
    return true;
  }

  LogStartup("Init: Initializing DevConsole");
  DevConsole::Get().Init();

  LogStartup("Init: DevConsole initialized");

  FILE *f_log = fopen("logs/startup_log.txt", "a");
  if (f_log) {
    fprintf(f_log, "Registering Commands...\n");
    fclose(f_log);
  }
  FPSConsoleCommands::RegisterAllCommands();
  CommandRegistry::Get().RegisterCommand(
      "fps_limit", [](const std::vector<std::string> &args) {
        if (!args.empty()) {
          float limit = std::stof(args[0]);
          Application::Get().SetFPSLimit(limit);
        }
      });

  CommandRegistry::Get().RegisterCommand(
      "sensitivity", [](const std::vector<std::string> &args) {
        if (!args.empty()) {
          Application::Get().SetSensitivity(std::stof(args[0]));
        }
      });

  CommandRegistry::Get().RegisterCommand(
      "debug_mode", [](const std::vector<std::string> &args) {
        if (!args.empty()) {
          try {
            int mode = std::stoi(args[0]);
            Application::Get().SetDevMode(mode != 0);
            std::cout << "Debug Mode set to: " << (mode != 0 ? "ON" : "OFF")
                      << std::endl;
          } catch (...) {
            std::cout << "Invalid argument for debug_mode" << std::endl;
          }
        } else {
          bool current = Application::Get().IsDevMode();
          Application::Get().SetDevMode(!current);
          std::cout << "Debug Mode toggled to: " << (!current ? "ON" : "OFF")
                    << std::endl;
        }
      });

  CommandRegistry::Get().RegisterCommand(
      "bind", [](const std::vector<std::string> &args) {
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
      "windowed", [](const std::vector<std::string> &) {
        auto &window = Application::Get().GetWindow();
        window.SetFullscreen(false);
      });

  CommandRegistry::Get().RegisterCommand(
      "fullscreen", [](const std::vector<std::string> &) {
        auto &window = Application::Get().GetWindow();
        window.SetFullscreen(true);
      });

  CommandRegistry::Get().RegisterCommand(
      "quit",
      [](const std::vector<std::string> &) { Application::Get().Quit(); });

  CommandRegistry::Get().RegisterCommand(
      "save_game", [](const std::vector<std::string> &args) {
        std::string name = args.empty() ? "QuickSave" : [&] {
          std::string r;
          for (auto &a : args)
            r += a + " ";
          if (!r.empty())
            r.pop_back();
          return r;
        }();
        Scene *scene = Application::Get().GetScene();
        if (scene) {
          GameSaveManager::Get().SaveProject(name, scene);
          std::cout << "Proje kaydedildi: " << name << std::endl;
        }
      });

  CommandRegistry::Get().RegisterCommand(
      "load_game", [](const std::vector<std::string> &args) {
        if (args.empty()) {
          // Projeleri listele
          GameSaveManager::Get().RefreshProjects();
          auto &projects = GameSaveManager::Get().GetProjects();
          if (projects.empty()) {
            std::cout << "Kayitli proje bulunamadi!" << std::endl;
          } else {
            for (const auto &p : projects)
              std::cout << "  [" << p.entityCount << " obj] " << p.name << " - "
                        << p.timestamp << std::endl;
          }
          return;
        }
        // Dosya yolu veya proje adıyla yükle
        std::string filePath = args[0];
        if (filePath.find(".scene") == std::string::npos)
          filePath =
              "saves/" + GameSaveManager::MakeSafeFileName(filePath) + ".scene";
        Scene *scene = Application::Get().GetScene();
        if (scene) {
          Application &app = Application::Get();
          if (app.m_Editor && app.m_Editor->GetMode() == EditorMode::Play) {
            if (!app.m_Editor->SetMode(EditorMode::Edit)) {
              std::cerr << "[Load] Cannot replace scene: Stop transition failed\n";
              return;
            }
          }
          const bool loaded = GameSaveManager::Get().LoadProject(filePath, scene);
          if (loaded) {
            app.m_PlayerHandleValue = 0;
            app.m_PlayerPersistentID.clear();
            for (const auto &entityPtr : scene->GetEntities()) {
              if (entityPtr && entityPtr->GetName() == "Player") {
                app.m_PlayerHandleValue = entityPtr->GetHandle().Value();
                app.m_PlayerPersistentID = entityPtr->GetPersistentID();
                break;
              }
            }
            if (app.m_Editor) {
              app.m_Editor->ClearSelection();
              app.m_Editor->GetUndoHistory().Clear();
              app.m_Editor->GetSelectionState().BindScene(scene);
              app.m_Editor->SanitizeSelection(scene);
            }
          }
          if (loaded)
            std::cout << "Proje yuklendi: " << filePath << std::endl;
          else
            std::cout << "Proje yuklenemedi: " << filePath << std::endl;
        }
      });

  CommandRegistry::Get().RegisterCommand(
      "game_mode", [](const std::vector<std::string> &args) {
        if (args.empty()) {
          std::cout << "Mod: " << GameModeManager::Get().GetModeString()
                    << std::endl;
          return;
        }
        auto &gmm = GameModeManager::Get();
        if (args[0] == "sp" || args[0] == "singleplayer") {
          Application::Get().LeaveNetworkGame();
          gmm.Disconnect();
        } else if (args[0] == "host") {
          uint16_t port = args.size() > 1 ? (uint16_t)std::stoi(args[1]) : 7777;
          gmm.StartHost("0.0.0.0", port);
        } else if (args[0] == "join") {
          std::string ip = args.size() > 1 ? args[1] : "127.0.0.1";
          uint16_t port = args.size() > 2 ? (uint16_t)std::stoi(args[2]) : 7777;
          Application::Get().JoinNetworkGame(ip, port);
        } else {
          std::cout << "game_mode <sp|host [port]|join [ip] [port]>"
                    << std::endl;
        }
      });

  LogStartup("Init: Initializing ImGuiLayer");

  m_ImGuiLayer = std::make_unique<ImGuiLayer>();
  m_ImGuiLayer->Init(m_Window);

  LogStartup("Init: ImGuiLayer initialized");

  return true;
}

void Application::InitializeGame(const GameManifest& game) {
  ResourceManager::Get().SetProjectRoot(game.root);
  m_Scene = std::make_unique<Scene>(game.title);
  if (!GameSaveManager::Get().LoadProject((game.root / game.scene).string(), m_Scene.get()))
    throw std::runtime_error("Standalone startup scene failed to load");
  Entity* player = m_Scene->GetEntityByPersistentID(game.playerID);
  if (!player || player->GetName() != "Player")
    throw std::runtime_error("Standalone player_id must resolve to the Player entity");
  size_t playerCount = 0;
  for (const auto& entity : m_Scene->GetEntities())
    if (entity->GetName() == "Player") ++playerCount;
  if (playerCount != 1) throw std::runtime_error("Standalone scene requires exactly one Player");
  m_PlayerPersistentID = game.playerID;
  m_PlayerHandleValue = player->GetHandle().Value();
  m_Camera = std::make_unique<Camera>(glm::vec3(player->GetWorldTransform()[3]));
  const auto* transform = player->GetComponent<Transform>();
  m_Camera->SetYaw(transform->rotation.y);
  m_Camera->SetPitch(transform->rotation.x);
  m_FPSController = std::make_unique<FPSController>(m_Camera.get());
  m_RenderSystem = std::make_unique<RenderSystem>(m_Camera.get());
  m_RenderSystem->Init(m_Scene.get());
  if (!m_RenderSystem->IsInitialized()) throw std::runtime_error("Standalone renderer failed");
  m_PhysicsSystem = std::make_unique<PhysicsSystem>();
  m_PhysicsSystem->Init(m_Scene.get());
  m_ParticleSystem = std::make_unique<ParticleSystem>();
  m_ParticleSystem->Init(m_Scene.get());
  m_ProjectileSystem = std::make_unique<ProjectileSystem>();
  m_ProjectileSystem->Init(m_Scene.get(), m_PhysicsSystem.get());
  m_ScriptSystem = std::make_unique<ScriptSystem>();
  ScriptEngine::Init();
  if (!ScriptEngine::GetCoreAssemblyImage()) throw std::runtime_error("Packaged ScriptCore failed to load");
  for (const auto& entity : m_Scene->GetEntities()) {
    if (const auto* script = entity->GetComponent<ScriptComponent>())
      if (!ScriptEngine::ClassExists(script->className))
        throw std::runtime_error("Packaged script class missing: " + script->className);
  }
  m_ScriptSystem->StartRuntime(m_Scene.get());
#ifdef ARCHURA_OPENAL
  m_OpenALAudioSystem = std::make_unique<OpenALAudioSystem>();
  m_OpenALAudioSystem->Init(m_Scene.get());
  m_OpenALAudioSystem->SetListenerCamera(m_Camera.get());
#endif
  m_HUDRenderer = std::make_unique<HUDRenderer>();
  if (!m_HUDRenderer->Init()) throw std::runtime_error("Standalone HUD failed");
  m_EngineWindow = Engine::Get().GetWindow();
  m_Input = Engine::Get().GetInput();
  m_Renderer = Engine::Get().GetRenderer();
  m_Input->SetCursorMode(2);
  if (m_Editor || Engine::Get().IsEditorMode())
    throw std::runtime_error("Standalone startup unexpectedly enabled the editor");
  std::cout << "Standalone scene loaded: entities=" << m_Scene->GetEntities().size()
            << "; editor=off\n";
}

void Application::Run(uint32_t frameLimit, bool playStopSmokeTest, const GameManifest* game, const NetworkLaunchOptions* network) {
  m_NetworkLaunch = network;
  m_Standalone = game != nullptr;
  if (m_Standalone) {
    if (playStopSmokeTest) throw std::runtime_error("Editor smoke cannot run in standalone mode");
    m_DevModeActive = false;
    m_GameTitle = game->title;
  }
  LogStartup("Run() started");

  LogStartup("Calling Init()");

  if (!Init()) {
    LogStartup("Init() FAILED");
    fprintf(stderr, "Application::Init Failed\n");
    throw std::runtime_error("Application initialization failed; see startup diagnostics");
  }

  LogStartup("Init() completed successfully");

  if (game) {
    InitializeGame(*game);
  } else {

  LogStartup("Creating Scene");

  m_Scene = std::make_unique<Scene>("Demo Scene");

  LogStartup("Scene created");

  // Initialize Camera as member variable
  m_Camera = std::make_unique<Camera>(glm::vec3(0.0f, 5.0f, 10.0f));

  LogStartup("Creating FPSController");

  m_FPSController = std::make_unique<FPSController>(m_Camera.get());

  LogStartup("FPSController created");

#ifdef ARCHURA_OPENAL
  LogStartup("Initializing OpenALAudioSystem");
  m_OpenALAudioSystem = std::make_unique<OpenALAudioSystem>();
  m_OpenALAudioSystem->Init(m_Scene.get());
  m_OpenALAudioSystem->SetListenerCamera(m_Camera.get());
  LogStartup("OpenALAudioSystem initialized");
#endif

  LogStartup("Initializing RenderSystem");

  m_RenderSystem = std::make_unique<RenderSystem>(m_Camera.get());
  m_RenderSystem->Init(m_Scene.get());
  if (!m_RenderSystem->IsInitialized()) {
    std::cerr << "[Render] RenderSystem initialization failed\n";
    m_Running = false;
    return;
  }

  LogStartup("RenderSystem initialized");

  // 4. Skybox Entity
  Entity *skyboxEntity = m_Scene->CreateEntity("Skybox");
  skyboxEntity->SetOwnership(EntityOwnership::Engine);
  auto *skyComp = skyboxEntity->AddComponent<SkyboxComponent>();
  skyComp->facePaths = {"assets/skybox/right.jpg", "assets/skybox/left.jpg",
                        "assets/skybox/top.jpg",   "assets/skybox/bottom.jpg",
                        "assets/skybox/front.jpg", "assets/skybox/back.jpg"};
  skyComp->shouldReload = true;

  // Initialize HUDRenderer as member
  m_HUDRenderer = std::make_unique<HUDRenderer>();
  if (!m_HUDRenderer->Init()) {
    std::cerr << "[HUD] HUD renderer initialization failed; continuing without HUD\n";
  }

  // Initialize Editor as member
  m_Editor = std::make_unique<Editor>();
  m_Editor->Init(m_Window);
  m_Editor->SetEnabled(true);

  // ── Play / Stop callbacks ────────────────────────────────────────────
  m_Editor->GetSelectionState().BindScene(m_Scene.get());
  m_Editor->RegisterPlayTransition([this]() {
    PlayModeContext playCtx{};
    playCtx.scene = m_Scene.get();
    playCtx.camera = m_Camera.get();
    playCtx.fpsController = m_FPSController.get();
    playCtx.physicsSystem = m_PhysicsSystem.get();
    playCtx.projectileSystem = m_ProjectileSystem.get();
    playCtx.particleSystem = m_ParticleSystem.get();
    playCtx.selection = m_Editor ? &m_Editor->GetSelectionState() : nullptr;
    playCtx.undoHistory = m_Editor ? &m_Editor->GetUndoHistory() : nullptr;
    playCtx.playerHandleValue = &m_PlayerHandleValue;
    playCtx.playerPersistentID = &m_PlayerPersistentID;
    playCtx.onRuntimeStart = [this](Scene *scene) {
      if (m_ScriptSystem) {
        m_ScriptSystem->StartRuntime(scene);
      }
    };
    if (!m_PlaySession.EnterPlayMode(playCtx)) {
      std::cerr << "[Editor] Play failed; authored scene retained\n";
      return false;
    }
    if (m_Editor) {
      m_Editor->SanitizeSelection(m_Scene.get());
    }
    m_Accumulator = 0.0f;
    m_Input->SetCursorMode(2);
    m_IsPaused = false;
    std::cout << "[Editor] Entered Play mode" << std::endl;
    return true;
  });
  m_Editor->RegisterStopTransition([this]() {
    PlayModeContext playCtx{};
    playCtx.scene = m_Scene.get();
    playCtx.camera = m_Camera.get();
    playCtx.fpsController = m_FPSController.get();
    playCtx.physicsSystem = m_PhysicsSystem.get();
    playCtx.projectileSystem = m_ProjectileSystem.get();
    playCtx.particleSystem = m_ParticleSystem.get();
    playCtx.selection = m_Editor ? &m_Editor->GetSelectionState() : nullptr;
    playCtx.undoHistory = m_Editor ? &m_Editor->GetUndoHistory() : nullptr;
    playCtx.playerHandleValue = &m_PlayerHandleValue;
    playCtx.playerPersistentID = &m_PlayerPersistentID;
    playCtx.onRuntimeStop = [this](Scene *scene) {
      if (m_ScriptSystem) {
        m_ScriptSystem->StopRuntime();
      }
      AudioSystem::Get().StopAllSceneAudio(scene);
      if (m_RenderSystem) {
        m_RenderSystem->SetIsolatedEntity(nullptr);
      }
    };
    if (!m_PlaySession.ExitPlayMode(playCtx)) {
      std::cerr << "[Editor] Stop failed; Play snapshot retained for retry\n";
      return false;
    }
    LeaveNetworkGame();
    if (m_Editor) {
      m_Editor->SanitizeSelection(m_Scene.get());
    }
    m_IsPaused = false;
    m_Accumulator = 0.0f;
    m_Input->SetCursorMode(0);
    std::cout << "[Editor] Returned to Edit mode" << std::endl;
    return true;
  });

  // Create Player entity (store as member)
  Entity *player = m_Scene->CreateEntity("Player");
  player->SetOwnership(EntityOwnership::Engine);
  m_PlayerHandleValue = player->GetHandle().Value();
  m_PlayerPersistentID = player->GetPersistentID();
  auto *weapon = player->AddComponent<Weapon>();
  weapon->InitInventory();
  auto *playerHealth = player->AddComponent<Health>();
  playerHealth->max = 100.0f;
  playerHealth->current = 100.0f;

  m_PhysicsSystem = std::make_unique<PhysicsSystem>();
  m_PhysicsSystem->Init(m_Scene.get());
#ifdef ARCHURA_DEBUG_PHYSICS
  m_DebugPhysicsSystem = std::make_unique<DebugPhysicsSystem>();
  m_DebugPhysicsSystem->Init(m_Scene.get());
  m_DebugPhysicsSystem->SetDependencies(m_FPSController.get(),
                                        m_PhysicsSystem.get());
#endif
  m_ScriptSystem = std::make_unique<ScriptSystem>();
  if (m_DevModeActive && m_Editor && m_Editor->GetMode() == EditorMode::Edit) {
    m_ScriptSystem->SetScene(m_Scene.get());
  } else {
    m_ScriptSystem->StartRuntime(m_Scene.get());
  }
  m_ParticleSystem = std::make_unique<ParticleSystem>();
  m_ParticleSystem->Init(m_Scene.get());
  m_ProjectileSystem = std::make_unique<ProjectileSystem>();
  m_ProjectileSystem->Init(m_Scene.get(), m_PhysicsSystem.get());

  LogStartup("All systems initialized. Entering main loop");

  m_EngineWindow = Engine::Get().GetWindow();
  m_Input = Engine::Get().GetInput();
  m_Renderer = Engine::Get().GetRenderer();

  // --- SETUP ROBUST MAP ---
  // 1. Sun
  Entity *light = m_Scene->CreateEntity("Sun");
  light->SetOwnership(EntityOwnership::Engine);
  auto *lightComp = light->AddComponent<LightComponent>();
  lightComp->type = LightComponent::Type::Directional;
  lightComp->color = glm::vec3(1.0f, 0.98f, 0.9f);
  lightComp->intensity = 1.0f;
  light->GetComponent<Transform>()->rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
  light->GetComponent<Transform>()->position = glm::vec3(0.0f, 50.0f, 0.0f);

  // 1.5 Ambient Light (Fill)
  Entity *ambientLight = m_Scene->CreateEntity("AmbientLight");
  ambientLight->SetOwnership(EntityOwnership::Engine);
  auto *ambientComp = ambientLight->AddComponent<LightComponent>();
  ambientComp->type = LightComponent::Type::Circle;
  ambientComp->color = glm::vec3(0.6f, 0.6f, 0.7f);
  ambientComp->intensity = 0.2f;

  // 2. Floor
  Entity *floor = m_Scene->CreateEntity("Floor");
  floor->SetOwnership(EntityOwnership::Engine);
  {
    auto *mesh = floor->AddComponent<MeshRenderer>();
    mesh->SetMeshAsset(Mesh::CreateCubeShared());
    mesh->color = glm::vec3(0.4f, 0.4f, 0.45f);

    auto *trans = floor->GetComponent<Transform>();
    trans->position = glm::vec3(0.0f, -1.0f, 0.0f);
    trans->scale = glm::vec3(100.0f, 1.0f, 100.0f);

    auto *col = floor->AddComponent<BoxCollider>();
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

  struct WallDef {
    std::string name;
    glm::vec3 p;
    glm::vec3 s;
    glm::vec3 c;
  };
  std::vector<WallDef> walls = {{"Wall_North",
                                 {0, wallY, -offset},
                                 {mapSize, wallHeight, wallThick},
                                 {0.8f, 0.4f, 0.4f}},
                                {"Wall_South",
                                 {0, wallY, offset},
                                 {mapSize, wallHeight, wallThick},
                                 {0.4f, 0.8f, 0.4f}},
                                {"Wall_East",
                                 {offset, wallY, 0},
                                 {wallThick, wallHeight, mapSize},
                                 {0.4f, 0.4f, 0.8f}},
                                {"Wall_West",
                                 {-offset, wallY, 0},
                                 {wallThick, wallHeight, mapSize},
                                 {0.8f, 0.8f, 0.4f}}};

  for (const auto &w : walls) {
    Entity *wall = m_Scene->CreateEntity(w.name);
    wall->SetOwnership(EntityOwnership::Engine);
    auto *mesh = wall->AddComponent<MeshRenderer>();
    mesh->SetMeshAsset(Mesh::CreateCubeShared());
    mesh->color = w.c;
    wall->GetComponent<Transform>()->position = w.p;
    wall->GetComponent<Transform>()->scale = w.s;

    auto *col = wall->AddComponent<BoxCollider>();
    col->size = w.s;
    col->isTrigger = false;
  }

  // Initialize PauseMenu as member
  m_PauseMenu = std::make_unique<PauseMenu>();

  // Set initial cursor mode: free in editor, locked in Play mode
  m_Input->SetCursorMode(m_DevModeActive ? 0 : 2);
  }

  std::cout << "DEBUG: Window ShouldClose: " << m_EngineWindow->ShouldClose()
            << ", Running: " << m_Running << std::endl;

  // Initialize timing for fixed timestep
  m_LastFrameTime = SDL_GetTicks() / 1000.0f;
  m_LastFPSUpdateTime = m_LastFrameTime;
  m_Accumulator = 0.0f;
  m_TickCount = 0;

  std::cout << "Starting 128 tickrate main loop..." << std::endl;

  uint32_t renderedFrames = 0;
  std::string smokeProbeID;
  std::string smokeSpawnID;
  EntityHandle smokeOriginalHandle;
  if (playStopSmokeTest) {
    if (frameLimit < 16 || !m_Editor)
      throw std::runtime_error("Play/Stop smoke requires editor and 16 frames");
    smokeProbeID = m_Scene->CreateEntity("PlayStopSmokeProbe")->GetPersistentID();
  }
  std::string networkProbeID;
  const auto authoredCameraPosition = m_Camera->GetPosition();
  if (network && network->smoke) {
    auto* probe = m_Scene->CreateEntity("Network smoke authored probe");
    probe->GetComponent<Transform>()->position = {17, 20, 23};
    probe->AddComponent<RigidBody>();
    networkProbeID = probe->GetPersistentID();
  }
  if (network && !JoinNetworkGame(network->host, network->port)) throw std::runtime_error("Network connection failed");
  const auto networkSmokeStart = std::chrono::steady_clock::now();
  int networkPhase = 0;
  std::uint32_t firstNetworkID = 0;
  bool sawRemote = false, sawReconnected = false;
  try {
    while (!m_EngineWindow->ShouldClose() && m_Running) {
      if (network && network->smoke) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-networkSmokeStart).count();
        auto* probe = m_Scene->GetEntityByPersistentID(networkProbeID);
        if (!probe || probe->GetComponent<Transform>()->position != glm::vec3(17,20,23) ||
            m_Camera->GetPosition() != authoredCameraPosition)
          throw std::runtime_error("Network session mutated authored scene/camera");
        if (m_NetworkGame && m_NetworkGame->LocalID()) {
          sawRemote |= m_NetworkGame->RemoteCount() > 0;
          if (!firstNetworkID) firstNetworkID = m_NetworkGame->LocalID();
          if (networkPhase == 4) sawReconnected |= m_NetworkGame->LocalID() != firstNetworkID;
        }
        if (networkPhase == 0 && elapsed >= 800) { m_IsPaused = true; networkPhase = 1; }
        else if (networkPhase == 1 && elapsed >= 1150) { m_IsPaused = false; networkPhase = 2; }
        else if (networkPhase == 2 && elapsed >= 1600) {
          if (!firstNetworkID || !m_Editor || !m_Editor->SetMode(EditorMode::Edit) || m_NetworkGame)
            throw std::runtime_error("Network Stop failed to release session");
          networkPhase = 3;
        } else if (networkPhase == 3 && elapsed >= 2000) {
          if (!JoinNetworkGame(network->host, network->port)) throw std::runtime_error("Network rejoin failed");
          networkPhase = 4;
        }
        if (elapsed >= network->durationMs) {
          if (!sawRemote || !sawReconnected || networkPhase != 4 || !m_NetworkGame || !m_NetworkGame->ValidateView())
            throw std::runtime_error("Graphical network smoke did not observe remote/reconnected view");
          m_Running = false;
        }
      }
      if (playStopSmokeTest) {
        if (renderedFrames == 2 || renderedFrames == 8) {
          auto* probe = m_Scene->GetEntityByPersistentID(smokeProbeID);
          if (!probe) throw std::runtime_error("Play/Stop smoke: authored probe missing");
          smokeOriginalHandle = probe->GetHandle();
          if (!m_Editor->SetMode(EditorMode::Play) || !m_PlaySession.IsPlaying())
            throw std::runtime_error("Play/Stop smoke: Play rejected");
          probe->GetComponent<Transform>()->position.x = 123.0f;
          smokeSpawnID = m_Scene->CreateEntity("PlayStopRuntimeSpawn")->GetPersistentID();
        } else if (renderedFrames == 5 || renderedFrames == 11) {
          if (!m_Editor->SetMode(EditorMode::Edit) || m_PlaySession.IsPlaying())
            throw std::runtime_error("Play/Stop smoke: Stop rejected");
          const auto* probe = m_Scene->GetEntityByPersistentID(smokeProbeID);
          if (!probe || probe->GetComponent<Transform>()->position.x != 0.0f ||
              m_Scene->GetEntity(smokeOriginalHandle) ||
              m_Scene->GetEntityByPersistentID(smokeSpawnID))
            throw std::runtime_error("Play/Stop smoke: authored restore failed");
        }
      }
      // Calculate frame time
      float frameStartTime = SDL_GetTicks() / 1000.0f;
      float frameTime = frameStartTime - m_LastFrameTime;
      m_LastFrameTime = frameStartTime;

      // Cap frame time to prevent spiral of death
      if (frameTime > 0.25f) {
        frameTime = 0.25f; // Max 4 FPS minimum
      }

      m_Accumulator += frameTime;

      const auto cpuWorkStart = std::chrono::steady_clock::now();

      // Fixed timestep update loop (128 Hz)
      // Process input and events (ONCE PER FRAME)
      ProcessInput();

      // Fixed timestep update loop (128 Hz)
      int ticksThisFrame = 0;
      while (m_Accumulator >= TICK_INTERVAL &&
             ticksThisFrame < MAX_TICKS_PER_FRAME) {

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

      const auto cpuWorkEnd = std::chrono::steady_clock::now();
      const std::chrono::duration<double, std::milli> cpuWorkDuration =
          cpuWorkEnd - cpuWorkStart;
      m_FrameTelemetry.RecordCpuWorkMilliseconds(cpuWorkDuration.count());

      // Swap/vsync and the limiter are deliberately outside the CPU-work
      // telemetry scope.
      m_EngineWindow->Update();
      m_Input->EndFrame();

      if (frameLimit != 0 && ++renderedFrames >= frameLimit) m_Running = false;

      LimitFrameRate(frameStartTime);

      // FPS Counter (every second)
      float currentTime = SDL_GetTicks() / 1000.0f;
      m_FrameCount++;
      if (currentTime - m_LastFPSUpdateTime >= 1.0f) {
        m_CurrentFPS = static_cast<float>(m_FrameCount /
                                          (currentTime - m_LastFPSUpdateTime));
        // std::cout << "FPS: " << m_CurrentFPS << " | Ticks: " << m_TickCount
        // << std::endl;
        m_FrameCount = 0;
        m_LastFPSUpdateTime = currentTime;
      }
    }
    std::cout << "DEBUG: Exited Loop. ShouldClose: "
              << m_EngineWindow->ShouldClose() << ", Running: " << m_Running
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    throw;
  } catch (...) {
    std::cerr << "Unknown Exception" << std::endl;
    throw;
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
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE)
      m_EngineWindow->SetShouldClose(true);
    if (event.type == SDL_WINDOWEVENT &&
        (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         event.window.event == SDL_WINDOWEVENT_RESIZED)) {
      m_EngineWindow->RefreshDrawableSize();
      m_Renderer->SetViewport(m_EngineWindow->GetFramebufferWidth(),
                              m_EngineWindow->GetFramebufferHeight());
      if (m_HUDRenderer) {
        m_HUDRenderer->SetScreenSize(static_cast<float>(m_EngineWindow->GetWidth()),
                                     static_cast<float>(m_EngineWindow->GetHeight()));
      }
    }
  }

  // 2. Handle special input (pause, dev mode, console)
  if (!m_Standalone && m_Input->IsKeyJustPressed(SDL_SCANCODE_TAB)) {
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
    if (!m_DevModeActive ||
        (m_Editor && m_Editor->GetMode() == EditorMode::Play)) {
      m_IsPaused = !m_IsPaused;
      m_Input->SetCursorMode(m_IsPaused ? 0 : 2);
    }
  }
  if (!m_Standalone && m_Input->IsKeyJustPressed(SDL_SCANCODE_GRAVE)) {
    DevConsole::Get().Toggle();
  }
#ifdef ARCHURA_DEBUG_PHYSICS
  if (m_Input->IsKeyJustPressed(SDL_SCANCODE_F3) && m_DebugPhysicsSystem) {
    m_DebugPhysicsSystem->Toggle();
  }
#endif

  // 3. Mouse / Camera look routing
  const bool inEditMode =
      m_DevModeActive && m_Editor && m_Editor->GetMode() == EditorMode::Edit;
  if (inEditMode) {
    // Editor fly-cam: update reads Input directly inside EditorCamera::Update
    float frameDt = ImGui::GetIO().DeltaTime;
    if (frameDt <= 0.0f)
      frameDt = 0.016f;
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
      Entity *picked =
          m_Editor->PickEntityAtScreenPos(m_Scene.get(), m_Input, w, h);
      if (picked)
        m_Editor->SetSelectedEntity(picked);
    }

    // --- Keyboard Shortcuts (Undo, Copy/Paste, Focus) ---
    m_Editor->ProcessEditorShortcuts(m_Input, m_Scene.get());
  } else if (IsNetworkViewActive() && !m_IsPaused && m_Input->IsCursorLocked()) {
    if (!m_NetworkLaunch || !m_NetworkLaunch->smoke) {
      auto delta = m_Input->GetMouseDelta();
      m_NetworkGame->GetCamera().ProcessMouseMovement(delta.x*m_FPSController->GetMouseSensitivity(),
                                                     -delta.y*m_FPSController->GetMouseSensitivity());
      m_NetworkGame->GetCamera().ProcessMouseScroll(m_Input->GetMouseScrollDelta());
    }
  } else if (!m_IsPaused && m_FPSController && GameModeManager::Get().GetMode() != GameMode::MultiplayerClient) {
    m_FPSController->HandleMouseLook(m_Input, 0.0f);
  }
}

void Application::UpdateGameLogic(float dt) {
  // Socket I/O is deliberately performed on the main/owner thread. Pump the
  // active transport every fixed tick so client handshakes, heartbeats and
  // queued frames can progress while the game is running.
  if (PumpNetworkGame()) return;

  const bool inEditMode =
      m_DevModeActive && m_Editor && m_Editor->GetMode() == EditorMode::Edit;
  if (!inEditMode && !m_IsPaused) {
    if (!m_PlaySession.IsPlaying() && m_Editor) {
      if (!m_Editor->SetMode(EditorMode::Play)) {
        m_IsPaused = true;
        return;
      }
    }
    if (m_Scene && !m_Scene->GetEntity(EntityHandle::FromValue(m_PlayerHandleValue)) &&
        !m_PlayerPersistentID.empty()) {
      if (Entity *resolvedPlayer = m_Scene->GetEntityByPersistentID(m_PlayerPersistentID)) {
        m_PlayerHandleValue = resolvedPlayer->GetHandle().Value();
      }
    }
    // Weapon progression is fixed-tick state. Advance it exactly once before
    // FPSController evaluates held-fire for this tick.
    WeaponSystem weaponSystem;
    weaponSystem.Update(m_Scene.get(),
                        EntityHandle::FromValue(m_PlayerHandleValue), m_Input,
                        m_Camera.get(), m_ProjectileSystem.get(), dt);

    m_FPSController->Update(m_Input, m_Scene.get(), dt,
                            m_ProjectileSystem.get(), m_PhysicsSystem.get());
#ifdef ARCHURA_DEBUG_PHYSICS
    if (m_DebugPhysicsSystem) {
      m_DebugPhysicsSystem->Update(dt);
    }
#endif
    m_ProjectileSystem->Update(dt);
    m_PhysicsSystem->Update(dt);
    m_ScriptSystem->Update(dt);
    m_ParticleSystem->Update(dt);
#ifdef ARCHURA_OPENAL
    if (m_OpenALAudioSystem) {
      m_OpenALAudioSystem->Update(dt);
    }
#else
    AudioSystem::Get().Update(m_Scene.get(), m_Camera.get());
#endif
  }
}

void Application::RenderFrame(float /*alpha*/) {
  m_Renderer->BeginFrame();
  m_ImGuiLayer->BeginFrame();

  // If editor is active and in Edit mode, inject the editor camera view/proj
  // so the 3D viewport shows the fly-cam perspective, not the game camera.
  const bool editorEditMode =
      m_DevModeActive && m_Editor && m_Editor->GetMode() == EditorMode::Edit;
  const bool networkView = IsNetworkViewActive();
  auto* sceneRenderer = GetRenderSystem();
  auto* renderCamera = GetCamera();
  if (networkView) m_NetworkGame->PrepareView();
  if (networkView && m_NetworkLaunch && m_NetworkLaunch->smoke) m_NetworkGame->AimAtRemote();
  glm::mat4 activeView = renderCamera->GetViewMatrix();
  glm::mat4 activeProjection =
      renderCamera->GetProjectionMatrix(m_Window->GetAspectRatio());
  if (editorEditMode) {
    EditorCamera *ec = m_Editor->GetEditorCamera();
    const float aspect = (m_Window && m_Window->GetHeight() > 0)
                             ? m_Window->GetAspectRatio()
                             : 1.777f;
    activeView = ec->GetViewMatrix();
    activeProjection = ec->GetProjectionMatrix(aspect);
    sceneRenderer->SetViewOverride(activeView, activeProjection);
  } else {
    sceneRenderer->ClearViewOverride();
  }

  sceneRenderer->Update(TICK_INTERVAL);
  if (networkView && m_NetworkLaunch && m_NetworkLaunch->smoke && m_NetworkGame->Snapshot().tick &&
      m_NetworkGame->Snapshot().tick != m_LastNetworkLoggedTick) {
    if (!m_NetworkGame->ValidateView()) throw std::runtime_error("Network camera/model binding diverged");
    const auto counters = sceneRenderer->GetLastFrameCounters();
    if (!counters.available || !counters.mainBatchSubmissions) throw std::runtime_error("Network arena did not draw");
    m_LastNetworkLoggedTick = m_NetworkGame->Snapshot().tick;
    const auto& pos = renderCamera->GetPosition();
    std::printf("GFXSNAP %llu SELF %u CAM %.6f %.6f %.6f REMOTES %zu VISIBLE %llu PAUSED %d",
        static_cast<unsigned long long>(m_LastNetworkLoggedTick), m_NetworkGame->LocalID(), pos.x,pos.y,pos.z,
        m_NetworkGame->RemoteCount(), static_cast<unsigned long long>(counters.visibleInstances), int(m_IsPaused));
    for (const auto& player : m_NetworkGame->Snapshot().players)
      std::printf(" PLAYER %u %.6f %.6f", player.id, player.x, player.z);
    std::printf(" VIEW");
    for (const auto& player : m_NetworkGame->View().players)
      std::printf(" PLAYER %u %.6f %.6f", player.id, player.x, player.z);
    std::printf("\n"); std::fflush(stdout);
  }

  // Draw the gameplay HUD after the 3-D pass and before ImGui overlays.
  if (m_HUDRenderer && !editorEditMode && m_Window) {
    m_HUDRenderer->BeginHUD(static_cast<float>(m_Window->GetWidth()),
                            static_cast<float>(m_Window->GetHeight()));
    m_HUDRenderer->DrawCrosshair();
    const Entity* player = m_Scene->GetEntity(
        EntityHandle::FromValue(m_PlayerHandleValue));
    if (player && !networkView) {
      if (const auto* health = player->GetComponent<Health>()) {
        m_HUDRenderer->DrawHealthBar(health->current, health->max, 32.0f,
                                     32.0f, 220.0f, 18.0f);
      }
      if (const auto* weapon = player->GetComponent<Weapon>()) {
        m_HUDRenderer->DrawAmmoCounter(weapon->stats.currentMag,
                                       weapon->stats.magSize, 32.0f,
                                       58.0f);
      }
    }
    m_HUDRenderer->EndHUD();
  }
#ifdef ARCHURA_DEBUG_PHYSICS
  if (m_DebugPhysicsSystem && !networkView) {
    m_DebugPhysicsSystem->Render(activeView, activeProjection);
  }
#endif

  if (m_DevModeActive) {
    m_Editor->BeginDockSpace();
    m_Editor->DrawMenuBar(m_Scene.get());
    m_Editor->DrawEditorUI(m_Scene.get());
    m_Editor->DrawOverlay(m_Scene.get(), GetCamera());

    // Draw 3-D transform gizmo when an entity is selected in Edit mode
    if (editorEditMode && m_Window) {
      m_Editor->DrawTransformGizmo(static_cast<int>(m_Window->GetWidth()),
                                   static_cast<int>(m_Window->GetHeight()));
    }
  }

#ifdef ARCHURA_DEBUG_PHYSICS
  if (m_DebugPhysicsSystem && !networkView) {
    m_DebugPhysicsSystem->DrawOverlay();
  }
#endif

  if (!m_Standalone) DevConsole::Get().Render();
  if (IsNetworkViewActive()) {
    ImGui::Begin("Multiplayer arena", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%zu players", m_NetworkGame->Snapshot().players.size());
    if (!m_NetworkGame->LocalID()) ImGui::TextUnformatted("Waiting for arena...");
    if (ImGui::Button("Disconnect")) LeaveNetworkGame();
    ImGui::End();
  }
  if (m_PauseMenu) m_PauseMenu->Render(m_IsPaused, *m_FPSController, *m_EngineWindow,
                                     m_Scene.get());
  if (m_Standalone && m_IsPaused) {
    ImGui::Begin("Paused", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Resume")) { m_IsPaused = false; m_Input->SetCursorMode(2); }
    if (ImGui::Button("Quit")) Quit();
    ImGui::End();
  }

  m_ImGuiLayer->EndFrame();
  m_Renderer->EndFrame();
}

void Application::LimitFrameRate(float frameStartTime) {
  if (m_FPSLimit <= 0.0f) {
    return;
  }

  const float targetFrameTime = 1.0f / m_FPSLimit;
  const float frameElapsed = SDL_GetTicks() / 1000.0f - frameStartTime;
  const float remaining = targetFrameTime - frameElapsed;

  if (remaining > 0.001f) {
    SDL_Delay(static_cast<Uint32>(remaining * 1000.0f));
  }
}

} // namespace Archura
