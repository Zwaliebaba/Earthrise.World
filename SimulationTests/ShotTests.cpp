#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"
#include "Script.h"
#include "Shot.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The trebuchet's shot (M2-E1; GameDesign.md, M2's values, item 12), on the flat world: its ground's
// top voxels lie at y 7, so a unit or an engine stands at y 8.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t TOP = FlatWorld::GROUND_VOXELS - 1;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

std::vector<std::byte> Fire(Protocol::EntityId _engine, Protocol::VoxelPosition _target)
{
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::FireEngine{.engine = _engine, .targetVoxels = _target}, writer);
  return writer.TakeBytes();
}

/// Runs until a hit or _ticks ticks, and returns the hit.
std::optional<Simulation::ShotHit> RunToHit(Simulation::World& _world, std::uint32_t _ticks = 200)
{
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    _world.Tick();
    if (!_world.LastTickHits().empty())
    {
      return _world.LastTickHits().front();
    }
  }
  return std::nullopt;
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

} // namespace

TEST_CLASS(ShotTests)
{
public:
  TEST_METHOD(StrikesOpenGroundInTheTargetsColumn)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, At(5, FlatWorld::GROUND_VOXELS, 32));
    world.Submit(PLAYER, Fire(engine, At(45, TOP, 32)));
    const std::optional<Simulation::ShotHit> hit = RunToHit(world);
    Assert::IsTrue(hit.has_value() && hit->voxel == At(45, TOP, 32));
    Assert::AreEqual(1, hit.value_or(Simulation::ShotHit{}).behindX);
    Assert::IsTrue(world.Shots().empty());

    // Across a diagonal it strikes the target's column, or the one before it on the way.
    world.Submit(PLAYER, Fire(engine, At(30, TOP, 61)));
    for (std::uint32_t tick = 0; tick < 400; ++tick)
    {
      world.Tick();
    }
    world.Submit(PLAYER, Fire(engine, At(30, TOP, 61)));
    const std::optional<Simulation::ShotHit> diagonal = RunToHit(world);
    Assert::IsTrue(diagonal.has_value());
    const Simulation::ShotHit found = diagonal.value_or(Simulation::ShotHit{});
    Assert::AreEqual(TOP, found.voxel.yVoxels);
    Assert::IsTrue(found.voxel.xVoxels >= 29 && found.voxel.xVoxels <= 30 && found.voxel.zVoxels >= 60 && found.voxel.zVoxels <= 61);
    Assert::AreEqual(1, found.behindZ); // 25 across x against 29 across z: z is larger
  }

  TEST_METHOD(StrikesAWallInTheWayOnItsFace)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, At(5, FlatWorld::GROUND_VOXELS, 32));
    Fill(world, At(30, FlatWorld::GROUND_VOXELS, 28), At(31, 25, 36), MASONRY);
    world.Submit(PLAYER, Fire(engine, At(45, TOP, 32)));
    const Simulation::ShotHit hit = RunToHit(world).value_or(Simulation::ShotHit{});
    Assert::AreEqual(30, hit.voxel.xVoxels); // the face towards the engine
    Assert::AreEqual(32, hit.voxel.zVoxels);
    Assert::IsTrue(hit.voxel.yVoxels > FlatWorld::GROUND_VOXELS);
  }

  TEST_METHOD(HitsAOneVoxelPostItsStepSpans)
  {
    // Find a step of a diagonal shot that clips a column neither of its ends lies in, and put a post
    // there: the traversal must find it.
    const Protocol::VoxelPosition engineCell = At(3, FlatWorld::GROUND_VOXELS, 3);
    const Protocol::VoxelPosition target = At(36, TOP, 40);
    const Simulation::ShotArc arc = Simulation::AimShot(engineCell, target, BakedServerData().Tuning()).value_or(Simulation::ShotArc{});
    Assert::IsTrue(arc.flightTicks.Raw() > 0);
    std::optional<Protocol::VoxelPosition> post;
    for (std::uint32_t tick = static_cast<std::uint32_t>(arc.flightTicks.FloorWhole()) / 2; tick < 200 && !post.has_value(); ++tick)
    {
      const Simulation::FixedPosition from = Simulation::ShotPositionAt(arc, tick);
      const Simulation::FixedPosition to = Simulation::ShotPositionAt(arc, tick + 1);
      const Protocol::VoxelPosition first = Simulation::VoxelHolding(from);
      const Protocol::VoxelPosition last = Simulation::VoxelHolding(to);
      static_cast<void>(Simulation::TraverseVoxels(from, to,
                                                   [&](Protocol::VoxelPosition _voxel)
                                                   {
                                                     const bool otherColumn =
                                                       (_voxel.xVoxels != first.xVoxels || _voxel.zVoxels != first.zVoxels) &&
                                                       (_voxel.xVoxels != last.xVoxels || _voxel.zVoxels != last.zVoxels);
                                                     if (otherColumn && _voxel.yVoxels > FlatWorld::GROUND_VOXELS)
                                                     {
                                                       post = _voxel;
                                                       return true;
                                                     }
                                                     return false;
                                                   }));
    }
    Assert::IsTrue(post.has_value());
    const Protocol::VoxelPosition top = post.value_or(Protocol::VoxelPosition{});

    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, engineCell);
    Fill(world, At(top.xVoxels, FlatWorld::GROUND_VOXELS, top.zVoxels), top, MASONRY);
    world.Submit(PLAYER, Fire(engine, target));
    const Simulation::ShotHit hit = RunToHit(world).value_or(Simulation::ShotHit{});
    Assert::AreEqual(top.xVoxels, hit.voxel.xVoxels);
    Assert::AreEqual(top.zVoxels, hit.voxel.zVoxels);
  }

  TEST_METHOD(RefusesOutOfRangeAndWhileReloading)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, At(5, FlatWorld::GROUND_VOXELS, 5));
    world.Submit(PLAYER, Fire(engine, At(16, TOP, 5))); // 11 m
    world.Submit(PLAYER, Fire(engine, At(56, TOP, 5))); // 51 m
    world.Submit(PLAYER + 1, Fire(engine, At(30, TOP, 5)));
    world.Tick();
    Assert::IsTrue(world.Shots().empty() && world.LastTickHits().empty());
    Assert::AreEqual(std::uint32_t{0}, world.SiegeEngines().at(engine).reloadTicks);

    world.Submit(PLAYER, Fire(engine, At(55, TOP, 5))); // 50 m
    world.Tick();
    Assert::AreEqual(std::size_t{1}, world.Shots().size());
    world.Submit(PLAYER, Fire(engine, At(17, TOP, 5))); // 12 m, but reloading
    world.Tick();
    Assert::IsTrue(world.Shots().size() <= 1);
    Assert::AreEqual(400, BakedServerData().Tuning().shotReloadTicks);
    while (world.SiegeEngines().at(engine).reloadTicks > 0)
    {
      world.Tick();
    }
    Assert::IsTrue(world.Shots().empty());
    world.Submit(PLAYER, Fire(engine, At(17, TOP, 5)));
    world.Tick();
    Assert::AreEqual(std::size_t{1}, world.Shots().size());
  }

  TEST_METHOD(GivesTheSameHitsForTheSameScript)
  {
    std::vector<Simulation::ScriptedCommand> script;
    for (std::uint32_t round = 0; round < 4; ++round)
    {
      const auto at = static_cast<std::int32_t>(20 + 9 * round);
      script.push_back(
        {.tick = 1 + 401 * round, .player = PLAYER, .command = Protocol::FireEngine{.engine = 1, .targetVoxels = At(at, TOP, 50 - at)}});
    }
    std::array<std::vector<Protocol::VoxelPosition>, 2> hits;
    for (std::vector<Protocol::VoxelPosition>& found : hits)
    {
      Simulation::World world(FlatWorld::Desc());
      static_cast<void>(world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, At(4, FlatWorld::GROUND_VOXELS, 4)));
      for (std::uint32_t tick = 0; tick < 1700; ++tick)
      {
        Simulation::RunScript(world, script, 1);
        for (const Simulation::ShotHit& hit : world.LastTickHits())
        {
          found.push_back(hit.voxel);
        }
      }
    }
    Assert::AreEqual(std::size_t{4}, hits[0].size());
    Assert::IsTrue(hits[0] == hits[1]);
  }

  TEST_METHOD(ContinuesAShotInFlightAfterASave)
  {
    Simulation::World straight(FlatWorld::Desc());
    const Protocol::EntityId engine =
      straight.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, At(5, FlatWorld::GROUND_VOXELS, 20));
    straight.Submit(PLAYER, Fire(engine, At(48, TOP, 40)));
    for (std::uint32_t tick = 0; tick < 12; ++tick)
    {
      straight.Tick();
    }
    Assert::AreEqual(std::size_t{1}, straight.Shots().size());
    Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(straight), FlatWorld::Desc());
    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(loaded));
    const std::optional<Simulation::ShotHit> first = RunToHit(straight);
    const std::optional<Simulation::ShotHit> second = RunToHit(loaded);
    Assert::IsTrue(first.has_value() && second.has_value());
    Assert::IsTrue(first.value_or(Simulation::ShotHit{}).voxel == second.value_or(Simulation::ShotHit{}).voxel);
    Assert::AreEqual(straight.TickCount(), loaded.TickCount());
    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(loaded));
  }
};

} // namespace SimulationTests
