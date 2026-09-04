#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Archura {

struct FrameTelemetrySummary {
    bool available = false;
    std::uint64_t observedFrames = 0;
    std::size_t windowSamples = 0;
    double averageCpuWorkMs = 0.0;
    double p95CpuWorkMs = 0.0;
    double p99CpuWorkMs = 0.0;
};

struct SceneRenderCounters {
    bool available = false;
    std::uint64_t visibleInstances = 0;
    std::uint64_t culledEntities = 0;
    std::uint64_t mainBatchSubmissions = 0;
    std::uint64_t shadowBatchSubmissions = 0;
};

class FrameTelemetry final {
public:
    static constexpr std::size_t Capacity = 512;

    // Recording is allocation-free and O(1). Invalid samples are rejected
    // without changing the ring or observed-frame count.
    bool RecordCpuWorkMilliseconds(double milliseconds) noexcept;
    FrameTelemetrySummary Snapshot() const;
    void Reset() noexcept;

private:
    std::array<double, Capacity> m_CpuWorkMilliseconds{};
    std::size_t m_NextSample = 0;
    std::size_t m_SampleCount = 0;
    std::uint64_t m_ObservedFrames = 0;
};

std::string FormatFrameTelemetryReport(const FrameTelemetrySummary& summary,
                                       const SceneRenderCounters& counters);

} // namespace Archura
