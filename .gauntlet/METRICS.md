# Metrics

## GAUNTLET Loops 4 & 5 — Resource budgets, project-root resolution, ASSET-01, AUD-01/02, REND-01 & TERR-01, 2026-10-18

- Implemented `SceneReconstructionBudget` (`maxEntities`, `maxTotalMeshVertices`,
  `maxTotalMeshIndices`, `maxTotalMeshBytes`, `maxTotalTextureBytes`,
  `maxSingleAssetFileBytes`) in `GameSaveManager` and `ProjectSerializer`, plus
  `ResourceManager::SetProjectRoot`, `IsSafeAssetPath`, `ResolveAssetPath`, and
  `CanonicalAssetKey`.
- Updated `TextureManager` and `ResourceManager` to index textures by canonical
  path so same-stem textures in different folders (`wood/albedo.bmp` vs
  `metal/albedo.bmp`) never collide, and enabled CPU-side `stbi_load` dimension
  validation in `Texture::LoadFromFile` when `RenderThread::IsCurrent()` is false.
- Fixed `GameSaveManager::LoadEntities` to use `std::as_const(*mesh).GetVertices()`
  and `std::as_const(*mesh).GetIndices()` when computing vertex/index budgets so
  loaded `MeshSource` metadata is not invalidated to `Custom`.
- Fixed `GameBuilderPanel` (`ASSET-01`, `AUD-02`) to resolve `.obj` models from
  `assets/models` when `assets/objects` is absent and pass unprefixed clip names to
  `AudioSystem::PlayOneShot`.
- Implemented `AudioSystem::ResolveAudioPath` (collapsing duplicate `assets/audio/`
  prefixes and rejecting `..` traversal), runtime alias opening in
  `AudioSystem::Update`, and `AudioSource` + `AudioSourceComponent` support in
  `ScriptGlue` (`AUD-01`, `AUD-02`).
- Fixed `RenderSystem::Update` (`REND-01`) to use world-space positions
  (`glm::vec3(entityPtr->GetWorldTransform()[3])`) for distance culling and light
  positions.
- Fixed `Terrain` (`TERR-01`): eliminated `new Terrain()` memory leak on missing
  heightmap files before fallback `CreateFlat`, stored rectangular `m_Cols`/`m_Rows`
  dimensions, guarded OpenGL calls with `RenderThread::IsCurrent()`, and applied
  horizontal `scale.x`/`scale.z` and bounds checks in `Terrain::GetHeightAt`.
- Final Debug all-target build exited 0; CTest 23/23 passed, 2.02 s
  (`Archura.Core.ProjectSerializer` 0.17 s, `Archura.Core.GameSaveManager` 0.15 s,
  `Archura.Editor.PlayStopLifecycle` 0.05 s, `Archura.Startup.OpenGL` 0.85 s).
- Final Release all-target build/staging exited 0; CTest 23/23 passed, 1.65 s
  (`Archura.Core.ProjectSerializer` 0.12 s, `Archura.Core.GameSaveManager` 0.14 s,
  `Archura.Editor.PlayStopLifecycle` 0.03 s, `Archura.Startup.OpenGL` 0.75 s).
- Commands: `cmake --build build-codex-graphics-api --config <Debug|Release> --parallel`
  and `ctest --test-dir build-codex-graphics-api -C <Debug|Release> --output-on-failure`.
- No frame-time performance claim, live audio hardware output certification, or
  standalone packaging claim is made.

## GAUNTLET Loop 3 — Editor selection lifetime & Play/Stop boundary, 2026-10-18

- Implemented `EditorSelectionState` (`EntityHandle` + `Scene` lifetime token +
  `Scene::OwnsEntityPointer`), handle-validated `RenderSystem::m_IsolationTarget`,
  persistent-ID rebinding in `TransformUndoHistory`, Edit-mode simulation gating,
  and `PlayModeSession` in-memory authored-scene snapshot and transactional
  `Scene::CommitEntityReplacement` restore on Stop.
- Fixed null-input dereference in `FPSController::Update` (`input->IsCursorLocked()`)
  when running headless/without an `Input` instance.
- Added `Archura.Editor.PlayStopLifecycle` (`tests/EditorPlayStopLifecycleTests.cpp`)
  covering:
  1. Selection lifetime across single/multi-select deletion, slot recycling, and
     `Scene` destruction (`EditorSelectionState` + `RenderSystem` isolation).
  2. Edit-mode simulation gating (transforms, physics, weapons, projectiles,
     particles, scripts, and audio remain stationary in Edit mode).
  3. Play/Stop restoration of authored transforms, components, hierarchy,
     procedural/edited mesh geometry + `MeshSource`, camera, controller, Player
     persistent ID, selection, and `TransformUndoHistory`, plus removal of
     runtime-spawned entities/projectiles/particles.
  4. 20-cycle Play/Stop stress regression with runtime spawns, deletions,
     pause/resume, weapon fire, physics velocity/contacts, and selection changes.
