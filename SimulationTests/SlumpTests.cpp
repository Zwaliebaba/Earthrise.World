#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Slump (M2-E6; GameDesign.md, M2's values, item 9), on the flat world: earth's top voxels at y 7.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t TOP = GROUND - 1;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

void Fill(Simulation::World& _world, Protocol::VoxelPosition _low, Protocol::VoxelPosition _high, std::uint8_t _index)
{
  for (std::int32_t y = _low.yVoxels; y <= _high.yVoxels; ++y)
  {
    for (std::int32_t z = _low.zVoxels; z <= _high.zVoxels; ++z)
    {
      for (std::int32_t x = _low.xVoxels; x <= _high.xVoxels; ++x)
      {
        _world.Voxels().Edit(At(x, y, z), _index);
      }
    }
  }
}

std::int32_t Top(const Simulation::World& _world, std::int32_t _x, std::int32_t _z)
{
  for (std::int32_t y = 63; y >= 0; --y)
  {
    if (_world.Voxels().At(At(_x, y, _z)) != 0)
    {
      return y;
    }
  }
  return -1;
}

std::size_t Count(const Simulation::World& _world, std::uint8_t _index)
{
  std::size_t count = 0;
  for (std::int32_t y = 0; y < 16; ++y)
  {
    for (std::int32_t z = 0; z < 64; ++z)
    {
      for (std::int32_t x = 0; x < 64; ++x)
      {
        if (_world.Voxels().At(At(x, y, z)) == _index)
        {
          ++count;
        }
      }
    }
  }
  return count;
}

/// Ticks until slump has made no edit for a tick, and returns how many ran.
std::uint32_t Settle(Simulation::World& _world, std::uint32_t _most = 2000)
{
  for (std::uint32_t tick = 1; tick <= _most; ++tick)
  {
    _world.Tick();
    if (_world.LastTickEdits().empty())
    {
      return tick;
    }
  }
  return _most;
}

/// Whether, in the square from _low to _high, no column whose top is earth stands more than a voxel
/// above a four-neighbour.
bool SlopesOfOne(const Simulation::World& _world, std::int32_t _low, std::int32_t _high)
{
  for (std::int32_t z = _low; z <= _high; ++z)
  {
    for (std::int32_t x = _low; x <= _high; ++x)
    {
      const std::int32_t top = Top(_world, x, z);
      if (_world.Voxels().At(At(x, top, z)) != EARTH)
      {
        continue;
      }
      for (const auto& [dx, dz] : std::array<std::pair<std::int32_t, std::int32_t>, 4>{{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}})
      {
        if (top > Top(_world, x + dx, z + dz) + 1)
        {
          return false;
        }
      }
    }
  }
  return true;
}

} // namespace

