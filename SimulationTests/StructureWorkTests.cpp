#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Structures at work (M4-D1; ADR-024, decisions 1 and 2): a structure records which of its voxels were built, and works
// while the share of its critical part's built voxels that stand is at least its template's threshold. T4's firing
// platform is 110 voxels on its top three layers, 49, 49 and 12 merlons, and works at 75 % (MvpPlan.md, M4, input 10):
// 83 standing is 75 %, 82 is 74 %.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t DEFENDER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr Protocol::VoxelPosition ORIGIN{.xVoxels = 20, .yVoxels = GROUND, .zVoxels = 20};

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

/// The world voxels of a template's part, unturned at ORIGIN, top layer first and then in the template's order.
std::vector<Protocol::VoxelPosition> PartVoxels(const Simulation::World& _world, std::string_view _template,
                                                Simulation::ServerTemplate::Part _part)
{
  const Simulation::ServerTemplate* found = _world.Data().FindTemplate(_template);
  Assert::IsNotNull(found);
  std::vector<Protocol::VoxelPosition> voxels;
  for (std::uint32_t y = found->heightVoxels; y-- > 0;)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x)
      {
        const std::size_t index =
          x + (static_cast<std::size_t>(found->widthVoxels) * (z + (static_cast<std::size_t>(found->depthVoxels) * y)));
        if (found->parts.at(index) == _part && found->indices.at(index) != 0)
        {
          voxels.push_back(At(ORIGIN.xVoxels + static_cast<std::int32_t>(x), ORIGIN.yVoxels + static_cast<std::int32_t>(y),
                              ORIGIN.zVoxels + static_cast<std::int32_t>(z)));
        }
      }
    }
  }
  return voxels;
}

bool Raised(const Simulation::World& _world, Simulation::TrialEventKind _kind, Protocol::EntityId _structure)
{
  return std::ranges::any_of(_world.LastTickEvents(), [_kind, _structure](const Simulation::TrialEvent& _event)
                             { return _event.kind == _kind && _event.subject == _structure; });
}

} // namespace

