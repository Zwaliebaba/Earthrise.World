#include "pch.h"

#include "FlatWorld.h"
#include "Formation.h"
#include "Scenario.h"
#include "ServerDataFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The trebuchet against units and engines, its stone, deploying and packing (M4-C5; ADR-023, decisions 7 and 10;
// MvpPlan.md, M4, inputs 5, 8 and 17): a stone deals 40 crush to every unit within a cell of the voxel it strikes and to
// an engine whose footprint holds its column; a shot costs 1.5 m3 of its player's stone; a trebuchet deploys and packs in
// 20 s each, fires only deployed, and moves only packed, as machines.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::uint8_t EAST = 2; // Simulation::FACINGS' +x

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

void Order(Simulation::World& _world, std::uint8_t _player, const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  _world.Submit(_player, writer.Bytes());
}

bool Refused(const Simulation::World& _world, std::uint8_t _player, Protocol::Refusal _reason)
{
  return std::ranges::any_of(_world.State(_player).refusals,
                             [_reason](const Protocol::RefusalView& _view) { return _view.reason == _reason; });
}

/// Fires _engine at _target and lets the stone land, returning the voxel it struck.
std::optional<Protocol::VoxelPosition> FireAndLand(Simulation::World& _world, Protocol::EntityId _engine, Protocol::VoxelPosition _target)
{
  Order(_world, ATTACKER, Protocol::FireEngine{.engine = _engine, .targetVoxels = _target});
  std::optional<Protocol::VoxelPosition> struck;
  for (std::uint32_t tick = 0; tick < 200 && !struck.has_value(); ++tick)
  {
    _world.Tick();
    if (!_world.LastTickHits().empty())
    {
      struck = _world.LastTickHits().front().voxel;
    }
  }
  return struck;
}

} // namespace

