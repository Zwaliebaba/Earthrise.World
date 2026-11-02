#include "pch.h"

#include "UnitStats.h"

#include "IntegerMath.h"

namespace Simulation
{

std::int32_t Damage(const UnitStats& _attacker, const UnitStats& _target, std::int32_t _elevationSteps)
{
  const std::int64_t steps = _attacker.attackType == Protocol::DamageType::Pierce ? std::clamp(_elevationSteps, -2, 2) : 0;
  const std::int64_t attack = std::int64_t{_attacker.attack} + _attacker.bonus.at(static_cast<std::size_t>(_target.kind));
  const std::int64_t scaled = Core::FloorDivide(attack * (100 + (ELEVATION_STEP_PERCENT * steps)), 100);
  const std::int64_t armor = _target.armor.at(static_cast<std::size_t>(_attacker.attackType));
  return static_cast<std::int32_t>(std::max<std::int64_t>(1, scaled - armor));
}

} // namespace Simulation
