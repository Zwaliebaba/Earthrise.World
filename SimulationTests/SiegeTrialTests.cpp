#include "pch.h"

#include "FlatWorld.h"
#include "Replay.h"
#include "Save.h"
#include "Scenario.h"
#include "SiegeAttacker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The Siege trial in CI (M4-F5), bounded by the owner's decisions of 2026-11-02 (GameDesign.md, M4's phase F, items 8 and
// 9): the scripted attacker against the defender, the voxel version, the control and the mirror each in a run of its own,
// checked against a committed table of when the attacker's tower stopped, the keep stopped and the breach opened, and how
// many stones it threw. A tick of the castle at play costs about 19 times in CI's Debug build what it does in the
// container, so each run stops once the keep has stopped, and each class has a CI shard of its own or shares one. The
// attacker does not win the trial: in the voxel version it stops the tower and the keep, opens its breach at tick 28,547,
// then spends its last stone and loses to the clock, as SimulationBench --siege shows by playing it out. The tables are
// printed whenever they change, as M3's lab keys are, since a retune changes them.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::uint32_t VOXEL_RUN_TICKS = 20000;   // past the keep, at tick 19,685
constexpr std::uint32_t MIRROR_RUN_TICKS = 21000;  // past the keep, at tick 19,685; W6 has not opened by tick 48,000
constexpr std::uint32_t CONTROL_RUN_TICKS = 20000; // the voxel run's, which has stopped the keep by then
constexpr std::uint32_t REPLAY_TICKS = 2000;
constexpr std::array<std::uint32_t, 2> SAVE_TICKS = {1000, 10000};
constexpr std::uint32_t CONTINUE_TICKS = 1000;
constexpr std::uint32_t NOT_YET = 0xffffffffU;
constexpr std::uint32_t CLOCK_TICKS = 6000;
constexpr Protocol::VoxelPosition DITCH_LIP{.xVoxels = 356, .yVoxels = 9, .zVoxels = 290}; // 12 m south of the curtain, by ST
constexpr std::uint32_t ORDER_AGAIN_TICKS = 1200;

/// What one run reached and when, the tick counted from 0 as the loop plays it: the table a run is checked against.
struct Milestones
{
  std::uint32_t towerStoppedTick = NOT_YET;  // the flank tower's first stop: NE, or NW on the mirror
  std::uint32_t keepStoppedTick = NOT_YET;   // the keep's first stop
  std::uint32_t sectionOpenedTick = NOT_YET; // the attacker's breach: E6, or W6 on the mirror
  std::uint32_t stones = 0;                  // thrown by the run's end
  Protocol::TrialOutcome outcome = Protocol::TrialOutcome::Running;

  friend bool operator==(const Milestones&, const Milestones&) = default;
};

/// One run of the trial and what it recorded on the way.
struct Run
{
  Milestones milestones;
  std::vector<std::byte> replay;                  // its first REPLAY_TICKS
  std::vector<std::vector<std::byte>> saves;      // at each of SAVE_TICKS
  std::vector<std::uint64_t> hashesAfterContinue; // the state hash CONTINUE_TICKS after each save
};

/// The trial of a world its scenario gave one, as the Siege's always does.
const Simulation::TrialRules& TrialOf(const Simulation::World& _world)
{
  const std::optional<Simulation::TrialRules>& trial = _world.Trial();
  if (!trial.has_value())
  {
    throw std::runtime_error("the world has no trial");
  }
  return *trial;
}

Simulation::World::Desc DescOf(std::string_view _scenario)
{
  return Simulation::ScenarioDesc(BakedServerData(), _scenario, FlatWorld::BUILD_ID);
}

