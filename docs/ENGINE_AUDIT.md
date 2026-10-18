# Engine audit

Reviewed 2026-10-18. This is a source audit; reproduction steps below are required checks, not claims that interactive scenarios ran. Current build/test evidence belongs in `.gauntlet/METRICS.md`. Historical PASS entries do not validate this checkout. Line references describe the inspected baseline and may move during repairs.

## Verified repair cycle — 2026-10-18

MSVC x64 Debug and Release all-target builds succeeded; both CTest runs passed
21/21 tests, including actual OpenGL 16-frame startup and orderly shutdown.
The freshly built baseline passed 14/14 contracts. Headless server help works;
runtime startup correctly exits 2 because SDL_net is unavailable.

Implemented shared JSON/schema validation, staged replacement with rollback,
atomic saves and backups, supported component/hierarchy persistence, mesh
recipes and original model paths. Unsupported components, assigned shaders,
generated textures and edited geometry reject saving. Exact format limits:
[serialization.md](serialization.md).

Undo resolves scene-owned handles. Project panel scans/sorts run on a worker
with immutable snapshots, navigation/manual invalidation and two-second polling.
Release warnings/errors remain visible; startup/loop exceptions produce failure
status. Build presets, dependency diagnostics and BUILD_TESTING gating work.

Follow-up: `.scene` display-name exclusions are replaced by explicit Entity
ownership. The regression first reproduced lost authored Player/Floor entities;
it now verifies their transforms/hierarchy, renamed engine services, repeated
loads, project ownership metadata and legacy/malformed input policies.

GAUNTLET Loop 1 repaired persistent identity: schema v2 stores independent IDs
and parent references, v1/unversioned files migrate, final index uniqueness is
validated before replacing the live scene. Runtime handles remain fresh.
Debug/Release full 21/21 gates include rollback fault injection and depth bounds.

GAUNTLET Loop 2 added real Weapon configuration persistence and strict component
validation; both full builds passed 22/22 tests.

GAUNTLET Loop 3 repaired editor selection lifetime (`EDIT-01`) and the Play/Stop
authored-state boundary (`EDIT-02`): `EditorSelectionState` validates selection
handles and `Scene` lifetime tokens, `RenderSystem::m_IsolationTarget` validates
handles, Edit mode gates gameplay simulation ticks, and `PlayModeSession`
snapshots and transactionally restores the complete authored scene, camera,
controller, Player identity, selection and undo history on Stop after stopping
runtime scripts/audio/physics/projectiles/particles. Both full builds passed
23/23 tests (Debug 2.61 s, Release 2.37 s).

GAUNTLET Loops 4 & 5 repaired aggregate scene reconstruction budgets, project-root
asset path resolution and `..` traversal rejection, canonical texture identity
and `GameBuilderPanel` `assets/models` fallback (`ASSET-01`), audio path
normalization (`AUD-02`) and runtime MCI alias opening / managed playback routing
(`AUD-01`), world-space distance culling and light positions in
`RenderSystem::Update` (`REND-01`), and `Terrain` missing-heightmap leak,
rectangular heightmap grid dimensions, headless GL guards, and horizontal scale /
bounds checks in `Terrain::GetHeightAt` (`TERR-01`). Both full builds pass 23/23
tests (Debug 2.02 s, Release 1.65 s).

Remaining P0/P1: standalone packaging (`PKG-01`), authoritative server simulation
(`NET-01`), render transform interpolation (`TIME-01`), and retained runtime UI
(`UI-01`). Full interactive reopen, texture rendering, managed callbacks, live
audio output and resource-growth acceptance remain untested.

## Prioritized baseline evidence matrix

The matrix records inspected baseline evidence. SER-01/02/03, EDIT-01/02,
ASSET-01, AUD-01/02, REND-01 and TERR-01 have the bounded repairs above.

