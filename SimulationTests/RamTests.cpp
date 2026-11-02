#include "pch.h"

#include "FlatWorld.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The ram (M4-C6; ADR-023, decision 9; MvpPlan.md, M4, input 9): ordered at a structure it drives to touch it and
// strikes every 15 s, 300 ticks, at the lowest voxel before its front; a blow takes 1 timber voxel, by the owner's
// decision of 2026-11-02 after M4-D4, 1 of earth, and 1 of stone within the structure's bottom metre and none above. Its roof is 20 pierce armor; its melee armor is 0.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t WIDTH = 64;

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

void Order(Simulation::World& _world, std::uint8_t _player, const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  _world.Submit(_player, writer.Bytes());
}

std::uint32_t BlowTicks(const Simulation::World& _world)
{
  return static_cast<std::uint32_t>(_world.Data().Tuning().ramBlowTicks);
}

/// The voxels of a box that are not empty.
std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> Standing(const Simulation::World& _world, Protocol::VoxelPosition _min,
                                                                        Protocol::VoxelPosition _max)
{
  std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> standing;
  for (std::int32_t y = _min.yVoxels; y <= _max.yVoxels; ++y)
  {
    for (std::int32_t z = _min.zVoxels; z <= _max.zVoxels; ++z)
    {
      for (std::int32_t x = _min.xVoxels; x <= _max.xVoxels; ++x)
      {
        if (_world.Voxels().At(At(x, y, z)) != 0)
        {
          standing.emplace(x, y, z);
        }
      }
    }
  }
  return standing;
}

/// A ram of the attacker's ordered at _structure; ticks until it has struck _blows times, at most _ticks, recording the
/// tick of each blow: the tick its cadence starts over.
std::vector<std::uint32_t> Ram(Simulation::World& _world, Protocol::EntityId _ram, Protocol::EntityId _structure, std::size_t _blows,
                               std::uint32_t _ticks, const std::function<void(std::size_t)>& _afterBlow)
{
  Order(_world, ATTACKER, Protocol::AttackTarget{.squad = _world.SiegeEngines().at(_ram).squad, .target = _structure});
  std::vector<std::uint32_t> blows;
  for (std::uint32_t tick = 0; tick < _ticks && blows.size() < _blows; ++tick)
  {
    _world.Tick();
    if (_world.SiegeEngines().at(_ram).reloadTicks == BlowTicks(_world))
    {
      blows.push_back(_world.TickCount());
      _afterBlow(blows.size());
    }
  }
  return blows;
}

/// Whether _class can reach (_x, 10), south of the wall line, from (_x, 50), north of it.
bool Through(const Simulation::World& _world, Simulation::NavigationClass _class, std::int32_t _x)
{
  const std::int64_t start = (std::int64_t{50} * WIDTH) + _x;
  const std::int64_t goal = (std::int64_t{10} * WIDTH) + _x;
  return _world.Navigation().NearestReachable(start, goal, _class) == goal;
}

} // namespace

