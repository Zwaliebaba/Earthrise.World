#include "pch.h"

#include "BreachLab.h"
#include "CraterLab.h"
#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The crater items (M5-D1; GameDesign.md, M5's values, item 12): W2 struck at its base by the ram or the trebuchet, the
// engines' own craters with no engine on the site, so that only the damage tells which made it; four items each, and
// the item set named by a stable hash.

namespace SimulationTests
{

namespace
{

constexpr std::int32_t MARGIN_VOXELS = 2;
constexpr std::uint32_t QUIET_TICKS = 400;

/// W2's box, both corners included, and MARGIN_VOXELS about it: where an item's damage may lie, and where it is looked for.
struct Region
{
  Protocol::VoxelPosition low;
  Protocol::VoxelPosition high;
};

Region AboutW2(const Simulation::World& _world, Protocol::EntityId _target)
{
  const Simulation::Structure& wall = _world.Structures().at(_target);
  const Simulation::ServerTemplate* shape = _world.Data().FindTemplate(wall.templateId);
  const bool across = wall.quarterTurns % 2 == 1;
  const auto width = static_cast<std::int32_t>(across ? shape->depthVoxels : shape->widthVoxels);
  const auto depth = static_cast<std::int32_t>(across ? shape->widthVoxels : shape->depthVoxels);
  const Protocol::VoxelPosition& origin = wall.originVoxels;
  return {.low = {.xVoxels = origin.xVoxels - MARGIN_VOXELS,
                  .yVoxels = origin.yVoxels - MARGIN_VOXELS,
                  .zVoxels = origin.zVoxels - MARGIN_VOXELS},
          .high = {.xVoxels = origin.xVoxels + width - 1 + MARGIN_VOXELS,
                   .yVoxels = origin.yVoxels + static_cast<std::int32_t>(shape->heightVoxels) - 1 + MARGIN_VOXELS,
                   .zVoxels = origin.zVoxels + depth - 1 + MARGIN_VOXELS}};
}

/// The voxels of _region where _first and _second differ.
std::vector<Protocol::VoxelPosition> Differences(const Simulation::World& _first, const Simulation::World& _second, const Region& _region)
{
  std::vector<Protocol::VoxelPosition> differ;
  for (std::int32_t y = _region.low.yVoxels; y <= _region.high.yVoxels; ++y)
  {
    for (std::int32_t z = _region.low.zVoxels; z <= _region.high.zVoxels; ++z)
    {
      for (std::int32_t x = _region.low.xVoxels; x <= _region.high.xVoxels; ++x)
      {
        const Protocol::VoxelPosition voxel{.xVoxels = x, .yVoxels = y, .zVoxels = z};
        if (_first.Voxels().At(voxel) != _second.Voxels().At(voxel))
        {
          differ.push_back(voxel);
        }
      }
    }
  }
  return differ;
}

} // namespace

TEST_CLASS(CraterLabTests)
{
public:
  TEST_METHOD(EachItemDiffersFromTheIntactEnclosureInItsCraterAloneWithNoEngineOnTheSite)
  {
    // A blow on W2's stone takes ramStoneCraterVoxels at its face; a stone takes buildingStoneCraterVoxels along its
    // shot, the voxel struck and the one behind it; and nothing else changes.
    Simulation::World intact(Simulation::LabDesc(BakedServerData(), FlatWorld::BUILD_ID));
    const Protocol::EntityId target = Simulation::BuildLabEnclosure(intact);
    const Region region = AboutW2(intact, target);
    const Simulation::Rules& rules = intact.Data().Tuning();
    for (const Simulation::CraterItem& item : Simulation::CraterItems())
    {
      const std::wstring id(item.id.begin(), item.id.end());
      Simulation::World world(Simulation::LabDesc(BakedServerData(), FlatWorld::BUILD_ID));
      const Simulation::CraterSite site = Simulation::BuildCraterItem(world, item);
      Assert::AreEqual(target, site.target, id.c_str());
      Assert::IsTrue(world.SiegeEngines().empty() && world.Squads().empty(), id.c_str());
      const std::vector<Protocol::VoxelPosition> differ = Differences(intact, world, region);
      const std::int32_t each = item.engine == Simulation::CraterEngine::Ram ? rules.ramStoneCraterVoxels : rules.buildingStoneCraterVoxels;
      Assert::AreEqual(static_cast<std::size_t>(each) * item.hits, differ.size(), id.c_str());
      for (const Protocol::VoxelPosition& voxel : differ)
      {
        Assert::AreEqual(Protocol::Chunk::EMPTY, world.Voxels().At(voxel), id.c_str());
        Assert::AreEqual(site.markVoxels.yVoxels, voxel.yVoxels, id.c_str()); // at the base
        Assert::IsTrue(voxel.xVoxels >= site.markVoxels.xVoxels &&
                         voxel.xVoxels < site.markVoxels.xVoxels + static_cast<std::int32_t>(item.hits),
                       id.c_str());
      }
    }
  }

  TEST_METHOD(AnItemGoesQuietWithNothingMoreFallen)
  {
    // The widest stone item, three holes through W2's base: well short of the width at which the stone above falls.
    const auto widest = std::ranges::max_element(Simulation::CraterItems(), {}, [](const Simulation::CraterItem& _item)
                                                 { return _item.engine == Simulation::CraterEngine::Trebuchet ? _item.hits : 0U; });
    Simulation::World world(Simulation::LabDesc(BakedServerData(), FlatWorld::BUILD_ID));
    const Simulation::CraterSite site = Simulation::BuildCraterItem(world, *widest);
    Simulation::World struck(Simulation::LabDesc(BakedServerData(), FlatWorld::BUILD_ID));
    static_cast<void>(Simulation::BuildCraterItem(struck, *widest));
    std::uint32_t ticks = 0;
    do
    {
      world.Tick();
      ++ticks;
    } while (!world.Quiet() && ticks < QUIET_TICKS);
    Assert::IsTrue(world.Quiet());
    Assert::IsTrue(world.Collapses().empty());
    Assert::IsTrue(Differences(struck, world, AboutW2(world, site.target)).empty());
  }

  TEST_METHOD(TheSetIsThePracticeItemThenFourByTheRamAndFourByTheTrebuchet)
  {
    const std::span<const Simulation::CraterItem> items = Simulation::CraterItems();
    Assert::AreEqual(std::size_t{9}, items.size());
    Assert::AreEqual(std::string("C0"), items.front().id);
    std::uint32_t rams = 0;
    std::uint32_t ramHits = 0;
    std::uint32_t stoneHits = 0;
    for (const Simulation::CraterItem& item : items.subspan(1))
    {
      const bool ram = item.engine == Simulation::CraterEngine::Ram;
      rams += ram ? 1 : 0;
      (ram ? ramHits : stoneHits) += item.hits;
      Assert::IsTrue(item.hits >= 1 && item.hits <= 3);
    }
    Assert::AreEqual(4U, rams);
    Assert::AreEqual(ramHits, stoneHits); // the count of holes does not tell the engine
  }

  TEST_METHOD(TheItemSetsHashIsStable)
  {
    Assert::AreEqual(Simulation::CraterItemsHash(), Simulation::CraterItemsHash());
    Assert::AreNotEqual(std::uint64_t{0}, Simulation::CraterItemsHash());
  }
};

} // namespace SimulationTests
