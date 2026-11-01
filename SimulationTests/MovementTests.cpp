#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Squads moving (M2-F5; ADR-015, decision 6): figures follow their squad's path in single file, in
// Core::Fixed, at foot's 1.4 m/s, halved on rubble, standing at their cell's height. 14 m at
// 0.07 m a tick is 200 ticks; Fixed's floor makes it 201.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 64;
constexpr std::uint32_t PLAIN_TICKS = 200;

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

/// A squad, foot unless _class says otherwise, standing in _cells, the lead first, ordered to _to.
Protocol::EntityId Order(Simulation::World& _world, std::span<const Protocol::VoxelPosition> _cells, Protocol::VoxelPosition _to,
                         Simulation::NavigationClass _class = Simulation::NavigationClass::Foot)
{
  const Protocol::EntityId squad = _world.AddSquad(PLAYER, _class, _cells);
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::MoveSquad{.squad = squad, .goalVoxels = _to}, writer);
  _world.Submit(PLAYER, writer.Bytes());
  return squad;
}

const Simulation::Unit& Lead(const Simulation::World& _world, Protocol::EntityId _squad)
{
  return _world.Units().at(_world.Squads().at(_squad).units.front());
}

/// The ticks a one-figure squad of _class takes from (10, 20) to (24, 20), 14 m east, counting the
/// tick its order arrives in.
std::uint32_t TicksForFourteenMeters(Simulation::World& _world, Simulation::NavigationClass _class = Simulation::NavigationClass::Foot)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {Cell(10, 20)};
  const Protocol::EntityId squad = Order(_world, cells, Cell(24, 20), _class);
  const Simulation::FixedPosition goal = Simulation::CellFloorMiddle(Cell(24, 20));
  for (std::uint32_t ticks = 1; ticks <= 6 * PLAIN_TICKS; ++ticks)
  {
    _world.Tick();
    if (Lead(_world, squad).position == goal)
    {
      return ticks;
    }
  }
  Assert::Fail(L"the squad never arrived");
  return 0;
}

bool InOrder(Core::Fixed _behind, Core::Fixed _ahead)
{
  return _behind <= _ahead;
}

} // namespace

