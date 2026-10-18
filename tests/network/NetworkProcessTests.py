"""Real loopback TCP: dedicated server, native clients and hostile wire peers."""
import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument("--server", required=True)
parser.add_argument("--client", required=True)
args = parser.parse_args()
children = []


def spawn(arguments, cwd):
    process = subprocess.Popen(arguments, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, bufsize=1, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    lines = []
    thread = threading.Thread(target=lambda: [lines.append(line.rstrip()) for line in process.stdout], daemon=True)
    thread.start()
    children.append((process, lines, thread))
    return process, lines


def wait_for(predicate, seconds=5):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.01)
    raise AssertionError("Timed out waiting for network condition")


def frame(kind, sequence, payload=b"", version=2):
    return struct.pack("!IHHHII", 0x41524348, version, kind, 0, len(payload), sequence) + payload


def exact(sock, size):
    output = b""
    while len(output) < size:
        data = sock.recv(size - len(output))
        if not data:
            raise AssertionError("Unexpected socket close")
        output += data
    return output


def receive(sock):
    magic, version, kind, flags, size, seq = struct.unpack("!IHHHII", exact(sock, 18))
    assert magic == 0x41524348 and version == 2 and not flags and 0 < seq and size <= 262144
    return kind, exact(sock, size)


def closed(sock):
    end = time.monotonic() + 2
    sock.settimeout(0.3)
    while time.monotonic() < end:
        try:
            if not sock.recv(4096):
                return
        except (ConnectionResetError, ConnectionAbortedError):
            return
        except socket.timeout:
            pass
    raise AssertionError("Invalid or timed-out peer remained open")


def handshake(sock, fragmented=False):
    hello = frame(1, 1, struct.pack("!Q", 12345))
    if fragmented:
        for part in (hello[:3], hello[3:17], hello[17:]):
            sock.sendall(part)
            time.sleep(0.01)
    else:
        sock.sendall(hello)
    kind, payload = receive(sock)
    assert kind == 6 and len(payload) == 12
    token, peer_id = struct.unpack("!QI", payload)
    assert token == 12345 and peer_id >= 2
    return peer_id


def snapshots(lines):
    result = []
    for line in list(lines):
        words = line.split()
        if not words or words[0] != "SNAP":
            continue
        players = {}
        for i in range(4, len(words), 4):
            assert words[i] == "PLAYER"
            players[int(words[i + 1])] = (float(words[i + 2]), float(words[i + 3]))
        result.append((int(words[1]), int(words[3]), players))
    return result


temporary = tempfile.TemporaryDirectory(prefix="archura-network-process-")
try:
    root = Path(temporary.name).resolve()
    assert root.parent == Path(tempfile.gettempdir()).resolve()
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    config = root / "server.json"
    config.write_text(json.dumps({"connection_timeout": 0.2, "client_timeout": 0.7}))
    server, server_lines = spawn([args.server, "--port", str(port), "--config", str(config)], root)
    wait_for(lambda: any(line.startswith("NETWORK_READY") for line in server_lines) or server.poll() is not None)
    assert server.poll() is None, server_lines
    time.sleep(0.1)  # Idle/empty peer set must not close the listener on Windows.

    def connect():
        return socket.create_connection(("127.0.0.1", port), timeout=2)

    # A fragmented hello and coalesced input frames must work on real TCP.
    with connect() as peer:
        peer_id = handshake(peer, fragmented=True)
        peer.sendall(frame(8, 2, struct.pack("!Iff", 1, 1, 0)) + frame(8, 3, struct.pack("!Iff", 2, 0, 1)))
        acknowledged = False
        for attempt in range(64):
            kind, payload = receive(peer)
            assert kind == 5, "Expected actual world snapshot"
            count = struct.unpack_from("!I", payload, 12)[0]
            for i in range(count):
                identity, ack, x, z = struct.unpack_from("!IIff", payload, 16 + i * 16)
                if identity == peer_id and ack == 2 and z > 0:
                    acknowledged = True
            if acknowledged:
                break
        assert acknowledged, "Coalesced input did not reach authoritative simulation"

    for attack in [
        frame(8, 2, struct.pack("!Iff", 1, float("nan"), 0)),
        frame(8, 2, struct.pack("!Iff", 1, 2, 0)),
        frame(8, 2, struct.pack("!Iff", 0, 0, 0)),
        frame(8, 2, b"short"),
        frame(3, 2, struct.pack("!Ifffff", 999, 1e9, 0, 0, 0, 0)),
        frame(5, 2, b"forged world"),
        frame(8, 2, struct.pack("!Iff", 1, 1, 0)) + frame(8, 2, struct.pack("!Iff", 2, 1, 0)),
        frame(8, 2, struct.pack("!Iff", 1, 1, 0)) + frame(8, 3, struct.pack("!Iff", 1, 1, 0)),
    ]:
        with connect() as peer:
            handshake(peer)
            peer.sendall(attack)
            closed(peer)
    with connect() as peer:  # Old protocol must fail explicitly, not reinterpret v1 positions.
        peer.sendall(frame(1, 1, struct.pack("!Q", 123), version=1))
        closed(peer)
    with connect() as peer:  # Handshake timeout.
        closed(peer)
    with connect() as peer:  # Established idle timeout (snapshots alone do not count as input activity).
        handshake(peer)
        closed(peer)

    first, first_lines = spawn([args.client, "--port", str(port), "--duration-ms", "1600", "--move-x", "1"], root)
    second, second_lines = spawn([args.client, "--port", str(port), "--duration-ms", "3300", "--move-z", "-1"], root)
    wait_for(lambda: any(len(state[2]) == 2 for state in snapshots(first_lines)) and
                    any(len(state[2]) == 2 for state in snapshots(second_lines)))
    first_id = snapshots(first_lines)[0][1]
    second_id = snapshots(second_lines)[0][1]
    assert first_id != second_id and min(first_id, second_id) >= 2
    wait_for(lambda: any(first_id in players and second_id in players and
                         players[first_id][0] > 0.3 and players[second_id][1] < -0.3
                         for _, _, players in snapshots(second_lines)))
    assert first.wait(timeout=5) == 0, first_lines
    wait_for(lambda: any(set(players) == {second_id} for _, _, players in snapshots(second_lines)))
    assert second.wait(timeout=5) == 0, second_lines
    first_states = {tick: players for tick, _, players in snapshots(first_lines)}
    second_states = {tick: players for tick, _, players in snapshots(second_lines)}
    common_ticks = first_states.keys() & second_states.keys()
    assert common_ticks and all(first_states[tick] == second_states[tick] for tick in common_ticks), "Clients diverged"
    for lines in (first_lines, second_lines):
        ticks = [tick for tick, _, _ in snapshots(lines)]
        assert ticks and all(a < b for a, b in zip(ticks, ticks[1:])), "Snapshot order changed"
    assert server.poll() is None, "Hostile peers killed server"
    print("Real SDL_net: two native ECS clients, movement, spawn/despawn, malformed/replay/spoof/version/timeout rejection PASS")
finally:
    for process, lines, thread in children:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill(); process.wait(timeout=3)
        thread.join(timeout=1)
        process.stdout.close()

    temporary.cleanup()
