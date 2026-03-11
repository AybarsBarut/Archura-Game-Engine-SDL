# Archura Engine

A C++ game engine built on SDL2 and OpenGL, targeting a deterministic, data-oriented runtime for interactive 3D applications. Archura is designed with strict boundaries between subsystems to maintain a clean architecture, separating the core engine execution from the game logic layer.

**The architecture is divided into three layers:**

*   **Game Layer:** Handles game logic such as the FPS Controller, Weapons, Map Loading, and Game Modes. It has no direct knowledge of rendering APIs like OpenGL.
*   **Runtime Layer:** Manages the System Scheduler, ECS World, and Render Pipeline.
*   **Engine Core Layer:** Handles low-level functionality such as the Clock, Platform API, Logger, Memory, and Asset Registry.

**Core Principles:**
*   Fixed-timestep loop with variable-rate rendering (128 Hz logic tick) and interpolation for smoothness. 
*   Explicit dependency passing instead of singletons for subsystems.

## Current State - version 2.0.3

The engine is actively evolving and currently includes a playable client and a dedicated server application. 

Key Features:
*   **Client & Dedicated Server Build:** Standalone networked architecture with independent game and server executables using SDL_net.
*   **Rendering Pipeline:** OpenGL 4.x context, forward rendering with shadow mapping, terrain support, and a post-processing system.
*   **Entity Component System:** Highly decoupled data-oriented ECS architecture.
*   **Asset Management:** Support for recursive directory scanning, advanced model loading (FBX and OBJ via ufbx and fast_obj), and background loading foundations.
*   **Editor:** An integrated ImGui-based editor overlay featuring a scene hierarchy, entity inspector, direct entity management (create, rename, delete), and advanced model spawning from nested folders.
*   **Scripting:** Mono integration for C# scripting capabilities.
*   **Physics & Particles:** Integrated physics system, and both CPU and GPU-based particle emitters.
*   **Build Archiving & Versioning:** Custom build scripts for tracking historical compilation iterations of both Debug and Release branches.

## Requirements

*   CMake 3.20+
*   MSVC (MultiThreaded DLL configured) or MinGW (C++17 context)
*   SDL2, SDL2_mixer, SDL2_net, SDL2_ttf (Bundled)
*   glad, glm, ImGui (Bundled)
*   Mono SDK (Bundled)
*   ufbx and fast_obj for 3D model processing (Bundled)

## Building & Running

The engine handles Windows builds through a unified set of batch scripts, providing build versioning and archiving out of the box.

### Build Executables

You can build the engine via standard CMake, or use the provided batch script for a streamlined process that also archives the builds:

```batch
Build.bat
```
Running `Build.bat` will trigger CMake, compile both the ArchuraEngine client and the ArchuraServer executable, and then archive the binaries into respective `builds/debug` or `builds/release` versioned folders.

### Run Client

```batch
StartGame_Dev.bat
StartGame_Release.bat
DebugRun.bat
```
`StartGame_Dev.bat` will prompt you to select an archived build with the developer console and editor overlay enabled. `StartGame_Release.bat` executes the clean production build version.

### Run Dedicated Server

```batch
start_server.bat
```
The server will start headlessly and utilize the `server_config.json` parameters.

*Note: Required game assets (models, textures, audio) must reside in the `assets/` relative path of the executable.*

## Upcoming Roadmap

*   **Render Pipeline Enhancements:** Material system, explicit render graph, complete glTF handling.
*   **Physics Integration:** Transition physics to Jolt Physics backend.
*   **Networking Realtime State:** Full client server state replication based on the new networking abstraction.

## Contributing

See CONTRIBUTING.md. Please submit bug reports through the GitHub issue template and initiate feature discussions in the project Discussions board before generating pull requests.

## License

Apache 2.0. See LICENSE for more details.
