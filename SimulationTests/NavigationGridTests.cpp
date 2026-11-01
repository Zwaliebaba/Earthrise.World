#include "pch.h"

#include "ChunkStore.h"
#include "NavigationGrid.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The navigation grid on a 64 x 64 world of four tiles: ground of earth over bedrock, 3 m deep.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t WATER = 9;
constexpr std::int32_t EDGE = 64;
constexpr std::int16_t GROUND = 3; // a unit stands in the cell above the earth's top at y 2

const Simulation::MaterialRules& Rules()
{
  static const Simulation::MaterialRules RULES = Simulation::StandInMaterialRules(1);
  return RULES;
}

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

Simulation::ChunkStore Ground()
{
  Simulation::ChunkStore store;
  Fill(store, 0, EDGE - 1, 0, 0, 0, EDGE - 1, BEDROCK);
  Fill(store, 0, EDGE - 1, 1, 2, 0, EDGE - 1, EARTH);
  return store;
}

Simulation::NavigationGrid Grid()
{
  return Simulation::NavigationGrid({.widthCells = EDGE, .depthCells = EDGE, .heightVoxels = 16, .climbVoxels = 1}, Rules());
}

constexpr std::uint8_t ALL = 0xff;

} // namespace

TEST_CLASS(NavigationGridTests)
{
public:
  TEST_METHOD(WalksFlatGroundEveryWay)
  {
    const Simulation::ChunkStore store = Ground();
    Simulation::NavigationGrid grid = Grid();
    grid.RebuildAll(store);
    Assert::AreEqual(GROUND, grid.WalkableHeight(20, 20));
    Assert::AreEqual(ALL, grid.Links(20, 20));
    // The world's edge has no neighbours beyond it.
    Assert::AreEqual(static_cast<std::uint8_t>(Simulation::NavigationGrid::North | Simulation::NavigationGrid::NorthEast |
                                               Simulation::NavigationGrid::East),
                     grid.Links(0, 0));
  }

  TEST_METHOD(ClimbsOneMeterAStepButNotTwo)
  {
    Simulation::ChunkStore store = Ground();
    Fill(store, 30, 30, 3, 3, 0, EDGE - 1, EARTH);       // a 1 m step at x 30
    Fill(store, 40, EDGE - 1, 3, 4, 0, EDGE - 1, EARTH); // a 2 m step at x 40
    Simulation::NavigationGrid grid = Grid();
    grid.RebuildAll(store);
    Assert::IsTrue((grid.Links(29, 10) & Simulation::NavigationGrid::East) != 0, L"up 1 m");
    Assert::IsTrue((grid.Links(39, 10) & Simulation::NavigationGrid::East) == 0, L"not up 2 m");
    Assert::IsTrue((grid.Links(40, 10) & Simulation::NavigationGrid::West) == 0, L"nor down 2 m");
  }

  TEST_METHOD(NeverStandsOnAStructureButPassesUnderAGate)
  {
    // A wall 6 m high along x 20 with a gate 3 m wide and 4 m high through it at z 30 to 32.
    Simulation::ChunkStore store = Ground();
    Fill(store, 20, 21, 3, 8, 0, EDGE - 1, MASONRY);
    Fill(store, 20, 21, 3, 6, 30, 32, 0);
    Simulation::NavigationGrid grid = Grid();
    grid.RebuildAll(store);
    Assert::AreEqual(Simulation::NavigationGrid::NOT_WALKABLE, grid.WalkableHeight(20, 10), L"the wall's top is not ground");
    Assert::AreEqual(GROUND, grid.WalkableHeight(20, 31), L"the passage is");
    Assert::IsTrue((grid.Links(19, 31) & Simulation::NavigationGrid::East) != 0);
    Assert::IsTrue((grid.Links(19, 10) & Simulation::NavigationGrid::East) == 0);
  }

  TEST_METHOD(WadesAFordButNotDeepWater)
  {
    Simulation::ChunkStore store = Ground();
    Fill(store, 10, 12, 2, 2, 0, EDGE - 1, WATER); // one voxel of water over earth: a ford
    Fill(store, 50, 52, 1, 2, 0, EDGE - 1, WATER); // two voxels: too deep
    Simulation::NavigationGrid grid = Grid();
    grid.RebuildAll(store);
    Assert::AreEqual(static_cast<std::int16_t>(2), grid.WalkableHeight(11, 5));
    Assert::AreEqual(Simulation::NavigationGrid::NOT_WALKABLE, grid.WalkableHeight(51, 5));
  }

  TEST_METHOD(OpensABreachWhenItsTileIsRebuilt)
  {
    Simulation::ChunkStore store = Ground();
    Fill(store, 20, 21, 3, 8, 0, EDGE - 1, MASONRY);
    Simulation::NavigationGrid grid = Grid();
    grid.RebuildAll(store);
    Assert::IsTrue((grid.Links(19, 40) & Simulation::NavigationGrid::East) == 0);

    std::vector<Protocol::VoxelPosition> breach;
    for (std::int32_t y = 3; y <= 8; ++y)
    {
      for (std::int32_t x = 20; x <= 21; ++x)
      {
        store.Set({.xVoxels = x, .yVoxels = y, .zVoxels = 40}, 0);
        breach.push_back({.xVoxels = x, .yVoxels = y, .zVoxels = 40});
      }
    }
    const std::vector<std::pair<std::int32_t, std::int32_t>> tiles = grid.TilesTouchedBy(breach);
    Assert::AreEqual(std::size_t{1}, tiles.size(), L"the breach lies inside one tile");
    Assert::IsTrue(tiles[0] == std::pair<std::int32_t, std::int32_t>{0, 1});
    grid.RebuildTile(store, tiles[0].first, tiles[0].second);
    Assert::AreEqual(GROUND, grid.WalkableHeight(20, 40));
    Assert::IsTrue((grid.Links(19, 40) & Simulation::NavigationGrid::East) != 0, L"a gap 1 m wide is a route");
  }

  TEST_METHOD(RebuildsTheNeighborsLinksIntoATile)
  {
    // A cell on a tile's edge changes, and the cell across the edge, in the next tile, must see it.
    Simulation::ChunkStore store = Ground();
    Simulation::NavigationGrid grid = Grid();
    grid.RebuildAll(store);
    Fill(store, 32, 32, 3, 4, 10, 10, EARTH); // a 2 m mound at the west edge of tile (1, 0)
    const std::array<Protocol::VoxelPosition, 1> edits = {{{.xVoxels = 32, .yVoxels = 4, .zVoxels = 10}}};
    const std::vector<std::pair<std::int32_t, std::int32_t>> tiles = grid.TilesTouchedBy(edits);
    Assert::AreEqual(std::size_t{2}, tiles.size(), L"its own tile and the one to the west");
    for (const auto& [tileX, tileZ] : tiles)
    {
      grid.RebuildTile(store, tileX, tileZ);
    }
    Assert::IsTrue((grid.Links(31, 10) & Simulation::NavigationGrid::East) == 0);
  }

  TEST_METHOD(RefusesASizeThatIsNotWholeTiles)
  {
    Assert::ExpectException<std::invalid_argument>(
      [] { Simulation::NavigationGrid grid({.widthCells = 40, .depthCells = 32, .heightVoxels = 16, .climbVoxels = 1}, Rules()); });
  }
};

} // namespace SimulationTests
