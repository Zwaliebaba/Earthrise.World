#include "pch.h"

#include "FlatWorld.h"
#include "Replay.h"
#include "Save.h"
#include "Scenario.h"
#include "ServerData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The trial's log's simulation half (M4-J2; ADR-026, decisions 5 and 6): a stone's ShotFired and ShotLanded, and a
// short run of the Siege with the player's own commands, replayed a tick at a time as Earthrise --replay plays one, and
// checked headless as SimulationBench --check-replays checks one (M5-B3).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::int32_t TOP = FlatWorld::GROUND_VOXELS - 1;
constexpr std::uint32_t SHORT_RUN_TICKS = 200;

std::vector<std::byte> Bytes(const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  return writer.TakeBytes();
}

std::vector<Simulation::TrialEvent> EventsOf(const Simulation::World& _world, Simulation::TrialEventKind _kind)
{
  std::vector<Simulation::TrialEvent> events;
  std::ranges::copy_if(_world.LastTickEvents(), std::back_inserter(events),
                       [_kind](const Simulation::TrialEvent& _event) { return _event.kind == _kind; });
  return events;
}

/// A short run of the Siege, SHORT_RUN_TICKS long, its squads each ordered north once, and its replay.
std::vector<std::byte> RecordShortSiege()
{
  Simulation::World world(Simulation::ScenarioDesc(BakedServerData(), "Siege", FlatWorld::BUILD_ID));
  Simulation::LoadScenario(world, "Siege");
  Simulation::ReplayRecorder recorder(world);
  std::vector<Protocol::EntityId> squads;
  for (const auto& [id, squad] : world.Squads())
  {
    if (squad.owner == PLAYER)
    {
      squads.push_back(id);
    }
  }
  for (std::uint32_t tick = 0; tick < SHORT_RUN_TICKS; ++tick)
  {
    if (tick < squads.size())
    {
      const Simulation::Unit& first = world.Units().at(world.Squads().at(squads.at(tick)).units.front());
      world.Submit(PLAYER, Bytes(Protocol::MoveSquad{.squad = squads.at(tick),
                                                     .goalVoxels = {.xVoxels = first.cellVoxels.xVoxels,
                                                                    .yVoxels = first.cellVoxels.yVoxels,
                                                                    .zVoxels = first.cellVoxels.zVoxels + 4}}));
    }
    world.Tick();
    recorder.Record(world);
  }
  return {recorder.Bytes().begin(), recorder.Bytes().end()};
}

} // namespace

