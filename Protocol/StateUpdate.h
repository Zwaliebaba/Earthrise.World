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
  // M4's (MvpPlan-M4.md, Contracts), each filled from the order that brings it; until then as a whole, working structure.
  bool working = true;                // ADR-024, decision 2; true for a structure without a critical part
  std::uint8_t criticalPercent = 100; // the part's standing share, 100 without a part
  std::uint32_t repairLockTicks = 0;
  bool gateOpen = false;
  std::uint8_t garrisonCount = 0; // the owner's update only; 0 in others'
  std::uint8_t garrisonCapacity = 0;
  std::int32_t hitPoints = 0; // the control only (ADR-028); 0 in the voxel version
  std::int32_t maxHitPoints = 0;
  bool opened = false;                // sections only: a foot route from the camp through its footprint to its inner side (ADR-026)
  bool lastSeen = false;              // not visible now: as it stood when last visible (ADR-025, decision 2)
  std::uint8_t standingPercent = 100; // the share of its built voxels that stand, every part (M4-F2)

  friend bool operator==(const StructureView&, const StructureView&) = default;
};

struct EngineView
{
  EntityId id;
  std::uint8_t owner;
  EngineKind kind;
  VoxelPosition cellVoxels;
  std::uint32_t quarterTurns;
  std::uint32_t reloadTicks;  // 0 when loaded
  EngineState state;          // a ram stays Packed
  EntityId squad = NO_ENTITY; // its body's: an engine moves as it
  std::uint32_t stateTicksLeft = 0;
  FixedPosition position{};
  std::int32_t hitPoints = 0; // its body's
  std::int32_t maxHitPoints = 0;

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
  EntityId garrisonedIn; // the building it is in, or NO_ENTITY; its units are then in no cell and not in the update
  UnitKind kind = UnitKind::Spearman;
  SquadOrder order = SquadOrder::Idle;
  std::uint8_t fullFigures = 0; // its kind's figures a squad
  EntityId target = NO_ENTITY;

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
  std::int32_t hitPoints = 0;
  std::int32_t maxHitPoints = 0;

  friend bool operator==(const UnitView&, const UnitView&) = default;
};

struct ShotView
{
  EntityId id;
  EntityId engine; // the shooter's id, a squad's for an arrow
  FixedPosition position;
  ShotKind kind = ShotKind::Stone;
  std::uint8_t owner = 0;

  friend bool operator==(const ShotView&, const ShotView&) = default;
};

/// A player of the match and the stockpiles they spend (MvpPlan-M4.md, Contracts).
struct PlayerView
{
  std::uint8_t player;
  Side side;
  std::int32_t stoneHundredthsCubicMeters; // 0 for other players
  std::int32_t timberHundredthsCubicMeters;

  friend bool operator==(const PlayerView&, const PlayerView&) = default;
};

/// A command of the receiving player's that the simulation refused this tick, and why (ADR-023, decision 10).
struct RefusalView
{
  std::uint16_t commandTag;
  Refusal reason;

  friend bool operator==(const RefusalView&, const RefusalView&) = default;
};

/// The Siege trial as it stands (ADR-026, decision 4): version Voxel and outcome Running outside a trial.
struct TrialView
{
  TrialVersion version = TrialVersion::Voxel;
  TrialOutcome outcome = TrialOutcome::Running;
  std::uint32_t ticksLeft = 0; // of the time limit
  EntityId keep = NO_ENTITY;
  bool keepWorking = true;
  std::uint32_t holdTicks = 0; // the storming clock
  std::uint32_t holdTicksNeeded = 0;
  bool holdContested = false;

  friend bool operator==(const TrialView&, const TrialView&) = default;
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
  std::vector<ShotHitView> hits;   // the tick's, in shot-id order
  std::vector<PlayerView> players; // in player order
  std::uint8_t player = 0;         // whose filter made it
  std::vector<RefusalView> refusals;
  TrialView trial;
  std::int32_t widthCells = 0; // the map's, for visibleCells
  std::int32_t depthCells = 0;
  std::vector<std::uint64_t> visibleCells; // bit z * widthCells + x (ADR-025, decision 2); empty until M4-E1

  friend bool operator==(const StateUpdate&, const StateUpdate&) = default;
};

} // namespace Protocol
