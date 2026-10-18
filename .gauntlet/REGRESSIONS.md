# Regressions

## Resource budgets, project-root resolution, ASSET-01, AUD-01/02, REND-01 & TERR-01 loops — verified

- `GameSaveManager` and `ProjectSerializer` enforce configurable aggregate
  `SceneReconstructionBudget` limits (`maxEntities`, `maxTotalMeshVertices`,
  `maxTotalMeshIndices`, `maxTotalMeshBytes`, `maxTotalTextureBytes`,
  `maxSingleAssetFileBytes`) before committing scene changes, preserving live
  entities and handles on budget failure.
- During initial Loop 4 CTest verification, calling non-const `mesh->GetVertices()`
  for vertex budget accounting inside `GameSaveManager::LoadEntities` invalidated
  `Mesh::m_Source` to `MeshSourceType::Custom`, causing `legacy sphere retains
  source type` and `mesh type and dimensions survive roundtrip` to fail. Using
  `std::as_const(*mesh).GetVertices()` and `std::as_const(*mesh).GetIndices()`
  repaired the regression immediately.
- `ResourceManager::IsSafeAssetPath` and `ResolveAssetPath` reject `..` parent
  traversal, drive-relative paths and NUL bytes, while resolving relocated project
  assets and directories across `documentRoot`, `documentRoot.parent_path()`, and
  `m_ProjectRoot` from unrelated working directories.
- `TextureManager` and `ResourceManager` index textures by canonical path so
  same-stem textures in distinct directories (`wood/albedo.bmp` vs
  `metal/albedo.bmp`) never collide.
- `GameBuilderPanel` discovers and spawns `.obj` models from `assets/models` when
  `assets/objects` is absent (`ASSET-01`) and passes unprefixed audio filenames to
  `AudioSystem::PlayOneShot`.
- `AudioSystem::ResolveAudioPath` collapses duplicate `assets/audio/` prefixes
  (`AUD-02`), and `AudioSystem::Update` assigns and opens `sound_<id>` when
  `AudioSource::isPlaying` is set (`AUD-01`).
- `Terrain::LoadFromHeightmap` no longer leaks `new Terrain()` when a heightmap is
  missing, preserves rectangular `m_Cols` and `m_Rows`, guards GL calls when
  `!RenderThread::IsCurrent()`, and `Terrain::GetHeightAt` accounts for horizontal
  `scale.x`/`scale.z` and returns `0.0f` outside world bounds (`TERR-01`).
- All 23/23 CTest checks pass in both Debug (2.02 s) and Release (1.65 s).

## Editor selection lifetime and Play/Stop boundary loop — verified

- `EditorSelectionState` validates primary, multi-select, last-clicked and
  looked-at entities through generation-checked `EntityHandle`s, `Scene` lifetime
  tokens and `Scene::OwnsEntityPointer`. Deleting a selected entity, recycling its
  slot or destroying the `Scene` clears stale selection without dereferencing
  freed memory. `RenderSystem::m_IsolationTarget` is likewise validated by handle.
- Edit mode blocks gameplay simulation ticks (`WeaponSystem`, `FPSController`,
  `ProjectileSystem`, `PhysicsSystem`, `ScriptSystem`, `ParticleSystem`,
  `AudioSystem`).
- `PlayModeSession` captures the complete in-memory authored scene, camera,
  controller, Player persistent ID and selection on Play, and transactionally
  restores them via `Scene::CommitEntityReplacement` on Stop after stopping scripts
  and audio, clearing transient projectiles/particles/contacts, and rebinding
  Player, selection and `TransformUndoHistory` by persistent ID.
- During headless lifecycle testing, `FPSController::Update` was found to
  dereference `input->IsCursorLocked()` before checking `input != nullptr`;
  `FPSController::Update`, `HandleMovement` and `HandleMouseLook` now guard
  `m_Camera`, `scene` and `input`.
- All 23/23 CTest checks pass in both Debug (2.61 s) and Release (2.37 s),
  including the 20-cycle Play/Stop stress test and actual OpenGL startup/shutdown.

## Weapon persistence loop — verified

Authored Weapon save rejection was reproduced before integration. The complete
codec now passes dedicated strict-schema tests and both project/content round
trips. Invalid ammo writes preserve the previous save; malformed loads preserve
the old runtime handle. Independent inventory/active stats and recoil patterns
persist; transient firing state resets. All 22 tests pass in Debug and Release,
including unchanged fixed-tick weapon progression and actual startup/shutdown.

## Persistent identity loop — intermediate failure and correction

- New save/load/save identity regression failed on d1e8a80 before implementation.
- Initial global allocation-failure injection caused an MSVC Debug CRT abort
  dialog and CTest timeout (30 s); this is a failed run, not a passing result.