TEST_CLASS(EngineTests)
{
public:
  TEST_METHOD(AShotCrushesTheFiguresWithinACellOfItsVoxelAndNoOthers)
  {
    // Six spearmen in two ranks facing east, the rear rank at x 36; the stone lands just west of them, so the rear rank
    // is within a cell of the voxel it strikes and the front rank, two cells on, is not.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32));
    const std::vector<Protocol::VoxelPosition> cells = Simulation::FormationSlots(At(37, GROUND, 32), EAST, 6, 2);
    const Protocol::EntityId squad = world.AddSquad(DEFENDER, Simulation::UnitKind::Spearman, cells);
    const std::vector<Protocol::EntityId> units = world.Squads().at(squad).units;
    const std::optional<Protocol::VoxelPosition> struck = FireAndLand(world, engine, At(35, GROUND - 1, 32));
    Assert::IsTrue(struck.has_value());
    const Protocol::VoxelPosition voxel = struck.value_or(At(0, 0, 0));
    const std::int32_t whole = world.Data().Stats(Simulation::UnitKind::Spearman).hitPoints;
    std::size_t hurt = 0;
    for (const Protocol::EntityId id : units)
    {
      const Simulation::Unit& unit = world.Units().at(id);
      const bool within = std::abs(unit.cellVoxels.xVoxels - voxel.xVoxels) <= 1 && std::abs(unit.cellVoxels.zVoxels - voxel.zVoxels) <= 1;
      Assert::AreEqual(within ? whole - 40 : whole, unit.hitPoints);
      hurt += within ? 1 : 0;
    }
    Assert::AreEqual(std::size_t{3}, hurt);
  }

  TEST_METHOD(FourShotsKillATrebuchet)
  {
    // 150 hit points and no armor against crush: 40 a shot, so the fourth kills it, and the engine goes with its body.
    // It stands on bedrock, which takes no crater, so each stone strikes the same voxel in its footprint.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, At(30, GROUND - 1, 26), At(42, GROUND - 1, 38), BEDROCK);
    world.Tick();
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32));
    const Protocol::EntityId theirs =
      world.AddSiegeEngine(DEFENDER, Simulation::EngineKind::Trebuchet, At(35, GROUND, 31), 0, Simulation::EngineState::Packed);
    const Protocol::EntityId body = world.Units().rbegin()->first;
    for (std::int32_t shot = 1; shot <= 4; ++shot)
    {
      Assert::IsTrue(world.SiegeEngines().contains(theirs));
      Assert::IsTrue(FireAndLand(world, engine, At(36, GROUND - 1, 32)).has_value());
      if (shot < 4)
      {
        Assert::AreEqual(150 - (40 * shot), world.Units().at(body).hitPoints);
        for (std::uint32_t tick = 0; tick < 400 && world.SiegeEngines().at(engine).reloadTicks > 0; ++tick)
        {
          world.Tick();
        }
      }
    }
    Assert::IsFalse(world.SiegeEngines().contains(theirs));
    Assert::IsFalse(world.Units().contains(body));
  }

  TEST_METHOD(AShotCostsItsStoneAndThe81stOf120CubicMetersIsRefused)
  {
    // 120 m3 at 1.5 m3 a shot is 80 shots. The reload is cut to a tick so that the 81st comes quickly.
    const Simulation::ServerData quick = BakedServerDataWith("shotReloadTicks", 1);
    Simulation::World world(FlatWorld::Desc(2, 2, &quick));
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32));
    std::set<Protocol::EntityId> fired;
    bool refused = false;
    for (std::uint32_t tick = 0; tick < 400 && !refused; ++tick)
    {
      const auto aim = static_cast<std::int32_t>(fired.size());
      Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(30 + (aim % 10), GROUND - 1, 20 + (aim / 10))});
      world.Tick();
      for (const auto& [id, shot] : world.Shots())
      {
        fired.insert(id);
      }
      if (fired.size() == 1)
      {
        Assert::AreEqual(12000 - 150, world.Players().at(ATTACKER).stoneHundredthsCubicMeters);
      }
      refused = Refused(world, ATTACKER, Protocol::Refusal::NotEnoughStone);
    }
    Assert::IsTrue(refused);
    Assert::AreEqual(std::size_t{80}, fired.size());
    Assert::AreEqual(0, world.Players().at(ATTACKER).stoneHundredthsCubicMeters);
  }

  TEST_METHOD(APackedTrebuchetCannotFire)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32), 0, Simulation::EngineState::Packed);
    Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(35, GROUND - 1, 32)});
    world.Tick();
    Assert::IsTrue(Refused(world, ATTACKER, Protocol::Refusal::NotDeployed));
    Assert::IsTrue(world.Shots().empty());
  }

  TEST_METHOD(DeploysInItsTimeAndFiresTheTickAfter)
  {
    // Told to deploy, it is Deploying for trebuchetDeployTicks, 400, refusing to fire; the tick after, it fires.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32), 0, Simulation::EngineState::Packed);
    const auto deployTicks = static_cast<std::uint32_t>(world.Data().Tuning().trebuchetDeployTicks);
    Assert::AreEqual(400U, deployTicks);
    Order(world, ATTACKER, Protocol::DeployEngine{.engine = engine, .quarterTurns = 1});
    const std::uint32_t deployed = world.TickCount();
    world.Tick();
    Assert::IsTrue(world.SiegeEngines().at(engine).state == Simulation::EngineState::Deploying);
    Assert::AreEqual(1U, world.SiegeEngines().at(engine).quarterTurns);
    std::optional<std::uint32_t> firedTick;
    for (std::uint32_t tick = 0; tick < 2 * deployTicks && !firedTick.has_value(); ++tick)
    {
      const std::uint32_t now = world.TickCount();
      Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(35, GROUND - 1, 32)});
      world.Tick();
      if (!world.Shots().empty())
      {
        firedTick = now;
      }
      else
      {
        Assert::IsTrue(Refused(world, ATTACKER, Protocol::Refusal::NotDeployed));
      }
    }
    Assert::IsTrue(firedTick.has_value());
    Assert::AreEqual(deployed + deployTicks, firedTick.value_or(0));
  }

  TEST_METHOD(PacksInItsTimeAndThenMovesAgain)
  {
    // A deployed trebuchet's body is refused a move; packed, 400 ticks after it is told to pack, it moves.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32));
    const Protocol::EntityId body = world.SiegeEngines().at(engine).squad;
    Order(world, ATTACKER, Protocol::MoveSquad{.squad = body, .goalVoxels = At(20, GROUND, 32)});
    world.Tick();
    Assert::IsTrue(Refused(world, ATTACKER, Protocol::Refusal::NotPacked));
    Order(world, ATTACKER, Protocol::PackEngine{.engine = engine});
    for (std::uint32_t tick = 0; tick < 400; ++tick)
    {
      world.Tick();
      Assert::IsTrue(world.SiegeEngines().at(engine).state ==
                     (tick + 1 < 400 ? Simulation::EngineState::Packing : Simulation::EngineState::Packed));
    }
    Order(world, ATTACKER, Protocol::MoveSquad{.squad = body, .goalVoxels = At(20, GROUND, 32)});
    world.Tick();
    Assert::IsTrue(world.State(ATTACKER).refusals.empty());
  }

  TEST_METHOD(APackedTrebuchetPathsAsMachinesFromTheCampToAStand)
  {
    // The Siege's trebuchet, packed in the camp on its castle side, walks as machines, 3 m wide, to a stand 32 m north of
    // it towards the castle, with none of the camp's squads in its way (GameDesign.md, M4's phases B and C, item 6).
    Simulation::World world(Simulation::ScenarioDesc(BakedServerData(), "Siege", FlatWorld::BUILD_ID));
    Simulation::LoadScenario(world, "Siege");
    const auto trebuchet = std::ranges::find_if(world.SiegeEngines(), [](const auto& _engine)
                                                { return _engine.second.kind == Simulation::EngineKind::Trebuchet; });
    Assert::IsTrue(trebuchet != world.SiegeEngines().end());
    const Protocol::EntityId engine = trebuchet->first;
    const Simulation::SiegeEngine& at = trebuchet->second;
    Assert::IsTrue(at.state == Simulation::EngineState::Packed);
    const Simulation::Squad& body = world.Squads().at(at.squad);
    Assert::IsTrue(body.navigationClass == Simulation::NavigationClass::Machines);
    const Protocol::VoxelPosition stand = At(at.cellVoxels.xVoxels, at.cellVoxels.yVoxels, at.cellVoxels.zVoxels + 32);
    Order(world, ATTACKER, Protocol::MoveSquad{.squad = at.squad, .goalVoxels = stand});
    for (std::uint32_t tick = 0; tick < 3000 && world.SiegeEngines().at(engine).cellVoxels != stand; ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(world.SiegeEngines().at(engine).cellVoxels == stand);
  }
};

} // namespace SimulationTests
