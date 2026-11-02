#include "pch.h"

#include "Arrow.h"
#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Arrows (M4-C4; ADR-023, decisions 5 and 6): a ranged figure looses at its target when it is within its range plus 5 m a
// step of elevation and the straight line to it meets nothing solid; the arrow flies that line at 2 m a tick, stops at
// the first solid voxel, passes every unit, and hits its target only if the target stands within a cell of where it was
// aimed. An archer's 4 pierce is 4 against a spearman level, floor(4 x 130 / 100) = 5 two steps down, and
// floor(4 x 70 / 100) = 2 two steps up.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t SHOOTER = 1;
constexpr std::uint8_t TARGETS = 2;
constexpr std::uint8_t COMMON_ROCK = 3;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;

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

/// A squad of one _kind for _owner in _cell, told to hold, so that it neither closes nor walks away.
Protocol::EntityId Holding(Simulation::World& _world, std::uint8_t _owner, Simulation::UnitKind _kind, Protocol::VoxelPosition _cell)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {_cell};
  const Protocol::EntityId squad = _world.AddSquad(_owner, _kind, cells);
  Order(_world, _owner, Protocol::HoldSquad{.squad = squad});
  return squad;
}

const Simulation::Unit& Figure(const Simulation::World& _world, Protocol::EntityId _squad)
{
  return _world.Units().at(_world.Squads().at(_squad).units.front());
}

std::size_t Arrows(const Simulation::World& _world)
{
  return static_cast<std::size_t>(
    std::ranges::count_if(_world.Shots(), [](const auto& _shot) { return _shot.second.kind == Protocol::ShotKind::Arrow; }));
}

/// The hit points an archer in _from takes from a spearman in _to over the 1.5 s an arrow takes to fly 30 m, and whether
/// it loosed at all.
struct Volley
{
  bool loosed = false;
  std::int32_t damage = 0;
};

Volley Shoot(Simulation::World& _world, Protocol::VoxelPosition _from, Protocol::VoxelPosition _to)
{
  Holding(_world, SHOOTER, Simulation::UnitKind::Archer, _from);
  const Protocol::EntityId target = Holding(_world, TARGETS, Simulation::UnitKind::Spearman, _to);
  const std::int32_t whole = _world.Data().Stats(Simulation::UnitKind::Spearman).hitPoints;
  Volley volley;
  // The first arrow, and no second: the reload is 40 ticks.
  for (std::uint32_t tick = 0; tick < 35; ++tick)
  {
    _world.Tick();
    volley.loosed = volley.loosed || Arrows(_world) > 0;
  }
  volley.damage = whole - Figure(_world, target).hitPoints;
  return volley;
}

} // namespace

