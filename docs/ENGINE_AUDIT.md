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

GAUNTLET Loop1 repaired persistent identity: schema v2 stores independent IDs
and parent references, v1/unversioned files migrate, final index uniqueness is
validated before replacing the live scene. Runtime handles remain fresh.
Debug/Release full21/21 gates include rollback fault injection and depth bounds.

Remaining P0: gameplay component coverage; broader selection lifetime and Play/Stop restoration;
standalone packaging; authoritative server simulation. Per-file/entity/mesh
limits do not establish an aggregate allocation budget. Full interactive reopen,
texture rendering, managed callbacks, audio and resource-growth acceptance remain
untested. No performance gain is claimed for the cache.

## Prioritized baseline evidence matrix

The matrix records inspected baseline evidence. SER-01/02/03 have the bounded
repairs above; EDIT-01's undo is fixed while broader selection safety remains.

| ID / priority | Verified source evidence and cause | Reproduction / verification requirement | Status |
|---|---|---|---|
| SER-01 / P0 | `src/core/ProjectSerializer.cpp`, `SerializeEntity`, writes name, ID, local Transform only; loader reconstructs only name/Transform and ignores persisted IDs. Mesh, hierarchy, script, physics and audio state cannot survive. | Round-trip parent/child entities with duplicate names, non-cube/imported meshes, material, light, body, collider and scripts; compare supported properties and asset references. | Partial serializer; repair in progress |
| SER-02 / P0 | `src/core/GameSaveManager.cpp:228`: renderer forces `mesh_type=cube`; `modelPath` is never assigned. Name-based system filtering also excludes user entities named Floor/Player. | Save sphere, capsule, ramp, stairs and OBJ/FBX; reopen and verify exact mesh descriptor/path. Include a user entity named Floor. | Confirmed data loss in baseline |
| SER-03 / P0 | Both loaders parse selected lines rather than complete JSON. Project loader destroys old entities before allocating replacements; rollback removes only new objects. Both writers truncate the destination directly. | Invalid/truncated/compact JSON, unknown types/versions, failed resource construction and failed disk writes must preserve live scene, config and prior file; test backup recovery. | Partial validation; transactional replacement missing in baseline |
| EDIT-01 / P0 | `src/editor/Editor.cpp:1974-2007`: undo stores `Entity*`, ignores supplied Scene and dereferences pointer after destruction. Inspector/gizmo/selection also retain raw pointers. | Gizmo-move selected entity, delete, Ctrl+Z; repeat with script deletion and scene reload. Run address diagnostics where available; resolve handles before any access. | Confirmed unsafe lifetime path |
| EDIT-02 / P0 | `src/core/Application.cpp:415-425`: Play/Stop callbacks only change cursor/pause. `UpdateGameLogic:726` gates on pause alone, so physics/scripts also run in Edit; authored scene has no snapshot/restore. | Record authored transforms/components, Play, move/spawn/destroy, Stop; compare to original. Repeat 20 cycles, including pause and selected-object deletion. | Runtime boundary absent in baseline |
| NET-01 / P0 | `src/server/server_main.cpp:45-54` pumps transport only; gameplay, world state, collision and snapshots are TODOs. No authoritative multiplayer simulation exists. | One real SDL_net server plus two processes: identities, spawn/despawn, validated movement, snapshots, disconnect cleanup and invalid input rejection. | Transport partial; simulation not implemented |
| PKG-01 / P0-P1 | CMake builds Engine/editor, Server and Launcher; no game-only target/startup-scene flow. `GameBuilderPanel` adds scene content; it does not package a game. `Build.bat` archives an executable and a few DLLs. | Build sample game into clean directory; launch from unrelated CWD with source/build folders unavailable; verify script/assets/runtime dependencies and startup scene. | Standalone creator workflow not implemented |
| AUD-01 / P1 | `src/scripting/ScriptGlue.cpp:297` sets legacy `AudioSource::isPlaying` only. MCI `AudioSystem::Update` sends volume to an alias but does not open it. OpenAL uses a separate `AudioSourceComponent`, so this managed call does not start that backend either. | Attach configured audio source, invoke C# Play, confirm actual sound/error and native state. Exercise both backends and missing clips. | Disconnected managed playback |
| AUD-02 / P1 | `GameBuilderPanel.cpp:277-280` passes `assets/audio/name` to `AudioSystem::PlayOneShot`, which prefixes `assets/audio/` again. MCI volume/status commands run in fixed ticks. | Preview existing clip; rapid one-shots, stop/reload/delete source, category changes; measure MCI work before optimizing. | Path defect confirmed; performance unmeasured |
| TIME-01 / P1 | `Application.cpp:589,755` computes alpha then discards it; simulation runs 128 Hz while PhysicsSystem internally accumulates 60 Hz. Five application ticks bound each frame but leftover backlog is retained. | Same scripted input at 30/60/120/144/240 render FPS and frame stalls; compare trajectories, cooldown/jump and bounded recovery. Keep simulation transforms unchanged by rendering. | Interpolation absent; cadence effects untested |
| ASSET-01 / P1 | `GameBuilderPanel.cpp:319` scans fallback `assets/models`; spawn path at :177 still uses `assets/objects`. ResourceManager caches by caller-supplied name, not canonical asset identity. | Remove objects folder, add OBJ in models, spawn. Also assign two different texture paths with same stem and verify each material. | Fallback path confirmed; asset identity partial |
| TERR-01 / P1 | `Terrain.cpp`: missing-heightmap fallback leaks the object allocated before `stbi_load`. Rectangular images record min(width,height) as query stride while BuildMesh uses width; GetHeightAt ignores horizontal scale. | Missing image repeated load; asymmetric rectangular heightmap and non-unit X/Z scale: compare sampled heights to rendered mesh. Validate grid dimensions before allocation/division. | Confirmed source defects; runtime untested |
| REND-01 / P1 | `RenderSystem.cpp:194` distance culling uses local position although draw matrices use world transforms. A parent can move the child into view while its local position is >1000. | Parent at -1500 X, child local +1500 X near camera; child must render and lights must agree with hierarchy. | Confirmed coordinate mismatch |
| PERF-01 / P1-P2 | `Editor.cpp:1023` enumerates/sorts Project directory every DrawProjectPanel call. Shader hot reload stats files each debug frame; batches rebuild each render frame. | Count directory scans across idle frames; navigation/import/manual refresh invalidate cache. Profile representative scene before changing batches, broadphase, jobs or SoA. | Project panel repaired: worker scans/sorts, immutable cache, navigation/manual invalidation and 2s polling; shader/batch costs unmeasured |
| UI-01 / P2 | `src/ui/RuntimeUI.h` has abstraction only; repository search finds no concrete backend or application integration. `docs/UI_Framework_Direction.txt` is design intent. | Retained UI backend must load a document, receive input, render, resize and emit real events before being advertised. | Not implemented |

