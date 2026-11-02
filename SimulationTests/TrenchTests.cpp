#include "pch.h"

#include "FlatWorld.h"
#include "Scenario.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Sappers' trenches (M4-D6; ADR-029; MvpPlan.md, M4, input 12): a squad of sappers digs a box from the top, 0.2 m^3 a
// second each for those within 2 m of it, every layer leaving a ramp a voxel a cell at the end of its long axis nearest
// them, the spoil leaving play. Earth under a structure slumps only towards what a trench opened, and so does earth beside
// a gap the trench opened under one. The flat world's trenches are dug with slump off, so that the box is all they dig;
// the castle's with it on, as the trial plays.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 64;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

void Order(Simulation::World& _world, std::uint8_t _player, const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  _world.Submit(_player, writer.Bytes());
}

/// A 2 x 8 x 4 m box in flat earth, x 20 to 21 and z 20 to 27, and six sappers in a row south of it, x 18 to 23 at z 19,
/// set digging it; its ramp is at its south end, where they stand.
constexpr Protocol::VoxelPosition BOX_MIN{.xVoxels = 20, .yVoxels = GROUND - 4, .zVoxels = 20};
constexpr Protocol::VoxelPosition BOX_MAX{.xVoxels = 21, .yVoxels = GROUND - 1, .zVoxels = 27};

Protocol::EntityId Sappers(Simulation::World& _world)
{
  std::vector<Protocol::VoxelPosition> cells;
  for (std::int32_t x = 20; x >= 18; --x)
  {
    cells.push_back(At(x, GROUND, 19));
  }
  for (std::int32_t x = 21; x <= 23; ++x)
  {
    cells.push_back(At(x, GROUND, 19));
  }
  const Protocol::EntityId squad = _world.AddSquad(ATTACKER, Simulation::UnitKind::Sapper, cells);
  Order(_world, ATTACKER, Protocol::DigTrench{.squad = squad, .minVoxels = BOX_MIN, .maxVoxels = BOX_MAX});
  return squad;
}

std::size_t Dug(const Simulation::World& _world, Protocol::VoxelPosition _min, Protocol::VoxelPosition _max)
{
  std::size_t dug = 0;
  for (std::int32_t y = _min.yVoxels; y <= _max.yVoxels; ++y)
  {
    for (std::int32_t z = _min.zVoxels; z <= _max.zVoxels; ++z)
    {
      for (std::int32_t x = _min.xVoxels; x <= _max.xVoxels; ++x)
      {
        dug += _world.Voxels().At(At(x, y, z)) == Protocol::Chunk::EMPTY ? 1 : 0;
      }
    }
  }
  return dug;
}

std::int64_t RatePerSapper(const Simulation::World& _world)
{
  return _world.Data().Stats(Simulation::UnitKind::Sapper).digCubicMetersPerTick.Raw();
}

/// Whether foot can walk from every cell of the box they can stand in to (20, 10), south of it.
bool EveryCellClimbsOut(const Simulation::World& _world)
{
  const std::int64_t goal = (std::int64_t{10} * WIDTH) + 20;
  for (std::int32_t z = BOX_MIN.zVoxels; z <= BOX_MAX.zVoxels; ++z)
  {
    for (std::int32_t x = BOX_MIN.xVoxels; x <= BOX_MAX.xVoxels; ++x)
    {
      const std::int64_t cell = (std::int64_t{z} * WIDTH) + x;
      if (_world.Navigation().Usable(x, z, Simulation::NavigationClass::Foot) &&
          _world.Navigation().NearestReachable(cell, goal, Simulation::NavigationClass::Foot) != goal)
      {
        return false;
      }
    }
  }
  return true;
}

/// The Siege castle, slump on, with six of the defender's sappers in a row from _first northwards, set digging _min to
/// _max; the defender's, so that its own garrisons leave them be.
struct Castle
{
  Simulation::World world{Simulation::ScenarioDesc(BakedServerData(), "Siege", FlatWorld::BUILD_ID)};
  Protocol::EntityId squad = Protocol::NO_ENTITY;

  Castle(Protocol::VoxelPosition _first, Protocol::VoxelPosition _min, Protocol::VoxelPosition _max)
  {
    Simulation::LoadScenario(world, "Siege");
    world.Tick();
    std::vector<Protocol::VoxelPosition> cells;
    cells.reserve(6);
    for (std::int32_t figure = 0; figure < 6; ++figure)
    {
      cells.push_back(At(_first.xVoxels, _first.yVoxels, _first.zVoxels + figure));
    }
    squad = world.AddSquad(DEFENDER, Simulation::UnitKind::Sapper, cells);
    Order(world, DEFENDER, Protocol::DigTrench{.squad = squad, .minVoxels = _min, .maxVoxels = _max});
  }

  [[nodiscard]] Protocol::EntityId Named(std::string_view _name) const
  {
    for (const auto& [id, structure] : world.Structures())
    {
      if (structure.name == _name)
      {
        return id;
      }
    }
    return Protocol::NO_ENTITY;
  }
};

} // namespace

