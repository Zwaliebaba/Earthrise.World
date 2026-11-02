#include "pch.h"

#include "FireTally.h"

namespace Client
{

void FireTally::Observe(const Protocol::StateUpdate& _state)
{
  for (const Protocol::EngineView& engine : _state.engines)
  {
    const auto last = m_reloadTicks.find(engine.id);
    if (last != m_reloadTicks.end() && engine.reloadTicks > last->second)
    {
      m_firedTick.insert_or_assign(engine.id, _state.tick);
      ++m_fired[engine.owner];
    }
    m_reloadTicks.insert_or_assign(engine.id, engine.reloadTicks);
  }
}

std::uint32_t FireTally::Fired(std::uint8_t _owner) const noexcept
{
  const auto found = m_fired.find(_owner);
  return found == m_fired.end() ? 0 : found->second;
}

std::optional<std::uint32_t> FireTally::LastFiredTick(Protocol::EntityId _engine) const noexcept
{
  const auto found = m_firedTick.find(_engine);
  if (found == m_firedTick.end())
  {
    return std::nullopt;
  }
  return found->second;
}

} // namespace Client