| ID / priority | Verified source evidence and cause | Reproduction / verification requirement | Status |
|---|---|---|---|
| SER-01 / P0 | `src/core/ProjectSerializer.cpp`, `SerializeEntity`, writes name, ID, local Transform only; loader reconstructs only name/Transform and ignores persisted IDs. Mesh, hierarchy, script, physics and audio state cannot survive. | Round-trip parent/child entities with duplicate names, non-cube/imported meshes, material, light, body, collider and scripts; compare supported properties and asset references. | Repaired for supported native/weapon/audio/mesh/hierarchy schema |
| SER-02 / P0 | `src/core/GameSaveManager.cpp:228`: renderer forces `mesh_type=cube`; `modelPath` is never assigned. Name-based system filtering also excludes user entities named Floor/Player. | Save sphere, capsule, ramp, stairs and OBJ/FBX; reopen and verify exact mesh descriptor/path. Include a user entity named Floor. | Repaired (mesh source recipes + explicit EntityOwnership) |
| SER-03 / P0 | Both loaders parse selected lines rather than complete JSON. Project loader destroys old entities before allocating replacements; rollback removes only new objects. Both writers truncate the destination directly. | Invalid/truncated/compact JSON, unknown types/versions, failed resource construction and failed disk writes must preserve live scene, config and prior file; test backup recovery. | Repaired (bounded JSON preflight, transactional commit, atomic save + .bak, configurable `SceneReconstructionBudget`) |
| EDIT-01 / P0 | `src/editor/Editor.cpp:1974-2007`: undo stores `Entity*`, ignores supplied Scene and dereferences pointer after destruction. Inspector/gizmo/selection also retain raw pointers. | Gizmo-move selected entity, delete, Ctrl+Z; repeat with script deletion and scene reload. Run address diagnostics where available; resolve handles before any access. | Repaired (`TransformUndoHistory` + `EditorSelectionState` + isolation handle validation; covered by CTest) |
| EDIT-02 / P0 | `src/core/Application.cpp:415-425`: Play/Stop callbacks only change cursor/pause. `UpdateGameLogic:726` gates on pause alone, so physics/scripts also run in Edit; authored scene has no snapshot/restore. | Record authored transforms/components, Play, move/spawn/destroy, Stop; compare to original. Repeat 20 cycles, including pause and selected-object deletion. | Repaired (`PlayModeSession` snapshot/restore + Edit simulation gating; 20-cycle CTest passes) |
| NET-01 / P0 | `src/server/server_main.cpp:45-54` pumps transport only; gameplay, world state, collision and snapshots are TODOs. No authoritative multiplayer simulation exists. | One real SDL_net server plus two processes: identities, spawn/despawn, validated movement, snapshots, disconnect cleanup and invalid input rejection. | Transport partial; simulation not implemented |
| PKG-01 / P0-P1 | CMake builds Engine/editor, Server and Launcher; no game-only target/startup-scene flow. `GameBuilderPanel` adds scene content; it does not package a game. `Build.bat` archives an executable and a few DLLs. | Build sample game into clean directory; launch from unrelated CWD with source/build folders unavailable; verify script/assets/runtime dependencies and startup scene. | CLI standalone startup/package workflow implemented (Loop 7); editor export UI and clean-OS deployment remain |
| AUD-01 / P1 | `src/scripting/ScriptGlue.cpp:297` sets legacy `AudioSource::isPlaying` only. MCI `AudioSystem::Update` sends volume to an alias but does not open it. OpenAL uses a separate `AudioSourceComponent`, so this managed call does not start that backend either. | Attach configured audio source, invoke C# Play, confirm actual sound/error and native state. Exercise both backends and missing clips. | Repaired in source/CTest (`AudioSystem::Update` opens `sound_<id>`, `ScriptGlue::AudioSource_Play` triggers both `AudioSource` and `AudioSourceComponent`); live audio hardware untested |
| AUD-02 / P1 | `GameBuilderPanel.cpp:277-280` passes `assets/audio/name` to `AudioSystem::PlayOneShot`, which prefixes `assets/audio/` again. MCI volume/status commands run in fixed ticks. | Preview existing clip; rapid one-shots, stop/reload/delete source, category changes; measure MCI work before optimizing. | Path normalization repaired (`AudioSystem::ResolveAudioPath`, covered by CTest); MCI tick cost unmeasured |
| TIME-01 / P1 | `Application.cpp:589,755` computes alpha then discards it; simulation runs 128 Hz while PhysicsSystem internally accumulates 60 Hz. Five application ticks bound each frame but leftover backlog is retained. | Same scripted input at 30/60/120/144/240 render FPS and frame stalls; compare trajectories, cooldown/jump and bounded recovery. Keep simulation transforms unchanged by rendering. | Interpolation absent; cadence effects untested |
| ASSET-01 / P1 | `GameBuilderPanel.cpp:319` scans fallback `assets/models`; spawn path at :177 still uses `assets/objects`. ResourceManager caches by caller-supplied name, not canonical asset identity. | Remove objects folder, add OBJ in models, spawn. Also assign two different texture paths with same stem and verify each material. | Repaired (`GameBuilderPanel::ResolveModelSpawnPath` + `CanonicalAssetKey` in `ResourceManager`/`TextureManager`; covered by CTest) |
| TERR-01 / P1 | `Terrain.cpp`: missing-heightmap fallback leaks the object allocated before `stbi_load`. Rectangular images record min(width,height) as query stride while BuildMesh uses width; GetHeightAt ignores horizontal scale. | Missing image repeated load; asymmetric rectangular heightmap and non-unit X/Z scale: compare sampled heights to rendered mesh. Validate grid dimensions before allocation/division. | Repaired (leak eliminated, `m_Cols`/`m_Rows` stored, headless GL guarded, `GetHeightAt` applies `scale.x`/`scale.z` and bounds check; covered by CTest) |
| REND-01 / P1 | `RenderSystem.cpp:194` distance culling uses local position although draw matrices use world transforms. A parent can move the child into view while its local position is >1000. | Parent at -1500 X, child local +1500 X near camera; child must render and lights must agree with hierarchy. | Repaired in source (`GetWorldTransform()[3]` used for distance culling and light positions) |
| PERF-01 / P1-P2 | `Editor.cpp:1023` enumerates/sorts Project directory every DrawProjectPanel call. Shader hot reload stats files each debug frame; batches rebuild each render frame. | Count directory scans across idle frames; navigation/import/manual refresh invalidate cache. Profile representative scene before changing batches, broadphase, jobs or SoA. | Project panel repaired: worker scans/sorts, immutable cache, navigation/manual invalidation and 2s polling; shader/batch costs unmeasured |
| UI-01 / P2 | `src/ui/RuntimeUI.h` has abstraction only; repository search finds no concrete backend or application integration. `docs/UI_Framework_Direction.txt` is design intent. | Retained UI backend must load a document, receive input, render, resize and emit real events before being advertised. | Not implemented |

