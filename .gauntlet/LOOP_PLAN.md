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

## Subsequent queue

Play/Stop authored-state boundary and selection lifetime; aggregate resource
budgets/project-root paths; standalone runtime packaging; real SDL_net backend
and authoritative world/input/snapshot/client vertical slice. Each needs its own
reproduction, bounded implementation and actual acceptance evidence.
