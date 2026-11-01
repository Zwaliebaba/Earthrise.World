#pragma once

#include "Script.h"
#include "ServerData.h"
#include "World.h"

#include <cstdint>
#include <vector>

namespace Simulation
{

/// The M2 breach run (MvpPlan.md, M2, *Done when*; M2-G1). It is built in C++, as M1's test world
/// is, so the tests and SimulationBench run the same thing; it is not a scenario in ADR-014's sense.
///
/// - **The ground:** the generated MVP map at fixed seeds. The run stands on a dry, treeless stretch
///   whose ground lies between 8 m and 10 m, which BreachRunSite names; its footprint is levelled to
///   9 m by edits.
/// - **The wall:** an enclosure of T3 sections, 24 m by 20 m outside, so a squad cannot walk round
///   it. Its south side is three sections; the middle one is the target.
/// - **The forces:** a trebuchet 40 m south of the wall's south face, and a squad of six foot 10 m
///   south of it, ordered on tick 1, once navigation knows the enclosure, to a point in the
///   courtyard 6 m inside the south wall.
/// - **The shots:** five FireEngine commands, one each time the trebuchet has reloaded, along the
///   middle section's foot. E3's finding is that a hole 5 wide through a T3 section drops the slot
///   above its middle, since stone spans 2 m (ADR-004), so the shots open that hole, a voxel and
///   the one behind it at a time.
struct BreachRun
{
  static constexpr std::uint64_t WORLD_SEED = 0x5eed2026ULL;
  static constexpr std::uint64_t RESOURCE_SEED = WORLD_SEED + 1;
  static constexpr std::uint64_t MATCH_SEED = WORLD_SEED + 2;
  static constexpr std::uint8_t PLAYER = 1;
  static constexpr std::size_t SQUAD_FIGURES = 6;

  /// The level the footprint is brought to: the first empty voxel, where units stand.
  static constexpr std::int32_t GROUND_VOXELS = 9;
  /// The enclosure's outside corners, both included.
  static constexpr std::int32_t WALL_WEST_VOXELS = 369;
  static constexpr std::int32_t WALL_EAST_VOXELS = 392;
  static constexpr std::int32_t WALL_SOUTH_VOXELS = 180;
  static constexpr std::int32_t WALL_NORTH_VOXELS = 199;
  /// The levelled footprint: the enclosure, and the ground south of it where the squad starts.
  static constexpr std::int32_t LEVEL_WEST_VOXELS = 362;
  static constexpr std::int32_t LEVEL_EAST_VOXELS = 399;
  static constexpr std::int32_t LEVEL_SOUTH_VOXELS = 165;
  static constexpr std::int32_t LEVEL_NORTH_VOXELS = 206;

  Protocol::EntityId engine;
  Protocol::EntityId squad;
  Protocol::VoxelPosition goalVoxels; // the courtyard's middle
  std::vector<ScriptedCommand> script;
  std::uint32_t ticks; // how long the run lasts: every shot, its collapse and the walk
};

/// The run's world: the generated MVP map at the run's seeds. The generator lives as long as the
/// Desc's functions do.
[[nodiscard]] World::Desc BreachRunDesc(const ServerData& _data, std::uint64_t _buildId);

/// Builds the run on a world made from BreachRunDesc and not yet ticked: levels the footprint,
/// raises the enclosure and places the trebuchet and the squad, all by edits and between ticks.
/// Returns the run's entities and its script.
[[nodiscard]] BreachRun BuildBreachRun(World& _world);

} // namespace Simulation
