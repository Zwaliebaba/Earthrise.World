#include "pch.h"

#include "Save.h"
#include "Scenario.h"
#include "ServerDataFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The Siege castle as a scenario (M4-B4; ADR-026, decisions 1 to 3), loaded from the baked file.

namespace SimulationTests
{

namespace
{

constexpr std::uint64_t BUILD_ID = 0xB4;
constexpr std::uint8_t GRASS = 1;
constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t COMMON_ROCK = 3;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t TIMBER = 7;

const Simulation::Scenario& Siege()
{
  const Simulation::Scenario* scenario = BakedServerData().FindScenario("Siege");
  if (scenario == nullptr)
  {
    throw std::runtime_error("the baked server data holds no Siege scenario");
  }
  return *scenario;
}

/// The castle, loaded on a world from its Desc and not yet ticked.
Simulation::World Castle()
{
  Simulation::World world(Simulation::ScenarioDesc(BakedServerData(), "Siege", BUILD_ID));
  Simulation::LoadScenario(world, "Siege");
  return world;
}

const Simulation::TrialRules& TrialOf(const Simulation::World& _world)
{
  const std::optional<Simulation::TrialRules>& trial = _world.Trial();
  if (!trial.has_value())
  {
    throw std::runtime_error("the world has no trial");
  }
  return *trial;
}

const Simulation::Structure& Named(const Simulation::World& _world, std::string_view _name)
{
  const auto found = std::ranges::find_if(_world.Structures(), [_name](const auto& _entry) { return _entry.second.name == _name; });
  if (found == _world.Structures().end())
  {
    throw std::runtime_error("the castle has no structure " + std::string(_name));
  }
  return found->second;
}

/// A structure's footprint after its turn: its least and greatest x and z, both included.
std::array<std::int32_t, 4> Footprint(const Simulation::World& _world, const Simulation::Structure& _structure)
{
  const Simulation::ServerTemplate* shape = _world.Data().FindTemplate(_structure.templateId);
  const bool across = _structure.quarterTurns % 2 == 1;
  const auto width = static_cast<std::int32_t>(across ? shape->depthVoxels : shape->widthVoxels);
  const auto depth = static_cast<std::int32_t>(across ? shape->widthVoxels : shape->depthVoxels);
  const Protocol::VoxelPosition& origin = _structure.originVoxels;
  return {origin.xVoxels, origin.zVoxels, origin.xVoxels + width - 1, origin.zVoxels + depth - 1};
}

/// The squared distance across the ground from the camp's middle to the nearest column of a structure's footprint.
std::int64_t SquaredDistanceFromCamp(const Simulation::World& _world, const Simulation::Structure& _structure)
{
  const Simulation::TrialRules& trial = TrialOf(_world);
  const std::int64_t campX = (std::int64_t{trial.campMinVoxels.xVoxels} + trial.campMaxVoxels.xVoxels) / 2;
  const std::int64_t campZ = (std::int64_t{trial.campMinVoxels.zVoxels} + trial.campMaxVoxels.zVoxels) / 2;
  const auto [minX, minZ, maxX, maxZ] = Footprint(_world, _structure);
  const std::int64_t dx = std::max<std::int64_t>({minX - campX, 0, campX - maxX});
  const std::int64_t dz = std::max<std::int64_t>({minZ - campZ, 0, campZ - maxZ});
  return (dx * dx) + (dz * dz);
}

std::uint8_t At(const Simulation::World& _world, std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return _world.Voxels().At({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z});
}

} // namespace

TEST_CLASS(ScenarioTests)
{
public:
  TEST_METHOD(BuildsTheCastlesStructuresWithTheirNames)
  {
    const Simulation::World world = Castle();
    std::map<std::string, std::vector<std::string>> byTemplate;
    for (const auto& [id, structure] : world.Structures())
    {
      byTemplate[structure.templateId].push_back(structure.name);
    }
    std::vector<std::string> sections = byTemplate["T3"];
    std::ranges::sort(sections);
    std::vector<std::string> expected;
    for (const char side : {'E', 'N', 'S', 'W'})
    {
      for (int number = 1; number <= (side == 'S' ? 4 : 6); ++number)
      {
        expected.push_back(std::string(1, side) + std::to_string(number));
      }
    }
    std::ranges::sort(expected);
    Assert::IsTrue(expected == sections);
    std::vector<std::string> towers = byTemplate["T4"];
    std::ranges::sort(towers);
    Assert::IsTrue(towers == std::vector<std::string>{"NE", "NW", "SE", "ST", "SW"});
    Assert::IsTrue(byTemplate["T5"] == std::vector<std::string>{"GH"});
    Assert::IsTrue(byTemplate["T2"] == std::vector<std::string>{"KEEP"});
    Assert::IsTrue(byTemplate["T1"] == std::vector<std::string>{"HALL"});
    Assert::AreEqual(std::size_t{30}, world.Structures().size());
    Assert::AreEqual(Named(world, "KEEP").id, TrialOf(world).keep);
  }

  TEST_METHOD(PutsS3NearestTheCampOnRockBehindTheDitch)
  {
    const Simulation::World world = Castle();
    const Simulation::Structure& s3 = Named(world, "S3");
    for (const auto& [id, structure] : world.Structures())
    {
      if (structure.templateId == "T3" && structure.name != "S3")
      {
        Assert::IsTrue(SquaredDistanceFromCamp(world, s3) < SquaredDistanceFromCamp(world, structure),
                       std::wstring(structure.name.begin(), structure.name.end()).c_str());
      }
    }
    // Its 1 m foundation stands on common rock, and the ditch is dug before its south face.
    const auto [minX, minZ, maxX, maxZ] = Footprint(world, s3);
    const std::int32_t ground = Siege().groundVoxels;
    for (std::int32_t x = minX; x <= maxX; ++x)
    {
      for (std::int32_t z = minZ; z <= maxZ; ++z)
      {
        Assert::AreEqual(MASONRY, At(world, x, ground - 1, z));
        Assert::AreEqual(COMMON_ROCK, At(world, x, ground - 2, z));
      }
      Assert::AreEqual(Protocol::Chunk::EMPTY, At(world, x, ground - 1, minZ - 10)); // the ditch, 4 to 8 m before the wall's
      Assert::AreEqual(EARTH, At(world, x, ground, minZ - 5));                       // the rampart before it, 1 to 4 m out
    }
  }

  TEST_METHOD(PutsE3OnDeepEarthWithNoDitchFartherThanS3)
  {
    const Simulation::World world = Castle();
    const Simulation::Structure& e3 = Named(world, "E3");
    Assert::IsTrue(SquaredDistanceFromCamp(world, Named(world, "S3")) < SquaredDistanceFromCamp(world, e3));
    const auto [minX, minZ, maxX, maxZ] = Footprint(world, e3);
    const std::int32_t ground = Siege().groundVoxels;
    for (std::int32_t z = minZ; z <= maxZ; ++z)
    {
      for (std::int32_t x = minX; x <= maxX; ++x)
      {
        // Its foundation reaches 4 m down (the paper tests' values, item 16), and stands on earth.
        for (std::int32_t depth = 1; depth <= 4; ++depth)
        {
          Assert::AreEqual(MASONRY, At(world, x, ground - depth, z));
        }
        Assert::AreEqual(EARTH, At(world, x, ground - 5, z));
      }
      // Before its east face, 1 to 9 m out, the ground is untouched: no rampart, no ditch.
      for (std::int32_t x = maxX + 1; x <= maxX + 12; ++x)
      {
        Assert::AreEqual(GRASS, At(world, x, ground - 1, z));
        Assert::AreEqual(Protocol::Chunk::EMPTY, At(world, x, ground, z));
      }
    }
  }

  TEST_METHOD(PutsTheGatehousesGateAtItsPassagesInnerEnd)
  {
    // GH faces the camp, south, so its passage's inner end is its footprint's north row.
    const Simulation::World world = Castle();
    const auto [minX, minZ, maxX, maxZ] = Footprint(world, Named(world, "GH"));
    const std::int32_t ground = Siege().groundVoxels;
    for (std::int32_t x = minX + 3; x <= minX + 5; ++x)
    {
      for (std::int32_t y = ground; y < ground + 4; ++y)
      {
        Assert::AreEqual(TIMBER, At(world, x, y, maxZ));
        for (std::int32_t z = minZ; z < maxZ; ++z)
        {
          Assert::AreEqual(Protocol::Chunk::EMPTY, At(world, x, y, z));
        }
      }
    }
  }

  TEST_METHOD(LoadsTwiceToOneStateHash)
  {
    Assert::AreEqual(Simulation::StateHash(Castle()), Simulation::StateHash(Castle()));
  }

  TEST_METHOD(LoadsASaveAtTickZeroToTheSameHash)
  {
    const Simulation::World world = Castle();
    const Simulation::World loaded =
      Simulation::LoadSave(Simulation::WriteSave(world), Simulation::ScenarioDesc(BakedServerData(), "Siege", BUILD_ID));
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(loaded));
    Assert::AreEqual(std::size_t{2}, loaded.Players().size());
    Assert::AreEqual(std::int32_t{12000}, loaded.Players().at(1).stoneHundredthsCubicMeters); // 120 m3 (input 5)
    Assert::AreEqual(std::uint32_t{48000}, TrialOf(loaded).timeLimitTicks);                   // 40 minutes (input 16)
  }

