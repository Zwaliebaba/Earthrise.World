#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The trial's rules and its events (M4-F1; ADR-026, decisions 4 and 5; MvpPlan.md, M4, inputs 15 and 16), on a castle
// built by hand on flat ground, so that units placed by hand meet no garrison: the defender's T2 keep at x 50 to 61 and
// z 90 to 104, a wall line across the map at z 60 to 61 of masonry with a T3, S3, at x 40 to 47 facing south, and the
// camp south of it. The keep's foot is the ring of cells within 3 m of its footprint.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 128;
constexpr std::uint32_t HOLD_TICKS = 2400;
constexpr Protocol::VoxelPosition KEEP{.xVoxels = 50, .yVoxels = GROUND, .zVoxels = 90};

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

struct Castle
{
  Simulation::World world{FlatWorld::Desc(4, 4)};
  Protocol::EntityId keep = Protocol::NO_ENTITY;
  Protocol::EntityId s3 = Protocol::NO_ENTITY;

  explicit Castle(std::uint32_t _timeLimitTicks = 48000)
  {
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
    keep = world.AddStructure(DEFENDER, "T2", KEEP, 0, "KEEP");
    s3 = world.AddStructure(DEFENDER, "T3", At(40, GROUND, 60), 2, "S3");
    Fill(world, At(0, GROUND, 60), At(39, GROUND + 6, 61), MASONRY);
    Fill(world, At(48, GROUND, 60), At(WIDTH - 1, GROUND + 6, 61), MASONRY);
    world.SetTrial({.version = Protocol::TrialVersion::Voxel,
                    .attacker = ATTACKER,
                    .defender = DEFENDER,
                    .keep = keep,
                    .timeLimitTicks = _timeLimitTicks,
                    .holdTicks = HOLD_TICKS,
                    .ringCells = 3,
                    .campMinVoxels = At(30, GROUND, 10),
                    .campMaxVoxels = At(60, GROUND, 25)});
  }

  /// The keep's top seven layers cut away: 236 of its great tower's 1,676 voxels, below its 90 %.
  void StopKeep()
  {
    Fill(world, At(KEEP.xVoxels, GROUND + 15, KEEP.zVoxels), At(KEEP.xVoxels + 11, GROUND + 21, KEEP.zVoxels + 14), Protocol::Chunk::EMPTY);
  }

  [[nodiscard]] const Protocol::TrialView& Trial() const
  {
    return world.State(ATTACKER).trial;
  }
};

/// A squad of one of _owner's, holding at _cell.
Protocol::EntityId Holding(Simulation::World& _world, std::uint8_t _owner, Simulation::UnitKind _kind, Protocol::VoxelPosition _cell)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {_cell};
  const Protocol::EntityId squad = _world.AddSquad(_owner, _kind, cells);
  Order(_world, _owner, Protocol::HoldSquad{.squad = squad});
  return squad;
}

void Run(Simulation::World& _world, std::uint32_t _ticks)
{
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    _world.Tick();
  }
}

std::size_t Raised(const Simulation::World& _world, Simulation::TrialEventKind _kind)
{
  return static_cast<std::size_t>(
    std::ranges::count_if(_world.LastTickEvents(), [_kind](const Simulation::TrialEvent& _event) { return _event.kind == _kind; }));
}

} // namespace