- Stack evidence identified iterator-proxy allocation inside noexcept MSVC STL
  vector move / `_Hash_vec` construction. Commit now accepts identity vectors by
  reference and copies in its recoverable preflight scope. Test-only injection
  excludes Debug iterator-proxy-sized allocations; Release excludes none.
- Debug ECS then passed with 19 injected payload allocation-failure points,
  checking old entity/parent/handle/identity/index preservation before commit.
- Test CRT errors/aborts now report without interactive dialogs. Final full
  Debug/Release validation is recorded in METRICS/STATE once complete.

## Explicit ownership protections — 2026-10-18

The authored Player/Floor round-trip regression failed before this fix; it passes
with explicit ownership along with all 21 checks in both Debug and Release.
Authored names/hierarchy/transforms survive; renamed engine services persist;
repeated scene loads replace authored entities without accumulating special
names; `.project` restores ownership and replaces old engine-owned entities;
legacy records migrate to authored; `.scene` cannot inject permanent services.
Existing cross-boundary hierarchy rejection and rollback guarantees still pass.

## Stabilization protections — 2026-10-18

Verified in final MSVC x64 Debug/Release CTest (21/21 each):

- Both project/save APIs use complete JSON/schema parsing; malformed/truncated,
  duplicate-key, invalid-vector/version/hierarchy input preserves live entities
  and project metadata. Failed second-entity model reconstruction rolls staged
  entities back while preserving existing handles.
- Scene/component/audio config, hierarchy and primitive recipes round-trip;
  OBJ/FBX paths with spaces and CPU geometry survive mesh reconstruction.
- Failed semantic/unsupported-component/material/edited-mesh saves retain the
  prior file; atomic replacement keeps its preceding successful content as .bak.
- Mesh moves preserve metadata/ownership; mutable geometry invalidates recipes.
- Undo never dereferences destroyed entities or removed transforms and cannot
  restore history into a different Scene; the history retains at most 50 entries.
- Project rendering consumes immutable cached entries; idle updates do not scan;
  navigation, refresh, polling, deletion, sorting and missing paths are covered.
- Warning/error methods and macros emit in Release. Startup initialization and
  frame-loop exceptions no longer return a false successful main exit.
- Real OpenGL startup/16 frames/shutdown passes; unsupported-backend failure is
  explicit. BUILD_TESTING=OFF disables focused tests too.

Known limitations are tracked in docs/ENGINE_AUDIT.md and docs/serialization.md;
these protections do not certify full editor lifecycle or standalone/multiplayer.

No integrated GAUNTLET regressions are known at the Phase Zero baseline.

## GAUNTLET-0001 protections

- Default rifle cannot fire before tick 13 at 128 Hz and can fire on tick 13.
- Successive automatic shots respect cooldown.
- Reload blocks firing, completes at its configured tick, and conserves ammunition.
- A destroyed Player handle is safely ignored by the weapon scheduler.
- Gameplay firing requires cursor lock, preventing editor selection clicks from shooting.

## GAUNTLET-0002 protections

- A wheel event changes gameplay FOV once with either zero or five fixed ticks in the render frame.
- The next frame does not replay the previous scroll delta.
- An unlocked cursor does not modify gameplay-camera FOV.

## GAUNTLET-0008 protections

- Pressed/released edges are non-consuming within a frame and reset next frame.
- Same-frame down/up and held up/down retain both transitions.
- Duplicate SDL button events do not fabricate transitions.
- Focus loss clears held LMB/RMB and produces a one-frame release edge.
- Held gameplay/editor actions continue to use `IsMouseButtonDown` level state.

## GAUNTLET-0003 protections

- The fixed telemetry ring retains the newest 512 valid samples and separately tracks all observed frames.
- Average and nearest-rank P95/P99 remain correct across wraparound, duplicate values, invalid inputs, and reset.
- CPU scope remains input through renderer end, excluding swap/vsync and the software limiter.
- Scene submission counters remain explicitly scoped; GPU time, total draws, triangles, and VRAM cannot silently acquire fabricated values.
- Statistics dumps must fail visibly when open, write, or close fails.

## Watch list

- Weapon cooldown/reload cadence at the 128 Hz application tick.
- Input edges and deltas across render rates from 30 to 240 FPS.
- Scene/entity handle validity during deferred destruction.
- OpenGL resource destruction before context teardown.
- Physics deterministic contact order and fixed-step spiral bound.
- Network no-backend mode must remain explicit and safe.

Failed tests, reverted work units, and post-integration regressions must be recorded here with revision, reproduction, and rollback information.
