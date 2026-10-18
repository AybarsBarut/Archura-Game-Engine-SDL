# GAUNTLET State

Last updated: 2026-10-18

## Current status

- Phase: bounded stabilization cycle verified; broader engine work incomplete
- Starting revision: `662d628` (clean working tree before this cycle)
- Initial stabilization pushed to origin/main as `3d1cd9d`; ownership follow-up
  is recorded in the subsequent integration commit (see Git history).
- Fresh baseline: Debug all targets PASS; 14/14 contracts PASS (0.78 s)
- Final build: Debug and Release all targets PASS (VS2022/MSVC x64, C++17)
- Latest tests: Debug 21/21 PASS (1.84 s), Release 21/21 PASS (1.88 s)
- Runtime: both configurations rendered 16 OpenGL frames and shut down; unsupported backend failed as expected
- Headless server: help PASS; no-backend startup exited 2 with explicit error
- Runtime networking: unavailable because SDL_net is absent; no authoritative simulation implemented
- OpenAL enabled configuration, full editor lifecycle, audio, Mono reload, image regressions and packaged deployment: untested

## Completed in this cycle

- Shared versioned scene codec for ProjectSerializer and GameSaveManager, complete
  bounded JSON parsing/schema preflight, legacy inputs, staged entity/resource
  creation with rollback, atomic save replacement and preceding-save backup.
- Hierarchy, supported native/audio configuration, original imported mesh paths
  and exact procedural recipes persist. Unknown components, custom/edited meshes,
  assigned shaders, generated textures and excluded-system hierarchy edges fail
  explicitly without replacing the prior save. See docs/serialization.md.
- Transform undo uses generation handles plus owner Scene identity; deleted
  entities/components and wrong-scene history entries are safely discarded.
- Project panel filesystem work moved to a joined worker with immutable snapshots,
  navigation/manual invalidation, polling and visible errors; no speedup measured.
- Startup failure/exception status, bounded startup tests and Release warning/error
  logging; CMake presets/dependency diagnostics/BUILD_TESTING controls; batch CWD fix.
- Required audit, roadmap, build guide and test plan created; history retained.
- Follow-up: EntityOwnership explicitly distinguishes engine bootstrap services
  from authored content. User Player/Floor names no longer disappear; renaming
  an engine service cannot cause it to be deleted on content reload. `.project`
  persists ownership; legacy records default to authored; `.scene` rejects service
  promotion. Both cross-boundary hierarchy guards remain in force.

## Current invariants

- Preserve the 128 Hz application simulation tick unless a separately measured work unit changes it.
- Scene/entity mutation remains main-thread affine.
- OpenGL resource owners must be destroyed before the window/context.
- Performance claims require measurements; simulated console values are not evidence.
- Each work unit must remain isolated, reversible, and validated.

## Next queue

1. Add persistent scene identifiers independent of runtime handles, preserving
   legacy hierarchy migration, transactional staging and live handle validity.
2. Cover gameplay configuration (Weapon etc.), editable geometry/shader materials;
   add aggregate resource budgets and project-relative asset resolution.
3. Validate all editor selections; implement authored-state Play/Stop restoration,
   including runtime script/audio/physics cleanup and stable Player references.
4. Separate runtime packaging and validate a sample game outside the repository.
5. Enable/test SDL_net, then implement authoritative world/input/snapshot vertical
   slice with a dedicated server and two real clients.

Do not treat old backlog statements about fabricated networking/no parser as
current evidence. Audit confirms transport pumping and truthful console already
exist. Shader, terrain, audio and timing findings remain queued in ENGINE_AUDIT.
