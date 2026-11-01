#include "pch.h"

#include "BreachRun.h"
#include "FlatWorld.h"
#include "Query.h"
#include "Script.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The aim check (M3-B4; Hud.md §8): the attack cursor's reading from the shot's own arc. It and step 4
// both step through StepShot, so what it says a shot strikes is what the shot strikes.

namespace SimulationTests
{

namespace
{

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

/// The breach run's world, built and ticked once, before any shot.
struct Site
{
  Simulation::World::Desc desc;
  std::unique_ptr<Simulation::World> world;
  Simulation::BreachRun run;
};

Site BuildSite()
{
  Site site{.desc = Simulation::BreachRunDesc(BakedServerData(), FlatWorld::BUILD_ID), .world = nullptr, .run = {}};
  site.world = std::make_unique<Simulation::World>(site.desc);
  site.run = Simulation::BuildBreachRun(*site.world);
  site.world->Tick();
  return site;
}

const Site& BreachSite()
{
  static const Site SITE = BuildSite();
  return SITE;
}

/// The readings for _targets, asked together on a copy of the site and answered at the end of a tick.
std::vector<Protocol::AimReading> Aim(std::span<const Protocol::VoxelPosition> _targets)
{
  const Site& site = BreachSite();
  Simulation::World world = *site.world;
  for (std::size_t index = 0; index < _targets.size(); ++index)
  {
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::QueryRequest{.id = static_cast<std::uint32_t>(index),
                                            .query = Protocol::AimCheck{.engine = site.run.engine, .targetVoxels = _targets[index]}},
                     writer);
    world.Ask(Simulation::BreachRun::PLAYER, writer.Bytes());
  }
  world.Tick();
  std::vector<Protocol::AimReading> readings;
  for (const std::vector<std::byte>& bytes : world.TakeAnswers())
  {
    Core::ByteReader reader(bytes);
    readings.push_back(std::get<Protocol::AimReading>(Protocol::DecodeAnswer(reader).reading));
  }
  Assert::AreEqual(_targets.size(), readings.size());
  return readings;
}

/// The voxel a shot fired at _target from the site strikes, on a copy of the site.
std::optional<Protocol::VoxelPosition> Fire(Protocol::VoxelPosition _target)
{
  const Site& site = BreachSite();
  Simulation::World world = *site.world;
  const std::array<Simulation::ScriptedCommand, 1> script = {
    {{.tick = world.TickCount(),
      .player = Simulation::BreachRun::PLAYER,
      .command = Protocol::FireEngine{.engine = site.run.engine, .targetVoxels = _target}}}};
  for (int tick = 0; tick < 400; ++tick)
  {
    Simulation::RunScript(world, script, 1);
    if (!world.LastTickHits().empty())
    {
      return world.LastTickHits().front().voxel;
    }
  }
  return std::nullopt;
}

std::vector<Protocol::VoxelPosition> ScriptedAims()
{
  std::vector<Protocol::VoxelPosition> aims;
  for (const Simulation::ScriptedCommand& scripted : BreachSite().run.script)
  {
    if (const auto* fire = std::get_if<Protocol::FireEngine>(&scripted.command))
    {
      aims.push_back(fire->targetVoxels);
    }
  }
  return aims;
}

} // namespace

TEST_CLASS(AimCheckTests)
{
public:
  TEST_METHOD(ReadsTheBreachRunsAimsInRangeAVoxelAboveWhereTheyAim)
  {
    // The M2 report: descending at 45 degrees, a shot aimed at the foot of the face meets it a voxel up.
    const std::vector<Protocol::VoxelPosition> aims = ScriptedAims();
    Assert::AreEqual(std::size_t{5}, aims.size());
    const std::vector<Protocol::AimReading> readings = Aim(aims);
    for (std::size_t index = 0; index < aims.size(); ++index)
    {
      const Protocol::AimReading& reading = readings[index];
      Assert::IsTrue(reading.verdict == Protocol::AimVerdict::InRange);
      Assert::IsTrue(reading.strikes);
      Assert::IsTrue(reading.strikeVoxels == At(aims[index].xVoxels, aims[index].yVoxels + 1, aims[index].zVoxels));
      Assert::AreEqual(40, reading.distanceMeters);
    }
  }

  TEST_METHOD(ReadsTooCloseAndOutOfRangeWithTheirDistances)
  {
    const Simulation::SiegeEngine& engine = BreachSite().world->SiegeEngines().at(BreachSite().run.engine);
    const Protocol::VoxelPosition cell = engine.cellVoxels;
    const std::array<Protocol::VoxelPosition, 2> targets = {At(cell.xVoxels, cell.yVoxels - 1, cell.zVoxels + 10),
                                                            At(cell.xVoxels, cell.yVoxels - 1, cell.zVoxels + 60)};
    const std::vector<Protocol::AimReading> readings = Aim(targets);
    Assert::IsTrue(readings[0].verdict == Protocol::AimVerdict::TooClose);
    Assert::AreEqual(10, readings[0].distanceMeters);
    Assert::IsTrue(readings[1].verdict == Protocol::AimVerdict::OutOfRange);
    Assert::AreEqual(60, readings[1].distanceMeters);
  }

  TEST_METHOD(ReadsAnAimBehindTheWallBlockedOnItsFace)
  {
    const std::array<Protocol::VoxelPosition, 1> targets = {
      At(381, Simulation::BreachRun::GROUND_VOXELS - 1, Simulation::BreachRun::WALL_SOUTH_VOXELS + 4)};
    const Protocol::AimReading reading = Aim(targets).front();
    Assert::IsTrue(reading.verdict == Protocol::AimVerdict::Blocked);
    Assert::IsTrue(reading.strikes);
    Assert::AreEqual(Simulation::BreachRun::WALL_SOUTH_VOXELS, reading.strikeVoxels.zVoxels);
    Assert::AreEqual(381, reading.strikeVoxels.xVoxels);
  }

  TEST_METHOD(StrikesWhatTheShotStrikesWhenFired)
  {
    const std::vector<Protocol::VoxelPosition> aims = ScriptedAims();
    const std::vector<Protocol::AimReading> readings = Aim(aims);
    for (std::size_t index = 0; index < aims.size(); ++index)
    {
      const std::optional<Protocol::VoxelPosition> hit = Fire(aims[index]);
      Assert::IsTrue(hit.has_value() && *hit == readings[index].strikeVoxels);
    }
  }

  TEST_METHOD(AnotherPlayersEngineReadsOutOfRange)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId engine = world.AddSiegeEngine(2, Simulation::EngineKind::Trebuchet, At(5, FlatWorld::GROUND_VOXELS, 32));
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::QueryRequest{.id = 1, .query = Protocol::AimCheck{.engine = engine, .targetVoxels = At(45, 7, 32)}}, writer);
    world.Ask(1, writer.Bytes());
    world.Tick();
    const std::vector<std::vector<std::byte>> answers = world.TakeAnswers();
    Core::ByteReader reader(answers.front());
    const Protocol::AimReading reading = std::get<Protocol::AimReading>(Protocol::DecodeAnswer(reader).reading);
    Assert::IsTrue(reading.verdict == Protocol::AimVerdict::OutOfRange);
    Assert::IsFalse(reading.strikes);
  }
};

} // namespace SimulationTests
