# GAUNTLET State

Last updated: 2026-10-18

## Active GAUNTLET loops (starting d1e8a80)

- Plan: LOOP_PLAN.md. Fresh baseline Debug/Release 21/21 passed.
- Loop 1 implemented: independent persistent identity, schema v2 with v1 migration,
  final uniqueness validation / prepared persistent index before replacement,
  deterministic save/load/save and malformed/collision/depth/allocator regressions.
- Loop 1 gate PASS: Debug full build and 21/21 tests (2.81 s); Release full
  build/staging and 21/21 tests (3.27 s), including real startup/shutdown.
- Intermediate global allocator injection opened an MSVC Debug abort dialog;
  stack diagnosis and test/API correction are recorded in REGRESSIONS. Debug ECS
  now passes 19 injected payload failure points in each configuration.
- Loop 2 gate PASS: complete Weapon codec and both scene integrations; Debug
  full build/22 tests (1.77 s), Release full build/staging/22 tests (1.68 s).
  Ammo/stat/pattern validation, transient reset and 128 Hz progression all pass.
- Loop 3 gate PASS: generation-checked `EditorSelectionState` with `Scene`
  lifetime tokens, `RenderSystem` isolation handle validation, `TransformUndoHistory`
  persistent-ID rebinding, Edit-mode simulation gating, and `PlayModeSession`
  authored-scene snapshot/transactional restore with runtime script/audio/physics/
  projectile/particle cleanup and Player/selection rebinding. Debug full build and
  23/23 tests (2.61 s); Release full build/staging and 23/23 tests (2.37 s).
- Loop 4 gate PASS: configurable aggregate `SceneReconstructionBudget`
  (`maxEntities`, `maxTotalMeshVertices`, `maxTotalMeshIndices`, `maxTotalMeshBytes`,
  `maxTotalTextureBytes`, `maxSingleAssetFileBytes`), project-root asset path
  resolution (`ResourceManager::SetProjectRoot`, `ResolveAssetPath`,
  `IsSafeAssetPath`), canonical texture identity (`CanonicalAssetKey` in
  `ResourceManager` and `TextureManager` so same-stem textures in different folders
  never collide), and `GameBuilderPanel` `assets/models` fallback spawn path
  resolution (`ASSET-01`).
- Loop 5 gate PASS: `AudioSystem::ResolveAudioPath` preventing double
  `assets/audio/assets/audio/` prefixing (`AUD-02`), `AudioSystem::Update` opening
  MCI aliases when `AudioSource::isPlaying` is set and `ScriptGlue::AudioSource_Play`
  supporting both `AudioSource` and `AudioSourceComponent` (`AUD-01`), world-space
  distance culling and light positions in `RenderSystem::Update` (`REND-01`), and
  `Terrain::LoadFromHeightmap` missing-file leak fix, rectangular (`m_Cols`/`m_Rows`)
  heightmap support, headless OpenGL guards, and horizontal `scale.x`/`scale.z` +
  bounds validation in `Terrain::GetHeightAt` (`TERR-01`).
- Loops 4 & 5 final verification PASS: Debug all-target build and 23/23 CTest
  (2.02 s); Release all-target build/staging and 23/23 CTest (1.65 s).

## Current status

- Phase: bounded stabilization cycle verified through Loop 5; broader engine work incomplete
- Starting revision: `662d628` (clean working tree before this cycle)
- Initial stabilization pushed to origin/main as `3d1cd9d`; ownership follow-up
  is recorded in the subsequent integration commit (see Git history).
- Fresh baseline: Debug all targets PASS; 14/14 contracts PASS (0.78 s)
- Final build: Debug and Release all targets PASS (VS2022/MSVC x64, C++17)
- Latest tests: Debug 23/23 PASS (2.02 s), Release 23/23 PASS (1.65 s)
- Runtime: both configurations rendered 16 OpenGL frames and shut down; unsupported backend failed as expected
- Headless server: help PASS; no-backend startup exited 2 with explicit error
- Runtime networking: unavailable because SDL_net is absent; no authoritative simulation implemented
- OpenAL enabled configuration, real audio output, Mono reload, image regressions and packaged deployment: untested

## Completed in this cycle

- Shared versioned scene codec for ProjectSerializer and GameSaveManager, complete
  bounded JSON parsing/schema preflight, legacy inputs, staged entity/resource
  creation with rollback, atomic save replacement and preceding-save backup.
- Hierarchy, supported native/audio configuration, original imported mesh paths
  and exact procedural recipes persist. Unknown components, custom/edited meshes,
  assigned shaders, generated textures and excluded-system hierarchy edges fail
  explicitly without replacing the prior save. See docs/serialization.md.
- Transform undo uses generation handles plus owner Scene identity; deleted
  entities/components and wrong-scene history entries are safely discarded.
- Project panel filesystem work moved to a joined worker with immutable snapshots,
  navigation/manual invalidation, polling and visible errors; no speedup measured.
- Startup failure/exception status, bounded startup tests and Release warning/error
  logging; CMake presets/dependency diagnostics/BUILD_TESTING controls; batch CWD fix.
- Required audit, roadmap, build guide and test plan created; history retained.
- Follow-up: EntityOwnership explicitly distinguishes engine bootstrap services
  from authored content. User Player/Floor names no longer disappear; renaming
  an engine service cannot cause it to be deleted on content reload. `.project`
  persists ownership; legacy records default to authored; `.scene` rejects service
  promotion. Both cross-boundary hierarchy guards remain in force.
- Loops 4 & 5: configurable aggregate scene reconstruction budgets, project-root
  asset resolution and traversal rejection, canonical texture identity,
  `GameBuilderPanel` `assets/models` fallback (`ASSET-01`), audio path
  normalization and runtime alias opening (`AUD-01`, `AUD-02`), world-space
  render culling/light positions (`REND-01`), and `Terrain` missing-file leak fix,
  rectangular heightmap dimensions, headless GL guards, and scaled bounds-checked
  `GetHeightAt` (`TERR-01`).

## Current invariants

- Preserve the 128 Hz application simulation tick unless a separately measured work unit changes it.
- Scene/entity mutation remains main-thread affine.
- OpenGL resource owners must be destroyed before the window/context.
- Performance claims require measurements; simulated console values are not evidence.
- Each work unit must remain isolated, reversible, and validated.

## Next queue

1. Persistent scene identity is implemented and verified (Loop 1).
2. Weapon configuration is implemented and verified (Loop 2).
3. Editor selection lifetime and authored-state Play/Stop restoration are
   implemented and verified (Loop 3).
4. Configurable aggregate resource budgets, project-root asset resolution, and
   `ASSET-01` are implemented and verified (Loop 4).
5. `AUD-01`, `AUD-02`, `REND-01`, and `TERR-01` are implemented and verified
   (Loop 5).
6. NEXT: standalone runtime packaging (`PKG-01`) and sample game validation
   outside the repository.
7. Enable/test SDL_net, then implement authoritative world/input/snapshot vertical
   slice (`NET-01`) with a dedicated server and two real clients.
8. Previous/current transform render interpolation (`TIME-01`) and retained
   runtime UI (`UI-01`).