TEST_CLASS(RamTests)
{
public:
  TEST_METHOD(BreaksT5sGateOneTimberVoxelABlowEvery15Seconds)
  {
    // T5 at (27, 28), its outer face north, in a masonry wall across the map; its 3 x 4 m gate at the passage's inner end,
    // x 30 to 32 at z 28. The ram drives into the passage and strikes the gate, each blow taking one timber voxel, the
    // lowest standing before its front. Foot need a gap 1 m wide and 2 m high, so the gate opens to them once a column
    // has lost its bottom two; machines need the passage's 3 m, after all 12.
    Simulation::World world(FlatWorld::Desc());
    Fill(world, At(0, GROUND, 28), At(26, GROUND + 5, 29), MASONRY);
    Fill(world, At(36, GROUND, 28), At(WIDTH - 1, GROUND + 5, 29), MASONRY);
    const Protocol::EntityId gatehouse = world.AddStructure(DEFENDER, "T5", At(27, GROUND, 28), 0, "GH");
    const Protocol::EntityId ram =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(30, GROUND, 45), 0, Simulation::EngineState::Packed);
    world.Tick();
    Assert::IsFalse(Through(world, Simulation::NavigationClass::Foot, 31));
    const auto gate = [&world] { return Standing(world, At(30, GROUND, 28), At(32, GROUND + 3, 28)).size(); };
    Assert::AreEqual(std::size_t{12}, gate());
    const auto footGap = [&world]
    {
      return std::ranges::any_of(std::array{30, 31, 32},
                                 [&world](std::int32_t _x) { return Standing(world, At(_x, GROUND, 28), At(_x, GROUND + 1, 28)).empty(); });
    };
    const std::vector<std::uint32_t> blows = Ram(world, ram, gatehouse, 12, 300 * 20,
                                                 [&](std::size_t _blow)
                                                 {
                                                   world.Tick(); // step 8 has rebuilt the tiles the blow cut
                                                   Assert::AreEqual(12 - _blow, gate());
                                                   Assert::AreEqual(footGap(), Through(world, Simulation::NavigationClass::Foot, 31));
                                                   Assert::AreEqual(_blow == 12, Through(world, Simulation::NavigationClass::Machines, 30));
                                                 });
    Assert::AreEqual(std::size_t{12}, blows.size());
    for (std::size_t blow = 1; blow < blows.size(); ++blow)
    {
      Assert::AreEqual(BlowTicks(world), blows[blow] - blows[blow - 1]);
    }
    Assert::AreEqual(300U, BlowTicks(world));
  }

  TEST_METHOD(ChipsOneStoneVoxelABlowAtAT3sFootAndNoneAbove)
  {
    // A T3 section 8 m long; each blow takes one stone voxel of its bottom metre, and when no column before its front
    // holds one at its floor the ram moves along to where one does. Nothing above the bottom metre goes.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId wall = world.AddStructure(DEFENDER, "T3", At(20, GROUND, 30), 0, "W1");
    const Protocol::EntityId ram =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(22, GROUND, 40), 0, Simulation::EngineState::Packed);
    world.Tick();
    const Protocol::VoxelPosition low = At(20, GROUND, 30);
    const Protocol::VoxelPosition high = At(27, GROUND + 6, 31);
    auto before = Standing(world, low, high);
    const std::vector<std::uint32_t> blows = Ram(world, ram, wall, 5, 200 * 20,
                                                 [&](std::size_t)
                                                 {
                                                   const auto after = Standing(world, low, high);
                                                   Assert::AreEqual(before.size() - 1, after.size());
                                                   for (const auto& voxel : before)
                                                   {
                                                     if (!after.contains(voxel))
                                                     {
                                                       Assert::AreEqual(GROUND, std::get<1>(voxel));
                                                     }
                                                   }
                                                   before = after;
                                                 });
    Assert::AreEqual(std::size_t{5}, blows.size());
    Assert::IsTrue(world.Collapses().empty());
  }

  TEST_METHOD(TakesOneVoxelABlowOfEarth)
  {
    // The north face's bottom course of a T3 replaced by earth, as a heap against it might fill a hole: the ram strikes
    // it there, the nearest place, and each blow takes one voxel of it.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId wall = world.AddStructure(DEFENDER, "T3", At(20, GROUND, 30), 0, "W1");
    Fill(world, At(20, GROUND, 31), At(27, GROUND, 31), EARTH);
    const Protocol::EntityId ram =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(22, GROUND, 40), 0, Simulation::EngineState::Packed);
    world.Tick();
    const auto earth = [&world]
    {
      std::size_t count = 0;
      for (std::int32_t x = 20; x <= 27; ++x)
      {
        count += world.Voxels().At(At(x, GROUND, 31)) == EARTH ? 1 : 0;
      }
      return count;
    };
    const std::size_t standing = Standing(world, At(20, GROUND, 30), At(27, GROUND + 6, 31)).size();
    const std::vector<std::uint32_t> blows = Ram(world, ram, wall, 1, 60 * 20, [](std::size_t) {});
    Assert::AreEqual(std::size_t{1}, blows.size());
    Assert::AreEqual(std::size_t{7}, earth());
    Assert::AreEqual(standing - 1, Standing(world, At(20, GROUND, 30), At(27, GROUND + 6, 31)).size());
  }

  TEST_METHOD(ItsRoofTakesOneFromAnArrowAndMeleeInFull)
  {
    // An archer's 4 pierce against the roof's 20 is 1, the formula's least; a spearman's 4 melee against its 0 is 4.
    Simulation::World world(FlatWorld::Desc());
    const Protocol::EntityId shot =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(10, GROUND, 10), 0, Simulation::EngineState::Packed);
    const Protocol::EntityId struck =
      world.AddSiegeEngine(ATTACKER, Simulation::EngineKind::Ram, At(40, GROUND, 40), 0, Simulation::EngineState::Packed);
    const std::array<Protocol::VoxelPosition, 1> archerCell = {At(10, GROUND, 30)};
    const std::array<Protocol::VoxelPosition, 1> spearmanCell = {At(39, GROUND, 41)};
    const Protocol::EntityId archer = world.AddSquad(DEFENDER, Simulation::UnitKind::Archer, archerCell);
    const Protocol::EntityId spearman = world.AddSquad(DEFENDER, Simulation::UnitKind::Spearman, spearmanCell);
    Order(world, DEFENDER, Protocol::HoldSquad{.squad = archer});
    Order(world, DEFENDER, Protocol::HoldSquad{.squad = spearman});
    const auto hitPoints = [&world](Protocol::EntityId _engine)
    { return world.Units().at(world.Squads().at(world.SiegeEngines().at(_engine).squad).units.front()).hitPoints; };
    for (std::uint32_t tick = 0; tick < 30; ++tick)
    {
      world.Tick();
    }
    Assert::AreEqual(299, hitPoints(shot));
    Assert::AreEqual(296, hitPoints(struck));
  }
};

} // namespace SimulationTests