- Final Debug all-target build exited 0; CTest 23/23 passed, 2.61 s
  (`Archura.Editor.PlayStopLifecycle` 0.06 s, `Archura.Startup.OpenGL` 1.19 s).
- Final Release all-target build/staging exited 0; CTest 23/23 passed, 2.37 s
  (`Archura.Editor.PlayStopLifecycle` 0.04 s, `Archura.Startup.OpenGL` 0.91 s).
- Commands: `cmake --build build-codex-graphics-api --config <Debug|Release> --parallel 4`
  and `ctest --test-dir build-codex-graphics-api -C <Debug|Release> --output-on-failure`.
- No frame-time performance claim, live OpenAL/MCI audio hardware verification,
  or standalone packaging claim is made.

## GAUNTLET Loop 2 — Weapon persistence, 2026-10-18

- Reproduced failure on Loop 1: authored Weapon save returned Unsupported
  component; new integration assertion failed before codec integration.
- Added validated WeaponData codec, actual project/content component support,
  and dedicated WeaponSerialization CTest. No WeaponSystem timing code changed.
- Focused codec/integration/128 Hz progression checks: 4/4 passed (0.65 s).
- Final Debug all-target build exited 0; CTest 22/22 passed, 1.77 s.
- Final Release all-target build/staging exited 0; CTest 22/22 passed, 1.68 s.
- Exact fields: enum, active stats, independent inventory stats, ammo and vec2
  recoil patterns. Transient cooldown/reload/recoil/shot-index state resets.
- Invalid ammo saves retained previous file; malformed weapon loads retained
  the live handle. Dedicated tests cover default/initialized/all six types,
  pattern 4096 boundary, missing/invalid fields, duplicates, numeric bounds and
  destination preservation. Both actual OpenGL startup checks passed.
- Commands: docs/BUILD_GUIDE.md; logs loop2-final-debug.log,
  loop2-final-release.log, loop2-focused.log and weapon-repro-build.log.
- No gameplay performance claim or Play/Stop lifecycle certification.

## GAUNTLET Loop 1 — persistent identity, 2026-10-18

- Fresh configured baseline: Debug21/21 (1.45 s), Release21/21 (1.80 s).
- First regression failed as expected: no independent persistent_id survives
  save/load/save in d1e8a80. Added separate 128-bit IDs and schema v2, with legacy
  numeric hierarchy migration and final persistent-index preparation before commit.
- Final full Debug build exited0; CTest21/21 passed, 2.81 s.
- Final full Release build/staging exited0; CTest21/21 passed, 3.27 s.
- ECS fault injection directly reported19 rollback points passed in each config.
  Debug test excludes MSVC iterator-proxy-sized allocations; Release excludes none.
  Earlier abort dialog/30s timeout is recorded as failure in REGRESSIONS.
- Checks include deterministic save/load/save, repeated same-ID replacement,
  malformed/duplicate/missing-parent/cyclic IDs, retained-service collision,
  invalidated runtime handles, numeric legacy migration and128/129-node depth.
- Commands: cmake --build build-codex-graphics-api --config <Debug|Release>
  --parallel 4; ctest --test-dir build-codex-graphics-api -C <config>
  --output-on-failure. Logs: loop1-final-debug.log, loop1-final-release.log,
  identity-repro-build.log, identity-fault-debug.log in the ignored build folder.
- No large-import timing or allocation-memory/performance claim is made.

## Ownership follow-up — 2026-10-18

- Initial stabilization committed/pushed as `3d1cd9d` before new implementation.
- Regression reproduced first: authored Player/Floor names saved successfully
  but vanished after load; GameSaveManager CTest failed with that exact assertion.
- Replaced name filtering with explicit EntityOwnership; default authored,
  Application bootstrap explicitly engine-owned. Project metadata round-trips;
  legacy content stays authored and `.scene` cannot claim retained ownership.
- Focused codec tests passed 2/2, including arbitrary service renames, authored
  names/hierarchy/transforms, three repeated loads, invalid metadata and both
  cross-boundary hierarchy directions.
- Debug all-target build passed; full CTest 21/21, 1.84 s.
- Release all-target build including runtime staging exited 0; full CTest
  passed 21/21, 1.88 s. Both startup checks ran actual engine executables.
- Commands: `cmake --build build-codex-graphics-api --config Debug --parallel 4`,
  same for Release, and `ctest --test-dir build-codex-graphics-api -C <config>
  --output-on-failure`. Logs: ownership-repro-build.log, ownership-debug.log,
  ownership-release.log in the ignored build directory.
- No new performance claim; simulation timing/graphics/scripting are unchanged.

## Stabilization cycle — 2026-10-18

Starting revision `662d628`; changes remain uncommitted. Environment: Windows x64,
Visual Studio 2022, MSVC 19.44, Windows SDK 10.0.26100.0, CMake 4.2.3, C++17.

- Initial discovery in reused build directory: 12 tests, 11 unbuilt executables;
  this was Not Run, not evidence of 11 runtime regressions.
