# Regressions

No integrated GAUNTLET regressions are known at the Phase Zero baseline.

## GAUNTLET-0001 protections

- Default rifle cannot fire before tick 13 at 128 Hz and can fire on tick 13.
- Successive automatic shots respect cooldown.
- Reload blocks firing, completes at its configured tick, and conserves ammunition.
- A destroyed Player handle is safely ignored by the weapon scheduler.
- Gameplay firing requires cursor lock, preventing editor selection clicks from shooting.

## GAUNTLET-0002 protections

- A wheel event changes gameplay FOV once with either zero or five fixed ticks in the render frame.
- The next frame does not replay the previous scroll delta.
- An unlocked cursor does not modify gameplay-camera FOV.

## GAUNTLET-0008 protections

- Pressed/released edges are non-consuming within a frame and reset next frame.
- Same-frame down/up and held up/down retain both transitions.
- Duplicate SDL button events do not fabricate transitions.
- Focus loss clears held LMB/RMB and produces a one-frame release edge.
- Held gameplay/editor actions continue to use `IsMouseButtonDown` level state.

## GAUNTLET-0003 protections

- The fixed telemetry ring retains the newest 512 valid samples and separately tracks all observed frames.
- Average and nearest-rank P95/P99 remain correct across wraparound, duplicate values, invalid inputs, and reset.
- CPU scope remains input through renderer end, excluding swap/vsync and the software limiter.
- Scene submission counters remain explicitly scoped; GPU time, total draws, triangles, and VRAM cannot silently acquire fabricated values.
- Statistics dumps must fail visibly when open, write, or close fails.

## Watch list

- Weapon cooldown/reload cadence at the 128 Hz application tick.
- Input edges and deltas across render rates from 30 to 240 FPS.
- Scene/entity handle validity during deferred destruction.
- OpenGL resource destruction before context teardown.
- Physics deterministic contact order and fixed-step spiral bound.
- Network no-backend mode must remain explicit and safe.

Failed tests, reverted work units, and post-integration regressions must be recorded here with revision, reproduction, and rollback information.
