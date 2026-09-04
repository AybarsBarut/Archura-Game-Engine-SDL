# GAUNTLET Iterations

## GAUNTLET ITERATION #1

Problem:
Weapon cooldown and reload state were never scheduled, so the default player could not fire or finish a reload.

Root Cause:
`WeaponSystem::Update` contained the only timer progression but had no runtime caller. Initial inventory selection also overwrote the intended rifle preset. The first repair attempt exposed unsafe raw Player lifetime and editor-click firing, both caught by adversarial review.

Agent:
Gameplay/Test Engineer, GAUNTLET Commander rework, independent Adversarial Reviewer.

Files Changed:
`CMakeLists.txt`, `src/core/Application.cpp/.h`, `src/game/FPSController.cpp`, `src/game/Weapon.cpp/.h`, `tests/WeaponProgressionTests.cpp`.

Implementation:
Schedule weapon progression once before held-fire evaluation on each unpaused 128 Hz tick; resolve Player through a generation-checked handle; block fire during reload; require cursor lock; preserve the initialized rifle preset.

Tests:
Focused cooldown, automatic cadence, default rifle boundary, reload/ammo conservation, and destroyed-handle coverage. Debug engine build PASS. Registered suite 6/6 PASS.

Performance Before:
Not applicable; correctness work unit.

Performance After:
Not applicable; correctness work unit.

Regression Status:
PASS.

Reviewer:
First review rejected a raw-pointer use-after-free and editor firing regression. Reworked patch received APPROVE with no remaining merge blocker.

Decision:
INTEGRATED.

Next Highest Priority:
`GAUNTLET-0002` — make mouse-wheel/FOV input deterministic across render rates.

## GAUNTLET ITERATION #2

Problem:
Mouse-wheel input was sampled once per render frame but applied in each fixed simulation tick, so a single event could be dropped or replayed depending on render rate.

Root Cause:
FOV adjustment lived in `FPSController::HandleMovement`, while SDL input reset/event accumulation and mouse-look routing operate per render frame.

Agent:
Gameplay/Input/Test Engineer and independent Adversarial Reviewer.

Files Changed:
`CMakeLists.txt`, `src/game/FPSController.cpp`, `tests/GameplayScrollTests.cpp`.

Implementation:
Move gameplay scroll/FOV handling into the once-per-render-frame mouse-look path under the gameplay cursor-lock guard; remove it from fixed movement ticks.

Tests:
Focused zero-tick versus five-tick frame equivalence, next-frame reset/no-repeat, and unlocked-cursor suppression. Debug engine build PASS. Registered suite 7/7 PASS.

Performance Before:
Not applicable; deterministic input correction.

Performance After:
Not applicable; deterministic input correction.

Regression Status:
PASS.

Reviewer:
APPROVE; no merge-blocking finding. Editor Edit-mode routing, pause behavior, and fixed-tick isolation verified.

Decision:
INTEGRATED.

Next Highest Priority:
Return to TRIAGE; truthful metrics remain the leading tooling prerequisite.

## GAUNTLET ITERATION #3

Problem:
Mouse `Pressed` returned held state and `Released` returned idle state, violating editor picking and gizmo activation contracts and allowing held state to stick across focus loss.

Root Cause:
Input retained only current mouse levels. The first repair used previous-frame comparison, which adversarial review proved could not represent multiple transitions queued in one render frame.

Agent:
Input/Test Engineer and independent Adversarial Reviewer.

Files Changed:
`CMakeLists.txt`, `src/input/Input.cpp/.h`, `tests/InputMouseButtonTests.cpp`.

Implementation:
Add non-consuming per-frame pressed/released transition latches reset at frame start, keep level state for `Down`, ignore duplicate events, and release held buttons on focus loss.

Tests:
Cross-frame lifecycle, repeated queries, both same-frame transition orders, duplicate events, LMB/RMB focus loss, next-frame reset, and invalid indices. Debug engine compile/link PASS. Registered suite 8/8 PASS.

Performance Before:
Not applicable; correctness work unit.

Performance After:
Not applicable; correctness work unit.

Regression Status:
PASS.

Reviewer:
Previous-state patch rejected for lost same-frame taps and stuck focus-loss state. Latch rework APPROVED with no remaining blocker.

Decision:
INTEGRATED.

Next Highest Priority:
`GAUNTLET-0003` — bounded truthful frame telemetry and honest console reporting.

## GAUNTLET ITERATION #4

Problem:
Rendering statistics, benchmark scores, and profiler phase splits were fabricated constants, while the renderer's old counters were dead. The engine had no trustworthy average or tail-latency frame metric for prioritizing performance work.

Root Cause:
Console commands were presentation stubs disconnected from runtime state, and no bounded main-thread frame sample history existed.

Agent:
Core/Rendering/Test Engineer, GAUNTLET Commander audit, and independent Adversarial Reviewer.

Files Changed:
`CMakeLists.txt`, `src/core/Application.cpp/.h`, `src/core/FrameTelemetry.cpp/.h`, `src/game/FPSConsoleCommands.cpp`, `src/game/RenderSystem.cpp/.h`, `tests/FrameTelemetryTests.cpp`.

Implementation:
Add an allocation-free O(1) 512-sample record path with on-demand snapshots and nearest-rank percentiles; measure from before input through renderer end; keep swap/vsync and limiting outside the scope; expose explicitly scoped scene batching counters; replace fabricated reporting with live values or `unavailable`; make statistics dumping verify open/write/close.

Tests:
Empty and reset states, known averages and percentiles, unsorted duplicates, 600-sample ring wrap, invalid samples, truthful formatting, scoped counters, and absence of previous fabricated values. Debug engine compile/link and resource staging PASS. Registered suite 9/9 PASS in 0.12 s on the final commander run.

Performance Before:
No trustworthy frame distribution was available; console values were excluded as evidence.

Performance After:
The runtime can report a bounded CPU-work average/P95/P99 and scoped scene batching counters. No interactive scene sample was captured, so no performance improvement is claimed. The per-frame record path is fixed-cost; sorting and formatting occur only on report requests.

Regression Status:
PASS.

Reviewer:
APPROVE; exact measurement boundary, ring math, counter scope, file failure handling, and bounded hot-path overhead verified with no merge blocker.

Decision:
INTEGRATED.

Next Highest Priority:
Return to TRIAGE with the runtime networking contract, false-success loaders, and Project panel enumeration as leading candidates.
