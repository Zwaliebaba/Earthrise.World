#pragma once

#include <chrono>

namespace Engine
{

/// Wall-clock time between frames, for presentation only. The simulation keeps its own clock in
/// ticks and never reads this one (R16, ADR-002 decision 6).
class FrameTimer
{
public:
  FrameTimer() noexcept;

  /// Seconds since the previous lap, or since construction for the first.
  [[nodiscard]] double LapSeconds() noexcept;

  /// Seconds since construction.
  [[nodiscard]] double TotalSeconds() const noexcept;

private:
  std::chrono::steady_clock::time_point m_start;
  std::chrono::steady_clock::time_point m_lastLap;
};

} // namespace Engine