TEST_CLASS(StructureWorkTests)
{
public:
  TEST_METHOD(AT4WorksWholeAndStopsWhenItsPlatformFallsBelowItsThreshold)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId tower = world.AddStructure(DEFENDER, "T4", ORIGIN, 0, "NE");
    world.Tick();
    Assert::IsTrue(world.Structures().at(tower).working);
    Assert::AreEqual(std::uint8_t{100}, world.Structures().at(tower).criticalPercent);
    const std::vector<Protocol::VoxelPosition> platform = PartVoxels(world, "T4", Simulation::ServerTemplate::Part::Critical);
    Assert::AreEqual(std::size_t{110}, platform.size());
    // The merlons and the top floor from the top down, so that nothing left loses its support.
    for (std::size_t removed = 0; removed < 27; ++removed)
    {
      world.Voxels().Edit(platform[removed], 0);
    }
    world.Tick();
    Assert::AreEqual(std::uint8_t{75}, world.Structures().at(tower).criticalPercent);
    Assert::IsTrue(world.Structures().at(tower).working);
    world.Voxels().Edit(platform[27], 0);
    world.Tick();
    Assert::AreEqual(std::uint8_t{74}, world.Structures().at(tower).criticalPercent);
    Assert::IsFalse(world.Structures().at(tower).working);
    Assert::IsTrue(Raised(world, Simulation::TrialEventKind::StructureStopped, tower));
    const auto view = std::ranges::find(world.State(1).structures, tower, &Protocol::StructureView::id);
    Assert::IsTrue(view != world.State(1).structures.end() && !view->working && view->criticalPercent == 74);
  }

  TEST_METHOD(DoesNotCountVoxelsHeldByACollapseInWarningAsStanding)
  {
    // The ring of wall on T4's eighth layer cut away: everything above it, the platform with it, has lost its support and
    // starts to warn. Its voxels are all still in the world, and none of them stands.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId tower = world.AddStructure(DEFENDER, "T4", ORIGIN, 0, "NE");
    world.Tick();
    for (std::int32_t z = 0; z < 7; ++z)
    {
      for (std::int32_t x = 0; x < 7; ++x)
      {
        world.Voxels().Edit(At(ORIGIN.xVoxels + x, GROUND + 8, ORIGIN.zVoxels + z), 0);
      }
    }
    world.Tick();
    Assert::IsFalse(world.Collapses().empty());
    const Protocol::VoxelPosition merlon = PartVoxels(world, "T4", Simulation::ServerTemplate::Part::Critical).front();
    Assert::AreNotEqual(std::uint8_t{0}, world.Voxels().At(merlon));
    Assert::AreEqual(std::uint8_t{0}, world.Structures().at(tower).criticalPercent);
    Assert::IsFalse(world.Structures().at(tower).working);
    Assert::IsTrue(Raised(world, Simulation::TrialEventKind::StructureStopped, tower));
  }

  TEST_METHOD(AT3WithNoCriticalPartAlwaysWorks)
  {
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId wall = world.AddStructure(DEFENDER, "T3", ORIGIN, 0, "W1");
    world.Tick();
    for (std::int32_t x = 0; x < 8; ++x)
    {
      world.Voxels().Edit(At(ORIGIN.xVoxels + x, GROUND + 6, ORIGIN.zVoxels), 0);
      world.Voxels().Edit(At(ORIGIN.xVoxels + x, GROUND + 5, ORIGIN.zVoxels), 0);
    }
    world.Tick();
    Assert::IsTrue(world.Structures().at(wall).working);
    Assert::AreEqual(std::uint8_t{100}, world.Structures().at(wall).criticalPercent);
    Assert::IsFalse(Raised(world, Simulation::TrialEventKind::StructureStopped, wall));
  }

  TEST_METHOD(KeepsItsShareOnATickWithNoEditInItsFootprint)
  {
    // Five of the platform's voxels gone, 105 of 110 standing, 95 %; an edit across the map leaves it so.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId tower = world.AddStructure(DEFENDER, "T4", ORIGIN, 0, "NE");
    const std::vector<Protocol::VoxelPosition> platform = PartVoxels(world, "T4", Simulation::ServerTemplate::Part::Critical);
    for (std::size_t removed = 0; removed < 5; ++removed)
    {
      world.Voxels().Edit(platform[removed], 0);
    }
    world.Tick();
    Assert::AreEqual(std::uint8_t{95}, world.Structures().at(tower).criticalPercent);
    world.Voxels().Edit(At(50, GROUND - 1, 50), 0);
    world.Tick();
    Assert::AreEqual(std::uint8_t{95}, world.Structures().at(tower).criticalPercent);
    world.Tick();
    Assert::AreEqual(std::uint8_t{95}, world.Structures().at(tower).criticalPercent);
  }

  TEST_METHOD(RoundTripsItsMaskAndShareThroughASave)
  {
    Simulation::World world(FlatWorld::Desc());
    // Five of the platform's voxels gone, so that what round-trips is a share of 95 %.
    const Protocol::EntityId tower = world.AddStructure(DEFENDER, "T4", ORIGIN, 0, "NE");
    const std::vector<Protocol::VoxelPosition> platform = PartVoxels(world, "T4", Simulation::ServerTemplate::Part::Critical);
    for (std::size_t removed = 0; removed < 5; ++removed)
    {
      world.Voxels().Edit(platform[removed], 0);
    }
    world.Tick();
    const Simulation::Structure& before = world.Structures().at(tower);
    Assert::AreEqual(std::uint8_t{95}, before.criticalPercent);
    Assert::AreEqual(std::size_t{(7 * 12 * 7 + 7) / 8}, before.builtMask.size());
    Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(world), FlatWorld::Desc());
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(loaded));
    const Simulation::Structure& after = loaded.Structures().at(tower);
    Assert::IsTrue(before.builtMask == after.builtMask);
    Assert::AreEqual(before.criticalPercent, after.criticalPercent);
    Assert::AreEqual(before.working, after.working);
    for (std::uint32_t tick = 0; tick < 20; ++tick)
    {
      world.Tick();
      loaded.Tick();
    }
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(loaded));
  }
};

} // namespace SimulationTests
