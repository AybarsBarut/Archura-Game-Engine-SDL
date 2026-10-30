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

### JobSystem lifecycle contracts

`Archura.Core.JobSystem` and its `EnqueueFailure`, `WorkerCopy` and `InitFailure`
tests have 30-second timeouts. They cover rejected/empty submissions, callable-copy
failures, worker move semantics, standard/unknown callback exceptions, capture
cleanup before idle, multiple waiters, shutdown draining, producer races and restart.
A gated copy verifies that work from a previous session cannot enter a restarted
queue. A hook compiled only into the test target throws after the first worker is
created to exercise partial-initialization cleanup without exhausting OS resources.
The concurrent producer scenario is stress coverage, not a deterministic race proof.

`Init` and `Shutdown` require external serialization and must run outside worker
jobs. `Wait` must also run outside jobs and includes callback capture destruction;
producers need separate synchronization when waiting for an entire batch.
`Shutdown` joins workers, not external producers; join producers before releasing
the state they capture. Callable
copies run outside the queue mutex; stopped or old-session submissions are rejected
at final admission. `Dispatch` grouping and runtime threading policy are unchanged.

```powershell
ctest --preset validation-debug -R 'Archura.Core.JobSystem'
ctest --preset validation-release -R 'Archura.Core.JobSystem'
```

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

Loop 7 adds `Archura.Core.GameManifest` (strict schema/size/identity/path limits,
case collisions and missing-content preservation), and Python
`Archura.Packaging.Contracts` (portable publication, no overwrite, missing native
and managed dependencies, undeclared/external scene assets, malformed manifests
and injected copy failure cleanup). The symlink case skips when Windows does not
allow test symlink creation; canonical containment is still enforced in code.

With Python 3.9+ and startup tests enabled, `Archura.Startup.StandalonePackage`
copies the sample project into a temporary source directory, packages it, renames
the source directory away and launches only the package from an unrelated CWD.
It verifies exactly four authored entities, editor disabled, actual OpenGL/Mono,
16 frames and shutdown. Missing imported OBJ, wrong player, malformed scene and
missing default manifest must fail without opening the editor/demo.

Loop 8 adds `Archura.Network.AuthoritativeWorld`: canonical input/welcome/full-world
wire encoding, axis/identity/count/size bounds and unchanged destination on failed
read; replay/wraparound rejection; input spam cannot increase per-tick travel;
diagonal normalization, fixed128Hz speed, timeout/bounds, stale/malformed snapshots,
and generation/lifetime-safe client ECS spawn/despawn preserving authored entities.

`Archura.Network.ProcessIntegration` uses actual SDL_net and TCP loopback, an
ArchuraServer process and two separate ArchuraNetClient processes. It verifies a
listener survives an empty peer set (Windows regression), fragmented hello and
coalesced movement commands, assigned identities, movement and identical states
at common ticks, disconnect/despawn, invalid NaN/range/size/zero sequence, replayed
frame/input, forged position/world, old protocol, handshake and established-idle
timeouts. Only test-owned processes and temporary files are cleaned up. These are
headless network/ECS clients; graphical multiplayer, map physics and combat are
not certified. The no-backend target remains compiled without ARCHURA_HAS_SDL_NET
and must still pass. Full final gates: Debug31/31 (17.58s), Release31/31 (12.21s),
with OpenAL OFF and the real SDL_net backend enabled.

Loop9 expands the world contract with shared swept-AABB obstacle/boundary tests,
sliding, normalized camera-relative keyboard axes, sequenced zero input during
pause/Edit and command reset on reconnect. Replica Clear preserves unrelated ECS
entities and generation/lifetime rules.

`Archura.Network.GraphicalIntegration` uses a dedicated server and two real
OpenGL Application processes, not the headless client target. Diagnostic movement
uses the same input normalization/transport path and validates the actual camera,
remote MeshRenderer transforms/assets and render submission counters. At common
server ticks the applications agree on player state. It exercises pause, Stop/Edit,
new connection identity, authored rigid-body/camera preservation, obstacle stopping,
normal shutdown and server loss with live GPU owners. Actual OS keyboard/mouse
interaction, custom levels, vertical physics, combat and Internet performance remain
outside this automated fixture. The test requires a desktop/OpenGL session and
Python3.9+; it is registered only with startup tests and SDL_net enabled.

