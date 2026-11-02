#pragma once

#include "Fixed.h"

#include <cstdint>

namespace Simulation
{

/// The tuning values the simulation decides by: ServerData's RULE section, one field for each key,
/// named as the key is and counted in its unit (ADR-016, decision 3). GameData/Server/Rules.json
/// holds them in the design's units, and the bake converts them exactly: meters to voxels and
/// seconds to ticks, whole, and speeds and accelerations to Core::Fixed a tick. A field without
/// Fixed's type is a whole number. An order that adds a key adds its field here in the same commit.
struct Rules
{
  // Spans, which MATL also gives each palette index (GameDesign.md, M2's values, item 14).
  std::int32_t buildingStoneSpanVoxels;
  std::int32_t commonRockSpanVoxels;
  std::int32_t timberSpanVoxels;
  std::int32_t leavesSpanVoxels;

  // A collapse's warning and its fall (items 4 and 5): the warning is warningBaseTicks plus the time
  // a foot soldier walks the piece's height, and the fall lasts the time it takes to drop that far.
  std::int32_t warningBaseTicks;
  Core::Fixed footSpeedMetersPerTick;
  Core::Fixed gravityMetersPerTickSquared;

  // The trebuchet's shot (item 12).
  std::int32_t shotLaunchHeightVoxels; // above the engine's cell
  std::int32_t shotMinRangeVoxels;
  std::int32_t shotMaxRangeVoxels;
  std::int32_t shotReloadTicks;

  // The crater a shot leaves in each behaviour, on the ladder of 1, 2, 4 and 7 voxels, or 0 for none
  // (item 12).
  std::int32_t earthCraterVoxels;
  std::int32_t commonRockCraterVoxels;
  std::int32_t buildingStoneCraterVoxels;
  std::int32_t bedrockCraterVoxels;
  std::int32_t timberCraterVoxels;
  std::int32_t leavesCraterVoxels;
  std::int32_t waterCraterVoxels;
  std::int32_t stoneRubbleCraterVoxels;
  std::int32_t wasteRubbleCraterVoxels;

  // Loose material rests at least this far below the top of a structure beside it (item 9).
  std::int32_t looseRestBelowTopVoxels;

  // The work budgets a tick (ADR-013, decision 6; item 13): Proposed until M2-G2 measures them.
  std::int32_t slumpVoxelsPerTick; // voxels slump moves
  std::int32_t pathNodesPerTick;   // nodes the path searches expand
  // The units of work step 12 spends on queries (ADR-009, amended; ADR-020, decision 6): an aim check
  // 1, a tile's reach 1, a section's integrity 8. Proposed until M3-G3 measures it (M3's values, item 10).
  std::int32_t queryWorkPerTick;

  // The navigation classes (item 10; ADR-015, decision 2): a climb of 1 m over a run of ClimbRun
  // cells, a headroom, a width, the deepest water forded, and the factors that slow it on rubble and
  // in a ford.
  std::int32_t footClimbRunCells;
  std::int32_t footHeadroomVoxels;
  std::int32_t footWidthVoxels;
  std::int32_t footFordDepthVoxels;
  std::int32_t footRubbleFactor;
  std::int32_t footFordFactor;
  std::int32_t cavalryClimbRunCells;
  std::int32_t cavalryHeadroomVoxels;
  std::int32_t cavalryWidthVoxels;
  std::int32_t cavalryFordDepthVoxels;
  std::int32_t cavalryRubbleFactor;
  std::int32_t cavalryFordFactor;
  std::int32_t machinesClimbRunCells;
  std::int32_t machinesHeadroomVoxels;
  std::int32_t machinesWidthVoxels;
  std::int32_t machinesFordDepthVoxels;
  std::int32_t machinesRubbleFactor;
  std::int32_t machinesFordFactor;
  // Each kind's own speed is UNIT's (ADR-023, decision 1): footSpeedMetersPerTick, above, times the collapse's warning.

  // M4's values (GameDesign.md, M4's values), by item. A shot's price in stone (5; ADR-023, decision 10).
  std::int32_t shotStoneHundredthsCubicMeters;
  // Arrows and elevation (7; ADR-023, decision 5): flown straight from the launch height above the shooter's cell to the
  // aim height above the target's, hitting a target within the reach of the aim point; a step of height adds a band of
  // range, at most the steps' maximum, and ELEVATION_STEP_PERCENT of damage (UnitStats.h).
  Core::Fixed arrowSpeedMetersPerTick;
  std::int32_t arrowLaunchHeightCentimeters;
  std::int32_t arrowAimHeightVoxels;
  std::int32_t arrowHitReachCells;
  std::int32_t elevationStepVoxels;
  std::int32_t elevationMaxSteps;
  std::int32_t elevationRangeBandVoxels;
  // Crush from a shot, a collapse and a drop beyond 3 m (8; ADR-023, decisions 7 and 8).
  std::int32_t shotCrushDamage;
  std::int32_t collapseCrushDamage;
  std::int32_t dropCrushDamagePerVoxel;
  // The ram's blow and its craters, stone only within the reach above the structure's base (9; ADR-023, decision 9).
  std::int32_t ramBlowTicks;
  std::int32_t ramTimberCraterVoxels;
  std::int32_t ramEarthCraterVoxels;
  std::int32_t ramStoneCraterVoxels;
  std::int32_t ramStoneReachVoxels;
  // A garrison heals a hit point every so many ticks (10; ADR-024, decision 5).
  std::int32_t garrisonHealTicksPerHitPoint;
  // Repair and its lock (11; ADR-024, decisions 3 and 4).
  Core::Fixed repairCubicMetersPerTick;
  std::int32_t repairReachVoxels;
  std::int32_t repairLockTicks;
  std::int32_t repairLockReachVoxels;
  // The gatehouse's passage attack (13; ADR-024, decision 6).
  std::int32_t passageAttackTicks;
  std::int32_t passageAttackDamage;
  // The trebuchet's deploying and packing (17).
  std::int32_t trebuchetDeployTicks;
  std::int32_t trebuchetPackTicks;
  // The control's damage to structures (19; ADR-028, decision 2): a shot's, a ram's blow, the share of a blow on stone,
  // and a trench's for each cubic metre dug near a section.
  std::int32_t controlShotDamage;
  std::int32_t controlRamDamage;
  std::int32_t controlRamStonePercent;
  std::int32_t controlTrenchDamagePerCubicMeter;
  // A figure blocked this long marks its squad to search again (ADR-022, decision 6).
  std::int32_t waitBeforeSearchTicks;
  // A scenario that reveals the map does so for this long (ADR-025, decision 5; item 14).
  std::int32_t revealTicks;
  // The commanders' cadence and their work a tick (ADR-027, decision 4; items 18 and 27, Proposed until M4-J4).
  std::int32_t commanderDecisionTicks;
  std::int32_t commanderWorkPerTick;
};

} // namespace Simulation
