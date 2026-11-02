#pragma once

#include "ChunkCoordinate.h"
#include "EntityId.h"
#include "EntityKinds.h"
#include "FixedPosition.h"
#include "PlacedVoxel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Protocol
{

// What the simulation tells the client each tick (ADR-020, decision 3): the whole visible state, as
// values. In single-player it crosses as a copy of this struct; it gains a byte encoding and a
// record version when online work starts (ADR-002, decision 3). Every vector is in id order.

/// A template placed in the world (ADR-005, decision 10).
struct StructureView
{
  EntityId id;
  std::uint8_t owner;
  std::string templateId;
  VoxelPosition originVoxels; // the footprint's minimum corner, after the turn
  std::uint32_t quarterTurns; // clockwise seen from above, as Engine::VoxelPass::Batch counts them
  VoxelPosition extentVoxels; // the box's size after the turn: templates are the server's, and the client draws by it
  std::string name;           // the scenario's, such as W2

  friend bool operator==(const StructureView&, const StructureView&) = default;
};

struct EngineView
{
  EntityId id;
  std::uint8_t owner;
  EngineKind kind;
  VoxelPosition cellVoxels;
  std::uint32_t quarterTurns;
  std::uint32_t reloadTicks; // 0 when loaded

  friend bool operator==(const EngineView&, const EngineView&) = default;
};

struct SquadView
{
  EntityId id;
  std::uint8_t owner;
  std::vector<EntityId> units; // in id order
  bool hasGoal;
  VoxelPosition goalVoxels;
  bool shortOfGoal;

  friend bool operator==(const SquadView&, const SquadView&) = default;
};

struct UnitView
{
  EntityId id;
  EntityId squad;
  std::uint8_t owner;
  UnitKind kind;
  FixedPosition position; // the middle of its feet
  VoxelPosition cellVoxels;

  friend bool operator==(const UnitView&, const UnitView&) = default;
};

struct ShotView
{
  EntityId id;
  EntityId engine;
  FixedPosition position;

  friend bool operator==(const ShotView&, const ShotView&) = default;
};

/// A collapse in progress, from its first warning tick to the tick its fall ends.
struct CollapseView
{
  EntityId id;
  EntityId structure;             // whose piece it is, or NO_ENTITY
  std::uint32_t warningTicksLeft; // 0 once it falls
  std::uint32_t fallTicksLeft;    // the fall's ticks still to come
  std::vector<VoxelPosition> piece;
  std::vector<VoxelPosition> strip; // the columns the fall covers, at their ground: where units are hit
  std::vector<PlacedVoxel> landing; // where each rubble voxel lands, nearest the foot first: where its debris rests
  std::uint32_t landedVoxels;       // how many of landing have landed: 0 until the fall's first tick
  std::vector<PlacedVoxel> fallen;  // the piece's voxels as they were, empty until the fall's first tick

  friend bool operator==(const CollapseView&, const CollapseView&) = default;
};

/// Where a shot struck this tick.
struct ShotHitView
{
  EntityId shot;
  VoxelPosition voxel;

  friend bool operator==(const ShotHitView&, const ShotHitView&) = default;
};

struct StateUpdate
{
  std::uint32_t tick; // the tick it describes the end of
  std::vector<StructureView> structures;
  std::vector<EngineView> engines;
  std::vector<SquadView> squads;
  std::vector<UnitView> units;
  std::vector<ShotView> shots;
  std::vector<CollapseView> collapses;
  std::vector<ShotHitView> hits; // the tick's, in shot-id order

  friend bool operator==(const StateUpdate&, const StateUpdate&) = default;
};

} // namespace Protocol
