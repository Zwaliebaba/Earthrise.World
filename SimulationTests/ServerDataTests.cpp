#include "pch.h"

#include "ServerData.h"
#include "ServerDataFile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The reader of ADR-016, on the file the build baked from GameData/, and on broken copies of it.

namespace SimulationTests
{

namespace
{

std::vector<std::byte> FileBytes()
{
  std::ifstream stream(ServerDataFile(), std::ios::binary);
  const std::vector<char> raw{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  const std::span<const std::byte> bytes = std::as_bytes(std::span(raw));
  return {bytes.begin(), bytes.end()};
}

/// The message Read refuses _file with, or an empty string when it reads it.
std::string Refusal(const std::vector<std::byte>& _file)
{
  try
  {
    static_cast<void>(Simulation::ServerData::Read(_file));
  }
  catch (const std::runtime_error& error)
  {
    return error.what();
  }
  return {};
}

/// What M1's StandInMaterialRules(1) gave each palette index, written out before it went: support,
/// span, ground and liquid. Every index not listed gave none of them.
struct StandIn
{
  std::uint8_t index;
  Simulation::Support support;
  std::uint8_t spanVoxels;
  bool walkableGround;
  bool liquid;
};

constexpr std::array<StandIn, 19> STAND_IN = {{
  {.index = 1, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 2, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 3, .support = Simulation::Support::Spans, .spanVoxels = 2, .walkableGround = true, .liquid = false},
  {.index = 4, .support = Simulation::Support::Spans, .spanVoxels = 2, .walkableGround = true, .liquid = false},
  {.index = 5, .support = Simulation::Support::Anchors, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 6, .support = Simulation::Support::Spans, .spanVoxels = 2, .walkableGround = false, .liquid = false},
  {.index = 7, .support = Simulation::Support::Spans, .spanVoxels = 5, .walkableGround = false, .liquid = false},
  {.index = 8, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 9, .support = Simulation::Support::None, .spanVoxels = 0, .walkableGround = false, .liquid = true},
  {.index = 10, .support = Simulation::Support::Spans, .spanVoxels = 3, .walkableGround = false, .liquid = false},
  {.index = 11, .support = Simulation::Support::Spans, .spanVoxels = 3, .walkableGround = false, .liquid = false},
  {.index = 12, .support = Simulation::Support::Spans, .spanVoxels = 5, .walkableGround = false, .liquid = false},
  {.index = 13, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 14, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 15, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  // 18 and 20, the decorations, gave timber's rule; they no longer reach the world, and are checked below.
  {.index = 25, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 26, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 28, .support = Simulation::Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false},
  {.index = 0, .support = Simulation::Support::None, .spanVoxels = 0, .walkableGround = false, .liquid = false},
}};

bool SameRule(const Simulation::MaterialRule& _a, const Simulation::MaterialRule& _b)
{
  return _a.behavior == _b.behavior && _a.support == _b.support && _a.spanVoxels == _b.spanVoxels &&
         _a.walkableGround == _b.walkableGround && _a.liquid == _b.liquid && _a.rubbleIndex == _b.rubbleIndex;
}

} // namespace

TEST_CLASS(ServerDataTests)
{
public:
  TEST_METHOD(ReadsTheBakedFile)
  {
    // GameData/Manifest.json lists nine templates, T1 to T9, and T3 counts 96 m3 of stone (ADR-006).
    const Simulation::ServerData& data = BakedServerData();
    Assert::AreEqual(std::size_t{9}, data.TemplateList().size());
    Assert::AreEqual(std::string("T1"), data.TemplateList().front().id);
    Assert::AreEqual(std::string("T9"), data.TemplateList().back().id);
    const Simulation::ServerTemplate* wall = data.FindTemplate("T3");
    Assert::IsNotNull(wall);
    Assert::AreEqual(std::uint32_t{96}, wall->stoneM3);
    Assert::AreEqual(std::size_t{96},
                     static_cast<std::size_t>(std::ranges::count_if(wall->indices, [](std::uint8_t _index) { return _index != 0; })));
    Assert::IsTrue(data.Templates()("T5").has_value());
    Assert::IsFalse(data.Templates()("T10").has_value());
    Assert::AreEqual(std::string("Gate"), data.FindTemplate("T5")->criticalPart);
  }

  TEST_METHOD(GivesMasonryItsSpanAndItsRubble)
  {
    const Simulation::MaterialRule& masonry = BakedServerData().Materials()[6];
    Assert::AreEqual(std::uint8_t{2}, masonry.spanVoxels);
    Assert::AreEqual(std::uint8_t{8}, masonry.rubbleIndex);
    Assert::IsTrue(masonry.behavior == Simulation::Behavior::BuildingStone);
  }

  TEST_METHOD(ReadsTheQueryBudget)
  {
    // M3's values, item 10: 16 units of work a tick, Proposed until M3-G3 measures it.
    Assert::AreEqual(16, BakedServerData().Tuning().queryWorkPerTick);
  }

  TEST_METHOD(RefusesAFileLackingAKeyItNeedsByName)
  {
    const std::vector<std::byte> file = BakedServerDataWithout("queryWorkPerTick");
    try
    {
      static_cast<void>(Simulation::ServerData::Read(file));
      Assert::Fail(L"a file lacking queryWorkPerTick was read");
    }
    catch (const std::runtime_error& error)
    {
      Assert::IsTrue(std::string_view(error.what()).find("queryWorkPerTick") != std::string_view::npos);
    }
  }

  TEST_METHOD(ReadsTheTuningInTheSimulationsUnits)
  {
    const Simulation::Rules& rules = BakedServerData().Tuning();
    Assert::AreEqual(4587, rules.footSpeedMetersPerTick.Raw());
    Assert::AreEqual(9175, rules.cavalrySpeedMetersPerTick.Raw());  // 2.8 m/s
    Assert::AreEqual(2293, rules.machinesSpeedMetersPerTick.Raw()); // 0.7 m/s
    Assert::AreEqual(20, rules.warningBaseTicks);
    Assert::AreEqual(400, rules.shotReloadTicks);
    Assert::AreEqual(5, rules.timberSpanVoxels);
    Assert::AreEqual(3, rules.machinesWidthVoxels);
  }

  TEST_METHOD(RefusesACorruptBody)
  {
    std::vector<std::byte> file = FileBytes();
    Assert::IsTrue(Refusal(file).empty());
    file[file.size() / 2] ^= std::byte{0x01};
    Assert::IsTrue(Refusal(file).find("hash") != std::string::npos);
  }

  TEST_METHOD(RefusesAnotherMagicNumberOrVersion)
  {
    std::vector<std::byte> file = FileBytes();
    file[0] = std::byte{'X'};
    Assert::IsTrue(Refusal(file).find("magic") != std::string::npos);
    file = FileBytes();
    file[4] = std::byte{2};
    Assert::IsTrue(Refusal(file).find("format version is 2") != std::string::npos);
    Assert::IsTrue(Refusal({}).find("magic") != std::string::npos);
  }

  TEST_METHOD(RefusesAMissingFile)
  {
    Assert::ExpectException<std::runtime_error>(
      [] { static_cast<void>(Simulation::ServerData::Load(ServerDataFile().parent_path() / "Missing.bin")); });
  }

  TEST_METHOD(KeepsWhatTheStandInGaveEachIndex)
  {
    const Simulation::MaterialRules& rules = BakedServerData().Materials();
    for (std::size_t index = 0; index < rules.size(); ++index)
    {
      if (index == 18 || index == 20 || index >= 29)
      {
        continue; // the decorations and their hosts, below
      }
      const auto listed = std::ranges::find(STAND_IN, index, &StandIn::index);
      const StandIn expected = listed != STAND_IN.end() ? *listed : STAND_IN.back();
      const Simulation::MaterialRule& rule = rules.at(index);
      Assert::IsTrue(rule.support == expected.support);
      Assert::AreEqual(expected.spanVoxels, rule.spanVoxels);
      Assert::AreEqual(expected.walkableGround, rule.walkableGround);
      Assert::AreEqual(expected.liquid, rule.liquid);
    }
  }

  TEST_METHOD(WritesDecorationsAsTheirHosts)
  {
    // Iron (18) and an opening (20) no longer reach the world; the bake writes them as 29, an opening
    // in timber, 30, an opening in stone, and 31, iron in timber, each with its host's rule.
    const Simulation::MaterialRules& rules = BakedServerData().Materials();
    for (const std::size_t decoration : {std::size_t{18}, std::size_t{20}})
    {
      Assert::IsTrue(rules.at(decoration).behavior == Simulation::Behavior::None);
      Assert::IsTrue(rules.at(decoration).support == Simulation::Support::None);
    }
    Assert::IsTrue(SameRule(rules[7], rules[29]));
    Assert::IsTrue(SameRule(rules[6], rules[30]));
    Assert::IsTrue(SameRule(rules[7], rules[31]));
    for (const Simulation::ServerTemplate& shape : BakedServerData().TemplateList())
    {
      Assert::IsFalse(std::ranges::any_of(shape.indices, [](std::uint8_t _index) { return _index == 18 || _index == 20; }));
    }
  }
};

} // namespace SimulationTests
