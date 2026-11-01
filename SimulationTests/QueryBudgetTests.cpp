#include "pch.h"

#include "FlatWorld.h"
#include "Query.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Queries within the tick's budget (M3-B3; ADR-009; ADR-020, decisions 5 and 6): answered in step 12
// on the tick's state, oldest first, within queryWorkPerTick units of work, an aim check costing 1 and
// a section's integrity 8; the 65th waiting drops the oldest. And Quiet, what the breach lab's
// fast-forward stops on.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;

constexpr Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z) noexcept
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

std::vector<std::byte> Bytes(std::uint32_t _id, const Protocol::Query& _query)
{
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::QueryRequest{.id = _id, .query = _query}, writer);
  return writer.TakeBytes();
}

std::vector<std::byte> Bytes(const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  return writer.TakeBytes();
}

// constexpr, so that building them at static initialization cannot throw.
constexpr Protocol::Query AIM = Protocol::AimCheck{.engine = 1, .targetVoxels = At(30, GROUND, 30)}; // costs 1
constexpr Protocol::Query INTEGRITY = Protocol::SectionIntegrity{.structure = 1};                    // costs 8

std::vector<Protocol::Answer> Answers(Simulation::World& _world)
{
  std::vector<Protocol::Answer> answers;
  for (const std::vector<std::byte>& bytes : _world.TakeAnswers())
  {
    Core::ByteReader reader(bytes);
    answers.push_back(Protocol::DecodeAnswer(reader));
    Assert::IsTrue(reader.AtEnd());
  }
  return answers;
}

/// Ticks until the world is quiet, at most _ticks; returns how many it took, or _ticks + 1.
std::uint32_t TicksUntilQuiet(Simulation::World& _world, std::uint32_t _ticks)
{
  for (std::uint32_t tick = 1; tick <= _ticks; ++tick)
  {
    _world.Tick();
    if (_world.Quiet())
    {
      return tick;
    }
  }
  return _ticks + 1;
}

} // namespace