TEST_CLASS(TrenchTests)
{
public:
  TEST_METHOD(SixSappersDigATwoByEightByFourTrenchInTheTimeTheRateGives)
  {
    // Its ramp leaves 1, 2 and 3 m of depth in the three cells at its south end and 4 m in the five beyond: 2 x 26 =
    // 52 m^3, at six times 0.01 m^3 a tick, as Q16.16 floored.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    world.Tick();
    Sappers(world);
    const auto ticks =
      static_cast<std::uint32_t>(((std::int64_t{52} * Core::Fixed::ONE_RAW) + (6 * RatePerSapper(world)) - 1) / (6 * RatePerSapper(world)));
    for (std::uint32_t tick = 0; tick + 1 < ticks; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(std::size_t{51}, Dug(world, BOX_MIN, BOX_MAX));
    world.Tick();
    Assert::AreEqual(std::size_t{52}, Dug(world, BOX_MIN, BOX_MAX));
    for (std::uint32_t tick = 0; tick < 200; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(std::size_t{52}, Dug(world, BOX_MIN, BOX_MAX));
    Logger::WriteMessage(("M4-D6: 52 m^3 in " + std::to_string(ticks) + " ticks").c_str());
  }

  TEST_METHOD(TheTrenchAlwaysHasARampAFootSoldierCanClimbOutBy)
  {
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    world.Tick();
    Sappers(world);
    for (std::uint32_t tick = 0; tick < 1000; ++tick)
    {
      world.Tick();
      if (tick % 10 == 0)
      {
        Assert::IsTrue(EveryCellClimbsOut(world));
      }
    }
    Assert::AreEqual(std::size_t{52}, Dug(world, BOX_MIN, BOX_MAX));
    Assert::IsTrue(EveryCellClimbsOut(world));
  }

  TEST_METHOD(ASapperKilledSlowsTheDig)
  {
    // A stone from a trebuchet north of the trench lands beside the row's west end and kills one sapper of six; the dig
    // then gains five sappers' rate a tick in place of six.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const Protocol::EntityId engine = world.AddSiegeEngine(DEFENDER, Simulation::EngineKind::Trebuchet, At(16, GROUND, 52));
    world.Tick();
    const Protocol::EntityId squad = Sappers(world);
    const auto gained = [&world, squad]
    {
      const std::int64_t before =
        world.Squads().at(squad).digProgress.Raw() + (static_cast<std::int64_t>(Dug(world, BOX_MIN, BOX_MAX)) * Core::Fixed::ONE_RAW);
      world.Tick();
      return world.Squads().at(squad).digProgress.Raw() + (static_cast<std::int64_t>(Dug(world, BOX_MIN, BOX_MAX)) * Core::Fixed::ONE_RAW) -
             before;
    };
    world.Tick();
    Assert::AreEqual(6 * RatePerSapper(world), gained());
    Order(world, DEFENDER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(17, GROUND - 1, 19)});
    for (std::uint32_t tick = 0; tick < 200 && world.Squads().at(squad).units.size() == 6; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(std::size_t{5}, world.Squads().at(squad).units.size());
    Assert::AreEqual(5 * RatePerSapper(world), gained());
  }

  TEST_METHOD(UnderminedE3FallsAndTheVolumeIsReported)
  {
    // E3 stands on deep earth, x 366 to 367 and z 325 to 332, its foundation 4 m down to y 5, earth from y 1 to y 8 beneath
    // and beside it, x 365 to 400. A box against its outer face, 8 m out from it and as long as it, x 368 to 375, its ramp
    // at its east end where the sappers stand: the trench is full depth along the wall, the earth under the foundation
    // slumps into it, and the courtyard's earth into the gap under the foundation, until the section falls. The volume dug
    // to the fall is logged against the route table's 224 m^3 (Review/PaperTests/SiegeRouteTable.md, E3 by sappers).
    Castle castle(At(377, 9, 326), At(368, 1, 325), At(375, 8, 332));
    Simulation::World& world = castle.world;
    const Protocol::EntityId e3 = castle.Named("E3");
    // The six stand still and all reach the box, so each tick gains six rates while there is earth to dig, less a cubic
    // metre for each voxel dug, of which a tick digs one at most.
    const std::int64_t gain = 6 * RatePerSapper(world);
    std::uint32_t fell = 0;
    std::uint32_t dug = 0;
    bool locked = false;
    for (std::uint32_t tick = 0; tick < 20 * 60 * 20 && fell == 0; ++tick)
    {
      const std::int64_t before = world.Squads().at(castle.squad).digProgress.Raw();
      world.Tick();
      const std::int64_t after = world.Squads().at(castle.squad).digProgress.Raw();
      dug += after != before && after < before + gain ? 1U : 0U;
      locked = locked || world.Structures().at(e3).repairLockTicks > 0;
      if (std::ranges::any_of(world.Collapses(), [](const auto& _collapse) { return _collapse.second.ticksElapsed == 1; }))
      {
        fell = world.TickCount();
      }
    }
    Assert::IsTrue(fell > 0);
    Assert::IsTrue(locked);
    Logger::WriteMessage(("M4-D6: E3 falls at tick " + std::to_string(fell) + " (" + std::to_string(fell / 20) + " s), " +
                          std::to_string(dug) + " m^3 dug, against the route table's 224 m^3 and 198 s")
                           .c_str());
  }

  TEST_METHOD(OnRockNothingFalls)
  {
    // E2, beside E3 to the south, stands on 1 m of soil over common rock: the same box against it takes the soil and
    // stops at the rock, which neither slumps nor is dug, and nothing falls.
    Castle castle(At(377, 9, 318), At(368, 1, 317), At(375, 8, 324));
    Simulation::World& world = castle.world;
    for (std::uint32_t tick = 0; tick < 4 * 60 * 20; ++tick)
    {
      world.Tick();
      Assert::IsTrue(std::ranges::none_of(world.Collapses(), [](const auto& _collapse) { return _collapse.second.ticksElapsed == 1; }));
    }
    Assert::AreEqual(std::size_t{64}, Dug(world, At(368, 8, 317), At(375, 8, 324)));
    Assert::AreEqual(std::size_t{0}, Dug(world, At(368, 1, 317), At(375, 7, 324)));
  }
};

} // namespace SimulationTests