/// The trial on _scenario in _version with the scripted attacker, run for _ticks or to its outcome. _tower and _section
/// are the names the plan's targets have on that layout.
Run Play(std::string_view _scenario, Protocol::TrialVersion _version, std::uint32_t _ticks, std::string_view _tower,
         std::string_view _section)
{
  Simulation::World world(DescOf(_scenario));
  Simulation::LoadScenario(world, _scenario, {.version = _version, .armies = true});
  world.SetCommander(ATTACKER, std::make_unique<Simulation::SiegeAttacker>(nullptr));
  Run run;
  Simulation::ReplayRecorder recorder(world);
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    for (std::size_t save = 0; save < SAVE_TICKS.size(); ++save)
    {
      if (tick == SAVE_TICKS.at(save))
      {
        run.saves.push_back(Simulation::WriteSave(world));
      }
      if (tick == SAVE_TICKS.at(save) + CONTINUE_TICKS)
      {
        run.hashesAfterContinue.push_back(Simulation::StateHash(world));
      }
    }
    world.Tick();
    if (tick < REPLAY_TICKS)
    {
      recorder.Record(world);
    }
    Milestones& reached = run.milestones;
    for (const Simulation::TrialEvent& event : world.LastTickEvents())
    {
      // A stone thrown is the stone its engine spends: ShotFired comes with the trial's log (M4-J2).
      reached.stones +=
        event.kind == Simulation::TrialEventKind::StoneSpent && event.player == ATTACKER && world.SiegeEngines().contains(event.subject)
          ? 1
          : 0;
    }
    for (const Protocol::StructureView& structure : world.State(DEFENDER).structures)
    {
      if (structure.name == _tower && !structure.working && reached.towerStoppedTick == NOT_YET)
      {
        reached.towerStoppedTick = tick;
      }
      if (structure.name == "KEEP" && !structure.working && reached.keepStoppedTick == NOT_YET)
      {
        reached.keepStoppedTick = tick;
      }
      if (structure.name == _section && structure.opened && reached.sectionOpenedTick == NOT_YET)
      {
        reached.sectionOpenedTick = tick;
      }
    }
    reached.outcome = world.State(ATTACKER).trial.outcome;
    if (reached.outcome != Protocol::TrialOutcome::Running)
    {
      break;
    }
  }
  const std::span<const std::byte> replay = recorder.Bytes();
  run.replay.assign(replay.begin(), replay.end());
  return run;
}

/// Checks _run's milestones against _table, printing the run's own whenever they differ, so that a retune can commit them.
void CheckTable(const Milestones& _run, const Milestones& _table, const std::string& _name)
{
  if (_run != _table)
  {
    Logger::WriteMessage((_name + ": tower stopped at " + std::to_string(_run.towerStoppedTick) + ", keep stopped at " +
                          std::to_string(_run.keepStoppedTick) + ", breach opened at " + std::to_string(_run.sectionOpenedTick) + ", " +
                          std::to_string(_run.stones) + " stones, outcome " + std::to_string(static_cast<int>(_run.outcome)))
                           .c_str());
  }
  Assert::IsTrue(_run == _table);
}

// The committed tables, by layout and version.
constexpr Milestones VOXEL_TABLE{.towerStoppedTick = 9310, .keepStoppedTick = 19685, .sectionOpenedTick = NOT_YET, .stones = 33};
constexpr Milestones MIRROR_TABLE{.towerStoppedTick = 9310, .keepStoppedTick = 19685, .sectionOpenedTick = NOT_YET, .stones = 36};
constexpr Milestones CONTROL_TABLE{.towerStoppedTick = 9309, .keepStoppedTick = NOT_YET, .sectionOpenedTick = NOT_YET, .stones = 33};

const Run& VoxelRun()
{
  static const Run RUN = Play("Siege", Protocol::TrialVersion::Voxel, VOXEL_RUN_TICKS, "NE", "E6");
  return RUN;
}

} // namespace

TEST_CLASS(SiegeTrialTests)
{
public:
  TEST_METHOD(TheVoxelVersionStopsTheTowerAndTheKeepAsTheTableSays)
  {
    // NE stops from the first stand and the keep from the second, which only NE covers, by tick 20,000, the trial still
    // running; E6, beside NE, opens later.
    CheckTable(VoxelRun().milestones, VOXEL_TABLE, "voxel");
  }

  TEST_METHOD(ItsFirst2000TicksReplayWithEveryStateHashEqual)
  {
    const Simulation::ReplayResult replayed = Simulation::Replay(VoxelRun().replay, DescOf("Siege"));
    Assert::IsTrue(replayed.matched);
    Assert::AreEqual(REPLAY_TICKS, replayed.ticksReplayed);
  }

  TEST_METHOD(SavesAt1000And10000EachContinueEqualFor1000Ticks)
  {
    const Run& run = VoxelRun();
    Assert::AreEqual(SAVE_TICKS.size(), run.saves.size());
    for (std::size_t save = 0; save < run.saves.size(); ++save)
    {
      Simulation::World loaded = Simulation::LoadSave(run.saves.at(save), DescOf("Siege"));
      for (std::uint32_t tick = 0; tick < CONTINUE_TICKS; ++tick)
      {
        loaded.Tick();
      }
      Assert::AreEqual(run.hashesAfterContinue.at(save), Simulation::StateHash(loaded));
    }
  }
};

