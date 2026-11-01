#include "pch.h"

#include "FlatWorld.h"
#include "Replay.h"
#include "Script.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A replay is a start and the commands, with a state hash every 20 ticks (ADR-014, decision 5).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::uint32_t FLIPPED_TICK = 45;

Protocol::VoxelPosition Cell(std::int32_t _x, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = _z};
}

std::vector<Simulation::ScriptedCommand> Script()
{
  std::vector<Simulation::ScriptedCommand> script;
  for (std::uint32_t tick = 1; tick < 200; tick += 11)
  {
    script.push_back({.tick = tick,
                      .player = PLAYER,
                      .command = Protocol::MoveSquad{.squad = 1, .goalVoxels = Cell(static_cast<std::int32_t>(tick % 61), 3)}});
  }
  // The command whose byte the tests flip, to the second squad, which nothing else orders: its goal,
  // 57, appears nowhere else in the replay.
  script.push_back({.tick = FLIPPED_TICK, .player = PLAYER, .command = Protocol::MoveSquad{.squad = 4, .goalVoxels = Cell(57, 13)}});
  return script;
}

/// Records 200 ticks of the script from a world with one squad.
std::vector<std::byte> Record()
{
  Simulation::World world(FlatWorld::Desc());
  const std::array<Protocol::VoxelPosition, 2> cells = {Cell(2, 2), Cell(3, 2)};
  static_cast<void>(world.AddSquad(PLAYER, Simulation::NavigationClass::Foot, cells));
  static_cast<void>(world.AddSquad(PLAYER, Simulation::NavigationClass::Foot, cells));
  world.Voxels().Edit(Cell(30, 30), 6);
  Simulation::ReplayRecorder recorder(world);
  const std::vector<Simulation::ScriptedCommand> script = Script();
  for (std::uint32_t tick = 0; tick < 200; ++tick)
  {
    Simulation::RunScript(world, script, 1);
    recorder.Record(world);
  }
  return {recorder.Bytes().begin(), recorder.Bytes().end()};
}

} // namespace

TEST_CLASS(ReplayTests)
{
public:
  TEST_METHOD(ReplaysTwoHundredTicksWithEveryHashEqual)
  {
    const Simulation::ReplayResult result = Simulation::Replay(Record(), FlatWorld::Desc());
    Assert::IsTrue(result.matched);
    Assert::IsFalse(result.firstDifferingTick.has_value());
    Assert::AreEqual(std::uint32_t{200}, result.ticksReplayed);
  }

  TEST_METHOD(FindsAFlippedCommandByteAtTheNextHash)
  {
    std::vector<std::byte> replay = Record();
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::MoveSquad{.squad = 4, .goalVoxels = Cell(57, 13)}, writer);
    const auto found = std::ranges::search(replay, writer.Bytes());
    Assert::IsFalse(found.empty());
    // The goal's x, after the tag, the version and the squad's id.
    replay[static_cast<std::size_t>(found.begin() - replay.begin()) + 12] ^= std::byte{0x01};
    const Simulation::ReplayResult result = Simulation::Replay(replay, FlatWorld::Desc());
    Assert::IsFalse(result.matched);
    // Applied in tick 45, so the first hash after it, at 60, differs: within 20 ticks.
    Assert::AreEqual(std::uint32_t{60}, result.firstDifferingTick.value_or(0));
  }

  TEST_METHOD(RefusesAReplayFromAnotherBuild)
  {
    Simulation::World::Desc otherBuild = FlatWorld::Desc();
    otherBuild.buildId += 1;
    const std::vector<std::byte> replay = Record();
    Assert::ExpectException<std::runtime_error>([&] { static_cast<void>(Simulation::Replay(replay, otherBuild)); });
  }
};

} // namespace SimulationTests
