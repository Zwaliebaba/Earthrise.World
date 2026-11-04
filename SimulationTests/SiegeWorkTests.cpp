#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Siege work as the trial's events (M5-B1; MvpPlan-M5.md, Contracts): a stone that strikes a structure, a ram's blow on
// one, and a cubic metre a sapper digs within repairLockReachVoxels of a section each raise an event priced in the
// control's hit points, in both versions alike, so that the playtest counts where a player tried to breach in one
// currency whichever version they play. The prices are read from the baked data, as the control's own tests read them.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::array<Protocol::TrialVersion, 2> VERSIONS = {Protocol::TrialVersion::Voxel, Protocol::TrialVersion::Control};

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

/// A world 128 m square with two players, _template of the defender's at x 40 to 47 and z 60 to 61, named S1 and the
/// trial's keep, in _version, with a spearman of the attacker's in the camp so that the trial does not end for want of
/// an army.
struct Site
{
  Simulation::World world{FlatWorld::Desc(4, 4)};
  Protocol::EntityId structure = Protocol::NO_ENTITY;

  Site(std::string_view _template, Protocol::TrialVersion _version)
  {
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
    structure = world.AddStructure(DEFENDER, _template, At(40, GROUND, 60), 0, "S1");
    world.SetTrial({.version = _version,
                    .attacker = ATTACKER,
                    .defender = DEFENDER,
                    .keep = structure,
                    .timeLimitTicks = 48000,
                    .holdTicks = 2400,
                    .ringCells = 3,
                    .campMinVoxels = At(0, GROUND, 0),
                    .campMaxVoxels = At(20, GROUND, 10)});
    const std::array<Protocol::VoxelPosition, 1> camp = {At(2, GROUND, 2)};
    static_cast<void>(world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, camp));
    world.Tick();
  }
};

/// Ticks _world up to _ticks times, stopping after the first tick that raised _kind, and gives that tick's events of it.
std::vector<Simulation::TrialEvent> TickUntil(Simulation::World& _world, Simulation::TrialEventKind _kind, std::uint32_t _ticks)
{
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    _world.Tick();
    std::vector<Simulation::TrialEvent> raised;
    std::ranges::copy_if(_world.LastTickEvents(), std::back_inserter(raised),
                         [_kind](const Simulation::TrialEvent& _event) { return _event.kind == _kind; });
    if (!raised.empty())
    {
      return raised;
    }
  }
  return {};
}

/// The first stone a trebuchet 31 m north of the site throws at _target, and what it raised.
std::vector<Simulation::TrialEvent> OneStone(Site& _site, Protocol::VoxelPosition _target, Protocol::EntityId* _engine)
{
  *_engine = _site.world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(43, GROUND, 92));
  Order(_site.world, ATTACKER, Protocol::FireEngine{.engine = *_engine, .targetVoxels = _target});
  return TickUntil(_site.world, Simulation::TrialEventKind::ShotLanded, 2000).empty()
           ? std::vector<Simulation::TrialEvent>{}
           : std::vector<Simulation::TrialEvent>(_site.world.LastTickEvents().begin(), _site.world.LastTickEvents().end());
}

/// _events of _kind.
std::vector<Simulation::TrialEvent> OfKind(const std::vector<Simulation::TrialEvent>& _events, Simulation::TrialEventKind _kind)
{
  std::vector<Simulation::TrialEvent> found;
  std::ranges::copy_if(_events, std::back_inserter(found), [_kind](const Simulation::TrialEvent& _event) { return _event.kind == _kind; });
  return found;
}

/// The first blow of a ram at the site's north foot, ordered at it, and what it raised.
std::vector<Simulation::TrialEvent> OneBlow(Site& _site, Protocol::EntityId* _ram)
{
  *_ram = _site.world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(42, GROUND, 62), 0, Simulation::EngineState::Packed);
  Order(_site.world, ATTACKER, Protocol::AttackTarget{.squad = _site.world.SiegeEngines().at(*_ram).squad, .target = _site.structure});
  return TickUntil(_site.world, Simulation::TrialEventKind::RamStruck, 600);
}

/// Six sappers north of _min to _max, set digging it; the first tick's SapperDug events.
std::vector<Simulation::TrialEvent> FirstDig(Site& _site, Protocol::VoxelPosition _min, Protocol::VoxelPosition _max, std::uint32_t _ticks,
                                             Protocol::EntityId* _squad)
{
  std::vector<Protocol::VoxelPosition> cells;
  for (std::int32_t x = 40; x <= 45; ++x)
  {
    cells.push_back(At(x, GROUND, _max.zVoxels + 1));
  }
  *_squad = _site.world.AddSquad(ATTACKER, Simulation::UnitKind::Sapper, cells);
  Order(_site.world, ATTACKER, Protocol::DigTrench{.squad = *_squad, .minVoxels = _min, .maxVoxels = _max});
  return TickUntil(_site.world, Simulation::TrialEventKind::SapperDug, _ticks);
}

void AreEqual(const Simulation::TrialEvent& _expected, const Simulation::TrialEvent& _actual)
{
  Assert::IsTrue(_expected.kind == _actual.kind);
  Assert::AreEqual(_expected.tick, _actual.tick);
  Assert::AreEqual(_expected.player, _actual.player);
  Assert::AreEqual(_expected.subject, _actual.subject);
  Assert::AreEqual(_expected.other, _actual.other);
  Assert::AreEqual(_expected.amount, _actual.amount);
  Assert::IsTrue(_expected.voxel == _actual.voxel);
}

} // namespace

