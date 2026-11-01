#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The path search (M2-F3; ADR-015, decision 5), on the flat world, 64 m square, for foot soldiers.
// Edits made between ticks reach the navigation grid in step 8, after step 3's searches, so each
// test that builds lets a tick pass before it orders a squad.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 64;

Protocol::VoxelPosition Cell(std::int32_t _x, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = GROUND, .zVoxels = _z};
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

/// A foot squad of one standing at _from, ordered to _to; the world runs until its search is done.
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

/// A path's cost, as the search counts it.
std::int64_t Cost(const Simulation::World& _world, const std::vector<std::int64_t>& _path)
{
  std::int64_t cost = 0;
  for (std::size_t step = 1; step < _path.size(); ++step)
  {
    const auto x = static_cast<std::int32_t>(_path[step] % WIDTH);
    const auto z = static_cast<std::int32_t>(_path[step] / WIDTH);
    const bool diagonal = _path[step] % WIDTH != _path[step - 1] % WIDTH && _path[step] / WIDTH != _path[step - 1] / WIDTH;
    const std::int64_t factor = _world.Navigation().GroundAt(x, z) == Simulation::NavigationGrid::Ground::Rubble ? 2 : 1;
    cost += (diagonal ? 14 : 10) * factor;
  }
  return cost;
}

std::int64_t Index(std::int32_t _x, std::int32_t _z)
{
  return std::int64_t{_z} * WIDTH + _x;
}

} // namespace

TEST_CLASS(SearchTests)
{
public:
  TEST_METHOD(FindsTheLeastCostOnOpenGround)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId squad = Order(world, Cell(5, 5), Cell(40, 30));
    RunUntilFound(world);
    const Simulation::Squad& found = world.Squads().at(squad);
    Assert::IsFalse(found.shortOfGoal);
    Assert::AreEqual(Index(5, 5), found.path.front());
    Assert::AreEqual(Index(40, 30), found.path.back());
    Assert::AreEqual(std::int64_t{10 * 35 + 4 * 25}, Cost(world, found.path)); // the octile distance
  }

  TEST_METHOD(DetoursRoundRubbleWhenThatIsCheaper)
  {
    // A band of rubble 8 m wide, from z 6 to 14, between the start and the goal: crossing it costs
    // 80 more, going round its end at z 15 only 40 more, so the path goes round.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, {.xVoxels = 18, .yVoxels = GROUND - 1, .zVoxels = 6}, {.xVoxels = 25, .yVoxels = GROUND - 1, .zVoxels = 14}, STONE_RUBBLE);
    world.Tick();
    const Protocol::EntityId around = Order(world, Cell(5, 10), Cell(35, 10));
    RunUntilFound(world);
    for (const std::int64_t cell : world.Squads().at(around).path)
    {
      Assert::IsTrue(world.Navigation().GroundAt(static_cast<std::int32_t>(cell % WIDTH), static_cast<std::int32_t>(cell / WIDTH)) !=
                     Simulation::NavigationGrid::Ground::Rubble);
    }
    Assert::AreEqual(std::int64_t{340}, Cost(world, world.Squads().at(around).path));

    // A band across the whole map is cheaper to cross than to go round.
    Simulation::World across(FlatWorld::Desc());
    Fill(across, {.xVoxels = 18, .yVoxels = GROUND - 1, .zVoxels = 0}, {.xVoxels = 25, .yVoxels = GROUND - 1, .zVoxels = WIDTH - 1},
         STONE_RUBBLE);
    across.Tick();
    const Protocol::EntityId through = Order(across, Cell(5, 10), Cell(35, 10));
    RunUntilFound(across);
    Assert::AreEqual(std::int64_t{300 + 80}, Cost(across, across.Squads().at(through).path));
  }

  TEST_METHOD(GivesEqualPathsTheSameWay)
  {
    std::array<std::vector<std::int64_t>, 2> paths;
    for (std::vector<std::int64_t>& path : paths)
    {
      Simulation::World world(FlatWorld::Desc());
      const Protocol::EntityId squad = Order(world, Cell(10, 10), Cell(30, 50));
      RunUntilFound(world);
      path = world.Squads().at(squad).path;
    }
    Assert::IsTrue(paths[0] == paths[1]);
  }

  TEST_METHOD(GivesTheSamePathSplitOverTicks)
  {
    Simulation::World whole(FlatWorld::Desc());
    Fill(whole, Cell(30, 10), {.xVoxels = 31, .yVoxels = GROUND + 3, .zVoxels = 55}, MASONRY);
    whole.Tick();
    const Protocol::EntityId squad = Order(whole, Cell(3, 60), Cell(60, 4));
    RunUntilFound(whole);

    const Simulation::ServerData small = BakedServerDataWith("pathNodesPerTick", 7);
    Simulation::World split(FlatWorld::Desc(2, 2, &small));
    Fill(split, Cell(30, 10), {.xVoxels = 31, .yVoxels = GROUND + 3, .zVoxels = 55}, MASONRY);
    split.Tick();
    static_cast<void>(Order(split, Cell(3, 60), Cell(60, 4)));
    split.Tick();
    Assert::IsFalse(split.Searches().empty());
    RunUntilFound(split);
    Assert::IsTrue(whole.Squads().at(squad).path == split.Squads().at(squad).path);
  }

  TEST_METHOD(StopsAtTheNearestReachableCellOfAnUnreachableGoal)
  {
    // The goal stands in a closed masonry box; the nearest cell outside, by octile distance and then
    // the lower index, is where the path ends.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, Cell(40, 40), {.xVoxels = 46, .yVoxels = GROUND + 3, .zVoxels = 46}, MASONRY);
    Fill(world, Cell(41, 41), {.xVoxels = 45, .yVoxels = GROUND + 3, .zVoxels = 45}, 0);
    world.Tick();
    const Protocol::EntityId squad = Order(world, Cell(10, 43), Cell(43, 43));
    RunUntilFound(world);
    const Simulation::Squad& found = world.Squads().at(squad);
    Assert::IsTrue(found.shortOfGoal);
    // West of the box, (39, 43), and south of it, (43, 39), are both 4 m off; the south one has the
    // lower index.
    Assert::AreEqual(Index(43, 39), found.path.back());
  }

  TEST_METHOD(ContinuesASearchFromASave)
  {
    const Simulation::ServerData small = BakedServerDataWith("pathNodesPerTick", 5);
    Simulation::World straight(FlatWorld::Desc(2, 2, &small));
    const Protocol::EntityId squad = Order(straight, Cell(3, 3), Cell(50, 40));
    for (std::uint32_t tick = 0; tick < 4; ++tick)
    {
      straight.Tick();
    }
    Assert::IsFalse(straight.Searches().empty());
    Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(straight), FlatWorld::Desc(2, 2, &small));
    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(loaded));
    RunUntilFound(straight);
    RunUntilFound(loaded);
    Assert::IsTrue(straight.Squads().at(squad).path == loaded.Squads().at(squad).path);
    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(loaded));
  }
};

} // namespace SimulationTests
