#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The drop step (M2-F6; ADR-015, decision 7): after the navigation rebuild, in unit-id order, a unit
// whose ground is dug away drops, one whose cell rubble fills is lifted, and one whose cell its class
// can no longer use moves to the nearest it can. Slump is off where a pit has to stay a pit.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

void Fill(Simulation::World& _world, Protocol::VoxelPosition _low, Protocol::VoxelPosition _high, std::uint8_t _index)
{
  for (std::int32_t y = _low.yVoxels; y <= _high.yVoxels; ++y)
  {
    for (std::int32_t z = _low.zVoxels; z <= _high.zVoxels; ++z)
    {
      for (std::int32_t x = _low.xVoxels; x <= _high.xVoxels; ++x)
      {
        _world.Voxels().Edit(At(x, y, z), _index);
      }
    }
  }
}

/// A squad of one standing at _cell; returns its unit.
Protocol::EntityId Place(Simulation::World& _world, Simulation::NavigationClass _class, Protocol::VoxelPosition _cell)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {_cell};
  const Simulation::UnitKind kind =
    _class == Simulation::NavigationClass::Cavalry ? Simulation::UnitKind::Outrider : Simulation::UnitKind::Spearman;
  const Protocol::EntityId squad = _world.AddSquad(PLAYER, _class, cells, kind);
  return _world.Squads().at(squad).units.front();
}

std::size_t CountFor(std::span<const Simulation::CrushEvent> _events, Protocol::EntityId _unit)
{
  return static_cast<std::size_t>(
    std::ranges::count_if(_events, [_unit](const Simulation::CrushEvent& _event) { return _event.unit == _unit; }));
}

} // namespace

