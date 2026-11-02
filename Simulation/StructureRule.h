#pragma once

#include "ChunkCoordinate.h"
#include "EntityId.h"

#include <cstdint>
#include <set>
#include <tuple>

namespace Simulation
{

class World;
struct ShotHit;
struct Structure;

/// How structures take hits, are repaired and work (ADR-028, decision 1): the one seam between the voxel version and the
/// control. The world takes the rule its trial's version names, and nothing else asks which version runs.
class StructureRule
{
public:
  StructureRule() = default;
  StructureRule(const StructureRule&) = delete;
  StructureRule& operator=(const StructureRule&) = delete;
  StructureRule(StructureRule&&) = delete;
  StructureRule& operator=(StructureRule&&) = delete;
  virtual ~StructureRule() = default;

  /// A stone's strike at _hit's voxel, in step 4: its crush on units, its crater, and the repair lock.
  virtual void ShotStrikes(World& _world, const ShotHit& _hit) const = 0;
  /// A ram's blow on _structure at _voxel, in step 4.
  virtual void RamStrikes(World& _world, Protocol::EntityId _structure, Protocol::VoxelPosition _voxel) const = 0;
  /// A sapper's cubic metre dug at _voxel, in step 2.
  virtual void TrenchDug(World& _world, Protocol::VoxelPosition _voxel) const = 0;
  /// One tick of repair of _structure by a laborer at work, its lock already counted out, in step 2.
  virtual void Repair(World& _world, Structure& _structure) const = 0;
  /// Whether _structure works, and its shares, in step 10, the voxels collapses hold given as _held.
  virtual void UpdateWorking(World& _world, Structure& _structure,
                             const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held) const = 0;
  /// Whether the support search may bring a structure's voxels down; in the control a structure stands while its pool lasts.
  [[nodiscard]] virtual bool CollapsesStructures() const noexcept = 0;
  /// _structure's full pool, for its view: 0 in the voxel version.
  [[nodiscard]] virtual std::int32_t MaxHitPoints(const World& _world, const Structure& _structure) const = 0;
};

} // namespace Simulation
