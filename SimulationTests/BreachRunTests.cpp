#include "pch.h"

#include "BreachLab.h"
#include "BreachRun.h"
#include "FlatWorld.h"
#include "Generator.h"
#include "Replay.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The M2 breach run, in CI (M2-G1; MvpPlan.md, M2, *Done when*). Each line of the done-when is a
// test below. The run is the costliest thing the suites do, a whole MVP map, so it runs once, and
// every test reads what that run recorded; the replay and the saves each run it again. The
// done-when's last line, the generator's golden hashes and slope invariant, is GeneratorTests'.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::int32_t SECTION_WEST = 377; // the middle south section, the target
constexpr std::int32_t SECTION_EAST = 384;

/// What the M2 breach run did, tick by tick.
struct Outcome
{
  Outcome() = default;
  ~Outcome() = default;
  Outcome(const Outcome&) = default;
  Outcome& operator=(const Outcome&) = default;
  // A move may allocate under the debug library's checked containers, so it is not noexcept
  // (bugprone-exception-escape), as World's is not.
  Outcome(Outcome&&) noexcept(false) = default;
  Outcome& operator=(Outcome&&) noexcept(false) = default;

  Simulation::World::Desc desc;
  Simulation::BreachRun run;
  std::vector<Simulation::ShotHit> hits;
  bool everyCraterOpened = true;               // each hit voxel and the one behind it empty the tick it struck
  std::vector<Simulation::Collapse> collapses; // each as it started
  std::map<Protocol::EntityId, std::string> structureNames;
  bool piecesHeldThroughTheirWarnings = true;
  std::vector<Simulation::CrushEvent> crushes;
  std::uint32_t firstFallTick = 0;                // the tick the first piece left the grid
  bool shortUntilTheFall = true;                  // from the order to the fall, short of the goal
  std::set<std::int64_t> waitingCells;            // where its paths ended while it was short
  std::optional<std::uint32_t> pathedThroughTick; // the first tick it was no longer short
  std::vector<std::int64_t> pathThrough;          // its path then
  std::vector<std::string> standingFaults;        // a unit on a structure, or outside its cell
  std::vector<Protocol::VoxelPosition> finalCells;
  std::map<Protocol::EntityId, std::uint32_t> inside; // the tick each figure first stood north of the wall's south face
  std::uint64_t finalHash = 0;
  std::size_t stoneRubble = 0;                                         // in the footprint at the end
  std::vector<std::pair<std::uint32_t, std::vector<std::byte>>> saves; // before the breach, mid-fall, mid-walk
  std::vector<std::byte> replay;
};

void CheckStanding(const Simulation::World& _world, std::uint32_t _tick, Outcome& _outcome)
{
  const Simulation::MaterialRules& materials = _world.Data().Materials();
  for (const auto& [id, unit] : _world.Units())
  {
    const Protocol::VoxelPosition cell = unit.cellVoxels;
    const std::uint8_t standing = _world.Voxels().At(cell);
    const std::uint8_t under = _world.Voxels().At({.xVoxels = cell.xVoxels, .yVoxels = cell.yVoxels - 1, .zVoxels = cell.zVoxels});
    const bool open = standing == Protocol::Chunk::EMPTY || materials.at(standing).liquid;
    const bool inCell = unit.position.xMeters.FloorWhole() == cell.xVoxels && unit.position.zMeters.FloorWhole() == cell.zVoxels &&
                        unit.position.yMeters.FloorWhole() == cell.yVoxels;
    if (!open || !materials.at(under).walkableGround || !inCell)
    {
      _outcome.standingFaults.push_back("tick " + std::to_string(_tick) + ": unit " + std::to_string(id) + " at (" +
                                        std::to_string(cell.xVoxels) + ", " + std::to_string(cell.yVoxels) + ", " +
                                        std::to_string(cell.zVoxels) + ")");
    }
  }
}

/// Notes the tick each of _squad's figures first stands north of the wall's south face.
void NoteInside(const Simulation::World& _world, Protocol::EntityId _squad, std::uint32_t _tick,
                std::map<Protocol::EntityId, std::uint32_t>& _inside)
{
  for (const Protocol::EntityId id : _world.Squads().at(_squad).units)
  {
    if (_world.Units().at(id).cellVoxels.zVoxels > Simulation::BreachRun::WALL_SOUTH_VOXELS)
    {
      _inside.emplace(id, _tick);
    }
  }
}

