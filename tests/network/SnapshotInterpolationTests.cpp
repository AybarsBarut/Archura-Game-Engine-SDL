#include "network/SnapshotInterpolation.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Archura::Net;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Near(float value, float expected, const char* message) { Check(std::abs(value-expected) < 0.0001F, message); }
WorldSnapshot State(std::uint64_t tick, float z) { return {tick, {{2, 1, 0, z}}}; }

int main() {
    try {
        // A single fixed128Hz authoritative trajectory, reused at every render rate.
        AuthoritativeWorld world;
        Check(world.SynchronizePeers({2}), "peer");
        std::vector<WorldSnapshot> snapshots;
        for (std::uint32_t tick = 1; tick <= 258; ++tick) {
            Check(world.AcceptInput(2, {tick, 0, 1}), "input"); world.Tick();
            if (!(tick % 2)) snapshots.push_back(world.Snapshot());
        }
        const auto immutable = EncodeWorldSnapshot(snapshots.back());
        for (int fps : {30, 60, 120, 144, 240}) {
            SnapshotInterpolation buffer;
            std::size_t next = 0;
            float previous = 0;
            bool fractional = false;
            for (int frame = 0; frame <= fps*2; ++frame) {
                const double now = static_cast<double>(frame)/fps;
                while (next < snapshots.size() && next/64.0 <= now+1e-10) {
                    Check(buffer.Push(snapshots[next], next/64.0), "ordered snapshots"); ++next;
                }
                const auto sample = buffer.Sample(now);
                const float z = sample.players[0].z;
                Near(z, 0.09375F+6.0F*static_cast<float>(std::max(0.0, now-4.0/128)), "FPS-independent delayed trajectory");
                Check(z >= previous && z <= snapshots[next-1].players[0].z, "bounded monotonic view");
                previous = z;
                fractional |= std::abs(z*128.0F/6-std::round(z*128.0F/6)) > 0.01F;
            }
            Check(fractional, "render samples between server ticks");
            Check(buffer.Size() == SnapshotInterpolation::Capacity, "bounded history");
            Near(buffer.Sample(100).players[0].z, snapshots.back().players[0].z, "starvation freezes at latest");
            Near(buffer.Sample(0).players[0].z, snapshots.back().players[0].z, "backward clock cannot reverse");
        }
        Check(EncodeWorldSnapshot(snapshots.back()) == immutable &&
              EncodeWorldSnapshot(world.Snapshot()) == immutable, "render cannot mutate authority");

        SnapshotInterpolation jitter;
        Check(jitter.Sample(0).players.empty(), "empty buffer");
        float previous = 0;
        // Alternating late and bunched packets, then a render stall.
        double arrival = 0;
        for (int i = 0; i < 50; ++i) {
            arrival += (i%2 ? 0.001 : 0.03025);
            Check(jitter.Push(State(100+i*2, i*0.09375F), arrival), "jitter push");
            const float z = jitter.Sample(arrival).players[0].z;
            Check(z >= previous && z <= i*0.09375F, "jitter cannot reverse/extrapolate"); previous = z;
        }
        const auto last = jitter.Sample(arrival+0.2);
        Near(last.players[0].z, 49*0.09375F, "frame stall clamps");
        Check(!jitter.Push(State(198, 0), arrival+1) && !jitter.Push(State(197, 0), arrival+1), "duplicate/stale reject");
        Check(!jitter.Push(State(200, 0), arrival-1), "backward arrival reject");
        Check(!jitter.Push(State(200, 0), std::numeric_limits<double>::quiet_NaN()), "NaN time reject");
        Check(!jitter.Push(State(200, std::numeric_limits<float>::infinity()), arrival+1), "invalid position reject");
        Near(jitter.Sample(std::numeric_limits<double>::quiet_NaN()).players[0].z, last.players[0].z, "invalid input preserves view");
        Check(jitter.Push(State(1000, 20), arrival+2), "long gap rebase");
        Check(jitter.Size() == 1, "old gap history discarded");
        Near(jitter.Sample(arrival+2).players[0].z, 20, "gap snaps");
        jitter.Reset();
        Check(jitter.Sample(0).players.empty() && jitter.Push(State(2, -10), 0), "reconnect resets tick/time/history");
        Near(jitter.Sample(0).players[0].z, -10, "no previous connection blending");
        Check(jitter.Push(State(4, 10), .015625), "teleport");
        // Discontinuity snaps immediately, then holds its first new pose.
        Near(jitter.Sample(.015625).players[0].z, 10, "teleport immediate");
        Near(jitter.Sample(.04).players[0].z, 10, "teleport snaps rather than flying");
        Check(jitter.Push({6, {}}, .05) && jitter.Sample(.05).players.empty(), "despawn immediate");
        Check(jitter.Push(State(8, 30), .06), "respawn");
        Near(jitter.Sample(.06).players[0].z, 30, "spawn does not blend with removed player");
        jitter.Reset();
        Check(jitter.Push(State(2, 0), 0) && jitter.Push(State(4, .09375F), .015625) &&
              jitter.Push({6, {}}, .016) && jitter.Push(State(8, .15F), .017), "bunched ID reuse");
        Near(jitter.Sample(.017).players[0].z, .15F, "ID reuse cannot resurrect delayed prior lifetime");
        jitter.Reset();
        Check(jitter.Push(State(2, 0), 0) && jitter.Push(State(4, 10), .015625), "teleport start");
        previous = 10;
        for (int i = 2; i <= 12; ++i) {
            const double now = i/64.0;
            Check(jitter.Push(State(2+i*2, 10+(i-1)*.09375F), now), "post-teleport trajectory");
            const float z = jitter.Sample(now).players[0].z;
            Check(z >= previous, "resuming interpolation must not rewind after a snap"); previous = z;
        }
        jitter.Reset();
        Check(jitter.Push({2, {{2, 1, 0, 0}, {3, 2, 0, 0}}}, 0) &&
              jitter.Push({4, {{2, 3, .09375F, 0}, {3, 4, 0, .09375F}}}, .015625), "two trajectories");
        const auto pair = jitter.Sample(.0390625);
        Near(pair.players[0].x, .046875F, "local interpolation");
        Near(pair.players[1].z, .046875F, "remote interpolation");
        Check(pair.players[0].acknowledgedInput == 3 && pair.players[1].acknowledgedInput == 4, "acks remain latest authority");
        Check(!jitter.Push({6, {{3, 1, 0, 0}, {2, 1, 0, 0}}}, .04) &&
              !jitter.Push(State(6, 257), .04) && !jitter.Push(State(0, 0), .04), "malformed snapshots reject");
        jitter.Reset();
        const auto high = std::numeric_limits<std::uint64_t>::max()-10;
        Check(jitter.Push(State(high, 0), 0) && jitter.Push(State(high+2, .09375F), .015625), "large server ticks");
        Near(jitter.Sample(.0390625).players[0].z, .046875F, "relative ticks retain fractional precision");
        std::cout << "Snapshot interpolation: 30/60/120/144/240FPS, immutable128Hz authority, jitter/stall/bounds/reset PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
