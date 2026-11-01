#include "pch.h"

#include "CollapsePlan.h"
#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The support search in the tick, and the collapse plan (M2-E3; ADR-004; GameDesign.md, M2's values,
// items 4 to 7).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr Protocol::VoxelPosition WALL_ORIGIN{.xVoxels = 20, .yVoxels = GROUND, .zVoxels = 20};

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

/// Places a template from the baked data, unturned, with its minimum corner at _origin, by edits.
void Place(Simulation::World& _world, std::string_view _id, Protocol::VoxelPosition _origin)
{
  const Simulation::ServerTemplate* found = BakedServerData().FindTemplate(_id);
  Assert::IsNotNull(found);
  const Simulation::TemplateShape shape = found->Shape();
  for (std::uint32_t y = 0; y < shape.heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < shape.depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < shape.widthVoxels; ++x)
      {
        const std::uint8_t index = shape.At(x, y, z);
        if (index != 0)
        {
          _world.Voxels().Edit(At(_origin.xVoxels + static_cast<std::int32_t>(x), _origin.yVoxels + static_cast<std::int32_t>(y),
                                  _origin.zVoxels + static_cast<std::int32_t>(z)),
                               index);
        }
      }
    }
  }
}

/// Removes the box from _low to _high, both included, by edits.
void Dig(Simulation::World& _world, Protocol::VoxelPosition _low, Protocol::VoxelPosition _high)
{
  for (std::int32_t y = _low.yVoxels; y <= _high.yVoxels; ++y)
  {
    for (std::int32_t z = _low.zVoxels; z <= _high.zVoxels; ++z)
    {
      for (std::int32_t x = _low.xVoxels; x <= _high.xVoxels; ++x)
      {
        _world.Voxels().Edit(At(x, y, z), 0);
      }
    }
  }
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

/// A T3 section standing on the flat world, searched once, then undercut _width wide at its base,
/// through its thickness and two voxels high, from 1 voxel in from its west end; one tick runs.
Simulation::World Undercut(std::int32_t _width)
{
  Simulation::World world(FlatWorld::Desc());
  Place(world, "T3", WALL_ORIGIN);
  world.Tick();
  Assert::IsTrue(world.Collapses().empty());
  Dig(world, At(WALL_ORIGIN.xVoxels + 1, GROUND, WALL_ORIGIN.zVoxels),
      At(WALL_ORIGIN.xVoxels + _width, GROUND + 1, WALL_ORIGIN.zVoxels + 1));
  world.Tick();
  return world;
}

/// A masonry block 3 x 4 x 3 resting on piers, and the plan of its collapse when _cause is removed.
Simulation::CollapsePlan PlanBlock(std::span<const Protocol::VoxelPosition> _cause)
{
  Simulation::World world(FlatWorld::Desc());
  Fill(world, At(10, GROUND + 2, 10), At(12, GROUND + 5, 12), MASONRY);
  Simulation::Piece piece;
  for (std::int32_t y = GROUND + 2; y <= GROUND + 5; ++y)
  {
    for (std::int32_t z = 10; z <= 12; ++z)
    {
      for (std::int32_t x = 10; x <= 12; ++x)
      {
        piece.push_back(At(x, y, z));
      }
    }
  }
  return Simulation::PlanCollapse(piece, _cause, world.Voxels(), BakedServerData());
}

} // namespace

