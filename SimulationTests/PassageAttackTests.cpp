#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The gatehouse's passage attack (M4-D4; ADR-024, decision 6; MvpPlan.md, M4, input 13): while T5 works and holds a
// garrison, every 4 s it deals 10 crush to every unit in its passage outside the gate, the ram included. T5 stands in a
// masonry wall across the map, its outer face north, its gate x 30 to 32 at z 28 and its passage x 30 to 32, z 29 to 34,
// 4 m high. Its garrison is men-at-arms, who loose nothing, so that every hit point lost is the passage's.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 64;
constexpr std::uint32_t TICKS_PER_ATTACK = 80;
constexpr std::int32_t DAMAGE = 10;

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

/// T5 in its wall with a man-at-arms of the defender's in it, built and ticked once.
struct Gatehouse
{
  Simulation::World world{FlatWorld::Desc()};
  Protocol::EntityId gatehouse = Protocol::NO_ENTITY;
  Protocol::EntityId garrison = Protocol::NO_ENTITY;

  Gatehouse()
  {
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
    Fill(world, At(0, GROUND, 28), At(26, GROUND + 5, 29), MASONRY);
    Fill(world, At(36, GROUND, 28), At(WIDTH - 1, GROUND + 5, 29), MASONRY);
    gatehouse = world.AddStructure(DEFENDER, "T5", At(27, GROUND, 28), 0, "GH");
    const std::array<Protocol::VoxelPosition, 1> cell = {At(31, GROUND, 10)};
    garrison = world.AddSquad(DEFENDER, Simulation::UnitKind::ManAtArms, cell);
    world.PlaceInGarrison(garrison, gatehouse);
    world.Tick();
  }
};

/// A ram of the attacker's at _cell, and its body's unit.
std::pair<Protocol::EntityId, Protocol::EntityId> Ram(Simulation::World& _world, Protocol::VoxelPosition _cell)
{
  const Protocol::EntityId ram = _world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, _cell, 0, Simulation::EngineState::Packed);
  return {ram, _world.Squads().at(_world.SiegeEngines().at(ram).squad).units.front()};
}

/// The units of a squad of _owner's spearmen holding in _cells.
std::vector<Protocol::EntityId> Spearmen(Simulation::World& _world, std::uint8_t _owner, std::span<const Protocol::VoxelPosition> _cells)
{
  const Protocol::EntityId squad = _world.AddSquad(_owner, Simulation::UnitKind::Spearman, _cells);
  Order(_world, _owner, Protocol::HoldSquad{.squad = squad});
  return _world.Squads().at(squad).units;
}

/// The ticks, by number, on which each of _units lost hit points over _ticks ticks, and what it lost on each.
std::map<Protocol::EntityId, std::vector<std::pair<std::uint32_t, std::int32_t>>>
Losses(Simulation::World& _world, std::span<const Protocol::EntityId> _units, std::uint32_t _ticks)
{
  std::map<Protocol::EntityId, std::vector<std::pair<std::uint32_t, std::int32_t>>> losses;
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    std::map<Protocol::EntityId, std::int32_t> before;
    for (const Protocol::EntityId unit : _units)
    {
      before[unit] = _world.Units().at(unit).hitPoints;
    }
    _world.Tick();
    for (const Protocol::EntityId unit : _units)
    {
      const std::int32_t lost = before.at(unit) - _world.Units().at(unit).hitPoints;
      losses[unit];
      if (lost != 0)
      {
        losses[unit].emplace_back(_world.TickCount() - 1, lost);
      }
    }
  }
  return losses;
}

/// Whether _losses are DAMAGE each, TICKS_PER_ATTACK apart, and _count of them.
bool OnTheCadence(const std::vector<std::pair<std::uint32_t, std::int32_t>>& _losses, std::size_t _count)
{
  bool cadence = _losses.size() == _count;
  for (std::size_t loss = 0; loss < _losses.size(); ++loss)
  {
    cadence = cadence && _losses[loss].second == DAMAGE && (loss == 0 || _losses[loss].first - _losses[loss - 1].first == TICKS_PER_ATTACK);
  }
  return cadence;
}

std::size_t GateStanding(const Simulation::World& _world)
{
  std::size_t standing = 0;
  for (std::int32_t y = GROUND; y < GROUND + 4; ++y)
  {
    for (std::int32_t x = 30; x <= 32; ++x)
    {
      standing += _world.Voxels().At(At(x, y, 28)) != 0 ? 1 : 0;
    }
  }
  return standing;
}

} // namespace

