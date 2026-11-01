import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("local_validation", Path(__file__).resolve().parents[1] / "tools/validate_local.py")
validation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validation)
GOOD = '<testsuite><testcase name="actual"/><testcase name="optional"><skipped/></testcase></testsuite>'


class LocalValidationTests(unittest.TestCase):
    def invoke(self, root, xml=GOOD, fail=None, source_change=False):
        def execute(command, cwd, log, timeout):
            log.write_text("fixture tool output\n", encoding="utf-8")
            if fail:
                result = fail(command)
                if result is not None:
                    return result
            if "--output-junit" in command and xml is not None:
                Path(command[command.index("--output-junit") + 1]).write_text(xml, encoding="utf-8")
            return 0
        states = [{"commit": "a", "status": "dirty"}, {"commit": "b" if source_change else "a", "status": "dirty"}]
        with patch.object(validation, "source_state", side_effect=states), patch.object(validation, "execute", side_effect=execute), patch("builtins.print"):
            path, code = validation.validate(Path(root), ["desktop"], ["Debug", "Release"], 30)
        return path, code, json.loads(path.read_text(encoding="utf-8"))

    def test_success_records_executed_and_skipped_and_unique_evidence(self):
        with tempfile.TemporaryDirectory() as root:
            first, code, report = self.invoke(root)
            self.assertEqual(code, 0)
            self.assertEqual(report["status"], "passed")
            self.assertTrue(report["source_unchanged"])
            tests = [step for step in report["steps"] if "junit" in step]
            self.assertEqual(len(tests), 2)
            for step in tests:
                self.assertIn("--no-tests=error", step["command"])
                self.assertEqual(step["tests"], {"total": 2, "executed": 1, "skipped": 1, "failed": 0})
            second, _, _ = self.invoke(root)
            self.assertNotEqual(first, second)
            self.assertFalse((Path(root) / "build/.local-validation.lock").exists())

    def test_failed_build_stops_before_tests(self):
        with tempfile.TemporaryDirectory() as root:
            _, code, report = self.invoke(root, fail=lambda cmd: 7 if "--build" in cmd else None)
            self.assertEqual(code, 1)
            failed = [step for step in report["steps"] if step["status"] == "failed"]
            self.assertEqual(failed[0]["returncode"], 7)
            self.assertTrue(all(step["status"] == "not_run" for step in report["steps"] if "junit" in step))

    def test_zero_missing_failed_malformed_or_all_skipped_junit_never_passes(self):
        for xml in [None, '<testsuite/>', '<testsuite><testcase><failure/></testcase></testsuite>',
                    '<testsuite><testcase><skipped/></testcase></testsuite>',
                    '<testsuite failures="1"><testcase/></testsuite>', 'invalid xml']:
            with self.subTest(xml=xml), tempfile.TemporaryDirectory() as root:
                _, code, report = self.invoke(root, xml=xml)
                self.assertEqual(code, 1)
                self.assertEqual(report["status"], "failed")
                tests = [step for step in report["steps"] if "junit" in step]
                self.assertEqual(tests[0]["status"], "failed")
                self.assertEqual(tests[1]["status"], "not_run")

    def test_timeout_and_missing_tool_fail_and_release_owned_lock(self):
        for error in [subprocess.TimeoutExpired(["cmake"], 30), FileNotFoundError("missing cmake")]:
            def fail(_):
                raise error
            with self.subTest(error=error), tempfile.TemporaryDirectory() as root:
                _, code, report = self.invoke(root, fail=fail)
                self.assertEqual(code, 1)
                self.assertEqual(report["steps"][0]["status"], "failed")
                self.assertFalse((Path(root) / "build/.local-validation.lock").exists())

    def test_interrupt_is_not_success(self):
        def fail(_):
            raise KeyboardInterrupt
        with tempfile.TemporaryDirectory() as root:
            _, code, report = self.invoke(root, fail=fail)
            self.assertEqual(code, 1)
            self.assertEqual(report["status"], "interrupted")
            self.assertEqual(report["steps"][0]["status"], "interrupted")
            self.assertFalse((Path(root) / "build/.local-validation.lock").exists())

    def test_changed_source_invalidates_otherwise_successful_run(self):
        with tempfile.TemporaryDirectory() as root:
            _, code, report = self.invoke(root, source_change=True)
            self.assertEqual(code, 1)
            self.assertFalse(report["source_unchanged"])
            self.assertTrue(all(step["status"] == "passed" for step in report["steps"]))

    def test_existing_lock_is_retained(self):
        with tempfile.TemporaryDirectory() as root:
            lock = Path(root) / "build/.local-validation.lock"
            lock.parent.mkdir()
            lock.write_text("another run", encoding="utf-8")
            _, code, report = self.invoke(root)
            self.assertEqual(code, 1)
            self.assertEqual(lock.read_text(), "another run")
            self.assertTrue(all(step["status"] == "not_run" for step in report["steps"]))

    def test_presets_and_tracked_dependency_scope(self):
        steps = validation.plan(["desktop", "contracts", "loop"], ["Debug"], Path("evidence"))
        by_name = {step["name"]: step["command"] for step in steps}
        self.assertIn("--require-tracked", by_name["contracts-dependencies"])
        self.assertNotIn("--require-tracked", by_name["desktop-dependencies"])
        self.assertEqual(by_name["loop-Debug-build"], ["cmake", "--build", "--preset", "loop-debug"])
        self.assertEqual(by_name["desktop-configure"], ["cmake", "--preset", "windows-validation"])


if __name__ == "__main__":
    unittest.main()
