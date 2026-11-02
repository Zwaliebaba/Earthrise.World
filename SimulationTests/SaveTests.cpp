#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"
#include "Script.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The save test of ADR-014, decision 4: run, save, load and run on, against the same run straight.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::uint8_t MASONRY = 6;

Protocol::VoxelPosition Cell(std::int32_t _x, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = _z};
}

/// The world the save tests run: two squads and an engine on the flat world.
Simulation::World Start()
{
  Simulation::World world(FlatWorld::Desc());
  const std::array<Protocol::VoxelPosition, 2> first = {Cell(3, 3), Cell(4, 3)};
  const std::array<Protocol::VoxelPosition, 2> second = {Cell(40, 40), Cell(41, 40)};
  static_cast<void>(world.AddSquad(PLAYER, Simulation::NavigationClass::Foot, first));
  static_cast<void>(world.AddSquad(PLAYER + 1, Simulation::NavigationClass::Cavalry, second, Simulation::UnitKind::Outrider));
  static_cast<void>(world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, Cell(10, 50)));
  return world;
}

std::vector<Simulation::ScriptedCommand> Script()
{
  std::vector<Simulation::ScriptedCommand> script;
  for (std::uint32_t tick = 0; tick < 100; tick += 9)
  {
    const auto at = static_cast<std::int32_t>(tick % 60);
    script.push_back({.tick = tick, .player = PLAYER, .command = Protocol::MoveSquad{.squad = 1, .goalVoxels = Cell(at, 5)}});
    script.push_back({.tick = tick + 4, .player = PLAYER + 1, .command = Protocol::MoveSquad{.squad = 4, .goalVoxels = Cell(5, at)}});
  }
  return script;
}

/// Runs the world on to _toTick: between ticks it edits a voxel every fifth tick, digging and
/// building, and before tick 37 a command arrives that the next tick applies.
void RunTo(Simulation::World& _world, std::uint32_t _toTick, bool _submitAt37 = true)
{
  const std::vector<Simulation::ScriptedCommand> script = Script();
  while (_world.TickCount() < _toTick)
  {
    const std::uint32_t tick = _world.TickCount();
    if (tick % 5 == 0)
    {
      const auto x = static_cast<std::int32_t>((tick * 7) % 60);
      _world.Voxels().Edit({.xVoxels = x, .yVoxels = FlatWorld::GROUND_VOXELS - 1 - static_cast<std::int32_t>(tick % 3), .zVoxels = 9}, 0);
      _world.Voxels().Edit(Cell(x, 20), MASONRY);
    }
    if (tick == 37 && _submitAt37)
    {
      Core::ByteWriter writer;
      Protocol::Encode(Protocol::MoveSquad{.squad = 4, .goalVoxels = Cell(61, 2)}, writer);
      _world.Submit(PLAYER + 1, writer.Bytes());
    }
    Simulation::RunScript(_world, script, 1);
  }
}

std::string Refusal(std::span<const std::byte> _save, const Simulation::World::Desc& _desc)
{
  try
  {
    static_cast<void>(Simulation::LoadSave(_save, _desc));
  }
  catch (const std::runtime_error& error)
  {
    return error.what();
  }
  return {};
}

} // namespace

TEST_CLASS(SaveTests)
{
public:
  TEST_METHOD(ContinuesAsTheStraightRunDoes)
  {
    Simulation::World straight = Start();
    RunTo(straight, 100);

    Simulation::World before = Start();
    RunTo(before, 37);
    // The command for tick 37 arrives before the save, so the save carries it as pending work.
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::MoveSquad{.squad = 4, .goalVoxels = Cell(61, 2)}, writer);
    before.Submit(PLAYER + 1, writer.Bytes());
    const std::vector<std::byte> save = Simulation::WriteSave(before);
    Simulation::World after = Simulation::LoadSave(save, FlatWorld::Desc());
    Assert::AreEqual(Simulation::StateHash(before), Simulation::StateHash(after));
    Assert::AreEqual(std::uint32_t{37}, after.TickCount());
    RunTo(after, 100, false);

    Assert::AreEqual(Simulation::StateHash(straight), Simulation::StateHash(after));
    Assert::AreEqual(straight.Voxels().VoxelHash(), after.Voxels().VoxelHash());
    Assert::IsTrue(after.Squads().at(4).goalVoxels == straight.Squads().at(4).goalVoxels);
  }

  TEST_METHOD(SavesALoadedSaveToTheSameBytes)
  {
    Simulation::World world = Start();
    RunTo(world, 52);
    const std::vector<std::byte> save = Simulation::WriteSave(world);
    const Simulation::World loaded = Simulation::LoadSave(save, FlatWorld::Desc());
    Assert::IsTrue(save == Simulation::WriteSave(loaded));
  }

  TEST_METHOD(LeavesOutAChunkBackToItsBase)
  {
    // Once a tick has searched the edits, a chunk dug and refilled saves as one never edited.
    Simulation::World unedited = Start();
    unedited.Tick();
    Simulation::World world = Start();
    world.Voxels().Edit(Cell(1, 1), MASONRY);
    world.Voxels().Edit(Cell(1, 1), 0);
    Assert::IsFalse(Simulation::WriteSave(unedited) == Simulation::WriteSave(world)); // the edits wait for step 5
    world.Tick();
    Assert::IsTrue(Simulation::WriteSave(unedited) == Simulation::WriteSave(world));
  }

  TEST_METHOD(RefusesASaveFromAnotherBuildDataGeneratorOrFormat)
  {
    Simulation::World world = Start();
    RunTo(world, 10);
    std::vector<std::byte> save = Simulation::WriteSave(world);

    Simulation::World::Desc otherBuild = FlatWorld::Desc();
    otherBuild.buildId += 1;
    Assert::IsTrue(Refusal(save, otherBuild).find("another build") != std::string::npos);

    Simulation::World::Desc otherGenerator = FlatWorld::Desc();
    otherGenerator.generatorVersion += 1;
    Assert::IsTrue(Refusal(save, otherGenerator).find("generator version") != std::string::npos);

    save[16] ^= std::byte{1}; // the server-data hash, after the magic, the version and the build id
    Assert::IsTrue(Refusal(save, FlatWorld::Desc()).find("server data") != std::string::npos);
    save[16] ^= std::byte{1};

    save[4] = std::byte{2}; // the format version
    Assert::IsTrue(Refusal(save, FlatWorld::Desc()).find("format version is 2") != std::string::npos);
    save[4] = std::byte{1};

    Assert::IsTrue(Refusal(save, FlatWorld::Desc()).empty());
    save.pop_back();
    Assert::IsFalse(Refusal(save, FlatWorld::Desc()).empty());
  }
};

} // namespace SimulationTests
