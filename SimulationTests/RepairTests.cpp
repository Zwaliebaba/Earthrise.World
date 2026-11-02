#include "pch.h"

#include "BreachLab.h"
#include "BreachRun.h"
#include "FlatWorld.h"
#include "Integrity.h"
#include "ServerDataFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Repair and its lock (M4-D5; ADR-024, decisions 3 and 4; MvpPlan.md, M4, input 11): one of the owner's laborers, set to
// repair a structure and standing within 3 m of its footprint on the owner's side, puts back one removed built voxel for
// each cubic metre of 0.08 m^3 a second, lowest first, paid from the stockpile; a shot, a crater or a ram blow on it locks
// repair for 10 s. A T3 runs east to west, x 40 to 47 and z 60 to 61, its outer face north; the defender's keep stands to
// the south, so the wall's inner side, the owner's, is south.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t STONE = 20000;
constexpr std::uint32_t LOCK_TICKS = 200;

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

/// A world 128 m square with two players, the defender's keep and a T3 of theirs, ticked once; with _across, masonry
/// carries the wall's line across the map, so that nothing walks round it.
struct Site
{
  Simulation::World world{FlatWorld::Desc(4, 4)};
  Protocol::EntityId wall = Protocol::NO_ENTITY;

  explicit Site(bool _across = false)
  {
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = STONE, .timberHundredthsCubicMeters = 2000});
    world.AddStructure(DEFENDER, "T2", At(38, GROUND, 20), 0, "KEEP");
    wall = world.AddStructure(DEFENDER, "T3", At(40, GROUND, 60), 0, "S1");
    if (_across)
    {
      Fill(world, At(0, GROUND, 60), At(39, GROUND + 6, 61), MASONRY);
      Fill(world, At(48, GROUND, 60), At(127, GROUND + 6, 61), MASONRY);
    }
    world.Tick();
  }
};

/// A laborer of the defender's at _cell, set to repair _structure.
Protocol::EntityId Repairing(Simulation::World& _world, Protocol::VoxelPosition _cell, Protocol::EntityId _structure)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {_cell};
  const Protocol::EntityId laborer = _world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cells);
  Order(_world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = _structure});
  return laborer;
}

/// The ticks the rate takes to put back _voxels voxels: whole cubic metres of its Q16.16 a tick, the remainder carried.
std::uint32_t TicksFor(const Simulation::World& _world, std::uint32_t _voxels)
{
  const auto raw = static_cast<std::uint32_t>(_world.Data().Tuning().repairCubicMetersPerTick.Raw());
  return ((_voxels * static_cast<std::uint32_t>(Core::Fixed::ONE_RAW)) + raw - 1) / raw;
}

bool Solid(const Simulation::World& _world, Protocol::VoxelPosition _voxel)
{
  return _world.Voxels().At(_voxel) != Protocol::Chunk::EMPTY;
}

std::size_t StoneSpentEvents(const Simulation::World& _world)
{
  return static_cast<std::size_t>(std::ranges::count_if(_world.LastTickEvents(), [](const Simulation::TrialEvent& _event)
                                                        { return _event.kind == Simulation::TrialEventKind::StoneSpent; }));
}

/// The two-voxel crater, 3 m up the wall's middle, through both its layers; and the ticks until it is whole, at most _limit.
std::uint32_t CraterRepairedAfter(Simulation::World& _world, std::uint32_t _limit, std::size_t* _spentEvents = nullptr)
{
  std::uint32_t ticks = 0;
  while (ticks < _limit && !(Solid(_world, At(43, GROUND + 2, 60)) && Solid(_world, At(43, GROUND + 2, 61))))
  {
    _world.Tick();
    ++ticks;
    if (_spentEvents != nullptr)
    {
      *_spentEvents += StoneSpentEvents(_world);
    }
  }
  return ticks;
}

void Crater(Simulation::World& _world)
{
  _world.Voxels().Edit(At(43, GROUND + 2, 60), Protocol::Chunk::EMPTY);
  _world.Voxels().Edit(At(43, GROUND + 2, 61), Protocol::Chunk::EMPTY);
}

