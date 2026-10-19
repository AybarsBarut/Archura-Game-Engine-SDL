"""Real dedicated server and two OpenGL Application processes, including teardown."""
import argparse
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import time
import os

parser = argparse.ArgumentParser()
parser.add_argument("--server", required=True, type=Path)
parser.add_argument("--engine", required=True, type=Path)
args = parser.parse_args()
children = []
temporary = tempfile.TemporaryDirectory(prefix="archura-graphical-network-")

def spawn(command, cwd):
    process = subprocess.Popen([str(value) for value in command], cwd=cwd, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1,
        creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    lines = []
    def read():
        for line in process.stdout:
            lines.append(line.rstrip())
    reader = threading.Thread(target=read, daemon=True)
    reader.start()
    children.append((process, lines, reader))
    return process, lines

def wait_for(predicate, timeout=15):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate(): return
        time.sleep(0.01)
    raise AssertionError("Timed out waiting for graphical session")

def states(lines):
    result = []
    for line in list(lines):
        words = line.split()
        if not words or words[0] != "GFXSNAP": continue
        tick, identity = int(words[1]), int(words[3])
        camera = tuple(float(value) for value in words[5:8])
        remote_count, visible, paused = int(words[9]), int(words[11]), int(words[13])
        divider = words.index("VIEW")
        players = {int(words[i+1]): (float(words[i+2]), float(words[i+3])) for i in range(14, divider, 4)}
        view = {int(words[i+1]): (float(words[i+2]), float(words[i+3])) for i in range(divider+1, len(words), 4)}
        assert identity in players and remote_count == len(players)-1 and visible > 0
        assert view.keys() == players.keys(), "Presentation resurrected/despawned peers"
        assert abs(camera[0]-view[identity][0]) < 0.00001 and abs(camera[2]-view[identity][1]) < 0.00001
        assert abs(camera[1]-1.6) < 0.00001, "Camera must use interpolated feet+eye height"
        result.append((tick, identity, camera, paused, players))
    return result

try:
    root = Path(temporary.name).resolve()
    assert root.parent == Path(tempfile.gettempdir()).resolve()
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0)); port = reserve.getsockname()[1]
    server, server_lines = spawn([args.server, "--port", port], root)
    wait_for(lambda: any(line.startswith("NETWORK_READY") for line in server_lines) or server.poll() is not None)
    assert server.poll() is None, server_lines
    common = [args.engine, "--connect", "127.0.0.1", "--port", port, "--network-smoke-test",
              "--graphics=opengl", "--no-graphics-fallback"]
    first, first_lines = spawn(common + ["--network-move-x", "1", "--network-move-z", "0"], args.engine.parent)
    second, second_lines = spawn(common + ["--network-move-x", "0", "--network-move-z", "1"], args.engine.parent)
    assert first.wait(timeout=20) == 0, first_lines[-20:]
    assert second.wait(timeout=20) == 0, second_lines[-20:]
    for lines in (first_lines, second_lines):
        assert any("Graphical network smoke completed and shutdown" in line for line in lines), lines[-10:]
        snapshots = states(lines)
        assert len(snapshots) >= 16 and len({state[1] for state in snapshots}) >= 2, "Stop/reconnect not observed"
        assert any(len(state[4]) >= 2 for state in snapshots), "No remote mesh view"
    # Both real renderers must display the same world at common authority ticks.
    one = {state[0]: state[4] for state in states(first_lines)}
    two = {state[0]: state[4] for state in states(second_lines)}
    intersection = one.keys() & two.keys()
    assert intersection and all(one[tick] == two[tick] for tick in intersection), "Graphical clients diverged"
    # The moving Z client is away from obstacles; pause must stop its input promptly.
    paused = [state for state in states(second_lines) if state[3]]
    assert len(paused) >= 7, "Pause transition was not exercised"
    middle = [state[2][2] for state in paused if state[0] >= paused[0][0]+20]
    assert len(middle) >= 2, "Pause did not outlast presentation delay"
    assert max(middle)-min(middle) <= 0.12, "Paused client continued moving"
    assert any(abs(state[2][2]-state[4][state[1]][1]) > 0.001 for state in states(second_lines)), "View still snaps to latest authority"
    # The X client must stop at the obstacle represented by its rendered cube.
    initial_id = states(first_lines)[0][1]
    initial_x = [state[2][0] for state in states(first_lines) if state[1] == initial_id]
    assert 2.5 < max(initial_x) <= 2.65001, "Graphical/server obstacle boundary mismatch"

    # Failure with live GPU/scene owners must close cleanly rather than retain callbacks.
    failed, failed_lines = spawn(common, args.engine.parent)
    wait_for(lambda: bool(states(failed_lines)) or failed.poll() is not None)
    assert failed.poll() is None, failed_lines[-10:]
    server.terminate(); server.wait(timeout=3)
    assert failed.wait(timeout=10) != 0, "Server loss was silently accepted by smoke"
    assert any("Graphical network failed" in line for line in failed_lines), failed_lines[-10:]
    print("Two OpenGL Application clients: camera/remote meshes/render, shared collision, pause/Stop/reconnect/authored safety and server-loss teardown PASS")
finally:
    for process, lines, reader in children:
        if process.poll() is None: process.terminate()
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=3)
        reader.join(timeout=1); process.stdout.close()
    temporary.cleanup()
