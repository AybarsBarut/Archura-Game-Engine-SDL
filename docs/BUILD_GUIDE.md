# Windows x64 build and test guide

Validated environment on 2026-10-18: Visual Studio 2022 Community, MSVC
19.44, Windows SDK 10.0.26100.0, CMake 4.2.3, C++17. Presets require
CMake 3.21 or later; manual configuration retains the CMake 3.15 minimum.

From the repository directory in PowerShell:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-debug
ctest --preset windows-debug
cmake --build --preset windows-release
ctest --preset windows-release
```

Visual Studio's generator discovers the installed x64 toolchain without a
developer shell. The preset enables every contract-test option. Outputs are
under `build/windows-x64/bin/Debug` and `bin/Release`. Do not reuse a build
directory configured with another generator. Existing NMake batch scripts
continue to work and now establish their own repository working directory.

## Dependencies and deployment

The current bundled Windows layout requires SDL2 headers/x64 import libraries
and DLL, SDL2_ttf 2.22.0 headers/x64 library and DLL, Mono headers/import library,
Mono runtime DLL and class libraries, GLAD, ImGui, glm, stb, ufbx and fast_obj.
Configuration reports the precise missing bundled dependency file. OpenGL is
provided by Windows and the display driver. These declarations do not establish
a working Linux/macOS build.

SDL_net is optional. With this checkout it is absent: transport protocol tests
work, while runtime connections/server startup are unavailable. Request a hard
configuration failure with `-DARCHURA_REQUIRE_SDL_NET=ON`. Install the x64 SDK
under `external/SDL2_net` or provide `SDL2_NET_INCLUDE_DIR` and
`SDL2_NET_LIBRARY`; deployment of its DLL must also be verified. OpenAL is
off by default; `-DARCHURA_OPENAL=ON` requires a discoverable OpenAL SDK/runtime.
Neither optional enabled backend was certified in this cycle.

The engine post-build stages SDL2/SDL2_ttf/Mono DLLs, Mono class libraries,
`assets` and `Resources`. CMake 3.26+ uses `copy_directory_if_different`;
older CMake uses directory copying. The directory tree is still traversed;
this cycle makes no deployment-performance claim. Managed assembly compilation
is separate from the native build. Inspect `ScriptCore` and the assemblies in
`Resources` before relying on newly edited C# code.

Run the engine from its output directory so existing relative asset/config/save
paths resolve correctly. Arbitrary-CWD asset resolution and a standalone game
packager remain open work; copying the EXE alone is insufficient.

## Startup checks

Startup tests are opt-in because they create an actual SDL/OpenGL window:

```powershell
cmake --preset windows-x64 -DARCHURA_BUILD_STARTUP_TESTS=ON
cmake --build --preset windows-debug
ctest --preset windows-debug -L startup
```

`ArchuraEngine --smoke-test` skips the artificial splash delay, initializes the
normal engine/editor, renders exactly 16 frames, and shuts down. Initialization,
runtime exceptions and premature interruption return failure. Unsupported Vulkan
with fallback disabled must fail visibly. This is startup coverage, not visual,
audio, script-callback or editor Play/Stop acceptance.

## Commands used for this cycle

The existing ignored build directory was reused:

```powershell
cmake -S . -B build-codex-graphics-api -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON -DARCHURA_BUILD_ECS_TESTS=ON -DARCHURA_BUILD_PHYSICS_TESTS=ON -DARCHURA_BUILD_RENDERER_TESTS=ON -DARCHURA_BUILD_STARTUP_TESTS=ON
cmake --build build-codex-graphics-api --config Debug --parallel 4
ctest --test-dir build-codex-graphics-api -C Debug --output-on-failure
cmake --build build-codex-graphics-api --config Release --parallel 4
ctest --test-dir build-codex-graphics-api -C Release --output-on-failure
```

Build and CTest results are recorded in `.gauntlet/METRICS.md`. Logs reside in
the ignored build directory. `BUILD_TESTING=OFF` now disables all contract test
targets/registrations, independently of the focused test options. Performance
benchmarks remain separate opt-in targets.

With `ARCHURA_BUILD_STARTUP_TESTS=ON`, CTest includes
`Archura.Startup.PlayStop`. For a direct check, run from the executable directory:

```powershell
.\ArchuraEngine.exe --play-stop-smoke-test --graphics=opengl --no-graphics-fallback
```

This skips the splash, renders 16 frames and verifies two actual Editor/Application
Play/Stop cycles and shutdown. It opens an OpenGL window and needs a graphics
session. Normal `--smoke-test` still checks startup/shutdown without entering Play.

## Standalone game packages (Loop 7)

Build the engine, then create a new output directory from a game manifest:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-release
python tools/package_game.py --manifest samples/standalone/game.json --runtime-dir build/windows-x64/bin/Release --output ../ArchuraStandaloneSample
& ../ArchuraStandaloneSample/ArchuraGame.exe
```

