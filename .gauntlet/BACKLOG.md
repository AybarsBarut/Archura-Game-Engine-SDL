# Backlog

Priority is conceptual: impact x confidence x frequency / risk / cost.

## Critical

### GAUNTLET-0009 — Make the network console truthful

- Category: Network / Correctness / Tooling
- Evidence: the live editor console reports random ping, invented traffic, unconditional connection success, and a hard-coded server/map/player status without calling `NetworkManager`.
- Benefit: operators see real transport state and the explicit SDL_net-unavailable failure instead of false success.
- Risk: low; keep the change to validated command routing and pure state/stat formatting without claiming functional multiplayer.

### GAUNTLET-0004 — Make runtime networking contract truthful

- Category: Network / Correctness
- Evidence: client/host networking is not pumped; SDL_net is absent; server cannot start.
- Benefit: prevents false operational state and defines supported behavior.
- Risk: medium; protocol and documentation compatibility must be preserved.

## High

### GAUNTLET-0005 — Cache editor Project panel enumeration

- Category: Editor / Performance
- Evidence: the default-visible panel enumerates and sorts the filesystem every rendered frame.
- Benefit: removes blocking I/O and allocations from the editor frame path.
- Risk: medium; cache invalidation must be correct.

### GAUNTLET-0006 — Fix false-success loaders

- Category: Serialization / Correctness
- Evidence: ServerConfig and ProjectSerializer load functions return success without parsing.
- Benefit: callers can trust results and handle unsupported input.
- Risk: low if false-success changes are covered by tests.

### GAUNTLET-0007 — Repair fallback audio lifecycle

- Category: Audio / Correctness / Performance
- Evidence: managed play does not open aliases, one-shots leak aliases, preview can double-prefix paths, and fixed ticks can issue synchronous MCI work.
- Benefit: correct playback and bounded main-thread work.
- Risk: medium; Windows MCI and optional OpenAL behavior differ.

## Medium

- Measure render-batch construction, duplicate instance uploads, and uniform-name allocation before changing them.
- Add physics proxy/contact and character-query benchmarks before spatial-index work.
- Remove empty-script tick allocations with a measured no-script and scripted baseline.
- Add render interpolation only with a defined previous/current transform contract.
- Replace artificial startup sleeps with real progress or a zero-delay bootstrap.
- Rate-limit debug shader filesystem timestamp checks.
- Batch particle destruction after profiling burst expiration.

## Low / deferred

- Eliminate redundant per-frame drawable-size refreshes.
- Improve software limiter precision after frametime instrumentation exists.
- Replace distance-only "frustum" culling with world-space frustum tests.
- Split god objects only alongside concrete correctness, testability, or performance benefits.

## Completed

### GAUNTLET-0003 — Truthful frame instrumentation

- Result: INTEGRATED on 2026-08-18.
- CPU work average/P95/P99 now use a bounded 512-frame sample window with an explicit pre-swap/pre-limiter scope.
- Last-frame visible, distance-culled, and main/shadow scoped batch submissions are live counters; unavailable totals remain labeled unavailable.
- Fabricated render/profile/benchmark statistics were removed, and statistics dumps verify actual file writes.
- Debug engine build and 9/9 registered tests passed; adversarial review approved.

### GAUNTLET-0001 — Restore weapon progression

- Result: INTEGRATED on 2026-08-18.
- Weapon cooldown/reload state now advances once per unpaused 128 Hz tick.
- Player identity is resolved through a generation-checked handle, avoiding editor-deletion use-after-free.
- Editor clicks cannot fire while the gameplay cursor is unlocked.
- Intended default rifle inventory selection is preserved.
- Debug engine build and 6/6 registered tests passed; adversarial review approved.

### GAUNTLET-0002 — Deterministic wheel/FOV input

- Result: INTEGRATED on 2026-08-18.
- Gameplay scroll now runs once in the render-frame input path, never in fixed movement ticks.
- Editor/unlocked cursor input remains isolated from game-camera FOV.
- Debug engine build and 7/7 registered tests passed; adversarial review approved.

### GAUNTLET-0008 — Correct mouse button edge semantics

- Result: INTEGRATED on 2026-08-18.
- `Pressed` and `Released` are non-consuming per-frame transition latches; `Down` remains level state.
- Same-frame tap/repress ordering is preserved and focus loss safely releases held buttons.
- Debug engine link and 8/8 registered tests passed; adversarial review approved after rework.
