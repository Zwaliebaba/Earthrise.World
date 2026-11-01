#include "pch.h"

#include "ChunkStore.h"
#include "SupportSearch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The support search of ADR-004, decision 2, on small worlds built here: a bedrock floor at y 0,
// and the spans the owner decided, stone 2 m and timber 5 m (GameDesign.md).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t TIMBER = 7;
constexpr std::int32_t HEIGHT_VOXELS = 16;

const Simulation::MaterialRules& Rules()
{
  static const Simulation::MaterialRules RULES = Simulation::StandInMaterialRules(1);
  return RULES;
}

/// The world's surface stands at _surface everywhere, so with an anchor depth of 8 voxels nothing
/// above _surface - 8 is an anchor but bedrock.
Simulation::SupportRules RulesWithSurface(std::int32_t _surface)
{
  return {.materials = &Rules(),
          .anchorDepthVoxels = 8,
          .surfaceVoxels = [_surface](std::int32_t, std::int32_t) { return _surface; },
          .heightVoxels = HEIGHT_VOXELS};
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

/// Lays a bedrock floor 20 x 20 at y 0 in _store. It fills rather than returns, as ChunkStore's
/// move may throw under the debug library (bugprone-exception-escape).
void Floor(Simulation::ChunkStore& _store)
{
  Fill(_store, -10, 9, 0, 0, -10, 9, BEDROCK);
}

/// A column at x 0 from y 1 to 4, and an arm along +x at y 4 of _length voxels, in _material. The
/// arm's last voxel is _length steps from the column. Returns the edit that made the arm's end.
Protocol::VoxelPosition BuildArm(Simulation::ChunkStore& _store, std::uint8_t _material, std::int32_t _length)
{
  Fill(_store, 0, 0, 1, 4, 0, 0, _material);
  Fill(_store, 1, _length, 4, 4, 0, 0, _material);
  return {.xVoxels = _length, .yVoxels = 4, .zVoxels = 0};
}

} // namespace

