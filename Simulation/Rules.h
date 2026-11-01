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
  Core::Fixed cavalrySpeedMetersPerTick; // the owner's, after M2's report: twice foot's
  std::int32_t machinesClimbRunCells;
  std::int32_t machinesHeadroomVoxels;
  std::int32_t machinesWidthVoxels;
  std::int32_t machinesFordDepthVoxels;
  std::int32_t machinesRubbleFactor;
  std::int32_t machinesFordFactor;
  Core::Fixed machinesSpeedMetersPerTick; // the owner's, after M2's report: half foot's
};

} // namespace Simulation
