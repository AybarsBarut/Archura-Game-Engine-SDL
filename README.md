# Archura Engine

A C++ game engine built on SDL2 and OpenGL, targeting a deterministic, data-oriented runtime for interactive 3D applications. This is an active prototype — architecture decisions are intentional and documented; the goal is a foundation worth building on, not feature count.

---

## Architecture Philosophy

Most hobby engines collapse under their own code over time because they treat *features* as the primary design concern. Archura treats *boundaries* as the primary concern. Every subsystem owns a clearly defined input contract and output surface. Systems do not reach into each other's internals.

**Three-layer model:**

```
[ Game Layer  ]  FPSController · Weapons · MapLoader · GameMode
[ Runtime     ]  SystemScheduler · ECS World · RenderPipeline
[ Engine Core ]  Clock · Platform · Logger · Memory · AssetRegistry
```

Each layer depends only on the one below it. The game layer has no knowledge of OpenGL. The engine core has no knowledge of what a `Weapon` is.

**Fixed-timestep loop, variable-rate rendering.** Game logic ticks at a fixed 128 Hz. The renderer runs at display rate and receives an interpolation alpha so object positions between ticks are smooth. This isn't an optimization — it's a correctness requirement for deterministic physics replay and future networking.

**Explicit ownership over convenient globals.** The engine instance is passed by reference. Subsystems don't call each other through singletons; they declare dependencies and receive them at init time.

---

## Current State — v0.1 (Prototype)

- SDL2 window + OpenGL 4.x context
- Forward-rendered scene with shadow mapping
- Type-indexed component system (ECS structure, not yet archetype-based)
- 128 Hz fixed-timestep with spiral-of-death protection
- Source-style FPS movement: friction, air acceleration, bunnyhop cap
- ImGui-based editor overlay (scene hierarchy, entity inspector)
- Developer console with runtime command binding
- SDL_mixer 3D audio
- SDL_net network stub (multiplayer scaffolding)
- C# scripting bridge placeholder (ScriptCore project)

---

## Building

### Requirements

| Dependency | Version | Notes |
|---|---|---|
| CMake | 3.20+ | |
| MSVC / MinGW | C++17 | Clang untested |
| SDL2 | 2.x | Bundled in `external/` |
| SDL2\_mixer | 2.x | Bundled |
| SDL2\_net | 2.x | Bundled |
| SDL2\_ttf | 2.x | Bundled |
| glad | GL 4.6 core | Bundled |
| glm | 0.9.9 | Bundled |
| ImGui | 1.90 | Bundled |

### Build

```batch
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or use the provided `Build.bat` for a one-step MSVC build.

### Running

```batch
DebugRun.bat       :: Debug build with console
StartGame_Dev.bat  :: Dev build, editor overlay enabled
StartGame_Release.bat
```

Assets must be present at `assets/` relative to the executable. Skybox images, fonts, and shaders are not redistributed; see `assets/README.txt` for placement.

---

## Roadmap

### v0.1 — Current (Foundation Cleanup)
Fix the structural issues before adding features:
- Single authoritative main loop — eliminate the `Engine`/`Application` split
- Replace C-style `fopen` log tracing with the existing `Logger` ring buffer
- Replace `ResourceManager` raw pointer maps with a handle-based `AssetRegistry`
- Extract hardcoded map geometry to a JSON scene format

### v0.2 — Data-Oriented ECS
- Archetype-based contiguous component storage
- `World::Query<T...>()` typed view with zero overhead at iteration
- Explicit system read/write declarations for future parallel scheduling
- Entity generation counter — detect use-after-destroy

### v0.3 — Render Pipeline
- `RenderContext` as the exclusive GL state owner — no `glEnable` outside this module
- Explicit render graph: shadow pass → geometry pass → post-process
- Material system with compile-time shader permutations
- Renderer interpolation using the fixed-timestep alpha

### v0.4 — Asset Pipeline
- Background asset loading with `AssetHandle<T>` reference counting
- Hot-reload for shaders in debug builds
- glTF2 static mesh loader

### v0.5 — Physics
- `PhysicsWorld` API replacing ad-hoc per-controller collision queries
- Jolt Physics integration behind the `PhysicsWorld` interface boundary

### v1.0 — Shippable Prototype
- Scene serialization / deserialization round-trip (no hardcoded world construction)
- Editor as an optional compile target, not tangled into the runtime
- Lua or WASM scripting sandbox replacing the C# placeholder
- Headless server build: no window, no renderer — for authoritative simulation

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Bug reports via the GitHub issue template; feature discussion in Discussions before any PR.

---

## License

Apache 2.0. See [LICENSE](LICENSE).
