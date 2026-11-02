#include "pch.h"

#include "VoxelStructureRule.h"

#include "World.h"

namespace Simulation
{

const VoxelStructureRule& VoxelStructureRule::Instance() noexcept
{
  static const VoxelStructureRule RULE;
  return RULE;
}

void VoxelStructureRule::ShotStrikes(World& _world, const ShotHit& _hit) const
{
  _world.CrushAround(_hit.voxel);
  _world.VoxelCrater(_hit);
}

void VoxelStructureRule::RamStrikes(World& _world, Protocol::EntityId _structure, Protocol::VoxelPosition _voxel) const
{
  _world.VoxelRamBlow(_world.m_structures.at(_structure), _voxel);
}

void VoxelStructureRule::TrenchDug(World& _world, Protocol::VoxelPosition _voxel) const
{
  _world.DigVoxel(_voxel);
}

void VoxelStructureRule::Repair(World& _world, Structure& _structure) const
{
  _world.VoxelRepair(_structure);
}

void VoxelStructureRule::UpdateWorking(World& _world, Structure& _structure,
                                       const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held) const
{
  _structure.criticalPercent = _world.CriticalShare(_structure, _held);
  _structure.standingPercent = _world.StandingShare(_structure, _held);
  const TemplateRules* rules = _world.Data().FindTemplateRules(_structure.templateId);
  _world.SetWorking(_structure, rules == nullptr || _structure.criticalPercent >= rules->criticalThresholdPercent);
}

bool VoxelStructureRule::CollapsesStructures() const noexcept
{
  return true;
}

std::int32_t VoxelStructureRule::MaxHitPoints(const World& /*_world*/, const Structure& /*_structure*/) const
{
  return 0;
}

} // namespace Simulation
