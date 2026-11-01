#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Cascades and collapses in a save (M2-E5; ADR-004, decision 3).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t TIMBER = 7;
constexpr std::uint8_t STONE_RUBBLE = 8;
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

/// A masonry pier with a beam reaching 3 m east from its top, and a timber beam 2 m long beyond it.
/// At stone's 2 m span the beam's last voxel is unsupported; the timber, spanning 5 m, holds by the
/// path through it until it falls.
void BuildCantilever(Simulation::World& _world)
{
  Fill(_world, At(10, GROUND, 10), At(10, GROUND + 3, 10), MASONRY);
  Fill(_world, At(11, GROUND + 3, 10), At(13, GROUND + 3, 10), MASONRY);
  Fill(_world, At(14, GROUND + 3, 10), At(15, GROUND + 3, 10), TIMBER);
}

/// Runs until no collapse is left, and returns how many ticks that took.
std::uint32_t RunOut(Simulation::World& _world)
{
  std::uint32_t ticks = 0;
  while (!_world.Collapses().empty() && ticks < 1000)
  {
    _world.Tick();
    ++ticks;
  }
  return ticks;
}

} // namespace

TEST_CLASS(CascadeTests)
{
public:
  TEST_METHOD(StartsAnUnderminedPieceOnTheNextTick)
  {
    Simulation::World world(FlatWorld::Desc());
    BuildCantilever(world);
    world.Tick();
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    const Simulation::Collapse beam = world.Collapses().begin()->second;
    Assert::AreEqual(std::size_t{1}, beam.piece.size());
    Assert::IsTrue(beam.piece.front() == At(13, GROUND + 3, 10));

    // Through the warning: the timber holds while the beam's last voxel is in the grid.
    while (world.Voxels().At(At(13, GROUND + 3, 10)) == MASONRY)
    {
      world.Tick();
      Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    }
    // The tick the beam's voxel left the grid started nothing: its edits wait for the next search.
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    world.Tick();
    Assert::AreEqual(std::size_t{2}, world.Collapses().size());
    const Simulation::Collapse& timber = world.Collapses().rbegin()->second;
    Assert::IsTrue(timber.id > beam.id);
    Assert::AreEqual(std::size_t{2}, timber.piece.size());
    RunOut(world);
    Assert::AreEqual(std::uint8_t{0}, world.Voxels().At(At(14, GROUND + 3, 10)));
  }

  TEST_METHOD(AdvancesCollapsesThatStartTogetherInIdOrder)
  {
    // A timber voxel and a masonry one, each held by the pier between them, lose it on one tick: the
    // timber falls east and the masonry west, onto the same voxel, on the same tick. They start
    // together, the timber first in piece order, and the masonry's rubble, written after the
    // timber's because its id is higher, is what remains.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, At(6, GROUND + 2, 10), At(6, GROUND + 2, 10), TIMBER);
    Fill(world, At(8, GROUND + 2, 10), At(8, GROUND + 2, 10), MASONRY);
    Fill(world, At(7, GROUND, 10), At(7, GROUND + 2, 10), MASONRY);
    world.Tick();
    Assert::IsTrue(world.Collapses().empty());
    Fill(world, At(7, GROUND, 10), At(7, GROUND + 2, 10), 0);
    world.Tick();
    Assert::AreEqual(std::size_t{2}, world.Collapses().size());
    const Simulation::Collapse first = world.Collapses().begin()->second;
    const Simulation::Collapse second = world.Collapses().rbegin()->second;
    Assert::AreEqual(first.id + 1, second.id);
    Assert::IsTrue(first.piece.front() == At(6, GROUND + 2, 10) && second.piece.front() == At(8, GROUND + 2, 10));
    Assert::IsTrue(first.plan.direction == Simulation::FallDirection::East);
    Assert::IsTrue(second.plan.direction == Simulation::FallDirection::West);
    Assert::IsTrue(first.plan.landing.front().position == second.plan.landing.front().position);
    Assert::AreEqual(first.ticksElapsed, second.ticksElapsed);
    RunOut(world);
    Assert::AreEqual(std::uint8_t{STONE_RUBBLE}, world.Voxels().At(At(7, GROUND, 10)));
    Assert::AreEqual(std::uint8_t{0}, world.Voxels().At(At(7, GROUND + 1, 10)));
  }
};

TEST_CLASS(CollapseSaveTests)
{
public:
  TEST_METHOD(ContinuesFromASaveMidWarningAndMidFall)
  {
    Simulation::World straight(FlatWorld::Desc());
    BuildCantilever(straight);
    straight.Tick();
    const std::uint32_t warning = straight.Collapses().begin()->second.plan.warningTicks;
    const std::uint32_t total = RunOut(straight);

    for (const std::uint32_t at : {warning / 2, warning + 3})
    {
      Simulation::World before(FlatWorld::Desc());
      BuildCantilever(before);
      before.Tick();
      for (std::uint32_t tick = 0; tick < at; ++tick)
      {
        before.Tick();
      }
      Assert::IsFalse(before.Collapses().empty());
      Simulation::World after = Simulation::LoadSave(Simulation::WriteSave(before), FlatWorld::Desc());
      Assert::AreEqual(Simulation::StateHash(before), Simulation::StateHash(after));
      Assert::AreEqual(total - at, RunOut(after));
      Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(after));
    }
  }
};

} // namespace SimulationTests
