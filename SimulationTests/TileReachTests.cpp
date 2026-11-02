#include "pch.h"

#include "BreachRun.h"
#include "FlatWorld.h"
#include "Generator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Reach in a tile (M3-B6; ADR-020, decision 5), what the passability view draws, on the breach run's
// site after its first tick. The breach is opened here by edits, a gap the full height of W2 through
// both its layers, so that the reading is tested without running the shots; BreachRunTests runs them.

namespace SimulationTests
{

namespace
{

using Run = Simulation::BreachRun;
constexpr std::int32_t TILE = Simulation::NavigationGrid::TILE_EDGE_CELLS;
constexpr std::int32_t WIDTH = Simulation::Generator::MAP_WIDTH_VOXELS;
constexpr std::int32_t GAP_WEST = 379; // in W2, x 377 to 384
constexpr std::int32_t GAP_EAST = 383;
constexpr std::int32_t W2_WEST = 377; // to 383 in the courtyard's tile, and 384 in the next
constexpr std::int32_t W2_TILE_EAST = 383;
constexpr std::int32_t WALL_TOP = 40; // above every template's battlements

/// The breach run's world, built and ticked once, before any shot.
struct Site
{
  Simulation::World::Desc desc;
  std::unique_ptr<Simulation::World> world;
  Run run;
};

Site BuildSite()
{
  Site site{.desc = Simulation::BreachRunDesc(BakedServerData(), FlatWorld::BUILD_ID), .world = nullptr, .run = {}};
  site.world = std::make_unique<Simulation::World>(site.desc);
  site.run = Simulation::BuildBreachRun(*site.world);
  site.world->Tick();
  return site;
}

const Site& BreachSite()
{
  static const Site SITE = BuildSite();
  return SITE;
}

/// Where the squad's lead stands: the start the passability view reads reach from.
Protocol::VoxelPosition Start(const Simulation::World& _world, const Site& _site)
{
  return _world.Units().at(_world.Squads().at(_site.run.squad).units.front()).cellVoxels;
}

std::int64_t Index(Protocol::VoxelPosition _cell)
{
  return std::int64_t{_cell.zVoxels} * WIDTH + _cell.xVoxels;
}

/// Opens the gap between ticks.
void OpenTheGap(Simulation::World& _world)
{
  for (std::int32_t y = Run::GROUND_VOXELS; y <= WALL_TOP; ++y)
  {
    for (std::int32_t z = Run::WALL_SOUTH_VOXELS; z <= Run::WALL_SOUTH_VOXELS + 1; ++z)
    {
      for (std::int32_t x = GAP_WEST; x <= GAP_EAST; ++x)
      {
        _world.Voxels().Edit({.xVoxels = x, .yVoxels = y, .zVoxels = z}, Protocol::Chunk::EMPTY);
      }
    }
  }
}

/// A tile's reading for foot from the lead's cell, asked before a tick and answered at its end.
Protocol::TileReachReading Ask(Simulation::World& _world, const Site& _site, std::int32_t _tileX, std::int32_t _tileZ)
{
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::QueryRequest{.id = 1,
                                          .query = Protocol::TileReach{.navigationClass = Protocol::NavigationClass::Foot,
                                                                       .startCellVoxels = Start(_world, _site),
                                                                       .tileX = _tileX,
                                                                       .tileZ = _tileZ}},
                   writer);
  _world.Ask(Run::PLAYER, writer.Bytes());
  _world.Tick();
  const std::vector<std::vector<std::byte>> answers = _world.TakeAnswers();
  Assert::AreEqual(std::size_t{1}, answers.size());
  Core::ByteReader reader(answers.front());
  return std::get<Protocol::TileReachReading>(Protocol::DecodeAnswer(reader).reading);
}

bool Bit(const std::array<std::uint8_t, Protocol::TILE_REACH_BYTES>& _bits, std::int32_t _x, std::int32_t _z)
{
  const std::size_t bit = (static_cast<std::size_t>(_z % TILE) * TILE) + static_cast<std::size_t>(_x % TILE);
  return (_bits.at(bit / 8) & (1U << (bit % 8))) != 0;
}

/// Every cell of the tile reads reached exactly when the reach the squads move by returns the cell.
void AgreesWithNearestReachable(const Simulation::World& _world, const Site& _site, const Protocol::TileReachReading& _reading)
{
  const std::int64_t start = Index(Start(_world, _site));
  for (std::int32_t z = _reading.tileZ * TILE; z < (_reading.tileZ + 1) * TILE; ++z)
  {
    for (std::int32_t x = _reading.tileX * TILE; x < (_reading.tileX + 1) * TILE; ++x)
    {
      const std::int64_t cell = std::int64_t{z} * WIDTH + x;
      const bool nearest = _world.Navigation().NearestReachable(start, cell, Simulation::NavigationClass::Foot) == cell;
      Assert::AreEqual(nearest, Bit(_reading.reached, x, z));
      Assert::IsTrue(!Bit(_reading.reached, x, z) || Bit(_reading.usable, x, z));
      // Its surface is navigation's own, where the passability view draws the cell.
      const std::int16_t height = _world.Navigation().WalkableHeight(x, z);
      const std::int32_t inTile = ((z % TILE) * TILE) + (x % TILE);
      const auto surface = _reading.surfaceVoxels.at(static_cast<std::size_t>(inTile));
      Assert::AreEqual(height == Simulation::NavigationGrid::NOT_WALKABLE ? Protocol::NO_SURFACE_VOXELS : height, surface);
    }
  }
}

} // namespace