TEST_CLASS(SiegeReplayTests)
{
public:
  TEST_METHOD(AStoneRaisesShotFiredWhenThrownAndShotLandedWhereItStrikes)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine =
      world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, {.xVoxels = 5, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = 32});
    const Protocol::VoxelPosition target{.xVoxels = 45, .yVoxels = TOP, .zVoxels = 32};
    world.Submit(PLAYER, Bytes(Protocol::FireEngine{.engine = engine, .targetVoxels = target}));
    world.Tick();
    const std::vector<Simulation::TrialEvent> fired = EventsOf(world, Simulation::TrialEventKind::ShotFired);
    Assert::AreEqual(std::size_t{1}, fired.size());
    Assert::AreEqual(PLAYER, fired.front().player);
    Assert::AreEqual(engine, fired.front().subject);
    Assert::IsTrue(fired.front().voxel == target);
    const Protocol::EntityId shot = fired.front().other;
    for (std::uint32_t tick = 0; tick < 200 && EventsOf(world, Simulation::TrialEventKind::ShotLanded).empty(); ++tick)
    {
      world.Tick();
    }
    const std::vector<Simulation::TrialEvent> landed = EventsOf(world, Simulation::TrialEventKind::ShotLanded);
    Assert::AreEqual(std::size_t{1}, landed.size());
    Assert::AreEqual(shot, landed.front().other);
    Assert::AreEqual(engine, landed.front().subject);
    Assert::AreEqual(1, landed.front().amount);
    Assert::IsTrue(landed.front().voxel == target);
    Assert::IsTrue(EventsOf(world, Simulation::TrialEventKind::ShotFired).empty());
  }

  TEST_METHOD(AShortRunWithThePlayersCommandsReplaysATickAtATime)
  {
    const Simulation::World::Desc desc = Simulation::ScenarioDesc(BakedServerData(), "Siege", FlatWorld::BUILD_ID);
    Simulation::World world(desc);
    Simulation::LoadScenario(world, "Siege");
    Simulation::ReplayRecorder recorder(world);
    // The player's squads ordered north, a step a tick at first and then every twenty, as a player clicks.
    std::vector<Protocol::EntityId> squads;
    for (const auto& [id, squad] : world.Squads())
    {
      if (squad.owner == PLAYER)
      {
        squads.push_back(id);
      }
    }
    Assert::IsFalse(squads.empty());
    for (std::uint32_t tick = 0; tick < SHORT_RUN_TICKS; ++tick)
    {
      if (tick < squads.size() || tick % 20 == 0)
      {
        const Protocol::EntityId squad = squads.at(tick % squads.size());
        const Simulation::Unit& first = world.Units().at(world.Squads().at(squad).units.front());
        world.Submit(PLAYER, Bytes(Protocol::MoveSquad{.squad = squad,
                                                       .goalVoxels = {.xVoxels = first.cellVoxels.xVoxels,
                                                                      .yVoxels = first.cellVoxels.yVoxels,
                                                                      .zVoxels = first.cellVoxels.zVoxels + 4}}));
      }
      world.Tick();
      recorder.Record(world);
    }
    const std::vector<std::byte> bytes(recorder.Bytes().begin(), recorder.Bytes().end());
    Simulation::ReplayPlayer player(bytes, desc);
    std::uint32_t steps = 0;
    while (player.Step())
    {
      ++steps;
    }
    Assert::IsTrue(player.Done());
    Assert::IsTrue(player.Result().matched);
    Assert::AreEqual(SHORT_RUN_TICKS, steps);
    Assert::AreEqual(SHORT_RUN_TICKS, player.Result().ticksReplayed);
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(player.Played()));
    Assert::IsFalse(player.Step()); // nothing past the end
  }

  TEST_METHOD(AShortRunsReplayPassesTheHeadlessCheckWithTheTrialStillRunning)
  {
    const Simulation::ReplayCheck check =
      Simulation::CheckScenarioReplay(RecordShortSiege(), BakedServerData(), "Siege", FlatWorld::BUILD_ID);
    Assert::IsTrue(check.result.matched);
    Assert::AreEqual(SHORT_RUN_TICKS, check.result.ticksReplayed);
    Assert::IsTrue(check.outcome == Protocol::TrialOutcome::Running);
  }

  TEST_METHOD(AReplayWhoseLastStoredHashIsAlteredFailsTheCheckAtItsTick)
  {
    // SHORT_RUN_TICKS is a multiple of REPLAY_HASH_TICKS, so the replay ends in its last tick's hash.
    static_assert(SHORT_RUN_TICKS % Simulation::REPLAY_HASH_TICKS == 0);
    std::vector<std::byte> replay = RecordShortSiege();
    replay.back() ^= std::byte{0x01};
    const Simulation::ReplayCheck check = Simulation::CheckScenarioReplay(replay, BakedServerData(), "Siege", FlatWorld::BUILD_ID);
    Assert::IsFalse(check.result.matched);
    Assert::IsTrue(check.result.firstDifferingTick.has_value());
    Assert::AreEqual(SHORT_RUN_TICKS, check.result.firstDifferingTick.value_or(0U));
  }

  TEST_METHOD(AReplayCheckedUnderAnotherBuildIdIsRefused)
  {
    Assert::ExpectException<std::runtime_error>(
      [] { static_cast<void>(Simulation::CheckScenarioReplay(RecordShortSiege(), BakedServerData(), "Siege", FlatWorld::BUILD_ID + 1)); });
  }
};

} // namespace SimulationTests
