# GAUNTLET implementation loop

Starting revision: d1e8a80. Main-thread Scene/Mono, render-owner OpenGL, C++17,
128 Hz application ticks and transactional data safety remain invariants.

## Gate 0 — Current baseline

Run the existing configured Debug/Release CTest executables and inspect changes.
Historical reports alone are insufficient. Preserve user data and existing APIs.

Result: configured Debug 21/21 (1.45 s), Release 21/21 (1.80 s) passed.

## Loop 1 — Persistent scene identity (P0)

- Inspect: Entity/Scene lifetime, shared SceneData and both load entry points.
- Reproduce: save/load/save changes entity identity because file IDs use fresh
  runtime handles; add a regression before implementing the replacement.
- Implement: independent immutable persistent identity, version-2 scene records,
  version-1/unversioned migration, validated parent references, and one Scene
  commit operation that checks final identity uniqueness before deleting anything.
- Accept: identity/hierarchy survive both APIs and repeated reloads; new creations
  remain unique; deleted runtime handles do not alias loaded entities; duplicate,
  malformed, missing-parent/cyclic identity input preserves scene/config. Loading
  a file over entities carrying the same persistent IDs must work transactionally.
- Verify: focused ECS/codec tests, then Debug/Release all targets and full CTest.
- Integrate/document: actual commands, failure reproduction, schema limits and
  next work in STATE/METRICS/REGRESSIONS and serialization/audit documents.

Implementation: v2 independent 128-bit identity, prepared final persistent-ID
index and no-throw commit; legacy migration and identity/depth/fault regressions.
Intermediate MSVC Debug proxy-allocation abort was diagnosed and corrected in
the API/test harness; see REGRESSIONS. Gate PASS: full Debug21/21 (2.81 s),
full Release21/21 (3.27 s), both including actual OpenGL startup/shutdown.

## Loop 2 — Weapon authoring data (P0, after Loop 1 gate)

- Inspect real Weapon inventory/stats/recoil configuration and its runtime state.
- Reproduce save rejection for an authored entity carrying Weapon.
- Implement a validated component codec for inventory, active stats and recoil
  patterns; define transient cooldown/reload state policy explicitly.
- Accept: a default/custom authored weapon round-trips; malformed enums, ammo,
  numeric bounds and patterns reject without mutation/overwrite; 128 Hz firing
  progression regressions remain green.
- Verify and record using the same loop. Do not call this Play/Stop restoration.

Loop 2 reproduction: saving an authored Weapon entity failed on Loop 1 revision
with Unsupported component. A dedicated header-only codec and strict tests are
integrated; focused tests 4/4 pass, including unchanged 128 Hz firing progression.
Final Debug gate 22/22 passed (1.77 s); Release 22/22 passed (1.68 s). Persist all real
stats/inventory/pattern data and reset only explicitly transient firing state.

## Loop 3 — Editor selection lifetime and authored Play/Stop boundary (P0, after Loop 2 gate)

- Inspect: `Editor` selection/gizmo/manipulation pointer ownership, `RenderSystem`
  isolation pointer, `Application::UpdateGameLogic` Edit/Play boundary, and
  runtime system cleanup (`PhysicsSystem`, `FPSController`, `WeaponSystem`,
  `ProjectileSystem`, `ParticleSystem`, `ScriptSystem`, `AudioSystem`).
- Reproduce: raw `Entity*` selection survives entity destruction and slot recycling;
  Edit mode runs simulation systems unless paused; Stop leaves runtime-mutated
  transforms/components/spawned entities in the authored scene.
- Implement: generation-checked `EditorSelectionState` with `Scene` lifetime tokens
  and `OwnsEntityPointer` validation; handle-validated `RenderSystem::m_IsolationTarget`;
  persistent-ID rebinding in `TransformUndoHistory`; Edit-mode simulation gating in
  `Application::UpdateGameLogic`; and `PlayModeSession` complete in-memory authored
  scene snapshot (`Mesh::RestoreGeometrySnapshot` preserving `MeshSource` recipes,
  hierarchy, ownership, and all supported + in-memory runtime components) restored
  transactionally via `Scene::CommitEntityReplacement` on Stop.
