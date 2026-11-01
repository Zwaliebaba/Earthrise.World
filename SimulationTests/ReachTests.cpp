#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Reach, and searching again (M2-F4; ADR-015, decision 5), on the flat world for foot soldiers.
// Edits made between ticks reach the navigation grid in step 8, after step 3's searches, so each
// test that builds lets a tick pass before it orders a squad.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 64;
constexpr std::int32_t WALL_HIGH = GROUND + 3;

Protocol::VoxelPosition Cell(std::int32_t _x, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = GROUND, .zVoxels = _z};
}

std::int64_t Index(std::int32_t _x, std::int32_t _z, std::int32_t _width = WIDTH)
{
  return std::int64_t{_z} * _width + _x;
}

void Fill(Simulation::World& _world, Protocol::VoxelPosition _low, Protocol::VoxelPosition _high, std::uint8_t _index)
{
  for (std::int32_t y = _low.yVoxels; y <= _high.yVoxels; ++y)
  {
    for (std::int32_t z = _low.zVoxels; z <= _high.zVoxels; ++z)
    {
      for (std::int32_t x = _low.xVoxels; x <= _high.xVoxels; ++x)
      {
        _world.Voxels().Edit({.xVoxels = x, .yVoxels = y, .zVoxels = z}, _index);
      }
    }
  }
}

/// A foot squad of one standing at _from, ordered to _to.
Protocol::EntityId Order(Simulation::World& _world, Protocol::VoxelPosition _from, Protocol::VoxelPosition _to)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {_from};
  const Protocol::EntityId squad = _world.AddSquad(PLAYER, Simulation::NavigationClass::Foot, cells);
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::MoveSquad{.squad = squad, .goalVoxels = _to}, writer);
  _world.Submit(PLAYER, writer.Bytes());
  return squad;
}

void RunUntilFound(Simulation::World& _world)
{
  _world.Tick();
  for (std::uint32_t tick = 0; tick < 2000 && !_world.Searches().empty(); ++tick)
  {
    _world.Tick();
  }
  Assert::IsTrue(_world.Searches().empty());
}

bool Searching(const Simulation::World& _world, Protocol::EntityId _squad)
{
  return std::ranges::any_of(_world.Searches(), [_squad](const auto& _search) { return _search.first.second == _squad; });
}

/// A masonry wall 4 m high across the whole 64 m world, at z 30 and 31, and a squad south of it
/// ordered to (10, 50), north of it.
Protocol::EntityId OrderPastTheWall(Simulation::World& _world)
{
  Fill(_world, Cell(0, 30), {.xVoxels = WIDTH - 1, .yVoxels = WALL_HIGH, .zVoxels = 31}, MASONRY);
  _world.Tick();
  const Protocol::EntityId squad = Order(_world, Cell(10, 10), Cell(10, 50));
  RunUntilFound(_world);
  return squad;
}

} // namespace

