#include "pch.h"

#include "BreachRun.h"

#include "BreachLab.h"
#include "Generator.h"

namespace Simulation
{

namespace
{

constexpr std::uint32_t FIRST_SHOT_TICK = 1;
constexpr std::uint32_t RUN_OUT_TICKS = 400; // after the trebuchet reloads from its last shot: the collapse is down and the squad inside

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
  // The lab's site, the same code, and the script the lab's LabShots would play: the order on tick 1,
  // once tick 0's rebuild has put the enclosure into navigation, and the shots.
  const LabSetup site = BuildLabSite(_world);
  const std::int32_t middleX = site.goalVoxels.xVoxels;
  BreachRun run{.engine = site.engine, .squad = site.squad, .goalVoxels = site.goalVoxels, .script = {}, .ticks = 0};
  run.script.push_back({.tick = LAB_ORDER_TICK,
                        .player = BreachRun::PLAYER,
                        .command = Protocol::MoveSquad{.squad = run.squad, .goalVoxels = run.goalVoxels}});

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
