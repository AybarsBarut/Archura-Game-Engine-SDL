#include "network/NetworkSceneReplica.h"
#include "network/NetworkInputState.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Archura;
using namespace Archura::Net;
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        PlayerInputPacket input{1, 1, 0}, output{7, 0, 0};
        auto bytes = SerializePlayerInput(input);
        Check(bytes.size() == 12 && bytes[3] == 1 && bytes[4] == 0x3f && bytes[5] == 0x80, "canonical input endian");
        Check(DeserializePlayerInput(bytes, output) && output.moveX == 1, "input roundtrip");
        bytes.push_back(0);
        Check(!DeserializePlayerInput(bytes, output) && output.sequence == 1, "invalid input preserves destination");
        Check(!ValidPlayerInput({0, 0, 0}) && !ValidPlayerInput({1, 2, 0}) &&
              !ValidPlayerInput({1, std::numeric_limits<float>::quiet_NaN(), 0}), "input bounds");
        std::uint64_t token = 0; std::uint32_t id = 0;
        Check(DeserializeWelcome(SerializeWelcome(123, 9), token, id) && token == 123 && id == 9, "welcome identity");
        Check(!DeserializeWelcome(SerializeSessionToken(123), token, id), "v1 acknowledgement rejected");
        NetworkInputState keyboard;
        const auto diagonal = keyboard.Next(1, 1, true);
        Check(ValidPlayerInput(diagonal) && std::abs(std::hypot(diagonal.moveX, diagonal.moveZ)-1) < 0.001F, "bounded graphical input");
        const auto paused = keyboard.Next(1, 1, false);
        Check(paused.sequence == 2 && paused.moveX == 0 && paused.moveZ == 0, "pause/Edit emits sequenced zero input");
        keyboard.Reset();
        Check(keyboard.Next(0, 0, true).sequence == 1, "reconnect resets command sequence");
        const auto forward = PlanarMovement(0, -1, 1, 0), right = PlanarMovement(0, -1, 0, 1);
        Check(forward[1] == -1 && right[0] == 1, "camera-relative movement basis");
        float sweptX = 0, sweptZ = 0;
        MoveArenaPlayer(sweptX, sweptZ, 100, 0);
        Check(sweptX == kArenaObstacles[0].minX-kArenaPlayerRadius, "swept footprint blocks tunneling");
        MoveArenaPlayer(sweptX, sweptZ, 0, 5);
        Check(sweptZ == 5, "footprint slides along obstacle");

        AuthoritativeWorld world;
        Check(world.SynchronizePeers({3, 2}), "spawn peers");
        Check(!world.SynchronizePeers({2, 2}) && !world.SynchronizePeers({1}), "invalid peer sets");
        Check(world.AcceptInput(2, input) && !world.AcceptInput(99, input), "bound peer input");
        Check(!world.AcceptInput(2, input), "replay input");
        for (std::uint32_t sequence = 2; sequence <= 1000; ++sequence)
            Check(world.AcceptInput(2, {sequence, 1, 0}), "ordered input flood");
        world.Tick();
        Check(std::abs(world.Snapshot().players[0].x - kWorldSpeed/128) < 0.0001F, "flood cannot accelerate movement");
        Check(world.AcceptInput(3, {1, 1, 1}), "diagonal input");
        world.Tick();
        auto state = world.Snapshot();
        Check(std::abs(std::hypot(state.players[1].x, state.players[1].z) - kWorldSpeed/128) < 0.0001F, "diagonal speed capped");
        for (int tick = 0; tick < 40; ++tick) world.Tick();
        state = world.Snapshot();
        world.Tick();
        Check(state.players[0].x == world.Snapshot().players[0].x, "stale input expires after32ticks");
        for (std::uint32_t tick = 0; tick < 128; ++tick) {
            world.AcceptInput(2, {1001+tick, 1, 0}); world.Tick();
        }
        Check(world.Snapshot().players[0].x == kArenaObstacles[0].minX-kArenaPlayerRadius, "server stops at shared obstacle");
        for (std::uint32_t tick = 0; tick < 128; ++tick) {
            world.AcceptInput(2, {1129+tick, 0, 1}); world.Tick();
        }
        for (std::uint32_t tick = 0; tick < 6000; ++tick) {
            Check(world.AcceptInput(2, {1257 + tick, 1, 0}), "fresh input"); world.Tick();
        }
        Check(world.Snapshot().players[0].x == kWorldExtent-kArenaPlayerRadius, "footprint remains inside world boundary");
        Check(world.AcceptInput(2, {0x7fffffffU, 0, 0}) &&
              world.AcceptInput(2, {0xfffffffdU, 0, 0}) && world.AcceptInput(2, {1, 0, 0}), "sequence wrap");
        Check(!world.AcceptInput(2, {0xfffffffcU, 0, 0}), "old command across wrap");

        ClientWorld client;
        auto snapshotBytes = EncodeWorldSnapshot(world.Snapshot());
        Check(client.Apply(snapshotBytes), "client applies full snapshot");
        Check(!client.Apply(snapshotBytes), "client rejects old snapshots");
        auto invalid = snapshotBytes; invalid[16] = invalid[17] = invalid[18] = invalid[19] = 0;
        Check(!client.Apply(invalid) && client.Snapshot().players.size() == 2, "malformed snapshot retains state");
        invalid = snapshotBytes; invalid.push_back(0);
        WorldSnapshot preserved = client.Snapshot();
        Check(!DecodeWorldSnapshot(invalid, preserved) && preserved.tick == client.Snapshot().tick, "trailing payload rejected");

        Scene scene;
        auto* authored = scene.CreateEntity("authored must survive");
        const auto authoredHandle = authored->GetHandle();
        NetworkSceneReplica replica;
        Check(replica.Apply(scene, snapshotBytes) && scene.GetEntities().size() == 3, "replica spawns ECS entities");
        bool checkedPosition = false;
        for (const auto& entity : scene.GetEntities()) {
            if (entity->GetName() == "NetworkPlayer_3") {
                const auto pos = entity->GetComponent<Transform>()->position;
                checkedPosition = pos.x == world.Snapshot().players[1].x && pos.z == world.Snapshot().players[1].z;
            }
        }
        Check(checkedPosition, "replicated ECS transform matches authority");
        Check(!replica.Apply(scene, invalid) && scene.IsAlive(authoredHandle), "replica malformed input safety");
        Check(world.SynchronizePeers({3}), "disconnect peer"); world.Tick();
        Check(replica.Apply(scene, EncodeWorldSnapshot(world.Snapshot())) && scene.GetEntities().size() == 2 &&
              scene.IsAlive(authoredHandle), "despawn preserves authored entity");
        Scene unrelated;
        Check(!replica.Apply(unrelated, EncodeWorldSnapshot(world.Snapshot())), "replica cannot replace another scene");
        EntityHandle replicatedHandle;
        for (const auto& entity : scene.GetEntities())
            if (entity->GetName() == "NetworkPlayer_3") replicatedHandle = entity->GetHandle();
        scene.DestroyEntity(replicatedHandle);
        const auto recycledSlot = scene.CreateEntity("unrelated recycled slot")->GetHandle();
        world.Tick();
        Check(replica.Apply(scene, EncodeWorldSnapshot(world.Snapshot())) && scene.IsAlive(recycledSlot), "recycled handle preserved");
        world.SynchronizePeers({}); world.Tick();
        Check(replica.Apply(scene, EncodeWorldSnapshot(world.Snapshot())) && scene.GetEntities().size() == 2 &&
              scene.IsAlive(authoredHandle) && scene.IsAlive(recycledSlot), "last disconnect only removes replicas");
        Check(replica.Clear() && scene.IsAlive(authoredHandle) && scene.IsAlive(recycledSlot), "disconnect cleanup preserves authored scene");
        NetworkSceneReplica expired;
        { Scene old; Check(expired.Apply(old, snapshotBytes), "temporary source replica"); }
        Scene fresh;
        Check(expired.Apply(fresh, snapshotBytes) && fresh.GetEntities().size() == 2, "expired scene binding resets safely");
        std::cout << "Authoritative world/codec/replica tests passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