TEST_CLASS(ReachTests)
{
public:
  TEST_METHOD(StopsShortOfAnIntactWall)
  {
    // Every cell north of the wall is out of reach; the nearest south of it is straight below the
    // goal, against the wall.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId squad = OrderPastTheWall(world);
    const Simulation::Squad& found = world.Squads().at(squad);
    Assert::IsTrue(found.shortOfGoal);
    Assert::AreEqual(Index(10, 29), found.path.back());
  }

  TEST_METHOD(SearchesAgainThroughTheBreach)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId squad = OrderPastTheWall(world);
    // The section at x 20 and 21 goes; the tick that rebuilds its tiles marks the squad, short of
    // its goal, to search again, and the next tick's search finds the gap.
    Fill(world, Cell(20, 30), {.xVoxels = 21, .yVoxels = WALL_HIGH, .zVoxels = 31}, 0);
    world.Tick();
    Assert::IsTrue(Searching(world, squad));
    RunUntilFound(world);
    const Simulation::Squad& found = world.Squads().at(squad);
    Assert::IsFalse(found.shortOfGoal);
    Assert::AreEqual(Index(10, 50), found.path.back());
    Assert::IsTrue(std::ranges::any_of(found.path,
                                       [](std::int64_t _cell)
                                       {
                                         const std::int64_t x = _cell % WIDTH;
                                         const std::int64_t z = _cell / WIDTH;
                                         return (x == 20 || x == 21) && (z == 30 || z == 31);
                                       }));
  }

  TEST_METHOD(SearchesAgainWhenItsPathCrossesARebuiltTile)
  {
    // Two squads on a world four tiles across: one walks the diagonal from (5, 5) to (25, 25) in the
    // first tile, the other from (100, 5) to (120, 25) in the last. A pillar on the first's path
    // rebuilds only the first tile, so only the first searches again, and goes round it.
    constexpr std::int32_t WIDE = 128;
    Simulation::World world(FlatWorld::Desc(4, 2));
    const Protocol::EntityId near = Order(world, Cell(5, 5), Cell(25, 25));
    const Protocol::EntityId far = Order(world, Cell(100, 5), Cell(120, 25));
    RunUntilFound(world);
    Assert::IsTrue(std::ranges::contains(world.Squads().at(near).path, Index(15, 15, WIDE)));
    const std::vector<std::int64_t> farPath = world.Squads().at(far).path;

    Fill(world, Cell(15, 15), {.xVoxels = 15, .yVoxels = WALL_HIGH, .zVoxels = 15}, MASONRY);
    world.Tick();
    Assert::IsTrue(Searching(world, near));
    Assert::IsFalse(Searching(world, far));
    RunUntilFound(world);
    const Simulation::Squad& found = world.Squads().at(near);
    Assert::IsFalse(found.shortOfGoal);
    Assert::AreEqual(Index(25, 25, WIDE), found.path.back());
    Assert::IsFalse(std::ranges::contains(found.path, Index(15, 15, WIDE)));
    Assert::IsTrue(farPath == world.Squads().at(far).path);
  }

  TEST_METHOD(StartsASearchNotDoneAgainAfterARebuild)
  {
    // At 5 nodes a tick the search is still going when a pillar rises far from it; it starts again,
    // asked for in the tick of the rebuild.
    const Simulation::ServerData small = BakedServerDataWith("pathNodesPerTick", 5);
    Simulation::World world(FlatWorld::Desc(2, 2, &small));
    const Protocol::EntityId squad = Order(world, Cell(3, 3), Cell(50, 40));
    world.Tick();
    world.Tick();
    Assert::IsTrue(Searching(world, squad));
    Assert::AreEqual(0U, world.Searches().begin()->first.first);
    Fill(world, Cell(60, 60), {.xVoxels = 60, .yVoxels = WALL_HIGH, .zVoxels = 60}, MASONRY);
    world.Tick();
    Assert::IsTrue(Searching(world, squad));
    Assert::AreEqual(2U, world.Searches().begin()->first.first);
    Assert::AreEqual(std::uint64_t{0}, world.Searches().begin()->second.NodesExpanded());
  }

  TEST_METHOD(ReachesAnUnreachableGoalForFewerNodesThanTheMap)
  {
    // The goal stands in a closed masonry box. A search for the goal itself floods every cell the
    // squad can reach before it gives up; the search the world runs goes straight for the nearest
    // reachable cell, and ends in the same place.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, Cell(40, 40), {.xVoxels = 46, .yVoxels = WALL_HIGH, .zVoxels = 46}, MASONRY);
    Fill(world, Cell(41, 41), {.xVoxels = 45, .yVoxels = WALL_HIGH, .zVoxels = 45}, 0);
    world.Tick();
    const Protocol::EntityId squad = Order(world, Cell(10, 43), Cell(43, 43));
    RunUntilFound(world);

    const Simulation::NavigationGrid& grid = world.Navigation();
    const std::int64_t start = Index(10, 43);
    const std::int64_t goal = Index(43, 43);
    const std::int64_t target = grid.NearestReachable(start, goal, Simulation::NavigationClass::Foot);
    Assert::AreEqual(Index(43, 39), target);
    Simulation::PathSearch flood(squad, Simulation::NavigationClass::Foot, start, goal, goal, WIDTH);
    static_cast<void>(flood.Advance(grid, UINT32_MAX));
    Simulation::PathSearch reach(squad, Simulation::NavigationClass::Foot, start, goal, target, WIDTH);
    static_cast<void>(reach.Advance(grid, UINT32_MAX));
    Assert::IsTrue(flood.ShortOfGoal());
    Assert::IsTrue(reach.ShortOfGoal());
    Assert::AreEqual(flood.Path().back(), reach.Path().back());
    Assert::AreEqual(target, world.Squads().at(squad).path.back());
    Assert::IsTrue(reach.NodesExpanded() * 4 < flood.NodesExpanded());
  }

  TEST_METHOD(KnowsReachAcrossTiles)
  {
    // The wall's gap is in the second tile across, and the start and the goal in the first: reach
    // runs through the components of three tiles.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, Cell(0, 30), {.xVoxels = WIDTH - 1, .yVoxels = WALL_HIGH, .zVoxels = 31}, MASONRY);
    Fill(world, Cell(50, 30), {.xVoxels = 50, .yVoxels = WALL_HIGH, .zVoxels = 31}, 0);
    world.Tick();
    const Protocol::EntityId squad = Order(world, Cell(10, 10), Cell(10, 50));
    RunUntilFound(world);
    Assert::AreEqual(Index(10, 50), world.Navigation().NearestReachable(Index(10, 10), Index(10, 50), Simulation::NavigationClass::Foot));
    Assert::IsFalse(world.Squads().at(squad).shortOfGoal);
    Assert::AreEqual(Index(10, 50), world.Squads().at(squad).path.back());
  }

  TEST_METHOD(RefusesReachTheGridHasOutrun)
  {
    // A grid rebuilt since its reach was brought up to date refuses to answer from stale reach. The
    // world brings it up to date in step 8 (M3-B5), so a copy is rebuilt here by hand.
    Simulation::World world(FlatWorld::Desc());
    Simulation::NavigationGrid grid = world.Navigation();
    Fill(world, Cell(20, 20), {.xVoxels = 20, .yVoxels = WALL_HIGH, .zVoxels = 20}, MASONRY);
    grid.RebuildTile(world.Voxels().Store(), 0, 0);
    Assert::ExpectException<std::logic_error>(
      [&grid] { static_cast<void>(grid.NearestReachable(Index(1, 1), Index(2, 2), Simulation::NavigationClass::Foot)); });
  }

  TEST_METHOD(KeepsTheWorldsReachCurrentAfterEveryTick)
  {
    // Step 8 brings reach up to date after its rebuild (M3-B5), so the queries of step 12 and whoever
    // reads navigation between ticks can ask it.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, Cell(20, 20), {.xVoxels = 20, .yVoxels = WALL_HIGH, .zVoxels = 20}, MASONRY);
    world.Tick();
    Assert::AreEqual(Index(2, 2), world.Navigation().NearestReachable(Index(1, 1), Index(2, 2), Simulation::NavigationClass::Foot));
  }
};

} // namespace SimulationTests