Python 3.9+ is required to build packages and run packaging tests, not to play a
packaged game. The output must be new and outside the project-content and staged
runtime directories. Existing outputs are refused. Copy failures remove only the
private staging directory; publishing renames it into place after all copies.

The version-1 `game.json` declares `title`, `scene`, `player_id` and `content`.
Paths use forward slashes relative to the manifest. Include the scene and every
referenced model, texture, skybox face and audio clip in `content`; absolute paths,
parent traversal, duplicate/case-colliding destinations and undeclared scene assets
are rejected. Content belongs under `scenes/`, `assets/` or `Resources/Scripts/`;
engine `assets/shaders/` are reserved and copied from the runtime build.
The scene uses the existing version-2 `.scene` schema and must contain exactly one
entity named `Player`, matching `player_id`. That entity's world position initializes
the camera spawn; its local rotation.x/y initialize camera pitch/yaw in degrees. Scene load errors fail startup rather than
opening a default demo. The supplied four-entity example includes an imported OBJ.

`ArchuraGame.exe` automatically finds `game.json` beside itself even when launched
from another working directory. The original binary also accepts
`ArchuraEngine.exe --game <manifest>`. Standalone startup constructs no Editor,
disables developer mode/console bindings, and retains the 128 Hz tick, runtime
systems, HUD and a small Escape pause/resume/quit menu. `--smoke-test` verifies
16 rendered frames and orderly shutdown. Editor and game share the native binary;
this is not yet a smaller editor-free linkage target or an editor export button.

Packages contain staged DLLs, engine shaders, Mono's 4.5 runtime profile and
ScriptCore. A project may declare its own compiled
`Resources/Scripts/net472/ScriptCore.dll` in content; separate user assemblies and
arbitrary managed dependency graphs are not discovered automatically. Declared
missing script classes fail startup. The sample has no configured managed script;
its smoke test verifies Mono/ScriptCore initialization, not custom C# behavior.

Target machines need Windows x64, OpenGL 3.3 and the Microsoft Visual C++
2015-2022 x64 runtime. Use Release for distribution; Debug relies on developer
runtime libraries. No clean-Windows VM or installer prerequisite deployment has
been certified. Optional SDL_net/OpenAL DLLs must already be staged beside the
engine when those backends are enabled; Loop7 used neither backend; Loop8 enables the bundled SDL_net backend.

To enable all native package/startup checks on a fresh preset:

```powershell
cmake --preset windows-x64 -DARCHURA_BUILD_STARTUP_TESTS=ON
cmake --build --preset windows-release
ctest --preset windows-release
```

## Loop 8 — Real SDL_net and authoritative movement

The Windows x64 subset of the official SDL2_net2.2.0 VC package is bundled under
external/SDL2_net; source URL/archive SHA256 and unmodified-file provenance are
recorded in PROVENANCE.txt. This is a pinned SDL2-compatible dependency, not a
claim about the latest upstream release. Engine, Server and NetClient stage
SDL2_net.dll and its license; standalone packaging also copies staged licenses.

```powershell
cmake --preset windows-x64 -DARCHURA_REQUIRE_SDL_NET=ON -DARCHURA_BUILD_STARTUP_TESTS=ON
cmake --build --preset windows-release
ctest --preset windows-release
```

Run each command below in its own terminal from the repository root:

```powershell
.\build\windows-x64\bin\Release\ArchuraServer.exe --port 27015
.\build\windows-x64\bin\Release\ArchuraNetClient.exe --port 27015 --duration-ms 10000 --move-x 1
.\build\windows-x64\bin\Release\ArchuraNetClient.exe --port 27015 --duration-ms 10000 --move-z -1
```

NetClient is a headless native tool, not a graphical game mode. It consumes full
snapshots into generation-checked transient ECS entities and prints server tick,
assigned connection identity and player positions. The server owns a bounded XZ
movement arena:128Hz simulation,6 units/second speed, diagonal normalization,
[-256,256] bounds and input expiry after32 ticks. Every connection spawns one
player; disconnected clients disappear from subsequent full snapshots. Clients
send sequence plus movement axes, never player identity, position, speed or dt.
The default snapshot rate is64Hz. Dedicated runtime requires128Hz; unsupported
password/RCON options fail explicitly rather than implying authentication.

Wire protocol is now version2: acknowledgement carries token plus server-assigned
peer identity, and PlayerInput is a new message. Version1 peers fail negotiation.
Duplicate/out-of-order input, invalid axes/NaNs, client position/shoot packets and
forged world state are rejected by the authoritative dedicated server. The legacy
transport relay APIs remain for existing engine-host callers; using StartHost in
the graphical application does not enable this authoritative simulation yet.