TEST_CLASS(QueryBudgetTests)
{
public:
  TEST_METHOD(AnswersAQueryAtTheEndOfTheTickItWasAskedBefore)
  {
    Simulation::World world(FlatWorld::Desc());
    world.Tick();
    world.Ask(PLAYER, Bytes(41, AIM));
    Assert::IsTrue(world.TakeAnswers().empty());
    world.Tick();
    const std::vector<Protocol::Answer> answers = Answers(world);
    Assert::AreEqual(std::size_t{1}, answers.size());
    Assert::AreEqual(41U, answers.front().queryId);
    Assert::AreEqual(1U, answers.front().tick);
    Assert::IsTrue(std::holds_alternative<Protocol::AimReading>(answers.front().reading));
    world.Tick();
    Assert::IsTrue(world.TakeAnswers().empty());
  }

  TEST_METHOD(SpendsTheBudgetOldestFirst)
  {
    // 16 units a tick: 17 aim checks take two ticks, and come back in the order they were asked.
    Assert::AreEqual(16, BakedServerData().Tuning().queryWorkPerTick);
    Simulation::World world(FlatWorld::Desc());
    for (std::uint32_t id = 1; id <= 17; ++id)
    {
      world.Ask(PLAYER, Bytes(id, AIM));
    }
    world.Tick();
    const std::vector<Protocol::Answer> first = Answers(world);
    Assert::AreEqual(std::size_t{16}, first.size());
    for (std::uint32_t index = 0; index < 16; ++index)
    {
      Assert::AreEqual(index + 1, first[index].queryId);
    }
    world.Tick();
    const std::vector<Protocol::Answer> second = Answers(world);
    Assert::AreEqual(std::size_t{1}, second.size());
    Assert::AreEqual(17U, second.front().queryId);
    Assert::AreEqual(1U, second.front().tick);
  }

  TEST_METHOD(AQueryThatDoesNotFitWaitsForTheNextTick)
  {
    // 1 + 8 leaves 7, so the second integrity reading waits, and is answered next tick, not dropped.
    Simulation::World world(FlatWorld::Desc());
    world.Ask(PLAYER, Bytes(1, AIM));
    world.Ask(PLAYER, Bytes(2, INTEGRITY));
    world.Ask(PLAYER, Bytes(3, INTEGRITY));
    world.Ask(PLAYER, Bytes(4, AIM));
    world.Tick();
    const std::vector<Protocol::Answer> first = Answers(world);
    Assert::AreEqual(std::size_t{2}, first.size());
    Assert::AreEqual(2U, first.back().queryId);
    world.Tick();
    const std::vector<Protocol::Answer> second = Answers(world);
    Assert::AreEqual(std::size_t{2}, second.size());
    Assert::AreEqual(3U, second.front().queryId);
    Assert::AreEqual(4U, second.back().queryId);
  }

  TEST_METHOD(DropsTheOldestPastSixtyFourWaiting)
  {
    Simulation::World world(FlatWorld::Desc());
    for (std::uint32_t id = 1; id <= 65; ++id)
    {
      world.Ask(PLAYER, Bytes(id, AIM));
    }
    std::vector<std::uint32_t> answered;
    for (int tick = 0; tick < 6; ++tick)
    {
      world.Tick();
      for (const Protocol::Answer& answer : Answers(world))
      {
        answered.push_back(answer.queryId);
      }
    }
    Assert::AreEqual(std::size_t{64}, answered.size());
    Assert::AreEqual(2U, answered.front());
    Assert::AreEqual(65U, answered.back());
  }

  TEST_METHOD(AMalformedQueryThrowsInStepTwelve)
  {
    Simulation::World world(FlatWorld::Desc());
    std::vector<std::byte> unknown = Bytes(1, AIM);
    unknown[4] = std::byte{9};
    world.Ask(PLAYER, unknown);
    Assert::ExpectException<std::invalid_argument>([&world] { world.Tick(); });
    Simulation::World other(FlatWorld::Desc());
    std::vector<std::byte> truncated = Bytes(1, AIM);
    truncated.pop_back();
    other.Ask(PLAYER, truncated);
    Assert::ExpectException<std::out_of_range>([&other] { other.Tick(); });
  }

  TEST_METHOD(QueriesChangeNothing)
  {
    Simulation::World asked(FlatWorld::Desc());
    Simulation::World plain(FlatWorld::Desc());
    asked.Ask(PLAYER, Bytes(1, INTEGRITY));
    asked.Tick();
    plain.Tick();
    Assert::AreEqual(Simulation::StateHash(plain), Simulation::StateHash(asked));
  }

  TEST_METHOD(IsQuietOnlyWhenNothingIsInFlight)
  {
    // A shot flies, its earth crater slumps closed, and the world goes quiet again.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Trebuchet, At(5, GROUND, 32));
    world.Tick();
    Assert::IsTrue(world.Quiet());
    world.Submit(PLAYER, Bytes(Protocol::FireEngine{.engine = engine, .targetVoxels = At(45, GROUND - 1, 32)}));
    world.Tick();
    Assert::IsFalse(world.Quiet());
    Assert::IsTrue(TicksUntilQuiet(world, 400) <= 400);

    // A T3 undercut 5 wide warns, falls and its rubble settles: loud throughout, quiet after.
    world.AddStructure(2, "T3", At(20, GROUND, 10), 0, "W1");
    world.Tick();
    Assert::IsTrue(world.Quiet());
    for (std::int32_t y = GROUND; y <= GROUND + 1; ++y)
    {
      for (std::int32_t z = 10; z <= 11; ++z)
      {
        for (std::int32_t x = 21; x <= 25; ++x)
        {
          world.Voxels().Edit(At(x, y, z), 0);
        }
      }
    }
    world.Tick();
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    Assert::IsFalse(world.Quiet());
    Assert::IsTrue(TicksUntilQuiet(world, 400) <= 400);
    Assert::IsTrue(world.Collapses().empty());

    // A squad walks 14 m, and stands still at the end.
    const std::array<Protocol::VoxelPosition, 2> cells = {At(4, GROUND, 50), At(5, GROUND, 50)};
    const Protocol::EntityId squad = world.AddSquad(PLAYER, Simulation::NavigationClass::Foot, cells);
    world.Submit(PLAYER, Bytes(Protocol::MoveSquad{.squad = squad, .goalVoxels = At(19, GROUND, 50)}));
    world.Tick();
    world.Tick();
    Assert::IsFalse(world.Quiet());
    Assert::IsTrue(TicksUntilQuiet(world, 400) <= 400);
    Assert::AreEqual(19, world.Units().at(world.Squads().at(squad).units.front()).cellVoxels.xVoxels);
  }
};

} // namespace SimulationTests
