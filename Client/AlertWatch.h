#pragma once

#include "EntityId.h"
#include "HudAlerts.h"
#include "HudModel.h"
#include "Session.h"
#include "StateUpdate.h"

#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>

namespace Client
{

/// M4's alerts (Hud.md §10; M4-I6), raised from each update the session receives, once a tick, beside the collapse's,
/// which Overlays raises:
/// - SAPPERS AT … the first time each enemy squad of sappers is seen, at its first figure, named for the structure
///   nearest it;
/// - BREACH OPEN AT … the first time a structure reads opened, at its foot;
/// - HIT … · REPAIR LOCKED n S when the player's shot lands on an enemy structure whose repair it locks, replaced where
///   it stands by the next hit on the same structure;
/// - NOT ENOUGH STONE and NOT ENOUGH TIMBER on a refusal for want of either, replaced where it stands by the next, so a
///   command refused tick after tick gives one alert.
/// None is urgent; each but the refusals has a place, for JUMP and Space.
class AlertWatch
{
public:
  /// Reads the session's newest update at _nowSeconds, unless it read that tick before.
  void Observe(const Session& _session, double _nowSeconds, HudAlerts& _alerts);

  /// Space's place: the newest shown alert's that has one.
  [[nodiscard]] static std::optional<Protocol::VoxelPosition> NewestPlace(std::span<const HudAlert> _alerts);

  /// The name of the structure whose box is nearest _voxels, or nothing in an update with no named structure.
  [[nodiscard]] static std::optional<std::string> NearestName(const Protocol::StateUpdate& _state, const Protocol::VoxelPosition& _voxels);

private:
  std::optional<std::uint32_t> m_tick;
  std::set<Protocol::EntityId> m_sappersSeen;
  std::set<Protocol::EntityId> m_opened;
};

} // namespace Client
