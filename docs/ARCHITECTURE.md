# Repository architecture and maintenance boundaries

## Responsibilities

`Application` is the composition root for window/input, scene, runtime services,
editor and networking. Entity lifetimes and persistent identities belong to
`Scene`, not to editor panels or history. Runtime systems consume ECS components;
authoring operations go through the editor's document guards and command journal.

The editor is split by workflow:

- `Editor.cpp`: lifecycle, layout, menu/hierarchy, viewport interactions and shortcuts.
- `EditorInspector.cpp`: component controls and persistent text drafts.
- `EditorHistory.cpp`: authoring command execution, selection restoration and Undo/Redo.
- `EditorSceneIO.cpp`: authored-document persistence and guarded scene actions.
- `EditorProjectIO.cpp`: project workspace and recent-project transitions.
- `internal/EditorPrimitives.h`: shared primitive/collider defaults; not a public API.

Editor fields are private. Callers use selection, document, project and history
APIs; the opt-in integration fixtures retain narrowly declared friend access.
The stdout adapter is private to the editor translation unit. History commands
store CPU values/file recipes, not entity or GPU owners; rendering resource
lifetimes and ImGui frame preview owners remain separate concerns.

## Build and test boundaries

The root CMake file assembles SDKs, engine/server/launcher targets and staging.
`cmake/NativeValidation.cmake` owns native contracts, optional benchmarks and
startup/process test registration. The SDK-free loop path returns before SDK
lookup. Source/header directory globs use `CONFIGURE_DEPENDS`, so new editor files
are discovered rather than silently omitted from incremental builds.

The large private-access editor fixtures live under `tests/support/editor`.
Only `ARCHURA_BUILD_STARTUP_TESTS=ON` links their suite translation unit. Runtime
code knows the small `StartupProbeSupport.h` bridge and rejects unavailable probes;
normal editor launches do not depend on test fixture bodies. `ConsoleCommand.cpp`
is explicitly compiled so its public numeric/bool conversion API has definitions.

Windows batch filenames remain compatibility entrypoints. Implementation belongs
to `tools/windows/engine.ps1`; it builds current presets, launches matching runtime
outputs and delegates packaging/validation to their existing Python tools. Build
scripts must not copy archived EXEs into another build or delete distribution trees.

## Evidence-based cleanup (2026-11-01)

Eight tracked files were removed only after source, build, test and example
reference checks:

- The unused `ConsoleInterface` pair: an unbuilt blocking stdin implementation.
- Legacy `Allocator.h`, `PoolAllocator` and `StackAllocator` files: unbuilt copies.
  The maintained `Archura::Memory` implementations in `Allocators.h`, tests and
  examples remain active.
- The unreferenced `HeadlessRuntime.cmake`: depended on a missing module and
  contained invalid syntax. It was not a supported Linux build path.

Old design notes, server overview and release notes are preserved under
`docs/archive` and `docs/releases`, with their historical status made explicit.
Redundant object-by-object ignore entries were collapsed into the existing build
folder rule. No third-party SDKs, authored assets, local build trees, logs or
historical binary archives were deleted. Include searches alone cannot establish
that optional runtime dependencies or packaged assets are unused.

## Remaining review areas

This is a measured structural improvement, not a proof that every module is fully
decoupled. `Editor.cpp` still coordinates several panels and viewport gestures;
`Application` still combines bootstrap and application-loop coordination. Further
extraction should follow ownership and lifecycle boundaries, not arbitrary line
counts. Physics, networking, mesh import and the command palette are sizeable but
need behavior-specific reviews before broad rewrites. Optional OpenAL and
experimental SoA code have real conditional/example references and are retained.

Use `python tools/validate_local.py` for the provisioned Windows Debug/Release
gate. Check the contracts and SDK-free profiles after build-system changes, and
verify normal startup-test-disabled builds separately when changing probe wiring.

## Validation of this refactor

The local `all` profile passed with unchanged source during the run:

| Profile | Debug | Release |
| --- | --- | --- |
| Full desktop | 58/58 | 58/58 |
| Native contracts | 48/48 | 48/48 |
| SDK-free loop | 1/1 | 1/1 |

The normal startup-test-disabled Release build also completed for the engine,
server and launcher through `Build.bat`. Its normal OpenGL smoke exited 0, while
`--play-stop-smoke-test` exited 1 with the expected disabled-probe explanation.
The new package wrapper created a sample package and its standalone OpenGL smoke
exited 0. `BUILD_TESTING=OFF` registered zero tests. Batch/PowerShell dry-runs,
space-containing paths and failure exit propagation were checked separately.

Evidence is retained under ignored `build/local-validation/20261101T131836.469806Z-e9217fb7/`
and `build/structure-*` logs/reports. These checks establish the exercised Windows
behavior and build boundaries; they are not a Linux run or proof of every possible
runtime path. Follow-up reviews should also examine the legacy empty `Engine`
loop methods and application/world-bootstrap responsibilities before changing
those APIs.
