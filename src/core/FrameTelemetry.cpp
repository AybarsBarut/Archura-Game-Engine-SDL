#include "FrameTelemetry.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace Archura {

bool FrameTelemetry::RecordCpuWorkMilliseconds(double milliseconds) noexcept {
    if (!std::isfinite(milliseconds) || milliseconds < 0.0)
        return false;

    m_CpuWorkMilliseconds[m_NextSample] = milliseconds;
    m_NextSample = (m_NextSample + 1) % Capacity;
    if (m_SampleCount < Capacity)
        ++m_SampleCount;
    if (m_ObservedFrames < std::numeric_limits<std::uint64_t>::max())
        ++m_ObservedFrames;
    return true;
}

FrameTelemetrySummary FrameTelemetry::Snapshot() const {
    FrameTelemetrySummary summary;
    summary.observedFrames = m_ObservedFrames;
    summary.windowSamples = m_SampleCount;
    if (m_SampleCount == 0)
        return summary;

    std::array<double, Capacity> sorted{};
    double average = 0.0;
    for (std::size_t i = 0; i < m_SampleCount; ++i) {
        sorted[i] = m_CpuWorkMilliseconds[i];
        average += (sorted[i] - average) / static_cast<double>(i + 1);
    }
    std::sort(sorted.begin(), sorted.begin() + m_SampleCount);

    const std::size_t p95Rank = (95 * m_SampleCount + 99) / 100;
    const std::size_t p99Rank = (99 * m_SampleCount + 99) / 100;

    summary.available = true;
    summary.averageCpuWorkMs = average;
    summary.p95CpuWorkMs = sorted[p95Rank - 1];
    summary.p99CpuWorkMs = sorted[p99Rank - 1];
    return summary;
}

void FrameTelemetry::Reset() noexcept {
    m_NextSample = 0;
    m_SampleCount = 0;
    m_ObservedFrames = 0;
}

std::string FormatFrameTelemetryReport(const FrameTelemetrySummary& summary,
                                       const SceneRenderCounters& counters) {
    std::ostringstream report;
    report << "=== Frame Telemetry ===\n";
    report << "CPU work scope: ProcessInput through Renderer::EndFrame "
              "(before swap/vsync and FPS limiter)\n";
    if (summary.available) {
        report << "CPU work observed frames: " << summary.observedFrames << '\n';
        report << "CPU work window samples: " << summary.windowSamples << " / "
               << FrameTelemetry::Capacity << '\n';
        report << std::fixed << std::setprecision(3);
        report << "CPU work average: " << summary.averageCpuWorkMs << " ms\n";
        report << "CPU work P95 (nearest-rank): " << summary.p95CpuWorkMs
               << " ms\n";
        report << "CPU work P99 (nearest-rank): " << summary.p99CpuWorkMs
               << " ms\n";
    } else {
        report << "CPU work samples: unavailable\n";
    }

    report << "GPU time: unavailable\n";
    if (counters.available) {
        report << "Visible instances accepted into batches: "
               << counters.visibleInstances << '\n';
        report << "Culled entities (distance rejects): "
               << counters.culledEntities << '\n';
        report << "Main scoped batch submissions (DrawInstanced): "
               << counters.mainBatchSubmissions << '\n';
        report << "Shadow scoped batch submissions (DrawInstanced): "
               << counters.shadowBatchSubmissions << '\n';
    } else {
        report << "Scene render counters: unavailable\n";
    }
    report << "Total draw calls: unavailable\n";
    report << "Triangles: unavailable\n";
    report << "VRAM usage: unavailable\n";
    report << "=======================\n";
    return report.str();
}

} // namespace Archura