/// The breach run's middle section shot at by its trebuchet, each time it has reloaded, until the section's first
/// collapse, at one of the five-wide hole's columns, west to east: first one whose two voxels stand, fresh or put back
/// whole, then one whose outer voxel stands, then any, each aimed as the run's script aims, under the column's outer
/// face. With _repaired, a laborer of the wall's owner repairs it from the courtyard. The ticks from the first shot to
/// the collapse, and the shots, out.
std::pair<std::uint32_t, std::uint32_t> BreachUnderRepair(bool _repaired)
{
  Simulation::World world(Simulation::BreachRunDesc(BakedServerData(), FlatWorld::BUILD_ID));
  const Simulation::LabSetup site = Simulation::BuildLabSite(world);
  const std::int32_t middleX = site.goalVoxels.xVoxels;
  constexpr std::int32_t SOUTH = Simulation::BreachRun::WALL_SOUTH_VOXELS;
  constexpr std::int32_t ABOVE_BASE = Simulation::BreachRun::GROUND_VOXELS + 1;
  world.Tick(); // tick 0's rebuild puts the enclosure into navigation
  if (_repaired)
  {
    static_cast<void>(Repairing(world, At(middleX, Simulation::BreachRun::GROUND_VOXELS, SOUTH + 3), site.target));
  }
  std::uint32_t shots = 0;
  std::uint32_t firstShot = 0;
  constexpr std::uint32_t MOST_TICKS = 40 * 400;
  for (std::uint32_t tick = 0; tick < MOST_TICKS; ++tick)
  {
    std::optional<std::int32_t> column;
    for (std::int32_t pass = 0; pass < 3 && !column.has_value() && world.SiegeEngines().at(site.engine).reloadTicks == 0; ++pass)
    {
      for (std::int32_t x = middleX - 2; x <= middleX + 2 && !column.has_value(); ++x)
      {
        const bool outer = Solid(world, At(x, ABOVE_BASE, SOUTH));
        const bool inner = Solid(world, At(x, ABOVE_BASE, SOUTH + 1));
        if ((pass == 0 && outer && inner) || (pass == 1 && outer) || (pass == 2 && inner))
        {
          column = x;
        }
      }
    }
    if (column.has_value())
    {
      Order(world, Simulation::BreachRun::PLAYER,
            Protocol::FireEngine{.engine = site.engine, .targetVoxels = At(*column, Simulation::BreachRun::GROUND_VOXELS, SOUTH)});
      firstShot = shots == 0 ? world.TickCount() : firstShot;
      ++shots;
    }
    world.Tick();
    if (std::ranges::any_of(world.Collapses(), [&site](const auto& _collapse) { return _collapse.second.structure == site.target; }))
    {
      return {world.TickCount() - firstShot, shots};
    }
  }
  return {0, shots};
}

} // namespace

