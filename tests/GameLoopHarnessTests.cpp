#include "core/GameLoopHarness.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cerr << __LINE__ << ": " << #c << '\n'; ++failures; } } while(false)
bool Near(double a, double b, double tolerance = 1e-9) { return std::abs(a-b) <= tolerance; }
using Loop = Archura::GameLoopHarness;

struct Probe {
  Loop loop;
  Loop::Callbacks callbacks;
  bool running = true;
  unsigned ticks = 0, frames = 0, presents = 0, ends = 0;
  double alpha = -1;
  std::string order;
  Probe() {
    callbacks.shouldContinue = [this] { return running; };
    callbacks.input = [this] { order += 'I'; };
    callbacks.fixedTick = [this](double dt) { CHECK(dt == Loop::TickInterval); ++ticks; order += 'T'; };
    callbacks.render = [this](double a) { alpha = a; ++frames; order += 'R'; CHECK(a >= 0 && a < 1); };
    callbacks.present = [this] { ++presents; order += 'P'; };
    callbacks.endFrame = [this] { ++ends; order += 'E'; };
  }
};

void Phases() {
  Probe p;
  CHECK(p.loop.RunFrame(0, p.callbacks));
  CHECK(p.order == "IRPE");
  CHECK(p.alpha == 0);
  p.order.clear();
  CHECK(p.loop.RunFrame(Loop::TickInterval * 3.5, p.callbacks));
  CHECK(p.order == "ITTTRPE");
  CHECK(p.loop.TotalTicks() == 3);
  CHECK(Near(p.alpha, 0.5));
  CHECK(p.loop.DroppedTicks() == 0);
  p.loop.ResetAccumulator();
  CHECK(p.loop.AccumulatorSeconds() == 0);
  CHECK(p.loop.TotalTicks() == 3);
}

