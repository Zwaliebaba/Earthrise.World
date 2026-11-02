#include "pch.h"

#include "HealthStructureRule.h"

#include "World.h"

namespace Simulation
{

const HealthStructureRule& HealthStructureRule::Instance() noexcept
{
  static const HealthStructureRule RULE;
  return RULE;
}

void HealthStructureRule::ShotStrikes(World& _world, const ShotHit& _hit) const
{
  // Its crush on units is the voxel version's. On a structure it deals controlShotDamage and cuts nothing; on terrain it
  // craters, leaving any structure's voxels in the crater standing (decision 2).
  _world.CrushAround(_hit.voxel);
  const Protocol::EntityId struck = _world.StructureHolding(_hit.voxel);
  if (struck != Protocol::NO_ENTITY)
  {
    Damage(_world, _world.m_structures.at(struck), _world.Data().Tuning().controlShotDamage);
    return;
  }
  _world.VoxelCrater(_hit, true);
}

void HealthStructureRule::RamStrikes(World& _world, Protocol::EntityId _structure, Protocol::VoxelPosition _voxel) const
{
  // controlRamDamage a blow, controlRamStonePercent of it on stone (decision 2).
  const Rules& rules = _world.Data().Tuning();
  const Behavior behavior = _world.Data().Materials().at(_world.Voxels().At(_voxel)).behavior;
  const bool stone = behavior == Behavior::BuildingStone || behavior == Behavior::CommonRock;
  Damage(_world, _world.m_structures.at(_structure),
         stone ? (rules.controlRamDamage * rules.controlRamStonePercent) / 100 : rules.controlRamDamage);
}

void HealthStructureRule::TrenchDug(World& _world, Protocol::VoxelPosition _voxel) const
{
  // The cubic metre is dug as in the voxel version, and each section within repairLockReachVoxels of it takes
  // controlTrenchDamagePerCubicMeter (decision 2).
  _world.DigVoxel(_voxel);
  const Rules& rules = _world.Data().Tuning();
  for (auto& [id, structure] : _world.m_structures)
  {
    const std::optional<World::Box> box = _world.BoxOf(structure);
    if (!box.has_value() || !_world.IsSection(structure))
    {
      continue;
    }
    const auto outside = [](std::int32_t _at, std::int32_t _low, std::int32_t _extent)
    { return std::max({_low - _at, 0, _at - (_low + _extent - 1)}); };
    if (std::max(outside(_voxel.xVoxels, box->min.xVoxels, box->widthVoxels),
                 outside(_voxel.zVoxels, box->min.zVoxels, box->depthVoxels)) <= rules.repairLockReachVoxels)
    {
      Damage(_world, structure, rules.controlTrenchDamagePerCubicMeter);
    }
  }
}

void HealthStructureRule::Repair(World& _world, Structure& _structure) const
{
  // The voxel version's rate, a cubic metre at a time, each worth controlHitPointsPerCubicMeter and paid for in the
  // template's material (decision 4).
  // A section at 0 has fallen whole and a gate at 0 has lost its gate, so neither is repaired; a building at 0 stands, and
  // is (decision 4, as amended).
  const TemplateRules* rules = _world.Data().FindTemplateRules(_structure.templateId);
  if (rules == nullptr || _structure.hitPoints >= rules->controlHitPoints ||
      (_structure.hitPoints == 0 && (_world.IsSection(_structure) || _world.HasGate(_structure))))
  {
    return;
  }
  const bool timber = _world.TimberTemplate(_structure);
  if (!_world.CanPay(_structure.owner, timber))
  {
    return;
  }
  _structure.repairProgress += _world.Data().Tuning().repairCubicMetersPerTick;
  if (_structure.repairProgress < Core::Fixed::FromRaw(Core::Fixed::ONE_RAW))
  {
    return;
  }
  _structure.repairProgress -= Core::Fixed::FromRaw(Core::Fixed::ONE_RAW);
  _structure.hitPoints = std::min(rules->controlHitPoints, _structure.hitPoints + rules->controlHitPointsPerCubicMeter);
  _world.Pay(_structure.owner, timber, _structure.id, _structure.originVoxels);
  _world.TouchStructure(_structure.id);
  _world.m_keepRestored = _world.m_keepRestored || (_world.m_trial.has_value() && _world.m_trial->keep == _structure.id);
}

void HealthStructureRule::UpdateWorking(World& _world, Structure& _structure,
                                        const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& /*_held*/) const
{
  // Its shares are its pool's, which the defender repairs by; a structure with a critical part works while it has hit
  // points, and one without none to lose stands as the voxel version leaves it.
  const TemplateRules* rules = _world.Data().FindTemplateRules(_structure.templateId);
  const std::int32_t most = rules == nullptr ? 0 : rules->controlHitPoints;
  const auto share = static_cast<std::uint8_t>(most <= 0 ? 100 : (100 * std::max(0, _structure.hitPoints)) / most);
  _structure.standingPercent = share;
  const ServerTemplate* found = _world.Data().FindTemplate(_structure.templateId);
  if (found == nullptr || found->criticalPart.empty())
  {
    return;
  }
  _structure.criticalPercent = share;
  _world.SetWorking(_structure, most <= 0 || _structure.hitPoints > 0);
}

bool HealthStructureRule::CollapsesStructures() const noexcept
{
  return false;
}

std::int32_t HealthStructureRule::MaxHitPoints(const World& _world, const Structure& _structure) const
{
  const TemplateRules* rules = _world.Data().FindTemplateRules(_structure.templateId);
  return rules == nullptr ? 0 : rules->controlHitPoints;
}

void HealthStructureRule::Damage(World& _world, Structure& _structure, std::int32_t _amount)
{
  const bool standing = _structure.hitPoints > 0;
  _structure.hitPoints = std::max(0, _structure.hitPoints - _amount);
  _structure.repairLockTicks = static_cast<std::uint32_t>(std::max(0, _world.Data().Tuning().repairLockTicks));
  _world.TouchStructure(_structure.id);
  if (!standing || _structure.hitPoints > 0)
  {
    return;
  }
  // At 0 (decision 3): a section falls whole, through the resolver's collapse; a gate's voxels go; a building stops in
  // step 10 and stands.
  if (_world.IsSection(_structure))
  {
    _world.FallWhole(_structure);
  }
  else if (_world.HasGate(_structure))
  {
    for (const Protocol::VoxelPosition& voxel : _world.GateVoxels(_structure))
    {
      _world.Voxels().Edit(voxel, Protocol::Chunk::EMPTY);
    }
  }
}

} // namespace Simulation
