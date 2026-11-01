#include "pch.h"

#include "ChunkStore.h"
#include "TestWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The test world the renderer spike draws and SimulationBench measures (MvpPlan.md, M1 tasks 9 and
// 11), built here with every template as one voxel: SimulationTests reads no client content, and
// the lookup is what keeps Simulation from needing any (GameDesign.md, M1's open values, item 9).

namespace SimulationTests
{

namespace
{

constexpr std::uint64_t SEED = 0x5eed2026ULL;
constexpr std::int32_t SCENE = Simulation::TestWorld::SCENE_EDGE_VOXELS;
constexpr std::uint8_t MASONRY = 6;

std::optional<Simulation::TemplateShape> OneVoxel(std::string_view /*_id*/)
{
  static constexpr std::array<std::uint8_t, 1> VOXELS = {MASONRY};
  return Simulation::TemplateShape{.widthVoxels = 1, .heightVoxels = 1, .depthVoxels = 1, .indices = VOXELS};
}

std::optional<Simulation::TemplateShape> NoTemplates(std::string_view /*_id*/)
{
  return std::nullopt;
}

/// Whether two stores published the same chunks, voxel for voxel.
bool SameVoxels(Simulation::ChunkStore& _first, Simulation::ChunkStore& _second)
{
  const std::vector<Protocol::ChunkSnapshot> first = _first.Publish();
  const std::vector<Protocol::ChunkSnapshot> second = _second.Publish();
  return std::ranges::equal(first, second, [](const Protocol::ChunkSnapshot& _a, const Protocol::ChunkSnapshot& _b)
                            { return _a.coordinate == _b.coordinate && std::ranges::equal(_a.voxels->Voxels(), _b.voxels->Voxels()); });
}

} // namespace

TEST_CLASS(TestWorldTests)
{
public:
  TEST_METHOD(BuildsTheSameWorldFromTheSameSeed)
  {
    Simulation::ChunkStore first;
    Simulation::ChunkStore second;
    const Simulation::TestWorld firstWorld = Simulation::BuildTestWorld(first, SCENE, SEED, OneVoxel);
    const Simulation::TestWorld secondWorld = Simulation::BuildTestWorld(second, SCENE, SEED, OneVoxel);
    Assert::IsTrue(firstWorld.surfaceVoxels == secondWorld.surfaceVoxels);
    Assert::AreEqual(firstWorld.editedVoxels, secondWorld.editedVoxels);
    Assert::IsTrue(SameVoxels(first, second));
  }

  TEST_METHOD(BuildsADifferentFieldFromAnotherSeed)
  {
    Simulation::ChunkStore first;
    Simulation::ChunkStore second;
    const Simulation::TestWorld firstWorld = Simulation::BuildTestWorld(first, SCENE, SEED, OneVoxel);
    const Simulation::TestWorld secondWorld = Simulation::BuildTestWorld(second, SCENE, SEED + 1, OneVoxel);
    Assert::IsFalse(firstWorld.surfaceVoxels == secondWorld.surfaceVoxels);
  }

  TEST_METHOD(RaisesThePlateauUnderTheCastle)
  {
    // The castle stands on ground the generator made, wherever the scene sits in the world.
    for (const std::int32_t edge : {SCENE, 2 * SCENE})
    {
      Simulation::ChunkStore store;
      const Simulation::TestWorld world = Simulation::BuildTestWorld(store, edge, SEED, OneVoxel);
      const auto castleX = static_cast<std::int32_t>(world.CastleXMeters());
      const auto castleZ = static_cast<std::int32_t>(world.CastleZMeters());
      Assert::AreEqual(Simulation::TestWorld::CASTLE_GROUND_VOXELS, world.SurfaceAt(castleX, castleZ));
      Assert::AreEqual(Simulation::TestWorld::CASTLE_GROUND_VOXELS - 1,
                       Simulation::TopSolidAtOrBelow(store, castleX, castleZ, Simulation::TestWorld::CASTLE_GROUND_VOXELS - 1));
      Assert::IsTrue(world.editedVoxels > 0);
    }
  }

  TEST_METHOD(RefusesAMissingTemplateAndABadEdge)
  {
    Simulation::ChunkStore store;
    Assert::ExpectException<std::runtime_error>([&] { static_cast<void>(Simulation::BuildTestWorld(store, SCENE, SEED, NoTemplates)); });
    Assert::ExpectException<std::invalid_argument>([&]
                                                   { static_cast<void>(Simulation::BuildTestWorld(store, SCENE / 2, SEED, OneVoxel)); });
    Assert::ExpectException<std::invalid_argument>([&]
                                                   { static_cast<void>(Simulation::BuildTestWorld(store, SCENE + 1, SEED, OneVoxel)); });
    Assert::ExpectException<std::invalid_argument>([&]
                                                   { static_cast<void>(Simulation::BuildSeasonWorld(store, 2048, 7, SEED, OneVoxel)); });
  }
};

} // namespace SimulationTests