TEST_CLASS(SiegeMirrorTrialTests)
{
public:
  TEST_METHOD(OnTheMirrorTheAttackerMirrorsItsPlan)
  {
    // SiegeMirror puts ST west of the gatehouse, so the attacker takes the west flank: NW, the mirror of NE, then the keep.
    // Its templates are placed, not mirrored, and W6, the mirror of E6, has not opened by the trial's end when the
    // attacker's stone runs out, so the run stops past the keep (MvpPlan.md, M4, input 28).
    CheckTable(Play("SiegeMirror", Protocol::TrialVersion::Voxel, MIRROR_RUN_TICKS, "NW", "W6").milestones, MIRROR_TABLE, "mirror");
  }
};

TEST_CLASS(SiegeControlTrialTests)
{
public:
  TEST_METHOD(TheControlStopsTheTowerButNotTheKeepAsTheTableSays)
  {
    // The same plan against the control's calibrated pools (ADR-028, decision 5): NE stops at 0 and its six stones past
    // the stop take it to -600, but its laborer has it working again in about 150 s, where in the voxel version it stays
    // stopped about 650. The trebuchet turns back to it, the keep is repaired meanwhile, and by tick 20,000 the keep,
    // which the voxel version has stopped, still works; NE's archers kill the trebuchet at tick 21,820.
    CheckTable(Play("Siege", Protocol::TrialVersion::Control, CONTROL_RUN_TICKS, "NE", "E6").milestones, CONTROL_TABLE, "control");
  }
};

TEST_CLASS(SiegeOutcomeTests)
{
public:
  TEST_METHOD(WithNoCommandsTheAttackerLosesToTheClock)
  {
    // The attacker's seat a human's, who gives nothing, and the clock cut to 5 minutes: an idle tick of the Siege costs
    // about 1 ms in the container where a tick of play costs 2.3, so the 40 minutes would take 12 of CI's. Measured in the
    // container at the full limit, it ends the same way at tick 48,000, the defender never sallying against engines 95 m
    // from its curtain.
    Simulation::World world(DescOf("Siege"));
    Simulation::LoadScenario(world, "Siege");
    Simulation::TrialRules rules = TrialOf(world);
    rules.timeLimitTicks = CLOCK_TICKS;
    world.SetTrial(rules);
    while (world.State(ATTACKER).trial.outcome == Protocol::TrialOutcome::Running)
    {
      world.Tick();
    }
    Assert::IsTrue(world.State(ATTACKER).trial.outcome == Protocol::TrialOutcome::LostToClock);
    Assert::AreEqual(CLOCK_TICKS, world.TickCount());
  }

  TEST_METHOD(SentIntoTheTowersReachUntilDeadTheAttackerLosesByItsArmy)
  {
    // Every squad of the attacker's, the engines' bodies among them, sent down one lane to the near lip of the south
    // ditch by ST, inside the towers' reach and the defender's sally's, and sent again each minute, since a squad that
    // fights turns idle where it stands. Down one lane the engines give way to nothing and the figures of one squad
    // trade cells with another's standing idle (ADR-022, decision 3, amended); before, the trebuchet and the figures
    // about it stood where they met for good.
    Simulation::World world(DescOf("Siege"));
    Simulation::LoadScenario(world, "Siege");
    world.Tick();
    const auto send = [&world]
    {
      for (const auto& [id, squad] : world.Squads())
      {
        if (squad.owner == ATTACKER)
        {
          Core::ByteWriter writer;
          Protocol::Encode(Protocol::Command{Protocol::MoveSquad{.squad = id, .goalVoxels = DITCH_LIP}}, writer);
          world.Submit(ATTACKER, writer.Bytes());
        }
      }
    };
    send();
    while (world.State(ATTACKER).trial.outcome == Protocol::TrialOutcome::Running)
    {
      world.Tick();
      if (world.TickCount() % ORDER_AGAIN_TICKS == 0)
      {
        send();
      }
    }
    Assert::IsTrue(world.State(ATTACKER).trial.outcome == Protocol::TrialOutcome::LostArmy);
    Logger::WriteMessage(("the attacker's army was lost at tick " + std::to_string(world.TickCount())).c_str());
  }
};

} // namespace SimulationTests