TEST_CLASS(SlumpTests)
{
public:
  TEST_METHOD(SettlesAPitToSlopesOfOne)
  {
    Simulation::World world(FlatWorld::Desc());
    const std::size_t earth = Count(world, EARTH);
    Fill(world, At(20, TOP - 3, 20), At(20, TOP, 20), 0);
    Assert::IsTrue(Settle(world) < 2000);
    Assert::IsTrue(SlopesOfOne(world, 10, 30));
    Assert::AreEqual(earth - 4, Count(world, EARTH)); // nothing lost but what was dug
    Assert::IsTrue(Top(world, 20, 20) >= TOP - 2);
  }

  TEST_METHOD(MovesOneVoxelATickWithABudgetOfOne)
  {
    const Simulation::ServerData one = BakedServerDataWith("slumpVoxelsPerTick", 1);
    Simulation::World world(FlatWorld::Desc(2, 2, &one));
    Fill(world, At(20, TOP - 3, 20), At(20, TOP, 20), 0);
    world.Tick();
    // The four dug, then one move: from the lowest-index column beside the pit, (20, 19), north into
    // the pit, landing on its floor, a voxel already among the tick's edits, which lists each once.
    Assert::AreEqual(std::size_t{5}, world.LastTickEdits().size());
    Assert::IsTrue(world.LastTickEdits()[4] == At(20, TOP, 19));
    Assert::AreEqual(std::uint8_t{0}, world.Voxels().At(At(20, TOP, 19)));
    Assert::AreEqual(EARTH, world.Voxels().At(At(20, TOP - 3, 20)));
    std::uint32_t moves = 1;
    for (std::uint32_t tick = 0; tick < 200; ++tick)
    {
      world.Tick();
      if (world.LastTickEdits().empty())
      {
        break;
      }
      Assert::AreEqual(std::size_t{2}, world.LastTickEdits().size());
      ++moves;
    }
    Assert::AreEqual(std::uint32_t{4}, moves); // twice from south of the pit, once from beyond that, once from the west
    Assert::IsTrue(SlopesOfOne(world, 10, 30));
  }

  TEST_METHOD(StartsACollapseTheTickAfterSlumpUnderminesIt)
  {
    // A masonry block held up only by an earth mound beside it: as the mound slumps away, the block
    // loses its support, and its collapse starts on the tick after the slump that took it.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, At(10, GROUND + 2, 10), At(11, GROUND + 3, 10), MASONRY);
    Fill(world, At(12, GROUND, 10), At(13, GROUND + 3, 10), EARTH);
    std::optional<std::uint32_t> slumped;
    for (std::uint32_t tick = 1; tick < 400 && world.Collapses().empty(); ++tick)
    {
      world.Tick();
      const bool gone = world.Voxels().At(At(12, GROUND + 2, 10)) == 0 && world.Voxels().At(At(12, GROUND + 3, 10)) == 0;
      if (gone && !slumped.has_value())
      {
        slumped = tick;
        Assert::IsTrue(world.Collapses().empty()); // not on the same tick
      }
      if (!world.Collapses().empty())
      {
        Assert::IsTrue(slumped.has_value());
        Assert::AreEqual(slumped.value_or(0) + 1, tick);
      }
    }
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    Assert::AreEqual(std::size_t{4}, world.Collapses().begin()->second.piece.size());
  }

  TEST_METHOD(ClosesATrebuchetsEarthCrater)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(1, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32));
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::FireEngine{.engine = engine, .targetVoxels = At(45, TOP, 32)}, writer);
    world.Submit(1, writer.Bytes());
    for (std::uint32_t tick = 0; tick < 200 && world.LastTickHits().empty(); ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(std::size_t{1}, world.LastTickHits().size());
    Settle(world);
    // The 2 m deep crater has spread into a dip no more than a voxel deep, every slope of one.
    Assert::IsTrue(SlopesOfOne(world, 35, 55));
    for (std::int32_t x = 40; x <= 50; ++x)
    {
      Assert::IsTrue(Top(world, x, 32) >= TOP - 1);
    }
  }

  TEST_METHOD(ContinuesFromASaveMidSlump)
  {
    const Simulation::ServerData one = BakedServerDataWith("slumpVoxelsPerTick", 1);
    Simulation::World straight(FlatWorld::Desc(2, 2, &one));
    Fill(straight, At(20, TOP - 3, 20), At(20, TOP, 20), 0);
    const std::uint32_t total = Settle(straight);

    Simulation::World before(FlatWorld::Desc(2, 2, &one));
    Fill(before, At(20, TOP - 3, 20), At(20, TOP, 20), 0);
    for (std::uint32_t tick = 0; tick < 3; ++tick)
    {
      before.Tick();
    }
    Simulation::World after = Simulation::LoadSave(Simulation::WriteSave(before), FlatWorld::Desc(2, 2, &one));
    Assert::AreEqual(Simulation::StateHash(before), Simulation::StateHash(after));
    Assert::AreEqual(total - 3, Settle(after));
    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(after));
  }
};

} // namespace SimulationTests