TEST_CLASS(MovementTests)
{
public:
  TEST_METHOD(CrossesFourteenMetersOfOpenGroundIn200Ticks)
  {
    Simulation::World world(FlatWorld::Desc());
    const std::uint32_t ticks = TicksForFourteenMeters(world);
    Assert::IsTrue(ticks + 1 >= PLAIN_TICKS && ticks <= PLAIN_TICKS + 1);
  }

  TEST_METHOD(MovesCavalryAndMachinesAtTheirOwnSpeeds)
  {
    // Cavalry at 2.8 m/s, twice foot's, and machines at 0.7 m/s, half of it (the owner, after M2's
    // report): 14 m in 101 ticks and in 401, each 9,175 and 2,293 raw a tick floored.
    Simulation::World cavalry(FlatWorld::Desc());
    Simulation::World machines(FlatWorld::Desc());
    Assert::AreEqual(101U, TicksForFourteenMeters(cavalry, Simulation::NavigationClass::Cavalry));
    Assert::AreEqual(401U, TicksForFourteenMeters(machines, Simulation::NavigationClass::Machines));
  }

  TEST_METHOD(TakesTwiceAsLongOnRubble)
  {
    // The ground under every cell is stone rubble, so no way round it is cheaper; slump is off, and
    // flat rubble would not slump anyway.
    Simulation::World plain(FlatWorld::Desc());
    Simulation::World rubble(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    Fill(rubble, {.xVoxels = 0, .yVoxels = GROUND - 1, .zVoxels = 0}, {.xVoxels = WIDTH - 1, .yVoxels = GROUND - 1, .zVoxels = WIDTH - 1},
         STONE_RUBBLE);
    rubble.Tick();
    Assert::IsTrue(rubble.Navigation().GroundAt(17, 20) == Simulation::NavigationGrid::Ground::Rubble);
    const std::uint32_t plainTicks = TicksForFourteenMeters(plain);
    const std::uint32_t rubbleTicks = TicksForFourteenMeters(rubble);
    Assert::IsTrue(rubbleTicks + 2 >= 2 * plainTicks && rubbleTicks <= 2 * plainTicks + 2);
  }

  TEST_METHOD(FollowsInSingleFile)
  {
    // Three figures in a row, the lead at the east end, walk 10 m east: on every tick none passes the
    // one ahead, and at the end they stand in the last three cells of the path.
    Simulation::World world(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 3> cells = {Cell(12, 30), Cell(11, 30), Cell(10, 30)};
    const Protocol::EntityId squad = Order(world, cells, Cell(22, 30));
    const std::vector<Protocol::EntityId> units = [&]
    {
      world.Tick();
      return world.Squads().at(squad).units;
    }();
    for (std::uint32_t tick = 0; tick < 2 * PLAIN_TICKS; ++tick)
    {
      world.Tick();
      const Simulation::FixedPosition lead = world.Units().at(units[0]).position;
      const Simulation::FixedPosition second = world.Units().at(units[1]).position;
      const Simulation::FixedPosition third = world.Units().at(units[2]).position;
      Assert::IsTrue(InOrder(third.xMeters, second.xMeters) && InOrder(second.xMeters, lead.xMeters));
    }
    Assert::IsTrue(world.Units().at(units[0]).position == Simulation::CellFloorMiddle(Cell(22, 30)));
    Assert::IsTrue(world.Units().at(units[1]).position == Simulation::CellFloorMiddle(Cell(21, 30)));
    Assert::IsTrue(world.Units().at(units[2]).position == Simulation::CellFloorMiddle(Cell(20, 30)));
  }

  TEST_METHOD(NeverStandsOnAStructure)
  {
    // A masonry wall 4 m high from z 0 to z 40 stands between a squad of three and its goal; they go
    // round its north end. On every tick each figure's cell is open, its ground is walkable ground,
    // and its position lies in its cell.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, Cell(25, 0), {.xVoxels = 26, .yVoxels = GROUND + 3, .zVoxels = 40}, MASONRY);
    world.Tick();
    const std::array<Protocol::VoxelPosition, 3> cells = {Cell(12, 10), Cell(11, 10), Cell(10, 10)};
    const Protocol::EntityId squad = Order(world, cells, Cell(40, 10));
    const Simulation::MaterialRules& materials = world.Data().Materials();
    for (std::uint32_t tick = 0; tick < 6 * PLAIN_TICKS; ++tick)
    {
      world.Tick();
      for (const Protocol::EntityId id : world.Squads().at(squad).units)
      {
        const Simulation::Unit& unit = world.Units().at(id);
        const Protocol::VoxelPosition cell = unit.cellVoxels;
        const std::uint8_t standing = world.Voxels().Store().At(cell);
        const std::uint8_t under =
          world.Voxels().Store().At({.xVoxels = cell.xVoxels, .yVoxels = cell.yVoxels - 1, .zVoxels = cell.zVoxels});
        Assert::IsTrue(standing == 0 || materials.at(standing).liquid);
        Assert::IsTrue(materials.at(under).walkableGround);
        Assert::AreEqual(cell.xVoxels, unit.position.xMeters.FloorWhole());
        Assert::AreEqual(cell.zVoxels, unit.position.zMeters.FloorWhole());
        Assert::AreEqual(cell.yVoxels, unit.position.yMeters.FloorWhole());
      }
    }
    Assert::IsTrue(Lead(world, squad).position == Simulation::CellFloorMiddle(Cell(40, 10)));
  }

  TEST_METHOD(HoldsWhileItsSearchIsNotDone)
  {
    // At 1 node a tick, a new order's search runs for many ticks. The squad, standing at its first
    // goal, starts no step until the path arrives, and then walks on.
    const Simulation::ServerData slow = BakedServerDataWith("pathNodesPerTick", 1);
    Simulation::World world(FlatWorld::Desc(2, 2, &slow));
    const std::array<Protocol::VoxelPosition, 1> cells = {Cell(10, 10)};
    const Protocol::EntityId squad = Order(world, cells, Cell(12, 10));
    for (std::uint32_t tick = 0; tick < 60; ++tick)
    {
      world.Tick();
    }
    const Simulation::FixedPosition first = Simulation::CellFloorMiddle(Cell(12, 10));
    Assert::IsTrue(Lead(world, squad).position == first);
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::MoveSquad{.squad = squad, .goalVoxels = Cell(40, 10)}, writer);
    world.Submit(PLAYER, writer.Bytes());
    std::uint32_t held = 0;
    for (world.Tick(); !world.Searches().empty(); world.Tick())
    {
      Assert::IsTrue(Lead(world, squad).position == first);
      ++held;
    }
    Assert::IsTrue(held > 20);
    world.Tick();
    Assert::IsTrue(Lead(world, squad).position.xMeters > first.xMeters);
  }

  TEST_METHOD(ContinuesAWalkFromASave)
  {
    Simulation::World straight(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 2> cells = {Cell(5, 5), Cell(4, 5)};
    const Protocol::EntityId squad = Order(straight, cells, Cell(50, 40));
    for (std::uint32_t tick = 0; tick < 90; ++tick)
    {
      straight.Tick();
    }
    Assert::IsTrue(Lead(straight, squad).stepEffort > 0);
    Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(straight), FlatWorld::Desc());
    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(loaded));
    for (std::uint32_t tick = 0; tick < 1000; ++tick)
    {
      straight.Tick();
      loaded.Tick();
    }
    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(loaded));
    Assert::IsTrue(Lead(straight, squad).position == Lead(loaded, squad).position);
    Assert::IsTrue(Lead(loaded, squad).position == Simulation::CellFloorMiddle(Cell(50, 40)));
  }
};

} // namespace SimulationTests