TEST_CLASS(SupportSearchTests)
{
public:
  TEST_METHOD(KeepsAColumnStandingOnBedrock)
  {
    Simulation::ChunkStore store;
    Floor(store);
    Fill(store, 0, 0, 1, 6, 0, 0, MASONRY);
    const std::array<Protocol::VoxelPosition, 1> edits = {{{.xVoxels = 0, .yVoxels = 6, .zVoxels = 0}}};
    Assert::IsTrue(Simulation::FindUnsupported(store, RulesWithSurface(4), edits).pieces.empty());
  }

  TEST_METHOD(HoldsStoneOutToItsSpanAndNoFurther)
  {
    Simulation::ChunkStore store;
    Floor(store);
    const std::array<Protocol::VoxelPosition, 1> two = {BuildArm(store, MASONRY, 2)};
    Assert::IsTrue(Simulation::FindUnsupported(store, RulesWithSurface(4), two).pieces.empty(), L"2 m of stone stands");

    const std::array<Protocol::VoxelPosition, 1> three = {BuildArm(store, MASONRY, 3)};
    const Simulation::SupportSearchResult result = Simulation::FindUnsupported(store, RulesWithSurface(4), three);
    Assert::AreEqual(std::size_t{1}, result.pieces.size());
    Assert::AreEqual(std::size_t{1}, result.pieces[0].size(), L"only the third meter falls");
    Assert::IsTrue(result.pieces[0][0] == Protocol::VoxelPosition{.xVoxels = 3, .yVoxels = 4, .zVoxels = 0});
  }

  TEST_METHOD(HoldsTimberOutToFiveMeters)
  {
    Simulation::ChunkStore store;
    Floor(store);
    const std::array<Protocol::VoxelPosition, 1> five = {BuildArm(store, TIMBER, 5)};
    Assert::IsTrue(Simulation::FindUnsupported(store, RulesWithSurface(4), five).pieces.empty());
    const std::array<Protocol::VoxelPosition, 1> seven = {BuildArm(store, TIMBER, 7)};
    const Simulation::SupportSearchResult result = Simulation::FindUnsupported(store, RulesWithSurface(4), seven);
    Assert::AreEqual(std::size_t{1}, result.pieces.size());
    Assert::AreEqual(std::size_t{2}, result.pieces[0].size(), L"the sixth and seventh meters fall together");
  }

  TEST_METHOD(BringsDownAnUnderminedWall)
  {
    // An 8 m wall on 3 m of earth over bedrock. Digging the earth out under its middle 6 m leaves
    // the middle of the wall more than 2 m from either end, and connectivity alone would keep it
    // up as a bridge (ADR-004, decision 2). Earth carries support, so the ends stand.
    Simulation::ChunkStore store;
    Floor(store);
    Fill(store, -2, 9, 1, 3, -2, 2, EARTH);
    Fill(store, 0, 7, 4, 6, 0, 1, MASONRY);
    std::vector<Protocol::VoxelPosition> dug;
    for (std::int32_t x = 1; x <= 6; ++x)
    {
      for (std::int32_t z = 0; z <= 1; ++z)
      {
        for (std::int32_t y = 1; y <= 3; ++y)
        {
          store.Set({.xVoxels = x, .yVoxels = y, .zVoxels = z}, 0);
          dug.push_back({.xVoxels = x, .yVoxels = y, .zVoxels = z});
        }
      }
    }
    const Simulation::SupportSearchResult result = Simulation::FindUnsupported(store, RulesWithSurface(4), dug);
    Assert::AreEqual(std::size_t{1}, result.pieces.size());
    // x 3 and 4 are 3 steps from the ends' earth: two meters of the wall's length, 2 deep, 3 high.
    Assert::AreEqual(std::size_t{12}, result.pieces[0].size());
    for (const Protocol::VoxelPosition& voxel : result.pieces[0])
    {
      Assert::IsTrue(voxel.xVoxels == 3 || voxel.xVoxels == 4);
    }
  }

  TEST_METHOD(AnchorsDeepGroundBelowTheAnchorDepth)
  {
    // No bedrock: a column on 10 m of earth. With the surface at 10 and an anchor depth of 8, the
    // earth below 2 is an anchor, so the column and the ground under it stand.
    Simulation::ChunkStore store;
    Fill(store, -3, 3, 0, 9, -3, 3, EARTH);
    Fill(store, 0, 0, 10, 13, 0, 0, MASONRY);
    Fill(store, 1, 2, 13, 13, 0, 0, MASONRY);
    const std::array<Protocol::VoxelPosition, 1> edits = {{{.xVoxels = 2, .yVoxels = 13, .zVoxels = 0}}};
    Assert::IsTrue(Simulation::FindUnsupported(store, RulesWithSurface(10), edits).pieces.empty());
    // With the anchor depth below the world, nothing anchors it, and all the stone falls.
    Simulation::SupportRules floating = RulesWithSurface(10);
    floating.anchorDepthVoxels = 20;
    Assert::AreEqual(std::size_t{1}, Simulation::FindUnsupported(store, floating, edits).pieces.size());
  }

  TEST_METHOD(JudgesOnlyWhatTheEditCanAffect)
  {
    // A floating stone voxel far from the edit is not the search's business: it searches the
    // edit's region, never the world (ADR-004, consequences).
    Simulation::ChunkStore store;
    Floor(store);
    store.Set({.xVoxels = 9, .yVoxels = 8, .zVoxels = 9}, MASONRY);
    Fill(store, 0, 0, 1, 3, 0, 0, MASONRY);
    const std::array<Protocol::VoxelPosition, 1> edits = {{{.xVoxels = 0, .yVoxels = 3, .zVoxels = 0}}};
    const Simulation::SupportSearchResult result = Simulation::FindUnsupported(store, RulesWithSurface(4), edits);
    Assert::IsTrue(result.pieces.empty());
    Assert::IsTrue(result.voxelsSearched > 0);
  }

  TEST_METHOD(DoublesEverySpanForTheHalfMeterWorld)
  {
    // ADR-006's 0.5 m comparison counts spans in its own voxels, two to the meter.
    const Simulation::MaterialRules half = Simulation::StandInMaterialRules(2);
    Assert::AreEqual(std::uint8_t{4}, half[MASONRY].spanVoxels);
    Assert::AreEqual(std::uint8_t{10}, half[TIMBER].spanVoxels);
    Assert::AreEqual(std::uint32_t{10}, Simulation::LongestSpanVoxels(half));
    Assert::AreEqual(std::uint32_t{5}, Simulation::LongestSpanVoxels(Rules()));
    Assert::ExpectException<std::invalid_argument>([] { (void)Simulation::StandInMaterialRules(0); });
  }

  TEST_METHOD(IgnoresWhatLiesBelowTheEdit)
  {
    // Paths run only down or sideways, so an edit cannot unsupport anything below it.
    Simulation::ChunkStore store;
    Floor(store);
    store.Set({.xVoxels = 1, .yVoxels = 2, .zVoxels = 4}, MASONRY); // floating, below the edit
    const std::array<Protocol::VoxelPosition, 1> edits = {{{.xVoxels = 1, .yVoxels = 5, .zVoxels = 4}}};
    Assert::IsTrue(Simulation::FindUnsupported(store, RulesWithSurface(4), edits).pieces.empty());
  }
};

} // namespace SimulationTests