This is a localhost transport/movement slice. Map loading/collision, combat,
graphical Application integration, authentication, prediction/reconciliation,
interpolation and a separate unreliable state channel remain open. SDL_net TCP
connect/send are synchronous; bounded queues do not certify nonblocking service
against slow receivers or Internet-scale performance.

## Loop9 — Graphical authoritative arena

The normal OpenGL Application can join the dedicated server:

```powershell
.\build\windows-x64\bin\Release\ArchuraServer.exe --port 27015
.\build\windows-x64\bin\Release\ArchuraEngine.exe --connect 127.0.0.1 --port 27015
```

Run the commands in separate terminals; launch another engine process to see the
other player. In the editor, `game_mode join 127.0.0.1 27015` also enters Play and
joins. Existing Pause menu Join works while playing. WASD follows camera direction,
mouse looks, Escape pauses your input, Tab toggles editor UI. Stop disconnects and
restores the authored scene. The arena overlay has a Disconnect button. Legacy
in-process Host still only provides transport; use the dedicated server for this
arena. Connections without an authoritative snapshot are dropped after5 seconds.

`GraphicalNetworkSession` owns a separate Scene, Camera and RenderSystem. Server
positions drive the first-person eye and cube avatars for remote players; the own
avatar is hidden. Local scripts, physics, weapons and particles are suspended for
the entire connection, and zero movement is sent while paused/Edit/console. Mouse
look and scroll are routed to the network camera. Disconnect clears callbacks and
releases network GPU owners while the context is current. Connection loss pauses
local play; failed Join attempts return to Edit when that attempt entered Play.
Reconnect clears replica tick history and input sequence, including server restarts
which reuse connection IDs. No network entities are inserted into the authored scene.

The built-in `dm_arena` uses shared `NetworkArena.h` obstacle/boundary definitions
for rendering and server movement. The server sweeps an axis-aligned0.7-unit-wide
player footprint in XZ, slides along obstacles, and keeps it inside256-unit arena
bounds at128Hz. Unknown map names fail rather than claiming to load an absent map.
This is static flat-arena collision: arbitrary authored levels, vertical movement,
player-vs-player collision and combat remain separate work. Wire version2 and the
full snapshot layout are unchanged; use client and server from the same build.
Network camera/avatars now use bounded render interpolation (Loop10 below). Client prediction remains unimplemented.

`Archura.Network.GraphicalIntegration` is opt-in with startup tests. It opens two
actual OpenGL applications against a real dedicated server and checks rendering,
authoritative camera/remote transforms, pause, Stop/Edit, rejoin and live server-loss
cleanup. It also verifies a gravity-bearing authored probe and local camera remain
unchanged. `--network-smoke-test` is the diagnostic entry used by this test;
`--network-move-x`/`--network-move-z` supply test-only motion with that flag.

## Loop10 — Network render interpolation

The same join commands now display camera feet and remote avatars from a separate
bounded presentation buffer. The configured delay is4 server ticks (31.25ms at128Hz);
up to32 snapshots are retained. Mouse look remains immediate. This adds movement
presentation latency; it does not predict input. The authoritative world, wire-v2
payload and singleplayer rendering are unchanged.

When snapshots stop, the view holds the last known position. A gap over32 server
ticks or an implausible displacement snaps instead of interpolating a discontinuity.
Pause input still sends zero; the delayed view settles after buffered movement.
Disconnect/Stop/reconnect discard both snapshot history and presentation state.
No adaptive jitter buffer, extrapolation or reconciliation is implemented.

Run the pure presentation contract or the existing native graphical fixture:

```powershell
ctest --test-dir build/windows-x64 -C Debug -R SnapshotInterpolation --output-on-failure
ctest --test-dir build/windows-x64 -C Debug -R GraphicalIntegration --output-on-failure
```

The diagnostic GFXSNAP line separates latest PLAYER authority from PLAYER positions
after the VIEW marker; CAM must match the local VIEW player. Native validation also
checks actual remote transforms/assets. These are correctness tests, not FPS benchmarks.

RAMP-01 fixes repeated walking onto the editor's Ramp collider. Focused verification:

```powershell
cmake --build build/windows-x64 --config Debug --target ArchuraPhysicsCoreTests --parallel 4
ctest --test-dir build/windows-x64 -C Debug -R Physics.Core --output-on-failure
```

For manual verification, add a Ramp on a flat floor, enter Play and walk onto its
low end at slow and normal speed. Repeat after yaw rotation, walk down, and check
that its side/back still block. Use the newly built engine under
`build/windows-x64/bin/Debug` or `Release`. No scene migration is required.
