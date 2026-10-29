"""Bounded real TCP nonreader regression; parent kills a blocked fixture.

The tiny receive window makes saturation quick, but success depends on observed
Backpressure and intact delivery to the other peer, never an assumed buffer size.
"""
import argparse
import os
import socket
import struct
import subprocess
import threading
import time


def exact(peer, size):
    result = bytearray()
    while len(result) < size:
        block = peer.recv(size - len(result))
        if not block:
            raise AssertionError("stream closed before completion marker")
        result.extend(block)
    return bytes(result)


def receive(peer, sequence):
    magic, version, kind, flags, size, actual_sequence = struct.unpack("!IHHHII", exact(peer, 18))
    assert (magic, version, flags, actual_sequence) == (0x41524348, 2, 0, sequence)
    assert size <= 262144
    return kind, exact(peer, size)


def connect(port, small_window=False):
    peer = socket.socket()
    if small_window:
        peer.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
    peer.settimeout(3)
    peer.connect(("127.0.0.1", port))
    peer.sendall(struct.pack("!IHHHIIQ", 0x41524348, 2, 1, 0, 8, 1, 12345))
    kind, welcome = receive(peer, 1)
    assert kind == 6 and len(welcome) == 12 and struct.unpack("!QI", welcome)[0] == 12345
    return peer


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    args = parser.parse_args()
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    child = subprocess.Popen([args.fixture, str(port)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, bufsize=1,
                             creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    lines = []
    output = threading.Thread(target=lambda: [lines.append(line.rstrip()) for line in child.stdout], daemon=True)
    output.start()
    peers = []
    errors = []
    received = []
    try:
        deadline = time.monotonic() + 8
        while "READY" not in lines:
            assert child.poll() is None, lines
            assert time.monotonic() < deadline, "fixture readiness watchdog expired"
            time.sleep(0.005)
        slow = connect(port, small_window=True)
        peers.append(slow)  # Intentionally never read another byte from this peer.
        healthy = connect(port)
        peers.append(healthy)

        def read_healthy():
            try:
                sequence, frame_index = 2, 0
                while True:
                    kind, payload = receive(healthy, sequence)
                    sequence += 1
                    if kind == 7:
                        assert payload == b""
                        continue
                    assert kind == 5
                    if payload == b"BACKPRESSURE_DONE":
                        assert frame_index > 0, "healthy peer made no progress during flood"
                        received.append(frame_index)
                        return
                    assert len(payload) == 262144
                    assert struct.unpack_from("!I", payload)[0] == frame_index, "lost/reordered world frame"
                    assert payload[4:] == bytes([frame_index % 251]) * (len(payload) - 4), "partial write corrupted payload"
                    frame_index += 1
            except BaseException as error:
                errors.append(error)

        reader = threading.Thread(target=read_healthy, daemon=True)
        reader.start()
        try:
            child.wait(timeout=max(0.1, deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            raise AssertionError("8s parent watchdog: transport blocked while established peer stopped reading")
        output.join(timeout=1)
        reader.join(timeout=1)
        assert child.returncode == 0, lines
        assert not errors, errors
        assert received, "responsive peer did not receive completion marker"
        assert any(line.startswith("PASS ") for line in lines), lines
        print("\n".join(lines))
        print("PASS responsive wire sequence/payload intact, frames=" + str(received[0]))
    finally:
        if child.poll() is None:
            child.kill()
        child.wait(timeout=2)
        for peer in peers:
            peer.close()
        output.join(timeout=1)
        if child.returncode != 0:
            print("\n".join(lines))


if __name__ == "__main__":
    main()
