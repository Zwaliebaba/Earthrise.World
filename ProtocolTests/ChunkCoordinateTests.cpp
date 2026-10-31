#include "pch.h"

#include "ChunkCoordinate.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ProtocolTests
{

TEST_CLASS(ChunkCoordinateTests)
{
public:
  TEST_METHOD(RoundTripsEveryOctant)
  {
    constexpr std::int32_t LIMIT = (1 << (Protocol::CHUNK_KEY_AXIS_BITS - 1)) - 1;
    const Protocol::ChunkCoordinate coordinates[] = {{.xChunks = 0, .yChunks = 0, .zChunks = 0},
                                                     {.xChunks = 1, .yChunks = 2, .zChunks = 3},
                                                     {.xChunks = -1, .yChunks = -2, .zChunks = -3},
                                                     {.xChunks = LIMIT, .yChunks = -LIMIT - 1, .zChunks = 17},
                                                     {.xChunks = -LIMIT - 1, .yChunks = LIMIT, .zChunks = -63}};
    for (const Protocol::ChunkCoordinate& coordinate : coordinates)
    {
      Assert::IsTrue(coordinate == Protocol::UnpackChunkKey(Protocol::PackChunkKey(coordinate)));
    }
  }

  TEST_METHOD(GivesNeighborsDistinctKeys)
  {
    const std::uint64_t origin = Protocol::PackChunkKey({.xChunks = 0, .yChunks = 0, .zChunks = 0});
    Assert::AreNotEqual(origin, Protocol::PackChunkKey({.xChunks = -1, .yChunks = 0, .zChunks = 0}));
    Assert::AreNotEqual(origin, Protocol::PackChunkKey({.xChunks = 0, .yChunks = -1, .zChunks = 0}));
    Assert::AreNotEqual(Protocol::PackChunkKey({.xChunks = 1, .yChunks = 0, .zChunks = 0}),
                        Protocol::PackChunkKey({.xChunks = 0, .yChunks = 1, .zChunks = 0}));
  }

  TEST_METHOD(KnowsWhichCoordinatesTheKeyHolds)
  {
    Assert::IsTrue(Protocol::FitsChunkKey(
      {.xChunks = Protocol::CHUNK_KEY_AXIS_MAX_CHUNKS, .yChunks = Protocol::CHUNK_KEY_AXIS_MIN_CHUNKS, .zChunks = 0}));
    Assert::IsFalse(Protocol::FitsChunkKey({.xChunks = Protocol::CHUNK_KEY_AXIS_MAX_CHUNKS + 1, .yChunks = 0, .zChunks = 0}));
    Assert::IsFalse(Protocol::FitsChunkKey({.xChunks = 0, .yChunks = 0, .zChunks = Protocol::CHUNK_KEY_AXIS_MIN_CHUNKS - 1}));
  }

  TEST_METHOD(RoundsNegativeVoxelsDown)
  {
    Assert::IsTrue(Protocol::ChunkContaining({.xVoxels = -1, .yVoxels = 0, .zVoxels = 31}) ==
                   Protocol::ChunkCoordinate{.xChunks = -1, .yChunks = 0, .zChunks = 0});
    Assert::IsTrue(Protocol::ChunkContaining({.xVoxels = -32, .yVoxels = -33, .zVoxels = 32}) ==
                   Protocol::ChunkCoordinate{.xChunks = -1, .yChunks = -2, .zChunks = 1});
    Assert::IsTrue(Protocol::PositionInChunk({.xVoxels = -1, .yVoxels = -32, .zVoxels = 33}) ==
                   Protocol::VoxelPosition{.xVoxels = 31, .yVoxels = 0, .zVoxels = 1});
  }

  TEST_METHOD(RebuildsEveryVoxelFromItsChunkAndOffset)
  {
    constexpr std::int32_t EDGE = Protocol::CHUNK_EDGE_VOXELS;
    const std::int32_t extremes[] = {INT32_MIN, INT32_MIN + 1, INT32_MAX - 1, INT32_MAX};
    std::vector<std::int32_t> values(std::begin(extremes), std::end(extremes));
    for (std::int32_t voxels = -100; voxels <= 100; ++voxels)
    {
      values.push_back(voxels);
    }
    for (const std::int32_t voxels : values)
    {
      const Protocol::VoxelPosition position{.xVoxels = voxels, .yVoxels = voxels / 3, .zVoxels = -(voxels / 2)};
      const Protocol::ChunkCoordinate chunk = Protocol::ChunkContaining(position);
      const Protocol::VoxelPosition inside = Protocol::PositionInChunk(position);
      Assert::IsTrue(inside.xVoxels >= 0 && inside.xVoxels < EDGE && inside.yVoxels >= 0 && inside.yVoxels < EDGE && inside.zVoxels >= 0 &&
                     inside.zVoxels < EDGE);
      Assert::AreEqual(std::int64_t{position.xVoxels}, std::int64_t{chunk.xChunks} * EDGE + inside.xVoxels);
      Assert::AreEqual(std::int64_t{position.yVoxels}, std::int64_t{chunk.yChunks} * EDGE + inside.yVoxels);
      Assert::AreEqual(std::int64_t{position.zVoxels}, std::int64_t{chunk.zChunks} * EDGE + inside.zVoxels);
    }
  }
};

} // namespace ProtocolTests
