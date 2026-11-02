#pragma once

#include "StructureRule.h"

namespace Simulation
{

/// The voxel version (ADR-028, decision 1): M2's and M4's craters, the support search and the resolver, critical parts
/// read from the voxels that stand, and repair that puts voxels back (ADR-024). Stateless: one instance serves every world.
class VoxelStructureRule final : public StructureRule
{
public:
  [[nodiscard]] static const VoxelStructureRule& Instance() noexcept;

  void ShotStrikes(World& _world, const ShotHit& _hit) const override;
  void RamStrikes(World& _world, Protocol::EntityId _structure, Protocol::VoxelPosition _voxel) const override;
  void TrenchDug(World& _world, Protocol::VoxelPosition _voxel) const override;
  void Repair(World& _world, Structure& _structure) const override;
  void UpdateWorking(World& _world, Structure& _structure,
                     const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held) const override;
  [[nodiscard]] bool CollapsesStructures() const noexcept override;
  [[nodiscard]] std::int32_t MaxHitPoints(const World& _world, const Structure& _structure) const override;
};

} // namespace Simulation
