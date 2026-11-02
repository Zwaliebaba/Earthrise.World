#include "pch.h"

#include "FlatWorld.h"
#include "Formation.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Garrisons (M4-D3; ADR-024, decision 5): a squad goes into a working building of its owner's with room for it; inside it
// stands in no cell, takes no damage, heals a hit point every 5 s, and its archers loose from the cell outside the
// building's face nearest their target, at its stand height. T4 holds six at 11 m, so an archer there is two steps above
// the ground and reaches 30 + 2 x 5 = 40 m. A building that stops working, or its owner's EjectGarrison, puts them out on
// its owner's side, clear of its footprint and of any falling piece.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr Protocol::VoxelPosition TOWER{.xVoxels = 20, .yVoxels = GROUND, .zVoxels = 60}; // its footprint x 20 to 26, z 60 to 66
constexpr std::uint8_t SOUTH = 4;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

void Order(Simulation::World& _world, std::uint8_t _player, const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  _world.Submit(_player, writer.Bytes());
}

/// A world 128 m square with two players and a T4 of the defender's.
struct Site
{
  Simulation::World world{FlatWorld::Desc(4, 4)};
  Protocol::EntityId tower = Protocol::NO_ENTITY;

  Site()
  {
    world.AddPlayer(
      {.id = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 12000, .timberHundredthsCubicMeters = 0});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
    tower = world.AddStructure(DEFENDER, "T4", TOWER, 0, "NE");
  }
};

Protocol::EntityId Archers(Simulation::World& _world, Protocol::VoxelPosition _lead)
{
  const std::vector<Protocol::VoxelPosition> cells = Simulation::FormationSlots(_lead, SOUTH, 6, 2);
  return _world.AddSquad(DEFENDER, Simulation::UnitKind::Archer, cells);
}

bool Refused(const Simulation::World& _world, std::uint8_t _player, Protocol::Refusal _reason)
{
  return std::ranges::any_of(_world.State(_player).refusals,
                             [_reason](const Protocol::RefusalView& _view) { return _view.reason == _reason; });
}

bool InFootprint(Protocol::VoxelPosition _cell)
{
  return _cell.xVoxels >= TOWER.xVoxels && _cell.xVoxels < TOWER.xVoxels + 7 && _cell.zVoxels >= TOWER.zVoxels &&
         _cell.zVoxels < TOWER.zVoxels + 7;
}

/// A spearman of the attacker's holding in _cell, and the hit points it has lost after _ticks.
std::int32_t SpearmanLoses(Simulation::World& _world, Protocol::VoxelPosition _cell, std::uint32_t _ticks)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {_cell};
  const Protocol::EntityId squad = _world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, cells);
  Order(_world, ATTACKER, Protocol::HoldSquad{.squad = squad});
  const Protocol::EntityId unit = _world.Squads().at(squad).units.front();
  for (std::uint32_t tick = 0; tick < _ticks; ++tick)
  {
    _world.Tick();
  }
  return _world.Data().Stats(Simulation::UnitKind::Spearman).hitPoints - _world.Units().at(unit).hitPoints;
}

} // namespace

