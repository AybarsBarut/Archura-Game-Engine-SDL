#pragma once
#include "NativeSocket.h"
#include <algorithm>
#include <deque>
#include <utility>
#include <vector>
namespace Archura::Net {
// Retained storage, including already sent prefixes, remains charged until a
// complete frame is removed. QueuedBytes reports only outstanding wire bytes.
class PendingSendQueue {
public:
    struct FlushResult { bool ok = true; std::size_t bytesSent = 0; std::size_t framesSent = 0; };
    bool Push(std::vector<std::uint8_t> frame, std::size_t limit) {
        if (frame.empty() || retained_ > limit || frame.capacity() > limit - retained_) return false;
        const auto size = frame.size();
        const auto storage = frame.capacity();
        writes_.push_back({std::move(frame), 0});
        retained_ += storage; queued_ += size; return true;
    }
    std::size_t QueuedBytes() const noexcept { return queued_; }
    std::size_t RetainedBytes() const noexcept { return retained_; }
    bool Empty() const noexcept { return writes_.empty(); }
    void Clear() noexcept { writes_.clear(); queued_ = retained_ = 0; }
    template<class Writer> FlushResult Flush(std::size_t budget, Writer writer) {
        FlushResult result;
        while (budget && !writes_.empty()) {
            auto& write = writes_.front();
            const auto amount = std::min(budget, write.bytes.size() - write.offset);
            const auto sent = writer(write.bytes.data() + write.offset, amount);
            if (sent.status == IoStatus::Pending) break;
            if (sent.status != IoStatus::Ready || sent.bytes == 0 || sent.bytes > amount) { result.ok = false; break; }
            write.offset += sent.bytes; queued_ -= sent.bytes; budget -= sent.bytes;
            result.bytesSent += sent.bytes;
            if (write.offset == write.bytes.size()) { retained_ -= write.bytes.capacity(); writes_.pop_front(); ++result.framesSent; }
        }
        return result;
    }
private:
    struct PendingWrite { std::vector<std::uint8_t> bytes; std::size_t offset; };
    std::deque<PendingWrite> writes_;
    std::size_t queued_ = 0, retained_ = 0;
};
}
