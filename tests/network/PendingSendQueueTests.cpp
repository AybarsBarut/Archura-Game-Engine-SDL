#include "network/PendingSendQueue.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace Archura::Net;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void TestPartialWouldBlockAndFifo() {
    PendingSendQueue queue;
    const std::vector<std::uint8_t> first{0, 1, 2, 3, 4, 5};
    const std::vector<std::uint8_t> second{6, 7, 8, 9};
    Require(queue.Push(first, 10) && queue.Push(second, 10), "exact retained cap rejected");
    Require(!queue.Push({10}, 10), "queue exceeded retained cap");
    Require(queue.QueuedBytes() == 10 && queue.RetainedBytes() == 10, "initial accounting wrong");
    std::vector<std::uint8_t> wire;
    int calls = 0;
    auto result = queue.Flush(10, [&](const std::uint8_t* bytes, std::size_t length) {
        ++calls;
        if (calls == 1) {
            Require(length == first.size(), "writer crossed first frame boundary");
            wire.insert(wire.end(), bytes, bytes + 2);
            return IoResult{IoStatus::Ready, 2};
        }
        Require(length == 4 && std::equal(bytes, bytes + length, first.begin() + 2),
                "short write did not retain exact first-frame suffix");
        return IoResult{IoStatus::Pending, 0};
    });
    Require(result.ok && result.bytesSent == 2 && result.framesSent == 0 && calls == 2,
            "short write / would-block progress accounting wrong");
    Require(queue.QueuedBytes() == 8 && queue.RetainedBytes() == 10,
            "partially consumed frame allocation disappeared from retained accounting");
    Require(!queue.Push({10}, 10), "consumed prefix incorrectly freed queue admission capacity");

    calls = 0;
    result = queue.Flush(3, [&](const std::uint8_t* bytes, std::size_t length) {
        ++calls;
        Require(length == 3 && bytes[0] == 2, "byte budget or retry offset changed");
        wire.insert(wire.end(), bytes, bytes + length);
        return IoResult{IoStatus::Ready, length};
    });
    Require(result.ok && result.bytesSent == 3 && result.framesSent == 0 && calls == 1,
            "byte budget not enforced");
    Require(queue.QueuedBytes() == 5 && queue.RetainedBytes() == 10,
            "budget-limited partial frame accounting wrong");

    result = queue.Flush(2, [&](const std::uint8_t* bytes, std::size_t length) {
        Require(length == 1, "two-byte budget should finish first then start second frame");
        wire.insert(wire.end(), bytes, bytes + length);
        return IoResult{IoStatus::Ready, length};
    });
    Require(result.ok && result.bytesSent == 2 && result.framesSent == 1,
            "frame counter incremented before exact completion");
    Require(queue.QueuedBytes() == 3 && queue.RetainedBytes() == 4,
            "completed frame allocation not released exactly once");
    Require(queue.Push({10, 11, 12, 13, 14, 15}, 10), "released frame did not restore admission capacity");

    result = queue.Flush(100, [&](const std::uint8_t* bytes, std::size_t length) {
        // Force every remaining write to be short; frame order must remain exact.
        Require(length > 0, "zero-length syscall requested");
        wire.push_back(*bytes);
        return IoResult{IoStatus::Ready, 1};
    });
    Require(result.ok && result.bytesSent == 9 && result.framesSent == 2,
            "draining multiple short-written frames miscounted completion");
    const std::vector<std::uint8_t> expected{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    Require(wire == expected, "short writes changed, duplicated, omitted or reordered wire bytes");
    Require(queue.Empty() && queue.QueuedBytes() == 0 && queue.RetainedBytes() == 0,
            "drained queue retained bytes");
}

void TestPendingZeroBudgetAndClear() {
    PendingSendQueue queue;
    Require(!queue.Push({}, 10), "empty frame accepted");
    Require(!queue.Push({1, 2}, 1), "oversize initial frame accepted");
    Require(queue.Push({1, 2, 3}, 3), "fixture frame rejected");
    int calls = 0;
    auto pending = [&](const std::uint8_t*, std::size_t) {
        ++calls;
        return IoResult{IoStatus::Pending, 0};
    };
    auto result = queue.Flush(0, pending);
    Require(result.ok && result.bytesSent == 0 && result.framesSent == 0 && calls == 0,
            "zero budget invoked writer");
    result = queue.Flush(3, pending);
    Require(result.ok && result.bytesSent == 0 && result.framesSent == 0 && calls == 1,
            "would-block spun or counted progress");
    Require(queue.QueuedBytes() == 3 && queue.RetainedBytes() == 3, "would-block mutated queue");
    queue.Clear();
    Require(queue.Empty() && queue.QueuedBytes() == 0 && queue.RetainedBytes() == 0,
            "Clear leaked accounting");
    result = queue.Flush(3, pending);
    Require(result.ok && result.bytesSent == 0 && result.framesSent == 0 && calls == 1,
            "empty queue invoked writer");
}

void TestHardErrorsAndInvalidProgress() {
    for (const auto status : {IoStatus::Closed, IoStatus::Error}) {
        PendingSendQueue queue;
        Require(queue.Push({1, 2, 3}, 3), "fixture frame rejected");
        int calls = 0;
        const auto result = queue.Flush(3, [&](const std::uint8_t*, std::size_t) {
            return ++calls == 1 ? IoResult{IoStatus::Ready, 1} : IoResult{status, 0};
        });
        Require(!result.ok && calls == 2 && result.bytesSent == 1 && result.framesSent == 0,
                "hard error lost prior progress or did not stop");
        Require(queue.QueuedBytes() == 2 && queue.RetainedBytes() == 3,
                "hard error corrupted retained suffix");
        queue.Clear();
        Require(queue.Empty() && queue.QueuedBytes() == 0 && queue.RetainedBytes() == 0,
                "hard-error cleanup leaked queue accounting");
    }
    for (const auto invalid : {std::size_t{0}, std::size_t{4}}) {
        PendingSendQueue queue;
        Require(queue.Push({1, 2, 3}, 3), "fixture frame rejected");
        int calls = 0;
        const auto result = queue.Flush(3, [&](const std::uint8_t*, std::size_t) {
            ++calls;
            return IoResult{IoStatus::Ready, invalid};
        });
        Require(!result.ok && calls == 1 && result.bytesSent == 0 && result.framesSent == 0,
                "invalid writer progress spun or corrupted counters");
        Require(queue.QueuedBytes() == 3 && queue.RetainedBytes() == 3,
                "invalid writer progress changed queue");
    }
}
} // namespace

int main() {
    try {
        TestPartialWouldBlockAndFifo();
        TestPendingZeroBudgetAndClear();
        TestHardErrorsAndInvalidProgress();
        std::cout << "PASS PendingSendQueue deterministic backpressure contracts\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
