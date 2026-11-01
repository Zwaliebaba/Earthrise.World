#include "pch.h"

#include "TickClock.h"

namespace Client
{

namespace
{

constexpr std::int64_t NANOSECONDS_PER_SECOND = 1'000'000'000;
constexpr std::int64_t NANOSECONDS_PER_TICK = NANOSECONDS_PER_SECOND / TickClock::TICKS_PER_SECOND;

} // namespace

std::uint32_t TickClock::Advance(double _seconds, double _speed)
{
  if (m_paused)
  {
    return 0;
  }
  if (!(_seconds >= 0.0) || !(_speed >= 0.0))
  {
    throw std::invalid_argument("the clock runs forwards: a frame's seconds and the speed are not negative");
  }
  m_nanoseconds += std::llround(_seconds * _speed * static_cast<double>(NANOSECONDS_PER_SECOND));
  const std::int64_t due = std::min<std::int64_t>(m_nanoseconds / NANOSECONDS_PER_TICK, MAX_TICKS_PER_FRAME);
  m_nanoseconds -= due * NANOSECONDS_PER_TICK;
  return static_cast<std::uint32_t>(due);
}

void TickClock::SetPaused(bool _paused) noexcept
{
  m_paused = _paused;
}

bool TickClock::Paused() const noexcept
{
  return m_paused;
}

double TickClock::TickFraction() const noexcept
{
  return static_cast<double>(m_nanoseconds % NANOSECONDS_PER_TICK) / static_cast<double>(NANOSECONDS_PER_TICK);
}

} // namespace Client