TEST_CLASS(GarrisonTests)
{
public:
  TEST_METHOD(SixArchersGarrisonAT4AndASeventhIsRefusedFull)
  {
    Site site;
    Simulation::World& world = site.world;
    const Protocol::EntityId archers = Archers(world, At(23, GROUND, 75));
    Order(world, DEFENDER, Protocol::GarrisonSquad{.squad = archers, .structure = site.tower});
    for (std::uint32_t tick = 0; tick < 60 * 20 && world.Squads().at(archers).garrisonedIn == Protocol::NO_ENTITY; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(site.tower, world.Squads().at(archers).garrisonedIn);
    const auto view = std::ranges::find(world.State(DEFENDER).structures, site.tower, &Protocol::StructureView::id);
    Assert::IsTrue(view != world.State(DEFENDER).structures.end() && view->garrisonCount == 6 && view->garrisonCapacity == 6);
    const std::array<Protocol::VoxelPosition, 1> one = {At(10, GROUND, 75)};
    const Protocol::EntityId seventh = world.AddSquad(DEFENDER, Simulation::UnitKind::Archer, one);
    Order(world, DEFENDER, Protocol::GarrisonSquad{.squad = seventh, .structure = site.tower});
    world.Tick();
    Assert::IsTrue(Refused(world, DEFENDER, Protocol::Refusal::Full));
    Assert::AreEqual(Protocol::NO_ENTITY, world.Squads().at(seventh).garrisonedIn);
  }

  TEST_METHOD(GarrisonedArchersReachFortyMetersFromTheFaceAndNotFortyOne)
  {
    // The east face is x 26, so they loose from x 27, 11 m up: a spearman at x 67 is 40 m off and takes six arrows of
    // floor(4 x 130 / 100) = 5; one at x 68 takes none.
    Site inReach;
    inReach.world.PlaceInGarrison(Archers(inReach.world, At(23, GROUND, 75)), inReach.tower);
    Assert::AreEqual(30, SpearmanLoses(inReach.world, At(67, GROUND, 63), 35));
    Site beyond;
    beyond.world.PlaceInGarrison(Archers(beyond.world, At(23, GROUND, 75)), beyond.tower);
    Assert::AreEqual(0, SpearmanLoses(beyond.world, At(68, GROUND, 63), 35));
  }

  TEST_METHOD(GarrisonedFiguresTakeNoDamageAndHeal)
  {
    // Men-at-arms struck by a stone, 40 of their 70 gone, then put into the tower; a stone on the tower's footprint and
    // spearmen at its foot do them no harm, and each heals a hit point every 100 ticks.
    Site site;
    Simulation::World& world = site.world;
    const std::array<Protocol::VoxelPosition, 1> cell = {At(60, GROUND, 100)};
    const Protocol::EntityId menAtArms = world.AddSquad(DEFENDER, Simulation::UnitKind::ManAtArms, cell);
    const Protocol::EntityId unit = world.Squads().at(menAtArms).units.front();
    const Protocol::EntityId engine = world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Trebuchet, At(60, GROUND, 70));
    Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(60, GROUND - 1, 100)});
    for (std::uint32_t tick = 0; tick < 200 && world.LastTickHits().empty(); ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(30, world.Units().at(unit).hitPoints);
    world.PlaceInGarrison(menAtArms, site.tower);
    const std::uint32_t from = world.TickCount();
    const std::array<Protocol::VoxelPosition, 3> foot = {At(19, GROUND, 63), At(19, GROUND, 62), At(19, GROUND, 64)};
    world.AddSquad(ATTACKER, Simulation::UnitKind::Spearman, foot);
    for (std::uint32_t tick = 0; tick < 400 && world.SiegeEngines().at(engine).reloadTicks > 0; ++tick)
    {
      world.Tick();
    }
    Order(world, ATTACKER, Protocol::FireEngine{.engine = engine, .targetVoxels = At(22, GROUND + 3, 66)});
    for (std::uint32_t tick = 0; tick < 500; ++tick)
    {
      world.Tick();
    }
    // A tick heals when its count is a multiple of 100: those from the tick it went in to the last one run.
    const std::uint32_t heals = ((world.TickCount() + 99) / 100) - ((from + 99) / 100);
    Assert::IsTrue(heals >= 5);
    Assert::AreEqual(30 + static_cast<std::int32_t>(heals), world.Units().at(unit).hitPoints);
  }

  TEST_METHOD(EjectPutsThemOnFreeCellsOnTheOwnersSideClearOfTheFootprint)
  {
    // The defender's keep stands to the south-east, farther south than east, so its side of the tower is the south.
    Site site;
    Simulation::World& world = site.world;
    world.AddStructure(DEFENDER, "T2", At(50, GROUND, 20), 0, "KEEP");
    const Protocol::EntityId archers = Archers(world, At(23, GROUND, 75));
    world.PlaceInGarrison(archers, site.tower);
    world.Tick();
    Order(world, DEFENDER, Protocol::EjectGarrison{.structure = site.tower});
    world.Tick();
    Assert::AreEqual(Protocol::NO_ENTITY, world.Squads().at(archers).garrisonedIn);
    // Doubled, the tower's centre is (46, 126) and the keep's (111, 54).
    std::set<std::pair<std::int32_t, std::int32_t>> cells;
    for (const Protocol::EntityId id : world.Squads().at(archers).units)
    {
      const Protocol::VoxelPosition cell = world.Units().at(id).cellVoxels;
      Assert::IsFalse(InFootprint(cell));
      Assert::IsTrue(world.Navigation().Usable(cell.xVoxels, cell.zVoxels, Simulation::NavigationClass::Foot));
      // The tower is square and the keep lies farther south than east of it, 72 to 65 doubled, so its side is south of
      // the tower's centre line across z.
      Assert::IsTrue(((2 * cell.zVoxels) - 126) * (54 - 126) > 0);
      cells.emplace(cell.xVoxels, cell.zVoxels);
    }
    Assert::AreEqual(std::size_t{6}, cells.size());
  }

  TEST_METHOD(ATowerThatStopsWorkingPutsThemOutClearOfItsFall)
  {
    // The ring of wall below the platform cut away: the platform warns and the tower stops; its archers are put out, none
    // of them in its footprint or the strip the piece will fall on.
    Site site;
    Simulation::World& world = site.world;
    const Protocol::EntityId archers = Archers(world, At(23, GROUND, 75));
    world.PlaceInGarrison(archers, site.tower);
    world.Tick();
    for (std::int32_t z = 0; z < 7; ++z)
    {
      for (std::int32_t x = 0; x < 7; ++x)
      {
        world.Voxels().Edit(At(TOWER.xVoxels + x, GROUND + 8, TOWER.zVoxels + z), 0);
      }
    }
    world.Tick();
    Assert::IsFalse(world.Structures().at(site.tower).working);
    Assert::AreEqual(Protocol::NO_ENTITY, world.Squads().at(archers).garrisonedIn);
    std::set<std::pair<std::int32_t, std::int32_t>> strip;
    for (const auto& [id, collapse] : world.Collapses())
    {
      for (const Protocol::VoxelPosition& cell : collapse.plan.strip)
      {
        strip.emplace(cell.xVoxels, cell.zVoxels);
      }
    }
    Assert::IsFalse(strip.empty());
    for (const Protocol::EntityId id : world.Squads().at(archers).units)
    {
      const Protocol::VoxelPosition cell = world.Units().at(id).cellVoxels;
      Assert::IsFalse(InFootprint(cell));
      Assert::IsFalse(strip.contains({cell.xVoxels, cell.zVoxels}));
    }
  }
};

} // namespace SimulationTests
