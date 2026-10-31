#include "pch.h"

#include "FrameTimer.h"

namespace Engine
{

FrameTimer::FrameTimer() noexcept
  : m_start(std::chrono::steady_clock::now()),
    m_lastLap(m_start)
{
}

double FrameTimer::LapSeconds() noexcept
{
  const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  const std::chrono::duration<double> lap = now - m_lastLap;
  m_lastLap = now;
  return lap.count();
}

double FrameTimer::TotalSeconds() const noexcept
{
  const std::chrono::duration<double> total = std::chrono::steady_clock::now() - m_start;
  return total.count();
}

} // namespace Engine
