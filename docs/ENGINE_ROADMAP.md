# Engine stabilization roadmap

Cycle 2026-10-18 implemented shared validated transactional serialization, mesh
source preservation, safe transform undo, async Project cache, startup coverage
and build/test controls. Debug and Release each pass 23/23 checks. This is a
bounded stabilization cycle; the engine objectives remain incomplete.

Follow-up implemented: explicit engine/authored ownership replaces display-name
exclusions. Authored Player/Floor content persists, renamed bootstrap services
survive content loads, project ownership round-trips and legacy records migrate.

## Next implementation queue

GAUNTLET Loop 1 completed: persistent scene IDs, v2 records/v1 migration,
prepared final identity index and runtime-handle invalidation passed all 21 tests
in Debug and Release. Loop 2 added real Weapon persistence and strict validation
(22/22). Loop 3 completed editor selection lifetime validation and authored-state
Play/Stop restoration (23/23). Loops 4 & 5 completed configurable aggregate
scene reconstruction budgets, project-root asset path resolution, canonical
texture identity, `GameBuilderPanel` `assets/models` fallback (`ASSET-01`),
audio path normalization and runtime alias opening (`AUD-01`, `AUD-02`),
world-space render culling/light positions (`REND-01`), and `Terrain`
heightmap leak/rectangular/scale/bounds fixes (`TERR-01`); both configurations
pass 23/23 (Debug 2.02 s, Release 1.65 s). Execution gates: `.gauntlet/LOOP_PLAN.md`.

| Priority | Concrete change / affected files | Acceptance |
|---|---|---|
| Done, Loop 1 | Independent persistent IDs and transactional final index; v2 schema with legacy migration. | Debug/Release 21/21; identity/hierarchy/determinism/collision/allocator/depth regressions pass. |
| Done, Loop 2 | Real Weapon active/inventory/ammo/recoil configuration codec with transient reset. | Both save APIs and dedicated malformed/boundary tests pass; existing 128 Hz progression stays green. |
| Done, Loop 3 | Authored Play/Stop boundary in `Application`/`Editor`/`PlayModeSession`/`ScriptSystem`, generation-checked `EditorSelectionState`, rebinding runtime references through persistent IDs, and transient audio/physics/script/projectile/particle cleanup. | Edit does not simulate; 20-cycle Play/Stop restores authored values and removes runtime-only entities; Debug/Release 23/23 pass. |
| Done, Loops 4 & 5 | Configurable aggregate `SceneReconstructionBudget`, project-root asset resolution (`ResourceManager`), canonical texture keys (`ASSET-01`), `GameBuilderPanel` `assets/models` fallback, `AudioSystem::ResolveAudioPath` & runtime alias opening (`AUD-01`, `AUD-02`), world-space render culling (`REND-01`), and `Terrain` heightmap leak/rectangular/scale/bounds repairs (`TERR-01`). | Excessive scenes fail before unbounded allocation; missing/unsafe assets retain old state; relocation, duplicate texture stems, `assets/models` fallback, audio paths, and rectangular scaled terrain pass in Debug/Release 23/23. |
| Done, Loop 7 CLI | Manifest-selected runtime startup, portable package builder and sample (`PKG-01`). | External-CWD OpenGL/Mono launch loads the exact authored scene with no Editor; broader dependency/clean-OS/export-UI work remains. |
| P0, partial Loop8 | Real SDL_net/headless authoritative movement+ECS replicas implemented (`NET-01`); next bind graphical input/camera/remote meshes and shared map collision. | Two graphical clients consume server state through reconnect and Edit/Play boundaries; map collisions agree with server authority. Headless two-client movement/spawn/despawn/hostile-input checks already pass. |
| P1 | Previous/current transform render interpolation (`TIME-01`) and retained runtime UI backend (`UI-01`). | Cross-FPS gameplay assertions and retained UI interaction tests pass. |

Keep these as sequential acceptance gates. Performance, renderer and UI expansion
follow the measured/reproducible work units below; no FPS improvement is inferred
from a structural change alone.

Updated 2026-10-18. Implementation status and evidence: [ENGINE_AUDIT.md](ENGINE_AUDIT.md), [TEST_PLAN.md](TEST_PLAN.md), `.gauntlet/STATE.md` and `.gauntlet/METRICS.md`. A source implementation is not a completed acceptance test.

Play/Stop safety follow-up (Loop 6): rejected transitions preserve the editor mode;
unsupported authored snapshot state fails before Play; snapshots are bound to a
live source Scene and release GPU owners before context shutdown. Focused lifecycle
regressions and an opt-in native two-cycle Play/Stop startup check were added.
PKG-01 remains the next acceptance gate, followed by SDL_net and interpolation.

Loop 6 final verification: all-target Debug/Release builds and 24/24 CTest each
passed (3.79 s / 2.98 s). The additional test is the native OpenGL Play/Stop
integration check; packaging, live SDL_net replication and managed script scenarios
remain separate acceptance work.

Loop 7 implements the first standalone creator path: strict game manifest,
manifest-selected runtime scene/player, no Editor construction, portable CLI
package publication and an external-CWD sample launch. Detailed constraints and
commands are in BUILD_GUIDE. Follow-up packaging work is editor export UI,
separate runtime linkage, broader managed dependency discovery and clean-OS
prerequisite deployment. The next engine gauntlet is real SDL_net transport
validation and an authoritative server/two-client world slice (NET-01), followed
by previous/current transform interpolation (TIME-01).

Loop 8 completes a real SDL_net/headless authoritative movement slice (31/31
Debug/Release tests): version2 input and assigned identity, server-owned bounded
128Hz movement, full snapshots and safe client ECS replicas. Native process tests
prove two clients agree on movement/spawn/despawn and reject malformed/replay/spoof
traffic. NET-01 remains partial. Next: integrate snapshots/input with graphical
Application/camera/remote meshes and local-mode boundaries, then load shared map
collision into authoritative simulation. Keep TCP limitations explicit. TIME-01
render interpolation follows a measured network/gameplay integration gate.
