#include "pch.h"

#include "FlatWorld.h"
#include "ServerDataFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Hit points and the crush of collapses and falls (M4-C1; ADR-023, decisions 2 and 8): a unit at 0 dies at the end of the
// step that killed it, a squad with no figure left goes with it, and an engine with its body.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t PLAYER = 1;
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

const Simulation::Rules& Rules()
{
  return BakedServerData().Tuning();
}

std::int32_t HitPoints(Protocol::UnitKind _kind)
{
  return BakedServerData().Stats(_kind).hitPoints;
}

/// FallTests' block, a 3 x 4 x 3 piece on a pier at x 12, which falls east once the pier goes: the strip it covers is
/// east of x 12. Ticks until the fall is over, and returns every event of those ticks.
std::vector<Simulation::TrialEvent> DropTheBlock(Simulation::World& _world)
{
  Fill(_world, At(10, GROUND + 2, 10), At(12, GROUND + 5, 12), MASONRY);
  Fill(_world, At(12, GROUND, 10), At(12, GROUND + 1, 12), MASONRY);
  _world.Tick();
  Fill(_world, At(12, GROUND, 10), At(12, GROUND + 1, 12), 0);
  std::vector<Simulation::TrialEvent> events;
  do
  {
    _world.Tick();
    events.insert(events.end(), _world.LastTickEvents().begin(), _world.LastTickEvents().end());
  } while (!_world.Collapses().empty());
  return events;
}

} // namespace

