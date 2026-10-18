# Engine stabilization roadmap

Cycle 2026-10-18 implemented shared validated transactional serialization, mesh
source preservation, safe transform undo, async Project cache, startup coverage
and build/test controls. Debug and Release each pass 21/21 checks. This is a
bounded stabilization cycle; the engine objectives remain incomplete.

Follow-up implemented: explicit engine/authored ownership replaces display-name
exclusions. Authored Player/Floor content persists, renamed bootstrap services
survive content loads, project ownership round-trips and legacy records migrate.

## Next implementation queue

| Priority | Concrete change / affected files | Acceptance |
|---|---|---|
| P0, next | Separate persistent scene IDs from runtime handles in Entity/Scene and SceneData. Stage ID restoration without colliding with the old live scene; migrate legacy numeric hierarchy references. | Save/load/save preserves persistent IDs and hierarchy; new creations never collide; old runtime handles remain invalid; duplicate IDs reject transactionally. |
| P0 | Serialize Weapon gameplay configuration, then implement the authored Play/Stop boundary in Application/Editor/ScriptSystem. Clear selection/history and transient audio/physics/script ownership at the boundary. | Default authored game scene saves; Edit does not simulate; repeated Play/Stop restores all authored values and removes runtime-only entities. |
| P1 | Add configurable aggregate mesh/texture reconstruction budgets and project-root asset resolution in the shared codec/ResourceManager. | Excessive scenes fail before unbounded allocation; missing/unsafe assets retain old state; relocation and duplicate filenames are tested. |
| P0/P1 | Extract standalone runtime startup and implement a manifest-driven builder with required dependencies. | A sample game launches from a clean directory outside the repository without opening the editor. |
| P0 | Enable real SDL_net validation, then authoritative input/world/snapshot modules and client consumption. | One dedicated server and two separate clients replicate movement/spawn/despawn and reject invalid input. |

Keep these as sequential acceptance gates. Performance, renderer and UI expansion
follow the measured/reproducible work units below; no FPS improvement is inferred
from a structural change alone.

Updated 2026-10-18. Implementation status and evidence: [ENGINE_AUDIT.md](ENGINE_AUDIT.md), [TEST_PLAN.md](TEST_PLAN.md), `.gauntlet/STATE.md` and `.gauntlet/METRICS.md`. A source implementation is not a completed acceptance test.

| Order | Bounded work unit | Exit condition |
|---|---|---|
| 0 | Reproduce current Windows x64 Debug/Release configure/build and discovered CTest baseline; audit optional SDK/runtime staging. | Exact commands, compiler/configuration, counts, failures and environmental limitations recorded. |
| 1 | Shared versioned scene codec, structured parsing, validated legacy migration; asset/primitive descriptors and supported native components; staged construction and atomic file replacement. | Round-trip and failure preservation regressions pass through both project/save APIs; unsupported state fails visibly. |
| 2 | Remove editor undo/selection stale pointers; establish Edit/Play snapshot, runtime initialization/cleanup and Stop restoration. Cache Project directory with explicit invalidation. | Delete/undo/reload safe; Play changes disappear after Stop; idle Project rendering performs no repeated scan. |
| 3 | Repair real audio playback routing/path resolution and source ownership; address measured fixed-tick MCI overhead; define script assembly reload support. | C# Play reaches configured backend; repeat scene shutdown has no orphan voices/instances; exceptions and missing dependencies observable. |
| 4 | Separate game startup/runtime from editor; manifest, startup scene, required scripts/shaders/assets/DLL staging; sample project. | Game launches and plays from clean directory and unrelated working directory without opening editor. |
| 5 | Add independent authoritative world/input/snapshot module atop existing bounded TCP transport; connect client consumption and spawn/despawn. | One server/two client movement and malformed-input acceptance run with actual SDL_net. No UDP/prediction claims. |
| 6 | Define previous/current transform render contract while preserving 128 Hz; verify 60 Hz physics interaction. Repair hierarchy render culling and terrain sampling. | Cross-FPS gameplay assertions and representative render/terrain regressions pass. |
| 7 | Measure CPU phase/GPU/resource baselines; optimize only demonstrated hot spots; introduce configurable low-end quality settings. | Same-scene before/after timings, memory and correctness recorded; no artificial metrics. |
| 8 | Complete retained runtime UI only after core gates. Then assess prefabs, reflection/material tooling, animation graphs, streaming and map packages. | A small functioning feature with integration and tests before expanding scope. |

Main-thread Scene/Mono and render-owner OpenGL access, backward-compatible saves, explicit unsupported backends and the 128 Hz application tick are invariants. Keep central build/integration ownership exclusive. Never count historical PASS reports or missing tests as present-day runtime evidence.
