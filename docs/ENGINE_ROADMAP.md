# Engine stabilization roadmap

Cycle 2026-10-18 implemented shared validated transactional serialization, mesh
source preservation, safe transform undo, async Project cache, startup coverage
and build/test controls. Debug and Release each pass 21/21 checks. This is a
bounded stabilization cycle; the engine objectives remain incomplete.

Next: persistent scene identifiers and explicit system-entity ownership metadata,
then runtime-component coverage and safe Play/Stop scene restoration. Add an
aggregate resource reconstruction budget before accepting untrusted large scenes.

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
