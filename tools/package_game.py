"""Build a new, relocatable Windows game package from a staged engine build."""
import argparse
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import tempfile


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(path, limit):
    if not path.is_file() or path.stat().st_size > limit:
        raise ValueError(f"Missing or oversized JSON: {path}")
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def package_path(value):
    if not isinstance(value, str) or not value or len(value.encode("utf-8")) > 240:
        raise ValueError("Content path must be a nonempty relative path")
    if any(ord(c) < 32 or ord(c) == 127 for c in value) or any(c in value for c in '\\:"<>|?*'):
        raise ValueError(f"Invalid content path: {value!r}")
    if value.startswith("/") or any(p in ("", ".", "..") for p in value.split("/")):
        raise ValueError(f"Content path escapes or aliases the package: {value}")
    devices = {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$"}
    devices.update(f"{prefix}{i}" for prefix in ("COM", "LPT") for i in range(1, 10))
    for part in value.split("/"):
        if part.endswith((".", " ")) or part.split(".")[0].upper() in devices:
            raise ValueError(f"Windows filename alias/device is not allowed: {value}")
    return PurePosixPath(value)


def contained_file(root, value):
    path = root.joinpath(*package_path(value).parts).resolve(strict=True)
    if not path.is_relative_to(root) or not path.is_file():
        raise ValueError(f"Content is outside package root or is not a file: {value}")
    return path


def load_manifest(path):
    manifest = read_json(path, 65536)
    fields = {"version", "title", "scene", "player_id", "content"}
    if not isinstance(manifest, dict) or set(manifest) != fields:
        raise ValueError("Manifest requires version, title, scene, player_id and content")
    if type(manifest["version"]) is not int or manifest["version"] != 1:
        raise ValueError("Unsupported manifest version")
    title = manifest["title"]
    if (not isinstance(title, str) or not title or len(title.encode("utf-8")) > 128 or
            any(ord(c) < 32 or ord(c) == 127 for c in title)):
        raise ValueError("Invalid game title")
    player = manifest["player_id"]
    if (not isinstance(player, str) or len(player) != 32 or
            any(c not in "0123456789abcdef" for c in player) or set(player) == {"0"}):
        raise ValueError("Invalid player persistent identity")
    package_path(manifest["scene"])
    files = manifest["content"]
    if not isinstance(files, list) or not 1 <= len(files) <= 1024:
        raise ValueError("Invalid content list")
    seen = set()
    for name in files:
        value = package_path(name)
        if (value.parts[0] not in ("assets", "scenes", "Resources") or
                name.startswith("assets/shaders/") or
                (value.parts[0] == "Resources" and not name.startswith("Resources/Scripts/"))):
            raise ValueError(f"Reserved package destination: {name}")
        if name.lower() in seen:
            raise ValueError(f"Duplicate content destination: {name}")
        seen.add(name.lower())
    if manifest["scene"] not in files:
        raise ValueError("Startup scene must be in content")
    root = path.resolve(strict=True).parent
    for name in files:
        contained_file(root, name)
    return manifest, root


def validate_scene_assets(scene, declared):
    # Engine loading validates the entire component schema. Packaging additionally
    # refuses external/undeclared assets so the output cannot depend on the repo.
    def require(path, audio=False):
        if not path:
            return
        if audio:
            while path.startswith("assets/audio/"):
                path = path[len("assets/audio/"):]
            path = "assets/audio/" + path
        package_path(path)
        if path not in declared:
            raise ValueError(f"Scene asset is not declared in content: {path}")

    def visit(value, context=""):
        if isinstance(value, dict):
            for key, item in value.items():
                if key in ("model_path", "texture", "texture_path"):
                    require(item)
                elif key == "clip" and context in ("audio", "audio_source"):
                    require(item, audio=context == "audio")
                elif key == "faces":
                    for face in item:
                        require(face)
                else:
                    visit(item, key)
        elif isinstance(value, list):
            for item in value:
                visit(item, context)
    visit(scene)


def build_package(manifest_path, runtime_dir, output):
    manifest, root = load_manifest(manifest_path)
    runtime = runtime_dir.resolve(strict=True)
    destination = output.resolve()
    if destination.exists():
        raise ValueError("Output already exists; choose a new package directory")
    if destination.is_relative_to(root) or destination.is_relative_to(runtime):
        raise ValueError("Output must be outside the project and staged runtime directories")
    scene = read_json(contained_file(root, manifest["scene"]), 32 * 1024 * 1024)
    validate_scene_assets(scene, set(manifest["content"]))
    required = ["ArchuraEngine.exe", "SDL2.dll", "SDL2_ttf.dll", "mono-2.0-sgen.dll",
                "mono/lib/mono/4.5/mscorlib.dll", "Resources/Scripts/net472/ScriptCore.dll"]
    for name in required:
        if not (runtime / name).is_file():
            raise ValueError(f"Required staged runtime dependency missing: {name}")
    if not (runtime / "assets/shaders").is_dir():
        raise ValueError("Required engine shaders missing")
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Only a private staging directory is removed on failure. Existing packages
    # and project sources are never deleted or modified.
    stage = Path(tempfile.mkdtemp(prefix=".archura-package-", dir=destination.parent))
    try:
        shutil.copy2(runtime / "ArchuraEngine.exe", stage / "ArchuraGame.exe")
        for dll in runtime.glob("*.dll"):
            shutil.copy2(dll, stage / dll.name)
        if (runtime / "licenses").is_dir():
            shutil.copytree(runtime / "licenses", stage / "licenses")
        shutil.copytree(runtime / "mono/lib/mono/4.5", stage / "mono/lib/mono/4.5")
        if (runtime / "mono/etc").is_dir():
            shutil.copytree(runtime / "mono/etc", stage / "mono/etc")
        shutil.copytree(runtime / "assets/shaders", stage / "assets/shaders")
        core = stage / "Resources/Scripts/net472/ScriptCore.dll"
        core.parent.mkdir(parents=True)
        shutil.copy2(runtime / "Resources/Scripts/net472/ScriptCore.dll", core)
        for name in manifest["content"]:
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(contained_file(root, name), target)
        (stage / "game.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        (stage / "RUN.txt").write_text(
            "Launch ArchuraGame.exe. WASD/mouse: move/look; Escape: pause.\n"
            "Windows x64 with OpenGL 3.3 and Microsoft Visual C++ 2015-2022 x64 runtime required.\n"
            "This package contains a shared engine binary; authoring UI is disabled at startup.\n",
            encoding="utf-8")
        # Windows rename fails if another publisher created the output meanwhile.
        if destination.exists():
            raise ValueError("Output was created during packaging")
        os.rename(stage, destination)
    finally:
        if stage.exists():
            if stage.parent.resolve() != destination.parent or not stage.name.startswith(".archura-package-"):
                raise ValueError("Invalid staging cleanup path")
            shutil.rmtree(stage)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--runtime-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = build_package(args.manifest, args.runtime_dir, args.output)
    except (ValueError, OSError, TypeError) as error:
        parser.exit(1, f"Package failed: {error}\n")
    print(f"Game package created: {result}")


if __name__ == "__main__":
    main()