TEST_CLASS(ArrowTests)
{
public:
  TEST_METHOD(ReachesThirtyMetersOnLevelGroundAndNotThirtyOne)
  {
    Simulation::World level(FlatWorld::Desc());
    const Volley thirty = Shoot(level, At(10, GROUND, 30), At(40, GROUND, 30));
    Assert::IsTrue(thirty.loosed);
    Assert::AreEqual(4, thirty.damage);
    Simulation::World beyond(FlatWorld::Desc());
    const Volley thirtyOne = Shoot(beyond, At(10, GROUND, 30), At(41, GROUND, 30));
    Assert::IsFalse(thirtyOne.loosed);
    Assert::AreEqual(0, thirtyOne.damage);
  }

  TEST_METHOD(ReachesFortyMetersFromSixMetersAboveWithTheTwoStepDamage)
  {
    // On a platform of rock 6 m high, two steps above its target: 30 m and two bands of 5.
    const auto shootDown = [](std::int32_t _toX)
    {
      Simulation::World world(FlatWorld::Desc());
      Fill(world, At(9, GROUND, 29), At(11, GROUND + 5, 31), COMMON_ROCK);
      world.Tick();
      return Shoot(world, At(10, GROUND + 6, 30), At(_toX, GROUND, 30));
    };
    const Volley forty = shootDown(50);
    Assert::IsTrue(forty.loosed);
    Assert::AreEqual(5, forty.damage);
    Assert::IsFalse(shootDown(51).loosed);
  }

  TEST_METHOD(DealsTheUphillDamageFromSixMetersBelow)
  {
    // The target on a platform of rock, two steps up: 30 m less two bands, 20, and the arrow's damage cut to 2.
    const auto shootUp = [](std::int32_t _toX)
    {
      Simulation::World world(FlatWorld::Desc());
      Fill(world, At(_toX - 1, GROUND, 29), At(_toX + 1, GROUND + 5, 31), COMMON_ROCK);
      world.Tick();
      return Shoot(world, At(10, GROUND, 30), At(_toX, GROUND + 6, 30));
    };
    const Volley twenty = shootUp(30);
    Assert::IsTrue(twenty.loosed);
    Assert::AreEqual(2, twenty.damage);
    Assert::IsFalse(shootUp(31).loosed);
  }

  TEST_METHOD(StopsAtAWallRisingBetweenItAndItsTargetDuringItsFlight)
  {
    // A wall raised across the arrow's line after it is loosed, as a gate shutting or a section falling would put one
    // between the two: the arrow stops at the wall's face, the target stands behind it unhurt.
    Simulation::World world(FlatWorld::Desc());
    Holding(world, SHOOTER, Simulation::UnitKind::Archer, At(10, GROUND, 30));
    const Protocol::EntityId target = Holding(world, TARGETS, Simulation::UnitKind::Spearman, At(40, GROUND, 30));
    for (std::uint32_t tick = 0; tick < 10 && Arrows(world) == 0; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(std::size_t{1}, Arrows(world));
    Fill(world, At(30, GROUND, 28), At(31, GROUND + 3, 32), MASONRY);
    Core::Fixed last{};
    for (std::uint32_t tick = 0; tick < 30 && Arrows(world) > 0; ++tick)
    {
      last = Simulation::ArrowPositionAt(std::get<Simulation::ArrowLine>(world.Shots().begin()->second.flight),
                                         world.Shots().begin()->second.ticksFlown)
               .xMeters;
      world.Tick();
    }
    Assert::AreEqual(std::size_t{0}, Arrows(world));
    Assert::IsTrue(last.FloorWhole() >= 27 && last.FloorWhole() < 30);
    Assert::AreEqual(world.Data().Stats(Simulation::UnitKind::Spearman).hitPoints, Figure(world, target).hitPoints);
  }

  TEST_METHOD(LoosesNothingWithoutALineAndWalksUntilItHasOne)
  {
    // A wall 5 m high from z 25 to z 35 stands between an archer and the spearman it is ordered to attack. It looses
    // nothing while the wall is in the way, walks its squad's path round the wall's end, and looses once its line clears.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, At(20, GROUND, 25), At(21, GROUND + 4, 35), MASONRY);
    world.Tick();
    const std::array<Protocol::VoxelPosition, 1> cells = {At(10, GROUND, 30)};
    const Protocol::EntityId archer = world.AddSquad(SHOOTER, Simulation::UnitKind::Archer, cells);
    const Protocol::EntityId target = Holding(world, TARGETS, Simulation::UnitKind::Spearman, At(30, GROUND, 30));
    Order(world, SHOOTER, Protocol::AttackTarget{.squad = archer, .target = target});
    std::optional<Protocol::VoxelPosition> loosedFrom;
    for (std::uint32_t tick = 0; tick < 60 * 20 && !loosedFrom.has_value(); ++tick)
    {
      world.Tick();
      if (Arrows(world) > 0)
      {
        loosedFrom = Figure(world, archer).cellVoxels;
      }
    }
    Assert::IsTrue(loosedFrom.has_value());
    const Protocol::VoxelPosition from = loosedFrom.value_or(At(10, GROUND, 30));
    Assert::IsTrue(from != At(10, GROUND, 30));
    const Simulation::ArrowLine line = Simulation::AimArrow(from, Figure(world, target).cellVoxels, world.Data().Tuning());
    Assert::IsFalse(Simulation::FirstSolid(line.start, line.end, world.Voxels()).has_value());
  }

  TEST_METHOD(PassesAUnitInItsWayThatIsNotItsTarget)
  {
    // A spearman of the archer's own stands on the line, halfway: the arrow flies through it to the target.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId friendly = Holding(world, SHOOTER, Simulation::UnitKind::Spearman, At(25, GROUND, 30));
    const Volley volley = Shoot(world, At(10, GROUND, 30), At(40, GROUND, 30));
    Assert::IsTrue(volley.loosed);
    Assert::AreEqual(4, volley.damage);
    Assert::AreEqual(world.Data().Stats(Simulation::UnitKind::Spearman).hitPoints, Figure(world, friendly).hitPoints);
  }

  TEST_METHOD(MissesATargetThatLeavesItsAimPointByMoreThanACell)
  {
    // An outrider 30 m off rides north at 2.8 m/s as the arrow is loosed at it: in the arrow's 15 ticks it goes 2 m, two
    // cells from where the arrow was aimed, and the arrow falls harmlessly. Beyond 30 m by then, it draws no other.
    Simulation::World world(FlatWorld::Desc());
    Holding(world, SHOOTER, Simulation::UnitKind::Archer, At(10, GROUND, 30));
    const std::array<Protocol::VoxelPosition, 1> cells = {At(40, GROUND, 30)};
    const Protocol::EntityId outrider = world.AddSquad(TARGETS, Simulation::UnitKind::Outrider, cells);
    Order(world, TARGETS, Protocol::MoveSquad{.squad = outrider, .goalVoxels = At(40, GROUND, 60)});
    bool loosed = false;
    for (std::uint32_t tick = 0; tick < 60; ++tick)
    {
      world.Tick();
      loosed = loosed || Arrows(world) > 0;
    }
    Assert::IsTrue(loosed);
    Assert::AreEqual(std::size_t{0}, Arrows(world));
    Assert::AreEqual(world.Data().Stats(Simulation::UnitKind::Outrider).hitPoints, Figure(world, outrider).hitPoints);
  }

  TEST_METHOD(ReadsTheSquadsAimAsItsArrowWouldFly)
  {
    // SquadAim from the archer's cell: 30 m off in range, 31 out of it, and behind a wall blocked at the wall's face.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, At(20, GROUND, 40), At(20, GROUND + 3, 44), MASONRY);
    world.Tick();
    const std::array<Protocol::VoxelPosition, 1> cells = {At(10, GROUND, 42)};
    const Protocol::EntityId archer = world.AddSquad(SHOOTER, Simulation::UnitKind::Archer, cells);
    const std::array<Protocol::VoxelPosition, 3> targets = {At(10, GROUND, 12), At(10, GROUND, 11), At(30, GROUND, 42)};
    for (std::size_t query = 0; query < targets.size(); ++query)
    {
      Core::ByteWriter writer;
      Protocol::Encode(Protocol::QueryRequest{.id = static_cast<std::uint32_t>(query + 1),
                                              .query = Protocol::SquadAim{.squad = archer, .targetVoxels = targets.at(query)}},
                       writer);
      world.Ask(SHOOTER, writer.Bytes());
    }
    world.Tick();
    std::map<std::uint32_t, Protocol::AimReading> readings;
    for (const std::vector<std::byte>& bytes : world.TakeAnswers())
    {
      Core::ByteReader reader(bytes);
      const Protocol::Answer answer = Protocol::DecodeAnswer(reader);
      readings.emplace(answer.queryId, std::get<Protocol::AimReading>(answer.reading));
    }
    Assert::AreEqual(std::size_t{3}, readings.size());
    Assert::IsTrue(readings.at(1).verdict == Protocol::AimVerdict::InRange);
    Assert::AreEqual(30, readings.at(1).distanceMeters);
    Assert::IsTrue(readings.at(2).verdict == Protocol::AimVerdict::OutOfRange);
    Assert::IsTrue(readings.at(3).verdict == Protocol::AimVerdict::Blocked);
    Assert::IsTrue(readings.at(3).strikes);
    Assert::AreEqual(20, readings.at(3).strikeVoxels.xVoxels);
  }
};

} // namespace SimulationTests
