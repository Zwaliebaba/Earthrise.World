#include "pch.h"

#include "FlatWorld.h"
#include "Script.h"
#include "World.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The world and its tick (ADR-013; MvpPlan.md, *One simulation tick*).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;

std::vector<std::byte> Bytes(const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  return writer.TakeBytes();
}

Protocol::VoxelPosition Cell(std::int32_t _x, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = _z};
}

Protocol::EntityId AddFootSquad(Simulation::World& _world, std::uint8_t _owner)
{
  const std::array<Protocol::VoxelPosition, 3> cells = {Cell(4, 4), Cell(5, 4), Cell(6, 4)};
  return _world.AddSquad(_owner, Simulation::NavigationClass::Foot, cells);
}

} // namespace

TEST_CLASS(WorldTests)
{
public:
  TEST_METHOD(AppliesACommandInTheNextTicksFirstStep)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId squad = AddFootSquad(world, PLAYER);
    world.Tick();
    world.Submit(PLAYER, Bytes(Protocol::MoveSquad{.squad = squad, .goalVoxels = Cell(20, 30)}));
    Assert::IsFalse(world.Squads().at(squad).hasGoal);
    Assert::AreEqual(std::uint32_t{1}, world.TickCount());
    world.Tick();
    Assert::IsTrue(world.Squads().at(squad).hasGoal);
    Assert::IsTrue(world.Squads().at(squad).goalVoxels == Cell(20, 30));
    Assert::AreEqual(std::uint32_t{2}, world.TickCount());
  }

  TEST_METHOD(RefusesAnOrderForAnotherPlayersSquad)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId squad = AddFootSquad(world, PLAYER);
    world.Submit(PLAYER + 1, Bytes(Protocol::MoveSquad{.squad = squad, .goalVoxels = Cell(20, 30)}));
    world.Submit(PLAYER, Bytes(Protocol::MoveSquad{.squad = squad + 100, .goalVoxels = Cell(20, 30)}));
    world.Tick();
    Assert::IsFalse(world.Squads().at(squad).hasGoal);
  }

  TEST_METHOD(GivesIdsFromOneAndNeverReusesThem)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId squad = AddFootSquad(world, PLAYER);
    Assert::AreEqual(Protocol::EntityId{1}, squad);
    Assert::IsTrue(world.Squads().at(1).units == std::vector<Protocol::EntityId>{2, 3, 4});
    world.RemoveSquad(squad);
    Assert::IsTrue(world.Squads().empty());
    Assert::IsTrue(world.Units().empty());
    Assert::AreEqual(Protocol::EntityId{5}, world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, Cell(10, 10)));
    Assert::AreEqual(Protocol::EntityId{6}, world.NewEntityId());
  }

  TEST_METHOD(TwoWorldsGivenTheSameScriptAgree)
  {
    Simulation::World first(FlatWorld::Desc());
    Simulation::World second(FlatWorld::Desc());
    std::vector<Simulation::ScriptedCommand> script;
    for (Simulation::World* world : {&first, &second})
    {
      AddFootSquad(*world, PLAYER);
      AddFootSquad(*world, PLAYER + 1);
    }
    for (std::uint32_t tick = 3; tick < 100; tick += 7)
    {
      const auto x = static_cast<std::int32_t>(tick % 50);
      script.push_back({.tick = tick, .player = PLAYER, .command = Protocol::MoveSquad{.squad = 1, .goalVoxels = Cell(x, 7)}});
      script.push_back({.tick = tick, .player = PLAYER + 1, .command = Protocol::MoveSquad{.squad = 5, .goalVoxels = Cell(7, x)}});
    }
    first.Voxels().Edit(Cell(9, 9), FlatWorld::EARTH);
    second.Voxels().Edit(Cell(9, 9), FlatWorld::EARTH);
    Simulation::RunScript(first, script, 100);
    Simulation::RunScript(second, script, 100);
    Assert::AreEqual(std::uint32_t{100}, first.TickCount());
    Assert::AreEqual(first.Voxels().VoxelHash(), second.Voxels().VoxelHash());
    for (const auto& [id, squad] : first.Squads())
    {
      Assert::IsTrue(squad.hasGoal);
      Assert::IsTrue(squad.goalVoxels == second.Squads().at(id).goalVoxels);
    }
    Assert::IsTrue(first.Squads().at(1).goalVoxels == Cell(94 % 50, 7));
  }

  TEST_METHOD(ThrowsOnAMalformedCommandInTheFirstStep)
  {
    Simulation::World world(FlatWorld::Desc());
    std::vector<std::byte> unknown = Bytes(Protocol::FireEngine{.engine = 1, .targetVoxels = {}});
    unknown[0] = std::byte{9};
    world.Submit(PLAYER, unknown);
    Assert::ExpectException<std::invalid_argument>([&world] { world.Tick(); });

    std::vector<std::byte> version = Bytes(Protocol::FireEngine{.engine = 1, .targetVoxels = {}});
    version[2] = std::byte{2};
    world.Submit(PLAYER, version);
    Assert::ExpectException<std::invalid_argument>([&world] { world.Tick(); });

    std::vector<std::byte> truncated = Bytes(Protocol::FireEngine{.engine = 1, .targetVoxels = {}});
    truncated.pop_back();
    world.Submit(PLAYER, truncated);
    Assert::ExpectException<std::out_of_range>([&world] { world.Tick(); });

    std::vector<std::byte> longer = Bytes(Protocol::FireEngine{.engine = 1, .targetVoxels = {}});
    longer.push_back(std::byte{0});
    world.Submit(PLAYER, longer);
    Assert::ExpectException<std::invalid_argument>([&world] { world.Tick(); });
  }

  TEST_METHOD(GeneratesEveryChunk)
  {
    std::vector<Protocol::ChunkCoordinate> generated;
    Simulation::World::Desc desc = FlatWorld::Desc(3, 2);
    const Simulation::GenerateChunk flat = desc.generate;
    desc.generate = [&generated, flat](Protocol::ChunkCoordinate _coordinate, Protocol::Chunk& _chunk)
    {
      generated.push_back(_coordinate);
      flat(_coordinate, _chunk);
    };
    const Simulation::World world(desc);
    Assert::AreEqual(std::size_t{12}, generated.size());
    // The ground chunks hold voxels; the sky's are empty and so are not stored.
    Assert::AreEqual(std::size_t{6}, world.Voxels().Store().Keys().size());
    Assert::AreEqual(FlatWorld::EARTH, world.Voxels().At({.xVoxels = 95, .yVoxels = FlatWorld::GROUND_VOXELS - 1, .zVoxels = 63}));
    Assert::IsTrue(world.Voxels().EditedChunks().empty());
  }

  TEST_METHOD(RecordsWhatTheTickApplied)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId squad = AddFootSquad(world, PLAYER);
    const std::vector<std::byte> move = Bytes(Protocol::MoveSquad{.squad = squad, .goalVoxels = Cell(1, 2)});
    const std::vector<std::byte> fire = Bytes(Protocol::FireEngine{.engine = 99, .targetVoxels = Cell(3, 4)});
    world.Submit(PLAYER, move);
    world.Submit(PLAYER + 1, fire);
    world.Tick();
    Assert::AreEqual(std::size_t{2}, world.LastTickCommands().size());
    Assert::AreEqual(PLAYER, world.LastTickCommands()[0].player);
    Assert::IsTrue(world.LastTickCommands()[0].bytes == move);
    Assert::IsTrue(world.LastTickCommands()[1].bytes == fire);
    world.Tick();
    Assert::IsTrue(world.LastTickCommands().empty());
  }
};

} // namespace SimulationTests