- Enabled ECS/physics/renderer tests; fresh Debug all-target build exited 0.
  Baseline CTest: 14/14 passed, 0.78 s.
- Final Debug all-target build exited 0; CTest: 21/21 passed, 2.17 s.
- Final Release all-target build exited 0; CTest: 21/21 passed, 2.74 s.
- 19 contracts plus 2 startup checks: Release diagnostics; directory cache;
  deletion/scene-safe undo; mesh recipes/OBJ/FBX/moves; shared project/save codec
  rollback/validation/backup; existing input/gameplay/ECS/physics/network contracts.
- Actual OpenGL startup used NVIDIA GeForce RTX 4060 Laptop GPU, OpenGL 3.3;
  each config rendered exactly 16 frames and completed normal destruction.
  Unsupported Vulkan without fallback failed intentionally with a useful error.
- Headless server `--help` exited 0; regular no-SDL_net startup exited 2 and
  named the missing backend. No multiplayer simulation or two-client run claimed.
- `BUILD_TESTING=OFF` fresh configuration discovered 0 tests; presets parsed.
- An intermediate Release build failed while shared-codec declarations were
  incomplete; declarations and test link dependencies were corrected before the
  successful final Debug/Release builds. No intermediate failure is counted PASS.
- Existing unused-variable/parameter and third-party compiler warnings remain;
  the added Windows-header macro warning was corrected. Build is not warning-free.
- Commands: docs/BUILD_GUIDE.md. Logs: ignored build-codex-graphics-api/
  baseline-debug.log, final-debug.log, final-release.log and Testing/Temporary.

No performance benchmark, cache FPS improvement, gameplay percentile/memory
growth result, visual comparison, real audio, managed reload, packaged game or
authoritative multiplayer acceptance is claimed. SDL_net absent; OpenAL off.

The historical measurements below are retained for provenance only.

## Baseline — 2026-08-18

Revision: `371c71fd11e25d30e61e5e88e7cdfca05ad4d64c`

### Build

- Configuration: Debug, NMake, x64 MSVC 19.44, C++17
- Result: PASS
- Duration: 316.216 s
- Warnings: 11 existing warnings
- Errors: 0
- Post-build Mono copy: 5,268 files, about 500 MB

### Tests

- CTest: 4/4 PASS, 0 failed
- Duration: 0.30 s
- Coverage areas: network protocol, safe no-backend network behavior, ECS core, physics core

### Microbenchmarks

Single runs; not statistically sampled and not representative gameplay metrics.

| Benchmark | Baseline | Comparison | Reported ratio |
|---|---:|---:|---:|
| Allocator | pool 12.478 ms | new/delete 65.233 ms | 5.228x |
| ECS storage | SoA 18.103 ms | AoS 81.646 ms | 4.510x |

### Missing metrics

- Average, P95, and P99 gameplay frame time
- CPU phase timing
- GPU frame/pass timing
- Allocations per frame outside custom allocators
- Reliable draw, triangle, instance, and upload counters
- Representative physics, projectile, networking, and stress scenes
- Startup time excluding artificial splash sleeps

Hard-coded console statistics are explicitly excluded from this history.

## After GAUNTLET-0001 — 2026-08-18

- Debug `ArchuraEngine` build: PASS.
- Registered tests in `build_ramp_verify`: 6/6 PASS in 0.13 s.
- New focused weapon progression test: PASS.
- Existing build warnings observed; no new warning category attributed to the work unit.
- Performance metrics: not applicable; this was a gameplay correctness change.

## After GAUNTLET-0002 — 2026-08-18

- Debug `ArchuraEngine` build: PASS.
- Registered tests in `build_ramp_verify`: 7/7 PASS in 0.09 s on final commander run.
- New focused gameplay scroll test: PASS.
- Validated fixed-tick counts per render frame: 0 and 5 produce the same single FOV step.
- Performance metrics: not applicable; this was input determinism work.

## After GAUNTLET-0008 — 2026-08-18

- Debug `ArchuraEngine` compile/link: PASS.
- Registered tests in `build_ramp_verify`: 8/8 PASS in 0.09 s on final commander run.
- New focused mouse-button transition test: PASS.
- Performance metrics: not applicable; this was editor/input correctness work.

## After GAUNTLET-0003 — 2026-08-18

- Debug `ArchuraEngine` compile/link and post-build resource staging: PASS.
- Registered tests in `build_ramp_verify`: 9/9 PASS in 0.12 s on the final commander run.
- New focused frame-telemetry test: PASS.
- Runtime CPU metric capability: bounded 512-frame average/P95/P99 for main-thread work from `ProcessInput` through `Renderer::EndFrame`, excluding swap/vsync and the FPS limiter.
- Runtime scene counters: visible instances accepted into batches, distance rejects, and scoped main/shadow instanced submissions.
- GPU time, total draws, triangles, and VRAM remain explicitly unavailable.
- No representative interactive runtime sample was gathered; no frame-time improvement is claimed.
