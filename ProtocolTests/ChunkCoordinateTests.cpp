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
};

} // namespace ProtocolTests
