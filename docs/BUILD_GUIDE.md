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
