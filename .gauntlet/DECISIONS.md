# Decisions

## 2026-08-18 — Start with weapon correctness

The first work unit is the disconnected weapon progression path because it is a confirmed, user-visible FPS correctness failure with a small scheduling fix and deterministic validation surface. Measurement-only performance hypotheses remain behind correctness defects.

## 2026-08-18 — Do not treat console profile output as data

The current console benchmark/profile commands print constants or derived placeholder splits. GAUNTLET decisions will use build/test output, focused benchmarks, or newly implemented truthful instrumentation only.

## 2026-08-18 — Preserve the current architecture during initial fixes

No broad ECS, renderer, threading, or singleton rewrite is justified by the Phase Zero evidence. Initial changes will use existing ownership and scheduling patterns and remain independently reversible.

## 2026-08-18 — SDL_net absence is baseline state

Protocol and safe no-backend tests passing does not mean multiplayer is operational. Runtime networking remains unavailable until an explicit backend/compatibility work unit is completed.

## 2026-08-18 — Resolve scheduled entities through handles

The first weapon patch cached a raw Player pointer and failed adversarial review because editor deletion could invalidate it. Fixed-tick scheduling now stores the serialized handle value and resolves it through Scene on every use. Raw entity pointers must not be retained across mutation boundaries.

## 2026-08-18 — Preserve intended inventory initialization

A default-boundary regression test proved that `InitInventory()` overwrote the rifle preset by switching from the already-default Rifle enum. Initialization now selects the populated preset directly. Boundary tests should use production defaults when the defect concerns default gameplay.

## 2026-08-18 — Correct mouse edges before telemetry

Post-iteration recon ranked the default editor's broken mouse-edge contract above observability work for one bounded iteration: impact is immediate, evidence is certain, and the implementation mirrors proven keyboard state. Existing work-unit IDs remain stable, so this work is `GAUNTLET-0008`; `GAUNTLET-0003` remains truthful telemetry.

## 2026-08-18 — Scope frame telemetry to claims the engine can support

The first frame metric covers main-thread CPU work from input through renderer end and deliberately excludes swap/vsync and the limiter. Render counters cover only accepted instances, distance rejects, and the RenderSystem's main/shadow instanced submissions. GPU time, total draw calls, triangles, and VRAM remain unavailable until instrumentation can observe them completely. No scoped counter may be presented as a whole-frame total.
