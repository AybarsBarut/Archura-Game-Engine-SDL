"""Launch only copied package files, from a different working directory."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--runtime-dir", required=True, type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("package_game", source / "tools/package_game.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)

with tempfile.TemporaryDirectory(prefix="archura-standalone-smoke-") as temporary:
    base = Path(temporary).resolve()
    assert base.parent == Path(tempfile.gettempdir()).resolve()
    assert base.name.startswith("archura-standalone-smoke-")
    project = base / "project with spaces"
    shutil.copytree(source / "samples/standalone", project)
    output = packager.build_package(project / "game.json", args.runtime_dir, base / "game with spaces")
    project.rename(base / "source-unavailable")
    cwd = base / "unrelated working directory"
    cwd.mkdir()

    def launch(*arguments):
        return subprocess.run([str(output / "ArchuraGame.exe"), *arguments], cwd=cwd,
                              capture_output=True, text=True, errors="replace", timeout=25)

    result = launch("--smoke-test", "--graphics=opengl", "--no-graphics-fallback")
    print(result.stdout, result.stderr)
    assert result.returncode == 0, "Packaged game failed"
    assert "entities=4; editor=off" in result.stdout, "Wrong scene or editor startup"
    assert "ScriptEngine Initialized!" in result.stdout, "Packaged Mono/ScriptCore failed"
    assert "16 rendered frames and shutdown" in result.stdout, "Incomplete runtime/shutdown"
    assert not (cwd / "assets").exists(), "Launch unexpectedly depended on caller assets"

    # Missing imported content fails before graphics startup rather than falling
    # back to the source tree. Wrong player/scene fail without demo fallback.
    marker = output / "assets/models/marker.obj"
    marker.rename(marker.with_suffix(".hidden"))
    assert launch("--smoke-test").returncode != 0, "Missing packaged asset was silently accepted"
    marker.with_suffix(".hidden").rename(marker)
    manifest = json.loads((output / "game.json").read_text())
    manifest["player_id"] = "20000000000000000000000000000001"
    (output / "game.json").write_text(json.dumps(manifest))
    assert launch("--smoke-test").returncode != 0, "Missing player was silently accepted"
    manifest["player_id"] = "10000000000000000000000000000001"
    (output / "game.json").write_text(json.dumps(manifest))
    scene = json.loads((output / "scenes/start.scene").read_text())
    scene["entities"][0]["components"] = {"script": {"class_name": "Sample.MissingScript"}}
    (output / "scenes/start.scene").write_text(json.dumps(scene))
    missing_script = launch("--smoke-test")
    assert missing_script.returncode != 0 and "Packaged script class missing" in missing_script.stderr
    (output / "scenes/start.scene").write_text('{"entities": [')
    assert launch("--smoke-test").returncode != 0, "Malformed scene fell back to demo"
    (output / "game.json").unlink()
    assert launch("--smoke-test").returncode != 0, "Missing manifest opened editor"
print("Standalone package relocation and failure checks passed")
