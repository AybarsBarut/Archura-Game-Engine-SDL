#include "core/GameLoopHarness.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

// Synthetic callback overhead only: no scene, GPU, sockets, swap or sleeping.
// Compare matching compiler/configuration and hardware; this is not an FPS claim.
int main() {
    using Clock = std::chrono::steady_clock;
    for (const int fps : {30, 60, 120, 144, 240}) {
        Archura::GameLoopHarness loop;
        Archura::GameLoopHarness::Callbacks callbacks;
        std::uint64_t updates = 0;
        callbacks.fixedTick = [&updates](double) { ++updates; };
        callbacks.render = [](double) {};
        std::vector<double> samples;
        // Batch to avoid reporting zero-duration samples at clock resolution.
        constexpr int batches = 2048, framesPerBatch = 64;
        samples.reserve(batches);
        for (int batch = 0; batch < batches; ++batch) {
            const auto start = Clock::now();
            for (int frame = 0; frame < framesPerBatch; ++frame)
                loop.RunFrame(1.0 / fps, callbacks);
            samples.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count() / framesPerBatch);
        }
        std::sort(samples.begin(), samples.end());
        std::cout << "synthetic fps=" << fps << " frames=" << batches * framesPerBatch
                  << " ticks=" << updates << " dropped_seconds=" << loop.DroppedSeconds()
                  << " batch_mean_callback_us_p50=" << samples[batches / 2 - 1]
                  << " p95=" << samples[(batches * 95 + 99) / 100 - 1]
                  << " p99=" << samples[(batches * 99 + 99) / 100 - 1] << '\n';
    }
}
