#!/usr/bin/env python3
"""Run existing Windows validation presets locally and retain honest evidence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import signal
from pathlib import Path
import platform
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET

PROFILES = {
    "desktop": ("windows-validation", "validation", "windows-runtime"),
    "contracts": ("windows-contracts", "contracts", "windows-contracts"),
    "loop": ("loop-only", "loop", None),
}


def source_state(root):
    def git(*args):
        return subprocess.run(["git", *args], cwd=root, check=True, capture_output=True).stdout
    untracked = {}
    for name in git("ls-files", "--others", "--exclude-standard", "-z").decode("utf-8").split("\0"):
        if not name:
            continue
        path = root / name
        if not path.is_file() or not path.resolve().is_relative_to(root):
            raise ValueError(f"Untracked source cannot be fingerprinted: {name}")
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        untracked[name] = digest.hexdigest()
    return {
        "commit": git("rev-parse", "HEAD").decode().strip(),
        "status": git("status", "--porcelain=v1", "--untracked-files=normal").decode("utf-8"),
        "tracked_diff_sha256": hashlib.sha256(git("diff", "--binary", "HEAD")).hexdigest(),
        "untracked_sha256": untracked,
    }


def test_counts(path):
    root = ET.parse(path).getroot()
    declared_failures = sum(int(node.get("failures", "0")) + int(node.get("errors", "0")) for node in root.iter())
    cases = list(root.iter("testcase"))
    if not cases:
        raise ValueError("JUnit contains zero tests")
    skipped = sum(case.find("skipped") is not None or case.get("status") in ("notrun", "disabled") for case in cases)
    failures = sum(case.find("failure") is not None or case.find("error") is not None for case in cases)
    if failures or declared_failures or len(cases) == skipped:
        raise ValueError("JUnit reports failures or no executed tests")
    return {"total": len(cases), "executed": len(cases) - skipped, "skipped": skipped, "failed": failures}


def execute(command, root, log, timeout):
    options = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {"start_new_session": True}
    with log.open("wb") as stream, subprocess.Popen(command, cwd=root, stdout=stream,
            stderr=subprocess.STDOUT, **options) as process:
        try:
            return process.wait(timeout=timeout)
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            # Stop this invocation's build/test descendants before releasing its
            # lock, so an interrupted MSBuild cannot race the next validation.
            try:
                if os.name == "nt":
                    subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                        stdout=stream, stderr=subprocess.STDOUT, timeout=15, check=False)
                else:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=15)
            raise


def plan(profiles, configurations, evidence):
    steps = []
    def add(name, command, junit=None):
        step = {"name": name, "command": command, "status": "not_run"}
        if junit is not None:
            step["junit"] = str(junit)
        steps.append(step)
    add("cmake-version", ["cmake", "--version"])
    add("ctest-version", ["ctest", "--version"])
    for profile in profiles:
        configure, prefix, dependency = PROFILES[profile]
        if dependency:
            command = [sys.executable, "tools/check_build_dependencies.py", "--profile", dependency]
            if profile == "contracts":
                command.append("--require-tracked")
            add(f"{profile}-dependencies", command)
        add(f"{profile}-configure", ["cmake", "--preset", configure])
        for configuration in configurations:
            preset = f"{prefix}-{configuration.lower()}"
            add(f"{profile}-{configuration}-build", ["cmake", "--build", "--preset", preset])
            junit = evidence / f"{profile}-{configuration}.xml"
            add(f"{profile}-{configuration}-tests", ["ctest", "--preset", preset,
                "--output-on-failure", "--no-tests=error", "--output-junit", str(junit)], junit)
    return steps


def validate(root, profiles, configurations, timeout):
    root = Path(root).resolve()
    evidence = root / "build" / "local-validation" / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ-") + uuid.uuid4().hex[:8])
    evidence.mkdir(parents=True, exist_ok=False)
    report_path = evidence / "report.json"
    report = {"schema_version": 1, "status": "running", "platform": platform.platform(),
              "python": sys.version, "profiles": profiles, "configurations": configurations,
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "scope": "Local Windows presets in this provisioned checkout; not hosted/Linux acceptance.",
              "steps": plan(profiles, configurations, evidence)}
    def publish():
        temporary = report_path.with_suffix(".tmp")
        temporary.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        temporary.replace(report_path)
    lock_path = root / "build" / ".local-validation.lock"
    # Never replace another invocation's lock or delete arbitrary output trees.
    try:
        lock = lock_path.open("x", encoding="utf-8")
    except FileExistsError:
        report.update(status="failed", error=f"Another local validation owns {lock_path}. A stale lock must be inspected manually.")
        publish()
        return report_path, 1
    try:
        with lock:
            lock.write(str(evidence))
            lock.flush()
            report["source_before"] = source_state(root)
            publish()
            for index, step in enumerate(report["steps"]):
                log = evidence / f"{index:02d}-{step['name']}.log"
                step.update(status="running", log=str(log))
                publish()
                print(f"[local] {step['name']}", flush=True)
                start = time.monotonic()
                try:
                    step["returncode"] = execute(step["command"], root, log, timeout)
                    if step["returncode"] != 0:
                        raise ValueError(f"Command exited {step['returncode']}")
                    if "junit" in step:
                        step["tests"] = test_counts(Path(step["junit"]))
                    step["status"] = "passed"
                except (OSError, ValueError, ET.ParseError, subprocess.TimeoutExpired) as error:
                    step.update(status="failed", error=str(error))
                except KeyboardInterrupt:
                    step.update(status="interrupted", error="Interrupted by user")
                    raise
                finally:
                    step["seconds"] = round(time.monotonic() - start, 3)
                    publish()
                if step["status"] != "passed":
                    break
            report["source_after"] = source_state(root)
            report["source_unchanged"] = report["source_before"] == report["source_after"]
            report["status"] = "passed" if report["source_unchanged"] and all(s["status"] == "passed" for s in report["steps"]) else "failed"
            if not report["source_unchanged"]:
                report["error"] = "Source changed during validation; rerun for a consistent result."
    except KeyboardInterrupt:
        report.update(status="interrupted", error="Interrupted by user")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        report.update(status="failed", error=str(error))
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        publish()
        # Only release the exact lock opened by this invocation.
        try:
            if lock_path.read_text(encoding="utf-8") == str(evidence):
                lock_path.unlink()
            else:
                raise ValueError("Validation lock changed unexpectedly; inspect it before rerunning.")
        except (OSError, ValueError) as error:
            report.update(status="failed", error=f"Cannot release owned validation lock: {error}")
            publish()
    return report_path, 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=[*PROFILES, "all"], default="desktop")
    parser.add_argument("--configuration", choices=["both", "Debug", "Release"], default="both")
    parser.add_argument("--timeout-seconds", type=int, default=1800, help="Timeout per command (default: 1800)")
    args = parser.parse_args()
    if args.timeout_seconds < 1:
        parser.error("--timeout-seconds must be positive")
    profiles = list(PROFILES) if args.profile == "all" else [args.profile]
    configurations = ["Debug", "Release"] if args.configuration == "both" else [args.configuration]
    try:
        report, code = validate(Path(__file__).resolve().parents[1], profiles, configurations, args.timeout_seconds)
    except OSError as error:
        parser.exit(1, f"Cannot create local validation evidence: {error}\n")
    print(f"[local] {'PASS' if code == 0 else 'FAIL'}: {report}", flush=True)
    return code


if __name__ == "__main__":
    sys.exit(main())
