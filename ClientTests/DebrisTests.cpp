#include "pch.h"

#include "Debris.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A collapse's debris as the client scatters it (M3-C2; GameDesign.md, M3's values, item 16), with no GPU: which
// pieces, where they rest, and the voxels they are drawn as.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::uint8_t EARTH_RUBBLE = 26;
constexpr std::uint8_t ROCK_RUBBLE = 28;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t TOP_VOXELS = 9;
constexpr float LONG_AFTER_SECONDS = 100.0F;

/// A landing of _width by _depth columns of _index, two voxels deep, its top at TOP_VOXELS.
std::vector<Protocol::PlacedVoxel> Landing(std::int32_t _width, std::int32_t _depth, std::uint8_t _index)
{
  std::vector<Protocol::PlacedVoxel> landing;
  for (std::int32_t z = 0; z < _depth; ++z)
  {
    for (std::int32_t x = 0; x < _width; ++x)
    {
      for (std::int32_t y = TOP_VOXELS - 1; y <= TOP_VOXELS; ++y)
      {
        landing.push_back({.position = {.xVoxels = 100 + x, .yVoxels = y, .zVoxels = 200 + z}, .paletteIndex = _index});
      }
    }
  }
  return landing;
}

/// Voxels that fell from a section above the landing.
std::vector<Protocol::PlacedVoxel> Fallen()
{
  constexpr std::int32_t FELL = 5;
  std::vector<Protocol::PlacedVoxel> fallen;
  fallen.reserve(FELL);
  for (std::int32_t x = 0; x < FELL; ++x)
  {
    fallen.push_back({.position = {.xVoxels = 102 + x, .yVoxels = 14, .zVoxels = 201}, .paletteIndex = MASONRY});
  }
  return fallen;
}

Client::Debris Scattered(std::uint64_t _id, std::span<const Protocol::PlacedVoxel> _landing)
{
  Client::Debris debris;
  debris.Scatter(_id, Fallen(), _landing, 0.0F);
  return debris;
}

bool SamePiece(const Client::Debris::Piece& _a, const Client::Debris::Piece& _b)
{
  return _a.modelId == _b.modelId && _a.cellXVoxels == _b.cellXVoxels && _a.cellZVoxels == _b.cellZVoxels &&
         _a.quarterTurns == _b.quarterTurns && _a.restingCenterMeters.x == _b.restingCenterMeters.x &&
         _a.restingCenterMeters.z == _b.restingCenterMeters.z && _a.fromMeters.y == _b.fromMeters.y;
}

} // namespace

TEST_CLASS(DebrisTests)
{
public:
  TEST_METHOD(ScattersTheSamePiecesForTheSameCollapse)
  {
    const std::vector<Protocol::PlacedVoxel> landing = Landing(8, 5, STONE_RUBBLE);
    const Client::Debris first = Scattered(7, landing);
    const Client::Debris again = Scattered(7, landing);
    Assert::AreEqual(first.Pieces().size(), again.Pieces().size());
    for (std::size_t index = 0; index < first.Pieces().size(); ++index)
    {
      Assert::IsTrue(SamePiece(first.Pieces()[index], again.Pieces()[index]));
    }
    const Client::Debris other = Scattered(8, landing);
    Assert::IsFalse(std::ranges::equal(first.Pieces(), other.Pieces(), SamePiece));
  }

  TEST_METHOD(ScattersAPieceOfStoneForEachFourColumnsOfStoneRubble)
  {
    const std::vector<Protocol::PlacedVoxel> landing = Landing(8, 5, STONE_RUBBLE);
    const Client::Debris debris = Scattered(7, landing);
    Assert::AreEqual(std::size_t{10}, debris.Pieces().size());
    for (const Client::Debris::Piece& piece : debris.Pieces())
    {
      Assert::IsTrue(piece.modelId == "D1" || piece.modelId == "D2" || piece.modelId == "D3");
      Assert::IsTrue(piece.quarterTurns < 4);
      Assert::AreEqual(std::uint8_t{0}, piece.tintIndex);
    }
  }

  TEST_METHOD(RestsEveryPieceOnTheLandingsTop)
  {
    const std::vector<Protocol::PlacedVoxel> landing = Landing(8, 5, STONE_RUBBLE);
    const Client::Debris debris = Scattered(7, landing);
    for (const Client::Debris::Piece& piece : debris.Pieces())
    {
      Assert::IsTrue(piece.cellXVoxels >= 100 && piece.cellXVoxels < 108 && piece.cellZVoxels >= 200 && piece.cellZVoxels < 205);
      Assert::AreEqual(static_cast<float>(TOP_VOXELS + 1), piece.restingBottomMeters);
    }
    // Long after the fall, every voxel lies on the top, at its piece's whole quarter turn.
    std::vector<Engine::OrientedVoxel> voxels;
    debris.Pose(LONG_AFTER_SECONDS, voxels);
    Assert::IsFalse(voxels.empty());
    for (const Engine::OrientedVoxel& voxel : voxels)
    {
      Assert::IsTrue(voxel.centerMeters.y - voxel.halfEdgeMeters >= static_cast<float>(TOP_VOXELS + 1) - 1e-4F);
      Assert::IsTrue(voxel.centerMeters.x >= 100.0F && voxel.centerMeters.x <= 109.0F);
      Assert::IsTrue(voxel.centerMeters.z >= 200.0F && voxel.centerMeters.z <= 206.0F);
      // A turn about +y alone: no tilt.
      Assert::AreEqual(0.0F, voxel.rotation.x, 1e-6F);
      Assert::AreEqual(0.0F, voxel.rotation.z, 1e-6F);
    }
  }

  TEST_METHOD(TintsEveryVoxelOverRockRubble)
  {
    const Client::Debris debris = Scattered(7, Landing(8, 5, ROCK_RUBBLE));
    Assert::IsFalse(debris.Pieces().empty());
    std::vector<Engine::OrientedVoxel> voxels;
    debris.Pose(0.5F, voxels);
    Assert::IsFalse(voxels.empty());
    for (const Engine::OrientedVoxel& voxel : voxels)
    {
      Assert::AreEqual(std::uint32_t{ROCK_RUBBLE}, voxel.paletteIndex);
    }
  }

  TEST_METHOD(ScattersClodsOverEarthRubble)
  {
    const Client::Debris debris = Scattered(7, Landing(4, 4, EARTH_RUBBLE));
    Assert::AreEqual(std::size_t{4}, debris.Pieces().size());
    for (const Client::Debris::Piece& piece : debris.Pieces())
    {
      Assert::IsTrue(piece.modelId == "D6");
    }
  }

  TEST_METHOD(ScattersNothingOnARubbleWithNoDebris)
  {
    Assert::IsTrue(Scattered(7, Landing(8, 5, MASONRY)).Pieces().empty());
  }

  TEST_METHOD(StartsEachPieceAmongTheVoxelsThatFell)
  {
    const Client::Debris debris = Scattered(7, Landing(8, 5, STONE_RUBBLE));
    for (const Client::Debris::Piece& piece : debris.Pieces())
    {
      Assert::IsTrue(piece.fromMeters.x >= 102.0F && piece.fromMeters.x < 107.0F);
      Assert::IsTrue(piece.fromMeters.y >= 14.0F && piece.fromMeters.y < 15.0F);
    }
  }
};

} // namespace ClientTests
