#include "pch.h"

#include "ServerDataFile.h"
#include "UnitStats.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// ADR-023, decisions 1 and 2: each kind's statistics as UNIT holds them, and the damage formula. These tests pin MvpPlan.md's
// M4 input 6 table on purpose, so a retune of Units.json changes them, and says so.

namespace SimulationTests
{

namespace
{

using Protocol::UnitKind;

const Simulation::UnitStats& Stats(UnitKind _kind)
{
  return BakedServerData().Stats(_kind);
}

constexpr std::array<UnitKind, 5> SOLDIERS = {UnitKind::Spearman, UnitKind::Archer, UnitKind::Outrider, UnitKind::ManAtArms,
                                              UnitKind::Sapper};

/// Input 6's first hits, the attacker by row and the target by column in SOLDIERS' order, on level ground: (attack + bonus)
/// less the target's armor against the attack's type, at least 1.
constexpr std::array<std::array<std::int32_t, 5>, 5> FIRST_HITS = {{
  {4, 4, 16, 2, 4}, // spearmen, +12 against outriders; men-at-arms' melee armor 2
  {4, 4, 2, 1, 1},  // archers' pierce, against outriders' 2, men-at-arms' 4 and sappers' 4
  {6, 12, 6, 4, 6}, // outriders, +6 against archers
  {12, 8, 8, 6, 8}, // men-at-arms, +4 against spearmen
  {3, 3, 3, 1, 3},  // sappers
}};

/// An archer's hit on each soldier at steps -2 to 2: floor(4 * (100 + 15 * steps) / 100) is 2, 3, 4, 4 and 5 before armor.
constexpr std::array<std::array<std::int32_t, 5>, 5> ARCHER_BY_STEPS = {{
  {2, 2, 1, 1, 1}, // -2
  {3, 3, 1, 1, 1}, // -1
  {4, 4, 2, 1, 1}, //  0
  {4, 4, 2, 1, 1}, //  1
  {5, 5, 3, 1, 1}, //  2
}};

} // namespace

TEST_CLASS(UnitStatsTests)
{
public:
  TEST_METHOD(ReadsEveryKindsInputSixValues)
  {
    const Simulation::UnitStats& laborer = Stats(UnitKind::Laborer);
    Assert::AreEqual(std::uint8_t{1}, laborer.squadFigures);
    Assert::AreEqual(25, laborer.hitPoints);
    Assert::AreEqual(2, laborer.attack);
    Assert::AreEqual(8, laborer.visionVoxels);
    Assert::AreEqual(327, laborer.digCubicMetersPerTick.Raw()); // 0.1 m3/s, 0.005 a tick, floored once

    const Simulation::UnitStats& spearman = Stats(UnitKind::Spearman);
    Assert::AreEqual(std::uint8_t{6}, spearman.squadFigures);
    Assert::AreEqual(45, spearman.hitPoints);
    Assert::AreEqual(12, spearman.bonus.at(static_cast<std::size_t>(UnitKind::Outrider)));
    Assert::AreEqual(40U, spearman.reloadTicks);
    Assert::AreEqual(0, spearman.rangeVoxels);

    const Simulation::UnitStats& archer = Stats(UnitKind::Archer);
    Assert::IsTrue(archer.attackType == Protocol::DamageType::Pierce);
    Assert::AreEqual(30, archer.rangeVoxels);
    Assert::AreEqual(32, archer.visionVoxels);

    const Simulation::UnitStats& outrider = Stats(UnitKind::Outrider);
    Assert::IsTrue(outrider.navigationClass == Simulation::NavigationClass::Cavalry);
    Assert::AreEqual(std::uint8_t{3}, outrider.squadFigures);
    Assert::AreEqual(60, outrider.hitPoints);
    Assert::AreEqual(2, outrider.armor.at(static_cast<std::size_t>(Protocol::DamageType::Pierce)));
    Assert::AreEqual(20, outrider.bonus.at(static_cast<std::size_t>(UnitKind::Ram)));
    Assert::AreEqual(20, outrider.bonus.at(static_cast<std::size_t>(UnitKind::Trebuchet)));
    Assert::AreEqual(4, outrider.bonus.at(static_cast<std::size_t>(UnitKind::Laborer)));
    Assert::AreEqual(16, outrider.visionVoxels);

    const Simulation::UnitStats& manAtArms = Stats(UnitKind::ManAtArms);
    Assert::AreEqual(70, manAtArms.hitPoints);
    Assert::AreEqual(2, manAtArms.armor.at(static_cast<std::size_t>(Protocol::DamageType::Melee)));
    Assert::AreEqual(4, manAtArms.armor.at(static_cast<std::size_t>(Protocol::DamageType::Pierce)));

    const Simulation::UnitStats& sapper = Stats(UnitKind::Sapper);
    Assert::AreEqual(40, sapper.hitPoints);
    Assert::AreEqual(655, sapper.digCubicMetersPerTick.Raw()); // 0.2 m3/s, twice a laborer's, floored once

    const Simulation::UnitStats& ram = Stats(UnitKind::Ram);
    Assert::IsTrue(ram.navigationClass == Simulation::NavigationClass::Machines);
    Assert::AreEqual(300, ram.hitPoints);
    Assert::AreEqual(20, ram.armor.at(static_cast<std::size_t>(Protocol::DamageType::Pierce)));
    Assert::AreEqual(0U, ram.reloadTicks); // RULE's ramBlowTicks owns its cadence

    const Simulation::UnitStats& trebuchet = Stats(UnitKind::Trebuchet);
    Assert::AreEqual(150, trebuchet.hitPoints);
    Assert::AreEqual(50, trebuchet.visionVoxels);
    Assert::AreEqual(2293, trebuchet.speedMetersPerTick.Raw()); // 0.7 m/s, packed
  }

  TEST_METHOD(EveryKindReadsBackAsItself)
  {
    for (std::size_t kind = 0; kind < Protocol::UNIT_KIND_COUNT; ++kind)
    {
      Assert::IsTrue(Stats(static_cast<UnitKind>(kind)).kind == static_cast<UnitKind>(kind));
    }
  }

  TEST_METHOD(GivesInputSixsFirstHitsForEveryPairingOfSoldiers)
  {
    for (std::size_t attacker = 0; attacker < SOLDIERS.size(); ++attacker)
    {
      for (std::size_t target = 0; target < SOLDIERS.size(); ++target)
      {
        Assert::AreEqual(FIRST_HITS.at(attacker).at(target),
                         Simulation::Damage(Stats(SOLDIERS.at(attacker)), Stats(SOLDIERS.at(target)), 0));
      }
    }
  }

  TEST_METHOD(CountsElevationForTheArchersArrowsAlone)
  {
    for (std::size_t row = 0; row < ARCHER_BY_STEPS.size(); ++row)
    {
      const std::int32_t steps = static_cast<std::int32_t>(row) - 2;
      for (std::size_t target = 0; target < SOLDIERS.size(); ++target)
      {
        Assert::AreEqual(ARCHER_BY_STEPS.at(row).at(target),
                         Simulation::Damage(Stats(UnitKind::Archer), Stats(SOLDIERS.at(target)), steps));
      }
    }
    // A melee hit ignores the steps it is given.
    Assert::AreEqual(12, Simulation::Damage(Stats(UnitKind::ManAtArms), Stats(UnitKind::Spearman), 2));
  }

  TEST_METHOD(IsNeverBelowOne)
  {
    for (std::size_t attacker = 0; attacker < Protocol::UNIT_KIND_COUNT; ++attacker)
    {
      for (std::size_t target = 0; target < Protocol::UNIT_KIND_COUNT; ++target)
      {
        for (std::int32_t steps = -2; steps <= 2; ++steps)
        {
          Assert::IsTrue(Simulation::Damage(Stats(static_cast<UnitKind>(attacker)), Stats(static_cast<UnitKind>(target)), steps) >= 1);
        }
      }
    }
    // The ram's roof, 20 pierce, takes an archer's arrow to 1.
    Assert::AreEqual(1, Simulation::Damage(Stats(UnitKind::Archer), Stats(UnitKind::Ram), 2));
  }
};

} // namespace SimulationTests