## Preserved foundations and stale historical claims

- ECS owns entities/components, validates handles, rejects hierarchy cycles/cross-scene parenting, and defers deletion during `ForEachEntity`. Existing tests cover these contracts; do not replace this with raw ID references.
- Weapon cooldown/reload advances per application tick; firing requires cursor lock. Mouse edges and wheel/FOV handling have focused regression tests. Their presence is coverage, not a fresh test result.
- NetworkManager already has bounded framing/queues, handshake/idle timeouts, owner-thread update calls, and explicit absent-backend failure. Application pumps client/server transport. `.gauntlet/BACKLOG.md` claims that the application never pumps networking are stale.
- Network console routes through transport state/counters. Historical fabricated-ping findings must not be reintroduced without new evidence.
- ProjectSerializer and ServerConfig already parse some input; the historical claim that both unconditionally succeed is stale. Complete schema validation and component persistence remain separate work.
- MCI one-shots now track aliases and clean completed playback; the historical blanket alias-leak claim is stale.
- OpenGL shader reload preserves the previous program on failed compile/link. PostProcess checks framebuffer completeness and restores program/VAO/texture/depth/blend/cull state; resize handles zero dimensions. Visual quality still requires live image comparisons.
- Unsupported graphics backend selection supports explicit failure or logged OpenGL fallback. No Vulkan renderer is claimed.
- Application tears render owners down before context; thread-affinity rules remain main-thread Scene/Mono and render-owner OpenGL.

