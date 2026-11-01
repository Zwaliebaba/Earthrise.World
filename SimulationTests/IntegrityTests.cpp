#include "pch.h"

#include "FlatWorld.h"
#include "Integrity.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The integrity reading (M3-B5): the breach-prediction paper test's support readout, read from the
// world and the real resolver. Each of the paper's 16 damage states (Design/Review/PaperTests/
// BreachPrediction.md, *The 16 states*) is built by edits on a T3 section in a line of eight that
// spans the world, so a route can only cross the damaged one; its reading is asked for at once and
// answered at the end of the next tick, as a client's is, on the state as it stands, as the paper takes
// its readouts; then the world runs until it is quiet, and the voxels that fell and whether foot can
// cross are its Fell and its key.
//
// The real resolver agrees with the paper's readings in all 16 states, and with its Fell and key in 14.
// Two differ, as the paper's report now says, because the fall crushes every structure voxel in its
// strip below the rubble's top into rubble (GameDesign.md, M2's values, item 8), which the paper's
// stand-in modelled only as the first stone under each falling column:
//   - W08's stone sill is crushed into rubble, which is ground, so the window that drops its top opens;
//   - W12's piece, the paper's 7 voxels, falls along the wall and crushes its inner face, and an outer
//     merlon left without support falls in a cascade, 8 in all.

