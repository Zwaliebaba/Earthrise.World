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
constexpr std::int32_t TOP = Simulation::TestWorld::HEIGHT_VOXELS - 1;
constexpr std::int32_t GROUND = Simulation::TestWorld::CASTLE_GROUND_VOXELS;
constexpr std::uint8_t GRASS = 1;
constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t STONE_RUBBLE = 8;

std::optional<Simulation::TemplateShape> OneVoxel(std::string_view /*_id*/)
{
  static constexpr std::array<std::uint8_t, 1> VOXELS = {MASONRY};
  return Simulation::TemplateShape{.widthVoxels = 1, .heightVoxels = 1, .depthVoxels = 1, .indices = VOXELS};
}

/// Every template as one voxel but T3, the wall, which is its own size: 8 m long, 7 m high and 2 m
/// thick, all masonry, so that the west curtain stands to be breached.
std::optional<Simulation::TemplateShape> Walls(std::string_view _id)
{
  static const std::vector<std::uint8_t> WALL(std::size_t{8} * 7 * 2, MASONRY);
  if (_id == "T3")
  {
    return Simulation::TemplateShape{.widthVoxels = 8, .heightVoxels = 7, .depthVoxels = 2, .indices = WALL};
  }
  return OneVoxel(_id);
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

  TEST_METHOD(MirrorsTheSceneNorthToSouth)
  {
    // The v5 scene's +z is south and the world's is north (ADR-013): the quarry the prototype digs
    // under the castle's south wall, at the scene's (74, 93), is at the world's lower z.
    Simulation::ChunkStore store;
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, SCENE, SEED, OneVoxel);
    Assert::AreEqual(SCENE - 1, world.WorldZVoxels(0));
    Assert::AreEqual(static_cast<float>(SCENE) - 93.5F, world.WorldZMeters(93.5F));
    const std::int32_t pitZ = world.WorldZVoxels(93);
    Assert::IsTrue(static_cast<float>(pitZ) < world.CastleZMeters());
    constexpr std::int32_t TOP = Simulation::TestWorld::HEIGHT_VOXELS - 1;
    constexpr std::int32_t GROUND = Simulation::TestWorld::CASTLE_GROUND_VOXELS;
    Assert::IsTrue(Simulation::TopSolidAtOrBelow(store, 74, pitZ, TOP) < GROUND - 5);
    Assert::IsTrue(Simulation::TopSolidAtOrBelow(store, 74, world.WorldZVoxels(34), TOP) >= GROUND - 2);
    // A turn's sense reverses, and a front facing the scene's +z faces the world's -z.
    for (const auto& [scene, expected] : {std::pair{0U, 2U}, std::pair{1U, 1U}, std::pair{2U, 0U}, std::pair{3U, 3U}})
    {
      Assert::AreEqual(expected, Simulation::TestWorld::WorldQuarterTurns(scene));
    }
  }

  TEST_METHOD(OpensTheBreachFromTheStart)
  {
    // The render-fix package, item 4: a notch through the west curtain's whole thickness, at least
    // 4 m wide where its first meter stands, with a ramp of stone rubble through it.
    Simulation::ChunkStore store;
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, SCENE, SEED, Walls);
    for (const std::int32_t z : {59, 60, 61, 62})
    {
      for (const std::int32_t x : {58, 59})
      {
        const std::int32_t worldX = world.WorldXVoxels(x);
        const std::int32_t worldZ = world.WorldZVoxels(z);
        Assert::AreEqual(MASONRY, store.At({.xVoxels = worldX, .yVoxels = GROUND, .zVoxels = worldZ}));
        Assert::AreEqual(std::uint8_t{0}, store.At({.xVoxels = worldX, .yVoxels = GROUND + 4, .zVoxels = worldZ}));
      }
    }
    Assert::IsFalse(world.breach.fallen.empty());
    Assert::IsTrue(
      std::ranges::all_of(world.breach.fallen, [](const Simulation::FallenVoxel& _fell) { return _fell.paletteIndex == MASONRY; }));
    Assert::IsFalse(world.breach.rubble.empty());
    Assert::IsTrue(std::ranges::all_of(world.breach.rubble,
                                       [&store](const Protocol::VoxelPosition& _voxel) { return store.At(_voxel) == STONE_RUBBLE; }));
    const Simulation::TestWorldEdits edits(store, world, SEED);
    Assert::AreNotEqual(edits.CollapseId(), world.breachId);
  }

  TEST_METHOD(LeavesTheDitchUndugBeforeTheBreach)
  {
    // The render-fix package, item 6: the ditch is 2 m deep and the rampart 2 m high, but neither is
    // dug in the 9 m before the breach.
    Simulation::ChunkStore store;
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, SCENE, SEED, OneVoxel);
    // The ditch's middle is at x 50 and its edge at x 48, short of the breach's rubble ramp; the
    // rampart's crest is at x 54.
    const auto top = [&](std::int32_t _x, std::int32_t _z)
    { return Simulation::TopSolidAtOrBelow(store, world.WorldXVoxels(_x), world.WorldZVoxels(_z), TOP); };
    const auto surface = [&](std::int32_t _x, std::int32_t _z) { return world.SurfaceAt(world.WorldXVoxels(_x), world.WorldZVoxels(_z)); };
    for (const std::int32_t z : {57, 61, 65})
    {
      Assert::AreEqual(surface(50, z) - 1, top(50, z));
      Assert::AreEqual(surface(48, z) - 1, top(48, z));
    }
    for (const std::int32_t z : {40, 70, 80})
    {
      Assert::AreEqual(surface(50, z) - 3, top(50, z));
      Assert::AreEqual(surface(48, z) - 2, top(48, z));
      Assert::AreEqual(surface(54, z) + 1, top(54, z));
    }
  }

  TEST_METHOD(WorksTheQuarryForTheWholeMinute)
  {
    // 94 voxels a second for the measured path's 60 s, and none from the meter before the castle's
    // south towers, at the scene's z of 80.
    Simulation::ChunkStore store;
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, SCENE, SEED, OneVoxel);
    std::vector<std::int32_t> towersFoot;
    for (std::int32_t x = 46; x < 104; ++x)
    {
      towersFoot.push_back(Simulation::TopSolidAtOrBelow(store, world.WorldXVoxels(x), world.WorldZVoxels(80), TOP));
    }
    Simulation::TestWorldEdits edits(store, world, SEED);
    std::size_t changed = 0;
    for (std::uint32_t frame = 0; frame < Simulation::TestWorldEdits::LAST_FRAME; ++frame)
    {
      changed += edits.Apply(store, frame).size();
    }
    const std::size_t collapse = edits.Fallen().size() + edits.Rubble().size();
    constexpr std::size_t MINUTE = std::size_t{Simulation::TestWorldEdits::QUARRY_VOXELS_PER_SECOND} * 60;
    Assert::AreEqual(MINUTE, changed - collapse);
    for (std::int32_t x = 46; x < 104; ++x)
    {
      Assert::AreEqual(towersFoot[static_cast<std::size_t>(x - 46)],
                       Simulation::TopSolidAtOrBelow(store, world.WorldXVoxels(x), world.WorldZVoxels(80), TOP));
    }
  }

  TEST_METHOD(LaysGrassOnlyOnEarth)
  {
    // The render-fix package, item 5: grass is the top of earth, never of stone, and the quarry's rim
    // keeps it so. Below 5 m bedrock may rise to meet it.
    Simulation::ChunkStore store;
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, SCENE, SEED, OneVoxel);
    for (std::int32_t y = 5; y <= TOP; ++y)
    {
      for (std::int32_t z = 0; z < world.edgeVoxels; ++z)
      {
        for (std::int32_t x = 0; x < world.edgeVoxels; ++x)
        {
          if (store.At({.xVoxels = x, .yVoxels = y, .zVoxels = z}) == GRASS)
          {
            Assert::AreEqual(EARTH, store.At({.xVoxels = x, .yVoxels = y - 1, .zVoxels = z}));
          }
        }
      }
    }
  }

  TEST_METHOD(CalmsTheGround)
  {
    // The render-fix package, item 8: no lone bump or pit, and no 1 m ridge or trench, so no column
    // stands above or below both its neighbours along x or along z.
    Simulation::ChunkStore store;
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, SCENE, SEED, OneVoxel);
    for (std::int32_t z = 1; z + 1 < world.edgeVoxels; ++z)
    {
      for (std::int32_t x = 1; x + 1 < world.edgeVoxels; ++x)
      {
        const std::int32_t height = world.SurfaceAt(x, z);
        for (const auto& [before, after] : {std::pair{world.SurfaceAt(x - 1, z), world.SurfaceAt(x + 1, z)},
                                            std::pair{world.SurfaceAt(x, z - 1), world.SurfaceAt(x, z + 1)}})
        {
          Assert::IsFalse(height > before && height > after);
          Assert::IsFalse(height < before && height < after);
        }
      }
    }
  }

  TEST_METHOD(SplitsEachVoxelIntoEight)
  {
    Simulation::ChunkStore oneMeter;
    oneMeter.Set({.xVoxels = 3, .yVoxels = 5, .zVoxels = 7}, MASONRY);
    oneMeter.Set({.xVoxels = 4, .yVoxels = 5, .zVoxels = 7}, 2);
    Simulation::ChunkStore halfMeter;
    Simulation::SplitToHalfMeters(oneMeter, 0, 0, 8, 8, halfMeter);
    for (const Protocol::VoxelPosition& half : Simulation::HalfMeterVoxels({.xVoxels = 3, .yVoxels = 5, .zVoxels = 7}))
    {
      Assert::IsTrue(half.xVoxels / 2 == 3 && half.yVoxels / 2 == 5 && half.zVoxels / 2 == 7);
      Assert::AreEqual(MASONRY, halfMeter.At(half));
    }
    Assert::AreEqual(std::uint8_t{2}, halfMeter.At({.xVoxels = 9, .yVoxels = 11, .zVoxels = 15}));
    Assert::AreEqual(std::uint8_t{0}, halfMeter.At({.xVoxels = 5, .yVoxels = 11, .zVoxels = 15}));
  }

  TEST_METHOD(SplitsAnEditAsTheWorldWouldBeSplit)
  {
    Simulation::ChunkStore oneMeter;
    oneMeter.Set({.xVoxels = 3, .yVoxels = 5, .zVoxels = 7}, MASONRY);
    Simulation::ChunkStore halfMeter;
    Simulation::SplitToHalfMeters(oneMeter, 0, 0, 8, 8, halfMeter);
    const std::array<Protocol::VoxelPosition, 2> changed = {
      {{.xVoxels = 3, .yVoxels = 5, .zVoxels = 7}, {.xVoxels = 3, .yVoxels = 6, .zVoxels = 7}}};
    oneMeter.Set(changed[0], 0);
    oneMeter.Set(changed[1], MASONRY);
    Simulation::SplitChanges(oneMeter, changed, halfMeter);
    Simulation::ChunkStore fresh;
    Simulation::SplitToHalfMeters(oneMeter, 0, 0, 8, 8, fresh);
    Assert::IsTrue(SameVoxels(halfMeter, fresh));
    Assert::AreEqual(std::uint8_t{0}, halfMeter.At({.xVoxels = 6, .yVoxels = 10, .zVoxels = 14}));
    Assert::AreEqual(MASONRY, halfMeter.At({.xVoxels = 7, .yVoxels = 13, .zVoxels = 15}));
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
