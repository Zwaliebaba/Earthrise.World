#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"
#include "Scenario.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Fog of war (M4-E1; ADR-025): each player sees by radius from their units and structures, walls blocking nothing; their
// update holds the others' units only where they see, and the others' structures as they last saw them; they are sent a
// chunk only while they see its column; an answer about what they do not see says so; a scenario may reveal the map for
// its first revealTicks. A world with players is fogged; one without is not, as M3's was.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr Protocol::VoxelPosition TOWER{.xVoxels = 90, .yVoxels = GROUND, .zVoxels = 90}; // a T4, x and z 90 to 96

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

/// A world 128 m square with an attacker, whose map is revealed when _revealed, and a defender.
struct Match
{
  Simulation::World world{FlatWorld::Desc(4, 4)};

  explicit Match(bool _revealed = false)
  {
    world.AddPlayer({.id = ATTACKER,
                     .side = Protocol::Side::Attacker,
                     .stoneHundredthsCubicMeters = 12000,
                     .timberHundredthsCubicMeters = 0,
                     .revealed = _revealed});
    world.AddPlayer(
      {.id = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 20000, .timberHundredthsCubicMeters = 2000});
  }
};

Protocol::EntityId One(Simulation::World& _world, std::uint8_t _owner, Simulation::UnitKind _kind, Protocol::VoxelPosition _cell)
{
  const std::array<Protocol::VoxelPosition, 1> cells = {_cell};
  return _world.AddSquad(_owner, _kind, cells);
}

bool Holds(const std::vector<Protocol::UnitView>& _units, std::uint8_t _owner)
{
  return std::ranges::any_of(_units, [_owner](const Protocol::UnitView& _unit) { return _unit.owner == _owner; });
}

const Protocol::StructureView* Find(const Protocol::StateUpdate& _state, Protocol::EntityId _structure)
{
  const auto found = std::ranges::find(_state.structures, _structure, &Protocol::StructureView::id);
  return found == _state.structures.end() ? nullptr : &*found;
}

/// Whether the chunks sent hold the column of (_x, _z).
bool Sends(const std::vector<Protocol::ChunkSnapshot>& _chunks, std::int32_t _x, std::int32_t _z)
{
  return std::ranges::any_of(_chunks,
                             [_x, _z](const Protocol::ChunkSnapshot& _chunk) {
                               return _chunk.coordinate.xChunks == _x / Protocol::CHUNK_EDGE_VOXELS &&
                                      _chunk.coordinate.zChunks == _z / Protocol::CHUNK_EDGE_VOXELS;
                             });
}

/// The answer to an integrity query of _player's about _structure, asked before a tick and answered in it.
Protocol::Answer Integrity(Simulation::World& _world, std::uint8_t _player, Protocol::EntityId _structure)
{
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::QueryRequest{.id = 9, .query = Protocol::SectionIntegrity{.structure = _structure}}, writer);
  _world.Ask(_player, writer.Bytes());
  _world.Tick();
  const std::vector<std::vector<std::byte>> answers = _world.TakeAnswers();
  Assert::AreEqual(std::size_t{1}, answers.size());
  Core::ByteReader reader(answers.front());
  return Protocol::DecodeAnswer(reader);
}

/// T4's top layer, its twelve merlons, all of its critical part.
void CutMerlons(Simulation::World& _world)
{
  const Simulation::ServerTemplate* found = _world.Data().FindTemplate("T4");
  Assert::IsNotNull(found);
  const std::uint32_t y = found->heightVoxels - 1;
  for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
  {
    for (std::uint32_t x = 0; x < found->widthVoxels; ++x)
    {
      const std::size_t index =
        x + (static_cast<std::size_t>(found->widthVoxels) * (z + (static_cast<std::size_t>(found->depthVoxels) * y)));
      if (found->indices.at(index) != Protocol::Chunk::EMPTY)
      {
        _world.Voxels().Edit(At(TOWER.xVoxels + static_cast<std::int32_t>(x), TOWER.yVoxels + static_cast<std::int32_t>(y),
                                TOWER.zVoxels + static_cast<std::int32_t>(z)),
                             Protocol::Chunk::EMPTY);
      }
    }
  }
}

} // namespace

