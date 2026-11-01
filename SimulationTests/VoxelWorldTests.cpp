#include "pch.h"

#include "Hash.h"
#include "VoxelWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The one editing call of ADR-014, decisions 1 and 2.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t MASONRY = 6;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

std::uint64_t KeyOf(Protocol::VoxelPosition _position)
{
  return Protocol::PackChunkKey(Protocol::ChunkContaining(_position));
}

/// A world whose base is a slab of earth 4 deep over two chunks.
void FillBase(Simulation::VoxelWorld& _world)
{
  Simulation::ChunkStore& base = _world.Base();
  for (std::int32_t y = 0; y < 4; ++y)
  {
    for (std::int32_t z = 0; z < 8; ++z)
    {
      for (std::int32_t x = 0; x < 40; ++x)
      {
        base.Set(At(x, y, z), EARTH);
      }
    }
  }
}

} // namespace

TEST_CLASS(VoxelWorldTests)
{
public:
  TEST_METHOD(AnEditToTheSameValueChangesNothing)
  {
    Simulation::VoxelWorld world;
    FillBase(world);
    world.Edit(At(1, 1, 1), EARTH);
    Assert::IsTrue(world.TickEdits().empty());
    Assert::IsTrue(world.EditedChunks().empty());
    Assert::AreEqual(std::size_t{2}, world.Publish().size()); // the base, two chunks, published once
    Assert::IsTrue(world.Publish().empty());
  }

  TEST_METHOD(ListsEachEditedVoxelOnceInFirstEditOrder)
  {
    Simulation::VoxelWorld world;
    FillBase(world);
    world.Edit(At(3, 3, 3), 0);
    world.Edit(At(1, 1, 1), MASONRY);
    world.Edit(At(3, 3, 3), MASONRY);
    world.Edit(At(1, 1, 1), 0);
    Assert::AreEqual(std::size_t{2}, world.TickEdits().size());
    Assert::IsTrue(world.TickEdits()[0] == At(3, 3, 3));
    Assert::IsTrue(world.TickEdits()[1] == At(1, 1, 1));
    world.EndTick();
    Assert::IsTrue(world.TickEdits().empty());
    world.Edit(At(1, 1, 1), EARTH);
    Assert::AreEqual(std::size_t{1}, world.TickEdits().size());
  }

  TEST_METHOD(RefusesTheBaseAfterTheFirstEdit)
  {
    Simulation::VoxelWorld world;
    FillBase(world);
    static_cast<void>(world.Base());
    world.Edit(At(0, 0, 0), 0);
    Assert::ExpectException<std::logic_error>([&world] { static_cast<void>(world.Base()); });
  }

  TEST_METHOD(KeepsAChunkDugAndRefilledEdited)
  {
    Simulation::VoxelWorld world;
    FillBase(world);
    world.Edit(At(35, 2, 2), 0);
    world.Edit(At(35, 2, 2), EARTH);
    Assert::AreEqual(std::uint8_t{EARTH}, world.At(At(35, 2, 2)));
    Assert::AreEqual(std::size_t{1}, world.EditedChunks().size());
    Assert::IsTrue(world.EditedChunks().contains(KeyOf(At(35, 2, 2))));
  }

  TEST_METHOD(HashesAChunkByItsVoxels)
  {
    Simulation::VoxelWorld world;
    FillBase(world);
    const std::uint64_t key = KeyOf(At(1, 1, 1));
    const std::uint64_t before = world.ChunkHash(key);
    world.Edit(At(1, 1, 1), MASONRY);
    const std::uint64_t after = world.ChunkHash(key);
    Assert::AreNotEqual(before, after);
    Assert::AreEqual(Core::Fnv1a64(std::as_bytes(world.Store().Find(key)->Voxels())), after);
    Assert::AreEqual(std::uint64_t{0}, world.ChunkHash(KeyOf(At(500, 0, 0))));
  }

  TEST_METHOD(HashesTheSameVoxelsTheSameWhateverTheOrder)
  {
    Simulation::VoxelWorld first;
    Simulation::VoxelWorld second;
    FillBase(first);
    FillBase(second);
    static_cast<void>(first.VoxelHash());
    first.Edit(At(1, 1, 1), MASONRY);
    first.Edit(At(36, 1, 1), 0);
    first.Edit(At(2, 2, 2), 0);
    second.Edit(At(2, 2, 2), MASONRY);
    second.Edit(At(36, 1, 1), 0);
    second.Edit(At(1, 1, 1), MASONRY);
    second.Edit(At(2, 2, 2), 0);
    Assert::AreEqual(first.VoxelHash(), second.VoxelHash());
    second.Edit(At(3, 3, 3), 0);
    Assert::AreNotEqual(first.VoxelHash(), second.VoxelHash());
  }

  TEST_METHOD(CountsAChunkDugEmptyAsAbsent)
  {
    Simulation::VoxelWorld empty;
    Simulation::VoxelWorld dug;
    dug.Edit(At(100, 0, 0), MASONRY);
    dug.Edit(At(100, 0, 0), 0);
    Assert::AreEqual(empty.VoxelHash(), dug.VoxelHash());
  }
};

} // namespace SimulationTests