TEST_CLASS(CollapseTests)
{
public:
  TEST_METHOD(DropsTheSlotTheHandCalculationGives)
  {
    // MvpPlan.md, M2's risks: at stone's 2 m span a hole 5 wide through a T3 section's base leaves
    // all but its middle supported, and a slot 5 - 4 = 1 wide falls: every voxel of the section
    // above the hole's middle column, through the wall's thickness.
    const Simulation::World world = Undercut(5);
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    const Simulation::Collapse& collapse = world.Collapses().begin()->second;
    const std::int32_t middle = WALL_ORIGIN.xVoxels + 3;
    std::size_t expected = 0;
    const Simulation::TemplateShape shape = BakedServerData().FindTemplate("T3")->Shape();
    for (std::uint32_t y = 2; y < shape.heightVoxels; ++y)
    {
      for (std::uint32_t z = 0; z < shape.depthVoxels; ++z)
      {
        if (shape.At(3, y, z) != 0)
        {
          ++expected;
        }
      }
    }
    Assert::AreEqual(expected, collapse.piece.size());
    for (const Protocol::VoxelPosition& voxel : collapse.piece)
    {
      Assert::AreEqual(middle, voxel.xVoxels);
      Assert::IsTrue(voxel.yVoxels >= GROUND + 2);
    }
    Assert::IsTrue(collapse.plan.direction == Simulation::FallDirection::Down);
  }

  TEST_METHOD(DropsNothingFromAHoleFourWide)
  {
    Assert::IsTrue(Undercut(4).Collapses().empty());
  }

  TEST_METHOD(WarnsAnEightMetrePieceFor135Ticks)
  {
    const Simulation::Rules& rules = BakedServerData().Tuning();
    Assert::AreEqual(std::uint32_t{135}, Simulation::WarningTicks(8, rules));
    Assert::AreEqual(std::uint32_t{26}, Simulation::FallTicks(8, rules));
    Simulation::World world(FlatWorld::Desc());
    Fill(world, At(5, GROUND, 5), At(5, GROUND + 7, 5), MASONRY);
    Simulation::Piece column;
    for (std::int32_t y = GROUND; y <= GROUND + 7; ++y)
    {
      column.push_back(At(5, y, 5));
    }
    const Simulation::CollapsePlan plan = Simulation::PlanCollapse(column, {}, world.Voxels(), BakedServerData());
    Assert::AreEqual(std::uint32_t{135}, plan.warningTicks);
    Assert::AreEqual(std::uint32_t{26}, plan.fallTicks);
  }

  TEST_METHOD(FallsTowardsTheSideThatLostSupport)
  {
    // The block's middle is (11.5, 11.5); a cause on one side pulls it that way, one below the middle
    // lets it fall straight down.
    const std::array<Protocol::VoxelPosition, 2> east = {At(13, GROUND + 1, 11), At(13, GROUND, 11)};
    const std::array<Protocol::VoxelPosition, 2> west = {At(9, GROUND + 1, 10), At(9, GROUND + 1, 12)};
    const std::array<Protocol::VoxelPosition, 1> north = {At(11, GROUND + 1, 14)};
    const std::array<Protocol::VoxelPosition, 1> south = {At(12, GROUND + 1, 9)};
    const std::array<Protocol::VoxelPosition, 2> even = {At(10, GROUND + 1, 11), At(12, GROUND + 1, 11)};
    Assert::IsTrue(PlanBlock(east).direction == Simulation::FallDirection::East);
    Assert::IsTrue(PlanBlock(west).direction == Simulation::FallDirection::West);
    Assert::IsTrue(PlanBlock(north).direction == Simulation::FallDirection::North);
    Assert::IsTrue(PlanBlock(south).direction == Simulation::FallDirection::South);
    Assert::IsTrue(PlanBlock(even).direction == Simulation::FallDirection::Down);
    Assert::IsTrue(PlanBlock({}).direction == Simulation::FallDirection::Down);
    // Under a quarter of the width off the middle is still straight down.
    const std::array<Protocol::VoxelPosition, 1> slightly = {At(11, GROUND + 1, 11)};
    Assert::IsTrue(PlanBlock(slightly).direction == Simulation::FallDirection::Down);
  }

  TEST_METHOD(LaysTheLandingAlongTheFall)
  {
    const std::array<Protocol::VoxelPosition, 2> east = {At(13, GROUND + 1, 11), At(13, GROUND, 11)};
    const Simulation::CollapsePlan plan = PlanBlock(east);
    Assert::AreEqual(std::uint32_t{36}, plan.fallenVoxels); // one rubble voxel for each of 3 x 4 x 3
    Assert::AreEqual(std::size_t{36}, plan.landing.size());
    for (const Simulation::PlacedVoxel& placed : plan.landing)
    {
      Assert::AreEqual(std::uint8_t{8}, placed.paletteIndex);                         // masonry leaves stone rubble
      Assert::IsTrue(placed.position.xVoxels >= 13 && placed.position.xVoxels <= 16); // at most the piece's 4 m beyond it
      Assert::IsTrue(placed.position.zVoxels >= 10 && placed.position.zVoxels <= 12);
      Assert::IsTrue(placed.position.yVoxels >= GROUND);
    }
    Assert::IsTrue(plan.landing.front().position.xVoxels == 13); // nearest the foot first
    Assert::AreEqual(std::size_t{12}, plan.strip.size());
  }

  TEST_METHOD(PlansWithoutChangingTheWorld)
  {
    Simulation::World world = Undercut(5);
    const Simulation::Collapse& collapse = world.Collapses().begin()->second;
    const std::uint64_t before = Simulation::StateHash(world);
    const std::array<Protocol::VoxelPosition, 1> cause = {At(23, GROUND, 20)};
    const Simulation::CollapsePlan first = Simulation::PlanCollapse(collapse.piece, cause, world.Voxels(), BakedServerData());
    const Simulation::CollapsePlan second = Simulation::PlanCollapse(collapse.piece, cause, world.Voxels(), BakedServerData());
    Assert::IsTrue(first == second);
    Assert::AreEqual(before, Simulation::StateHash(world));
  }

  TEST_METHOD(MovesNothingDuringTheWarning)
  {
    Simulation::World world = Undercut(5);
    const Simulation::Collapse collapse = world.Collapses().begin()->second;
    for (std::uint32_t tick = 1; tick < collapse.plan.warningTicks; ++tick)
    {
      world.Tick();
      for (const Protocol::VoxelPosition& voxel : collapse.piece)
      {
        Assert::AreEqual(std::uint8_t{MASONRY}, world.Voxels().At(voxel));
      }
    }
    Assert::AreEqual(std::size_t{1}, world.Collapses().size()); // found once, not again by later searches
  }
};

} // namespace SimulationTests
