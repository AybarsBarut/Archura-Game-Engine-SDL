#pragma once

#include <cstdint>
#include <functional>

namespace Archura {

// Clock-, SDL-, and renderer-independent policy shared by the real application
// and deterministic tests. Callers inject elapsed seconds from a monotonic clock.
class GameLoopHarness {
public:
  static constexpr double TickInterval = 1.0 / 128.0;
  static constexpr double MaxFrameElapsed = 0.25;
  static constexpr unsigned MaxTicksPerFrame = 5;
  // The graphical client drops excess whole ticks. The server can preserve
  // existing catch-up debt while still executing at most five ticks per call.
  enum class OverloadPolicy { DropWholeTicks, PreserveBacklog };
  explicit GameLoopHarness(OverloadPolicy policy = OverloadPolicy::DropWholeTicks)
      : m_OverloadPolicy(policy) {}

  struct Callbacks {
    std::function<bool()> shouldContinue;
    std::function<void()> input;
    std::function<void(double)> fixedTick;
    std::function<void(double)> render;
    std::function<void()> present;
    std::function<void()> endFrame;
  };

  // Returns true when presentation completed. Quit skips subsequent phases;
  // endFrame runs once after input starts, even on quit or an exception.
  // Callback exceptions propagate; cleanup cannot replace an active exception.
  bool RunFrame(double elapsedSeconds, const Callbacks& callbacks);
  void Reset() noexcept;
  void ResetAccumulator() noexcept { m_Accumulator = 0.0; }
  double AccumulatorSeconds() const noexcept { return m_Accumulator; }
  std::uint64_t TotalTicks() const noexcept { return m_TotalTicks; }
  // Whole ticks discarded after hitting the per-frame execution cap.
  std::uint64_t DroppedTicks() const noexcept { return m_DroppedTicks; }
  // Includes both frame-elapsed clamping and discarded whole backlog ticks.
  double DroppedSeconds() const noexcept { return m_DroppedSeconds; }

private:
  OverloadPolicy m_OverloadPolicy;
  double m_Accumulator = 0.0;
  double m_DroppedSeconds = 0.0;
  std::uint64_t m_TotalTicks = 0;
  std::uint64_t m_DroppedTicks = 0;
};

} // namespace Archura
