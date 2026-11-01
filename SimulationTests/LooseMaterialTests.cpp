#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Loose material near structures (M2-E7; GameDesign.md, M2's values, item 9; GameDescription.md,
// *Materials*): it never rests on a structure, nor less than looseRestBelowTopVoxels, 1 m, below the
// top of a structure beside it. A T3 section stands on the flat world, its front along x.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WALL_X = 20; // the section runs from x 20 to 27, at z 20 and 21
constexpr std::int32_t WALL_Z = 20;

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

std::int32_t Top(const Simulation::World& _world, std::int32_t _x, std::int32_t _z)
{
  for (std::int32_t y = 63; y >= 0; --y)
  {
    if (_world.Voxels().At(At(_x, y, _z)) != 0)
    {
      return y;
    }
  }
  return -1;
}

/// The flat world with a T3 section placed by edits and searched once.
Simulation::World WithWall()
{
  Simulation::World world(FlatWorld::Desc());
  const Simulation::TemplateShape shape = BakedServerData().FindTemplate("T3")->Shape();
  for (std::uint32_t y = 0; y < shape.heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < shape.depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < shape.widthVoxels; ++x)
      {
        if (shape.At(x, y, z) != 0)
        {
          world.Voxels().Edit(
            At(WALL_X + static_cast<std::int32_t>(x), GROUND + static_cast<std::int32_t>(y), WALL_Z + static_cast<std::int32_t>(z)),
            shape.At(x, y, z));
        }
      }
    }
  }
  world.Tick();
  return world;
}

void Settle(Simulation::World& _world)
{
  for (std::uint32_t tick = 0; tick < 2000; ++tick)
  {
    _world.Tick();
    if (_world.LastTickEdits().empty())
    {
      return;
    }
  }
  Assert::Fail(L"the rubble did not settle");
}

/// Whether every column beside the wall that rubble tops stands at least 1 m below the wall's top
/// beside it, and every wall column is topped by the wall.
bool RestsBelowTheWallsTop(const Simulation::World& _world)
{
  for (std::int32_t x = WALL_X; x < WALL_X + 8; ++x)
  {
    for (const std::int32_t z : {WALL_Z, WALL_Z + 1})
    {
      if (_world.Voxels().At(At(x, Top(_world, x, z), z)) != MASONRY)
      {
        return false; // something rests on the wall
      }
    }
    for (const auto& [beside, wall] : {std::pair{WALL_Z - 1, WALL_Z}, std::pair{WALL_Z + 2, WALL_Z + 1}})
    {
      const std::int32_t heap = Top(_world, x, beside);
      if (_world.Voxels().At(At(x, heap, beside)) == STONE_RUBBLE && heap > Top(_world, x, wall) - 1)
      {
        return false;
      }
    }
  }
  return true;
}

} // namespace

TEST_CLASS(LooseMaterialTests)
{
public:
  TEST_METHOD(SettlesRubbleAgainstAWallBelowItsTop)
  {
    Simulation::World world = WithWall();
    // Rubble heaped against the north face to the wall's full height, 3 m out.
    Fill(world, At(WALL_X, GROUND, WALL_Z + 2), At(WALL_X + 7, GROUND + 6, WALL_Z + 4), STONE_RUBBLE);
    Settle(world);
    Assert::IsTrue(RestsBelowTheWallsTop(world));
    // As high as the rule lets it: 1 m below the top beside it, where enough rubble lies.
    Assert::IsTrue(Top(world, WALL_X + 4, WALL_Z + 2) >= GROUND + 2);
  }

  TEST_METHOD(LeavesNothingOnTheWallsTop)
  {
    Simulation::World world = WithWall();
    for (std::int32_t x = WALL_X; x < WALL_X + 8; ++x)
    {
      for (const std::int32_t z : {WALL_Z, WALL_Z + 1})
      {
        world.Voxels().Edit(At(x, Top(world, x, z) + 1, z), STONE_RUBBLE);
      }
    }
    Settle(world);
    Assert::IsTrue(RestsBelowTheWallsTop(world));
  }

  TEST_METHOD(NeverLeavesAHeapWithinAMetreOfTheTop)
  {
    // Heaps on both faces and over the top, of different heights.
    Simulation::World world = WithWall();
    Fill(world, At(WALL_X - 2, GROUND, WALL_Z - 3), At(WALL_X + 9, GROUND + 3, WALL_Z - 1), STONE_RUBBLE);
    Fill(world, At(WALL_X + 2, GROUND, WALL_Z + 2), At(WALL_X + 5, GROUND + 9, WALL_Z + 3), STONE_RUBBLE);
    Fill(world, At(WALL_X + 3, GROUND + 7, WALL_Z), At(WALL_X + 4, GROUND + 9, WALL_Z + 1), STONE_RUBBLE);
    Settle(world);
    Assert::IsTrue(RestsBelowTheWallsTop(world));
  }
};

} // namespace SimulationTests