TEST_CLASS(SiegeWorkTests)
{
public:
  TEST_METHOD(AStoneOnAT3RaisesStoneStruckAtTheControlsPriceAlikeInBothVersions)
  {
    std::vector<Simulation::TrialEvent> struck;
    for (const Protocol::TrialVersion version : VERSIONS)
    {
      Site site("T3", version);
      Protocol::EntityId engine = Protocol::NO_ENTITY;
      const std::vector<Simulation::TrialEvent> stone =
        OfKind(OneStone(site, At(44, GROUND + 3, 61), &engine), Simulation::TrialEventKind::StoneStruck);
      Assert::AreEqual(std::size_t{1}, stone.size());
      Assert::AreEqual(site.world.Data().Tuning().controlShotDamage, stone.front().amount);
      Assert::AreEqual(ATTACKER, stone.front().player);
      Assert::AreEqual(engine, stone.front().subject);
      Assert::AreEqual(site.structure, stone.front().other);
      struck.push_back(stone.front());
    }
    AreEqual(struck.at(0), struck.at(1));
  }

  TEST_METHOD(AStoneOnTheGroundRaisesNoStoneStruck)
  {
    for (const Protocol::TrialVersion version : VERSIONS)
    {
      Site site("T3", version);
      Protocol::EntityId engine = Protocol::NO_ENTITY;
      const std::vector<Simulation::TrialEvent> landed = OneStone(site, At(44, GROUND - 1, 74), &engine);
      Assert::IsFalse(OfKind(landed, Simulation::TrialEventKind::ShotLanded).empty());
      Assert::IsTrue(OfKind(landed, Simulation::TrialEventKind::StoneStruck).empty());
    }
  }

  TEST_METHOD(ARamsBlowOnStoneRaisesRamStruckAtAFourthAlikeInBothVersions)
  {
    std::vector<Simulation::TrialEvent> blows;
    for (const Protocol::TrialVersion version : VERSIONS)
    {
      Site site("T3", version);
      Protocol::EntityId ram = Protocol::NO_ENTITY;
      const std::vector<Simulation::TrialEvent> blow = OneBlow(site, &ram);
      Assert::AreEqual(std::size_t{1}, blow.size());
      const Simulation::Rules& rules = site.world.Data().Tuning();
      Assert::AreEqual((rules.controlRamDamage * rules.controlRamStonePercent) / 100, blow.front().amount);
      Assert::AreEqual(ram, blow.front().subject);
      Assert::AreEqual(site.structure, blow.front().other);
      blows.push_back(blow.front());
    }
    AreEqual(blows.at(0), blows.at(1));
  }

  TEST_METHOD(ARamsBlowOnTimberRaisesRamStruckAtTheFullPrice)
  {
    // A T6, the palisade, is timber: its blow is controlRamDamage whole.
    std::vector<Simulation::TrialEvent> blows;
    for (const Protocol::TrialVersion version : VERSIONS)
    {
      Site site("T6", version);
      Protocol::EntityId ram = Protocol::NO_ENTITY;
      const std::vector<Simulation::TrialEvent> blow = OneBlow(site, &ram);
      Assert::AreEqual(std::size_t{1}, blow.size());
      Assert::AreEqual(site.world.Data().Tuning().controlRamDamage, blow.front().amount);
      blows.push_back(blow.front());
    }
    AreEqual(blows.at(0), blows.at(1));
  }

  TEST_METHOD(ASappersCubicMetreBesideASectionRaisesSapperDugAlikeInBothVersions)
  {
    // A 2 x 2 x 2 m box against the T3's north face, z 62 and 63, within the 2 m reach.
    std::vector<Simulation::TrialEvent> dug;
    for (const Protocol::TrialVersion version : VERSIONS)
    {
      Site site("T3", version);
      Protocol::EntityId squad = Protocol::NO_ENTITY;
      const std::vector<Simulation::TrialEvent> first = FirstDig(site, At(42, GROUND - 2, 62), At(43, GROUND - 1, 63), 600, &squad);
      Assert::AreEqual(std::size_t{1}, first.size());
      Assert::AreEqual(site.world.Data().Tuning().controlTrenchDamagePerCubicMeter, first.front().amount);
      Assert::AreEqual(squad, first.front().subject);
      Assert::AreEqual(site.structure, first.front().other);
      dug.push_back(first.front());
    }
    AreEqual(dug.at(0), dug.at(1));
  }

  TEST_METHOD(ACubicMetreThreeMetresOutRaisesNoSapperDug)
  {
    // The box z 64 and 65: 3 and 4 m out from the T3's north face, past the reach. The dig runs long enough to take
    // several cubic metres.
    for (const Protocol::TrialVersion version : VERSIONS)
    {
      Site site("T3", version);
      Protocol::EntityId squad = Protocol::NO_ENTITY;
      Assert::IsTrue(FirstDig(site, At(42, GROUND - 2, 64), At(43, GROUND - 1, 65), 400, &squad).empty());
    }
  }
};

} // namespace SimulationTests
