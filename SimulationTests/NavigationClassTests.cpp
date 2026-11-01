#include "pch.h"

#include "ChunkStore.h"
#include "NavigationGrid.h"
#include "ServerDataFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Navigation's classes (M2-F1; ADR-015, decisions 1 and 2; GameDesign.md, M2's values, item 10): foot
// climbs 1 m over 1 cell, cavalry over 2 and machines over 3, with 2 m, 3 m and 4 m of headroom.
// The ground is bedrock and earth, its top at y 2, so a unit stands at y 3.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t WATER = 9;
constexpr std::int32_t EDGE = 64;
constexpr std::int16_t GROUND = 3;

using Grid = Simulation::NavigationGrid;
using Class = Simulation::NavigationClass;

void Fill(Simulation::ChunkStore& _store, std::int32_t _x0, std::int32_t _x1, std::int32_t _y0, std::int32_t _y1, std::int32_t _z0,
          std::int32_t _z1, std::uint8_t _index)
{
  for (std::int32_t y = _y0; y <= _y1; ++y)
  {
    for (std::int32_t z = _z0; z <= _z1; ++z)
    {
      for (std::int32_t x = _x0; x <= _x1; ++x)
      {
        _store.Set({.xVoxels = x, .yVoxels = y, .zVoxels = z}, _index);
      }
    }
  }
}

void Ground(Simulation::ChunkStore& _store)
{
  Fill(_store, 0, EDGE - 1, 0, 0, 0, EDGE - 1, BEDROCK);
  Fill(_store, 0, EDGE - 1, 1, 2, 0, EDGE - 1, EARTH);
}

Grid Built(const Simulation::ChunkStore& _store)
{
  Grid grid({.widthCells = EDGE, .depthCells = EDGE, .heightVoxels = 24, .climbVoxels = 1, .rules = &BakedServerData().Tuning()},
            BakedServerData().Materials());
  grid.RebuildAll(_store);
  return grid;
}

bool Open(const Grid& _grid, std::int32_t _x, std::int32_t _z, Class _class, Grid::Link _link)
{
  return (_grid.Links(_x, _z, _class) & _link) != 0;
}

} // namespace

