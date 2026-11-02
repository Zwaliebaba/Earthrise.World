#include "pch.h"

#include "Formation.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A squad's slots about its lead (ADR-022, decision 4): foot squads of six in two ranks of three, outriders in one rank
// of three, 1 m apart, facing one of eight directions.

namespace SimulationTests
{

namespace
{

constexpr Protocol::VoxelPosition LEAD{.xVoxels = 10, .yVoxels = 9, .zVoxels = 10};

bool At(const Protocol::VoxelPosition& _slot, std::int32_t _x, std::int32_t _z)
{
  return _slot.xVoxels == _x && _slot.yVoxels == LEAD.yVoxels && _slot.zVoxels == _z;
}

} // namespace

TEST_CLASS(FormationTests)
{
public:
  TEST_METHOD(StandsSixFacingNorthInTwoRanksOfThreeTheLeadInTheFrontsMiddle)
  {
    // North is +z, so the rank behind is at z - 1, and the lead's left is west, -x.
    const std::vector<Protocol::VoxelPosition> slots = Simulation::FormationSlots(LEAD, 0, 6, 2);
    Assert::AreEqual(std::size_t{6}, slots.size());
    Assert::IsTrue(At(slots[0], 10, 10));
    Assert::IsTrue(At(slots[1], 9, 10));
    Assert::IsTrue(At(slots[2], 11, 10));
    Assert::IsTrue(At(slots[3], 10, 9));
    Assert::IsTrue(At(slots[4], 9, 9));
    Assert::IsTrue(At(slots[5], 11, 9));
  }

  TEST_METHOD(TurnsTheSameShapeForEachOfTheEightFacings)
  {
    // Each slot north's is a steps forward and b to the right; facing f it is a steps along f and b along the facing a
    // quarter turn clockwise of it, two eighths on.
    const std::vector<Protocol::VoxelPosition> north = Simulation::FormationSlots(LEAD, 0, 6, 2);
    for (std::uint8_t facing = 0; facing < 8; ++facing)
    {
      const std::vector<Protocol::VoxelPosition> slots = Simulation::FormationSlots(LEAD, facing, 6, 2);
      const std::array<std::int32_t, 2>& forward = Simulation::FACINGS.at(facing);
      const std::array<std::int32_t, 2>& right = Simulation::FACINGS.at((facing + 2) % 8);
      std::set<std::pair<std::int32_t, std::int32_t>> cells;
      for (std::size_t slot = 0; slot < north.size(); ++slot)
      {
        const std::int32_t ahead = north[slot].zVoxels - LEAD.zVoxels;
        const std::int32_t across = north[slot].xVoxels - LEAD.xVoxels;
        Assert::IsTrue(At(slots[slot], LEAD.xVoxels + (ahead * forward[0]) + (across * right[0]),
                          LEAD.zVoxels + (ahead * forward[1]) + (across * right[1])));
        cells.emplace(slots[slot].xVoxels, slots[slot].zVoxels);
      }
      Assert::AreEqual(north.size(), cells.size());
    }
  }

  TEST_METHOD(StandsThreeOutridersInOneRank)
  {
    const std::vector<Protocol::VoxelPosition> slots = Simulation::FormationSlots(LEAD, 0, 3, 1);
    Assert::AreEqual(std::size_t{3}, slots.size());
    Assert::IsTrue(At(slots[0], 10, 10));
    Assert::IsTrue(At(slots[1], 9, 10));
    Assert::IsTrue(At(slots[2], 11, 10));
  }

  TEST_METHOD(LaysASquadShortOfFiguresOutAsAFullOnesFirstSlots)
  {
    // Four left of six keep the full squad's ranks of three: three in front, one behind the lead.
    const std::vector<Protocol::VoxelPosition> slots = Simulation::FormationSlots(LEAD, 0, 4, 2, 6);
    Assert::AreEqual(std::size_t{4}, slots.size());
    Assert::IsTrue(At(slots[2], 11, 10));
    Assert::IsTrue(At(slots[3], 10, 9));
  }

  TEST_METHOD(FacesTheWayOfAStep)
  {
    Assert::AreEqual(std::uint8_t{0}, Simulation::FacingOf(0, 1));
    Assert::AreEqual(std::uint8_t{2}, Simulation::FacingOf(1, 0));
    Assert::AreEqual(std::uint8_t{5}, Simulation::FacingOf(-1, -1));
    Assert::AreEqual(std::uint8_t{0}, Simulation::FacingOf(0, 0));
  }
};

} // namespace SimulationTests
