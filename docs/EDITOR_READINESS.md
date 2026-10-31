# Editor usability and readiness

This is a source-based comparison with Aeon Engine as of 2026-10-29, not a
claim that either engine is production ready. Archura retains C++/SDL/OpenGL;
matching authoring quality does not require porting to Rust or IrisUI.

## Reference capabilities

Aeon's [dock builder](https://github.com/AethelisDEV/aeon-engine/blob/main/crates/ae_editor_ui/src/ui/iris_bridge/native_dock/builder.rs)
and [panel layout](https://github.com/AethelisDEV/aeon-engine/blob/main/crates/ae_editor_ui/src/ui/panel_layout.rs)
provide tabbed panels, splitters, floating panels and layout reset.
Its [theme tokens](https://github.com/AethelisDEV/aeon-engine/blob/main/crates/ae_editor_ui/src/ui/iris_bridge/theme.rs)
centralize surfaces, text and accent colors. Its
[asset toolbar](https://github.com/AethelisDEV/aeon-engine/blob/main/crates/ae_editor_ui/src/ui/iris_bridge/assets/toolbar.rs)
includes breadcrumbs, import/reveal, grid/list views and search.
[Scene I/O](https://github.com/AethelisDEV/aeon-engine/blob/main/crates/ae_editor_ui/src/processor/scene_io.rs)
uses native dialogs and queued save/load requests; its
[menus](https://github.com/AethelisDEV/aeon-engine/blob/main/crates/ae_editor_ui/src/ui/iris_bridge/menubar.rs)
disable unavailable actions (including New Project). These are inspected source
capabilities; this comparison did not build or run Aeon.

## Implemented and verified — 2026-10-29

- Dear ImGui v1.89.9 docking, with movable/tabbed panels, resize splitters,
  saved docking layout and View > Reset Workspace. Independent OS windows are off.
- Graphite theme, bundled font, clear empty Inspector, searchable hierarchy and assets.
  Hierarchy IDs no longer leak into displayed names.
- A Scene image with matching projection, logical-coordinate picking, clipped gizmos
  and terrain brush. Skybox and debug colliders use the same camera matrices.
- File > Open Scene / Save Scene / Save Scene As for `.gameproj`, with visible errors,
  absolute path normalization, replace/discard confirmation and Play-mode gating.
  This is a path-entry dialog, not an OS file picker or project hub.
- Edit > Undo Transform / Copy / Paste / Duplicate share the existing implementation.
  Inspector transform edits/resets capture undo, and per-editor name/script drafts
  follow scene lifetime, entity generations and external component changes.
- Mouse press coordinates survive coalesced down/move/up events; rapid gizmo drags
  apply their final release delta without selecting objects behind the handle.

Fresh `windows-validation` all-target builds passed in Debug and Release.
CTest passed **47/47** in Debug (40.14 s) and Release (27.59 s), including actual
OpenGL startup, Play/Stop, graphical network integration, packaging, viewport
projection/ray contracts, inspector lifetime and launcher discovery.
Evidence: ignored `build/editor-final-{build,test}-{debug,release}.log` files.

Native UI acceptance on the final Debug runtime verified:

- [x] Saved dock width survives restart; Reset Workspace restores default splits/tabs.
- [x] Create Sphere, edit Y=3, save, change Y=5, reopen, observe Y=3.
- [x] Select Sphere directly in the Scene after resizing the hierarchy split.
- [x] Rapid X gizmo drag changes X from 0 to 2.429; Edit > Undo Transform restores 0.
- [x] Missing file shows a visible error and Cancel returns to the unchanged scene.
- [x] File > Exit closes the runtime normally.
- [ ] Full keyboard-only authoring across focus changes and modal windows.
- [ ] Terrain sculpting and all inspector components exercised through native UI.
- [ ] Every screen resolution, DPI scale and undock/redock combination.

The UI checks use an ignored `WorkspaceAcceptance.gameproj` fixture in the Debug
runtime directory. Launcher discovery has eight filesystem cases; the launcher
itself was not run because its existing startup path performs Git update operations.

## Remaining product work

- Project creation/open/recent-project workflow and an explicit content root.
- Asset import/reimport, searchable previews, dependency diagnostics and missing-asset repair.
- Complete command history for creation, deletion, parenting and component changes.
  Existing transform undo is not general scene Undo/Redo.
- Recovery/autosave and explicit project session identity; document guards are implemented below.
- Integrated standalone export and a clean-machine package acceptance run.
- Complete native authoring coverage, especially keyboard focus, terrain tools, asset errors and small/high-DPI windows.

The launcher now searches documented preset outputs by executable modification time
with deterministic ties, falling back to sibling/legacy runtimes only when no
preset output exists. Builds use the documented Windows presets, and child
processes start in the selected runtime directory. Modification time is discovery
heuristics, not proof that an executable matches every current source file.
The launcher's existing Git auto-update behavior is unchanged.

## Current limits

Scene Save As serializes existing asset references; it does not copy or rebase
assets into the destination directory. Keep scene files with their project content;
use the existing manifest packager for a transferable game. Relative asset lookup
still has legacy fallback rules. An explicit project/content-root workflow is open.

The Scene view currently captures the full-resolution rendered backbuffer into a
GPU texture before drawing ImGui. This avoids CPU readback, but still adds a GPU
copy and renders at window resolution; it is not a viewport-sized render target or
a measured performance improvement. A target-sized framebuffer is follow-up work.

## Scene document guard — 2026-10-31

The editor tracks authored changes against an exact, bounded snapshot from the
existing scene codec. It polls in Edit mode every 250 ms and checks immediately
before Save, Play and destructive requests. The menu shows the document name and
an asterisk for unsaved changes. Active name/script drafts are committed before
these checks without truncating untouched long source strings.

File > New Scene, Open Scene and Exit share Save / Discard / Cancel decisions.
New Scene retains engine-owned bootstrap entities. Main-window SDL close and
SDL_QUIT, both consoles' quit aliases, and legacy loads from the console and pause
menu use the same guard. Secondary-window close events do not quit the editor.
Pending decisions keep the editor visible and prevent Play and authoring input.
Save As owns its path separately from a queued load target. Failed saves and loads
preserve the current document; cancelling Save As also cancels the pending action.
Legacy .scene saves omit engine entities and do not mark the .gameproj document clean.

Play freezes the authored dirty state; Stop restores authored contents before a
close/load decision. A rejected Stop prevents the destructive action. Runtime-only
changes do not become authored edits. Unsupported codec data (including deformed
meshes without a reconstruction recipe) is conservatively dirty and cannot be
made clean by a failed save. This adds no persistence support for such geometry.
The comparison retains at most 32 MiB of entity text; it is CPU serialization,
not a measured authoring-performance improvement. Autosave/recovery and an explicit
project/content-root workflow remain separate work.
Saving also rejects complete project files above the loader's 32 MiB limit,
including metadata and the JSON wrapper, before replacing an existing file.

Coverage includes a pure document-state test, actual codec snapshot and inspector
draft regressions, and a native coordinator fixture within Startup.PlayStop. The
native fixture covers real save failures, Save As target retention, failed and
successful opens, New Scene, clean/dirty Play–Stop, and synthetic SDL window/quit
events through Application input. It does not simulate modal button clicks.

A separate native Debug UI run verified File > Exit > Cancel, Exit > Save > Save As
> Cancel (both preserve the edited name), toolbar Save to a chosen .gameproj,
JSON verification of the edited name, removal of the dirty marker, and clean
File > Exit. Keyboard-only Ctrl+S/Alt+F4 injection was inconclusive and is not
claimed as acceptance. Broader keyboard, DPI and tool coverage remains open.

Final Windows validation on this change: all-target Debug and Release builds
passed; CTest passed 53/53 in Debug (33.18 s) and Release (32.58 s), including the
native coordinator/SDL event fixture. Evidence is retained under ignored
build/m1a-final-{debug,release}-{build,tests}.log and CTest JUnit outputs.
