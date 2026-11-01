# Archura Game Engine SDL

A C++17 game engine and scene editor using SDL2, OpenGL, ImGui and ECS-style
entities, with a separate dedicated server and optional Mono scripting runtime.

## Build and run on Windows

Use Visual Studio 2022 C++ tools, CMake 3.21+ and Python 3.9+. SDK inputs are
bundled under `external/`; the full runtime also needs the locally provisioned
Mono DLL described in the [build guide](docs/BUILD_GUIDE.md).

```powershell
.\Build.bat -Configuration Release
.\StartGame_Dev.bat
.\StartGame_Release.bat
```

The root scripts delegate to [one Windows entrypoint](tools/windows/engine.ps1).
They use current CMake presets and matching runtime directories; archived EXEs
are never copied into a live build. `-WhatIf` previews the commands, and `-SkipBuild`
is an explicit opt-out for run/package actions. `build_all.bat` builds both
configurations. Native contract tests are run separately through validation.

## Validate locally

```powershell
python tools/validate_local.py
python tools/validate_local.py --profile contracts
python tools/validate_local.py --profile loop
```

The desktop profile builds all targets and runs the full Debug/Release graph.
Contracts checks tracked SDK inputs without the graphical runtime; loop uses the
SDK-free graph. Logs, JUnit and a source-fingerprinted JSON report are written to
`build/local-validation/`. These are local Windows checks, not hosted/Linux results.

## Package a game

```powershell
.\Publish_Release.bat -Manifest .\samples\standalone\game.json -Output .\build\packages\MyGame
```

Packaging uses the [validated manifest packager](tools/package_game.py). The output
must be a new directory. See the [standalone sample](samples/standalone/game.json)
and [build guide](docs/BUILD_GUIDE.md) for the manifest and runtime prerequisites.

## Repository layout

| Directory | Responsibility |
| --- | --- |
| `src/core` | Application composition, lifecycle, services and persistence |
| `src/ecs` | Entity identity, components, hierarchy and scene ownership |
| `src/editor` | Editor UI, selection, authoring commands and document/project sessions |
| `src/game`, `src/network` | Gameplay systems and networking |
| `src/rendering`, `src/audio`, `src/input`, `src/scripting` | Runtime platform systems |
| `src/server`, `src/launcher` | Separate application entrypoints |
| `tests`, `tests/support/editor` | Contracts, process fixtures and opt-in editor startup checks |
| `cmake` | Dependency inventory and native validation target registration |
| `tools`, `tools/windows` | Validation, packaging and build/run entrypoints |
| `assets`, `Resources`, `ScriptCore` | Runtime content and managed scripting inputs |
| `benchmarks`, `examples`, `samples` | Opt-in measurements, API examples and sample game data |
| `docs` | Architecture, current guides and explicitly historical material |
| `external` | Third-party dependencies and required runtime SDK inputs |

[Architecture and maintenance boundaries](docs/ARCHITECTURE.md) ·
[Editor readiness](docs/EDITOR_READINESS.md) · [Test plan](docs/TEST_PLAN.md) ·
[Server configuration](docs/BUILD_GUIDE.md#dedicated-server-configuration) ·
[Release history](docs/releases/1.0.20.md)
