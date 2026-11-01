#include "pch.h"

#include "ChunkStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t STONE = 6;
constexpr std::uint8_t EARTH = 4;

const Protocol::Chunk& VoxelsOf(const Protocol::ChunkSnapshot& _snapshot)
{
  Assert::IsNotNull(_snapshot.voxels.get());
  return *_snapshot.voxels;
}

} // namespace

TEST_CLASS(ChunkStoreTests)
{
public:
  TEST_METHOD(StartsEmptyWithNothingToPublish)
  {
    Simulation::ChunkStore store;
    Assert::AreEqual(Protocol::Chunk::EMPTY, store.At({.xVoxels = 5, .yVoxels = -3, .zVoxels = 900}));
    Assert::IsTrue(store.Publish().empty());
    Assert::IsTrue(store.Snapshots().empty());
    Assert::AreEqual(std::uint64_t{0}, store.Version());
  }

  TEST_METHOD(ShowsAnEditToTheClientOnlyOncePublished)
  {
    Simulation::ChunkStore store;
    store.Set({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}, STONE);
    Assert::AreEqual(STONE, store.At({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}));
    Assert::IsTrue(store.Snapshots().empty());

    const std::vector<Protocol::ChunkSnapshot> published = store.Publish();
    Assert::AreEqual(std::size_t{1}, published.size());
    Assert::IsTrue(published[0].coordinate == Protocol::ChunkCoordinate{.xChunks = 0, .yChunks = 0, .zChunks = 0});
    Assert::AreEqual(std::uint64_t{1}, published[0].version);
    Assert::AreEqual(STONE, VoxelsOf(published[0]).At(1, 2, 3));
    Assert::AreEqual(std::size_t{1}, store.Snapshots().size());
  }

  TEST_METHOD(NeverChangesAPublishedSnapshot)
  {
    Simulation::ChunkStore store;
    store.Set({.xVoxels = 0, .yVoxels = 0, .zVoxels = 0}, STONE);
    const Protocol::ChunkSnapshot first = store.Publish().front();

    store.Set({.xVoxels = 0, .yVoxels = 0, .zVoxels = 0}, Protocol::Chunk::EMPTY);
    store.Set({.xVoxels = 1, .yVoxels = 0, .zVoxels = 0}, EARTH);
    Assert::AreEqual(STONE, VoxelsOf(first).At(0, 0, 0));
    Assert::AreEqual(Protocol::Chunk::EMPTY, VoxelsOf(first).At(1, 0, 0));
    Assert::IsTrue(store.Snapshots().front().voxels == first.voxels, L"a client that joins now gets what was published");

    const Protocol::ChunkSnapshot second = store.Publish().front();
    Assert::AreEqual(std::uint64_t{2}, second.version);
    Assert::AreEqual(Protocol::Chunk::EMPTY, VoxelsOf(second).At(0, 0, 0));
    Assert::AreEqual(EARTH, VoxelsOf(second).At(1, 0, 0));
    Assert::AreEqual(STONE, VoxelsOf(first).At(0, 0, 0));
  }

  TEST_METHOD(ACopyNeverSharesAChunkBeingChanged)
  {
    // A chunk edited and not yet published is the store's own: a copy takes its own copy of it, so an
    // edit to either store leaves the other as it was, published or not.
    Simulation::ChunkStore store;
    store.Set({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}, STONE);
    Simulation::ChunkStore copy = store;
    copy.Set({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}, EARTH);
    copy.Set({.xVoxels = 4, .yVoxels = 2, .zVoxels = 3}, EARTH);
    Assert::AreEqual(STONE, store.At({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}));
    Assert::AreEqual(Protocol::Chunk::EMPTY, store.At({.xVoxels = 4, .yVoxels = 2, .zVoxels = 3}));
    Simulation::ChunkStore assigned;
    assigned = store;
    store.Set({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}, Protocol::Chunk::EMPTY);
    Assert::AreEqual(STONE, assigned.At({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}));
    Assert::AreEqual(EARTH, copy.At({.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}));
  }

  TEST_METHOD(SharesOneCopyUntilTheNextEdit)
  {
    Simulation::ChunkStore store;
    store.Set({.xVoxels = 0, .yVoxels = 0, .zVoxels = 0}, STONE);
    const Protocol::ChunkSnapshot published = store.Publish().front();
    Assert::IsTrue(store.Snapshots().front().voxels == published.voxels);

    store.Set({.xVoxels = 2, .yVoxels = 0, .zVoxels = 0}, STONE);
    Assert::IsTrue(store.Publish().front().voxels != published.voxels);
  }

  TEST_METHOD(RepublishesOnlyWhatChanged)
  {
    Simulation::ChunkStore store;
    store.Set({.xVoxels = 0, .yVoxels = 0, .zVoxels = 0}, STONE);
    store.Set({.xVoxels = 40, .yVoxels = 0, .zVoxels = 0}, STONE);
    Assert::AreEqual(std::size_t{2}, store.Publish().size());
    Assert::IsTrue(store.Publish().empty());
    Assert::AreEqual(std::uint64_t{1}, store.Version());

    store.Set({.xVoxels = 41, .yVoxels = 0, .zVoxels = 0}, EARTH);
    store.Set({.xVoxels = 0, .yVoxels = 0, .zVoxels = 0}, STONE); // already stone, so no change
    const std::vector<Protocol::ChunkSnapshot> published = store.Publish();
    Assert::AreEqual(std::size_t{1}, published.size());
    Assert::AreEqual(1, published[0].coordinate.xChunks);
    Assert::AreEqual(std::uint64_t{2}, published[0].version);

    const std::vector<Protocol::ChunkSnapshot> all = store.Snapshots();
    Assert::AreEqual(std::size_t{2}, all.size());
    Assert::AreEqual(std::uint64_t{1}, all[0].version);
    Assert::AreEqual(std::uint64_t{2}, all[1].version);
  }

  TEST_METHOD(PublishesInKeyOrderWhateverTheEditOrder)
  {
    Simulation::ChunkStore store;
    const Protocol::VoxelPosition positions[] = {{.xVoxels = 100, .yVoxels = 0, .zVoxels = 0},
                                                 {.xVoxels = -50, .yVoxels = 0, .zVoxels = 0},
                                                 {.xVoxels = 0, .yVoxels = 70, .zVoxels = 0},
                                                 {.xVoxels = 0, .yVoxels = 0, .zVoxels = -1}};
    for (const Protocol::VoxelPosition& position : positions)
    {
      store.Set(position, STONE);
    }
    const std::vector<Protocol::ChunkSnapshot> published = store.Publish();
    Assert::AreEqual(std::size(positions), published.size());
    for (std::size_t index = 1; index < published.size(); ++index)
    {
      Assert::IsTrue(Protocol::PackChunkKey(published[index - 1].coordinate) < Protocol::PackChunkKey(published[index].coordinate));
    }
  }

  TEST_METHOD(PutsNegativeVoxelsInTheChunkBelow)
  {
    Simulation::ChunkStore store;
    store.Set({.xVoxels = -1, .yVoxels = -1, .zVoxels = -1}, STONE);
    const Protocol::ChunkSnapshot snapshot = store.Publish().front();
    Assert::IsTrue(snapshot.coordinate == Protocol::ChunkCoordinate{.xChunks = -1, .yChunks = -1, .zChunks = -1});
    Assert::AreEqual(STONE, VoxelsOf(snapshot).At(31, 31, 31));
    Assert::AreEqual(Protocol::Chunk::EMPTY, store.At({.xVoxels = 0, .yVoxels = 0, .zVoxels = 0}));
  }

  TEST_METHOD(ClearingAVoxelNobodySetCreatesNothing)
  {
    Simulation::ChunkStore store;
    store.Set({.xVoxels = 0, .yVoxels = 0, .zVoxels = 0}, Protocol::Chunk::EMPTY);
    Assert::IsTrue(store.Publish().empty());
  }

  TEST_METHOD(RefusesVoxelsBeyondTheChunkKey)
  {
    Simulation::ChunkStore store;
    constexpr std::int32_t BEYOND_VOXELS = (Protocol::CHUNK_KEY_AXIS_MAX_CHUNKS + 1) * Protocol::CHUNK_EDGE_VOXELS;
    Assert::ExpectException<std::out_of_range>([&store] { store.Set({.xVoxels = BEYOND_VOXELS, .yVoxels = 0, .zVoxels = 0}, STONE); });
    Assert::AreEqual(Protocol::Chunk::EMPTY, store.At({.xVoxels = BEYOND_VOXELS, .yVoxels = 0, .zVoxels = 0}));
  }
};

} // namespace SimulationTests
