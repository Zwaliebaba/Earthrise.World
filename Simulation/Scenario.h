#pragma once

#include "ByteReader.h"
#include "ChunkCoordinate.h"
#include "EntityKinds.h"
#include "ServerData.h"
#include "World.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Simulation
{

// A scenario as ServerData's SCEN section holds it (ADR-026, decisions 1 and 2): GameData/Server/Scenarios/'s source,
// baked with every name resolved, every value converted and every mirror made whole. Coordinates are the map's voxels.

/// A box of one material, both corners included; material 0 empties it.
struct ScenarioBox
{
  std::uint8_t material;
  Protocol::VoxelPosition minVoxels;
  Protocol::VoxelPosition maxVoxels;
};

struct ScenarioStructure
{
  std::string templateId;
  std::string name;
  std::uint8_t owner;
  Protocol::VoxelPosition originVoxels;
  std::uint32_t quarterTurns;
  std::int32_t foundationDepthVoxels;
};

struct ScenarioPlayer
{
  std::uint8_t id;
  Protocol::Side side;
  std::int32_t stoneHundredthsCubicMeters;
  std::int32_t timberHundredthsCubicMeters;
  std::string commander; // human, siegeDefender or siegeAttacker (ADR-027, decision 1)
  bool revealed;         // its map starts seen (ADR-025, decision 5)
};

struct ScenarioSquad
{
  Protocol::UnitKind kind;
  std::uint8_t owner;
  std::vector<Protocol::VoxelPosition> cellsVoxels; // the lead's first
};

struct ScenarioEngine
{
  Protocol::EngineKind kind;
  std::uint8_t owner;
  Protocol::VoxelPosition cellVoxels;
  std::uint32_t quarterTurns;
  Protocol::EngineState state;
};

/// A squad of its kind's figures placed in a named building.
struct ScenarioGarrison
{
  Protocol::UnitKind kind;
  std::uint8_t owner;
  std::string structure;
};

struct ScenarioTrial
{
  Protocol::TrialVersion version;
  std::uint8_t attacker;
  std::uint8_t defender;
  std::string keep;
  std::uint32_t timeLimitTicks;
  std::uint32_t holdTicks;
  std::int32_t ringCells;
  Protocol::VoxelPosition campMinVoxels;
  Protocol::VoxelPosition campMaxVoxels;
  std::string nearest; // the section nearest the camp and the weak one, for the log; nothing in the simulation reads them
  std::string weak;
};

/// One scenario: its seeds, the site it owns and levels, its ground and earthworks, its structures, its players, its
/// armies and its trial.
struct Scenario
{
  std::string id;
  std::uint64_t worldSeed;
  std::uint64_t resourceSeed;
  std::uint64_t matchSeed;
  std::int32_t siteMinXVoxels; // the site's columns, both corners included
  std::int32_t siteMinZVoxels;
  std::int32_t siteMaxXVoxels;
  std::int32_t siteMaxZVoxels;
  std::int32_t groundVoxels; // the level its ground is brought to: the first empty voxel
  std::vector<ScenarioBox> ground;
  std::vector<ScenarioStructure> structures;
  std::vector<ScenarioBox> earthworks;
  std::vector<ScenarioPlayer> players;
  std::vector<ScenarioSquad> squads;
  std::vector<ScenarioEngine> engines;
  std::vector<ScenarioGarrison> garrisons;
  ScenarioTrial trial;
};

/// Reads one scenario in SCEN's layout (Tools/BakeContent.py). Throws std::out_of_range when the bytes end first and
/// std::runtime_error for a value this build does not know.
[[nodiscard]] Scenario ReadScenario(Core::ByteReader& _reader);

/// The world a scenario is played on: the generated MVP map from its seeds, whose surface inside its site is the
/// site's level, so that the anchor depth counts from the ground the scenario owns (ADR-004, decision 2). The generator
/// lives as long as the Desc's functions do. Throws std::invalid_argument for a scenario _data lacks.
[[nodiscard]] World::Desc ScenarioDesc(const ServerData& _data, std::string_view _scenarioId, std::uint64_t _buildId);

/// Builds the scenario on a world made from ScenarioDesc and not yet ticked, in the source's order (ADR-026, decision 3):
/// the site levelled and filled from its ground boxes, the earthworks, the structures, the players, the squads, the
/// engines and the garrisons, and the trial's rules, all by edits and between ticks. Throws std::invalid_argument for
/// an unknown id.
void LoadScenario(World& _world, std::string_view _scenarioId);

} // namespace Simulation
