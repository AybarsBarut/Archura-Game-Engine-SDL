#include "GameLoopHarness.h"

#include <cmath>

namespace Archura {

bool GameLoopHarness::RunFrame(double elapsedSeconds, const Callbacks& callbacks) {
  const auto running = [&]() {
    return !callbacks.shouldContinue || callbacks.shouldContinue();
  };
  if (!running()) return false;

  if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
    elapsedSeconds = 0.0;
  if (elapsedSeconds > MaxFrameElapsed) {
    m_DroppedSeconds += elapsedSeconds - MaxFrameElapsed;
    elapsedSeconds = MaxFrameElapsed;
  }
  m_Accumulator += elapsedSeconds;

  bool presented = false;
  try {
    if (callbacks.input) callbacks.input();
    if (running()) {
      unsigned ticks = 0;
      while (m_Accumulator >= TickInterval && ticks < MaxTicksPerFrame && running()) {
        // Consume before invoking user code: Play/Stop can reset the remainder
        // from inside a tick. Subtracting afterwards would create negative alpha.
        m_Accumulator -= TickInterval;
        if (callbacks.fixedTick) callbacks.fixedTick(TickInterval);
        ++m_TotalTicks;
        ++ticks;
      }
      if (m_OverloadPolicy == OverloadPolicy::DropWholeTicks &&
          ticks == MaxTicksPerFrame && m_Accumulator >= TickInterval) {
        const auto dropped = static_cast<std::uint64_t>(m_Accumulator / TickInterval);
        m_Accumulator -= static_cast<double>(dropped) * TickInterval;
        m_DroppedTicks += dropped;
        m_DroppedSeconds += static_cast<double>(dropped) * TickInterval;
      }
      if (running()) {
        const double alpha = m_Accumulator < TickInterval
            ? m_Accumulator / TickInterval : std::nextafter(1.0, 0.0);
        if (callbacks.render) callbacks.render(alpha);
        if (running()) {
          if (callbacks.present) callbacks.present();
          presented = true;
        }
      }
    }
  } catch (...) {
    try { if (callbacks.endFrame) callbacks.endFrame(); } catch (...) {}
    throw;
  }
  if (callbacks.endFrame) callbacks.endFrame();
  return presented;
}

void GameLoopHarness::Reset() noexcept {
  m_Accumulator = 0.0;
  m_DroppedSeconds = 0.0;
  m_TotalTicks = 0;
  m_DroppedTicks = 0;
}

} // namespace Archura
