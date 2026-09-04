# GAUNTLET State

Last updated: 2026-08-18

## Current status

- Phase: RECON
- Baseline revision: `371c71fd11e25d30e61e5e88e7cdfca05ad4d64c`
- Last integrated work unit: `GAUNTLET-0003` — truthful frame and render instrumentation
- Active work unit: `GAUNTLET-0009` — truthful network console contract
- Baseline build: PASS (Debug, NMake/MSVC, 316.216 s)
- Baseline tests: PASS (4/4, 0.30 s)
- Runtime networking: unavailable because SDL_net is not installed

## Current invariants

- Preserve the 128 Hz application simulation tick unless a separately measured work unit changes it.
- Scene/entity mutation remains main-thread affine.
- OpenGL resource owners must be destroyed before the window/context.
- Performance claims require measurements; simulated console values are not evidence.
- Each work unit must remain isolated, reversible, and validated.

## Next queue

1. `GAUNTLET-0009` — truthful network console contract.
2. `GAUNTLET-0004` — runtime networking contract.
3. `GAUNTLET-0005` — cache editor Project panel enumeration.