TEST_CLASS(FogTests)
{
public:
  TEST_METHOD(AnEnemySquad33MFromTheAttackersArchersIsHiddenAndOne31MOffIsSeen)
  {
    // An archer sees 32 m, by squared distance in whole cells.
    for (const std::int32_t metres : {33, 31})
    {
      Match match;
      One(match.world, ATTACKER, Simulation::UnitKind::Archer, At(20, GROUND, 20));
      const Protocol::EntityId spearmen = One(match.world, DEFENDER, Simulation::UnitKind::Spearman, At(20, GROUND, 20 + metres));
      match.world.Tick();
      const Protocol::StateUpdate& state = match.world.State(ATTACKER);
      const bool squadSeen =
        std::ranges::any_of(state.squads, [spearmen](const Protocol::SquadView& _squad) { return _squad.id == spearmen; });
      Assert::AreEqual(metres == 31, Holds(state.units, DEFENDER));
      Assert::AreEqual(metres == 31, squadSeen);
      Assert::IsTrue(Holds(match.world.State(DEFENDER).units, DEFENDER));
      Assert::AreEqual(std::size_t{256}, state.visibleCells.size()); // 128 x 128 cells, 64 a word
    }
  }

  TEST_METHOD(AGarrisonsCountIsItsOwnersAlone)
  {
    // A spearman of the attacker's beside the tower sees it, and neither its garrison nor its count.
    Match match;
    Simulation::World& world = match.world;
    const Protocol::EntityId tower = world.AddStructure(DEFENDER, "T4", TOWER, 0, "NE");
    const std::array<Protocol::VoxelPosition, 2> cells = {At(80, GROUND, 80), At(80, GROUND, 81)};
    const Protocol::EntityId archers = world.AddSquad(DEFENDER, Simulation::UnitKind::Archer, cells);
    world.PlaceInGarrison(archers, tower);
    One(world, ATTACKER, Simulation::UnitKind::Spearman, At(88, GROUND, 93));
    world.Tick();
    const Protocol::StructureView* own = Find(world.State(DEFENDER), tower);
    const Protocol::StructureView* seen = Find(world.State(ATTACKER), tower);
    Assert::IsTrue(own != nullptr && own->garrisonCount == 2);
    Assert::IsTrue(seen != nullptr && seen->garrisonCount == 0 && !seen->lastSeen);
    Assert::IsTrue(
      std::ranges::none_of(world.State(ATTACKER).squads, [archers](const Protocol::SquadView& _squad) { return _squad.id == archers; }));
  }

  TEST_METHOD(ASectionCrateredOutOfSightKeepsItsLastSeenViewAndChunksUntilSeen)
  {
    // The attacker's map revealed for 40 ticks shows them the tower whole; then fog falls, its merlons go, and they keep
    // the tower as they saw it, and are sent none of its chunks, until a spearman of theirs stands beside it.
    Match match(true);
    Simulation::World& world = match.world;
    const Protocol::EntityId tower = world.AddStructure(DEFENDER, "T4", TOWER, 0, "NE");
    One(world, ATTACKER, Simulation::UnitKind::Spearman, At(10, GROUND, 10));
    bool sentDuringReveal = false;
    const auto revealTicks = static_cast<std::uint32_t>(world.Data().Tuning().revealTicks);
    for (std::uint32_t tick = 0; tick <= revealTicks; ++tick)
    {
      world.Tick();
      sentDuringReveal = Sends(world.Publish(ATTACKER), TOWER.xVoxels, TOWER.zVoxels) || sentDuringReveal;
    }
    Assert::IsTrue(sentDuringReveal);
    const Protocol::StructureView* before = Find(world.State(ATTACKER), tower);
    Assert::IsTrue(before != nullptr && before->lastSeen && before->criticalPercent == 100);

    CutMerlons(world);
    for (std::uint32_t tick = 0; tick < 5; ++tick)
    {
      world.Tick();
      Assert::IsFalse(Sends(world.Publish(ATTACKER), TOWER.xVoxels, TOWER.zVoxels));
    }
    Assert::IsTrue(world.Structures().at(tower).criticalPercent < 100);
    const Protocol::StructureView* kept = Find(world.State(ATTACKER), tower);
    Assert::IsTrue(kept != nullptr && kept->lastSeen && kept->criticalPercent == 100);
    Assert::AreEqual(world.Structures().at(tower).criticalPercent, Find(world.State(DEFENDER), tower)->criticalPercent);

    One(world, ATTACKER, Simulation::UnitKind::Spearman, At(88, GROUND, 93));
    world.Tick();
    Assert::IsTrue(Sends(world.Publish(ATTACKER), TOWER.xVoxels, TOWER.zVoxels));
    const Protocol::StructureView* now = Find(world.State(ATTACKER), tower);
    Assert::IsTrue(now != nullptr && !now->lastSeen && now->criticalPercent == world.Structures().at(tower).criticalPercent);
  }

  TEST_METHOD(ARevealedMapSendsEveryChunkAndStructureOnceButNoDefenderUnit)
  {
    // The Siege trial reveals the map to its attacker: in its first revealTicks they are sent every chunk and every
    // structure, and an integrity reading of S3, but never a figure of the defender's, all of whom stand in the castle,
    // beyond every attacker's sight from the camp.
    Simulation::World world(Simulation::ScenarioDesc(BakedServerData(), "Siege", FlatWorld::BUILD_ID));
    Simulation::LoadScenario(world, "Siege");
    Protocol::EntityId s3 = Protocol::NO_ENTITY;
    for (const auto& [id, structure] : world.Structures())
    {
      s3 = structure.name == "S3" ? id : s3;
    }
    world.Tick();
    const std::vector<Protocol::ChunkSnapshot> first = world.Publish(ATTACKER);
    Assert::AreEqual(world.Voxels().Store().Snapshots().size(), first.size());
    Assert::AreEqual(world.Structures().size(), world.State(ATTACKER).structures.size());
    Assert::IsFalse(Holds(world.State(ATTACKER).units, DEFENDER));
    Assert::IsFalse(Integrity(world, ATTACKER, s3).notVisible);
    Assert::IsTrue(world.Publish(ATTACKER).size() < first.size()); // each sent once, only a changed one again
    const auto revealTicks = static_cast<std::uint32_t>(world.Data().Tuning().revealTicks);
    while (world.TickCount() <= revealTicks)
    {
      world.Tick();
      Assert::IsFalse(Holds(world.State(ATTACKER).units, DEFENDER));
    }
    const Protocol::StructureView* section = Find(world.State(ATTACKER), s3);
    Assert::IsTrue(section != nullptr && section->lastSeen);
    Assert::AreEqual(world.Structures().size(), world.State(ATTACKER).structures.size());
    const Protocol::Answer late = Integrity(world, ATTACKER, s3);
    Assert::IsTrue(late.notVisible);
  }

  TEST_METHOD(AnIntegrityQueryOnASectionOutOfSightAnswersNotVisible)
  {
    Match match;
    Simulation::World& world = match.world;
    const Protocol::EntityId section = world.AddStructure(DEFENDER, "T3", At(100, GROUND, 100), 0, "S1");
    One(world, ATTACKER, Simulation::UnitKind::Spearman, At(10, GROUND, 10));
    world.Tick();
    const Protocol::Answer hidden = Integrity(world, ATTACKER, section);
    Assert::IsTrue(hidden.notVisible);
    const auto& reading = std::get<Protocol::IntegrityReading>(hidden.reading);
    Assert::AreEqual(section, reading.structure);
    Assert::AreEqual(std::uint8_t{0}, reading.holeWidthVoxels);
    Assert::IsFalse(Integrity(world, DEFENDER, section).notVisible);
  }

  TEST_METHOD(ASaveAndLoadFiltersTheSame)
  {
    // Saved after the reveal, with the tower last seen and chunks sent: the loaded world's next update and chunks for the
    // attacker are the original's, and so is its hash.
    Match match(true);
    Simulation::World& world = match.world;
    world.AddStructure(DEFENDER, "T4", TOWER, 0, "NE");
    One(world, ATTACKER, Simulation::UnitKind::Spearman, At(10, GROUND, 10));
    One(world, DEFENDER, Simulation::UnitKind::Spearman, At(16, GROUND, 18));
    const auto revealTicks = static_cast<std::uint32_t>(world.Data().Tuning().revealTicks);
    for (std::uint32_t tick = 0; tick <= revealTicks + 2; ++tick)
    {
      world.Tick();
      static_cast<void>(world.Publish(ATTACKER));
    }
    CutMerlons(world);
    Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(world), FlatWorld::Desc(4, 4));
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(loaded));
    world.Tick();
    loaded.Tick();
    Assert::IsTrue(world.State(ATTACKER) == loaded.State(ATTACKER));
    const std::vector<Protocol::ChunkSnapshot> sent = world.Publish(ATTACKER);
    const std::vector<Protocol::ChunkSnapshot> sentLoaded = loaded.Publish(ATTACKER);
    Assert::AreEqual(sent.size(), sentLoaded.size());
    for (std::size_t chunk = 0; chunk < sent.size(); ++chunk)
    {
      Assert::IsTrue(sent[chunk].coordinate == sentLoaded[chunk].coordinate &&
                     std::ranges::equal(sent[chunk].voxels->Voxels(), sentLoaded[chunk].voxels->Voxels()));
    }
    Assert::IsTrue(Holds(world.State(ATTACKER).units, DEFENDER));
  }
};

} // namespace SimulationTests
