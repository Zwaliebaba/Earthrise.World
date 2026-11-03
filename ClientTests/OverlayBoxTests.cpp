#include "pch.h"

#include "OverlayBox.h"
#include "VoxelRecord.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The faces each overlay box shares with another of its layer (ADR-021, decision 1, amended), through which the overlay's
// shader does not tint a ray a second time.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t NEGATIVE_X = Engine::JoinedBit(Engine::VoxelFace::NegativeX);
constexpr std::uint8_t POSITIVE_X = Engine::JoinedBit(Engine::VoxelFace::PositiveX);
constexpr std::uint8_t POSITIVE_Y = Engine::JoinedBit(Engine::VoxelFace::PositiveY);
constexpr std::uint8_t NEGATIVE_Z = Engine::JoinedBit(Engine::VoxelFace::NegativeZ);
constexpr std::uint8_t POSITIVE_Z = Engine::JoinedBit(Engine::VoxelFace::PositiveZ);

Client::OverlayBox At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.cellVoxels = {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z}, .paletteIndex = 0};
}

void AssertJoined(const std::vector<std::uint8_t>& _expected, const std::vector<Client::OverlayBox>& _boxes)
{
  const std::vector<std::uint8_t> joined = Client::JoinedFacesOf(_boxes);
  Assert::AreEqual(_expected.size(), joined.size());
  for (std::size_t index = 0; index < joined.size(); ++index)
  {
    Assert::AreEqual(_expected.at(index), joined.at(index), std::format(L"box {}", index).c_str());
  }
}

} // namespace

TEST_CLASS(OverlayBoxTests)
{
public:
  TEST_METHOD(JoinsTheFacesBoxesShareAndNoOther)
  {
    // A row of three along x, one on the middle one, one that meets the row's end only at an edge, and one alone; in
    // the boxes' own order.
    AssertJoined({POSITIVE_X, static_cast<std::uint8_t>(NEGATIVE_X | POSITIVE_X | POSITIVE_Y), NEGATIVE_X, Engine::JOINED_BOTTOM, 0, 0},
                 {At(0, 5, 0), At(1, 5, 0), At(2, 5, 0), At(1, 6, 0), At(3, 6, 0), At(10, 5, 10)});
  }

  TEST_METHOD(JoinsAlongZAndAcrossTheRenderersBlocks)
  {
    // The renderer splits a layer into 64-voxel blocks after this, so boxes either side of a block's edge still join;
    // and z both ways, the boxes out of order.
    AssertJoined({NEGATIVE_X, static_cast<std::uint8_t>(POSITIVE_X | POSITIVE_Z | NEGATIVE_Z), NEGATIVE_Z, POSITIVE_Z},
                 {At(64, 0, 7), At(63, 0, 7), At(63, 0, 8), At(63, 0, 6)});
  }

  TEST_METHOD(JoinsBelowTheOriginAndNothingWithoutANeighbor)
  {
    AssertJoined({}, {});
    AssertJoined({0}, {At(-3, -1, -2)});
    AssertJoined({POSITIVE_X, NEGATIVE_X}, {At(-1, -1, -2), At(0, -1, -2)});
  }
};

} // namespace ClientTests
