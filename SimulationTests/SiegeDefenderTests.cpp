#include "pch.h"

#include "FlatWorld.h"
#include "Replay.h"
#include "Save.h"
#include "Scenario.h"
#include "SiegeDefender.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The Siege defender's first half (M4-F2; ADR-027; MvpPlan.md, M4, input 18), a commander inside the simulation that
// reads only its player's update: it aims its garrisons at sappers first, then rams, then the trebuchet; sends a laborer
// to each of the most damaged structures not locked; and sends archers who are out to the nearest working building with
// room. The castle's own tests load Siege.json, whose defender is a SiegeDefender.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t SITE = 9; // the castle's ground

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

Simulation::World::Desc CastleDesc()
{
  return Simulation::ScenarioDesc(BakedServerData(), "Siege", FlatWorld::BUILD_ID);
}

Simulation::World Castle()
{
  Simulation::World world(CastleDesc());
  Simulation::LoadScenario(world, "Siege");
  return world;
}

Protocol::EntityId Named(const Simulation::World& _world, std::string_view _name)
{
  for (const auto& [id, structure] : _world.Structures())
  {
    if (structure.name == _name)
    {
      return id;
    }
  }
  return Protocol::NO_ENTITY;
}

Protocol::EntityId GarrisonOf(const Simulation::World& _world, Protocol::EntityId _building)
{
  for (const auto& [id, squad] : _world.Squads())
  {
    if (squad.garrisonedIn == _building)
    {
      return id;
    }
  }
  return Protocol::NO_ENTITY;
}

/// A squad of _owner's _kind holding in a column of cells at x _x from z _z northwards.
Protocol::EntityId Column(Simulation::World& _world, std::uint8_t _owner, Simulation::UnitKind _kind, std::int32_t _x, std::int32_t _z,
                          std::int32_t _figures)
{
  std::vector<Protocol::VoxelPosition> cells;
  cells.reserve(static_cast<std::size_t>(_figures));
  for (std::int32_t figure = 0; figure < _figures; ++figure)
  {
    cells.push_back(At(_x, SITE, _z + figure));
  }
  const Protocol::EntityId squad = _world.AddSquad(_owner, _kind, cells);
  Order(_world, _owner, Protocol::HoldSquad{.squad = squad});
  return squad;
}

/// The defender's AttackTarget and RepairStructure commands the last tick applied.
std::vector<Protocol::Command> DefendersCommands(const Simulation::World& _world)
{
  std::vector<Protocol::Command> commands;
  for (const Simulation::SubmittedCommand& submitted : _world.LastTickCommanderCommands())
  {
    if (submitted.player == DEFENDER)
    {
      Core::ByteReader reader(submitted.bytes);
      commands.push_back(Protocol::DecodeCommand(reader));
    }
  }
  return commands;
}

} // namespace

