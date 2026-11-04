#include "pch.h"

#include "ReasonPrompt.h"

#include "Strings.h"

namespace Client
{

ReasonPrompt::ReasonPrompt(bool _enabled)
  : m_enabled(_enabled)
{
}

void ReasonPrompt::Raise(std::uint32_t _tick, SiegeWork _work, std::string_view _structure, Protocol::VoxelPosition _voxel, double _seconds)
{
  const auto kind = static_cast<std::size_t>(_work);
  if (!m_enabled || kind >= WORKS || m_raised.at(kind))
  {
    return;
  }
  m_raised.at(kind) = true;
  m_waiting.push_back({.tick = _tick, .work = _work, .structure = std::string(_structure), .voxel = _voxel, .raisedSeconds = _seconds});
}

bool ReasonPrompt::Up() const noexcept
{
  return !m_waiting.empty();
}

std::optional<HudMode> ReasonPrompt::Banner() const
{
  if (m_waiting.empty())
  {
    return std::nullopt;
  }
  return HudMode{.command = "ASK", .banner = std::format(REASON_PROMPT_BANNER, m_waiting.front().structure), .buttons = {}};
}

std::optional<Protocol::VoxelPosition> ReasonPrompt::Mark() const noexcept
{
  return m_waiting.empty() ? std::nullopt : std::optional(m_waiting.front().voxel);
}

std::optional<ReasonAnswered> ReasonPrompt::GoOn(double _seconds)
{
  if (m_waiting.empty())
  {
    return std::nullopt;
  }
  ReasonAsk ask = std::move(m_waiting.front());
  m_waiting.pop_front();
  if (!m_waiting.empty())
  {
    m_waiting.front().raisedSeconds = _seconds; // the next one up is up from now
  }
  const double secondsUp = std::max(0.0, _seconds - ask.raisedSeconds);
  return ReasonAnswered{.ask = std::move(ask), .secondsUp = secondsUp};
}

} // namespace Client