TEST_CLASS(TrialRuleTests)
{
public:
  TEST_METHOD(SoldiersInTheRingOfAWorkingKeepStartNoClock)
  {
    Castle castle;
    Holding(castle.world, ATTACKER, Simulation::UnitKind::Spearman, At(48, GROUND, 95));
    Run(castle.world, 100);
    Assert::AreEqual(std::uint32_t{0}, castle.Trial().holdTicks);
    Assert::IsTrue(castle.Trial().keepWorking);
    Assert::IsTrue(castle.Trial().outcome == Protocol::TrialOutcome::Running);
  }

  TEST_METHOD(TheClockCountsPausesAndResets)
  {
    // The keep stopped and a spearman of the attacker's at its west foot: the clock counts; a defender's spearman at its
    // east foot, 15 m off and out of the other's sight, pauses it; it counts again once he walks away, and goes to 0 when
    // the attacker's walks away too. Back at the foot, a laborer of the defender's at the keep pauses it, and the voxel
    // he puts back sets it to 0.
    Castle castle;
    Simulation::World& world = castle.world;
    castle.StopKeep();
    const Protocol::EntityId attacker = Holding(world, ATTACKER, Simulation::UnitKind::Spearman, At(48, GROUND, 95));
    Run(world, 50);
    Assert::IsFalse(castle.Trial().keepWorking);
    Assert::AreEqual(std::uint32_t{50}, castle.Trial().holdTicks);
    const Protocol::EntityId defender = Holding(world, DEFENDER, Simulation::UnitKind::Spearman, At(63, GROUND, 95));
    Run(world, 30);
    Assert::AreEqual(std::uint32_t{50}, castle.Trial().holdTicks);
    Assert::IsTrue(castle.Trial().holdContested);
    Order(world, DEFENDER, Protocol::MoveSquad{.squad = defender, .goalVoxels = At(80, GROUND, 95)});
    Run(world, 400);
    Assert::IsTrue(castle.Trial().holdTicks > 50 && !castle.Trial().holdContested);
    Order(world, ATTACKER, Protocol::MoveSquad{.squad = attacker, .goalVoxels = At(30, GROUND, 95)});
    Run(world, 400);
    Assert::AreEqual(std::uint32_t{0}, castle.Trial().holdTicks);

    Order(world, ATTACKER, Protocol::MoveSquad{.squad = attacker, .goalVoxels = At(48, GROUND, 95)});
    Run(world, 400);
    Assert::IsTrue(castle.Trial().holdTicks > 0);
    const std::array<Protocol::VoxelPosition, 1> cell = {At(48, GROUND, 105)};
    const Protocol::EntityId laborer = world.AddSquad(DEFENDER, Simulation::UnitKind::Laborer, cell);
    Order(world, DEFENDER, Protocol::RepairStructure{.laborer = laborer, .structure = castle.keep});
    bool restored = false;
    std::uint32_t before = 0;
    for (std::uint32_t tick = 0; tick < 600 && !restored; ++tick)
    {
      before = castle.Trial().holdTicks;
      world.Tick();
      restored = Raised(world, Simulation::TrialEventKind::StoneSpent) > 0;
    }
    Assert::IsTrue(restored);
    Assert::IsTrue(before > 0);
    Assert::AreEqual(std::uint32_t{0}, castle.Trial().holdTicks);
  }

  TEST_METHOD(TwoMinutesHeldWin)
  {
    Castle castle;
    castle.StopKeep();
    Holding(castle.world, ATTACKER, Simulation::UnitKind::Spearman, At(48, GROUND, 95));
    Run(castle.world, HOLD_TICKS - 1);
    Assert::IsTrue(castle.Trial().outcome == Protocol::TrialOutcome::Running);
    castle.world.Tick();
    Assert::IsTrue(castle.Trial().outcome == Protocol::TrialOutcome::Won);
    Assert::AreEqual(std::size_t{1}, Raised(castle.world, Simulation::TrialEventKind::Outcome));
    const std::uint32_t ticksLeft = castle.Trial().ticksLeft;
    Run(castle.world, 10);
    Assert::AreEqual(ticksLeft, castle.Trial().ticksLeft); // the clock stopped
  }

  TEST_METHOD(TheLimitLoses)
  {
    Castle castle(100);
    Holding(castle.world, ATTACKER, Simulation::UnitKind::Spearman, At(40, GROUND, 20));
    Run(castle.world, 99);
    Assert::IsTrue(castle.Trial().outcome == Protocol::TrialOutcome::Running);
    Assert::AreEqual(std::uint32_t{1}, castle.Trial().ticksLeft);
    castle.world.Tick();
    Assert::IsTrue(castle.Trial().outcome == Protocol::TrialOutcome::LostToClock);
    Assert::AreEqual(std::uint32_t{0}, castle.Trial().ticksLeft);
  }

  TEST_METHOD(TheArmyRuleCountsEnginesAlive)
  {
    // A stone from the defender's trebuchet kills the attacker's sapper: with a trebuchet of theirs left they play on, and
    // with nothing left they have lost.
    for (const bool withEngine : {true, false})
    {
      Castle castle;
      Simulation::World& world = castle.world;
      const Protocol::EntityId engine = world.AddSiegeEngine(DEFENDER, Simulation::EngineKind::Trebuchet, At(40, GROUND, 110));
      if (withEngine)
      {
        world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(100, GROUND, 15));
      }
      const Protocol::EntityId sapper = Holding(world, ATTACKER, Simulation::UnitKind::Sapper, At(40, GROUND, 80));
      world.Tick();
      Order(world, DEFENDER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(40, GROUND - 1, 80)});
      for (std::uint32_t tick = 0; tick < 200 && world.Squads().contains(sapper); ++tick)
      {
        world.Tick();
      }
      Assert::IsFalse(world.Squads().contains(sapper));
      world.Tick();
      Assert::IsTrue(castle.Trial().outcome == (withEngine ? Protocol::TrialOutcome::Running : Protocol::TrialOutcome::LostArmy));
    }
  }

  TEST_METHOD(S3OpenedByShotsRaisesOneSectionOpenedAndSixFiguresThroughItSixPasses)
  {
    // A trebuchet 40 m south of S3 opens a hole 5 wide through it a voxel above its base, as the M2 breach run does, and
    // the slot above falls; once a foot route runs from the camp through its footprint to the castle's side it opens, once.
    // Six spearmen from the camp walk through it to the courtyard, each passing it inwards.
    Castle castle;
    Simulation::World& world = castle.world;
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(43, GROUND, 20));
    world.Tick();
    std::size_t opened = 0;
    std::int32_t column = 41;
    for (std::uint32_t tick = 0; tick < 4000; ++tick)
    {
      if (column <= 45 && world.SiegeEngines().at(engine).reloadTicks == 0)
      {
        Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(column++, GROUND, 60)});
      }
      world.Tick();
      opened += Raised(world, Simulation::TrialEventKind::SectionOpened);
    }
    Assert::AreEqual(std::size_t{1}, opened);
    const auto view = std::ranges::find(world.State(ATTACKER).structures, castle.s3, &Protocol::StructureView::id);
    Assert::IsTrue(view != world.State(ATTACKER).structures.end() && view->opened);

    std::vector<Protocol::VoxelPosition> cells;
    cells.reserve(6);
    for (std::int32_t figure = 0; figure < 6; ++figure)
    {
      cells.push_back(At(46 - figure, GROUND, 24));
    }
    const Protocol::EntityId squad = world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, cells);
    Order(world, ATTACKER, Protocol::MoveSquad{.squad = squad, .goalVoxels = At(44, GROUND, 75)});
    std::set<Protocol::EntityId> passed;
    for (std::uint32_t tick = 0; tick < 3000; ++tick)
    {
      world.Tick();
      for (const Simulation::TrialEvent& event : world.LastTickEvents())
      {
        if (event.kind == Simulation::TrialEventKind::FigurePassed)
        {
          Assert::AreEqual(castle.s3, event.other);
          Assert::AreEqual(1, event.amount);
          Assert::IsTrue(passed.insert(event.subject).second);
        }
      }
    }
    Assert::AreEqual(std::size_t{6}, passed.size());
  }
};

} // namespace SimulationTests
