import copy
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("package_game", Path(__file__).resolve().parents[1] / "tools/package_game.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class PackageContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="archura-package-contract-")
        self.base = Path(self.temp.name).resolve()
        self.project = self.base / "project"
        self.runtime = self.base / "runtime"
        self.manifest = {"version": 1, "title": "Sample", "scene": "scenes/start.scene",
                         "player_id": "10000000000000000000000000000001", "content": ["scenes/start.scene"]}
        self.write(self.project / "game.json", json.dumps(self.manifest))
        self.write(self.project / "scenes/start.scene", '{"entities":[]}')
        for name in ["ArchuraEngine.exe", "SDL2.dll", "SDL2_ttf.dll", "mono-2.0-sgen.dll",
                     "mono/lib/mono/4.5/mscorlib.dll", "Resources/Scripts/net472/ScriptCore.dll",
                     "assets/shaders/basic.vert"]:
            self.write(self.runtime / name, "test fixture")

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def tearDown(self):
        assert self.base.parent == Path(tempfile.gettempdir()).resolve()
        assert self.base.name.startswith("archura-package-contract-")
        self.temp.cleanup()

    def build(self):
        return packager.build_package(self.project / "game.json", self.runtime, self.base / "published")

    def test_publish_is_portable_and_does_not_overwrite(self):
        output = self.build()
        self.assertTrue((output / "ArchuraGame.exe").is_file())
        self.assertFalse((output / "ArchuraEngine.exe").exists())
        self.assertEqual(json.loads((output / "game.json").read_text()), self.manifest)
        with self.assertRaises(ValueError):
            self.build()
        self.assertTrue((output / "scenes/start.scene").is_file())

    def test_missing_runtime_or_content_cannot_publish(self):
        for missing in [self.runtime / "mono/lib/mono/4.5/mscorlib.dll", self.project / "scenes/start.scene"]:
            old = missing.read_text()
            missing.unlink()
            with self.assertRaises((ValueError, OSError)):
                self.build()
            self.assertFalse((self.base / "published").exists())
            self.write(missing, old)

    def test_copy_failure_cleans_only_private_staging(self):
        with mock.patch.object(packager.shutil, "copy2", side_effect=OSError("simulated disk failure")):
            with self.assertRaises(OSError):
                self.build()
        self.assertFalse((self.base / "published").exists())
        self.assertFalse(list(self.base.glob(".archura-package-*")))
        self.assertEqual(json.loads((self.project / "game.json").read_text()), self.manifest)

    def test_invalid_manifest_paths_and_collisions(self):
        for path in ["../outside.scene", "C:/outside.scene", "scenes/../start.scene",
                     "scenes//start.scene", "scenes/./start.scene", "scenes\\start.scene",
                     "scenes/start.scene\x00", "scenes/start.scene.", "scenes/NUL.scene", "scenes/file*.scene",
                     "assets/shaders/basic.vert", "ArchuraGame.exe"]:
            with self.subTest(path=path):
                bad = copy.deepcopy(self.manifest)
                bad["content"].append(path)
                self.write(self.project / "game.json", json.dumps(bad))
                with self.assertRaises((ValueError, OSError)):
                    self.build()
        bad = copy.deepcopy(self.manifest)
        bad["content"].append("Scenes/start.scene")
        self.write(self.project / "game.json", json.dumps(bad))
        with self.assertRaises(ValueError):
            self.build()

    def test_symlink_escape_rejects(self):
        self.write(self.base / "outside.scene", '{}')
        (self.project / "scenes/start.scene").unlink()
        try:
            (self.project / "scenes/start.scene").symlink_to(self.base / "outside.scene")
        except OSError:
            self.skipTest("Windows symlink creation unavailable")
        with self.assertRaises(ValueError):
            self.build()

    def test_runtime_destinations_reject_before_staging(self):
        names = [
            "assets/shaders", "assets/Shaders", "assets/shaders/basic.vert",
            "assets/Shaders/basic.vert", "assets/SHADERS/custom/new.frag",
            "Resources/Scripts/net472/ScriptCore.dll",
            "Resources/Scripts/NET472/scriptcore.DLL",
            "Resources/Scripts/net472/ScriptCore.dll/child",
        ]
        for index, name in enumerate(names):
            with self.subTest(path=name):
                project = self.base / f"collision-project-{index}"
                manifest = copy.deepcopy(self.manifest)
                manifest["content"].append(name)
                self.write(project / "game.json", json.dumps(manifest))
                self.write(project / "scenes/start.scene", '{"entities":[]}')
                self.write(project / name, "project overwrite")
                output = self.base / f"rejected-{index}"
                with mock.patch.object(packager.tempfile, "mkdtemp", wraps=tempfile.mkdtemp) as stage:
                    with self.assertRaisesRegex(ValueError, "Reserved package destination"):
                        packager.build_package(project / "game.json", self.runtime, output)
                    stage.assert_not_called()
                self.assertFalse(output.exists())
                self.assertFalse(list(self.base.glob(".archura-package-*")))
                self.assertEqual((self.runtime / "assets/shaders/basic.vert").read_text(), "test fixture")
                self.assertEqual((self.runtime / "Resources/Scripts/net472/ScriptCore.dll").read_text(),
                                 "test fixture")

    def test_game_scripts_and_similarly_named_assets_remain_allowed(self):
        names = ["Resources/Scripts/net472/GameScripts.dll",
                 "Resources/Scripts/net472/ScriptCore.dll.config",
                 "assets/shaders-custom/basic.vert", "assets/textures/albedo.bmp",
                 "assets/models/custom~name.obj"]
        for name in names:
            self.manifest["content"].append(name)
            self.write(self.project / name, f"project content: {name}")
        self.write(self.project / "game.json", json.dumps(self.manifest))
        output = self.build()
        for name in names:
            self.assertEqual((output / name).read_text(), f"project content: {name}")
        for name in ["assets/shaders/basic.vert", "Resources/Scripts/net472/ScriptCore.dll"]:
            self.assertEqual((output / name).read_text(), "test fixture")

    def test_case_alias_content_destinations_reject(self):
        names = ["assets/models/object.obj", "assets/models/OBJECT.obj"]
        self.manifest["content"].extend(names)
        self.write(self.project / "game.json", json.dumps(self.manifest))
        with self.assertRaisesRegex(ValueError, "Duplicate content destination"):
            self.build()
        self.assertFalse((self.base / "published").exists())

    def test_short_name_destinations_reject(self):
        names = ["Resources/Scripts/net472/SCRIPT~1.DLL",
                 "Resources/Scripts/net472/script~12.dll", "assets/models/OBJECT~1.obj"]
        for index, name in enumerate(names):
            with self.subTest(path=name):
                manifest = copy.deepcopy(self.manifest)
                manifest["content"].append(name)
                self.write(self.project / "game.json", json.dumps(manifest))
                self.write(self.project / name, "project overwrite")
                output = self.base / f"short-rejected-{index}"
                with mock.patch.object(packager.tempfile, "mkdtemp", wraps=tempfile.mkdtemp) as stage:
                    with self.assertRaisesRegex(ValueError, "short filename alias"):
                        packager.build_package(self.project / "game.json", self.runtime, output)
                    stage.assert_not_called()
                self.assertFalse(output.exists())

    @unittest.skipUnless(os.name == "nt", "Windows short-name alias check")
    def test_actual_runtime_short_name_alias_rejects(self):
        import ctypes
        core = self.runtime / "Resources/Scripts/net472/ScriptCore.dll"
        get_short = ctypes.WinDLL("kernel32", use_last_error=True).GetShortPathNameW
        get_short.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32]
        get_short.restype = ctypes.c_uint32
        size = get_short(str(core), None, 0)
        if not size:
            raise ctypes.WinError(ctypes.get_last_error())
        buffer = ctypes.create_unicode_buffer(size)
        length = get_short(str(core), buffer, size)
        if not length or length >= size:
            raise ctypes.WinError(ctypes.get_last_error())
        alias = Path(buffer.value).name
        if alias.casefold() == core.name.casefold():
            self.skipTest("Windows short-name generation unavailable")
        self.assertTrue(core.samefile(core.with_name(alias)))
        name = "Resources/Scripts/net472/" + alias
        self.manifest["content"].append(name)
        self.write(self.project / "game.json", json.dumps(self.manifest))
        self.write(self.project / name, "project overwrite")
        with self.assertRaisesRegex(ValueError, "short filename alias"):
            self.build()
        self.assertFalse((self.base / "published").exists())
        self.assertEqual(core.read_text(), "test fixture")

    def test_undeclared_scene_asset_rejects(self):
        for path in ["assets/models/missing.obj", "C:/original/models/object.obj", "../outside.obj"]:
            self.write(self.project / "scenes/start.scene", json.dumps({"entities": [{"model_path": path}]}))
            with self.assertRaises(ValueError):
                self.build()
            self.assertFalse((self.base / "published").exists())

    def test_bad_schema_and_duplicate_json(self):
        for update in [{"version": True}, {"version": 2}, {"title": ""},
                       {"title": "x" * 129}, {"player_id": "0" * 32}, {"content": []}, {"unknown": 1}]:
            bad = dict(self.manifest, **update)
            self.write(self.project / "game.json", json.dumps(bad))
            with self.assertRaises(ValueError):
                self.build()
        self.write(self.project / "game.json", '{"version":1,"version":1}')
        with self.assertRaises(ValueError):
            self.build()


if __name__ == "__main__":
    unittest.main()
