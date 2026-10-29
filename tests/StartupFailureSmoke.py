"""A missing default shader must fail startup, including partial splash cleanup.

Use an isolated working directory; never rename or modify the runtime's assets.
The executable and its DLLs remain in the staged runtime directory.
"""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--engine", type=pathlib.Path, required=True)
args = parser.parse_args()
engine = args.engine.resolve(strict=True)
for mode in ("--smoke-test", "--startup-smoke-test"):
    with tempfile.TemporaryDirectory(prefix="archura-startup-failure-") as work:
        result = subprocess.run(
            [str(engine), mode, "--graphics=opengl", "--no-graphics-fallback"],
            cwd=work, capture_output=True, text=True, errors="replace", timeout=45,
        )
        output = result.stdout + result.stderr
        if result.returncode == 0 or "EXCEPTION: RenderSystem initialization failed" not in output:
            raise AssertionError(f"{mode}: expected RenderSystem startup failure, exit={result.returncode}\n{output}")
        if "Startup smoke completed" in output:
            raise AssertionError("Failed startup was incorrectly reported as completed")
print("Missing-shader startup failures propagated with and without splash")
