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
| Partial, Loop9 slice done | Dedicated authoritative movement, graphical camera/remote meshes and shared static arena collision (`NET-01`). | Real two-OpenGL-client pause/Stop/rejoin/server-loss and collision checks PASS; custom levels, vertical/player collision, combat/auth/prediction remain. Loop10 adds bounded snapshot/render interpolation; local simulation presentation remains next. |
| Partial, Loop10 network done | Previous/current local transform render interpolation (`TIME-01`) and retained runtime UI backend (`UI-01`). | Cross-FPS gameplay assertions and retained UI interaction tests pass. |

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

Loop9 binds dedicated-server movement to the normal graphical Application's own
network scene/camera/renderer and cube avatars, preserving authored scene and
Edit/Play snapshot boundaries. Static dm_arena obstacles and outer limits are now
shared with the authoritative swept XZ player footprint. Pause/Stop/reconnect and
server-loss cleanup have real two-OpenGL-client acceptance. NET-01 remains partial
for arbitrary shared levels, vertical/player collision, combat, authentication and
prediction. Next measured work unit: TIME-01 snapshot/render interpolation while
keeping authoritative128Hz state and authored transforms unchanged.

Loop10 completes bounded network presentation: separate latest authority and
per-frame interpolated camera/remote transforms, four-tick delay,32-snapshot cap,
starvation clamping and discontinuity/reconnect handling. One fixed128Hz trajectory
passes30/60/120/144/240FPS sampling; two real OpenGL clients retain pause/Stop/rejoin,
collision, authored safety and server-loss acceptance. Both configurations pass33/33.

Next gauntlet unit: inspect the local fixed-step/render boundary, reproduce visible
stepping with a deterministic moving body, and add previous/current render poses
without writing presentation back into simulation or serialized authored transforms.
Explicitly test parent/child transforms, spawn/despawn, teleport and Play/Stop reset;
then compare local movement/cooldown/jump across render rates and stalls. Keep128Hz
and current physics cadence until evidence justifies a separate timing change.

User-reported RAMP-01 took priority over local presentation work: repeated walking
from the low end of a ramp exposed a zero-time historical side-normal collision.
The convex query now uses the current nearest face for inside/surface starts and
an upward normal at coincident entry edges. Forty previously failing sustained
walk cases, tangent/jump/descent, side/back and ceiling checks pass. Continue the
planned local presentation work after this correctness gate; arbitrary mesh
collision and stair stepping remain separate capabilities.

PHYS-BALL adds native Sphere collider queries/contacts, editable finite-mass/gravity/
material behavior, sphere rolling and finite-mass player push. Inspector motion and
explicit Ball presets are backed by actual runtime components; save/load/copy/Stop
retain the settings. Supported native shapes are now Box/Ramp/Sphere. Remaining
geometry work includes capsule/stair/mesh colliders, rotating convex rigid bodies
and broader continuous collision; network dynamic-object replication is also open.
Continue local render-pose work as planned after this user-requested physics gate.

HIT-MOTION completes the first local dynamic-body presentation slice of TIME-01:
previous/current60Hz physics poses, quaternion rotation,128Hz remainder composition,
hierarchical decal attachment and safe pause/Stop/external-edit boundaries. Bullet
impulses now affect dynamic objects, and marks follow the struck surface instead
of remaining in world space. General kinematic/script/camera interpolation and
cross-FPS jump/cooldown cadence remain separate work; no physics rate was changed.

GRAB-01 repairs the legacy direct-position object hold after the new ball workflow:
finite-mass dynamic eligibility, bounded physics impulses, world-space centers,
contact/visibility boundaries and release momentum now replace Transform lerping.
Controller tests cover real E/right-mouse events and scene/handle resets. Further
interaction work can build explicit joints/throw controls on this path; capsule/
stair/mesh colliders and general camera/kinematic interpolation remain open.

Next bounded physics unit: inspect Capsule mesh dimensions/origin, implement a
native capsule collider and matching queries/contacts instead of its box fallback,
then integrate Inspector/persistence and run shape-boundary plus full build gates.
This is planned work; capsules still use the documented fallback in this revision.
