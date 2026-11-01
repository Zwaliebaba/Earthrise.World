#include "pch.h"

#include "BreachRun.h"

#include "Generator.h"

namespace Simulation
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::int32_t SECTION_LENGTH_VOXELS = 8;
constexpr std::int32_t TREBUCHET_STANDOFF_VOXELS = 40;
constexpr std::int32_t SQUAD_STANDOFF_VOXELS = 10;
constexpr std::int32_t GOAL_INSIDE_VOXELS = 6; // nearer the south wall than the north, so the squad waits south
constexpr std::uint32_t ORDER_TICK = 1;
constexpr std::uint32_t FIRST_SHOT_TICK = 1;
constexpr std::uint32_t RUN_OUT_TICKS = 400; // after the trebuchet reloads from its last shot: the collapse is down and the squad inside

/// The first empty voxel of a column, from the top down.
std::int32_t FirstEmptyAbove(const World& _world, std::int32_t _x, std::int32_t _z)
{
  const std::int32_t height = _world.Description().heightChunks * Protocol::CHUNK_EDGE_VOXELS;
  for (std::int32_t y = height - 1; y >= 0; --y)
  {
    if (_world.Voxels().At({.xVoxels = _x, .yVoxels = y, .zVoxels = _z}) != Protocol::Chunk::EMPTY)
    {
      return y + 1;
    }
  }
  return 0;
}

/// Brings every column of the footprint to BreachRun::GROUND_VOXELS: earth below, nothing above.
void Level(World& _world)
{
  const std::int32_t height = _world.Description().heightChunks * Protocol::CHUNK_EDGE_VOXELS;
  for (std::int32_t z = BreachRun::LEVEL_SOUTH_VOXELS; z <= BreachRun::LEVEL_NORTH_VOXELS; ++z)
  {
    for (std::int32_t x = BreachRun::LEVEL_WEST_VOXELS; x <= BreachRun::LEVEL_EAST_VOXELS; ++x)
    {
      for (std::int32_t y = 0; y < height; ++y)
      {
        const Protocol::VoxelPosition voxel{.xVoxels = x, .yVoxels = y, .zVoxels = z};
        if (y < BreachRun::GROUND_VOXELS && _world.Voxels().At(voxel) == Protocol::Chunk::EMPTY)
        {
          _world.Voxels().Edit(voxel, EARTH);
        }
        else if (y >= BreachRun::GROUND_VOXELS)
        {
          _world.Voxels().Edit(voxel, Protocol::Chunk::EMPTY);
        }
      }
    }
  }
}

/// The enclosure, as structures W1 to W10, numbered anticlockwise seen from above from the south-west
/// corner, south side first: three sections a side north and south and two a side east and west
/// between them, each facing out. Turns are clockwise seen from above, as the voxel pass counts them:
/// the south side's fronts face south after two, the east side's east after one, and the west side's
/// west after three.
void RaiseEnclosure(World& _world)
{
  const ServerTemplate* t3 = _world.Data().FindTemplate("T3");
  if (t3 == nullptr)
  {
    throw std::runtime_error("the breach run needs T3 in the server data");
  }
  const auto thickness = static_cast<std::int32_t>(t3->depthVoxels);
  const auto at = [](std::int32_t _x, std::int32_t _z)
  { return Protocol::VoxelPosition{.xVoxels = _x, .yVoxels = BreachRun::GROUND_VOXELS, .zVoxels = _z}; };
  std::int32_t number = 0;
  const auto raise = [&](std::int32_t _x, std::int32_t _z, std::uint32_t _quarterTurns)
  { static_cast<void>(_world.AddStructure(BreachRun::DEFENDER, "T3", at(_x, _z), _quarterTurns, "W" + std::to_string(++number))); };
  for (std::int32_t section = 0; section < 3; ++section)
  {
    raise(BreachRun::WALL_WEST_VOXELS + section * SECTION_LENGTH_VOXELS, BreachRun::WALL_SOUTH_VOXELS, 2);
  }
  for (std::int32_t section = 0; section < 2; ++section)
  {
    raise(BreachRun::WALL_EAST_VOXELS - thickness + 1, BreachRun::WALL_SOUTH_VOXELS + thickness + section * SECTION_LENGTH_VOXELS, 1);
  }
  for (std::int32_t section = 2; section >= 0; --section)
  {
    raise(BreachRun::WALL_WEST_VOXELS + section * SECTION_LENGTH_VOXELS, BreachRun::WALL_NORTH_VOXELS - thickness + 1, 0);
  }
  for (std::int32_t section = 1; section >= 0; --section)
  {
    raise(BreachRun::WALL_WEST_VOXELS, BreachRun::WALL_SOUTH_VOXELS + thickness + section * SECTION_LENGTH_VOXELS, 3);
  }
}

} // namespace

