#include "pch.h"

#include "SiegeLog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The trial's log's rows (M4-J2; MvpPlan-M4.md, *The trial's log*), with no simulation: each kind's fields, every row
// as wide as the header, the figures a minute written when the minute ends, and the waits to frame a breach.

namespace ClientTests
{

namespace
{

std::vector<std::string> Fields(const std::string& _row)
{
  std::vector<std::string> fields;
  std::string field;
  for (const char character : _row)
  {
    if (character == ',')
    {
      fields.push_back(std::exchange(field, {}));
    }
    else
    {
      field += character;
    }
  }
  fields.push_back(field);
  return fields;
}

} // namespace

TEST_CLASS(SiegeLogTests)
{
public:
  TEST_METHOD(ARunsRowsHaveEveryColumnAndReadAsThePlanSays)
  {
    const std::vector<std::string> head = Client::SiegeLog::Head({.participant = "P00",
                                                                  .control = false,
                                                                  .buildId = 0x1234,
                                                                  .serverDataHash = 0xabcd,
                                                                  .scenario = "SiegeMirror",
                                                                  .nearest = "S3",
                                                                  .weak = "W3",
                                                                  .gate = "GH",
                                                                  .widthPixels = 1920,
                                                                  .heightPixels = 1080,
                                                                  .uiScale = 1.5F,
                                                                  .startTime = "2026-11-03 07:00:00"});
    Assert::AreEqual(std::string("# weak: W3"), head.at(6));
    Assert::AreEqual(std::string("kind,tick,a,b,c,d,e"), head.back());
    Assert::AreEqual(std::string("Siege-P00-voxel-SiegeMirror-20261103-070000"),
                     Client::SiegeLog::FileStem("P00", false, "SiegeMirror", "20261103-070000"));
    Client::SiegeLog log;
    log.Shot(100, 1, 60, {300, 9, 250}, Protocol::VoxelPosition{301, 12, 251}, "W3");
    log.Shot(140, 1, 60, {300, 9, 250}, std::nullopt, "");
    log.Opened(900, "W3");
    log.Spent(100, 1, 150, false, false);
    log.Spent(950, 2, 25, false, true);
    log.Stopped(1000, "NE");
    log.Works(1100, "NE");
    log.Died(1150, 2, Protocol::UnitKind::Archer);
    log.Command({.tick = 1160, .seconds = 58.0, .command = "MoveSquad 3 300 9 260", .source = "click"});
    log.End(1200, Protocol::TrialOutcome::Won);
    const std::vector<std::string> rows = log.TakeRows();
    const std::vector<std::vector<std::string>> expected = {{"shot", "100", "1", "60", "300 9 250", "301 12 251", "W3"},
                                                            {"shot", "140", "1", "60", "300 9 250", "-", "-"},
                                                            {"opened", "900", "W3", "", "", "", ""},
                                                            {"stone", "100", "1", "150", "shot", "", ""},
                                                            {"stone", "950", "2", "25", "repair", "", ""},
                                                            {"stopped", "1000", "NE", "", "", "", ""},
                                                            {"works", "1100", "NE", "", "", "", ""},
                                                            {"died", "1150", "2", "archer", "", "", ""},
                                                            {"command", "1160", "58.000", "MoveSquad 3 300 9 260", "click", "", ""},
                                                            {"outcome", "1200", "won", "60.00", "", "", ""}};
    Assert::AreEqual(expected.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
      const std::vector<std::string> fields = Fields(rows.at(index));
      Assert::AreEqual(Client::SiegeLog::COLUMNS, fields.size());
      Assert::IsTrue(expected.at(index) == fields, std::wstring(rows.at(index).begin(), rows.at(index).end()).c_str());
    }
    Assert::IsTrue(log.TakeRows().empty());
  }

  TEST_METHOD(WritesTheFiguresThroughASectionWhenTheirMinuteEnds)
  {
    Client::SiegeLog log;
    log.Passed(1210, "W3", true);
    log.Passed(1300, "W3", true);
    log.Passed(1310, "W3", false);
    log.Advance(2399); // still the second minute
    Assert::IsTrue(log.TakeRows().empty());
    log.Advance(2400);
    const std::vector<std::string> rows = log.TakeRows();
    Assert::AreEqual(std::size_t{2}, rows.size());
    Assert::AreEqual(std::string("passed,1,W3,attacker,2,,"), rows.at(0));
    Assert::AreEqual(std::string("passed,1,W3,defender,1,,"), rows.at(1));
  }

  TEST_METHOD(ABreachUnderTheDeckWaitsToBeFramedAndOneNeverFramedSaysSo)
  {
    Client::SiegeLog log;
    log.Onscreen(1000, "W3", false, {.place = Client::ScreenPlace::Deck, .deckSharePercent = 80});
    log.Onscreen(1100, "E3", true, {.place = Client::ScreenPlace::Uncovered, .deckSharePercent = 0});
    log.Onscreen(1200, "S3", true, {.place = Client::ScreenPlace::Off, .deckSharePercent = 0});
    Assert::IsTrue(log.Unframed() == std::vector<std::string>{"W3", "S3"});
    log.Framed(1090, "W3");
    log.End(3000, std::nullopt);
    const std::vector<std::string> rows = log.TakeRows();
    const std::vector<std::string> expected = {"onscreen,1000,W3,opened,deck,80,", "onscreen,1100,E3,warning,uncovered,0,",
                                               "framed,1100,E3,warning,0.00,,",    "onscreen,1200,S3,warning,off,0,",
                                               "framed,1000,W3,opened,4.50,,",     "framed,1200,S3,warning,never,,"};
    Assert::IsTrue(expected == rows);
  }
};

} // namespace ClientTests