TEST_CLASS(DamageTests)
{
public:
  TEST_METHOD(ACollapseCrushesAUnitInItsStripOnce)
  {
    // A man-at-arms, 70 hit points, in the strip takes the collapse's 60 crush once, however many of its ticks reach him.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const std::array<Protocol::VoxelPosition, 1> cell = {At(14, GROUND, 11)};
    const Protocol::EntityId squad = world.AddSquad(PLAYER, Protocol::UnitKind::ManAtArms, cell);
    const Protocol::EntityId unit = world.Squads().at(squad).units.front();
    static_cast<void>(DropTheBlock(world));
    Assert::AreEqual(HitPoints(Protocol::UnitKind::ManAtArms) - Rules().collapseCrushDamage, world.Units().at(unit).hitPoints);
  }

  TEST_METHOD(AFiveMetreDropDealsTwoMetresWorth)
  {
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const std::array<Protocol::VoxelPosition, 1> cell = {At(20, GROUND, 20)};
    const Protocol::EntityId squad = world.AddSquad(PLAYER, Protocol::UnitKind::Spearman, cell);
    const Protocol::EntityId unit = world.Squads().at(squad).units.front();
    world.Tick();
    Fill(world, At(20, GROUND - 5, 20), At(20, GROUND - 1, 20), 0);
    world.Tick();
    Assert::AreEqual(HitPoints(Protocol::UnitKind::Spearman) - (2 * Rules().dropCrushDamagePerVoxel), world.Units().at(unit).hitPoints);
  }

  TEST_METHOD(AUnitKilledLeavesItsSquadOneFigureShort)
  {
    // Two spearmen, 45 hit points: the one in the strip dies of the collapse's 60, and its squad goes on with one.
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const std::array<Protocol::VoxelPosition, 2> cells = {At(14, GROUND, 11), At(30, GROUND, 30)};
    const Protocol::EntityId squad = world.AddSquad(PLAYER, Protocol::UnitKind::Spearman, cells);
    const Protocol::EntityId struck = world.Squads().at(squad).units.front();
    const Protocol::EntityId spared = world.Squads().at(squad).units.back();
    std::vector<Simulation::TrialEvent> died = DropTheBlock(world);
    std::erase_if(died, [](const Simulation::TrialEvent& _event) { return _event.kind != Simulation::TrialEventKind::UnitDied; });
    Assert::IsFalse(world.Units().contains(struck));
    Assert::IsTrue(world.Squads().at(squad).units == std::vector<Protocol::EntityId>{spared});
    Assert::AreEqual(std::size_t{1}, died.size());
    Assert::AreEqual(struck, died.front().subject);
    Assert::AreEqual(PLAYER, died.front().player);
    Assert::AreEqual(static_cast<std::int32_t>(Protocol::UnitKind::Spearman), died.front().amount);
  }

  TEST_METHOD(ASquadsLastFigureKilledRemovesTheSquad)
  {
    Simulation::World world(FlatWorld::Desc(2, 2, FlatWorld::NoSlump()));
    const std::array<Protocol::VoxelPosition, 1> cell = {At(20, GROUND, 20)};
    const Protocol::EntityId squad = world.AddSquad(PLAYER, Protocol::UnitKind::Laborer, cell);
    world.Tick();
    // A 6 m drop deals 30 crush, past a laborer's 25 hit points.
    Fill(world, At(20, GROUND - 6, 20), At(20, GROUND - 1, 20), 0);
    world.Tick();
    Assert::IsFalse(world.Squads().contains(squad));
    Assert::IsTrue(world.Units().empty());
  }

  TEST_METHOD(ARamsBodyKilledRemovesTheEngine)
  {
    // A ram's 300 hit points outlast any drop the flat world holds, so the drop's crush is raised for this test alone.
    static const Simulation::ServerData DEADLY = BakedServerDataWith("dropCrushDamagePerVoxel", 1000);
    Simulation::World world(FlatWorld::Desc(2, 2, &DEADLY));
    const Protocol::EntityId ram =
      world.AddSiegeEngine(PLAYER, Simulation::EngineKind::Ram, At(20, GROUND, 20), 0, Protocol::EngineState::Packed);
    const Protocol::EntityId body = world.SiegeEngines().at(ram).squad;
    world.Tick();
    Assert::AreEqual(HitPoints(Protocol::UnitKind::Ram), world.State().engines.front().hitPoints);
    Fill(world, At(17, GROUND - 4, 17), At(25, GROUND - 1, 25), 0);
    world.Tick();
    Assert::IsTrue(world.SiegeEngines().empty());
    Assert::IsFalse(world.Squads().contains(body));
    Assert::IsTrue(world.Units().empty());
  }

  TEST_METHOD(RefusesACommandItHasNotBuiltAndNamesItToItsPlayer)
  {
    // M4-C1: every command a later order builds is refused Invalid until then, and an order for another player's squad
    // is refused NotOwned; each comes back in its player's update alone.
    Simulation::World world(FlatWorld::Desc());
    world.AddPlayer({.id = 1, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0});
    world.AddPlayer({.id = 2, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0});
    const std::array<Protocol::VoxelPosition, 1> cell = {At(5, GROUND, 5)};
    const Protocol::EntityId theirs = world.AddSquad(2, Protocol::UnitKind::Spearman, cell);
    for (const Protocol::Command& command : {Protocol::Command{Protocol::SetGate{.structure = 9, .open = true}},
                                             Protocol::Command{Protocol::MoveSquad{.squad = theirs, .goalVoxels = {}}}})
    {
      Core::ByteWriter writer;
      Protocol::Encode(command, writer);
      world.Submit(1, writer.Bytes());
    }
    world.Tick();
    const std::vector<Protocol::RefusalView> refusals = world.State(1).refusals;
    Assert::AreEqual(std::size_t{2}, refusals.size());
    Assert::AreEqual(std::uint16_t{15}, refusals.front().commandTag);
    Assert::IsTrue(refusals.front().reason == Protocol::Refusal::Invalid);
    Assert::AreEqual(std::uint16_t{1}, refusals.back().commandTag);
    Assert::IsTrue(refusals.back().reason == Protocol::Refusal::NotOwned);
    Assert::IsTrue(world.State(2).refusals.empty());
    world.Tick();
    Assert::IsTrue(world.State(1).refusals.empty());
  }
};

} // namespace SimulationTests
