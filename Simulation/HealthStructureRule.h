#pragma once

#include "StructureRule.h"

namespace Simulation
{

/// The control (ADR-028, decisions 2 to 4): one pool of hit points a structure, from STRC. A stone, a ram's blow or a
/// trench beside a section takes from the pool and removes no voxel of a structure; terrain craters as in the voxel
/// version. At 0 a section falls whole with its warning, a building stops and stands, and a gate's voxels go. Repair puts
/// back hit points, controlHitPointsPerCubicMeter for each cubic metre the voxel version's rate gives. Stateless.
class HealthStructureRule final : public StructureRule
{
public:
  [[nodiscard]] static const HealthStructureRule& Instance() noexcept;

  void ShotStrikes(World& _world, const ShotHit& _hit) const override;
  void RamStrikes(World& _world, Protocol::EntityId _structure, Protocol::VoxelPosition _voxel) const override;
  void TrenchDug(World& _world, Protocol::VoxelPosition _voxel) const override;
  void Repair(World& _world, Structure& _structure) const override;
  void UpdateWorking(World& _world, Structure& _structure,
                     const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held) const override;
  [[nodiscard]] bool CollapsesStructures() const noexcept override;
  [[nodiscard]] std::int32_t MaxHitPoints(const World& _world, const Structure& _structure) const override;

private:
  /// Takes _amount from _structure's pool, locks its repair, and at 0 lets it fall, stop or lose its gate.
  static void Damage(World& _world, Structure& _structure, std::int32_t _amount);
};

} // namespace Simulation
