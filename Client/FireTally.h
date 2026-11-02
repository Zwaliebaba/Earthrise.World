#pragma once

#include "EntityId.h"
#include "StateUpdate.h"

#include <cstdint>
#include <map>
#include <optional>

namespace Client
{

/// The shots each side's engines have fired, read from the state updates tick by tick: an engine fired in the tick its
/// reload rose. The HUD counts them as SHOTS n (M3-E2), and the trebuchet is drawn released after one (M3-C3).
class FireTally
{
public:
  /// Reads one tick's update. Call it for every tick, in order: a tick missed may hide a shot.
  void Observe(const Protocol::StateUpdate& _state);

  /// The shots _owner's engines have fired since the first update observed.
  [[nodiscard]] std::uint32_t Fired(std::uint8_t _owner) const noexcept;

  /// The tick _engine last fired in, or nothing when it has not fired since the first update observed.
  [[nodiscard]] std::optional<std::uint32_t> LastFiredTick(Protocol::EntityId _engine) const noexcept;

private:
  std::map<Protocol::EntityId, std::uint32_t> m_reloadTicks;
  std::map<Protocol::EntityId, std::uint32_t> m_firedTick;
  std::map<std::uint8_t, std::uint32_t> m_fired;
};

} // namespace Client
