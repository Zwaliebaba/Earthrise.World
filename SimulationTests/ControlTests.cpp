#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The control (M4-F4; ADR-028): structures with one pool of hit points, behind the same seam as the voxel version. A
// stone on a structure takes 100 and cuts nothing; a ram's blow 100, a fourth of it on stone; at 0 a section falls whole,
// a building stops and stands, and stones past 0 take its pool on down to minus its full pool, so that repair must bring
// it back above 0 before it works; a gate goes; repair puts back 50 a cubic metre of stone at the voxel version's rate.
// Every structure starts with 900, input 19's first value, until M4-F5 calibrates them.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t FULL = 900;

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

/// A world 128 m square with two players, a structure of the defender's, _template at _origin, and the trial in _version,
/// that structure its keep. With _army, a spearman of the attacker's stands in the camp, so the first tick does not end the
/// trial for want of an army.
struct Site
{
  Simulation::World world{FlatWorld::Desc(4, 4)};
  Protocol::EntityId structure = Protocol::NO_ENTITY;

  Site(std::string_view _template, Protocol::VoxelPosition _origin, Protocol::TrialVersion _version = Protocol::TrialVersion::Control,
       std::uint32_t _turns = 0, bool _army = false)
  {
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
    structure = world.AddStructure(DEFENDER, _template, _origin, _turns, "S1");
    world.SetTrial({.version = _version,
                    .attacker = ATTACKER,
                    .defender = DEFENDER,
                    .keep = structure,
                    .timeLimitTicks = 48000,
                    .holdTicks = 2400,
                    .ringCells = 3,
                    .campMinVoxels = At(0, GROUND, 0),
                    .campMaxVoxels = At(20, GROUND, 10)});
    if (_army)
    {
      const std::array<Protocol::VoxelPosition, 1> camp = {At(2, GROUND, 2)};
      static_cast<void>(world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, camp));
    }
    world.Tick();
  }

  [[nodiscard]] const Simulation::Structure& Of() const
  {
    return world.Structures().at(structure);
  }
};

/// _shots stones from _engine at _target, each once its reload is done, ticking until each lands; the voxels struck.
std::vector<Protocol::VoxelPosition> Pound(Simulation::World& _world, Protocol::EntityId _engine, Protocol::VoxelPosition _target,
                                           std::size_t _shots)
{
  std::vector<Protocol::VoxelPosition> struck;
  for (std::uint32_t tick = 0; tick < 20000 && struck.size() < _shots; ++tick)
  {
    if (_world.SiegeEngines().at(_engine).reloadTicks == 0 && _world.Shots().empty())
    {
      Order(_world, ATTACKER, Protocol::FireEngine{.engine = _engine, .targetVoxels = _target});
    }
    _world.Tick();
    for (const Simulation::ShotHit& hit : _world.LastTickHits())
    {
      struck.push_back(hit.voxel);
    }
  }
  return struck;
}

/// How many voxels of the box _min to _max stand.
std::size_t Solid(const Simulation::World& _world, Protocol::VoxelPosition _min, Protocol::VoxelPosition _max)
{
  std::size_t solid = 0;
  for (std::int32_t y = _min.yVoxels; y <= _max.yVoxels; ++y)
  {
    for (std::int32_t z = _min.zVoxels; z <= _max.zVoxels; ++z)
    {
      for (std::int32_t x = _min.xVoxels; x <= _max.xVoxels; ++x)
      {
        solid += _world.Voxels().At(At(x, y, z)) != Protocol::Chunk::EMPTY ? 1 : 0;
      }
    }
  }
  return solid;
}

/// _template's pool in the control (ADR-028, decision 5).
std::int32_t PoolOf(const Simulation::World& _world, std::string_view _template)
{
  return _world.Data().FindTemplateRules(_template)->controlHitPoints;
}

/// The stones that take _template's pool to 0.
std::size_t ShotsToZero(const Simulation::World& _world, std::string_view _template)
{
  return static_cast<std::size_t>(PoolOf(_world, _template) / 100);
}

/// The ticks the voxel version's rate takes to repair one cubic metre.
std::uint32_t TicksForACubicMeter(const Simulation::World& _world)
{
  const auto raw = static_cast<std::uint32_t>(_world.Data().Tuning().repairCubicMetersPerTick.Raw());
  return (static_cast<std::uint32_t>(Core::Fixed::ONE_RAW) + raw - 1) / raw;
}

} // namespace

