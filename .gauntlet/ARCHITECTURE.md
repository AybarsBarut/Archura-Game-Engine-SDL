# Architecture

## Targets and platform

- `ArchuraEngine`: Windows SDL2/OpenGL client and editor.
- `ArchuraServer`: nominal headless server; runtime transport is unavailable without SDL_net.
- `ArchuraLauncher`: Windows launcher/updater.
- C++17/C via CMake and MSVC/NMake; C#/.NET Framework 4.7.2 scripting surface via Mono.

## Ownership

- `main.cpp` owns splash resources and constructs `Application`.
- `Engine` owns `Window`, `Input`, and `Renderer`.
- `Application` is the composition root for Scene, camera, gameplay, physics, scripting, particles, editor/UI, and audio.
- `Scene` uniquely owns entities; entities uniquely own components. Hierarchy pointers are non-owning and constrained to a Scene.
- Resource caches mix shared assets with legacy raw-pointer ownership. Application teardown destroys OpenGL owners before the context.

## Actual frame pipeline

```text
SDL / ImGui events and frame-sampled input
  -> up to five 128 Hz application ticks
     -> FPS controller
     -> debug physics
     -> projectiles
     -> physics (internally accumulated at 60 Hz)
     -> scripts
     -> particles
     -> audio
  -> renderer begin
  -> shadow pass
  -> skybox and normal instanced pass
  -> debug rendering
  -> editor / console / pause UI
  -> swap
  -> software FPS limiter
```

The application computes interpolation alpha but rendering currently ignores it.

## Coupling and thread model

- Rendering, simulation, Scene, and Mono are main-thread affine.
- The renderer's "render thread" marker identifies the main thread; no separate render thread exists.
- `Application.cpp` crosses nearly every subsystem boundary.
- Singleton callbacks create `core <-> game`, `core <-> rendering`, and `core <-> editor/scripting` dependency cycles.
- `JobSystem` and `TransformSoASystem` are not integrated into the runtime loop.

## High-risk exclusive files

- `src/core/Application.cpp`
- `CMakeLists.txt`
- `src/editor/Editor.cpp`
- `src/game/FPSController.cpp`
- `src/game/Weapon.cpp`
- `src/network/NetworkManager.cpp`
- `src/game/PhysicsSystem.cpp`
- `src/game/RenderSystem.cpp`