void StallsAndInvalidElapsed() {
  Probe p;
  p.loop.RunFrame(Loop::TickInterval * 0.5, p.callbacks);
  p.loop.RunFrame(0.25, p.callbacks);
  CHECK(p.ticks == 5);
  CHECK(p.loop.DroppedTicks() == 27);
  CHECK(Near(p.loop.DroppedSeconds(), 27 * Loop::TickInterval));
  CHECK(Near(p.alpha, 0.5));
  p.loop.RunFrame(0, p.callbacks);
  CHECK(p.ticks == 5); // Whole backlog was discarded, fractional phase retained.
  p.loop.RunFrame(Loop::TickInterval * 0.5, p.callbacks);
  CHECK(p.ticks == 6);
  p.loop.Reset();
  CHECK(p.loop.TotalTicks() == 0 && p.loop.DroppedTicks() == 0);
  CHECK(p.loop.DroppedSeconds() == 0);
  p.loop.RunFrame(2.0, p.callbacks);
  CHECK(p.loop.TotalTicks() == 5);
  CHECK(p.loop.DroppedTicks() == 27);
  CHECK(Near(p.loop.DroppedSeconds(), 2.0 - 5 * Loop::TickInterval));
  const auto ticks = p.ticks;
  for (double elapsed : {-1.0, std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
    p.loop.RunFrame(elapsed, p.callbacks);
    CHECK(p.ticks == ticks);
    CHECK(std::isfinite(p.alpha));
  }
}

void FrameRatesAndUptime() {
  for (int fps : {30, 60, 90, 120, 128, 144, 165, 240}) {
    Probe p;
    for (int i = 0; i < fps * 60; ++i) p.loop.RunFrame(1.0 / fps, p.callbacks);
    // Floating addition can leave a fraction within machine epsilon of a tick.
    const auto ticks = p.loop.TotalTicks();
    CHECK(ticks >= 60 * 128 - 1 && ticks <= 60 * 128);
    CHECK(Near(ticks * Loop::TickInterval + p.loop.AccumulatorSeconds(), 60.0));
    CHECK(p.loop.DroppedTicks() == 0);
    CHECK(p.frames == static_cast<unsigned>(fps * 60));
  }
  Probe longRun;
  double previous = 60.0 * 60 * 24 * 365; // One year uptime, no float clock quantization.
  for (int i = 0; i < 24000; ++i) {
    const double now = previous + 1.0 / 240;
    longRun.loop.RunFrame(now - previous, longRun.callbacks);
    previous = now;
  }
  CHECK(longRun.loop.TotalTicks() >= 12799 && longRun.loop.TotalTicks() <= 12800);
  CHECK(longRun.loop.DroppedTicks() == 0);
}

void QuitPhases() {
  for (char stop : std::string("ITRP")) {
    Probe p;
    if (stop == 'I') p.callbacks.input = [&] { p.order += 'I'; p.running = false; };
    if (stop == 'T') p.callbacks.fixedTick = [&](double) { p.order += 'T'; p.running = false; };
    if (stop == 'R') p.callbacks.render = [&](double) { p.order += 'R'; p.running = false; };
    if (stop == 'P') p.callbacks.present = [&] { p.order += 'P'; p.running = false; };
    const bool presented = p.loop.RunFrame(Loop::TickInterval * 4, p.callbacks);
    const std::string expected = stop == 'I' ? "IE" : stop == 'T' ? "ITE" : stop == 'R' ? "ITTTTRE" : "ITTTTRPE";
    CHECK(p.order == expected);
    CHECK(presented == (stop == 'P'));
    CHECK(p.ends == 1);
    CHECK(!p.loop.RunFrame(0, p.callbacks));
    CHECK(p.ends == 1);
  }
}

void ExceptionPhases() {
  for (char phase : std::string("ITRPE")) {
    Probe p;
    const auto fail = [] { throw std::runtime_error("original"); };
    if (phase == 'I') p.callbacks.input = fail;
    if (phase == 'T') p.callbacks.fixedTick = [&](double) { fail(); };
    if (phase == 'R') p.callbacks.render = [&](double) { fail(); };
    if (phase == 'P') p.callbacks.present = fail;
    if (phase == 'E') p.callbacks.endFrame = [&] { ++p.ends; fail(); };
    bool caught = false;
    try { p.loop.RunFrame(Loop::TickInterval, p.callbacks); }
    catch (const std::runtime_error& e) { caught = std::string(e.what()) == "original"; }
    CHECK(caught);
    CHECK(p.ends == 1);
    if (phase == 'I' || phase == 'T' || phase == 'R') CHECK(p.presents == 0);
  }
  Probe p;
  p.callbacks.input = [] { throw std::runtime_error("original"); };
  p.callbacks.endFrame = [] { throw std::runtime_error("cleanup"); };
  try { p.loop.RunFrame(0, p.callbacks); CHECK(false); }
  catch (const std::runtime_error& e) { CHECK(std::string(e.what()) == "original"); }
}

void PreservedServerBacklog() {
  Loop loop(Loop::OverloadPolicy::PreserveBacklog);
  unsigned ticks = 0;
  double alpha = 0;
  Loop::Callbacks c;
  c.fixedTick = [&](double) { ++ticks; };
  c.render = [&](double a) { alpha = a; };
  loop.RunFrame(0.25, c);
  CHECK(ticks == 5);
  CHECK(Near(loop.AccumulatorSeconds(), 27 * Loop::TickInterval));
  CHECK(loop.DroppedTicks() == 0 && loop.DroppedSeconds() == 0);
  CHECK(alpha >= 0 && alpha < 1);
  for (int i = 0; i < 6; ++i) loop.RunFrame(0, c);
  CHECK(ticks == 32);
  CHECK(loop.AccumulatorSeconds() == 0);
}

void ResetDuringTick() {
  Probe p;
  p.callbacks.fixedTick = [&](double) { ++p.ticks; p.loop.ResetAccumulator(); };
  p.loop.RunFrame(Loop::TickInterval * 4.5, p.callbacks);
  CHECK(p.ticks == 1);
  CHECK(p.loop.TotalTicks() == 1);
  CHECK(p.loop.AccumulatorSeconds() == 0);
  CHECK(p.alpha == 0);
  p.loop.RunFrame(Loop::TickInterval * 0.5, p.callbacks);
  CHECK(p.ticks == 1);
  CHECK(Near(p.alpha, 0.5));
}
} // namespace

int main() {
  Phases(); StallsAndInvalidElapsed(); FrameRatesAndUptime(); QuitPhases();
  ExceptionPhases(); PreservedServerBacklog(); ResetDuringTick();
  if (failures) return 1;
  std::cout << "GameLoopHarness tests passed\n";
}