TEST_CLASS(ControlTests)
{
public:
  TEST_METHOD(AStoneOnAT3RemovesNoVoxelAndTakes100)
  {
    // A T3, x 40 to 47 and z 60 to 61, and a trebuchet 31 m north of it.
    Site site("T3", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    Assert::AreEqual(FULL, site.Of().hitPoints);
    const std::size_t before = Solid(world, At(40, GROUND, 60), At(47, GROUND + 6, 61));
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(43, GROUND, 92));
    Pound(world, engine, At(44, GROUND + 3, 61), 1);
    Assert::AreEqual(before, Solid(world, At(40, GROUND, 60), At(47, GROUND + 6, 61)));
    Assert::AreEqual(FULL - 100, site.Of().hitPoints);
    Assert::IsTrue(site.Of().repairLockTicks > 0);
    const Protocol::StructureView& view = world.State(DEFENDER).structures.front();
    Assert::AreEqual(FULL - 100, view.hitPoints);
    Assert::AreEqual(FULL, view.maxHitPoints);
  }

  TEST_METHOD(ARamsBlowTakes100AFourthOfItOnStone)
  {
    // A ram at a T3's foot strikes its stone: 25 of the pool, and no voxel.
    Site site("T3", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    const std::size_t before = Solid(world, At(40, GROUND, 60), At(47, GROUND + 6, 61));
    const Protocol::EntityId ram =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(42, GROUND, 62), 0, Simulation::EngineState::Packed);
    Order(world, ATTACKER, Protocol::AttackTarget{.squad = world.SiegeEngines().at(ram).squad, .target = site.structure});
    for (std::uint32_t tick = 0; tick < 600 && site.Of().hitPoints == FULL; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(FULL - 25, site.Of().hitPoints);
    Assert::AreEqual(before, Solid(world, At(40, GROUND, 60), At(47, GROUND + 6, 61)));
  }

  TEST_METHOD(AtZeroAT3FallsWholeWithItsWarningAndLeavesItsRubble)
  {
    Site site("T3", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(43, GROUND, 92));
    Pound(world, engine, At(44, GROUND + 3, 61), 8);
    Assert::AreEqual(100, site.Of().hitPoints);
    Assert::IsTrue(world.Collapses().empty());
    Pound(world, engine, At(44, GROUND + 3, 61), 1);
    Assert::AreEqual(0, site.Of().hitPoints);
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    const Simulation::Collapse& collapse = world.Collapses().begin()->second;
    Assert::AreEqual(site.structure, collapse.structure);
    Assert::IsTrue(collapse.plan.warningTicks > 0);
    Assert::AreEqual(std::size_t{96}, collapse.piece.size()); // every voxel of it, 96 m^3
    for (std::uint32_t tick = 0; tick < 600 && !world.Collapses().empty(); ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(world.Collapses().empty());
    // None of its building stone stands, and its rubble lies about its footprint.
    std::size_t stone = 0;
    std::size_t rubble = 0;
    for (std::int32_t z = 50; z <= 70; ++z)
    {
      for (std::int32_t x = 30; x <= 57; ++x)
      {
        for (std::int32_t y = GROUND; y <= GROUND + 6; ++y)
        {
          const Simulation::Behavior behavior = world.Data().Materials().at(world.Voxels().At(At(x, y, z))).behavior;
          stone += behavior == Simulation::Behavior::BuildingStone ? 1 : 0;
          rubble += behavior == Simulation::Behavior::StoneRubble ? 1 : 0;
        }
      }
    }
    Assert::AreEqual(std::size_t{0}, stone);
    Assert::IsTrue(rubble > 0);
    // Fallen whole, it is not repaired: a laborer at it puts back no hit point and spends no stone.
    const std::array<Protocol::VoxelPosition, 1> cell = {At(43, GROUND, 56)};
    const Protocol::EntityId laborer = world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cell);
    Order(world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = site.structure});
    const std::int32_t stock = world.Players().at(DEFENDER).stoneHundredthsCubicMeters;
    for (std::uint32_t tick = 0; tick < 600; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(0, site.Of().hitPoints);
    Assert::AreEqual(stock, world.Players().at(DEFENDER).stoneHundredthsCubicMeters);
  }

  TEST_METHOD(AT4AtZeroStopsAndStands)
  {
    Site site("T4", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    const std::size_t before = Solid(world, At(40, GROUND, 60), At(46, GROUND + 12, 66));
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(42, GROUND, 98));
    Pound(world, engine, At(43, GROUND + 4, 66), 9);
    Assert::AreEqual(0, site.Of().hitPoints);
    world.Tick();
    Assert::IsFalse(site.Of().working);
    Assert::IsTrue(world.Collapses().empty());
    Assert::AreEqual(before, Solid(world, At(40, GROUND, 60), At(46, GROUND + 12, 66)));
  }

  TEST_METHOD(StonesPastTheStopTakeAT4BelowZeroAndRepairMustBringItBackAboveZero)
  {
    // Two stones past its stop take a T4 to -200, which its view shows as 0. Once its lock runs out, a laborer's first
    // 4 m^3 bring it back to 0, still stopped, and the fifth to 50, working (the owner, 2026-11-02).
    Site site("T4", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    const std::array<Protocol::VoxelPosition, 1> cell = {At(43, GROUND, 57)};
    const Protocol::EntityId laborer = world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cell);
    Order(world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = site.structure});
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(42, GROUND, 98));
    Pound(world, engine, At(43, GROUND + 4, 66), ShotsToZero(world, "T4") + 2);
    Assert::AreEqual(-200, site.Of().hitPoints);
    world.Tick();
    Assert::IsFalse(site.Of().working);
    Assert::AreEqual(0, world.State(DEFENDER).structures.front().hitPoints);
    for (std::uint32_t tick = 0; tick < 1000 && site.Of().repairLockTicks > 0; ++tick)
    {
      world.Tick();
    }
    for (std::uint32_t tick = 0; tick < 2000 && site.Of().hitPoints < 0; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(0, site.Of().hitPoints);
    world.Tick();
    Assert::IsFalse(site.Of().working);
    for (std::uint32_t tick = 0; tick < 2000 && site.Of().hitPoints <= 0; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(50, site.Of().hitPoints);
    world.Tick();
    Assert::IsTrue(site.Of().working);
  }

  TEST_METHOD(ItsPoolGoesNoLowerThanMinusItsFullPool)
  {
    // A T4 struck its pool's worth twice over and once more stands at minus its pool.
    Site site("T4", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(42, GROUND, 98));
    Pound(world, engine, At(43, GROUND + 4, 66), (2 * ShotsToZero(world, "T4")) + 1);
    Assert::AreEqual(-PoolOf(world, "T4"), site.Of().hitPoints);
  }

  TEST_METHOD(AShotOnT5sStoneTakesFromTheOnePoolAndAtZeroTheGateGoesAndGHStops)
  {
    // T5 at x 27 to 35 and z 28 to 34, its gate x 30 to 32 at z 28; the stones strike its roof's stone, never the gate.
    Site site("T5", At(27, GROUND, 28));
    Simulation::World& world = site.world;
    const auto gate = [&world] { return Solid(world, At(30, GROUND, 28), At(32, GROUND + 3, 28)); };
    Assert::AreEqual(std::size_t{12}, gate());
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(30, GROUND, 66));
    const std::vector<Protocol::VoxelPosition> struck = Pound(world, engine, At(31, GROUND + 5, 34), 8);
    Assert::AreEqual(std::size_t{12}, gate());
    Assert::AreEqual(100, site.Of().hitPoints);
    Assert::IsTrue(site.Of().working);
    Pound(world, engine, At(31, GROUND + 5, 34), 1);
    world.Tick();
    Assert::AreEqual(0, site.Of().hitPoints);
    Assert::AreEqual(std::size_t{0}, gate());
    Assert::IsFalse(site.Of().working);
  }

  TEST_METHOD(AGateAtZeroIsRemoved)
  {
    // A palisade gate, T7, pounded to 0 from 30 m: its gate's voxels go, and it stops.
    Site site("T7", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    std::vector<Protocol::VoxelPosition> gate;
    const Simulation::ServerTemplate* found = world.Data().FindTemplate("T7");
    std::size_t index = 0;
    for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
    {
      for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
      {
        for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++index)
        {
          if (found->parts.at(index) == Simulation::ServerTemplate::Part::Critical && found->indices.at(index) != 0)
          {
            gate.push_back(At(40 + static_cast<std::int32_t>(x), GROUND + static_cast<std::int32_t>(y), 60 + static_cast<std::int32_t>(z)));
          }
        }
      }
    }
    Assert::IsFalse(gate.empty());
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(41, GROUND, 92));
    Pound(world, engine, gate.front(), 9);
    world.Tick();
    Assert::AreEqual(0, site.Of().hitPoints);
    Assert::IsTrue(std::ranges::all_of(gate, [&world](const Protocol::VoxelPosition& _voxel) { return world.Voxels().At(_voxel) == 0; }));
    Assert::IsFalse(site.Of().working);
    // Its gate gone, it is not repaired: a laborer at it puts back no hit point and spends no timber.
    const std::array<Protocol::VoxelPosition, 1> cell = {At(41, GROUND, 57)};
    const Protocol::EntityId laborer = world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cell);
    Order(world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = site.structure});
    const Simulation::Player stock = world.Players().at(DEFENDER);
    for (std::uint32_t tick = 0; tick < 600; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(0, site.Of().hitPoints);
    Assert::AreEqual(stock.timberHundredthsCubicMeters, world.Players().at(DEFENDER).timberHundredthsCubicMeters);
    Assert::AreEqual(stock.stoneHundredthsCubicMeters, world.Players().at(DEFENDER).stoneHundredthsCubicMeters);
  }

  TEST_METHOD(RepairPutsBack50ACubicMetreOfStoneAtTheVoxelVersionsRate)
  {
    // A stone takes 100, a crater's 2 m^3 in the voxel version, so a cubic metre of repair is worth 50 in both, and a
    // window repairs the same share of a shot: after its lock, the laborer's progress runs at the voxel version's rate and
    // the pool gains 50 at each whole cubic metre.
    Site site("T3", At(40, GROUND, 60));
    Simulation::World& world = site.world;
    const std::array<Protocol::VoxelPosition, 1> cell = {At(43, GROUND, 58)};
    const Protocol::EntityId laborer = world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cell);
    Order(world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = site.structure});
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(43, GROUND, 92));
    Pound(world, engine, At(44, GROUND + 3, 61), 1);
    Assert::AreEqual(FULL - 100, site.Of().hitPoints);
    for (std::uint32_t tick = 0; tick < 200; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(std::uint32_t{0}, site.Of().repairLockTicks);
    const std::int32_t stone = world.Players().at(DEFENDER).stoneHundredthsCubicMeters;
    for (std::uint32_t tick = 0; tick < TicksForACubicMeter(world); ++tick)
    {
      Assert::AreEqual(FULL - 100, site.Of().hitPoints);
      world.Tick();
    }
    Assert::AreEqual(FULL - 50, site.Of().hitPoints);
    Assert::AreEqual(stone - 100, world.Players().at(DEFENDER).stoneHundredthsCubicMeters);
  }

  TEST_METHOD(ATenSecondWindowRestoresTheSameShareOfAShotInBothVersions)
  {
    // One stone on a T3's north face and a laborer at its foot. From the tick its lock runs out, each 10 s window of
    // repair puts back the same share of the shot in both versions: in the voxel version the voxels back and the progress
    // over the crater's 2 m^3, in the control the hit points back and the progress at 50 a cubic metre over its 100. A
    // window is 0.8 m^3 at 0.08 m^3/s, so the first ends short of a whole voxel and the second past one.
    constexpr std::uint32_t WINDOW_TICKS = 200;
    std::vector<std::vector<std::int64_t>> shares;
    for (const Protocol::TrialVersion version : {Protocol::TrialVersion::Voxel, Protocol::TrialVersion::Control})
    {
      Site site("T3", At(40, GROUND, 60), version);
      Simulation::World& world = site.world;
      const std::array<Protocol::VoxelPosition, 1> cell = {At(43, GROUND, 58)};
      const Protocol::EntityId laborer = world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cell);
      Order(world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = site.structure});
      const auto standing = [&world] { return static_cast<std::int64_t>(Solid(world, At(40, GROUND, 60), At(47, GROUND + 6, 61))); };
      const std::int64_t full = standing();
      const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(43, GROUND, 92));
      Pound(world, engine, At(44, GROUND + 3, 61), 1);
      for (std::uint32_t tick = 0; tick < 1000 && site.Of().repairLockTicks > 0; ++tick)
      {
        world.Tick();
      }
      const std::int64_t cut = full - standing();
      const std::int32_t hitPoints = site.Of().hitPoints;
      shares.emplace_back();
      for (std::uint32_t window = 0; window < 2; ++window)
      {
        for (std::uint32_t tick = 0; tick < WINDOW_TICKS; ++tick)
        {
          world.Tick();
        }
        // The share of the shot put back, in Core::Fixed's raw units of a whole shot.
        const std::int64_t progress = site.Of().repairProgress.Raw();
        const std::int64_t back = version == Protocol::TrialVersion::Voxel
                                    ? (((standing() - (full - cut)) * Core::Fixed::ONE_RAW) + progress) / 2
                                    : ((std::int64_t{site.Of().hitPoints - hitPoints} * Core::Fixed::ONE_RAW) + (progress * 50)) / 100;
        shares.back().push_back(back);
      }
      Assert::AreEqual(version == Protocol::TrialVersion::Voxel ? std::int64_t{2} : std::int64_t{0}, cut);
    }
    Assert::IsTrue(shares[0][0] > 0 && shares[0][0] < Core::Fixed::ONE_RAW / 2);
    Assert::IsTrue(shares[0][1] > Core::Fixed::ONE_RAW / 2);
    Assert::IsTrue(shares[0] == shares[1]);
  }

  TEST_METHOD(TheStormingClockResetsWhenTheKeepRegainsAHitPoint)
  {
    // The keep, a T2, pounded to 0 stops; a soldier of the attacker's at its foot starts the clock; a laborer of the
    // defender's brings it back to 50, which makes it work again and so sets the clock to 0.
    Site site("T2", At(50, GROUND, 60), Protocol::TrialVersion::Control, 0, true);
    Simulation::World& world = site.world;
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(54, GROUND, 105));
    Pound(world, engine, At(55, GROUND + 6, 74), 9);
    world.Tick();
    Assert::IsFalse(site.Of().working);
    const std::array<Protocol::VoxelPosition, 1> soldier = {At(48, GROUND, 65)};
    const Protocol::EntityId spearman = world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, soldier);
    Order(world, ATTACKER, Protocol::HoldSquad{.squad = spearman});
    for (std::uint32_t tick = 0; tick < 50; ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(world.State(ATTACKER).trial.holdTicks > 0);
    const std::array<Protocol::VoxelPosition, 1> cell = {At(64, GROUND, 65)};
    const Protocol::EntityId laborer = world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cell);
    Order(world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = site.structure});
    for (std::uint32_t tick = 0; tick < 1000 && site.Of().hitPoints == 0; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(50, site.Of().hitPoints);
    Assert::AreEqual(std::uint32_t{0}, world.State(ATTACKER).trial.holdTicks);
  }

  TEST_METHOD(TerrainCratersAsInTheVoxelVersion)
  {
    // A stone into open earth 20 m from the T3 takes the same voxels in both versions.
    std::vector<std::size_t> taken;
    for (const Protocol::TrialVersion version : {Protocol::TrialVersion::Voxel, Protocol::TrialVersion::Control})
    {
      Site site("T3", At(40, GROUND, 60), version);
      Simulation::World& world = site.world;
      const std::size_t before = Solid(world, At(36, GROUND - 4, 76), At(46, GROUND, 86));
      const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(41, GROUND, 110));
      Pound(world, engine, At(41, GROUND - 1, 81), 1);
      taken.push_back(before - Solid(world, At(36, GROUND - 4, 76), At(46, GROUND, 86)));
    }
    Assert::IsTrue(taken[0] > 0);
    Assert::AreEqual(taken[0], taken[1]);
  }

  TEST_METHOD(UnitsFightAlikeInBothVersions)
  {
    // Six spearmen against three outriders, away from any structure: the same hit points after 300 ticks in both.
    std::vector<std::vector<std::int32_t>> hitPoints;
    for (const Protocol::TrialVersion version : {Protocol::TrialVersion::Voxel, Protocol::TrialVersion::Control})
    {
      Site site("T3", At(100, GROUND, 100), version);
      Simulation::World& world = site.world;
      std::vector<Protocol::VoxelPosition> spearmen;
      spearmen.reserve(6);
      for (std::int32_t figure = 0; figure < 6; ++figure)
      {
        spearmen.push_back(At(20 + figure, GROUND, 30));
      }
      const std::array<Protocol::VoxelPosition, 3> outriders = {At(21, GROUND, 34), At(22, GROUND, 34), At(23, GROUND, 34)};
      world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, spearmen);
      world.AddSquad(DEFENDER, Simulation::UnitKind::Outrider, outriders);
      for (std::uint32_t tick = 0; tick < 300; ++tick)
      {
        world.Tick();
      }
      std::vector<std::int32_t> each;
      for (const auto& [id, unit] : world.Units())
      {
        each.push_back(unit.hitPoints);
      }
      hitPoints.push_back(each);
    }
    Assert::IsTrue(hitPoints[0] == hitPoints[1]);
  }
};

} // namespace SimulationTests
