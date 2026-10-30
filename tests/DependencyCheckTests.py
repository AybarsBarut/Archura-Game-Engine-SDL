import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "dependency_check", Path(__file__).resolve().parents[1] / "tools/check_build_dependencies.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class DependencyContracts(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="archura-dependency-check-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        (self.root / "cmake").mkdir()
        self.binary = self.root / "sdk.dll"
        self.binary.write_bytes(b"fixed SDK fixture")
        self.entry = {"path": "sdk.dll", "profiles": ["windows-contracts"],
                      "sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest()}
        self.write_inventory()

    def write_inventory(self):
        (self.root / "cmake/dependency-pins.json").write_text(
            json.dumps({"version": 1, "files": [self.entry]}), encoding="utf-8")

    def test_valid_input_and_tampering(self):
        self.assertEqual(checker.check_dependencies(self.root, "windows-contracts"), [])
        self.binary.write_bytes(b"different bytes")
        self.assertEqual(checker.check_dependencies(self.root, "windows-contracts"),
                         ["SHA256 mismatch: sdk.dll"])

    def test_missing_file_and_invalid_profile(self):
        self.binary.unlink()
        self.assertEqual(checker.check_dependencies(self.root, "windows-contracts"),
                         ["Required input is missing: sdk.dll"])
        with self.assertRaises(ValueError):
            checker.check_dependencies(self.root, "not-a-profile")

    def test_root_escape_rejects(self):
        self.entry["path"] = "../outside.dll"
        self.write_inventory()
        self.assertEqual(checker.check_dependencies(self.root, "windows-contracts"),
                         ["Invalid dependency inventory path: ../outside.dll"])

    def test_untracked_required_input_is_not_a_pass(self):
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)
        self.assertEqual(checker.check_dependencies(self.root, "windows-contracts", True),
                         ["Required input is not tracked: sdk.dll"])
        subprocess.run(["git", "add", "--", "sdk.dll"], cwd=self.root, check=True)
        self.assertEqual(checker.check_dependencies(self.root, "windows-contracts", True), [])


if __name__ == "__main__":
    unittest.main()
