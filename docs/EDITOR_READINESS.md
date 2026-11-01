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

- Asset import/reimport, searchable previews, dependency diagnostics and missing-asset repair.
- Complete command history for creation, deletion, parenting and component changes.
  Existing transform Undo/Redo is not general scene command history.
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

## Project sessions — 2026-10-31

File > New Project / Open Project / Recent Projects now operate on separate
version-1 `.archproj` descriptors. Creation is exclusive, supplies default folders
and an asset-free Player/Floor/Sun starter, and never overwrites an existing folder.
Descriptor content and scene paths are canonical, relative and contained in the
project. The active session owns project identity and the explicit content root;
scene Save As remains a scene copy. Invalid metadata, missing assets and failed
loads retain the previous scene, document path, dirty state and project root.
Project switches invalidate asset caches and hide stale asynchronous browser data.

Relative assets in explicit projects no longer fall back to another project's
root, parent folders or the process CWD. Asset lists, audio and skybox loading use
the same context. Absolute legacy authored asset paths remain supported; general
asset confinement and importer/decode expansion budgets remain later work.
Legacy scene documents can still be opened without project metadata. Recents are
bounded and atomically persisted; corrupt preferences are reported and retained.

`--project` supports native Unicode project/scene paths and resolves relative
arguments against the caller's original CWD, then uses the executable folder for
engine bootstrap. Actual native fixtures cover creation, guarded cancellation,
starter Play/Stop, project switching, relative OBJ import with conflicting CWD
files, missing-asset rollback, recents restart, Unicode scene Save As and legacy
detachment. A separate process fixture covers Unicode project restart, relative
CLI paths, preference corruption and invalid/conflicting launch options. Unicode
project/browser paths are covered; full Unicode asset importer support is not.

Windows all-target Debug and Release builds passed. CTest passed **55/55** in
Debug (37.66 s) and Release (35.99 s). A separate native Debug UI run, with isolated
recent-project preferences, verified New Project creation, three starter entities,
project name/content-root display and directory listing, orderly exit, restart,
and reopening that same project through Recent Projects. Metadata and recent-list
JSON were inspected. Keyboard-only and full Unicode asset-import acceptance are
not claimed. Evidence: ignored build/m1b-final-*-{build,tests}.log, JUnit outputs,
build/m1b-ui.log, build/m1b-ui-restart.log and isolated test project/preferences.

## Transform history — M1c first slice, 2026-10-31

Transform history now supports Redo, bounded value snapshots and Scene lifetime,
entity generation and persistent-ID checks. Inspector/gizmo interactions commit
lazily: an activated field with no actual delta neither consumes Undo nor clears
Redo. A continuous drag is one operation. New captured transform edits invalidate
the Redo branch. Both stacks and pending captures rebind after Play/Stop; new scenes
and scene/project loads clear them. History stores no entity or GPU ownership.

Edit menu availability follows valid history, with Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y
routing. Undo/Redo ends an active gizmo/numeric interaction before applying values.
Play and pending document decisions block capture and history operations. Mesh
keyboard Undo is kept separate so one shortcut does not operate on two histories.
This completes the transform portion only; entity/component command history remains.

Pure regressions cover complete transform round trips, multiple entities, branches,
no-op activation queries, 50 real edits, deletion/component removal, wrong/reused
Scene lifetimes and transactional generation replacement. The native coordinator
fixture extends Startup.PlayStop with clean/dirty baseline restoration, live
selection, Redo through actual Play/Stop, runtime capture rejection, pending document
guards, deleted targets and history clearing at scene boundaries.

Final Windows all-target Debug/Release builds passed; CTest passed **55/55** in
Debug (42.09 s) and Release (42.66 s). Native Debug UI verified Floor scale reset
(20,1,20) -> (1,1,1), Edit-menu Undo -> original scale/clean marker, Redo -> reset
scale/dirty marker, Undo -> original again, and unchanged Rotation reset preserving
Redo availability. The test exited cleanly without changing its saved scene.
Keyboard injection and all numeric/gizmo combinations are not claimed as UI
acceptance. Logs/JUnit: ignored build/m1c-transform-*.

## Entity create/delete history — M1c second slice, 2026-11-01

Creation (including paste/duplicate and Game Builder) and deletion now share the
transform Undo/Redo journal. Commands preserve persistent IDs, entity order,
parent/child and sibling links, serialized component values and selection. Only
affected entities are reconstructed; unrelated runtime handles remain stable.
Play and pending document decisions block authored commands, and Play/Stop keeps
the shared chronology. New/load boundaries clear it.

The history has 50 completed entries / 32 MiB of accounted CPU payload, a 2048
entity command limit, 4 MiB serialized batches and an 8 MiB command limit.
Unsupported/oversized data, stale context and missing assets reject or block
commands while preserving the scene/history. Restored assets allow retry. No
entity/GPU resources are retained by completed commands. A creator callback is
internal, synchronous and create-only; a failure removes all partial new entities.

Regressions cover transactional identity/hierarchy/order commits with allocation
fault injection, batch codec validation and retained texture metadata, mixed
transform/entity chronology, fresh generations, component values, selection
recipes, external changes, missing asset retry and failed creation rollback.
The native Startup.PlayStop coordinator exercises actual Spawn, Copy/Paste,
multi-delete, dirty baseline and selection restoration, Play/pending guards,
and Redo after actual Stop. These are native coordinator tests; this slice has
no separate mouse/keyboard UI acceptance claim. Parenting and standalone
component/property commands and M1d autosave/recovery remain unimplemented.

Final Windows all-target builds passed for Debug and Release. Full CTest passed
**56/56** in Debug (40.49 s) and Release (35.92 s). Evidence is retained under
ignored `build/m1c-entity-*` build/test logs and JUnit files. Hosted CI and Linux
execution were outside this local Windows acceptance.

## Hierarchy and component commands — M1c third slice, 2026-11-01

Hierarchy drag/drop and context Unparent now publish one chronological command,
after drawing the tree. Group unparent restores all original parents and sibling
positions atomically through the existing prepared ECS graph commit. Local TRS and
entity handles remain unchanged. Only affected sibling groups are captured;
unrelated hierarchy edits remain intact. Cycles/depth/stale identities reject
without consuming history. Captured groups detect external sibling changes.

Collider add/remove/fit, Physics Motion/Apply Ball Physics, and Script add/remove
now use restricted CPU-value commands. They preserve the entity generation and
unrelated components. Complete Script class names are retained, runtime instance
IDs are excluded. Both command types bind Scene lifetime/persistent IDs, work after
delete reconstruction and Play/Stop, preserve selection and dirty baselines,
and reject authoring while Play or a document decision owns input. Existing
Play-time Inspector component changes still affect only runtime values.

A new StructuralHistory contract covers multiple parent groups, exact siblings,
nested group unparent, cycles, external changes, foreign/reused Scene addresses,
iteration rejection, mixed transform history, component values/no-ops/bounds and
allocation-failure rollback. Native Startup.PlayStop extends the coordinator with
mixed create/reparent/add/remove/delete history through Stop and restored entity
generations, long Script names, selection and clean baseline restoration. Mouse
and keyboard UI acceptance is not claimed for this slice.

This covers the exposed structural operations for Collider, RigidBody and Script.
Continuous property edits, Script class typing, material/texture assignment,
other component types, mesh histories and M1d autosave/recovery remain later work.

Final all-target Windows builds passed in Debug and Release; full CTest passed
**57/57** in Debug (38.46 s) and Release (36.59 s). Logs/JUnit are under ignored
`build/m1c-structural-*`. Hosted CI and Linux execution were outside this local
Windows gate.
