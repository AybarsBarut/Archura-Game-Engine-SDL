# Engine regression and acceptance plan

Updated 2026-10-18. Record actual executions in `.gauntlet/METRICS.md`; this plan does not assert a pass. Use disposable project copies/temp directories; never overwrite user saves during failure tests.

## Current execution

Debug and Release builds passed, each latest CTest run passed 23/23 (21 contracts,
two startup checks; Debug 2.02 s, Release 1.65 s). Added coverage: Release logger
diagnostics; async directory cache; deletion-safe undo; editor selection lifetime
and 20-cycle Play/Stop authored-state restoration; primitive/OBJ/FBX identity and
moves; shared scene codec; configurable aggregate `SceneReconstructionBudget`
(`maxEntities`, `maxTotalMeshVertices`, `maxTotalMeshIndices`, `maxTotalMeshBytes`,
`maxTotalTextureBytes`, `maxSingleAssetFileBytes`); project-root asset path
resolution and `..` traversal rejection; canonical texture identity for same-stem
textures in different directories (`ASSET-01`); `GameBuilderPanel` `assets/models`
fallback discovery and spawn (`ASSET-01`); `AudioSystem::ResolveAudioPath` and
runtime alias assignment (`AUD-01`, `AUD-02`); `Terrain` missing-heightmap
fallback, rectangular heightmap grid dimensions, and scaled bounds-checked
`GetHeightAt` (`TERR-01`); malformed/Unicode/schema input; legacy formats;
hierarchy; audio config; failed construction rollback; save rejection/backup
preservation. Startup creates a real OpenGL context, renders 16 frames and shuts
down; unsupported-backend failure is also checked.

## Automated layers

Loops 4 & 5 extend `Archura.Core.ProjectSerializer` and
`Archura.Core.GameSaveManager` to cover:
- Aggregate `SceneReconstructionBudget` rejection for entity count, mesh vertex
  totals, texture byte totals, and single-asset file size while preserving live
  scene state and handles.
- Parent directory traversal (`..`) rejection in model, texture and audio paths.
- Project-root asset resolution from an unrelated working directory (`unrelated_cwd`)
  and preservation of authored relative `model_path` and `texture` paths across
  save/load.
- Distinct canonical texture caching for same-stem textures in different folders
  (`assets/textures/wood/albedo.bmp` vs `assets/textures/metal/albedo.bmp`).
- `GameBuilderPanel` discovery and spawning of `.obj` models from `assets/models`
  when `assets/objects` is absent.
- `AudioSystem::ResolveAudioPath` collapsing duplicate `assets/audio/` prefixes
  and `AudioSystem::Update` assigning `sound_<id>` runtime aliases when
  `AudioSource::isPlaying` is true.
- `Terrain::LoadFromHeightmap` missing-file flat fallback without memory leak,
  rectangular 4x2 heightmap column/row preservation, and `Terrain::GetHeightAt`
  interpolation with horizontal `scale.x`/`scale.z` and out-of-bounds `0.0f` return.

Loop 3 adds `Archura.Editor.PlayStopLifecycle`: single/multi-select handle
validation across deletion, slot recycling and `Scene` destruction;
`RenderSystem` isolation handle validation; Edit-mode simulation gating across
gameplay/physics/weapon/projectile/particle/script/audio systems; Play/Stop
authored-scene restoration (transforms, components, hierarchy, procedural and
edited mesh geometry + `MeshSource`, camera, controller, Player persistent ID,
selection and `TransformUndoHistory`); and a 20-cycle Play/Stop stress test with
runtime spawns, deletions, pause/resume and selection changes.

Weapon loop adds `Archura.Core.WeaponSerialization`: default and initialized
inventory, all six types, independent active/stored stats, precise float and
4096-offset boundary round-trips, transient reset, malformed enum/field/array,
duplicate inventory, ammo/rate bounds and failed-read destination preservation.

Persistent-ID loop adds v2 identity/hierarchy determinism, repeated same-ID
replacement, v1 numeric hierarchy migration, retained-service collision rollback,
pending-destroy lookup, duplicate/malformed IDs and 128/129-node depth boundaries.

Run Debug and Release independently using the commands in [BUILD_GUIDE.md](BUILD_GUIDE.md). Preserve logs and explicitly report optional SDL_net/OpenAL availability. Run `ctest --test-dir <build> --output-on-failure -C <config>` after a successful build.

Loop 6 expands `Archura.Editor.PlayStopLifecycle` with unsupported-component and
unowned-resource rejection, foreign/expired Scene guards, snapshot owner release,
failed-transition mode retention and reentrant/throwing callback handling.
`Archura.Startup.PlayStop` exercises actual Editor/Application callbacks, native
script runtime start/stop and OpenGL over two cycles. Each cycle checks transform
restoration, deletion of runtime spawns and old-handle invalidation. It does not
execute a configured managed ScriptComponent or certify audible output.
