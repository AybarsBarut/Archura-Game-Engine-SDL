# Engine regression and acceptance plan

Updated 2026-10-18. Record actual executions in `.gauntlet/METRICS.md`; this plan does not assert a pass. Use disposable project copies/temp directories; never overwrite user saves during failure tests.

## Current execution

Debug and Release builds passed, each CTest run passed 21/21 (19 contracts,
two startup checks). Added coverage: Release logger diagnostics; async directory
cache; deletion-safe undo; primitive/OBJ/FBX identity and moves; shared scene
codec; malformed/Unicode/schema input; legacy formats; hierarchy; audio config;
failed construction rollback; save rejection/backup preservation. Startup creates
a real OpenGL context, renders 16 frames and shuts down; unsupported-backend
failure is also checked. Missing-backend headless-server failure was observed
separately. Interactive acceptance below remains pending.

## Automated layers

| Layer | Existing coverage inspected | Required follow-up |
|---|---|---|
| Unit/contracts | ECS core, physics core, weapon progression, gameplay scroll, mouse edges, telemetry, job system, graphics selection, server config, network framing/no-backend/console, basic ProjectSerializer, and Project directory cache snapshots/invalidation/polling/navigation/errors/shutdown. Renderer ownership is opt-in. | Build every registered executable before CTest; inspect `ctest --test-dir <build> -N`. A discovered test is not necessarily built. |
| Scene integration | Basic quoted name and Transform project round-trip; malformed-file scene preservation. | Both save entry points: empty/multiple entities, nested hierarchy, duplicates/renames, stable IDs, UTF-8/control escapes, models/primitives/textures/materials/lights/colliders/bodies/scripts/audio and supported gameplay config. |
| Failure/data integrity | Selected malformed network/config/project inputs. | Invalid/truncated JSON, unknown schema/component/asset, unsafe paths, failed construction/write/replace and backup recovery. Assert old scene/config/file unchanged on failure. Legacy migration must be explicit and deterministic. |
| Startup/headless | No application/server end-to-end smoke certified by this audit. | Finite startup/exit switches with nonzero failures; no-backend server must fail informatively; real backend server must accept and stop cleanly. |
| Graphics | Ownership contracts; no image baseline certified. | Shader failure retains previous program; GL debug log, FBO resize/minimize/restore, post-process state, hierarchy visibility, terrain samples, shadows/color/gamma reference images. |
| Performance | Telemetry/statistics contracts and opt-in allocator/ECS microbenchmarks. | Reproducible representative gameplay scene; average/P95/P99, CPU phases, memory, startup and actual submissions. GPU results only with timer-query evidence. |

Run Debug and Release independently using the commands in [BUILD_GUIDE.md](BUILD_GUIDE.md). Preserve logs and explicitly report optional SDL_net/OpenAL availability. Run `ctest --test-dir <build> --output-on-failure -C <config>` after a successful build. Renderer tests need their own enabled option; benchmarks are not functional acceptance tests.

## Manual/system scenarios still requiring real execution

1. **A / editor lifecycle:** Open, create/move/rename, save, close, reopen; verify properties and hierarchy. Move/delete/undo and script deletion must not dereference stale selection.
2. **B / scene integrity:** Persist representative scene through both APIs and restart. Missing assets must report their paths and preserve references, not silently substitute cubes.
3. **C / gameplay boundary:** Play, move/collide/fire, pause/resume, Stop; runtime entities disappear and authored state returns. Repeat 20 cycles with selections and scripts.
4. **D / resources:** Repeated scene load/unload and burst projectiles/audio; track process memory plus live native/GL/Mono/audio ownership. A rising allocator high-water mark alone is not proof of a leak.
5. **E / networking:** Launch one dedicated server and two separate clients with actual backend. Verify distinct identity, spawn, authoritative movement, snapshots, disconnect, reconnect and rejection of malformed/oversized/unauthorized input. Protocol unit tests alone do not fulfill this scenario.
6. **F / distribution:** Package sample game; launch in clean deployment directory from unrelated CWD with source/build paths unavailable. Verify startup scene, controls, assets, managed code and dependency diagnostics.
7. **G / failure recovery:** Corrupt config/save/shader/script, remove an asset/SDK, deny save destination; errors and exit status must reflect real failure, with old state/data intact.
8. **H / timing/performance:** Same deterministic input/seed at 30/60/120/144/240 rendering FPS plus stalls; compare gameplay time, shots, jumps and trajectories. Capture same workload before/after optimizations.

## Completion evidence

Each repaired defect links its audit ID to a focused regression or a concrete testing limitation, the exact configure/build/test command, exit status, test count and runtime artifacts where applicable. Record sanitizers/Windows diagnostic tool availability explicitly. Interactive/audio/two-process/image tests remain pending until performed; do not convert source inspection into runtime PASS.