TEST_CLASS(NavigationClassTests)
{
public:
  TEST_METHOD(ClimbsByEachClassesRun)
  {
    Simulation::ChunkStore store;
    Ground(store);
    // Steps of 1 m every 1 m along x from 10 to 14, at z 0 to 20.
    for (std::int32_t x = 10; x <= 14; ++x)
    {
      Fill(store, x, EDGE - 1, 3 + x - 10, 3 + x - 10, 0, 20, EARTH);
    }
    // A ramp of 1 m every 2 m along x from 40, at z 40 to 63.
    for (std::int32_t x = 40; x <= 52; ++x)
    {
      Fill(store, x, x, 3, 3 + (x - 40) / 2, 40, EDGE - 1, EARTH);
    }
    const Grid grid = Built(store);
    Assert::IsTrue(Open(grid, 12, 10, Class::Foot, Grid::East), L"foot climbs the steps");
    Assert::IsFalse(Open(grid, 12, 10, Class::Cavalry, Grid::East), L"cavalry does not");
    Assert::IsTrue(Open(grid, 45, 50, Class::Cavalry, Grid::East), L"cavalry climbs the ramp");
    Assert::IsTrue(Open(grid, 45, 50, Class::Foot, Grid::East));
    Assert::IsFalse(Open(grid, 45, 50, Class::Machines, Grid::East), L"machines do not");
    Assert::IsTrue(Open(grid, 30, 30, Class::Machines, Grid::East), L"machines cross flat ground");
  }

  TEST_METHOD(GivesEachClassItsHeadroom)
  {
    Simulation::ChunkStore store;
    Ground(store);
    // T5, the gatehouse, from the server's data: its passage is 3 m wide and 4 m high.
    const Simulation::TemplateShape gatehouse = BakedServerData().FindTemplate("T5")->Shape();
    for (std::uint32_t y = 0; y < gatehouse.heightVoxels; ++y)
    {
      for (std::uint32_t z = 0; z < gatehouse.depthVoxels; ++z)
      {
        for (std::uint32_t x = 0; x < gatehouse.widthVoxels; ++x)
        {
          if (gatehouse.At(x, y, z) != 0)
          {
            store.Set({.xVoxels = 20 + static_cast<std::int32_t>(x),
                       .yVoxels = 3 + static_cast<std::int32_t>(y),
                       .zVoxels = 20 + static_cast<std::int32_t>(z)},
                      gatehouse.At(x, y, z));
          }
        }
      }
    }
    // A wall with an arch 2 m high through it.
    Fill(store, 40, 42, 3, 8, 40, 41, MASONRY);
    Fill(store, 41, 41, 3, 4, 40, 41, 0);
    const Grid grid = Built(store);
    Assert::AreEqual(std::uint8_t{4}, grid.Headroom(24, 22));
    Assert::AreEqual(GROUND, grid.WalkableHeight(24, 22));
    for (const Class navigationClass : {Class::Foot, Class::Cavalry, Class::Machines})
    {
      Assert::IsTrue(grid.Standable(24, 22, navigationClass));
      Assert::IsTrue(grid.Standable(23, 22, navigationClass) && grid.Standable(25, 22, navigationClass));
    }
    Assert::AreEqual(std::uint8_t{2}, grid.Headroom(41, 40));
    Assert::IsTrue(grid.Standable(41, 40, Class::Foot));
    Assert::IsFalse(grid.Standable(41, 40, Class::Cavalry));
    Assert::IsFalse(grid.Standable(41, 40, Class::Machines));
    Assert::AreEqual(std::uint8_t{4}, grid.Headroom(5, 5)); // the open sky, capped at the tallest need
  }

  TEST_METHOD(FordsOneVoxelOfWaterAndNotTwo)
  {
    Simulation::ChunkStore store;
    Ground(store);
    Fill(store, 10, 12, 2, 2, 0, EDGE - 1, WATER);
    Fill(store, 50, 52, 1, 2, 0, EDGE - 1, WATER);
    const Grid grid = Built(store);
    Assert::IsTrue(grid.GroundAt(11, 5) == Grid::Ground::Ford);
    for (const Class navigationClass : {Class::Foot, Class::Cavalry, Class::Machines})
    {
      Assert::IsTrue(grid.Usable(11, 5, navigationClass));
      Assert::IsTrue(Open(grid, 10, 5, navigationClass, Grid::East));
      Assert::IsFalse(grid.Usable(51, 5, navigationClass));
    }
  }

  TEST_METHOD(CutsNoCorner)
  {
    Simulation::ChunkStore store;
    Ground(store);
    Fill(store, 31, 31, 3, 5, 30, 30, MASONRY); // a pillar
    const Grid grid = Built(store);
    for (const Class navigationClass : {Class::Foot, Class::Cavalry, Class::Machines})
    {
      Assert::IsFalse(Open(grid, 30, 31, navigationClass, Grid::SouthEast), L"past the pillar's corner");
      Assert::IsFalse(Open(grid, 30, 29, navigationClass, Grid::NorthEast));
      Assert::IsTrue(Open(grid, 30, 31, navigationClass, Grid::NorthEast), L"away from it");
    }
  }

  TEST_METHOD(MarksRubbleGround)
  {
    Simulation::ChunkStore store;
    Ground(store);
    Fill(store, 5, 5, 2, 2, 5, 5, 8); // stone rubble in the ground's top
    const Grid grid = Built(store);
    Assert::IsTrue(grid.GroundAt(5, 5) == Grid::Ground::Rubble);
    Assert::IsTrue(grid.GroundAt(6, 5) == Grid::Ground::Plain);
  }

  TEST_METHOD(WidensTheTilesAnEditTouches)
  {
    Simulation::ChunkStore store;
    Ground(store);
    const Grid grid = Built(store);
    Assert::AreEqual(5, grid.MarginCells()); // machines climb over 3 cells, beyond a clearance square 3 wide
    const std::array<Protocol::VoxelPosition, 1> nearEdge = {{{.xVoxels = 29, .yVoxels = 3, .zVoxels = 10}}};
    Assert::AreEqual(std::size_t{2}, grid.TilesTouchedBy(nearEdge).size());
    const std::array<Protocol::VoxelPosition, 1> inside = {{{.xVoxels = 20, .yVoxels = 3, .zVoxels = 10}}};
    Assert::AreEqual(std::size_t{1}, grid.TilesTouchedBy(inside).size());
  }
};

} // namespace SimulationTests
