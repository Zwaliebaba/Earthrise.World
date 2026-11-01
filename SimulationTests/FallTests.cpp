#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A collapse's fall (M2-E4; GameDesign.md, M2's values, items 5, 7 and 8): a masonry block on a pier
// under its east edge, whose pier is dug away, so the block falls east.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t TIMBER = 7;
constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::uint8_t TIMBER_RUBBLE = 25;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::uint8_t PLAYER = 1;

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

/// What a run of the fall saw.
struct Run
{
  std::vector<std::vector<Protocol::VoxelPosition>> editsByTick; // from the tick the piece left
  std::vector<Simulation::CrushEvent> crushes;
  Simulation::Collapse collapse;
};

/// A block of _index 3 x 4 x 3 at x 10 to 12 and z 10 to 12, two voxels up on a pier under its east
/// edge, a unit in the fall's strip and one outside it. The pier is dug away and the world runs
/// until the collapse ends. _world is left as the fall left it.
Run FallEast(Simulation::World& _world, std::uint8_t _index)
{
  Fill(_world, At(10, GROUND + 2, 10), At(12, GROUND + 5, 12), _index);
  Fill(_world, At(12, GROUND, 10), At(12, GROUND + 1, 12), _index);
  const std::array<Protocol::VoxelPosition, 1> inside = {At(14, GROUND, 11)};
  const std::array<Protocol::VoxelPosition, 1> outside = {At(20, GROUND, 11)};
  static_cast<void>(_world.AddSquad(PLAYER, Simulation::NavigationClass::Foot, inside));
  static_cast<void>(_world.AddSquad(PLAYER, Simulation::NavigationClass::Foot, outside));
  _world.Tick();
  Assert::IsTrue(_world.Collapses().empty());
  Fill(_world, At(12, GROUND, 10), At(12, GROUND + 1, 12), 0);
  _world.Tick();
  Assert::AreEqual(std::size_t{1}, _world.Collapses().size());
  Run run{.editsByTick = {}, .crushes = {}, .collapse = _world.Collapses().begin()->second};
  Assert::IsTrue(run.collapse.plan.direction == Simulation::FallDirection::East);
  bool fallen = false;
  for (std::uint32_t tick = 0; tick < 400 && !_world.Collapses().empty(); ++tick)
  {
    _world.Tick();
    fallen = fallen || _world.Voxels().At(At(10, GROUND + 5, 10)) == 0;
    if (fallen)
    {
      run.editsByTick.emplace_back(_world.LastTickEdits().begin(), _world.LastTickEdits().end());
    }
    run.crushes.insert(run.crushes.end(), _world.LastTickCrushes().begin(), _world.LastTickCrushes().end());
  }
  Assert::IsTrue(_world.Collapses().empty());
  return run;
}

} // namespace

TEST_CLASS(FallTests)
{
public:
  TEST_METHOD(LandsAsManyVoxelsAsFell)
  {
    Simulation::World world(FlatWorld::Desc());
    const Run run = FallEast(world, MASONRY);
    std::size_t rubble = 0;
    for (std::int32_t y = GROUND; y < GROUND + 8; ++y)
    {
      for (std::int32_t z = 8; z <= 14; ++z)
      {
        for (std::int32_t x = 8; x <= 20; ++x)
        {
          rubble += world.Voxels().At(At(x, y, z)) == STONE_RUBBLE ? std::size_t{1} : std::size_t{0};
          Assert::AreNotEqual(std::uint8_t{MASONRY}, world.Voxels().At(At(x, y, z)));
        }
      }
    }
    Assert::AreEqual(run.collapse.piece.size(), rubble);
    Assert::AreEqual(std::size_t{36}, rubble);
  }

  TEST_METHOD(LeavesRubbleOfEachMaterial)
  {
    Simulation::World world(FlatWorld::Desc());
    static_cast<void>(FallEast(world, TIMBER));
    // Timber leaves timber rubble, east of where it stood, within the piece's 4 m.
    for (std::int32_t x = 13; x <= 16; ++x)
    {
      Assert::AreEqual(std::uint8_t{TIMBER_RUBBLE}, world.Voxels().At(At(x, GROUND, 11)));
    }
    Assert::AreEqual(std::uint8_t{0}, world.Voxels().At(At(17, GROUND, 11)));
  }

  TEST_METHOD(ChangesNothingOutsideThePieceAndTheStrip)
  {
    Simulation::World world(FlatWorld::Desc());
    const Run run = FallEast(world, MASONRY);
    for (const std::vector<Protocol::VoxelPosition>& edits : run.editsByTick)
    {
      for (const Protocol::VoxelPosition& voxel : edits)
      {
        const bool inPiece = std::ranges::find(run.collapse.piece, voxel) != run.collapse.piece.end();
        const bool inStrip = std::ranges::any_of(run.collapse.plan.strip, [&voxel](const Protocol::VoxelPosition& _column)
                                                 { return _column.xVoxels == voxel.xVoxels && _column.zVoxels == voxel.zVoxels; });
        Assert::IsTrue(inPiece || inStrip);
      }
    }
  }

  TEST_METHOD(SpreadsTheEditsOverTheFall)
  {
    Simulation::World world(FlatWorld::Desc());
    const Run run = FallEast(world, MASONRY);
    Assert::AreEqual(static_cast<std::size_t>(run.collapse.plan.fallTicks), run.editsByTick.size());
    Assert::AreEqual(std::uint32_t{19}, run.collapse.plan.fallTicks); // 4 m
    std::size_t landed = 0;
    for (std::size_t tick = 0; tick < run.editsByTick.size(); ++tick)
    {
      landed += run.editsByTick[tick].size();
      Assert::IsTrue(run.editsByTick[tick].size() >= 1);
      Assert::IsTrue(run.editsByTick[tick].size() <= (tick == 0 ? 36 + 2 : 2) + 1); // the piece leaves on the first
    }
    Assert::AreEqual(std::size_t{36 + 36}, landed);
  }

  TEST_METHOD(HitsEachUnitInTheStripOnce)
  {
    Simulation::World world(FlatWorld::Desc());
    const Run run = FallEast(world, MASONRY);
    Assert::AreEqual(std::size_t{1}, run.crushes.size());
    Assert::AreEqual(Protocol::EntityId{2}, run.crushes.front().unit); // the first squad's one unit
    Assert::AreEqual(run.collapse.id, run.crushes.front().collapse);
  }
};

} // namespace SimulationTests
