"""Restart an editor project from unrelated CWD, with isolated preferences.

Runs the native OpenGL startup path; this is not a UI screenshot/click test.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


parser = argparse.ArgumentParser()
parser.add_argument("--engine", required=True, type=Path)
args = parser.parse_args()
engine = args.engine.resolve(strict=True)

with tempfile.TemporaryDirectory(prefix="archura-editor-project-startup-") as temporary:
    base = Path(temporary).resolve()
    assert base.parent == Path(tempfile.gettempdir()).resolve()
    assert base.name.startswith("archura-editor-project-startup-")
    project = base / "Proje-\u0130\u015f-\u6e38\u620f"
    scenes = project / "scenes"
    scenes.mkdir(parents=True)
    (project / "assets").mkdir()
    working = base / "unrelated working directory"
    working.mkdir()
    profile = base / "isolated preferences"
    profile.mkdir()
    environment = os.environ.copy()
    for variable in ("LOCALAPPDATA", "APPDATA", "XDG_CONFIG_HOME", "HOME", "USERPROFILE"):
        environment[variable] = str(profile)

    project_name = "CLI Proje \u0130\u015f \u6e38\u620f"
    descriptor = project / "Oyun-\u015e-\u9879\u76ee.archproj"
    metadata = {
        "project_version": 1,
        "name": project_name,
        "content_root": ".",
        "start_scene": "scenes/Main-\u573a\u666f.gameproj",
    }
    scene_file = project / metadata["start_scene"]
    descriptor.write_text(json.dumps(metadata, ensure_ascii=False), encoding="utf-8")
    starter = {
        "format_version": 1,
        "project": {"name": project_name, "version": "1", "start_scene": "MainScene"},
        "entities": [
            {"name": "Player", "has_mesh_renderer": False, "position": [0, 2, 4]},
            {"name": "Floor", "has_mesh_renderer": True, "mesh_type": "cube",
             "scale": [12, 1, 12], "position": [0, -1, 0], "collider": [1, 1, 1]},
        ],
    }
    scene_file.write_text(json.dumps(starter, ensure_ascii=False), encoding="utf-8")

    def launch(*arguments):
        result = subprocess.run(
            [str(engine), *arguments, "--graphics=opengl", "--no-graphics-fallback"],
            cwd=working, env=environment, capture_output=True, timeout=45,
        )
        return result.returncode, (result.stdout + result.stderr).decode("utf-8", errors="replace")

    def require_success(*arguments):
        code, output = launch("--smoke-test", *arguments)
        if code or "Startup smoke completed: 16 rendered frames and shutdown" not in output:
            raise AssertionError(f"Project restart failed: exit={code}\n{output}")
        if f"[Project] Opened {project_name} | content_root=" not in output:
            raise AssertionError(f"Startup did not establish the requested project session\n{output}")

    require_success("--project", str(descriptor))
    recent_file = profile / "Archura" / "Editor" / "recent_projects.json"
    recent = json.loads(recent_file.read_text(encoding="utf-8"))
    if len(recent["projects"]) != 1 or Path(recent["projects"][0]).resolve() != descriptor.resolve():
        raise AssertionError(f"Successful startup did not record exact native project identity: {recent}")

    # A relative argument must resolve against the caller's original directory,
    # before engine shaders/Mono switch to the executable bootstrap directory.
    relative = os.path.relpath(descriptor, working)
    require_success("--project", relative)
    if len(json.loads(recent_file.read_text(encoding="utf-8"))["projects"]) != 1:
        raise AssertionError("Restart duplicated the recent project identity")

    corrupt_preferences = b"malformed preferences must be preserved"
    recent_file.write_bytes(corrupt_preferences)
    require_success("--project=" + str(descriptor))
    if recent_file.read_bytes() != corrupt_preferences:
        raise AssertionError("Project startup overwrote malformed preferences")

    def require_failure(*arguments):
        code, output = launch(*arguments)
        if code == 0 or "Startup smoke completed" in output:
            raise AssertionError(f"Invalid project startup was accepted: {arguments}\n{output}")
        return output

    invalid_descriptor = dict(metadata, project_version=99)
    descriptor.write_text(json.dumps(invalid_descriptor, ensure_ascii=False), encoding="utf-8")
    output = require_failure("--smoke-test", "--project", str(descriptor))
    if "Project startup failed" not in output:
        raise AssertionError(f"Invalid project did not report a project startup failure\n{output}")
    descriptor.write_text(json.dumps(metadata, ensure_ascii=False), encoding="utf-8")
    starter["entities"][1].update(mesh_type="obj", model_path="assets/models/missing.obj")
    scene_file.write_text(json.dumps(starter, ensure_ascii=False), encoding="utf-8")
    output = require_failure("--smoke-test", "--project", str(descriptor))
    if "Project startup failed" not in output:
        raise AssertionError(f"Missing project asset did not reject startup\n{output}")

    for conflicting in (("--game", "missing-game.json"), ("--connect", "localhost"),
                        ("--play-stop-smoke-test",)):
        output = require_failure("--project", str(descriptor), *conflicting)
        if "LAUNCH ERROR" not in output:
            raise AssertionError(f"Conflicting project mode was not rejected before initialization\n{output}")
    for invalid in (("--project",), ("--project=",),
                    ("--project", str(descriptor), "--project", str(descriptor))):
        output = require_failure(*invalid)
        if "LAUNCH ERROR" not in output:
            raise AssertionError(f"Invalid project argument was not rejected before initialization\n{output}")
    if recent_file.read_bytes() != corrupt_preferences:
        raise AssertionError("Failed startup modified isolated malformed preferences")

print("Editor project restart, native Unicode paths, caller CWD, preferences and failure checks passed")
