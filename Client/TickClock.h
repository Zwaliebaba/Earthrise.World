#pragma once

#include <cstdint>

namespace Client
{

/// The client's clock (ADR-020, decision 4): a fixed-step accumulator, fed each frame by the executable with the wall
/// seconds since the last and the speed, which gives the ticks due. At most MAX_TICKS_PER_FRAME come out of one frame,
/// and what is left waits, so a slow machine slows game time and never skips a tick (ADR-013, decision 2). Time is
/// kept in whole nanoseconds of game time, so that frames of a third of a tick add up to whole ticks exactly.
class TickClock
{
public:
  static constexpr std::uint32_t TICKS_PER_SECOND = 20;
  static constexpr std::uint32_t MAX_TICKS_PER_FRAME = 8;

  /// Adds _seconds times _speed to the accumulator and returns the ticks due, at most MAX_TICKS_PER_FRAME; the rest
  /// waits. A paused clock takes no time and gives no ticks.
  [[nodiscard]] std::uint32_t Advance(double _seconds, double _speed);

  void SetPaused(bool _paused) noexcept;
  [[nodiscard]] bool Paused() const noexcept;

  /// The accumulator's part of the next tick, 0 to below 1: where the shot is drawn between its last two positions
  /// (GameDesign.md, M3's values, item 17).
  [[nodiscard]] double TickFraction() const noexcept;

private:
  std::int64_t m_nanoseconds = 0; // game time not yet given out as ticks
  bool m_paused = false;
};

} // namespace Client
