#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Gates (M4-D2; ADR-024, decision 6; MvpPlan.md, M4, input 13): a gate is a flag on its structure. Open, navigation reads
// its voxels as air for every unit, while shots still meet them; its owner opens or shuts it while it works, and one that
// stops working shuts. T5 stands in a masonry wall across the map, its outer face north and its 3 x 4 m gate at the
// inner end of its passage, x 30 to 32 at z 28.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 64;

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

/// T5 in its wall, built and ticked once, in a match of two players so that each has its update; returns the gatehouse.
Protocol::EntityId Gatehouse(Simulation::World& _world)
{
  _world.AddPlayer(
    {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
  _world.AddPlayer(
    {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
  Fill(_world, At(0, GROUND, 28), At(26, GROUND + 5, 29), MASONRY);
  Fill(_world, At(36, GROUND, 28), At(WIDTH - 1, GROUND + 5, 29), MASONRY);
  const Protocol::EntityId gatehouse = _world.AddStructure(DEFENDER, "T5", At(27, GROUND, 28), 0, "GH");
  _world.Tick();
  return gatehouse;
}

/// Whether _class can reach (_x, 10), south of the wall, from (_x, 50), north of it.
bool Through(const Simulation::World& _world, Simulation::NavigationClass _class, std::int32_t _x)
{
  const std::int64_t start = (std::int64_t{50} * WIDTH) + _x;
  const std::int64_t goal = (std::int64_t{10} * WIDTH) + _x;
  return _world.Navigation().NearestReachable(start, goal, _class) == goal;
}

bool Refused(const Simulation::World& _world, std::uint8_t _player, Protocol::Refusal _reason)
{
  return std::ranges::any_of(_world.State(_player).refusals,
                             [_reason](const Protocol::RefusalView& _view) { return _view.reason == _reason; });
}

std::size_t GateStanding(const Simulation::World& _world)
{
  std::size_t standing = 0;
  for (std::int32_t y = GROUND; y < GROUND + 4; ++y)
  {
    for (std::int32_t x = 30; x <= 32; ++x)
    {
      standing += _world.Voxels().At(At(x, y, 28)) != 0 ? 1 : 0;
    }
  }
  return standing;
}

} // namespace

TEST_CLASS(GateTests)
{
public:
  TEST_METHOD(AShutGateBlocksFootAndMachinesAndAnOpenOnePassesBoth)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId gatehouse = Gatehouse(world);
    Assert::IsFalse(Through(world, Simulation::NavigationClass::Foot, 31));
    Assert::IsFalse(Through(world, Simulation::NavigationClass::Machines, 30));
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = true});
    world.Tick();
    Assert::IsTrue(world.Structures().at(gatehouse).gateOpen);
    Assert::IsTrue(Through(world, Simulation::NavigationClass::Foot, 31));
    Assert::IsTrue(Through(world, Simulation::NavigationClass::Machines, 30));
    Assert::AreEqual(std::size_t{12}, GateStanding(world));
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = false});
    world.Tick();
    Assert::IsFalse(Through(world, Simulation::NavigationClass::Foot, 31));
  }

  TEST_METHOD(AnOpenGatesViewListsItsVoxelsAndAShutOnesNone)
  {
    // The client leaves an open gate's voxels out of the world it draws and draws G1 over them (M4-H1), so the update
    // names them: T5's twelve, x 30 to 32 and four high at z 28, and none while it is shut.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId gatehouse = Gatehouse(world);
    const auto view = [&world, gatehouse]()
    {
      const std::vector<Protocol::StructureView>& structures = world.State(DEFENDER).structures;
      return *std::ranges::find(structures, gatehouse, &Protocol::StructureView::id);
    };
    Assert::IsTrue(view().gateVoxels.empty());
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = true});
    world.Tick();
    std::vector<Protocol::VoxelPosition> expected;
    expected.reserve(12);
    for (std::int32_t y = GROUND; y < GROUND + 4; ++y)
    {
      for (std::int32_t x = 30; x <= 32; ++x)
      {
        expected.push_back(At(x, y, 28));
      }
    }
    std::vector<Protocol::VoxelPosition> listed = view().gateVoxels;
    const auto order = [](const Protocol::VoxelPosition& _a, const Protocol::VoxelPosition& _b)
    {
      return _a.yVoxels != _b.yVoxels ? _a.yVoxels < _b.yVoxels
                                      : (_a.zVoxels != _b.zVoxels ? _a.zVoxels < _b.zVoxels : _a.xVoxels < _b.xVoxels);
    };
    std::ranges::sort(listed, order);
    Assert::IsTrue(listed == expected);
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = false});
    world.Tick();
    Assert::IsTrue(view().gateVoxels.empty());
  }

  TEST_METHOD(AShotAtAnOpenGateStrikesItsVoxels)
  {
    // A trebuchet in the courtyard, 28 m south of the gate's inner face, aims at it: the stone strikes the gate, open or
    // not, and its crater takes timber.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId gatehouse = Gatehouse(world);
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = true});
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(30, GROUND, 0));
    world.Tick();
    Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(31, GROUND + 1, 28)});
    std::optional<Protocol::VoxelPosition> struck;
    for (std::uint32_t tick = 0; tick < 200 && !struck.has_value(); ++tick)
    {
      world.Tick();
      if (!world.LastTickHits().empty())
      {
        struck = world.LastTickHits().front().voxel;
      }
    }
    Assert::IsTrue(struck.has_value());
    const Protocol::VoxelPosition voxel = struck.value_or(At(0, 0, 0));
    Assert::AreEqual(28, voxel.zVoxels);
    Assert::IsTrue(voxel.xVoxels >= 30 && voxel.xVoxels <= 32 && voxel.yVoxels >= GROUND && voxel.yVoxels < GROUND + 4);
    Assert::IsTrue(GateStanding(world) < 12);
  }

  TEST_METHOD(OpeningRebuildsItsTilesOnTheNextStepEightAndWaitingSquadsSearchAgain)
  {
    // Spearmen of the attacker's, ordered through the shut gate, wait as near as they can, short of their goal. The tick
    // the defender opens it, step 8 rebuilds its tiles and the squad searches again; its next path goes through.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId gatehouse = Gatehouse(world);
    const std::array<Protocol::VoxelPosition, 1> cells = {At(31, GROUND, 50)};
    const Protocol::EntityId squad = world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, cells);
    Order(world, ATTACKER, Protocol::MoveSquad{.squad = squad, .goalVoxels = At(31, GROUND, 10)});
    for (std::uint32_t tick = 0; tick < 20; ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(world.Squads().at(squad).shortOfGoal);
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = true});
    world.Tick();
    Assert::IsTrue(std::ranges::any_of(world.Searches(), [squad](const auto& _search) { return _search.first.second == squad; }));
    for (std::uint32_t tick = 0; tick < 5 && !world.Searches().empty(); ++tick)
    {
      world.Tick();
    }
    Assert::IsFalse(world.Squads().at(squad).shortOfGoal);
  }

  TEST_METHOD(ABrokenGateCannotBeOpenedAndAnOpenOneThatBreaksShuts)
  {
    // T5's gate works while half its 12 voxels stand (input 10): six gone leaves 50 %, seven 41 %.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId gatehouse = Gatehouse(world);
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = true});
    world.Tick();
    Assert::IsTrue(world.Structures().at(gatehouse).gateOpen);
    Fill(world, At(30, GROUND + 2, 28), At(32, GROUND + 3, 28), 0);
    world.Voxels().Edit(At(30, GROUND + 1, 28), 0);
    world.Tick();
    Assert::IsFalse(world.Structures().at(gatehouse).working);
    Assert::IsFalse(world.Structures().at(gatehouse).gateOpen);
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = true});
    world.Tick();
    Assert::IsTrue(Refused(world, DEFENDER, Protocol::Refusal::NotWorking));
    Assert::IsFalse(world.Structures().at(gatehouse).gateOpen);
  }

  TEST_METHOD(OnlyItsOwnerSwingsAGateAndAnOpenOneSurvivesASave)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId gatehouse = Gatehouse(world);
    Order(world, ATTACKER, Protocol::SetGate{.structure = gatehouse, .open = true});
    world.Tick();
    Assert::IsTrue(Refused(world, ATTACKER, Protocol::Refusal::NotOwned));
    Order(world, DEFENDER, Protocol::SetGate{.structure = gatehouse, .open = true});
    world.Tick();
    const Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(world), FlatWorld::Desc());
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(loaded));
    Assert::IsTrue(Through(loaded, Simulation::NavigationClass::Foot, 31));
  }

  TEST_METHOD(ARamReachesT5sGateDownItsPassage)
  {
    // ADR-015's 3 by 4 m: the ram, 3 cells wide and needing 4 m of headroom, drives up the passage to stand at the gate.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId gatehouse = Gatehouse(world);
    const Protocol::EntityId ram =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(30, GROUND, 45), 0, Simulation::EngineState::Packed);
    Order(world, ATTACKER, Protocol::AttackTarget{.squad = world.SiegeEngines().at(ram).squad, .target = gatehouse});
    for (std::uint32_t tick = 0; tick < 60 * 20 && world.SiegeEngines().at(ram).cellVoxels != At(30, GROUND, 29); ++tick)
    {
      world.Tick();
    }
    Assert::IsTrue(world.SiegeEngines().at(ram).cellVoxels == At(30, GROUND, 29));
  }
};

} // namespace SimulationTests
