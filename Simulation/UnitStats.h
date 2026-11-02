#pragma once

#include "EntityKinds.h"
#include "Fixed.h"

#include <array>
#include <cstdint>

namespace Simulation
{

using Protocol::NavigationClass;

/// A unit kind's statistics, ServerData's UNIT section, from GameData/Server/Units.json (ADR-023, decision 1), each
/// field named for its unit as the bake converts it.
struct UnitStats
{
  Protocol::UnitKind kind;
  NavigationClass navigationClass;
  std::uint8_t squadFigures; // 6 foot, 3 mounted, 1 laborers and engines
  std::int32_t hitPoints;
  std::array<std::int32_t, 3> armor; // by Protocol::DamageType
  Protocol::DamageType attackType;
  std::int32_t attack;       // 0 for kinds with no attack on units
  std::uint32_t reloadTicks; // 0 for the engines: RULE owns the trebuchet's and the ram's (ADR-023, decision 1)
  std::int32_t rangeVoxels;  // 0: melee, the eight neighbouring cells; 0 for the engines, as reloadTicks
  std::array<std::int32_t, Protocol::UNIT_KIND_COUNT> bonus; // added to the attack against each kind
  std::int32_t visionVoxels;
  Core::Fixed speedMetersPerTick;    // replaces RULE's class speeds; RULE keeps footSpeedMetersPerTick for the warning
  Core::Fixed digCubicMetersPerTick; // sappers and laborers; 0 otherwise
};

/// Each elevation step adds this share of the attack to a pierce hit, and takes it away shooting uphill (ADR-023,
/// decision 2: the formula's own 15).
inline constexpr std::int32_t ELEVATION_STEP_PERCENT = 15;

/// ADR-023, decision 2: max(1, floor((attack + bonus) * (100 + 15 * steps) / 100) - armor), where armor is _target's
/// against _attacker's attack type and steps, -2 to 2, count for pierce alone.
[[nodiscard]] std::int32_t Damage(const UnitStats& _attacker, const UnitStats& _target, std::int32_t _elevationSteps);

} // namespace Simulation