- Accept: deleted/recycled entities and destroyed scenes never leave dangling
  selection/isolation/gizmo pointers; Edit mode does not advance gameplay/physics/
  scripts/particles/audio; Play/Stop restores exact authored transforms, components,
  camera, controller, Player identity and selection across 20 repeated cycles.
- Verify: `Archura.Editor.PlayStopLifecycle` plus full Debug/Release builds and CTest.

Loop 3 implementation and verification: `Archura.Editor.PlayStopLifecycle` passed,
and final Debug all-target build + 23/23 CTest (2.61 s) and Release all-target
build/staging + 23/23 CTest (2.37 s) passed, including real OpenGL startup/shutdown.

## Loop 4 — Configurable aggregate resource budgets, project-root resolution & canonical asset identity (P1, after Loop 3 gate)

- Inspect: `GameSaveManager::LoadEntities`, `ProjectSerializer::LoadProject`,
  `ResourceManager`, `TextureManager`, and `GameBuilderPanel`.
- Reproduce: multiple valid entities can exceed desired aggregate scene memory
  without a configurable scene budget; textures in different directories with the
  same filename stem (`wood/albedo.bmp` vs `metal/albedo.bmp`) collide on stem key;
  `GameBuilderPanel` discovers `.obj` files in `assets/models` when `assets/objects`
  is absent but attempts to load them from `assets/objects/`; relocated project
  loads depend on CWD.
- Implement: `SceneReconstructionBudget` (`maxEntities`, `maxTotalMeshVertices`,
  `maxTotalMeshIndices`, `maxTotalMeshBytes`, `maxTotalTextureBytes`,
  `maxSingleAssetFileBytes`) enforced in preflight and staged reconstruction;
  `ResourceManager::SetProjectRoot`, `IsSafeAssetPath`, `ResolveAssetPath`, and
  `CanonicalAssetKey`; canonical path indexing in `TextureManager` and
  `ResourceManager` plus CPU-side image validation in headless mode; and
  `GameBuilderPanel::ResolveModelSpawnPath` / `SpawnModelEntity`.
- Accept: scenes exceeding entity/mesh/texture/file-size budgets reject before
  replacing live scene state; `..` traversal paths reject; relocated projects
  load from unrelated CWDs and preserve authored relative paths; same-stem textures
  in different folders remain distinct; `GameBuilderPanel` spawns `.obj` models
  from `assets/models` fallback.

## Loop 5 — Audio path/playback routing, world-space render culling & terrain heightmap fixes (P1, after Loop 4)

- Inspect: `AudioSystem`, `ScriptGlue::AudioSource_Play`, `RenderSystem::Update`,
  and `Terrain`.
- Reproduce: `GameBuilderPanel` and `AudioSystem` double-prefix `assets/audio/`;
  `AudioSystem::Update` sends MCI volume commands to `sound_<id>` without opening
  the alias when `AudioSource::isPlaying` is set; `RenderSystem::Update` uses local
  `transform->position` instead of `GetWorldTransform()[3]` for distance culling
  and light positions; `Terrain::LoadFromHeightmap` leaks `new Terrain()` on missing
  files, truncates rectangular heightmaps to `std::min(w, h)`, and ignores
  horizontal `scale.x`/`scale.z` and bounds in `GetHeightAt`.
- Implement: `AudioSystem::ResolveAudioPath`, runtime alias opening in
  `AudioSystem::Update`, `AudioSource` + `AudioSourceComponent` support in
  `ScriptGlue`, world-space positions in `RenderSystem::Update`, and leak-free,
  rectangular (`m_Cols`/`m_Rows`), headless-safe, scale- and bounds-aware `Terrain`.
- Verify (Loops 4 & 5): `Archura.Core.ProjectSerializer` and
  `Archura.Core.GameSaveManager` expanded; full Debug all-target build + 23/23 CTest
  (2.02 s) and full Release all-target build/staging + 23/23 CTest (1.65 s) passed.

## Subsequent queue

Standalone runtime packaging (`PKG-01`); real SDL_net backend and authoritative
world/input/snapshot/client vertical slice (`NET-01`); render transform
interpolation (`TIME-01`); retained runtime UI (`UI-01`). Each needs its own
reproduction, bounded implementation and actual acceptance evidence.