## Preserved foundations and stale historical claims

- ECS owns entities/components, validates handles, rejects hierarchy cycles/cross-scene parenting, and defers deletion during `ForEachEntity`. Existing tests cover these contracts; do not replace this with raw ID references.
- Weapon cooldown/reload advances per application tick; firing requires cursor lock. Mouse edges and wheel/FOV handling have focused regression tests. Their presence is coverage, not a fresh test result.
- NetworkManager already has bounded framing/queues, handshake/idle timeouts, owner-thread update calls, and explicit absent-backend failure. Application pumps client/server transport. `.gauntlet/BACKLOG.md` claims that the application never pumps networking are stale.
- Network console routes through transport state/counters. Historical fabricated-ping findings must not be reintroduced without new evidence.
- ProjectSerializer and ServerConfig already parse some input; the historical claim that both unconditionally succeed is stale. Complete schema validation and component persistence remain separate work.
- MCI one-shots now track aliases and clean completed playback; the historical blanket alias-leak claim is stale. Managed source playback/path handling and scene-source lifetime remain open.
- OpenGL shader reload preserves the previous program on failed compile/link. PostProcess checks framebuffer completeness and restores program/VAO/texture/depth/blend/cull state; resize handles zero dimensions. Visual quality still requires live image comparisons.
- Unsupported graphics backend selection supports explicit failure or logged OpenGL fallback. No Vulkan renderer is claimed.
- Application tears render owners down before context; thread-affinity rules remain main-thread Scene/Mono and render-owner OpenGL.

## Reconnaissance scope and confidence

Inspected build scripts/CMake, main, Application/Engine/Window, ECS definitions and hierarchy, physics/FPS/weapon/projectile/script update paths, rendering API/shaders/post-process/terrain/scene collection, editor and GameBuilder, both serializers and resource caches, Mono/glue/ScriptCore, MCI/OpenAL, network/server, runtime UI interface/design, `.gauntlet` architecture/backlog/state/regressions/metrics, and existing test sources/registrations. This is reconnaissance of system boundaries and concrete failure paths, not an exhaustive line-by-line or runtime certification.

Missing tests are coverage gaps, not proof of runtime failure. Live graphics, audio, Mono reload, clean deployment and two-client acceptance remain unverified until their actual outputs are recorded.

## Current cycle: Project panel cache

`ProjectDirectoryCache` moves enumeration, directory metadata and sorting to a dedicated filesystem-only worker. The panel consumes immutable snapshots, displays scan failures, refreshes on navigation/manual request and polls external changes at two-second intervals while visible. Worker shutdown wakes and joins its thread; an in-progress OS filesystem call can delay destruction until it returns. Direct MSVC C++17 `/W4` compilation and focused cache contracts passed on 2026-10-18. Engine/CTest integration results are recorded by the integration owner; no measured frame-time improvement is claimed.