TEST_CLASS(TileReachTests)
{
public:
  TEST_METHOD(ReadsTheCourtyardUsableAndOutOfReachBeforeTheBreach)
  {
    const Site& site = BreachSite();
    Simulation::World world = *site.world;
    const Protocol::VoxelPosition goal = site.run.goalVoxels;
    const Protocol::TileReachReading reading = Ask(world, site, goal.xVoxels / TILE, goal.zVoxels / TILE);
    Assert::AreEqual(goal.xVoxels / TILE, reading.tileX);
    Assert::AreEqual(goal.zVoxels / TILE, reading.tileZ);
    Assert::IsTrue(Bit(reading.usable, goal.xVoxels, goal.zVoxels));
    Assert::IsFalse(Bit(reading.reached, goal.xVoxels, goal.zVoxels));
    // The lead stands in the same tile, south of the wall, and reaches its own cell; no cell of W2's
    // footprint can be entered.
    const Protocol::VoxelPosition start = Start(world, site);
    Assert::AreEqual(goal.zVoxels / TILE, start.zVoxels / TILE);
    Assert::IsTrue(Bit(reading.reached, start.xVoxels, start.zVoxels));
    for (std::int32_t x = W2_WEST; x <= W2_TILE_EAST; ++x)
    {
      Assert::IsFalse(Bit(reading.usable, x, Run::WALL_SOUTH_VOXELS));
      Assert::IsFalse(Bit(reading.usable, x, Run::WALL_SOUTH_VOXELS + 1));
    }
  }

  TEST_METHOD(ReadsTheCourtyardReachedThroughW2AfterTheBreach)
  {
    const Site& site = BreachSite();
    Simulation::World world = *site.world;
    OpenTheGap(world);
    const Protocol::VoxelPosition goal = site.run.goalVoxels;
    const Protocol::TileReachReading reading = Ask(world, site, goal.xVoxels / TILE, goal.zVoxels / TILE);
    Assert::IsTrue(Bit(reading.reached, goal.xVoxels, goal.zVoxels));
    // The route of boxes crosses W2's footprint: through both its layers, inside the gap and nowhere
    // else along it.
    for (std::int32_t z = Run::WALL_SOUTH_VOXELS; z <= Run::WALL_SOUTH_VOXELS + 1; ++z)
    {
      for (std::int32_t x = W2_WEST; x <= W2_TILE_EAST; ++x)
      {
        Assert::AreEqual(x >= GAP_WEST && x <= GAP_EAST, Bit(reading.reached, x, z));
      }
    }
  }

  TEST_METHOD(AgreesWithTheReachSquadsMoveByOnThreeTiles)
  {
    // Before the breach, where reach splits at the wall: the courtyard's tile, which the squad starts
    // in; the tile east of it, with the enclosure's south-east corner; and the tile north of it, with
    // the north wall. After it, the courtyard's tile again.
    const Site& site = BreachSite();
    const std::int32_t tileX = site.run.goalVoxels.xVoxels / TILE;
    const std::int32_t tileZ = site.run.goalVoxels.zVoxels / TILE;
    Simulation::World intact = *site.world;
    for (const auto& [x, z] :
         std::array<std::pair<std::int32_t, std::int32_t>, 3>{{{tileX, tileZ}, {tileX + 1, tileZ}, {tileX, tileZ + 1}}})
    {
      AgreesWithNearestReachable(intact, site, Ask(intact, site, x, z));
    }
    Simulation::World breached = *site.world;
    OpenTheGap(breached);
    AgreesWithNearestReachable(breached, site, Ask(breached, site, tileX, tileZ));
  }

  TEST_METHOD(RefusesATileOffTheGrid)
  {
    const Site& site = BreachSite();
    const Simulation::NavigationGrid& grid = site.world->Navigation();
    const std::int64_t start = Index(Start(*site.world, site));
    Assert::ExpectException<std::out_of_range>(
      [&grid, start] { static_cast<void>(grid.ReachInTile(start, grid.TilesAcross(), 0, Simulation::NavigationClass::Foot)); });
    Assert::ExpectException<std::out_of_range>([&grid, start]
                                               { static_cast<void>(grid.ReachInTile(start, 0, -1, Simulation::NavigationClass::Foot)); });
    Assert::ExpectException<std::out_of_range>([&grid]
                                               { static_cast<void>(grid.ReachInTile(-1, 0, 0, Simulation::NavigationClass::Foot)); });
  }

  TEST_METHOD(AnswersATileOffTheGridEmpty)
  {
    // A query is the client's, about what is on its screen: a tile off the grid reads empty, and the
    // tick goes on.
    const Site& site = BreachSite();
    Simulation::World world = *site.world;
    const Protocol::TileReachReading reading = Ask(world, site, world.Navigation().TilesAcross(), 0);
    Assert::AreEqual(world.Navigation().TilesAcross(), reading.tileX);
    Assert::IsTrue(reading.usable == std::array<std::uint8_t, Protocol::TILE_REACH_BYTES>{});
    Assert::IsTrue(reading.reached == std::array<std::uint8_t, Protocol::TILE_REACH_BYTES>{});
  }
};

} // namespace SimulationTests
