#include "pch.h"

#include "FlatWorld.h"
#include "Formation.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Squads moving (M2-F5; ADR-015, decision 6): figures follow their squad's path, in Core::Fixed, at
// foot's 1.4 m/s, halved on rubble, standing at their cell's height. 14 m at 0.07 m a tick is 200
// ticks; Fixed's floor makes it 201. Since M4-C2 (ADR-022) they keep one to a cell, in ranks where
// the ground allows and in single file where it does not.

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
constexpr std::uint8_t EAST = 2; // Simulation::FACINGS' +x

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

/// The kind each class's speed is UNIT's for (ADR-023, decision 1): a spearman, an outrider or a ram.
Simulation::UnitKind KindOf(Simulation::NavigationClass _class)
{
  switch (_class)
  {
  case Simulation::NavigationClass::Cavalry:
    return Simulation::UnitKind::Outrider;
  case Simulation::NavigationClass::Machines:
    return Simulation::UnitKind::Ram;
  default:
    return Simulation::UnitKind::Spearman;
  }
}

/// A squad, foot unless _class says otherwise, standing in _cells, the lead first, ordered to _to.
Protocol::EntityId Order(Simulation::World& _world, std::span<const Protocol::VoxelPosition> _cells, Protocol::VoxelPosition _to,
                         Simulation::NavigationClass _class = Simulation::NavigationClass::Foot)
{
  const Protocol::EntityId squad = _world.AddSquad(PLAYER, KindOf(_class), _cells);
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::MoveSquad{.squad = squad, .goalVoxels = _to}, writer);
  _world.Submit(PLAYER, writer.Bytes());
  return squad;
}

/// Orders _squad to hold where it stands (ADR-023, decision 4).
void Hold(Simulation::World& _world, Protocol::EntityId _squad)
{
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::HoldSquad{.squad = _squad}, writer);
  _world.Submit(PLAYER, writer.Bytes());
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
  std::uint32_t ticks = 0;
  bool arrived = false;
  while (!arrived && ticks < 6 * PLAIN_TICKS)
  {
    _world.Tick();
    ++ticks;
    arrived = Lead(_world, squad).position == goal;
  }
  Assert::IsTrue(arrived);
  return ticks;
}

bool InOrder(Core::Fixed _behind, Core::Fixed _ahead)
{
  return _behind <= _ahead;
}

/// A 1 m corridor along z = _z from x _west to x _east, walled 3 m high at z - 1 and z + 1, and closed
/// at x _west - 1 and x _east + 1 when _closed.
void Corridor(Simulation::World& _world, std::int32_t _west, std::int32_t _east, std::int32_t _z, bool _closed)
{
  const std::int32_t top = GROUND + 2;
  Fill(_world, Cell(_west - 1, _z - 1), {.xVoxels = _east + 1, .yVoxels = top, .zVoxels = _z - 1}, MASONRY);
  Fill(_world, Cell(_west - 1, _z + 1), {.xVoxels = _east + 1, .yVoxels = top, .zVoxels = _z + 1}, MASONRY);
  if (_closed)
  {
    Fill(_world, Cell(_west - 1, _z), {.xVoxels = _west - 1, .yVoxels = top, .zVoxels = _z}, MASONRY);
    Fill(_world, Cell(_east + 1, _z), {.xVoxels = _east + 1, .yVoxels = top, .zVoxels = _z}, MASONRY);
  }
}

/// Whether two units stand in one cell.
bool Shared(const Simulation::World& _world)
{
  std::set<std::pair<std::int32_t, std::int32_t>> cells;
  for (const auto& [id, unit] : _world.Units())
  {
    if (!cells.emplace(unit.cellVoxels.xVoxels, unit.cellVoxels.zVoxels).second)
    {
      return true;
    }
  }
  return false;
}

/// Ticks _world until it is quiet, at most _ticks, asserting on every tick that no two units share a cell.
void Settle(Simulation::World& _world, std::uint32_t _ticks)
{
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    _world.Tick();
    Assert::IsFalse(Shared(_world));
    if (_world.Quiet())
    {
      return;
    }
  }
  Assert::Fail(L"the world did not come to rest");
}