Outcome RunBreach()
{
  Outcome outcome;
  outcome.desc = Simulation::BreachRunDesc(BakedServerData(), FlatWorld::BUILD_ID);
  Simulation::World world(outcome.desc);
  outcome.run = Simulation::BuildBreachRun(world);
  const Simulation::BreachRun& run = outcome.run;
  for (const auto& [id, structure] : world.Structures())
  {
    outcome.structureNames.emplace(id, structure.name);
  }
  Simulation::ReplayRecorder recorder(world);
  bool beforeBreachSaved = false;
  bool midFallSaved = false;
  bool midWalkSaved = false;
  for (std::uint32_t tick = 0; tick < run.ticks; ++tick)
  {
    // Saves between ticks, each the first time its moment comes: before the breach, once two shots
    // have struck; mid-fall, halfway through a collapse's fall; mid-walk, with the lead in a step a
    // second after the squad found its way through.
    const Simulation::Squad& waiting = world.Squads().at(run.squad);
    const bool beforeBreach = !beforeBreachSaved && outcome.hits.size() == 2;
    const bool midFall =
      !midFallSaved && std::ranges::any_of(world.Collapses(),
                                           [](const auto& _collapse)
                                           {
                                             const Simulation::Collapse& collapse = _collapse.second;
                                             return collapse.ticksElapsed == collapse.plan.warningTicks + collapse.plan.fallTicks / 2;
                                           });
    const bool midWalk = !midWalkSaved && outcome.pathedThroughTick.has_value() && tick > *outcome.pathedThroughTick + 20 &&
                         world.Units().at(waiting.units.front()).stepEffort > 0;
    if (beforeBreach || midFall || midWalk)
    {
      beforeBreachSaved = beforeBreachSaved || beforeBreach;
      midFallSaved = midFallSaved || midFall;
      midWalkSaved = midWalkSaved || midWalk;
      outcome.saves.emplace_back(tick, Simulation::WriteSave(world));
    }

    Simulation::RunScript(world, run.script, 1);
    recorder.Record(world);

    for (const Simulation::ShotHit& hit : world.LastTickHits())
    {
      outcome.hits.push_back(hit);
      const Protocol::VoxelPosition behind{
        .xVoxels = hit.voxel.xVoxels + hit.behindX, .yVoxels = hit.voxel.yVoxels, .zVoxels = hit.voxel.zVoxels + hit.behindZ};
      outcome.everyCraterOpened = outcome.everyCraterOpened && world.Voxels().At(hit.voxel) == Protocol::Chunk::EMPTY &&
                                  world.Voxels().At(behind) == Protocol::Chunk::EMPTY;
    }
    for (const auto& [id, collapse] : world.Collapses())
    {
      if (collapse.ticksElapsed == 1)
      {
        outcome.collapses.push_back(collapse);
      }
      if (collapse.ticksElapsed <= collapse.plan.warningTicks)
      {
        outcome.piecesHeldThroughTheirWarnings =
          outcome.piecesHeldThroughTheirWarnings &&
          std::ranges::all_of(collapse.piece, [&world](const Protocol::VoxelPosition& _voxel) { return world.Voxels().At(_voxel) != 0; });
      }
      else if (outcome.firstFallTick == 0)
      {
        outcome.firstFallTick = tick;
      }
    }
    outcome.crushes.insert(outcome.crushes.end(), world.LastTickCrushes().begin(), world.LastTickCrushes().end());

    const Simulation::Squad& squad = world.Squads().at(run.squad);
    const bool searching = std::ranges::any_of(world.Searches(), [&run](const auto& _search) { return _search.first.second == run.squad; });
    if (squad.hasGoal && !squad.path.empty() && !searching && !outcome.pathedThroughTick.has_value())
    {
      if (squad.shortOfGoal)
      {
        outcome.waitingCells.insert(squad.path.back());
      }
      else
      {
        outcome.pathedThroughTick = tick;
        outcome.pathThrough = squad.path;
        outcome.shortUntilTheFall = outcome.firstFallTick != 0;
      }
    }
    CheckStanding(world, tick, outcome);
    NoteInside(world, run.squad, tick, outcome.inside);
  }
  for (const Protocol::EntityId id : world.Squads().at(run.squad).units)
  {
    outcome.finalCells.push_back(world.Units().at(id).cellVoxels);
  }
  for (std::int32_t z = Simulation::BreachRun::LEVEL_SOUTH_VOXELS; z <= Simulation::BreachRun::LEVEL_NORTH_VOXELS; ++z)
  {
    for (std::int32_t x = Simulation::BreachRun::LEVEL_WEST_VOXELS; x <= Simulation::BreachRun::LEVEL_EAST_VOXELS; ++x)
    {
      for (std::int32_t y = 0; y < Simulation::Generator::MAP_HEIGHT_VOXELS; ++y)
      {
        outcome.stoneRubble +=
          world.Voxels().At({.xVoxels = x, .yVoxels = y, .zVoxels = z}) == STONE_RUBBLE ? std::size_t{1} : std::size_t{0};
      }
    }
  }
  outcome.finalHash = Simulation::StateHash(world);
  outcome.replay.assign(recorder.Bytes().begin(), recorder.Bytes().end());
  return outcome;
}