TEST_CLASS(DropTests)
{
public:
  TEST_METHOD(DropsWhereTheGroundIsDugAway)
  {
    // A pit 4 m deep under one unit and 3 m deep under another: the first drop crushes, the second
    // does not.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const Protocol::EntityId deep = Place(world, Simulation::NavigationClass::Foot, At(20, GROUND, 20));
    const Protocol::EntityId shallow = Place(world, Simulation::NavigationClass::Foot, At(30, GROUND, 30));
    world.Tick();
    Fill(world, At(20, GROUND - 4, 20), At(20, GROUND - 1, 20), 0);
    Fill(world, At(30, GROUND - 3, 30), At(30, GROUND - 1, 30), 0);
    world.Tick();
    Assert::AreEqual(std::size_t{1}, world.LastTickCrushes().size());
    const Simulation::CrushEvent& crush = world.LastTickCrushes().front();
    Assert::AreEqual(deep, crush.unit);
    Assert::AreEqual(Protocol::NO_ENTITY, crush.collapse);
    Assert::AreEqual(4, crush.dropVoxels);
    Assert::AreEqual(GROUND - 4, world.Units().at(deep).cellVoxels.yVoxels);
    Assert::IsTrue(world.Units().at(deep).position == Simulation::CellFloorMiddle(At(20, GROUND - 4, 20)));
    Assert::AreEqual(GROUND - 3, world.Units().at(shallow).cellVoxels.yVoxels);
    Assert::IsTrue(world.Units().at(shallow).position == Simulation::CellFloorMiddle(At(30, GROUND - 3, 30)));
  }

  TEST_METHOD(LiftsAUnitWhoseCellRubbleFillsWithThatCollapse)
  {
    // FallTests' block, falling east off its pier. One unit stands in the strip when the piece
    // leaves; another arrives in the strip after it has left, where the landing comes last. Each is
    // hit by the collapse once, the second on the tick its cell fills and it is lifted.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    Fill(world, At(10, GROUND + 2, 10), At(12, GROUND + 5, 12), MASONRY);
    Fill(world, At(12, GROUND, 10), At(12, GROUND + 1, 12), MASONRY);
    const Protocol::EntityId early = Place(world, Simulation::NavigationClass::Foot, At(14, GROUND, 11));
    world.Tick();
    Fill(world, At(12, GROUND, 10), At(12, GROUND + 1, 12), 0);
    world.Tick();
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    const Protocol::EntityId collapse = world.Collapses().begin()->first;
    std::vector<Simulation::CrushEvent> crushes;
    while (world.Collapses().begin()->second.ticksElapsed <= world.Collapses().begin()->second.plan.warningTicks)
    {
      world.Tick();
      crushes.insert(crushes.end(), world.LastTickCrushes().begin(), world.LastTickCrushes().end());
    }
    Assert::AreEqual(std::size_t{1}, CountFor(crushes, early));

    // The last voxel to land, in a column nobody stands in and nothing has landed in yet.
    const Simulation::CollapsePlan& plan = world.Collapses().begin()->second.plan;
    const auto landing =
      std::ranges::find_if(plan.landing.rbegin(), plan.landing.rend(),
                           [&world](const Simulation::PlacedVoxel& _placed)
                           {
                             return (_placed.position.xVoxels != 14 || _placed.position.zVoxels != 11) &&
                                    world.Voxels().At(At(_placed.position.xVoxels, GROUND, _placed.position.zVoxels)) == 0;
                           });
    Assert::IsTrue(landing != plan.landing.rend());
    const Protocol::VoxelPosition cell = At(landing->position.xVoxels, GROUND, landing->position.zVoxels);
    const Protocol::EntityId late = Place(world, Simulation::NavigationClass::Foot, cell);
    bool lifted = false;
    for (std::uint32_t tick = 0; tick < 200 && !world.Collapses().empty(); ++tick)
    {
      world.Tick();
      crushes.insert(crushes.end(), world.LastTickCrushes().begin(), world.LastTickCrushes().end());
      if (!lifted && world.Units().at(late).cellVoxels.yVoxels > GROUND)
      {
        lifted = true;
        Assert::IsTrue(std::ranges::any_of(world.LastTickCrushes(), [late, collapse](const Simulation::CrushEvent& _event)
                                           { return _event.unit == late && _event.collapse == collapse && _event.dropVoxels == 0; }));
      }
    }
    Assert::IsTrue(lifted);
    Assert::AreEqual(cell.xVoxels, world.Units().at(late).cellVoxels.xVoxels);
    Assert::AreEqual(cell.zVoxels, world.Units().at(late).cellVoxels.zVoxels);
    Assert::AreEqual(std::size_t{1}, CountFor(crushes, late));
    Assert::AreEqual(std::size_t{1}, CountFor(crushes, early));
  }

  TEST_METHOD(MovesOffACellThatLostItsHeadroom)
  {
    // A masonry overhang 2 m up over (20, 20), held by a pillar east of it, leaves foot their 2 m of
    // headroom and cavalry less than their 3. The horseman goes to the nearest cell he can use: west,
    // south and north are all 1 m off, and south, (20, 19), has the lowest index.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId horseman = Place(world, Simulation::NavigationClass::Cavalry, At(20, GROUND, 20));
    const Protocol::EntityId footman = Place(world, Simulation::NavigationClass::Foot, At(20, GROUND, 20));
    world.Tick();
    Fill(world, At(21, GROUND, 20), At(21, GROUND + 2, 20), MASONRY);
    Fill(world, At(20, GROUND + 2, 20), At(20, GROUND + 2, 20), MASONRY);
    world.Tick();
    Assert::IsTrue(world.Collapses().empty());
    Assert::AreEqual(std::uint8_t{2}, world.Navigation().Headroom(20, 20));
    const Protocol::VoxelPosition moved = world.Units().at(horseman).cellVoxels;
    Assert::AreEqual(20, moved.xVoxels);
    Assert::AreEqual(GROUND, moved.yVoxels);
    Assert::AreEqual(19, moved.zVoxels);
    Assert::IsTrue(world.Units().at(horseman).position == Simulation::CellFloorMiddle(At(20, GROUND, 19)));
    Assert::AreEqual(20, world.Units().at(footman).cellVoxels.zVoxels);
    Assert::IsTrue(world.LastTickCrushes().empty());
  }

  TEST_METHOD(HandlesUnitsInIdOrder)
  {
    // Two units over pits 5 m deep, the lower id's further along both axes: their crush events come
    // in unit-id order, not in the order of their cells.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const Protocol::EntityId first = Place(world, Simulation::NavigationClass::Foot, At(40, GROUND, 40));
    const Protocol::EntityId second = Place(world, Simulation::NavigationClass::Foot, At(20, GROUND, 20));
    Assert::IsTrue(first < second);
    world.Tick();
    Fill(world, At(20, GROUND - 5, 20), At(20, GROUND - 1, 20), 0);
    Fill(world, At(40, GROUND - 5, 40), At(40, GROUND - 1, 40), 0);
    world.Tick();
    Assert::AreEqual(std::size_t{2}, world.LastTickCrushes().size());
    Assert::AreEqual(first, world.LastTickCrushes()[0].unit);
    Assert::AreEqual(second, world.LastTickCrushes()[1].unit);
    Assert::AreEqual(5, world.LastTickCrushes()[0].dropVoxels);
    Assert::AreEqual(5, world.LastTickCrushes()[1].dropVoxels);
  }
};

} // namespace SimulationTests
