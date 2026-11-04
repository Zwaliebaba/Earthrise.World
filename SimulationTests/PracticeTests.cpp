#include "pch.h"

#include "Save.h"
#include "Scenario.h"
#include "ServerDataFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The Siege playtest's practice ground (M5-C2; GameDesign.md, M5's values, item 7): a scenario with no trial, the Siege
// castle's map with the attacker's army as it stands in the Siege camp, and no structure and no defender.

namespace SimulationTests
{

namespace
{

constexpr std::uint64_t BUILD_ID = 0xC2;
constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint32_t PRACTICE_TICKS = 100;

const Simulation::Scenario& Named(std::string_view _id)
{
  const Simulation::Scenario* scenario = BakedServerData().FindScenario(_id);
  if (scenario == nullptr)
  {
    throw std::runtime_error("the baked server data holds no scenario " + std::string(_id));
  }
  return *scenario;
}

Simulation::World Practice()
{
  Simulation::World world(Simulation::ScenarioDesc(BakedServerData(), "Practice", BUILD_ID));
  Simulation::LoadScenario(world, "Practice");
  return world;
}

} // namespace

TEST_CLASS(PracticeTests)
{
public:
  TEST_METHOD(LoadsTwiceToOneStateHashWithNoStructureAndNoTrial)
  {
    const Simulation::World first = Practice();
    const Simulation::World second = Practice();
    Assert::AreEqual(Simulation::StateHash(first), Simulation::StateHash(second));
    Assert::IsTrue(first.Structures().empty());
    Assert::IsFalse(first.Trial().has_value());
    Assert::IsFalse(Named("Practice").trial.has_value());
  }

  TEST_METHOD(HoldsTheSiegeCampsArmyAndNoDefender)
  {
    const Simulation::Scenario& practice = Named("Practice");
    const Simulation::Scenario& siege = Named("Siege");
    std::vector<const Simulation::ScenarioSquad*> camp;
    for (const Simulation::ScenarioSquad& squad : siege.squads)
    {
      if (squad.owner == ATTACKER)
      {
        camp.push_back(&squad);
      }
    }
    Assert::AreEqual(camp.size(), practice.squads.size());
    for (std::size_t index = 0; index < camp.size(); ++index)
    {
      Assert::IsTrue(camp.at(index)->kind == practice.squads.at(index).kind);
      Assert::IsTrue(camp.at(index)->cellsVoxels == practice.squads.at(index).cellsVoxels);
    }
    Assert::AreEqual(std::size_t{2}, practice.engines.size());
    Assert::AreEqual(std::size_t{1}, practice.players.size());
    Assert::IsTrue(practice.players.front().side == Protocol::Side::Attacker);
    Assert::IsTrue(practice.garrisons.empty() && practice.earthworks.empty());
    Assert::AreEqual(siege.worldSeed, practice.worldSeed);
  }

  TEST_METHOD(TicksWithNoOutcome)
  {
    Simulation::World world = Practice();
    for (std::uint32_t tick = 0; tick < PRACTICE_TICKS; ++tick)
    {
      world.Tick();
      Assert::IsTrue(std::ranges::none_of(world.LastTickEvents(), [](const Simulation::TrialEvent& _event)
                                          { return _event.kind == Simulation::TrialEventKind::Outcome; }));
    }
    Assert::IsFalse(world.Squads().empty());
  }
};

} // namespace SimulationTests
