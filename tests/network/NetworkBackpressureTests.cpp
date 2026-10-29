#include "network/NetworkManager.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Process fixture: the Python parent owns the hostile sockets and watchdog.
// In particular, a blocking transport cannot hang the test runner indefinitely.
int main(int argc, char** argv) {
    using namespace Archura;
    using Clock = std::chrono::steady_clock;
    auto& network = NetworkManager::Get();
    double longestMs = 0;
    std::size_t calls = 0;
    auto timed = [&](auto&& operation) {
        const auto start = Clock::now();
        operation();
        const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        longestMs = std::max(longestMs, elapsed);
        ++calls;
        if (elapsed > 250.0) throw std::runtime_error("transport call exceeded practical 250ms test bound");
    };
    try {
        if (argc != 2) throw std::runtime_error("expected port");
        NetworkLimits limits;
        limits.maxClients = 2;
        limits.maxQueuedBytesPerPeer = 2 * (Net::kMaxPayloadSize + Net::kFrameHeaderSize);
        limits.handshakeTimeoutMs = 3000;
        limits.idleTimeoutMs = 30000;
        if (!network.SetLimits(limits) || !network.StartServer(std::atoi(argv[1])))
            throw std::runtime_error("fixture listener failed: " + network.GetLastError());
        std::cout << "READY" << std::endl;
        const auto handshakeEnd = Clock::now() + std::chrono::seconds(4);
        while (network.GetConnectedPeerIds().size() != 2) {
            timed([&] { network.UpdateServer(); });
            if (Clock::now() >= handshakeEnd) throw std::runtime_error("two peers did not establish");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::cout << "FLOOD" << std::endl;
        std::vector<std::uint8_t> payload(Net::kMaxPayloadSize);
        std::size_t peakQueue = 0;
        std::uint32_t frames = 0;
        const auto floodEnd = Clock::now() + std::chrono::seconds(4);
        while (network.GetConnectedPeerIds().size() == 2) {
            std::fill(payload.begin(), payload.end(), static_cast<std::uint8_t>(frames % 251));
            for (int byte = 0; byte != 4; ++byte)
                payload[byte] = static_cast<std::uint8_t>(frames >> (24 - byte * 8));
            timed([&] { network.SendWorldState(payload); });
            ++frames;
            // More than one bounded flush may be needed for a maximum-size frame.
            // Let a reading peer drain; do not rely on any OS send-buffer size.
            for (int update = 0; update != 16; ++update) {
                timed([&] { network.UpdateServer(); });
                const auto stats = network.GetStats();
                peakQueue = std::max(peakQueue, stats.queuedBytes);
                if (stats.queuedBytes > 2 * limits.maxQueuedBytesPerPeer)
                    throw std::runtime_error("aggregate peer queue exceeded configured cap");
            }
            if (Clock::now() >= floodEnd) throw std::runtime_error("nonreader never reached backpressure");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (network.GetConnectedPeerIds().size() != 1 ||
            network.GetLastDisconnectReason() != DisconnectReason::Backpressure)
            throw std::runtime_error("expected one slow peer evicted for Backpressure");
        const std::string marker = "BACKPRESSURE_DONE";
        bool queued = false;
        timed([&] { queued = network.SendWorldState({marker.begin(), marker.end()}); });
        if (!queued) throw std::runtime_error("responsive peer rejected completion marker");
        const auto drainEnd = Clock::now() + std::chrono::seconds(1);
        while (network.GetStats().queuedBytes != 0) {
            timed([&] { network.UpdateServer(); });
            if (Clock::now() >= drainEnd) throw std::runtime_error("responsive queue did not drain");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto healthyId = network.GetConnectedPeerIds().front();
        timed([&] { if (!network.DisconnectPeer(healthyId)) throw std::runtime_error("disconnect failed"); });
        timed([&] { network.UpdateServer(); });
        if (!network.GetConnectedPeerIds().empty()) throw std::runtime_error("disconnect did not complete");
        timed([&] { network.Shutdown(); });
        std::cout << "PASS frames=" << frames << " calls=" << calls << " peak_queue=" << peakQueue
                  << " max_call_ms=" << longestMs << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << std::endl;
        network.Shutdown();
        return 1;
    }
}
