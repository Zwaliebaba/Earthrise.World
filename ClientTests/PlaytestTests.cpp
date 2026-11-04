#include "pch.h"

#include "PlaytestGame.h"
#include "SiegeLog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The Siege playtest's games (M5-C1; MvpPlan-M5.md, *The session*; GameDesign.md, M5's values, items 3 and 4): a
// participant's id gives each game's version and layout, version order crossed with layout over four cells, so that the
// facilitator types neither; and a game's log names it.

namespace ClientTests
{

namespace
{

struct Expected
{
  std::string_view id;
  std::uint32_t cell;
  std::string_view group;
  bool firstControl;
  bool firstMirror;
};

/// MvpPlan-M5.md's table: cell 0 the voxel version on Siege first, 1 the control on Siege, 2 the voxel version on
/// SiegeMirror, 3 the control on SiegeMirror; the second game the other version on the other layout.
constexpr std::array<Expected, 12> TABLE = {{
  {.id = "S01", .cell = 0, .group = "rts", .firstControl = false, .firstMirror = false},
  {.id = "S02", .cell = 1, .group = "rts", .firstControl = true, .firstMirror = false},
  {.id = "S03", .cell = 2, .group = "rts", .firstControl = false, .firstMirror = true},
  {.id = "S04", .cell = 3, .group = "rts", .firstControl = true, .firstMirror = true},
  {.id = "S05", .cell = 0, .group = "rts", .firstControl = false, .firstMirror = false},
  {.id = "S06", .cell = 1, .group = "rts", .firstControl = true, .firstMirror = false},
  {.id = "S07", .cell = 2, .group = "rts", .firstControl = false, .firstMirror = true},
  {.id = "S08", .cell = 3, .group = "rts", .firstControl = true, .firstMirror = true},
  {.id = "N01", .cell = 0, .group = "novice", .firstControl = false, .firstMirror = false},
  {.id = "N02", .cell = 1, .group = "novice", .firstControl = true, .firstMirror = false},
  {.id = "N03", .cell = 2, .group = "novice", .firstControl = false, .firstMirror = true},
  {.id = "X01", .cell = 0, .group = "pilot", .firstControl = false, .firstMirror = false},
}};

} // namespace

TEST_CLASS(PlaytestTests)
{
public:
  TEST_METHOD(EachIdGivesItsCellsTwoGamesTheOtherVersionOnTheOtherLayoutSecond)
  {
    for (const Expected& expected : TABLE)
    {
      const std::wstring id(expected.id.begin(), expected.id.end());
      const std::optional<Client::PlaytestGame> first = Client::PlaytestGameOf(expected.id, 1);
      const std::optional<Client::PlaytestGame> second = Client::PlaytestGameOf(expected.id, 2);
      Assert::IsTrue(first.has_value() && second.has_value(), id.c_str());
      Assert::AreEqual(1U, first.value_or(Client::PlaytestGame{}).game, id.c_str());
      Assert::AreEqual(2U, second.value_or(Client::PlaytestGame{}).game, id.c_str());
      for (const Client::PlaytestGame& game : {first.value_or(Client::PlaytestGame{}), second.value_or(Client::PlaytestGame{})})
      {
        Assert::AreEqual(expected.cell, game.cell, id.c_str());
        Assert::IsTrue(expected.group == game.group, id.c_str());
      }
      Assert::AreEqual(expected.firstControl, first.value_or(Client::PlaytestGame{}).control, id.c_str());
      Assert::AreEqual(expected.firstMirror, first.value_or(Client::PlaytestGame{}).mirror, id.c_str());
      Assert::AreEqual(!expected.firstControl, second.value_or(Client::PlaytestGame{}).control, id.c_str());
      Assert::AreEqual(!expected.firstMirror, second.value_or(Client::PlaytestGame{}).mirror, id.c_str());
    }
  }

  TEST_METHOD(RefusesAnIdOfAnotherFormAndAGameButOneOrTwo)
  {
    for (const std::string_view id : {"Q01", "S00", "S1", "S001", "s01", "P00", "SA1", ""})
    {
      const std::wstring wide(id.begin(), id.end());
      Assert::IsFalse(Client::PlaytestGameOf(id, 1).has_value(), wide.c_str());
    }
    Assert::IsFalse(Client::PlaytestGameOf("S01", 0).has_value());
    Assert::IsFalse(Client::PlaytestGameOf("S01", 3).has_value());
  }

  TEST_METHOD(TheEightRtsPlayersBalanceVersionOrderAndLayout)
  {
    // Four play the voxel version first, four play it on each layout, and four start on each layout.
    std::uint32_t voxelFirst = 0;
    std::uint32_t voxelOnMirror = 0;
    std::uint32_t startOnMirror = 0;
    for (std::uint32_t number = 1; number <= 8; ++number)
    {
      const std::string id = std::format("S{:02}", number);
      const Client::PlaytestGame first = Client::PlaytestGameOf(id, 1).value_or(Client::PlaytestGame{});
      const Client::PlaytestGame second = Client::PlaytestGameOf(id, 2).value_or(Client::PlaytestGame{});
      voxelFirst += first.control ? 0 : 1;
      voxelOnMirror += (first.control ? second.mirror : first.mirror) ? 1 : 0;
      startOnMirror += first.mirror ? 1 : 0;
    }
    Assert::AreEqual(4U, voxelFirst);
    Assert::AreEqual(4U, voxelOnMirror);
    Assert::AreEqual(4U, startOnMirror);
  }

  TEST_METHOD(AGamesLogNamesItsGameCellAndGroupBeforeItsHeaderRow)
  {
    Client::SiegeLogHeader header{.participant = "S03",
                                  .control = false,
                                  .buildId = 0x1234,
                                  .serverDataHash = 0xabcd,
                                  .scenario = "SiegeMirror",
                                  .nearest = "S2",
                                  .weak = "W3",
                                  .gate = "GH",
                                  .widthPixels = 1920,
                                  .heightPixels = 1080,
                                  .uiScale = 1.5F,
                                  .startTime = "2026-11-04 09:00:00"};
    const std::vector<std::string> bare = Client::SiegeLog::Head(header);
    Assert::IsTrue(std::ranges::none_of(bare, [](const std::string& _line) { return _line.starts_with("# game:"); }));
    header.playtest = Client::PlaytestGameOf("S03", 1);
    const std::vector<std::string> head = Client::SiegeLog::Head(header);
    Assert::AreEqual(bare.size() + 3, head.size());
    Assert::AreEqual(std::string("# game: 1"), head.at(head.size() - 4));
    Assert::AreEqual(std::string("# cell: 2"), head.at(head.size() - 3));
    Assert::AreEqual(std::string("# group: rts"), head.at(head.size() - 2));
    Assert::AreEqual(std::string("kind,tick,a,b,c,d,e"), head.back());
  }
};

} // namespace ClientTests