TEST_CLASS(SiegeDefenderTests)
{
public:
  TEST_METHOD(SappersAtE3sFootDrawTheSEAndNEGarrisonsArrowsBeforeSpearmenBesideThem)
  {
    // Six sappers at E3's outer foot, x 369 and z 326 to 331, and six spearmen just south of them, nearer SE: once the
    // defender has aimed them, every arrow SE's and NE's garrisons loose while a sapper lives is at a sapper.
    Simulation::World world = Castle();
    const Protocol::EntityId sappers = Column(world, ATTACKER, Simulation::UnitKind::Sapper, 369, 326, 6);
    Column(world, ATTACKER, Simulation::UnitKind::Spearman, 369, 318, 6);
    const Protocol::EntityId se = GarrisonOf(world, Named(world, "SE"));
    const Protocol::EntityId ne = GarrisonOf(world, Named(world, "NE"));
    std::set<Protocol::EntityId> seen;
    std::size_t atSappers = 0;
    bool aimed = false;
    for (std::uint32_t tick = 0; tick < 600 && world.Squads().contains(sappers); ++tick)
    {
      world.Tick();
      aimed = aimed || (world.Squads().at(se).target == sappers && world.Squads().at(ne).target == sappers);
      for (const auto& [id, shot] : world.Shots())
      {
        if (shot.kind != Protocol::ShotKind::Arrow || !seen.insert(id).second || !world.Units().contains(shot.shooter))
        {
          continue;
        }
        const Protocol::EntityId from = world.Units().at(shot.shooter).squad;
        if (aimed && (from == se || from == ne) && world.Squads().contains(sappers))
        {
          Assert::IsTrue(world.Units().contains(shot.target) && world.Units().at(shot.target).squad == sappers);
          ++atSappers;
        }
      }
    }
    Assert::IsTrue(aimed);
    Assert::IsTrue(atSappers > 0);
  }

  TEST_METHOD(ACrateredTowerIsRepairedOnceItsLockEnds)
  {
    // A trebuchet of the attacker's 47 m south of SE, beyond every garrison's reach, craters its south face once: no
    // laborer is sent while the lock lasts, then the nearest is, and the tower stands whole again.
    Simulation::World world = Castle();
    const Protocol::EntityId tower = Named(world, "SE");
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(366, SITE, 255));
    world.Tick();
    Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(367, SITE + 6, 302)});
    for (std::uint32_t tick = 0; tick < 400 && world.Structures().at(tower).repairLockTicks == 0; ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(world.Structures().at(tower).repairLockTicks > 0);
    Assert::IsTrue(world.Structures().at(tower).standingPercent < 100);
    bool sent = false;
    for (std::uint32_t tick = 0; tick < 4000 && world.Structures().at(tower).standingPercent < 100; ++tick)
    {
      const bool locked = world.Structures().at(tower).repairLockTicks > 0;
      world.Tick();
      for (const Protocol::Command& command : DefendersCommands(world))
      {
        if (const auto* repair = std::get_if<Protocol::RepairStructure>(&command); repair != nullptr && repair->structure == tower)
        {
          Assert::IsFalse(locked);
          sent = true;
        }
      }
    }
    Assert::IsTrue(sent);
    Assert::AreEqual(std::uint8_t{100}, world.Structures().at(tower).standingPercent);
  }

  TEST_METHOD(AStoppedTowersArchersGarrisonTheKeep)
  {
    // On flat ground, a defender's T4 with six archers in it and a T2 with room: the tower's platform cut away, it stops
    // and puts them out, and the defender sends them into the keep.
    Simulation::World world(FlatWorld::Desc(4, 4));
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
    world.SetCommander(DEFENDER, Simulation::MakeCommander(Simulation::SiegeDefender::KIND, nullptr));
    const std::int32_t ground = FlatWorld::GROUND_VOXELS;
    const Protocol::EntityId tower = world.AddStructure(DEFENDER, "T4", At(20, ground, 60), 0, "NE");
    const Protocol::EntityId keep = world.AddStructure(DEFENDER, "T2", At(60, ground, 60), 0, "KEEP");
    std::vector<Protocol::VoxelPosition> cells;
    cells.reserve(6);
    for (std::int32_t figure = 0; figure < 6; ++figure)
    {
      cells.push_back(At(30 + figure, ground, 50));
    }
    const Protocol::EntityId archers = world.AddSquad(DEFENDER, Simulation::UnitKind::Archer, cells);
    world.PlaceInGarrison(archers, tower);
    world.Tick();
    for (std::int32_t z = 0; z < 7; ++z)
    {
      for (std::int32_t x = 0; x < 7; ++x)
      {
        world.Voxels().Edit(At(20 + x, ground + 8, 60 + z), Protocol::Chunk::EMPTY);
      }
    }
    for (std::uint32_t tick = 0; tick < 2000 && world.Squads().at(archers).garrisonedIn != keep; ++tick)
    {
      world.Tick();
    }
    Assert::IsFalse(world.Structures().at(tower).working);
    Assert::AreEqual(keep, world.Squads().at(archers).garrisonedIn);
  }

  TEST_METHOD(ARunOf2000TicksReplaysEqualAndContinuesEqualFromASaveAt1000)
  {
    // The castle with attackers at E3's foot for the defender to aim at, recorded from its first tick.
    Simulation::World world = Castle();
    Column(world, ATTACKER, Simulation::UnitKind::Sapper, 369, 326, 6);
    Column(world, ATTACKER, Simulation::UnitKind::Spearman, 369, 318, 6);
    Simulation::ReplayRecorder recorder(world);
    std::vector<std::byte> save;
    for (std::uint32_t tick = 0; tick < 2000; ++tick)
    {
      if (tick == 1000)
      {
        save = Simulation::WriteSave(world);
      }
      world.Tick();
      recorder.Record(world);
    }
    const Simulation::ReplayResult replayed = Simulation::Replay(recorder.Bytes(), CastleDesc());
    Assert::IsTrue(replayed.matched);
    Assert::AreEqual(std::uint32_t{2000}, replayed.ticksReplayed);
    Simulation::World loaded = Simulation::LoadSave(save, CastleDesc());
    for (std::uint32_t tick = 0; tick < 1000; ++tick)
    {
      loaded.Tick();
    }
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(loaded));
  }

  TEST_METHOD(TheDefenderNeverAimsAtAnEnemyItCannotSee)
  {
    // Every target the defender names is a squad its last update showed it; the attackers at E3 are seen, and those
    // standing in the camp, 100 m south, are not.
    Simulation::World world = Castle();
    Column(world, ATTACKER, Simulation::UnitKind::Sapper, 369, 326, 6);
    std::set<Protocol::EntityId> visible;
    std::size_t aims = 0;
    for (std::uint32_t tick = 0; tick < 600; ++tick)
    {
      world.Tick();
      for (const Protocol::Command& command : DefendersCommands(world))
      {
        if (const auto* attack = std::get_if<Protocol::AttackTarget>(&command); attack != nullptr)
        {
          Assert::IsTrue(visible.contains(attack->target));
          ++aims;
        }
      }
      visible.clear();
      for (const Protocol::SquadView& squad : world.State(DEFENDER).squads)
      {
        if (squad.owner != DEFENDER)
        {
          visible.insert(squad.id);
        }
      }
    }
    Assert::IsTrue(aims > 0);
  }
};

} // namespace SimulationTests