Loop10 adds `Archura.Network.SnapshotInterpolation`: a single fixed128Hz server
trajectory is sampled at30/60/120/144/240 render FPS and checked against the analytic
31.25ms-delayed path. Tests require fractional presentation positions, monotonic
bounded motion, unchanged encoded authority and32-snapshot memory bounds. Additional
cases cover jitter/bunched arrivals, render stalls, starvation, reversed/invalid
clocks, invalid/stale snapshots, spawn/despawn/ID reuse, teleports, long gaps, reconnect
with lower ticks and near-uint64-limit server ticks.

The graphical process fixture now compares camera to VIEW positions and separately
compares latest authority between clients. It requires observable presentation lag
on the moving Z client and checks pause settling after20 ticks; collision, authored
safety, Stop/rejoin and server-loss checks remain. Remote transforms/assets are
validated natively against the sampled view. Full all-target Debug/Release builds
plus33/33 CTest each passed21.59s/15.26s (graphical10.40s/5.43s), SDL_net ON/OpenAL OFF.
This does not certify singleplayer interpolation, jump/cooldown cross-FPS behavior,
adaptive Internet buffering, prediction, GPU performance or subjective visual quality.

RAMP-01 adds sustained character walking to `Archura.Physics.Core`: the editor's
4x2x4 ramp, flat floor, AABB half-extents0.3/0.9/0.3 and repeated128Hz movement.
Gravity is applied only while airborne, matching FPSController's policy. Five yaw
angles (0/30/90/180/270), four speeds (0.5/2/4/8) and two starting heights produce
40 cases. All40 failed before the fix and pass afterward. Separate checks cover
tangent surface motion, jumping away, descending to flat floor, side/back faces
and a low ceiling. Tests call the same physics query used by FPSController; they
do not automate OS keyboard input or certify arbitrary mesh collision.

PHYS-BALL extends Physics.Core with seven groups: rounded ray/sphere/character
queries that miss the sphere's bounding-cube corners; sphere/box and sphere/sphere
narrowphase; unequal-mass momentum/elastic energy; gravity toggle, settling and
bounce on static/kinematic surfaces; friction-driven spin/orientation versus a
frictionless slider and ramp descent; parent/negative/nonuniform world scale and
center offset; and player pushing light/heavy spheres/boxes with opt-out/static/
kinematic controls and sustained pushing against a wall. Old40 ramp cases remain.

ProjectSerializer/GameSaveManager tests round-trip radius, angular velocity/drag
and push flags, exercise legacy defaults and reject invalid fields without changing
the scene or saved file. PlayStopLifecycle restores the complete sphere/body state
over three cycles. The GameplayScroll physics stub follows the extended movement
API; production FPSController opts into player pushing. Native startup/network/
standalone checks exercise the updated renderer initialization and resource teardown.
Inspector mouse interaction itself and subjective visual quality are not automated.

HIT-MOTION adds `Archura.Physics.Presentation` and
`Archura.Projectile.SurfaceAttachment`. Presentation tests run30/60/120/144/240FPS
around128Hz application/60Hz physics, comparing analytic positions and quaternion
rotation through Euler wrap; Transform/RigidBody bytes remain unchanged. Tests
include hierarchy/decal poses, teleport/scale/reparent edits, scene/handle replacement,
stalls, invalid render remainder, mode/timestep/reset and pause/resume without rewind.
Impulse tests cover mass, off-center torque and invalid/static/kinematic/stale targets.

SurfaceAttachment tests exercise oblique/downward normals, scaled/reflected parents,
world footprint, rolling attachment and destroyed/recycled targets. The existing
PlayStopLifecycle target now also invokes the actual ProjectileSystem shot path:
a2kg ball receives1.25m/s fromdamage25, its mark follows translation/rotation,
deleting the ball removes the mark, and kinematic targets receive no impulse.
These tests verify mechanics and interpolation; they do not measure monitor
ghosting, subjective visual quality or FPS gains.

GRAB-01 adds `Archura.Physics.Grab`: eligibility rejection for static/kinematic/
zero-mass/opt-out/trigger bodies, invalid input, no pose/config writes,500N force
and80m/s² response bounds, mass differences, gravity holding/release, momentum,
wall contact, parent collider center and stale/reused scene/entity lifetimes.

PlayStopLifecycle exercises actual Input/FPSController/Physics with injected E and
right-mouse events: acquire, moving-camera impulse, release momentum and settings,
seven ineligible cases, occluders, cursor/focus loss, body changes, reset, range,
destroyed targets and identical handles in a different scene. OS pointer/keyboard
delivery itself is outside the fixture. The GameplayScroll test retains lightweight
stubs for the new holding API and continues checking scroll timing independently.
GRAB-01 final edge cases also verify trigger volumes do not occlude acquisition/holding and mismatched Physics/Scene bindings cannot acquire an identically numbered entity.