  TEST_METHOD(PlacesEverySquadOnWalkableDistinctCells)
  {
    Simulation::World world = Castle();
    world.Tick(); // step 8 brings navigation up to the levelled site
    std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> taken;
    for (const Simulation::ScenarioSquad& squad : Siege().squads)
    {
      for (const Protocol::VoxelPosition& cell : squad.cellsVoxels)
      {
        Assert::AreEqual(static_cast<std::int16_t>(cell.yVoxels), world.Navigation().WalkableHeight(cell.xVoxels, cell.zVoxels));
        Assert::IsTrue(world.Navigation().Usable(cell.xVoxels, cell.zVoxels, world.Data().Stats(squad.kind).navigationClass));
        Assert::IsTrue(taken.insert({cell.xVoxels, cell.yVoxels, cell.zVoxels}).second);
      }
    }
    // Nothing moved on the first tick: every figure stood where the scenario put it.
    for (const auto& [id, unit] : world.Units())
    {
      if (world.Squads().at(unit.squad).garrisonedIn == Protocol::NO_ENTITY)
      {
        Assert::IsTrue(taken.contains({unit.cellVoxels.xVoxels, unit.cellVoxels.yVoxels, unit.cellVoxels.zVoxels}));
      }
    }
  }

  TEST_METHOD(HoldsInputFivesArmiesAndGarrisons)
  {
    const Simulation::World world = Castle();
    std::map<std::uint8_t, std::size_t> figures;
    std::size_t garrisoned = 0;
    for (const auto& [id, squad] : world.Squads())
    {
      figures[squad.owner] += squad.units.size();
      garrisoned += squad.garrisonedIn != Protocol::NO_ENTITY ? squad.units.size() : 0;
    }
    Assert::AreEqual(std::size_t{72}, figures[1]);             // the attacker's soldiers, its engines apart
    Assert::AreEqual(std::size_t{42 + 6 + 3 + 6}, figures[2]); // seven garrisons of six, the sally, the reserve, six laborers
    Assert::AreEqual(std::size_t{42}, garrisoned);
    Assert::AreEqual(std::size_t{2}, world.SiegeEngines().size());
    for (const auto& [id, engine] : world.SiegeEngines())
    {
      Assert::IsTrue(engine.state == Protocol::EngineState::Packed);
    }
  }
};

} // namespace SimulationTests
