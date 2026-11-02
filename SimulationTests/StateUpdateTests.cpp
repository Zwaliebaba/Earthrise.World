#include "pch.h"

#include "BreachRun.h"
#include "FlatWorld.h"
#include "Script.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// State updates as values (M3-B2; ADR-020, decision 3): each tick's update is the whole state the
// player sees, in id order, and nothing more. The breach run is driven once and every tick's update
// checked against the world it came from.

namespace SimulationTests
{

namespace
{

/// What the run found, tick by tick.
struct Findings
{
  std::vector<std::string> faults;
  std::size_t collapsesSeen = 0;
  std::size_t shotsSeen = 0;
  std::size_t hitsSeen = 0;
  bool collapseEndedWithItsLandingLanded = false;
};

template <typename Map, typename View> bool SameIds(const Map& _map, const std::vector<View>& _views)
{
  if (_map.size() != _views.size())
  {
    return false;
  }
  std::size_t index = 0;
  for (const auto& [id, entity] : _map)
  {
    if (_views[index++].id != id)
    {
      return false;
    }
  }
  return true;
}

void Check(const Simulation::World& _world, std::uint32_t _tick, const std::set<Protocol::EntityId>& _collapsesBefore,
           std::map<Protocol::EntityId, Protocol::CollapseView>& _lastViews, Findings& _findings)
{
  const Protocol::StateUpdate& state = _world.State();
  const auto fault = [&_findings, _tick](const std::string& _what)
  { _findings.faults.push_back("tick " + std::to_string(_tick) + ": " + _what); };
  if (state.tick != _tick)
  {
    fault("the update names tick " + std::to_string(state.tick));
  }
  if (!SameIds(_world.Structures(), state.structures) || !SameIds(_world.SiegeEngines(), state.engines) ||
      !SameIds(_world.Squads(), state.squads) || !SameIds(_world.Units(), state.units) || !SameIds(_world.Shots(), state.shots))
  {
    fault("an entity is missing, extra or out of id order");
    return;
  }
  for (const Protocol::StructureView& view : state.structures)
  {
    const Simulation::Structure& structure = _world.Structures().at(view.id);
    if (view.name != structure.name || view.templateId != structure.templateId || !(view.originVoxels == structure.originVoxels) ||
        view.quarterTurns != structure.quarterTurns)
    {
      fault("structure " + std::to_string(view.id) + " differs");
    }
    // Its box: the template's shape, turned.
    const Simulation::ServerTemplate* found = _world.Data().FindTemplate(structure.templateId);
    if (found == nullptr)
    {
      fault("structure " + std::to_string(view.id) + "'s template is missing");
      return;
    }
    const Simulation::TemplateShape shape = found->Shape();
    const bool across = structure.quarterTurns % 2 == 1;
    const Protocol::VoxelPosition extent{static_cast<std::int32_t>(across ? shape.depthVoxels : shape.widthVoxels),
                                         static_cast<std::int32_t>(shape.heightVoxels),
                                         static_cast<std::int32_t>(across ? shape.widthVoxels : shape.depthVoxels)};
    if (!(view.extentVoxels == extent))
    {
      fault("structure " + std::to_string(view.id) + "'s box differs");
    }
  }
  for (const Protocol::EngineView& view : state.engines)
  {
    const Simulation::SiegeEngine& engine = _world.SiegeEngines().at(view.id);
    if (view.reloadTicks != engine.reloadTicks || !(view.cellVoxels == engine.cellVoxels) || view.quarterTurns != engine.quarterTurns)
    {
      fault("engine " + std::to_string(view.id) + " differs");
    }
  }
  for (const Protocol::UnitView& view : state.units)
  {
    const Simulation::Unit& unit = _world.Units().at(view.id);
    if (!(view.position == unit.position) || !(view.cellVoxels == unit.cellVoxels) || view.kind != unit.kind || view.squad != unit.squad)
    {
      fault("unit " + std::to_string(view.id) + " differs");
    }
  }
  for (const Protocol::ShotView& view : state.shots)
  {
    const Simulation::Shot& shot = _world.Shots().at(view.id);
    if (!(view.position == Simulation::ShotPositionAt(shot.arc, shot.ticksFlown)))
    {
      fault("shot " + std::to_string(view.id) + " is not where its arc puts it");
    }
    ++_findings.shotsSeen;
  }
  const std::span<const Simulation::ShotHit> hits = _world.LastTickHits();
  if (hits.size() != state.hits.size())
  {
    fault("the hits differ");
  }
  for (std::size_t hit = 0; hit < std::min(hits.size(), state.hits.size()); ++hit)
  {
    if (hits[hit].shot != state.hits[hit].shot || !(hits[hit].voxel == state.hits[hit].voxel))
    {
      fault("a hit differs");
    }
    ++_findings.hitsSeen;
  }
  // Collapses: every one running, and those whose fall ended this tick, once more.
  for (const auto& [id, collapse] : _world.Collapses())
  {
    if (std::ranges::none_of(state.collapses, [id](const Protocol::CollapseView& _view) { return _view.id == id; }))
    {
      fault("collapse " + std::to_string(id) + " is missing");
    }
  }
  for (const Protocol::CollapseView& view : state.collapses)
  {
    const auto running = _world.Collapses().find(view.id);
    const bool ended = running == _world.Collapses().end();
    if (ended && !_collapsesBefore.contains(view.id))
    {
      fault("collapse " + std::to_string(view.id) + " is neither running nor just ended");
      continue;
    }
    const auto last = _lastViews.find(view.id);
    if (last == _lastViews.end())
    {
      ++_findings.collapsesSeen;
      if (ended || !(view.landing == running->second.plan.landing) || !view.fallen.empty() || view.landedVoxels != 0 ||
          view.warningTicksLeft + 1 != running->second.plan.warningTicks || view.structure != running->second.structure)
      {
        fault("collapse " + std::to_string(view.id) + " does not start as its plan says");
      }
    }
    else
    {
      const Protocol::CollapseView& before = last->second;
      if (view.warningTicksLeft > before.warningTicksLeft ||
          (before.warningTicksLeft > 0 && view.warningTicksLeft != before.warningTicksLeft - 1) ||
          view.landedVoxels < before.landedVoxels || !(view.landing == before.landing))
      {
        fault("collapse " + std::to_string(view.id) + " does not count down or land in order");
      }
      // A warning that reached 0 last tick ended then; the piece leaves the grid on the tick after.
      if (before.warningTicksLeft == 0 && view.fallen.size() != view.piece.size())
      {
        fault("collapse " + std::to_string(view.id) + " falls without its fallen voxels");
      }
      if (view.warningTicksLeft > 0 && (!view.fallen.empty() || view.landedVoxels != 0))
      {
        fault("collapse " + std::to_string(view.id) + " lands during its warning");
      }
    }
    if (ended)
    {
      _findings.collapseEndedWithItsLandingLanded =
        view.landedVoxels == view.landing.size() && view.fallTicksLeft == 0 && !view.landing.empty();
      _lastViews.erase(view.id);
    }
    else
    {
      _lastViews[view.id] = view;
    }
  }
}

Findings RunBreach()
{
  Findings findings;
  const Simulation::World::Desc desc = Simulation::BreachRunDesc(BakedServerData(), FlatWorld::BUILD_ID);
  Simulation::World world(desc);
  const Simulation::BreachRun run = Simulation::BuildBreachRun(world);
  Assert::IsTrue(world.State().structures.empty());
  std::map<Protocol::EntityId, Protocol::CollapseView> lastViews;
  for (std::uint32_t tick = 0; tick < run.ticks; ++tick)
  {
    std::set<Protocol::EntityId> before;
    for (const auto& [id, collapse] : world.Collapses())
    {
      before.insert(id);
    }
    Simulation::RunScript(world, run.script, 1);
    Check(world, tick, before, lastViews, findings);
  }
  return findings;
}

const Findings& Breach()
{
  static const Findings FINDINGS = RunBreach();
  return FINDINGS;
}

} // namespace

TEST_CLASS(StateUpdateTests)
{
public:
  TEST_METHOD(EveryEntityAppearsInItsTicksUpdateInIdOrder)
  {
    const Findings& findings = Breach();
    for (const std::string& fault : findings.faults)
    {
      Logger::WriteMessage(fault.c_str());
    }
    Assert::IsTrue(findings.faults.empty());
    Assert::IsTrue(findings.shotsSeen > 0);
  }

  TEST_METHOD(TheTicksHitsAreItsShotsHits)
  {
    Assert::AreEqual(std::size_t{5}, Breach().hitsSeen);
  }

  TEST_METHOD(ACollapseCarriesItsLandingFromItsFirstWarningTickToItsLastFallTick)
  {
    const Findings& findings = Breach();
    Assert::AreEqual(std::size_t{1}, findings.collapsesSeen);
    Assert::IsTrue(findings.collapseEndedWithItsLandingLanded);
  }

  TEST_METHOD(CarriesThePlayersTheEnginesStatesAndTheGarrisons)
  {
    // M4-B4: the update names each player's stockpiles, each engine's state, and the building a squad is garrisoned
    // in, whose units it leaves out.
    Simulation::World world(FlatWorld::Desc());
    world.AddPlayer({.id = 2, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
    const Protocol::EntityId tower =
      world.AddStructure(2, "T4", {.xVoxels = 20, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = 20}, 0, "SW", 1);
    const std::array<Protocol::VoxelPosition, 2> cells = {{{.xVoxels = 4, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = 4},
                                                           {.xVoxels = 5, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = 4}}};
    const Protocol::EntityId garrison = world.AddSquad(2, Simulation::NavigationClass::Foot, cells, Protocol::UnitKind::Archer);
    world.PlaceInGarrison(garrison, tower);
    static_cast<void>(world.AddSiegeEngine(1, Simulation::EngineKind::Ram,
                                           {.xVoxels = 40, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = 4}, 0,
                                           Protocol::EngineState::Packed));
    world.Tick();
    const Protocol::StateUpdate& state = world.State();
    Assert::AreEqual(std::size_t{1}, state.players.size());
    Assert::AreEqual(std::uint8_t{2}, state.players.front().player);
    Assert::AreEqual(std::int32_t{20000}, state.players.front().stoneHundredthsCubicMeters);
    Assert::IsTrue(state.engines.front().state == Protocol::EngineState::Packed);
    Assert::AreEqual(tower, state.squads.front().garrisonedIn);
    Assert::IsTrue(state.units.empty());
  }

  TEST_METHOD(AFlatWorldsFirstUpdateNamesItsTickAndItsSquad)
  {
    Simulation::World world(FlatWorld::Desc());
    const std::array<Protocol::VoxelPosition, 2> cells = {{{.xVoxels = 4, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = 4},
                                                           {.xVoxels = 5, .yVoxels = FlatWorld::GROUND_VOXELS, .zVoxels = 4}}};
    const Protocol::EntityId squad = world.AddSquad(1, Simulation::NavigationClass::Foot, cells, Protocol::UnitKind::Archer);
    world.Tick();
    world.Tick();
    const Protocol::StateUpdate& state = world.State();
    Assert::AreEqual(1U, state.tick);
    Assert::AreEqual(std::size_t{1}, state.squads.size());
    Assert::AreEqual(squad, state.squads.front().id);
    Assert::AreEqual(std::size_t{2}, state.units.size());
    Assert::IsTrue(state.units.front().kind == Protocol::UnitKind::Archer);
  }
};

} // namespace SimulationTests