TEST_CLASS(PassageAttackTests)
{
public:
  TEST_METHOD(ARamInThePassageTakesTenEveryFourSeconds)
  {
    // 800 ticks hold ten of the cadence's; the ram's roof is pierce armor, and its crush armor is 0.
    Gatehouse site;
    const auto [ram, body] = Ram(site.world, At(30, GROUND, 29));
    const std::array<Protocol::EntityId, 1> units = {body};
    const auto losses = Losses(site.world, units, 10 * TICKS_PER_ATTACK);
    Assert::IsTrue(OnTheCadence(losses.at(body), 10));
    Assert::AreEqual(std::uint32_t{0}, losses.at(body).front().first % TICKS_PER_ATTACK);
  }

  TEST_METHOD(AFootSquadInThePassageTakesItAndOneBeforeItsMouthDoesNot)
  {
    // Three spearmen in the passage, at its gate, its middle and its mouth, and one a cell beyond the mouth.
    Gatehouse site;
    const std::array<Protocol::VoxelPosition, 3> inside = {At(31, GROUND, 29), At(31, GROUND, 31), At(31, GROUND, 34)};
    const std::array<Protocol::VoxelPosition, 1> beyond = {At(31, GROUND, 35)};
    std::vector<Protocol::EntityId> units = Spearmen(site.world, ATTACKER, inside);
    const std::vector<Protocol::EntityId> outside = Spearmen(site.world, ATTACKER, beyond);
    units.insert(units.end(), outside.begin(), outside.end());
    const auto losses = Losses(site.world, units, 4 * TICKS_PER_ATTACK);
    for (std::size_t unit = 0; unit < 3; ++unit)
    {
      Assert::IsTrue(OnTheCadence(losses.at(units[unit]), 4));
    }
    Assert::IsTrue(losses.at(units[3]).empty());
  }

  TEST_METHOD(ItsOwnersFiguresInThePassageTakeItAlike)
  {
    // Every unit in the passage, as GameDesign.md's input 13 says: a sally that stands there is struck too.
    Gatehouse site;
    const std::array<Protocol::VoxelPosition, 1> cell = {At(31, GROUND, 32)};
    const std::vector<Protocol::EntityId> units = Spearmen(site.world, DEFENDER, cell);
    const auto losses = Losses(site.world, units, 2 * TICKS_PER_ATTACK);
    Assert::IsTrue(OnTheCadence(losses.at(units.front()), 2));
  }

  TEST_METHOD(ItStopsWhenTheGarrisonLeaves)
  {
    // The man-at-arms is put out south of the shut gate, where he cannot reach the ram, and is held there.
    Gatehouse site;
    const auto [ram, body] = Ram(site.world, At(30, GROUND, 29));
    const std::array<Protocol::EntityId, 1> units = {body};
    Assert::IsTrue(OnTheCadence(Losses(site.world, units, 2 * TICKS_PER_ATTACK).at(body), 2));
    Order(site.world, DEFENDER, Protocol::EjectGarrison{.structure = site.gatehouse});
    Order(site.world, DEFENDER, Protocol::HoldSquad{.squad = site.garrison});
    site.world.Tick();
    Assert::AreEqual(Protocol::NO_ENTITY, site.world.Squads().at(site.garrison).garrisonedIn);
    Assert::IsTrue(Losses(site.world, units, 4 * TICKS_PER_ATTACK).at(body).empty());
  }

  TEST_METHOD(ARamBreaksTheGateUnderTheAttackWellInsideFourMinutes)
  {
    // The ram at the gate, ordered at the gatehouse. Each blow takes one of the gate's twelve voxels; the seventh leaves
    // five, 41 % of the part, below T5's 50 %, and the gatehouse stops working, which ends the attack and puts the
    // garrison out; the twelfth opens the 3 by 4 m. GameDescription.md asks about four minutes of a gate; the measure
    // is logged against it (MvpPlan.md, M4's risks), and what the ram lost to the passage until then is 10 a hit.
    Gatehouse site;
    Simulation::World& world = site.world;
    const auto [ram, body] = Ram(world, At(30, GROUND, 29));
    Order(world, ATTACKER, Protocol::AttackTarget{.squad = world.SiegeEngines().at(ram).squad, .target = site.gatehouse});
    const std::uint32_t ordered = world.TickCount();
    std::uint32_t firstBlow = 0;
    std::uint32_t stopped = 0;
    std::uint32_t opened = 0;
    for (std::uint32_t tick = 0; tick < 4 * 60 * 20 && opened == 0; ++tick)
    {
      world.Tick();
      firstBlow = firstBlow == 0 && GateStanding(world) < 12 ? world.TickCount() : firstBlow;
      if (stopped == 0 && !world.Structures().at(site.gatehouse).working)
      {
        stopped = world.TickCount();
        Order(world, DEFENDER, Protocol::HoldSquad{.squad = site.garrison});
      }
      opened = GateStanding(world) == 0 ? world.TickCount() : 0;
    }
    Assert::IsTrue(firstBlow > 0 && stopped > firstBlow && opened >= stopped);
    const std::int32_t lost = world.Data().Stats(Simulation::UnitKind::Ram).hitPoints - world.Units().at(body).hitPoints;
    // The attack's ticks from the order's to the one the gatehouse stopped in, whose step 4 runs before its step 10.
    const std::uint32_t hits = ((stopped - 1) / TICKS_PER_ATTACK) - ((ordered - 1) / TICKS_PER_ATTACK);
    Assert::AreEqual(static_cast<std::int32_t>(hits) * DAMAGE, lost);
    Order(world, ATTACKER, Protocol::StopSquad{.squad = world.SiegeEngines().at(ram).squad});
    const std::array<Protocol::EntityId, 1> units = {body};
    Assert::IsTrue(Losses(world, units, 4 * TICKS_PER_ATTACK).at(body).empty());
    const std::wstring report =
      L"M4-D4: the ram's first blow at " + std::to_wstring(firstBlow - ordered) + L" ticks after its order; T5 stops working at " +
      std::to_wstring(stopped - ordered) + L" ticks (" + std::to_wstring((stopped - ordered) / 20) + L" s), its gate open at " +
      std::to_wstring(opened - ordered) + L" ticks (" + std::to_wstring((opened - ordered) / 20) +
      L" s), against GameDescription.md's 240 s; the ram lost " + std::to_wstring(lost) + L" of its 300 to the passage.";
    Logger::WriteMessage(report.c_str());
  }
};

} // namespace SimulationTests