const Outcome& Breach()
{
  static const Outcome OUTCOME = RunBreach();
  return OUTCOME;
}

/// The breach run with a second squad of six men-at-arms a row behind the first, ordered on the same
/// tick to 4 m beyond the first's goal, and run on until both have come to rest.
struct TwoSquads
{
  std::vector<std::string> sharedCells; // each tick two units stood in one cell, and which
  bool quiet = false;
  std::vector<Protocol::VoxelPosition> finalCells; // both squads' figures, the first's first
  Protocol::VoxelPosition firstLead{};
  Protocol::VoxelPosition secondLead{};
  Protocol::VoxelPosition firstGoal{};
  Protocol::VoxelPosition secondGoal{};
};

TwoSquads RunTwoSquads()
{
  TwoSquads outcome;
  Simulation::World world(Simulation::BreachRunDesc(BakedServerData(), FlatWorld::BUILD_ID));
  const Simulation::BreachRun run = Simulation::BuildBreachRun(world);
  std::vector<Protocol::VoxelPosition> cells;
  for (const Protocol::EntityId id : world.Squads().at(run.squad).units)
  {
    const Protocol::VoxelPosition cell = world.Units().at(id).cellVoxels;
    cells.push_back({.xVoxels = cell.xVoxels, .yVoxels = cell.yVoxels, .zVoxels = cell.zVoxels - 1});
  }
  const Protocol::EntityId second = world.AddSquad(Simulation::BreachRun::PLAYER, Simulation::UnitKind::ManAtArms, cells);
  outcome.firstGoal = run.goalVoxels;
  outcome.secondGoal = {.xVoxels = run.goalVoxels.xVoxels, .yVoxels = run.goalVoxels.yVoxels, .zVoxels = run.goalVoxels.zVoxels + 4};
  std::vector<Simulation::ScriptedCommand> script = run.script;
  script.push_back({.tick = Simulation::LAB_ORDER_TICK,
                    .player = Simulation::BreachRun::PLAYER,
                    .command = Protocol::MoveSquad{.squad = second, .goalVoxels = outcome.secondGoal}});
  constexpr std::uint32_t MORE_TICKS = 1200; // a minute more, for the second squad to follow the first
  for (std::uint32_t tick = 0; tick < run.ticks + MORE_TICKS && !outcome.quiet; ++tick)
  {
    Simulation::RunScript(world, script, 1);
    std::map<std::pair<std::int32_t, std::int32_t>, Protocol::EntityId> held;
    for (const auto& [id, unit] : world.Units())
    {
      const auto [at, inserted] = held.emplace(std::pair{unit.cellVoxels.xVoxels, unit.cellVoxels.zVoxels}, id);
      if (!inserted)
      {
        outcome.sharedCells.push_back("tick " + std::to_string(tick) + ": units " + std::to_string(at->second) + " and " +
                                      std::to_string(id));
      }
    }
    outcome.quiet = tick >= run.ticks && world.Quiet();
  }
  for (const Protocol::EntityId squad : {run.squad, second})
  {
    for (const Protocol::EntityId id : world.Squads().at(squad).units)
    {
      outcome.finalCells.push_back(world.Units().at(id).cellVoxels);
    }
  }
  outcome.firstLead = world.Units().at(world.Squads().at(run.squad).units.front()).cellVoxels;
  outcome.secondLead = world.Units().at(world.Squads().at(second).units.front()).cellVoxels;
  return outcome;
}

std::int64_t Cell(std::int32_t _x, std::int32_t _z)
{
  return std::int64_t{_z} * Simulation::Generator::MAP_WIDTH_VOXELS + _x;
}

} // namespace