/// Whether each figure of _squad stands in the cell _cells gives its place.
bool StandsIn(const Simulation::World& _world, Protocol::EntityId _squad, std::span<const Protocol::VoxelPosition> _cells)
{
  const std::vector<Protocol::EntityId>& units = _world.Squads().at(_squad).units;
  if (units.size() != _cells.size())
  {
    return false;
  }
  for (std::size_t place = 0; place < units.size(); ++place)
  {
    if (_world.Units().at(units[place]).cellVoxels != _cells[place])
    {
      return false;
    }
  }
  return true;
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
    // Three figures in a row in a 1 m corridor, where no slot beside the lead can be used (ADR-022,
    // decision 4), the lead at the east end, walk 10 m east: on every tick none passes the one ahead,
    // and at the end they stand in the last three cells of the path.
    Simulation::World world(FlatWorld::Desc());
    Corridor(world, 5, 30, 30, false);
    world.Tick();
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

  TEST_METHOD(ArrivesInTwoRanksAboutItsGoalOnOpenGround)
  {
    // Six spearmen in their slots facing east walk 19 m east, keeping one to a cell, and stand in the
    // same slots about the goal, still facing east (ADR-022, decisions 4 and 5).
    Simulation::World world(FlatWorld::Desc());
    const std::vector<Protocol::VoxelPosition> start = Simulation::FormationSlots(Cell(11, 30), EAST, 6, 2);
    const Protocol::EntityId squad = Order(world, start, Cell(30, 30));
    Settle(world, 3 * PLAIN_TICKS);
    Assert::IsTrue(StandsIn(world, squad, Simulation::FormationSlots(Cell(30, 30), EAST, 6, 2)));
    Assert::AreEqual(EAST, world.Squads().at(squad).facing);
  }

  TEST_METHOD(PassesAOneMeterGapInSingleFileAndReforms)
  {
    // A masonry wall 2 m thick and 3 m high across the map at x 20 and 21, open only at z 30. Six
    // spearmen in two ranks facing east walk through it to (32, 30): every figure inside the wall's
    // thickness is in the gap, so they pass one behind another, and they re-form about the goal.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, Cell(20, 0), {.xVoxels = 21, .yVoxels = GROUND + 2, .zVoxels = 29}, MASONRY);
    Fill(world, Cell(20, 31), {.xVoxels = 21, .yVoxels = GROUND + 2, .zVoxels = WIDTH - 1}, MASONRY);
    world.Tick();
    const std::vector<Protocol::VoxelPosition> start = Simulation::FormationSlots(Cell(14, 30), EAST, 6, 2);
    const Protocol::EntityId squad = Order(world, start, Cell(32, 30));
    std::set<Protocol::EntityId> through;
    bool quiet = false;
    for (std::uint32_t tick = 0; tick < 4 * PLAIN_TICKS && !quiet; ++tick)
    {
      world.Tick();
      Assert::IsFalse(Shared(world));
      for (const Protocol::EntityId id : world.Squads().at(squad).units)
      {
        const Protocol::VoxelPosition cell = world.Units().at(id).cellVoxels;
        if (cell.xVoxels == 20 || cell.xVoxels == 21)
        {
          Assert::AreEqual(30, cell.zVoxels);
          through.insert(id);
        }
      }
      quiet = world.Quiet();
    }
    Assert::IsTrue(quiet);
    Assert::AreEqual(std::size_t{6}, through.size());
    Assert::IsTrue(StandsIn(world, squad, Simulation::FormationSlots(Cell(32, 30), EAST, 6, 2)));
  }

  TEST_METHOD(PassesASquadMeetingItHeadOnInAOneMeterCorridor)
  {
    // Two squads of three of one player, at either end of a 1 m corridor closed at both, each ordered
    // to the other's end. Where they meet, figures that want each other's cells swap (ADR-022,
    // decision 3), and both arrive in single file, no two in one cell on any tick.
    Simulation::World world(FlatWorld::Desc());
    Corridor(world, 12, 38, 30, true);
    world.Tick();
    const std::array<Protocol::VoxelPosition, 3> west = {Cell(14, 30), Cell(13, 30), Cell(12, 30)};
    const std::array<Protocol::VoxelPosition, 3> east = {Cell(36, 30), Cell(37, 30), Cell(38, 30)};
    const Protocol::EntityId eastward = Order(world, west, Cell(38, 30));
    const Protocol::EntityId westward = Order(world, east, Cell(12, 30));
    Settle(world, 6 * PLAIN_TICKS);
    const std::array<Protocol::VoxelPosition, 3> eastEnd = {Cell(38, 30), Cell(37, 30), Cell(36, 30)};
    const std::array<Protocol::VoxelPosition, 3> westEnd = {Cell(12, 30), Cell(13, 30), Cell(14, 30)};
    Assert::IsTrue(StandsIn(world, eastward, eastEnd));
    Assert::IsTrue(StandsIn(world, westward, westEnd));
  }

  TEST_METHOD(TradesCellsWithAnIdleFigureInAOneMeterCorridor)
  {
    // A figure of another squad of the same player stands idle in a 1 m corridor, wanting no cell. A
    // squad of three walking through trades cells with it, each figure in turn, as two that want each
    // other's cells do, and arrives; the idle figure, passed back one cell a figure, stands three cells
    // west of where it stood, idle still (ADR-022, decision 3, amended). Before, the walkers stood
    // behind it for good: their new searches found the same corridor.
    Simulation::World world(FlatWorld::Desc());
    Corridor(world, 5, 35, 30, true);
    world.Tick();
    const std::array<Protocol::VoxelPosition, 1> idle = {Cell(20, 30)};
    const Protocol::EntityId standing = world.AddSquad(PLAYER, Simulation::UnitKind::Spearman, idle);
    const std::array<Protocol::VoxelPosition, 3> cells = {Cell(12, 30), Cell(11, 30), Cell(10, 30)};
    const Protocol::EntityId walking = Order(world, cells, Cell(30, 30));
    Settle(world, 6 * PLAIN_TICKS);
    const std::array<Protocol::VoxelPosition, 3> end = {Cell(30, 30), Cell(29, 30), Cell(28, 30)};
    Assert::IsTrue(StandsIn(world, walking, end));
    Assert::IsTrue(Lead(world, standing).cellVoxels == Cell(17, 30));
    Assert::IsTrue(world.Squads().at(standing).order == Protocol::SquadOrder::Idle);
  }

  TEST_METHOD(AFigureInAMachinesWayStepsAsideForIt)
  {
    // A machine never trades cells, so a figure of its own side standing idle in its 3 m lane steps
    // aside, out of the machine's cells, and the machine arrives (ADR-022, decision 3, amended). Before,
    // the machine stood behind it for good.
    Simulation::World world(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 1> idle = {Cell(18, 21)};
    const Protocol::EntityId standing = world.AddSquad(PLAYER, Simulation::UnitKind::Spearman, idle);
    const std::array<Protocol::VoxelPosition, 1> cells = {Cell(10, 20)};
    const Protocol::EntityId machine = Order(world, cells, Cell(24, 20), Simulation::NavigationClass::Machines);
    Settle(world, 3 * 401);
    Assert::IsTrue(Lead(world, machine).cellVoxels == Cell(24, 20));
    const Protocol::VoxelPosition aside = Lead(world, standing).cellVoxels;
    Assert::IsTrue(aside != Cell(18, 21));
    Assert::IsTrue(aside.zVoxels < 20 || aside.zVoxels > 22);
  }

  TEST_METHOD(SearchesAgainWhenBlockedForWaitTicksBeforeSearch)
  {
    // A figure of another squad, holding, stands on a squad of one's way, so it trades cells with
    // nobody. The walker waits behind it, and on the tick it has waited waitTicksBeforeSearch ticks in
    // a row its squad searches again (ADR-022, decision 6), its count starting over.
    Simulation::World world(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 1> blocker = {Cell(15, 30)};
    Hold(world, world.AddSquad(PLAYER, Simulation::UnitKind::Spearman, blocker));
    const std::array<Protocol::VoxelPosition, 1> cells = {Cell(10, 30)};
    const Protocol::EntityId squad = Order(world, cells, Cell(20, 30));
    const auto searching = [&world, squad]
    { return std::ranges::any_of(world.Searches(), [squad](const auto& _search) { return _search.first.second == squad; }); };
    const auto patience = static_cast<std::uint32_t>(world.Data().Tuning().waitBeforeSearchTicks);
    Assert::AreEqual(40U, patience);
    for (std::uint32_t tick = 0; tick < 2 * PLAIN_TICKS && Lead(world, squad).waitTicks == 0; ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(Lead(world, squad).cellVoxels == Cell(14, 30));
    for (std::uint32_t waited = 1; waited < patience; ++waited)
    {
      Assert::AreEqual(waited, Lead(world, squad).waitTicks);
      Assert::IsFalse(searching());
      world.Tick();
    }
    Assert::IsTrue(searching());
    Assert::AreEqual(0U, Lead(world, squad).waitTicks);
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