namespace SimulationTests
{

namespace
{

constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t SECTION_X = 24; // the fourth of eight sections, x 0 to 63
constexpr std::int32_t INNER_Z = 30;   // T3's back, its template's z 0
constexpr std::int32_t OUTER_Z = 31;   // its front, facing the attacker at +z
constexpr std::int32_t SECTIONS = 8;
constexpr std::int32_t SECTION_LENGTH = 8;

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

/// The paper's column c, 1 to 8 from the left as seen from outside, is the template file's column
/// c - 1, which the bake mirrors in x (ADR-017).
std::int32_t ColumnX(std::int32_t _column)
{
  return SECTION_X + SECTION_LENGTH - _column;
}

enum class Faces : std::uint8_t
{
  Both,
  Outer,
  Inner
};

std::vector<std::int32_t> LayersOf(Faces _faces)
{
  if (_faces == Faces::Outer)
  {
    return {OUTER_Z};
  }
  if (_faces == Faces::Inner)
  {
    return {INNER_Z};
  }
  return {OUTER_Z, INNER_Z};
}

struct Damage
{
  bool undercut;
  std::int32_t firstColumn;
  std::int32_t lastColumn;
  std::int32_t bottomRow; // for a hole
  std::int32_t topRow;
  Faces faces;
};

constexpr Damage Hole(std::int32_t _first, std::int32_t _last, std::int32_t _bottom, std::int32_t _top, Faces _faces = Faces::Both) noexcept
{
  return {.undercut = false, .firstColumn = _first, .lastColumn = _last, .bottomRow = _bottom, .topRow = _top, .faces = _faces};
}

constexpr Damage Undercut(std::int32_t _first, std::int32_t _last, Faces _faces = Faces::Both) noexcept
{
  return {.undercut = true, .firstColumn = _first, .lastColumn = _last, .bottomRow = 0, .topRow = 0, .faces = _faces};
}

constexpr Damage SHAVED = Hole(1, 8, 0, 4, Faces::Outer);

/// What the paper's table says of a state: the support readout's three readings and the resolution.
struct Expected
{
  const char* id;
  std::vector<Damage> damage;
  std::uint8_t holeWidth;
  bool holeFalls;
  bool hasOpening;
  std::uint8_t openingFloor;
  bool onStone;
  std::uint8_t openingClear;
  bool routeNow;
  std::uint8_t gapRun;
  bool gapFalls;
  std::size_t fell;
  bool open;
};

const std::vector<Expected>& States()
{
  static const std::vector<Expected> STATES = {
    {"W01", {Hole(3, 6, 0, 0)}, 4, false, true, 0, false, 1, false, 0, false, 0, false},
    {"W02", {Undercut(2, 7)}, 0, false, false, 0, false, 0, false, 6, true, 24, true},
    {"W03", {Hole(2, 6, 0, 0)}, 5, true, true, 0, false, 1, false, 0, false, 9, true},
    {"W04", {SHAVED}, 0, false, false, 0, false, 0, false, 0, false, 0, false},
    {"W05", {Hole(5, 5, 1, 4)}, 1, false, true, 1, true, 4, false, 0, false, 0, false},
    {"W06", {SHAVED, Undercut(2, 6)}, 0, false, false, 0, false, 0, false, 5, true, 8, true},
    {"W07", {Hole(6, 6, 0, 1)}, 1, false, true, 0, false, 2, true, 0, false, 0, true},
    {"W08", {Hole(2, 7, 2, 3)}, 6, true, true, 2, true, 2, false, 0, false, 8, true}, // the paper: shut
    {"W09", {Hole(1, 7, 0, 0)}, 7, true, true, 0, false, 1, false, 0, false, 31, true},
    {"W10", {Undercut(3, 6)}, 0, false, false, 0, false, 0, false, 4, false, 0, false},
    {"W11", {Hole(1, 3, 0, 0), Hole(5, 7, 0, 0)}, 3, false, true, 0, false, 1, false, 0, false, 0, false},
    {"W12", {SHAVED, Hole(3, 7, 0, 0, Faces::Inner)}, 5, true, true, 0, false, 1, false, 0, false, 8, true}, // the paper: 7
    {"W13", {Undercut(1, 5)}, 0, false, false, 0, false, 0, false, 5, true, 13, true},
    {"W14", {Hole(3, 5, 2, 3)}, 3, false, true, 2, true, 2, false, 0, false, 0, false},
    {"W15", {Hole(3, 8, 0, 0)}, 6, true, true, 0, false, 1, false, 0, false, 20, true},
    {"W16", {Undercut(1, 8, Faces::Outer)}, 0, false, false, 0, false, 0, false, 0, false, 0, false},
  };
  return STATES;
}

/// A line of eight T3 sections across the flat world, ticked once; returns the fourth's id.
Protocol::EntityId BuildLine(Simulation::World& _world)
{
  Protocol::EntityId section = Protocol::NO_ENTITY;
  for (std::int32_t index = 0; index < SECTIONS; ++index)
  {
    const Protocol::EntityId id =
      _world.AddStructure(2, "T3", At(index * SECTION_LENGTH, GROUND, INNER_Z), 0, "W" + std::to_string(index + 1));
    if (index * SECTION_LENGTH == SECTION_X)
    {
      section = id;
    }
  }
  _world.Tick();
  Assert::IsTrue(_world.Collapses().empty());
  return section;
}

void Apply(Simulation::World& _world, const Damage& _damage)
{
  for (std::int32_t column = _damage.firstColumn; column <= _damage.lastColumn; ++column)
  {
    const std::int32_t x = ColumnX(column);
    if (_damage.undercut)
    {
      // The ground under the foundation, 1 m deep, under the given faces, and the sappers' trench
      // along the outer foot, 2 m wide and 1 m deep, that took it.
      for (const std::int32_t z : LayersOf(_damage.faces))
      {
        _world.Voxels().Edit(At(x, GROUND - 1, z), 0);
      }
      _world.Voxels().Edit(At(x, GROUND - 1, OUTER_Z + 1), 0);
      _world.Voxels().Edit(At(x, GROUND - 1, OUTER_Z + 2), 0);
      continue;
    }
    for (std::int32_t row = _damage.bottomRow; row <= _damage.topRow; ++row)
    {
      for (const std::int32_t z : LayersOf(_damage.faces))
      {
        _world.Voxels().Edit(At(x, GROUND + row, z), 0);
      }
    }
  }
}

/// A state's reading as it stands, the voxels that fell before the world went quiet, and whether foot
/// can then cross the line from outside to inside.
struct Outcome
{
  Protocol::IntegrityReading reading;
  std::size_t fell;
  bool open;
  std::uint32_t searches; // the reading's, taken directly on the same state
};

Outcome Resolve(const Expected& _state)
{
  Simulation::World world(FlatWorld::Desc());
  const Protocol::EntityId section = BuildLine(world);
  for (const Damage& damage : _state.damage)
  {
    Apply(world, damage);
  }
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::QueryRequest{.id = 1, .query = Protocol::SectionIntegrity{.structure = section}}, writer);
  world.Ask(1, writer.Bytes());
  Outcome outcome{.reading = {}, .fell = 0, .open = false, .searches = 0};
  static_cast<void>(Simulation::ReadIntegrity(world, section, &outcome.searches));
  std::set<Protocol::EntityId> seen;
  for (int tick = 0; tick < 3000; ++tick)
  {
    world.Tick();
    if (tick == 0)
    {
      const std::vector<std::vector<std::byte>> answers = world.TakeAnswers();
      Assert::AreEqual(std::size_t{1}, answers.size());
      Core::ByteReader reader(answers.front());
      outcome.reading = std::get<Protocol::IntegrityReading>(Protocol::DecodeAnswer(reader).reading);
    }
    for (const auto& [id, collapse] : world.Collapses())
    {
      if (seen.insert(id).second)
      {
        outcome.fell += collapse.piece.size();
      }
    }
    if (world.Quiet())
    {
      break;
    }
  }
  Assert::IsTrue(world.Quiet());
  const Simulation::NavigationGrid& navigation = world.Navigation();
  const std::int64_t width = navigation.WidthCells();
  const std::int64_t outside = 40 * width + 28;
  const std::int64_t inside = 20 * width + 28;
  outcome.open = navigation.NearestReachable(outside, inside, Simulation::NavigationClass::Foot) == inside;
  return outcome;
}

const std::vector<Outcome>& Outcomes()
{
  static const std::vector<Outcome> OUTCOMES = []
  {
    std::vector<Outcome> outcomes;
    for (const Expected& state : States())
    {
      outcomes.push_back(Resolve(state));
    }
    return outcomes;
  }();
  return OUTCOMES;
}

} // namespace

