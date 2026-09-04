#include "core/FrameTelemetry.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

int g_Failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "          \
                << #condition << '\n';                                         \
      ++g_Failures;                                                            \
    }                                                                          \
  } while (false)

bool Near(double actual, double expected) {
  return std::fabs(actual - expected) <= 0.000000001;
}

void TestEmptyTelemetry() {
  Archura::FrameTelemetry telemetry;
  const auto summary = telemetry.Snapshot();
  CHECK(!summary.available);
  CHECK(summary.observedFrames == 0);
  CHECK(summary.windowSamples == 0);
  CHECK(Near(summary.averageCpuWorkMs, 0.0));
  CHECK(Near(summary.p95CpuWorkMs, 0.0));
  CHECK(Near(summary.p99CpuWorkMs, 0.0));
}

void TestKnownPercentilesAndAverage() {
  Archura::FrameTelemetry telemetry;
  for (int sample = 1; sample <= 100; ++sample)
    CHECK(telemetry.RecordCpuWorkMilliseconds(static_cast<double>(sample)));

  const auto summary = telemetry.Snapshot();
  CHECK(summary.available);
  CHECK(summary.observedFrames == 100);
  CHECK(summary.windowSamples == 100);
  CHECK(Near(summary.averageCpuWorkMs, 50.5));
  CHECK(Near(summary.p95CpuWorkMs, 95.0));
  CHECK(Near(summary.p99CpuWorkMs, 99.0));
}

void TestUnsortedDuplicates() {
  Archura::FrameTelemetry telemetry;
  const double samples[] = {5.0, 1.0, 5.0, 2.0};
  for (const double sample : samples)
    CHECK(telemetry.RecordCpuWorkMilliseconds(sample));

  const auto summary = telemetry.Snapshot();
  CHECK(Near(summary.averageCpuWorkMs, 3.25));
  CHECK(Near(summary.p95CpuWorkMs, 5.0));
  CHECK(Near(summary.p99CpuWorkMs, 5.0));
}

void TestRingKeepsNewest512Samples() {
  Archura::FrameTelemetry telemetry;
  for (int sample = 1; sample <= 600; ++sample)
    CHECK(telemetry.RecordCpuWorkMilliseconds(static_cast<double>(sample)));

  const auto summary = telemetry.Snapshot();
  CHECK(summary.available);
  CHECK(summary.observedFrames == 600);
  CHECK(summary.windowSamples == Archura::FrameTelemetry::Capacity);
  CHECK(Near(summary.averageCpuWorkMs, 344.5));
  CHECK(Near(summary.p95CpuWorkMs, 575.0));
  CHECK(Near(summary.p99CpuWorkMs, 595.0));
}

void TestRejectsInvalidSamples() {
  Archura::FrameTelemetry telemetry;
  CHECK(!telemetry.RecordCpuWorkMilliseconds(-0.001));
  CHECK(!telemetry.RecordCpuWorkMilliseconds(
      std::numeric_limits<double>::quiet_NaN()));
  CHECK(!telemetry.RecordCpuWorkMilliseconds(
      std::numeric_limits<double>::infinity()));
  CHECK(!telemetry.RecordCpuWorkMilliseconds(
      -std::numeric_limits<double>::infinity()));
  CHECK(!telemetry.Snapshot().available);

  CHECK(telemetry.RecordCpuWorkMilliseconds(0.0));
  const auto summary = telemetry.Snapshot();
  CHECK(summary.available);
  CHECK(summary.observedFrames == 1);
  CHECK(summary.windowSamples == 1);

  Archura::FrameTelemetry finiteTelemetry;
  CHECK(finiteTelemetry.RecordCpuWorkMilliseconds(
      std::numeric_limits<double>::max()));
  CHECK(std::isfinite(finiteTelemetry.Snapshot().averageCpuWorkMs));
}

void TestReset() {
  Archura::FrameTelemetry telemetry;
  CHECK(telemetry.RecordCpuWorkMilliseconds(4.0));
  CHECK(telemetry.RecordCpuWorkMilliseconds(8.0));
  telemetry.Reset();

  const auto empty = telemetry.Snapshot();
  CHECK(!empty.available);
  CHECK(empty.observedFrames == 0);
  CHECK(empty.windowSamples == 0);

  CHECK(telemetry.RecordCpuWorkMilliseconds(3.0));
  const auto resetSummary = telemetry.Snapshot();
  CHECK(resetSummary.observedFrames == 1);
  CHECK(resetSummary.windowSamples == 1);
  CHECK(Near(resetSummary.averageCpuWorkMs, 3.0));
}

void TestTruthfulFormattingAndSceneCounters() {
  Archura::FrameTelemetry telemetry;
  const std::string unavailable = Archura::FormatFrameTelemetryReport(
      telemetry.Snapshot(), Archura::SceneRenderCounters{});
  CHECK(unavailable.find("CPU work samples: unavailable") != std::string::npos);
  CHECK(unavailable.find("GPU time: unavailable") != std::string::npos);
  CHECK(unavailable.find("Total draw calls: unavailable") != std::string::npos);
  CHECK(unavailable.find("Triangles: unavailable") != std::string::npos);
  CHECK(unavailable.find("VRAM usage: unavailable") != std::string::npos);

  CHECK(telemetry.RecordCpuWorkMilliseconds(2.5));
  Archura::SceneRenderCounters counters;
  counters.available = true;
  counters.visibleInstances = 42;
  counters.culledEntities = 7;
  counters.mainBatchSubmissions = 4;
  counters.shadowBatchSubmissions = 3;
  const std::string report =
      Archura::FormatFrameTelemetryReport(telemetry.Snapshot(), counters);

  CHECK(report.find("ProcessInput through Renderer::EndFrame") !=
        std::string::npos);
  CHECK(report.find("before swap/vsync and FPS limiter") != std::string::npos);
  CHECK(report.find("Visible instances accepted into batches: 42") !=
        std::string::npos);
  CHECK(report.find("Culled entities (distance rejects): 7") !=
        std::string::npos);
  CHECK(report.find("Main scoped batch submissions (DrawInstanced): 4") !=
        std::string::npos);
  CHECK(report.find("Shadow scoped batch submissions (DrawInstanced): 3") !=
        std::string::npos);

  const char* fabricatedValues[] = {
      "Draw Calls: 1234", "5,234,567", "2048 MB", "45 shader programs"};
  for (const char* fabricated : fabricatedValues)
    CHECK(report.find(fabricated) == std::string::npos);
}

} // namespace

int main() {
  TestEmptyTelemetry();
  TestKnownPercentilesAndAverage();
  TestUnsortedDuplicates();
  TestRingKeepsNewest512Samples();
  TestRejectsInvalidSamples();
  TestReset();
  TestTruthfulFormattingAndSceneCounters();

  if (g_Failures != 0) {
    std::cerr << g_Failures << " frame telemetry check(s) failed\n";
    return 1;
  }

  std::cout << "Frame telemetry tests passed\n";
  return 0;
}
