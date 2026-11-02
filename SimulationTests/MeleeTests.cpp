#include "pch.h"

#include "FlatWorld.h"
#include "Formation.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Melee and the squads' orders (M4-C3; ADR-023, decisions 3 and 4): each figure takes the nearest enemy within its
// vision and closes on it; a strike lands on a neighbouring cell, worked out from the state before the tick's strikes,
// all applied together. The counters are input 6's arithmetic: six spearmen kill three outriders in about 8 s and lose
// one or two, three outriders kill six archers in about 12 s, and men-at-arms beat spearmen.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::uint8_t EAST = 2; // Simulation::FACINGS' +x
constexpr std::uint8_t WEST = 6;
constexpr std::uint32_t TICKS_PER_SECOND = 20;

Protocol::VoxelPosition Cell(std::int32_t _x, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = GROUND, .zVoxels = _z};
}

/// A full squad of _kind for _owner in its slots about _lead, facing _facing.
Protocol::EntityId Squad(Simulation::World& _world, std::uint8_t _owner, Simulation::UnitKind _kind, Protocol::VoxelPosition _lead,
                         std::uint8_t _facing)
{
  const Simulation::UnitStats& stats = _world.Data().Stats(_kind);
  const std::uint8_t ranks = stats.navigationClass == Simulation::NavigationClass::Cavalry ? 1 : 2;
  const std::vector<Protocol::VoxelPosition> cells = Simulation::FormationSlots(_lead, _facing, stats.squadFigures, ranks);
  return _world.AddSquad(_owner, _kind, cells);
}

void Order(Simulation::World& _world, std::uint8_t _player, const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  _world.Submit(_player, writer.Bytes());
}

std::size_t Figures(const Simulation::World& _world, Protocol::EntityId _squad)
{
  const auto squad = _world.Squads().find(_squad);
  return squad == _world.Squads().end() ? 0 : squad->second.units.size();
}

std::int64_t HitPoints(const Simulation::World& _world, Protocol::EntityId _squad)
{
  std::int64_t total = 0;
  if (const auto squad = _world.Squads().find(_squad); squad != _world.Squads().end())
  {
    for (const Protocol::EntityId id : squad->second.units)
    {
      total += _world.Units().at(id).hitPoints;
    }
  }
  return total;
}

/// Two squads fought until one is gone: the tick of the first strike, the tick the loser's last figure died, and what
/// each has left.
struct Fight
{
  std::uint32_t firstStrikeTick = 0;
  std::uint32_t endTick = 0;
  std::size_t strongLeft = 0;
  std::size_t weakLeft = 0;
  bool ended = false;
};

Fight RunFight(Simulation::World& _world, Protocol::EntityId _strong, Protocol::EntityId _weak, std::uint32_t _ticks)
{
  Fight fight;
  std::int64_t before = HitPoints(_world, _strong) + HitPoints(_world, _weak);
  bool struck = false;
  for (std::uint32_t tick = 0; tick < _ticks && !fight.ended; ++tick)
  {
    _world.Tick();
    const std::int64_t now = HitPoints(_world, _strong) + HitPoints(_world, _weak);
    if (!struck && now < before)
    {
      struck = true;
      fight.firstStrikeTick = _world.TickCount();
    }
    before = now;
    fight.ended = Figures(_world, _strong) == 0 || Figures(_world, _weak) == 0;
    fight.endTick = _world.TickCount();
  }
  fight.strongLeft = Figures(_world, _strong);
  fight.weakLeft = Figures(_world, _weak);
  return fight;
}

/// The fight's length, from the first strike to the loser's last death, in whole seconds rounded down.
std::uint32_t Seconds(const Fight& _fight)
{
  return (_fight.endTick - _fight.firstStrikeTick) / TICKS_PER_SECOND;
}

} // namespace

