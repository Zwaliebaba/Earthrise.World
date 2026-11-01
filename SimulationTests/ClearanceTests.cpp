#include "pch.h"

#include "ChunkStore.h"
#include "NavigationGrid.h"
#include "ServerDataFile.h"
#include "SplitMix64.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Clearance (M2-F2; ADR-015, decision 2): the widest square, with a cell at its minimum corner, a
// class can stand on, capped at the widest class's 3 m. Foot and cavalry are 1 wide and machines 3.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t EDGE = 96;

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

Grid Empty()
{
  return Grid({.widthCells = EDGE, .depthCells = EDGE, .heightVoxels = 24, .climbVoxels = 1, .rules = &BakedServerData().Tuning()},
              BakedServerData().Materials());
}

} // namespace

TEST_CLASS(ClearanceTests)
{
public:
  TEST_METHOD(GivesOpenGroundTheCap)
  {
    Simulation::ChunkStore store;
    Ground(store);
    Grid grid = Empty();
    grid.RebuildAll(store);
    Assert::AreEqual(std::uint8_t{3}, grid.Clearance(40, 40, Class::Machines));
    Assert::AreEqual(std::uint8_t{3}, grid.Clearance(40, 40, Class::Foot));
    Assert::IsTrue(grid.Usable(40, 40, Class::Machines));
    // At the map's edge the square runs off it.
    Assert::AreEqual(std::uint8_t{1}, grid.Clearance(EDGE - 1, 40, Class::Machines));
    Assert::IsFalse(grid.Usable(EDGE - 1, 40, Class::Machines));
    Assert::IsTrue(grid.Usable(EDGE - 1, 40, Class::Foot));
  }

  TEST_METHOD(FitsTheRamThroughTheGatehousesPassage)
  {
    Simulation::ChunkStore store;
    Ground(store);
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
    Grid grid = Empty();
    grid.RebuildAll(store);
    // The passage runs from x 23 to 25: 3 m, with the passage's west side at the square's corner.
    Assert::AreEqual(std::uint8_t{3}, grid.Clearance(23, 21, Class::Machines));
    Assert::IsTrue(grid.Usable(23, 21, Class::Machines));
    Assert::AreEqual(std::uint8_t{2}, grid.Clearance(24, 21, Class::Machines));
  }

  TEST_METHOD(StopsTheRamAtAGapTwoWide)
  {
    Simulation::ChunkStore store;
    Ground(store);
    Fill(store, 50, 70, 3, 6, 50, 51, MASONRY);
    Fill(store, 60, 61, 3, 6, 50, 51, 0); // a gap 2 m wide
    Grid grid = Empty();
    grid.RebuildAll(store);
    Assert::AreEqual(std::uint8_t{2}, grid.Clearance(60, 50, Class::Machines));
    Assert::IsFalse(grid.Usable(60, 50, Class::Machines));
    Assert::IsFalse(grid.Usable(59, 50, Class::Machines));
    Assert::IsTrue(grid.Usable(60, 50, Class::Foot) && grid.Usable(61, 51, Class::Foot));
    Assert::IsTrue((grid.Links(60, 49, Class::Foot) & Grid::North) != 0, L"foot goes through");
    for (std::int32_t x = 56; x <= 62; ++x)
    {
      Assert::IsTrue((grid.Links(x, 49, Class::Machines) & Grid::North) == 0, L"the ram does not");
    }
  }

  TEST_METHOD(RebuildsTouchedTilesAsAFullRebuildWould)
  {
    Simulation::ChunkStore store;
    Ground(store);
    Grid incremental = Empty();
    incremental.RebuildAll(store);
    Core::SplitMix64 random(0xc1ea4a9ce);
    std::vector<Protocol::VoxelPosition> edits;
    for (std::uint32_t edit = 0; edit < 300; ++edit)
    {
      const auto x = static_cast<std::int32_t>(Core::DrawBelow(random.Next(), EDGE));
      const auto z = static_cast<std::int32_t>(Core::DrawBelow(random.Next(), EDGE));
      const auto y = static_cast<std::int32_t>(1 + Core::DrawBelow(random.Next(), 6));
      const std::uint8_t index =
        Core::DrawBelow(random.Next(), 3) == 0 ? std::uint8_t{0} : (Core::DrawBelow(random.Next(), 2) == 0 ? EARTH : MASONRY);
      store.Set({.xVoxels = x, .yVoxels = y, .zVoxels = z}, index);
      edits.push_back({.xVoxels = x, .yVoxels = y, .zVoxels = z});
    }
    for (const auto& [tileX, tileZ] : incremental.TilesTouchedBy(edits))
    {
      incremental.RebuildTile(store, tileX, tileZ);
    }
    Grid full = Empty();
    full.RebuildAll(store);
    for (std::int32_t z = 0; z < EDGE; ++z)
    {
      for (std::int32_t x = 0; x < EDGE; ++x)
      {
        Assert::AreEqual(full.WalkableHeight(x, z), incremental.WalkableHeight(x, z));
        Assert::AreEqual(full.Headroom(x, z), incremental.Headroom(x, z));
        for (const Class navigationClass : {Class::Foot, Class::Cavalry, Class::Machines})
        {
          Assert::AreEqual(full.Clearance(x, z, navigationClass), incremental.Clearance(x, z, navigationClass));
          Assert::AreEqual(full.Links(x, z, navigationClass), incremental.Links(x, z, navigationClass));
        }
      }
    }
  }
};

} // namespace SimulationTests
