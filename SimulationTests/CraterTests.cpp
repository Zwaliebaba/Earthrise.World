#include "pch.h"

#include "FlatWorld.h"
#include "Shot.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A shot's crater by the struck voxel's material (M2-E2; GameDesign.md, M2's values, item 12).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t COMMON_ROCK = 3;
constexpr std::uint8_t BUILDING_STONE = 4;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t TIMBER = 7;
constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::uint8_t WATER = 9;
constexpr std::uint8_t LEAVES = 10;
constexpr std::uint8_t ROOF = 12;
constexpr std::uint8_t EARTH_RUBBLE = 26;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

/// A 5 x 5 x 5 block of _index about the origin, with _others placed over it.
struct Block
{
  std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::uint8_t> voxels;

  explicit Block(std::uint8_t _index)
  {
    for (std::int32_t y = -2; y <= 2; ++y)
    {
      for (std::int32_t z = -2; z <= 2; ++z)
      {
        for (std::int32_t x = -2; x <= 2; ++x)
        {
          voxels[{x, y, z}] = _index;
        }
      }
    }
  }

  [[nodiscard]] std::uint8_t At(Protocol::VoxelPosition _voxel) const
  {
    const auto found = voxels.find({_voxel.xVoxels, _voxel.yVoxels, _voxel.zVoxels});
    return found == voxels.end() ? std::uint8_t{0} : found->second;
  }

  /// The crater of a shot striking the origin going +x.
  [[nodiscard]] std::vector<Protocol::VoxelPosition> Crater() const
  {
    return Simulation::CraterVoxels(
      At(0, 0, 0), 1, 0, [this](Protocol::VoxelPosition _voxel) { return At(_voxel); }, BakedServerData().Materials(),
      BakedServerData().Tuning());
  }

  [[nodiscard]] static Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    return SimulationTests::At(_x, _y, _z);
  }
};

bool Holds(const std::vector<Protocol::VoxelPosition>& _voxels, Protocol::VoxelPosition _voxel)
{
  return std::ranges::find(_voxels, _voxel) != _voxels.end();
}

} // namespace

TEST_CLASS(CraterTests)
{
public:
  TEST_METHOD(GivesEachMaterialItsShape)
  {
    // Timber: the voxel and its six face neighbours.
    const std::vector<Protocol::VoxelPosition> timber = Block(TIMBER).Crater();
    Assert::AreEqual(std::size_t{7}, timber.size());
    for (const Protocol::VoxelPosition voxel :
         {At(0, 0, 0), At(1, 0, 0), At(-1, 0, 0), At(0, 1, 0), At(0, -1, 0), At(0, 0, 1), At(0, 0, -1)})
    {
      Assert::IsTrue(Holds(timber, voxel));
    }
    // Earth: the voxel, the one behind it along the shot, and the one below each.
    const std::vector<Protocol::VoxelPosition> earth = Block(EARTH).Crater();
    Assert::AreEqual(std::size_t{4}, earth.size());
    for (const Protocol::VoxelPosition voxel : {At(0, 0, 0), At(1, 0, 0), At(0, -1, 0), At(1, -1, 0)})
    {
      Assert::IsTrue(Holds(earth, voxel));
    }
    // Stone: the voxel and the one behind it.
    const std::vector<Protocol::VoxelPosition> stone = Block(BUILDING_STONE).Crater();
    Assert::AreEqual(std::size_t{2}, stone.size());
    Assert::IsTrue(Holds(stone, At(0, 0, 0)) && Holds(stone, At(1, 0, 0)));
    // Along z, behind is along z.
    Block block(BUILDING_STONE);
    const std::vector<Protocol::VoxelPosition> alongZ = Simulation::CraterVoxels(
      At(0, 0, 0), 0, -1, [&block](Protocol::VoxelPosition _voxel) { return block.At(_voxel); }, BakedServerData().Materials(),
      BakedServerData().Tuning());
    Assert::IsTrue(Holds(alongZ, At(0, 0, -1)));
  }

  TEST_METHOD(TakesNoStoneInATimberCrater)
  {
    Block block(MASONRY);
    block.voxels[{0, 0, 0}] = TIMBER;
    block.voxels[{0, 1, 0}] = ROOF; // roof behaves as timber
    const std::vector<Protocol::VoxelPosition> crater = block.Crater();
    Assert::AreEqual(std::size_t{2}, crater.size());
    Assert::IsTrue(Holds(crater, At(0, 0, 0)) && Holds(crater, At(0, 1, 0)));
    // Leaves take leaves only, as timber takes timber.
    Block canopy(TIMBER);
    canopy.voxels[{0, 0, 0}] = LEAVES;
    canopy.voxels[{1, 0, 0}] = LEAVES;
    Assert::AreEqual(std::size_t{2}, canopy.Crater().size());
  }

  TEST_METHOD(GivesTheRestTheirKinsCrater)
  {
    Assert::AreEqual(std::size_t{2}, Block(MASONRY).Crater().size());
    Assert::AreEqual(std::size_t{2}, Block(COMMON_ROCK).Crater().size());
    Assert::AreEqual(std::size_t{4}, Block(STONE_RUBBLE).Crater().size());
    Assert::AreEqual(std::size_t{4}, Block(EARTH_RUBBLE).Crater().size());
    Assert::AreEqual(std::size_t{0}, Block(BEDROCK).Crater().size());
    Assert::AreEqual(std::size_t{0}, Block(WATER).Crater().size());
    // Earth over bedrock: the crater stops at the bedrock.
    Block layered(EARTH);
    layered.voxels[{0, -1, 0}] = BEDROCK;
    layered.voxels[{1, -1, 0}] = BEDROCK;
    Assert::AreEqual(std::size_t{2}, layered.Crater().size());
  }

  TEST_METHOD(EditsTheCraterInTheTick)
  {
    // Slump is off, so the crater stays open to be seen.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const Protocol::EntityId engine = world.AddSiegeEngine(1, Simulation::EngineKind::Trebuchet, At(5, FlatWorld::GROUND_VOXELS, 32));
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::FireEngine{.engine = engine, .targetVoxels = At(45, FlatWorld::GROUND_VOXELS - 1, 32)}, writer);
    world.Submit(1, writer.Bytes());
    for (std::uint32_t tick = 0; tick < 200 && world.LastTickHits().empty(); ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(std::size_t{1}, world.LastTickHits().size());
    const std::int32_t top = FlatWorld::GROUND_VOXELS - 1;
    for (const Protocol::VoxelPosition voxel : {At(45, top, 32), At(46, top, 32), At(45, top - 1, 32), At(46, top - 1, 32)})
    {
      Assert::AreEqual(std::uint8_t{0}, world.Voxels().At(voxel));
      Assert::IsTrue(std::ranges::find(world.LastTickEdits(), voxel) != world.LastTickEdits().end());
    }
    Assert::AreEqual(std::size_t{4}, world.LastTickEdits().size());
  }
};

} // namespace SimulationTests
