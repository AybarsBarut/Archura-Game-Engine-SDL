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