TEST_CLASS(MeleeTests)
{
public:
  TEST_METHOD(SixSpearmenKillThreeOutridersLosingNoMoreThanTwo)
  {
    // A spearman strikes an outrider for 4 + 12 = 16, so each of 60 hit points takes four; an outrider strikes a
    // spearman for 6, so each of 45 takes eight. Twelve strikes kill the three: two rounds with all six striking, four
    // with the front rank alone, every 2 s, so the fight lasts 2 to 6 s and some more while they close on each other.
    // Input 6's arithmetic has the spearmen lose one or two; measured, they lose none, since each outrider strikes its
    // own nearest spearman and their twelve strikes, 72 hit points, fall on three of them.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId spearmen = Squad(world, ATTACKER, Simulation::UnitKind::Spearman, Cell(20, 30), EAST);
    const Protocol::EntityId outriders = Squad(world, DEFENDER, Simulation::UnitKind::Outrider, Cell(25, 30), WEST);
    const Fight fight = RunFight(world, spearmen, outriders, 60 * TICKS_PER_SECOND);
    Logger::WriteMessage(("spearmen " + std::to_string(fight.strongLeft) + " left with " + std::to_string(HitPoints(world, spearmen)) +
                          " of 270 hit points after " + std::to_string(Seconds(fight)) + " s")
                           .c_str());
    Assert::IsTrue(fight.ended);
    Assert::AreEqual(std::size_t{0}, fight.weakLeft);
    Assert::IsTrue(fight.strongLeft >= 4);
    Assert::IsTrue(Seconds(fight) >= 2 && Seconds(fight) <= 8);
  }

  TEST_METHOD(ThreeOutridersKillSixArchers)
  {
    // An outrider strikes an archer for 6 + 6 = 12, so each of 30 hit points takes three: eighteen strikes, six rounds
    // of three, at least 10 s from the first to the last. Archers loose arrows, which M4-C4 builds; in melee they do
    // nothing.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId outriders = Squad(world, ATTACKER, Simulation::UnitKind::Outrider, Cell(20, 30), EAST);
    const Protocol::EntityId archers = Squad(world, DEFENDER, Simulation::UnitKind::Archer, Cell(25, 30), WEST);
    const Fight fight = RunFight(world, outriders, archers, 60 * TICKS_PER_SECOND);
    Logger::WriteMessage(("archers dead after " + std::to_string(Seconds(fight)) + " s").c_str());
    Assert::IsTrue(fight.ended);
    Assert::AreEqual(std::size_t{3}, fight.strongLeft);
    Assert::AreEqual(std::size_t{0}, fight.weakLeft);
    Assert::IsTrue(Seconds(fight) >= 10 && Seconds(fight) <= 14);
  }

  TEST_METHOD(MenAtArmsBeatSpearmen)
  {
    // A man-at-arms strikes a spearman for 8 + 4 = 12, four strikes each of 45; a spearman strikes a man-at-arms for
    // 4 - 2 = 2, thirty-five each of 70. The spearmen's every strike, six figures for the fight's few rounds, falls short
    // of two men-at-arms' hit points, so at most one dies.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId menAtArms = Squad(world, ATTACKER, Simulation::UnitKind::ManAtArms, Cell(20, 30), EAST);
    const Protocol::EntityId spearmen = Squad(world, DEFENDER, Simulation::UnitKind::Spearman, Cell(25, 30), WEST);
    const Fight fight = RunFight(world, menAtArms, spearmen, 60 * TICKS_PER_SECOND);
    Logger::WriteMessage(("men-at-arms " + std::to_string(fight.strongLeft) + " left with " + std::to_string(HitPoints(world, menAtArms)) +
                          " of 420 hit points after " + std::to_string(Seconds(fight)) + " s")
                           .c_str());
    Assert::IsTrue(fight.ended);
    Assert::AreEqual(std::size_t{0}, fight.weakLeft);
    Assert::IsTrue(fight.strongLeft >= 5);
  }

  TEST_METHOD(TwoFiguresThatKillEachOtherOnOneTickBothDie)
  {
    // Two spearmen side by side strike each other for 4 on the same ticks: the twelfth strike takes each from 1 to -3,
    // and both die on that tick, since every strike is worked out before any lands.
    Simulation::World world(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 1> west = {Cell(20, 30)};
    const std::array<Protocol::VoxelPosition, 1> east = {Cell(21, 30)};
    const Protocol::EntityId first = world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, west);
    const Protocol::EntityId second = world.AddSquad(DEFENDER, Simulation::UnitKind::Spearman, east);
    std::uint32_t firstGone = 0;
    std::uint32_t secondGone = 0;
    for (std::uint32_t tick = 0; tick < 60 * TICKS_PER_SECOND && (firstGone == 0 || secondGone == 0); ++tick)
    {
      world.Tick();
      firstGone = firstGone == 0 && Figures(world, first) == 0 ? world.TickCount() : firstGone;
      secondGone = secondGone == 0 && Figures(world, second) == 0 ? world.TickCount() : secondGone;
    }
    Assert::AreNotEqual(0U, firstGone);
    Assert::AreEqual(firstGone, secondGone);
  }

  TEST_METHOD(AHoldingSquadStrikesAndDoesNotMove)
  {
    // Spearmen told to hold stand in their cells on every tick while the outriders close on them, and strike those that
    // reach them.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId spearmen = Squad(world, ATTACKER, Simulation::UnitKind::Spearman, Cell(20, 30), EAST);
    const Protocol::EntityId outriders = Squad(world, DEFENDER, Simulation::UnitKind::Outrider, Cell(26, 30), WEST);
    Order(world, ATTACKER, Protocol::HoldSquad{.squad = spearmen});
    world.Tick();
    Assert::IsTrue(world.State(ATTACKER).refusals.empty());
    std::map<Protocol::EntityId, Protocol::VoxelPosition> cells;
    for (const Protocol::EntityId id : world.Squads().at(spearmen).units)
    {
      cells.emplace(id, world.Units().at(id).cellVoxels);
    }
    const std::int64_t before = HitPoints(world, outriders);
    for (std::uint32_t tick = 0; tick < 20 * TICKS_PER_SECOND && Figures(world, spearmen) > 0; ++tick)
    {
      world.Tick();
      for (const Protocol::EntityId id : world.Squads().at(spearmen).units)
      {
        Assert::IsTrue(world.Units().at(id).cellVoxels == cells.at(id));
      }
    }
    Assert::IsTrue(HitPoints(world, outriders) < before);
    Assert::IsTrue(world.Squads().at(spearmen).order == Protocol::SquadOrder::Hold);
  }

  TEST_METHOD(AStormingSquadEngagesWhatItMeetsAndGoesOn)
  {
    // Men-at-arms storm to (52, 30) through spearmen standing at (34, 30): they kill them on the way, then walk on to
    // the goal, and stand idle there.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId menAtArms = Squad(world, ATTACKER, Simulation::UnitKind::ManAtArms, Cell(14, 30), EAST);
    const Protocol::EntityId spearmen = Squad(world, DEFENDER, Simulation::UnitKind::Spearman, Cell(34, 30), WEST);
    Order(world, ATTACKER, Protocol::StormTo{.squad = menAtArms, .goalVoxels = Cell(52, 30)});
    const std::int64_t whole = HitPoints(world, spearmen);
    bool fought = false;
    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && !(Figures(world, spearmen) == 0 && world.Quiet()); ++tick)
    {
      world.Tick();
      fought = fought || HitPoints(world, spearmen) < whole;
    }
    Assert::IsTrue(fought);
    Assert::AreEqual(std::size_t{0}, Figures(world, spearmen));
    const Simulation::Squad& squad = world.Squads().at(menAtArms);
    Assert::IsTrue(world.Units().at(squad.units.front()).cellVoxels == Cell(52, 30));
    Assert::IsTrue(squad.order == Protocol::SquadOrder::Idle);
  }

  TEST_METHOD(AnAttackingSquadFollowsItsTargetPastItsVisionAndKillsIt)
  {
    // Spearmen ordered at a packed trebuchet 24 m off, past their 10 m vision, walk to it and strike its body for 4
    // until its 150 hit points are gone; the engine goes with its body, and the squad turns idle.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId spearmen = Squad(world, ATTACKER, Simulation::UnitKind::Spearman, Cell(14, 30), EAST);
    const Protocol::EntityId engine =
      world.AddSiegeEngine(DEFENDER, Simulation::EngineKind::Trebuchet, Cell(38, 30), 0, Simulation::EngineState::Packed);
    Order(world, ATTACKER, Protocol::AttackTarget{.squad = spearmen, .target = engine});
    world.Tick();
    Assert::IsTrue(world.State(ATTACKER).refusals.empty());
    Assert::IsTrue(world.Squads().at(spearmen).order == Protocol::SquadOrder::Attack);
    for (std::uint32_t tick = 0; tick < 120 * TICKS_PER_SECOND && world.SiegeEngines().contains(engine); ++tick)
    {
      world.Tick();
    }
    Assert::IsFalse(world.SiegeEngines().contains(engine));
    world.Tick();
    Assert::IsTrue(world.Squads().at(spearmen).order == Protocol::SquadOrder::Idle);
    Assert::AreEqual(std::size_t{6}, Figures(world, spearmen));
  }

  TEST_METHOD(ContinuesAFightFromASave)
  {
    // Saved in the middle of the fight, with reloads running, targets taken and the storm under way, the loaded world
    // goes on to the same state as the one that was saved.
    Simulation::World fighting(FlatWorld::Desc());
    const Protocol::EntityId menAtArms = Squad(fighting, ATTACKER, Simulation::UnitKind::ManAtArms, Cell(14, 30), EAST);
    Squad(fighting, DEFENDER, Simulation::UnitKind::Spearman, Cell(24, 30), WEST);
    Order(fighting, ATTACKER, Protocol::StormTo{.squad = menAtArms, .goalVoxels = Cell(40, 30)});
    for (std::uint32_t tick = 0; tick < 8 * TICKS_PER_SECOND; ++tick)
    {
      fighting.Tick();
    }
    Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(fighting), FlatWorld::Desc());
    Assert::AreEqual(Simulation::StateHash(fighting), Simulation::StateHash(loaded));
    for (std::uint32_t tick = 0; tick < 40 * TICKS_PER_SECOND; ++tick)
    {
      fighting.Tick();
      loaded.Tick();
    }
    Assert::AreEqual(Simulation::StateHash(fighting), Simulation::StateHash(loaded));
  }

  TEST_METHOD(SplitGivesASquadOfOneWithTheHighestIdFigure)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId spearmen = Squad(world, ATTACKER, Simulation::UnitKind::Spearman, Cell(20, 30), EAST);
    const std::vector<Protocol::EntityId> units = world.Squads().at(spearmen).units;
    Order(world, ATTACKER, Protocol::SplitSquad{.squad = spearmen});
    world.Tick();
    Assert::AreEqual(std::size_t{5}, Figures(world, spearmen));
    const Simulation::Unit& leaver = world.Units().at(units.back());
    Assert::AreNotEqual(spearmen, leaver.squad);
    const Simulation::Squad& alone = world.Squads().at(leaver.squad);
    Assert::AreEqual(std::size_t{1}, alone.units.size());
    Assert::IsTrue(alone.kind == Simulation::UnitKind::Spearman);
    Assert::AreEqual(ATTACKER, alone.owner);
  }

  TEST_METHOD(MergesAFourAndAThreeIntoASixAndAOne)
  {
    // The first squad takes the second's lowest-id figures up to its kind's six, the second keeps the last.
    Simulation::World world(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 4> four = {Cell(20, 30), Cell(20, 31), Cell(20, 29), Cell(19, 30)};
    const std::array<Protocol::VoxelPosition, 3> three = {Cell(30, 30), Cell(30, 31), Cell(30, 29)};
    const Protocol::EntityId first = world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, four);
    const Protocol::EntityId second = world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, three);
    const std::vector<Protocol::EntityId> given = world.Squads().at(second).units;
    Order(world, ATTACKER, Protocol::MergeSquads{.squads = {first, second}});
    world.Tick();
    Assert::IsTrue(world.State(ATTACKER).refusals.empty());
    Assert::AreEqual(std::size_t{6}, Figures(world, first));
    Assert::AreEqual(std::size_t{1}, Figures(world, second));
    Assert::AreEqual(first, world.Units().at(given[0]).squad);
    Assert::AreEqual(first, world.Units().at(given[1]).squad);
    Assert::AreEqual(second, world.Units().at(given[2]).squad);
  }

  TEST_METHOD(RefusesAMergeAcrossKinds)
  {
    Simulation::World world(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 1> one = {Cell(20, 30)};
    const std::array<Protocol::VoxelPosition, 1> other = {Cell(30, 30)};
    const Protocol::EntityId spearmen = world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, one);
    const Protocol::EntityId archers = world.AddSquad(ATTACKER, Simulation::UnitKind::Archer, other);
    Order(world, ATTACKER, Protocol::MergeSquads{.squads = {spearmen, archers}});
    world.Tick();
    const std::vector<Protocol::RefusalView>& refusals = world.State(ATTACKER).refusals;
    Assert::AreEqual(std::size_t{1}, refusals.size());
    Assert::AreEqual(std::uint16_t{8}, refusals.front().commandTag);
    Assert::IsTrue(refusals.front().reason == Protocol::Refusal::Invalid);
    Assert::AreEqual(std::size_t{1}, Figures(world, spearmen));
    Assert::AreEqual(std::size_t{1}, Figures(world, archers));
  }
};

} // namespace SimulationTests