TEST_CLASS(BreachRunTests)
{
public:
  TEST_METHOD(FiresOnScriptedCommandsAndLeavesCratersByMaterial)
  {
    // Five shots, each aimed at the foot of the south face; descending at 45 degrees, each meets the
    // face a voxel above its aim point, and a stone crater takes the voxel struck and the one behind
    // it along the shot, here north, through the wall's 2 m.
    const Outcome& outcome = Breach();
    Assert::AreEqual(std::size_t{5}, outcome.hits.size());
    for (std::size_t shot = 0; shot < outcome.hits.size(); ++shot)
    {
      const Simulation::ShotHit& hit = outcome.hits[shot];
      Assert::AreEqual(379 + static_cast<std::int32_t>(shot), hit.voxel.xVoxels);
      Assert::AreEqual(Simulation::BreachRun::GROUND_VOXELS + 1, hit.voxel.yVoxels);
      Assert::AreEqual(Simulation::BreachRun::WALL_SOUTH_VOXELS, hit.voxel.zVoxels);
      Assert::AreEqual(0, hit.behindX);
      Assert::AreEqual(1, hit.behindZ);
    }
    Assert::IsTrue(outcome.everyCraterOpened);
  }

  TEST_METHOD(TheSlotLosesSupportWarnsAndFallsTowardsTheSideThatLostIt)
  {
    // E3's finding: once the hole is 5 wide, the slot above its middle is more than stone's 2 m span
    // from the wall either side, and falls. The fifth crater, at its east end, is what took its
    // support, so it falls east (GameDesign.md, M2's values, item 6).
    const Outcome& outcome = Breach();
    Assert::AreEqual(std::size_t{1}, outcome.collapses.size());
    const Simulation::Collapse& collapse = outcome.collapses.front();
    for (const Protocol::VoxelPosition& voxel : collapse.piece)
    {
      Assert::AreEqual(381, voxel.xVoxels);
      Assert::IsTrue(voxel.yVoxels > Simulation::BreachRun::GROUND_VOXELS + 1);
    }
    // 1 s, plus the time foot take to walk the piece's 5 m at 1.4 m/s, in whole ticks (item 4).
    Assert::AreEqual(92U, collapse.plan.warningTicks);
    Assert::IsTrue(outcome.piecesHeldThroughTheirWarnings);
    Assert::IsTrue(collapse.plan.direction == Simulation::FallDirection::East);
  }

  TEST_METHOD(TheSlotThatFallsIsW2s)
  {
    // The enclosure is W1 to W10, south side first, so the middle south section, the target, is W2,
    // and the collapse names it (M3-B1).
    const Outcome& outcome = Breach();
    Assert::AreEqual(std::size_t{10}, outcome.structureNames.size());
    Assert::AreEqual(std::size_t{1}, outcome.collapses.size());
    Assert::AreEqual(std::string("W2"), outcome.structureNames.at(outcome.collapses.front().structure));
  }

  TEST_METHOD(ItsRubbleKeepsItsMaterial)
  {
    // Masonry falls as stone rubble, one voxel for each (item 7), and slump moves it without changing
    // it: the stone rubble in the footprint at the end is what the landing placed.
    const Outcome& outcome = Breach();
    std::size_t landed = 0;
    for (const Simulation::Collapse& collapse : outcome.collapses)
    {
      for (const Simulation::PlacedVoxel& placed : collapse.plan.landing)
      {
        Assert::AreEqual(STONE_RUBBLE, placed.paletteIndex);
      }
      landed += collapse.plan.landing.size();
    }
    Assert::IsTrue(landed >= outcome.collapses.front().piece.size());
    Assert::AreEqual(landed, outcome.stoneRubble);
  }

  TEST_METHOD(TheSquadWaitsAsNearAsItCanThenPathsThroughTheBreach)
  {
    const Outcome& outcome = Breach();
    // Before the breach: short of its goal, its path ending at the nearest cell it could reach,
    // against the south wall straight below the goal.
    Assert::IsTrue(outcome.shortUntilTheFall);
    Assert::AreEqual(std::size_t{1}, outcome.waitingCells.size());
    Assert::AreEqual(Cell(381, Simulation::BreachRun::WALL_SOUTH_VOXELS - 1), *outcome.waitingCells.begin());
    // The way opens when the last of the fall's rubble lands and the strip under it is crushed, a
    // ramp over the wall's foot: the squad, searching again after every rebuild, finds it the next
    // tick, and its path goes through the section.
    Assert::IsTrue(outcome.pathedThroughTick.has_value());
    const std::uint32_t pathedThrough = outcome.pathedThroughTick.value_or(0);
    const std::uint32_t fallTicks = outcome.collapses.front().plan.fallTicks;
    Assert::IsTrue(pathedThrough > outcome.firstFallTick && pathedThrough <= outcome.firstFallTick + fallTicks + 1);
    Assert::IsTrue(std::ranges::any_of(outcome.pathThrough,
                                       [](std::int64_t _cell)
                                       {
                                         const std::int64_t x = _cell % Simulation::Generator::MAP_WIDTH_VOXELS;
                                         const std::int64_t z = _cell / Simulation::Generator::MAP_WIDTH_VOXELS;
                                         return x >= SECTION_WEST && x <= SECTION_EAST && z == Simulation::BreachRun::WALL_SOUTH_VOXELS;
                                       }));
    Assert::AreEqual(outcome.run.goalVoxels.xVoxels, outcome.finalCells.front().xVoxels);
    Assert::AreEqual(outcome.run.goalVoxels.zVoxels, outcome.finalCells.front().zVoxels);
    for (const Protocol::VoxelPosition& cell : outcome.finalCells)
    {
      Assert::IsTrue(cell.zVoxels > Simulation::BreachRun::WALL_SOUTH_VOXELS);
    }
  }

  TEST_METHOD(NoUnitStandsOnAStructureOnAnyTick)
  {
    const Outcome& outcome = Breach();
    for (const std::string& fault : outcome.standingFaults)
    {
      Logger::WriteMessage(fault.c_str());
    }
    Assert::IsTrue(outcome.standingFaults.empty());
  }

  TEST_METHOD(ReplaysWithEveryStateHashEqual)
  {
    const Outcome& outcome = Breach();
    const Simulation::ReplayResult result = Simulation::Replay(outcome.replay, outcome.desc);
    Assert::IsTrue(result.matched);
    Assert::AreEqual(outcome.run.ticks, result.ticksReplayed);
  }

  TEST_METHOD(ContinuesIdenticallyFromSavesBeforeTheBreachMidFallAndMidWalk)
  {
    const Outcome& outcome = Breach();
    Assert::AreEqual(std::size_t{3}, outcome.saves.size());
    for (const auto& [tick, save] : outcome.saves)
    {
      Simulation::World loaded = Simulation::LoadSave(save, outcome.desc);
      Assert::AreEqual(tick, loaded.TickCount());
      Simulation::RunScript(loaded, outcome.run.script, outcome.run.ticks - tick);
      Assert::AreEqual(outcome.finalHash, Simulation::StateHash(loaded));
    }
  }

  TEST_METHOD(RecordsTheRateThroughTheBreachAndReplaysItToTheSameTicks)
  {
    // ADR-022, decision 7: the rate through the breach falls out of the rule, and is measured. The
    // tick each figure first stands inside is recorded, and the run continued from the save before
    // the breach brings each inside on the same tick.
    const Outcome& outcome = Breach();
    Assert::AreEqual(Simulation::BreachRun::SQUAD_FIGURES, outcome.inside.size());
    std::uint32_t first = UINT32_MAX;
    std::uint32_t last = 0;
    for (const auto& [id, tick] : outcome.inside)
    {
      first = std::min(first, tick);
      last = std::max(last, tick);
    }
    Logger::WriteMessage(("six figures inside from tick " + std::to_string(first) + " to tick " + std::to_string(last)).c_str());
    Assert::IsTrue(last > first);
    const auto& [saved, save] = outcome.saves.front();
    Simulation::World loaded = Simulation::LoadSave(save, outcome.desc);
    std::map<Protocol::EntityId, std::uint32_t> inside;
    for (std::uint32_t tick = saved; tick < outcome.run.ticks; ++tick)
    {
      Simulation::RunScript(loaded, outcome.run.script, 1);
      NoteInside(loaded, outcome.run.squad, tick, inside);
    }
    Assert::IsTrue(inside == outcome.inside);
  }

  TEST_METHOD(KeepsTwoSquadsOneToACellThroughTheBreach)
  {
    // ADR-022, *Consequences*: no two units in one cell on any tick of the breach run with two squads
    // ordered through it; both come to rest inside, each lead at its goal.
    const TwoSquads outcome = RunTwoSquads();
    for (const std::string& shared : outcome.sharedCells)
    {
      Logger::WriteMessage(shared.c_str());
    }
    Assert::IsTrue(outcome.sharedCells.empty());
    Assert::IsTrue(outcome.quiet);
    Assert::IsTrue(outcome.firstLead == outcome.firstGoal);
    Assert::IsTrue(outcome.secondLead == outcome.secondGoal);
    Assert::AreEqual(2 * Simulation::BreachRun::SQUAD_FIGURES, outcome.finalCells.size());
    for (const Protocol::VoxelPosition& cell : outcome.finalCells)
    {
      Assert::IsTrue(cell.zVoxels > Simulation::BreachRun::WALL_SOUTH_VOXELS);
    }
  }
};

} // namespace SimulationTests
