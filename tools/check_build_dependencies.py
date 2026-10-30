"""Read-only validation of selected bundled Windows build inputs."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess


def check_dependencies(root, profile, require_tracked=False):
    root = Path(root).resolve(strict=True)
    manifest = json.loads((root / "cmake/dependency-pins.json").read_text(encoding="utf-8"))
    if manifest.get("version") != 1:
        raise ValueError("Unsupported dependency inventory version")
    selected = [entry for entry in manifest["files"] if profile in entry["profiles"]]
    if not selected:
        raise ValueError("No inputs declared for dependency profile")
    tracked = set()
    if require_tracked:
        result = subprocess.run(["git", "ls-files", "-z"], cwd=root, capture_output=True, check=True)
        tracked = set(result.stdout.decode("utf-8").split("\0"))
    errors = []
    for entry in selected:
        name = entry["path"]
        relative = PurePosixPath(name)
        path = (root / relative).resolve()
        if relative.is_absolute() or ".." in relative.parts or not path.is_relative_to(root):
            errors.append(f"Invalid dependency inventory path: {name}")
            continue
        if require_tracked and name not in tracked:
            errors.append(f"Required input is not tracked: {name}")
        if not path.is_file():
            errors.append(f"Required input is missing: {name}")
            continue
        if "sha256" in entry:
            digest = hashlib.sha256()
            with path.open("rb") as source:
                for chunk in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(chunk)
            if digest.hexdigest() != entry["sha256"]:
                errors.append(f"SHA256 mismatch: {name}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--profile", choices=["windows-contracts", "windows-runtime"],
                        default="windows-contracts")
    parser.add_argument("--require-tracked", action="store_true")
    args = parser.parse_args()
    try:
        errors = check_dependencies(args.root, args.profile, args.require_tracked)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Dependency preflight failed: {error}\n")
    if errors:
        parser.exit(1, "\n".join(errors) + "\n")
    print(f"Dependency preflight passed: {args.profile}")


if __name__ == "__main__":
    main()