## Loop 6 — Play/Stop rejection and snapshot lifetime (2026-10-18)

Starting revision: `f2e58ab`. Reproduction in the lifecycle test exposed silent
loss of an unsupported authored component and replacement of an unrelated scene
by Stop (seven failed assertions before the repair).

`EditorModeState` publishes Edit/Play only after the runtime callback succeeds,
rejects reentrant transitions and preserves its mode/guard across a thrown callback.
Application respects failed Play/Stop, including the load-game boundary. Snapshot
preflight rejects components without snapshot support and raw mesh/material assets
without retained shared owners. Stop/tick require the captured Scene lifetime token;
a destroyed source or another Scene cannot consume the snapshot. Shutdown explicitly
releases captured GPU owners before destroying the OpenGL context.

Coverage includes rejected Play/Stop, missing callbacks, reentrancy, exception guard
recovery, foreign/expired Scene rejection, captured asset release and the existing
20-cycle restoration suite. `Archura.Startup.PlayStop` also runs two real Editor /
Application Play/Stop cycles with OpenGL, checking authored transform restoration,
runtime-spawn removal and stale-handle invalidation before 16-frame shutdown.

Limits: arbitrary C# script execution, audio hardware and allocation/runtime-hook
failure recovery are not certified by these checks. Standalone packaging (PKG-01)
remains the next feature gate; this loop repairs data safety before that work.

Final Loop 6 gate: VS2022 x64 all-target Debug and Release builds/staging passed.
Debug CTest 24/24 (3.79 s), Release 24/24 (2.98 s), including real OpenGL startup,
two native Play/Stop cycles and unsupported-backend rejection. OpenAL is OFF;
SDL_net is unavailable. No performance or live-audio claim is made.

## Loop 7 — Standalone runtime startup and packaging

`GameManifest` provides a bounded versioned manifest, explicit persistent player
identity, relative content paths and canonical root containment. The runtime
entry resolves the default manifest relative to the executable, anchors CWD to
that package and initializes a scene without constructing the Editor or demo.
ScriptCore failure and missing script classes fail visibly; normal gameplay uses
the existing 128 Hz loop and runtime systems.

`tools/package_game.py` publishes a new package only after all declared content,
native DLLs, engine shaders, Mono 4.5 and ScriptCore have been copied. Existing
outputs are refused, scene asset references must be declared and portable, and
failed copies remove only private staging. The sample scene includes an imported
OBJ and a persistent Player identity. See BUILD_GUIDE for actual use and limits.

PKG-01 now has a CLI runtime/package path; editor export UI, a separate smaller
runtime binary, automatic arbitrary managed dependencies and clean-OS prerequisite
installation remain future work. The sample certifies startup/relocation, not a
complete authored gameplay or managed-script project.

Loop 7 final gate (VS2022 x64, build/windows-x64): all-target Debug/Release
build and staging PASS; CTest 29/29 each (6.69 s / 6.61 s). Additional final
space-containing package paths and missing-script-class checks passed in both
configs (focused package smoke 3.53 s / 3.58 s). Python contracts contain seven
cases; the Windows symlink-permission case skipped, six ran successfully.
OpenAL OFF; SDL_net unavailable. A retained Release sample under
Documents/Archura_Game_Engine/ArchuraStandaloneSample-20261018-Loop7 also passed
16-frame OpenGL/Mono startup from Temp CWD. No frame-time improvement or custom
managed-script/clean-OS distribution certification is claimed.