TEST_CLASS(RepairTests)
{
public:
  TEST_METHOD(ATwoVoxelCraterIsWholeAgainAfterTheTicksTheRateGivesAndTwoCubicMetersLighter)
  {
    // The two are equally near the footprint's middle and in one column through the wall, so its outer face's comes first.
    Site site;
    Simulation::World& world = site.world;
    Crater(world);
    Repairing(world, At(43, GROUND, 58), site.wall);
    std::size_t spent = 0;
    const std::uint32_t first = TicksFor(world, 1);
    for (std::uint32_t tick = 0; tick < first; ++tick)
    {
      world.Tick();
      spent += StoneSpentEvents(world);
    }
    Assert::IsTrue(Solid(world, At(43, GROUND + 2, 61)));
    Assert::IsFalse(Solid(world, At(43, GROUND + 2, 60)));
    Assert::AreEqual(TicksFor(world, 2) - first, CraterRepairedAfter(world, 2000, &spent));
    Assert::AreEqual(std::size_t{2}, spent);
    Assert::AreEqual(STONE - 200, world.Players().at(DEFENDER).stoneHundredthsCubicMeters);
    Assert::IsTrue(world.Structures().at(site.wall).repairProgress < Core::Fixed::FromRaw(Core::Fixed::ONE_RAW));
  }

  TEST_METHOD(TwoLaborersRepairNoFaster)
  {
    Site site;
    Crater(site.world);
    Repairing(site.world, At(43, GROUND, 58), site.wall);
    Repairing(site.world, At(45, GROUND, 58), site.wall);
    Assert::AreEqual(TicksFor(site.world, 2), CraterRepairedAfter(site.world, 2000));
  }

  TEST_METHOD(ALaborerOnTheOuterSideRepairsNothing)
  {
    // With the wall's line carried across the map, a laborer north of it cannot reach the owner's side; it stands as
    // near as it can, on the outer side, and the crater stays.
    Site site(true);
    Simulation::World& world = site.world;
    Crater(world);
    const Protocol::EntityId laborer = Repairing(world, At(43, GROUND, 66), site.wall);
    const std::uint32_t ticks = TicksFor(world, 2) + 200;
    Assert::AreEqual(ticks, CraterRepairedAfter(world, ticks));
    Assert::AreEqual(0, world.Structures().at(site.wall).repairProgress.Raw());
    Assert::AreEqual(STONE, world.Players().at(DEFENDER).stoneHundredthsCubicMeters);
    Assert::IsTrue(world.Units().at(world.Squads().at(laborer).units.front()).cellVoxels.zVoxels > 61);
  }

  TEST_METHOD(AShotSetsTheLockAndRepairWaitsTwoHundredTicks)
  {
    // A trebuchet of the attacker's, 31 m north of the outer face, throws a stone at it while the crater is repaired: the
    // tick it strikes, the lock is 200 ticks, the integrity reading shows it, and the progress then stands still for
    // exactly those 200 ticks.
    Site site;
    Simulation::World& world = site.world;
    Crater(world);
    Repairing(world, At(43, GROUND, 58), site.wall);
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(45, GROUND, 92));
    world.Tick();
    Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(46, GROUND + 3, 61)});
    for (std::uint32_t tick = 0; tick < 400 && world.LastTickHits().empty(); ++tick)
    {
      world.Tick();
    }
    Assert::IsFalse(world.LastTickHits().empty());
    Assert::AreEqual(LOCK_TICKS, world.Structures().at(site.wall).repairLockTicks);
    Assert::AreEqual(LOCK_TICKS, Simulation::ReadIntegrity(world, site.wall).repairLockTicks);
    const Core::Fixed progress = world.Structures().at(site.wall).repairProgress;
    Assert::IsTrue(progress.Raw() > 0);
    for (std::uint32_t tick = 0; tick < LOCK_TICKS; ++tick)
    {
      world.Tick();
      Assert::IsTrue(world.Structures().at(site.wall).repairProgress == progress);
    }
    Assert::AreEqual(std::uint32_t{0}, world.Structures().at(site.wall).repairLockTicks);
    world.Tick();
    Assert::IsTrue(world.Structures().at(site.wall).repairProgress > progress);
  }

  TEST_METHOD(AVoxelThatWouldStandOnNothingWaits)
  {
    // The wall's base voxel and the one above it gone at x 44 on its outer layer, and the earth under them: neither has
    // anything to stand on, so nothing is put back. The earth back, the base comes first and the one above it after.
    Site site;
    Simulation::World& world = site.world;
    world.Voxels().Edit(At(44, GROUND, 61), Protocol::Chunk::EMPTY);
    world.Voxels().Edit(At(44, GROUND + 1, 61), Protocol::Chunk::EMPTY);
    world.Voxels().Edit(At(44, GROUND - 1, 61), Protocol::Chunk::EMPTY);
    Repairing(world, At(44, GROUND, 58), site.wall);
    for (std::uint32_t tick = 0; tick < TicksFor(world, 2); ++tick)
    {
      world.Tick();
    }
    Assert::IsFalse(Solid(world, At(44, GROUND, 61)));
    Assert::IsFalse(Solid(world, At(44, GROUND + 1, 61)));
    Assert::AreEqual(0, world.Structures().at(site.wall).repairProgress.Raw());
    world.Voxels().Edit(At(44, GROUND - 1, 61), FlatWorld::EARTH);
    for (std::uint32_t tick = 0; tick < TicksFor(world, 1); ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(Solid(world, At(44, GROUND, 61)));
    Assert::IsFalse(Solid(world, At(44, GROUND + 1, 61)));
    for (std::uint32_t tick = 0; tick < TicksFor(world, 1) + 1; ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(Solid(world, At(44, GROUND + 1, 61)));
  }

  TEST_METHOD(RepairStretchesTheBreachRunWithinItsDecidedMultiple)
  {
    // GameDesign.md, the paper tests' values, item 3: with one trebuchet at its 20 s reload, repair adds 1.3 to 2 times
    // to the time to open a section. The breach run's W2, shot until its slot first falls, with and without a laborer in
    // the courtyard 3 m from its inner face; the multiple is measured on the time from the first shot to the fall. Put
    // back a column at a time from the outside, a repaired column is taken again whole by one shot.
    const auto [unrepairedTicks, unrepairedShots] = BreachUnderRepair(false);
    const auto [repairedTicks, repairedShots] = BreachUnderRepair(true);
    Assert::IsTrue(unrepairedTicks > 0 && repairedTicks > 0);
    Assert::AreEqual(std::uint32_t{5}, unrepairedShots);
    const double multiple = static_cast<double>(repairedTicks) / unrepairedTicks;
    const std::wstring report = L"M4-D5: unrepaired, " + std::to_wstring(unrepairedShots) + L" shots and " +
                                std::to_wstring(unrepairedTicks) + L" ticks to the fall; repaired, " + std::to_wstring(repairedShots) +
                                L" shots and " + std::to_wstring(repairedTicks) + L" ticks; the multiple " + std::to_wstring(multiple);
    Logger::WriteMessage(report.c_str());
    Assert::IsTrue(multiple >= 1.3 && multiple <= 2.0);
  }
};

} // namespace SimulationTests
