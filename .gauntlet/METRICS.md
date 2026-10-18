# Metrics

## Stabilization cycle — 2026-10-18

Starting revision `662d628`; changes remain uncommitted. Environment: Windows x64,
Visual Studio 2022, MSVC 19.44, Windows SDK 10.0.26100.0, CMake 4.2.3, C++17.

- Initial discovery in reused build directory: 12 tests, 11 unbuilt executables;
  this was Not Run, not evidence of 11 runtime regressions.
- Enabled ECS/physics/renderer tests; fresh Debug all-target build exited 0.
  Baseline CTest: 14/14 passed, 0.78 s.
- Final Debug all-target build exited 0; CTest: 21/21 passed, 2.17 s.
- Final Release all-target build exited 0; CTest: 21/21 passed, 2.74 s.
- 19 contracts plus 2 startup checks: Release diagnostics; directory cache;
  deletion/scene-safe undo; mesh recipes/OBJ/FBX/moves; shared project/save codec
  rollback/validation/backup; existing input/gameplay/ECS/physics/network contracts.
- Actual OpenGL startup used NVIDIA GeForce RTX 4060 Laptop GPU, OpenGL 3.3;
  each config rendered exactly 16 frames and completed normal destruction.
  Unsupported Vulkan without fallback failed intentionally with a useful error.
- Headless server `--help` exited 0; regular no-SDL_net startup exited 2 and
  named the missing backend. No multiplayer simulation or two-client run claimed.
- `BUILD_TESTING=OFF` fresh configuration discovered 0 tests; presets parsed.
- An intermediate Release build failed while shared-codec declarations were
  incomplete; declarations and test link dependencies were corrected before the
  successful final Debug/Release builds. No intermediate failure is counted PASS.
- Existing unused-variable/parameter and third-party compiler warnings remain;
  the added Windows-header macro warning was corrected. Build is not warning-free.
- Commands: docs/BUILD_GUIDE.md. Logs: ignored build-codex-graphics-api/
  baseline-debug.log, final-debug.log, final-release.log and Testing/Temporary.

No performance benchmark, cache FPS improvement, gameplay percentile/memory
growth result, visual comparison, real audio, managed reload, packaged game or
authoritative multiplayer acceptance is claimed. SDL_net absent; OpenAL off.

The historical measurements below are retained for provenance only.

## Baseline — 2026-08-18

Revision: `371c71fd11e25d30e61e5e88e7cdfca05ad4d64c`

### Build

- Configuration: Debug, NMake, x64 MSVC 19.44, C++17
- Result: PASS
- Duration: 316.216 s
- Warnings: 11 existing warnings
- Errors: 0
- Post-build Mono copy: 5,268 files, about 500 MB

### Tests

- CTest: 4/4 PASS, 0 failed
- Duration: 0.30 s
- Coverage areas: network protocol, safe no-backend network behavior, ECS core, physics core

### Microbenchmarks

Single runs; not statistically sampled and not representative gameplay metrics.

| Benchmark | Baseline | Comparison | Reported ratio |
|---|---:|---:|---:|
| Allocator | pool 12.478 ms | new/delete 65.233 ms | 5.228x |
| ECS storage | SoA 18.103 ms | AoS 81.646 ms | 4.510x |

### Missing metrics

- Average, P95, and P99 gameplay frame time
- CPU phase timing
- GPU frame/pass timing
- Allocations per frame outside custom allocators
- Reliable draw, triangle, instance, and upload counters
- Representative physics, projectile, networking, and stress scenes
- Startup time excluding artificial splash sleeps

Hard-coded console statistics are explicitly excluded from this history.

## After GAUNTLET-0001 — 2026-08-18

- Debug `ArchuraEngine` build: PASS.
- Registered tests in `build_ramp_verify`: 6/6 PASS in 0.13 s.
- New focused weapon progression test: PASS.
- Existing build warnings observed; no new warning category attributed to the work unit.
- Performance metrics: not applicable; this was a gameplay correctness change.

## After GAUNTLET-0002 — 2026-08-18

- Debug `ArchuraEngine` build: PASS.
- Registered tests in `build_ramp_verify`: 7/7 PASS in 0.09 s on final commander run.
- New focused gameplay scroll test: PASS.
- Validated fixed-tick counts per render frame: 0 and 5 produce the same single FOV step.
- Performance metrics: not applicable; this was input determinism work.

## After GAUNTLET-0008 — 2026-08-18

- Debug `ArchuraEngine` compile/link: PASS.
- Registered tests in `build_ramp_verify`: 8/8 PASS in 0.09 s on final commander run.
- New focused mouse-button transition test: PASS.
- Performance metrics: not applicable; this was editor/input correctness work.

## After GAUNTLET-0003 — 2026-08-18

- Debug `ArchuraEngine` compile/link and post-build resource staging: PASS.
- Registered tests in `build_ramp_verify`: 9/9 PASS in 0.12 s on the final commander run.
- New focused frame-telemetry test: PASS.
- Runtime CPU metric capability: bounded 512-frame average/P95/P99 for main-thread work from `ProcessInput` through `Renderer::EndFrame`, excluding swap/vsync and the FPS limiter.
- Runtime scene counters: visible instances accepted into batches, distance rejects, and scoped main/shadow instanced submissions.
- GPU time, total draws, triangles, and VRAM remain explicitly unavailable.
- No representative interactive runtime sample was gathered; no frame-time improvement is claimed.