TEST_CLASS(IntegrityTests)
{
public:
  TEST_METHOD(ReadsThePaperTestsSixteenStatesAsItsTableDoes)
  {
    std::vector<std::string> faults;
    for (std::size_t index = 0; index < States().size(); ++index)
    {
      const Expected& state = States()[index];
      const Protocol::IntegrityReading& reading = Outcomes()[index].reading;
      const bool holeFalls = reading.holeFallWidthVoxels != 0 && reading.holeWidthVoxels >= reading.holeFallWidthVoxels;
      const bool gapFalls = reading.gapFallRunVoxels != 0 && reading.gapRunVoxels >= reading.gapFallRunVoxels;
      const bool onStone = reading.openingFloor == Protocol::OpeningFloor::Stone;
      std::string fault;
      if (reading.holeWidthVoxels != state.holeWidth || (state.holeWidth > 0 && holeFalls != state.holeFalls))
      {
        fault += " hole " + std::to_string(reading.holeWidthVoxels) + (holeFalls ? " falls" : " holds") + " at " +
                 std::to_string(reading.holeFallWidthVoxels) + ";";
      }
      if (reading.hasOpening != state.hasOpening ||
          (state.hasOpening && (reading.openingFloorVoxels != state.openingFloor || onStone != state.onStone ||
                                reading.openingClearVoxels != state.openingClear || reading.routeNow != state.routeNow)))
      {
        fault += " opening floor " + std::to_string(reading.openingFloorVoxels) + (onStone ? " stone" : " ground") + " clear " +
                 std::to_string(reading.openingClearVoxels) + (reading.routeNow ? " route" : "") + ";";
      }
      if (reading.gapRunVoxels != state.gapRun || (state.gapRun > 0 && gapFalls != state.gapFalls))
      {
        fault += " gap " + std::to_string(reading.gapRunVoxels) + (gapFalls ? " falls" : " holds") + " at " +
                 std::to_string(reading.gapFallRunVoxels) + ";";
      }
      if (!fault.empty())
      {
        faults.push_back(std::string(state.id) + ":" + fault);
      }
    }
    for (const std::string& fault : faults)
    {
      Logger::WriteMessage(fault.c_str());
    }
    Assert::IsTrue(faults.empty());
  }

  TEST_METHOD(DropsWhatThePaperTestsTableSaysFellAndKeepsItsKey)
  {
    std::vector<std::string> faults;
    for (std::size_t index = 0; index < States().size(); ++index)
    {
      const Expected& state = States()[index];
      const Outcome& outcome = Outcomes()[index];
      if (outcome.fell != state.fell || outcome.open != state.open)
      {
        faults.push_back(std::string(state.id) + ": fell " + std::to_string(outcome.fell) + ", " + (outcome.open ? "open" : "shut"));
      }
    }
    for (const std::string& fault : faults)
    {
      Logger::WriteMessage(fault.c_str());
    }
    Assert::IsTrue(faults.empty());
  }

  TEST_METHOD(RunsAtMostEightSupportSearches)
  {
    // What the query budget charges a section's integrity (ADR-020, decision 6).
    for (const Outcome& outcome : Outcomes())
    {
      Assert::IsTrue(outcome.searches <= 8);
    }
  }

  TEST_METHOD(ReadsAHoleBridgedAtBothEndsFallingAtFiveAndOneAgainstAPillarSooner)
  {
    // The paper's ambiguity 3: 2 x span + 1 is the fall width only for a hole bridged at both ends.
    const std::array<Expected, 2> states = {{
      {"bridged", {Hole(3, 6, 0, 0)}, 4, false, true, 0, false, 1, false, 0, false, 0, false},
      {"cut", {Hole(3, 6, 0, 0), Hole(2, 2, 1, 1)}, 4, false, true, 0, false, 1, false, 0, false, 0, false},
    }};
    Assert::AreEqual(std::uint8_t{5}, Resolve(states[0]).reading.holeFallWidthVoxels);
    Assert::IsTrue(Resolve(states[1]).reading.holeFallWidthVoxels < 5);
  }

  TEST_METHOD(ReadsTheGroundUnderTheSection)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId onEarth = world.AddStructure(2, "T3", At(4, GROUND, 4), 0, "W1");
    const Protocol::EntityId onBedrock = world.AddStructure(2, "T3", At(4, FlatWorld::BEDROCK_VOXELS, 40), 0, "W2");
    for (std::int32_t z = 40; z < 42; ++z)
    {
      for (std::int32_t x = 4; x < 12; ++x)
      {
        for (std::int32_t y = FlatWorld::BEDROCK_VOXELS; y < GROUND; ++y)
        {
          if (world.Voxels().At(At(x, y, z)) == FlatWorld::EARTH)
          {
            world.Voxels().Edit(At(x, y, z), 0);
          }
        }
      }
    }
    world.Tick();
    Assert::IsTrue(Simulation::ReadIntegrity(world, onEarth).ground == Protocol::GroundKind::Earth);
    Assert::IsTrue(Simulation::ReadIntegrity(world, onEarth).canBeUndermined);
    Assert::IsTrue(Simulation::ReadIntegrity(world, onBedrock).ground == Protocol::GroundKind::Bedrock);
    Assert::IsFalse(Simulation::ReadIntegrity(world, onBedrock).canBeUndermined);
  }

  TEST_METHOD(ChangesNothingAndReadsTheSameTwice)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId section = BuildLine(world);
    Apply(world, Hole(3, 6, 0, 0));
    const std::uint64_t before = world.Voxels().VoxelHash();
    const Protocol::IntegrityReading first = Simulation::ReadIntegrity(world, section);
    Assert::AreEqual(before, world.Voxels().VoxelHash());
    Assert::IsTrue(first == Simulation::ReadIntegrity(world, section));
    Assert::ExpectException<std::out_of_range>([&world] { static_cast<void>(Simulation::ReadIntegrity(world, 999)); });
  }
};

} // namespace SimulationTests