World::Desc BreachRunDesc(const ServerData& _data, std::uint64_t _buildId)
{
  const auto generator = std::make_shared<const Generator>(Generator::Desc{.worldSeed = BreachRun::WORLD_SEED,
                                                                           .resourceSeed = BreachRun::RESOURCE_SEED,
                                                                           .widthVoxels = Generator::MAP_WIDTH_VOXELS,
                                                                           .heightVoxels = Generator::MAP_HEIGHT_VOXELS,
                                                                           .depthVoxels = Generator::MAP_DEPTH_VOXELS});
  return {.serverData = &_data,
          .buildId = _buildId,
          .worldSeed = BreachRun::WORLD_SEED,
          .resourceSeed = BreachRun::RESOURCE_SEED,
          .matchSeed = BreachRun::MATCH_SEED,
          .generatorVersion = GENERATOR_VERSION,
          .widthChunks = generator->WidthChunks(),
          .heightChunks = generator->HeightChunks(),
          .depthChunks = generator->DepthChunks(),
          .generate = [generator](Protocol::ChunkCoordinate _coordinate, Protocol::Chunk& _chunk)
          { generator->GenerateChunk(_coordinate, _chunk); },
          .surfaceVoxels = [generator](std::int32_t _x, std::int32_t _z) { return generator->SurfaceAt(_x, _z); }};
}

BreachRun BuildBreachRun(World& _world)
{
  if (_world.TickCount() != 0)
  {
    throw std::invalid_argument("the breach run is built on a world that has not ticked");
  }
  Level(_world);
  RaiseEnclosure(_world);

  const std::int32_t middleX = (BreachRun::WALL_WEST_VOXELS + BreachRun::WALL_EAST_VOXELS + 1) / 2;
  const std::int32_t engineZ = BreachRun::WALL_SOUTH_VOXELS - TREBUCHET_STANDOFF_VOXELS;
  const Protocol::VoxelPosition engineCell{.xVoxels = middleX, .yVoxels = FirstEmptyAbove(_world, middleX, engineZ), .zVoxels = engineZ};
  BreachRun run{
    .engine = _world.AddSiegeEngine(BreachRun::PLAYER, EngineKind::Trebuchet, engineCell, BreachRun::TREBUCHET_QUARTER_TURNS),
    .squad = Protocol::NO_ENTITY,
    .goalVoxels = {.xVoxels = middleX, .yVoxels = BreachRun::GROUND_VOXELS, .zVoxels = BreachRun::WALL_SOUTH_VOXELS + GOAL_INSIDE_VOXELS},
    .script = {},
    .ticks = 0};

  // Six foot in a row from east to west, the lead at the east end, 10 m south of the wall.
  std::vector<Protocol::VoxelPosition> cells;
  cells.reserve(BreachRun::SQUAD_FIGURES);
  for (std::size_t figure = 0; figure < BreachRun::SQUAD_FIGURES; ++figure)
  {
    cells.push_back({.xVoxels = middleX + 2 - static_cast<std::int32_t>(figure),
                     .yVoxels = BreachRun::GROUND_VOXELS,
                     .zVoxels = BreachRun::WALL_SOUTH_VOXELS - SQUAD_STANDOFF_VOXELS});
  }
  run.squad = _world.AddSquad(BreachRun::PLAYER, NavigationClass::Foot, cells);
  // Ordered once tick 0's rebuild has put the enclosure into navigation.
  run.script.push_back(
    {.tick = ORDER_TICK, .player = BreachRun::PLAYER, .command = Protocol::MoveSquad{.squad = run.squad, .goalVoxels = run.goalVoxels}});

  // The shots: five along the middle section's foot, centred under its middle, west to east, each as
  // soon as the trebuchet has reloaded. Each is aimed at the bottom row of the south face, and a
  // shot descending at 45 degrees meets the face one voxel above its aim point, so they open a hole
  // 5 wide through the wall one voxel above its base, and the slot above the hole's middle falls
  // (ADR-004; E3's finding).
  const auto reloadTicks = static_cast<std::uint32_t>(_world.Data().Tuning().shotReloadTicks);
  std::uint32_t tick = FIRST_SHOT_TICK;
  for (std::int32_t x = middleX - 2; x <= middleX + 2; ++x)
  {
    run.script.push_back({.tick = tick,
                          .player = BreachRun::PLAYER,
                          .command = Protocol::FireEngine{
                            .engine = run.engine,
                            .targetVoxels = {.xVoxels = x, .yVoxels = BreachRun::GROUND_VOXELS, .zVoxels = BreachRun::WALL_SOUTH_VOXELS}}});
    tick += reloadTicks;
  }
  run.ticks = tick + RUN_OUT_TICKS;
  return run;
}

} // namespace Simulation
