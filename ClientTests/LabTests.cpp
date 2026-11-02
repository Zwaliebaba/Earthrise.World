#include "pch.h"

#include "LabModel.h"

#include <set>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The breach lab's flow and its log (M3-G1; GameDesign.md, M3's values, items 2 to 4), with no window.

namespace ClientTests
{

namespace
{

/// P0 and L01 to L16, each asking _shots shots.
std::vector<Client::LabItemSpec> Items(std::uint32_t _shots)
{
  std::vector<Client::LabItemSpec> items;
  items.push_back({.id = "P0", .shotsAsked = 1, .markVoxels = {380, 9, 180}});
  for (std::uint32_t index = 1; index <= 16; ++index)
  {
    items.push_back(
      {.id = std::format("L{:02}", index), .shotsAsked = _shots, .markVoxels = {378 + static_cast<std::int32_t>(index % 5), 9, 180}});
  }
  return items;
}

/// Runs the current item through to its result, answering _answer, and on to the next.
void RunItem(Client::LabModel& _lab, Client::LabAnswer _answer)
{
  _lab.PriorShotsDone(10.0);
  Assert::IsTrue(_lab.Answer(_answer, 12.5, true, false));
  _lab.AskedShotsDone(true);
  Assert::IsTrue(_lab.Next(1, 1, true).has_value());
}

/// The columns of a CSV row, its line ending dropped.
std::vector<std::string> Columns(std::string_view _row)
{
  Assert::IsTrue(_row.ends_with('\n'));
  _row.remove_suffix(1);
  std::vector<std::string> columns;
  std::size_t start = 0;
  while (true)
  {
    const std::size_t comma = _row.find(',', start);
    columns.emplace_back(_row.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start));
    if (comma == std::string_view::npos)
    {
      return columns;
    }
    start = comma + 1;
  }
}

} // namespace

TEST_CLASS(LabTests)
{
public:
  TEST_METHOD(ShufflesTheItemsAfterThePracticeByTheParticipantsSeed)
  {
    const std::vector<std::size_t> first = Client::LabModel::Order(17, 1);
    Assert::IsTrue(first == std::vector<std::size_t>{0, 10, 8, 14, 7, 9, 5, 15, 11, 4, 6, 13, 3, 16, 2, 12, 1});
    const std::vector<std::size_t> second = Client::LabModel::Order(17, 2);
    Assert::IsTrue(first != second);
    Assert::AreEqual(std::size_t{0}, second.front()); // the practice item always first
    Assert::AreEqual(std::size_t{17}, std::set<std::size_t>(second.begin(), second.end()).size());
  }

  TEST_METHOD(IgnoresAnAnswerBeforeTheQuestionIsUp)
  {
    Client::LabModel lab(Items(3), 1);
    Assert::IsTrue(lab.Phase() == Client::LabPhase::FreeFire);
    Assert::IsTrue(lab.FireAllowed());
    Assert::IsFalse(lab.Answer(Client::LabAnswer::Open, 1.0, true, false));
    lab.FreeFireTicked(Client::LabModel::FREE_FIRE_TICKS - 1);
    Assert::IsTrue(lab.Phase() == Client::LabPhase::FreeFire);
    lab.FreeFireTicked(Client::LabModel::FREE_FIRE_TICKS);
    Assert::IsTrue(lab.Phase() == Client::LabPhase::Watching);
    Assert::IsFalse(lab.FireAllowed());
    Assert::AreEqual(Client::LabModel::FAST_FORWARD_SPEED, lab.Speed());
    Assert::IsFalse(lab.Answer(Client::LabAnswer::Open, 2.0, true, false)); // the prior shots still run
    lab.PriorShotsDone(3.0);
    Assert::IsTrue(lab.Paused());
    Assert::IsTrue(lab.Answer(Client::LabAnswer::Shut, 4.0, true, false));
    Assert::IsFalse(lab.Answer(Client::LabAnswer::Open, 4.5, true, false)); // once only
    Assert::IsTrue(lab.PassabilityForced());
    Assert::IsFalse(lab.Next(1, 1, false).has_value()); // the shots still run
  }

  TEST_METHOD(ReadsTheBannerAndTheObjectiveForItem3Of16)
  {
    Client::LabModel lab(Items(3), 1);
    Assert::AreEqual(std::string("FREE FIRE · 1:59 LEFT"), lab.Mode(20).banner);
    Assert::IsFalse(lab.Objective().has_value());
    lab.FreeFireTicked(Client::LabModel::FREE_FIRE_TICKS);
    Assert::AreEqual(std::string("WATCH"), lab.Mode(0).banner);
    Assert::AreEqual(std::string("PRACTICE"), lab.Objective().value_or(Client::HudObjective{}).title);
    lab.PriorShotsDone(1.0);
    Assert::AreEqual(std::string("WILL W2 BE OPEN TO FOOT AFTER 1 SHOT AT THE MARK?"), lab.Mode(0).banner);
    lab.Answer(Client::LabAnswer::Open, 2.0, true, false);
    lab.AskedShotsDone(false);
    Assert::AreEqual(std::string("W2 IS SHUT · SPACE FOR THE NEXT"), lab.Mode(0).banner);
    Assert::IsTrue(lab.Next(1, 1, false).has_value());
    RunItem(lab, Client::LabAnswer::Open); // item 1
    RunItem(lab, Client::LabAnswer::Shut); // item 2
    lab.PriorShotsDone(30.0);
    Assert::AreEqual(std::string("ITEM 3 OF 16"), lab.Objective().value_or(Client::HudObjective{}).title);
    const Client::HudMode mode = lab.Mode(0);
    Assert::AreEqual(std::string("WILL W2 BE OPEN TO FOOT AFTER 3 SHOTS AT THE MARK?"), mode.banner);
    Assert::AreEqual(std::size_t{2}, mode.buttons.size());
    Assert::IsTrue(mode.buttons.at(0).control == Client::HudControl::AnswerOpen);
    Assert::AreEqual(std::string("OPEN · O"), mode.buttons.at(0).label);
    Assert::IsTrue(mode.buttons.at(1).control == Client::HudControl::AnswerShut);
    Assert::AreEqual(std::string("SHUT · H"), mode.buttons.at(1).label);
    // The item is the third of the participant's order after the practice item.
    Assert::AreEqual(std::string("L14"), lab.Current()->id);
  }

  TEST_METHOD(WritesAnAnsweredItemsRowWithEveryColumn)
  {
    Client::LabModel lab(Items(2), 1);
    lab.FreeFireTicked(Client::LabModel::FREE_FIRE_TICKS);
    lab.PriorShotsDone(100.0);
    lab.NoteCamera(1, 2.0);
    lab.NoteCamera(-1, -0.5);
    Assert::IsTrue(lab.Answer(Client::LabAnswer::Open, 104.25, true, false));
    lab.NoteCamera(1, 1.0); // after the answer: not counted
    lab.AskedShotsDone(true);
    const std::optional<Client::LabItemRow> row = lab.Next(0x0123456789abcdefULL, 0x0123456789abcdefULL, true);
    Assert::IsTrue(row.has_value());
    const std::vector<std::string> columns = Columns(Client::LabModel::ItemRowText(row.value_or(Client::LabItemRow{})));
    Assert::IsTrue(columns == std::vector<std::string>{"item", "P0", "1", "open", "open", "1", "4.25", "1", "0", "2", "2.5",
                                                       "0123456789abcdef", "0123456789abcdef", "1"});
    // The header names the same columns, after the comment lines.
    const std::string header = Client::LabModel::LogHeader({.participant = "P03",
                                                            .buildId = 1,
                                                            .serverDataHash = 2,
                                                            .itemsHash = 3,
                                                            .widthPixels = 1920,
                                                            .heightPixels = 1080,
                                                            .uiScale = 1.5F,
                                                            .startTime = "2026-11-01 12:00:00"});
    Assert::IsTrue(header.starts_with("# participant: P03\n"));
    Assert::IsTrue(header.find("# item-set hash: 0000000000000003\n") != std::string::npos);
    Assert::IsTrue(header.ends_with("kind,id,n,key,answer,correct,secondsToAnswer,integrityOn,passabilityOn,cameraTurns,zoomNotches,"
                                    "liveHash,headlessHash,matched\n"));
    Assert::AreEqual(columns.size(), Columns(header.substr(header.rfind("kind,"))).size());

    // A wrong answer and a hash that did not match.
    lab.PriorShotsDone(200.0);
    lab.Answer(Client::LabAnswer::Shut, 201.0, false, true);
    lab.AskedShotsDone(true);
    const Client::LabItemRow wrong = lab.Next(1, 2, true).value_or(Client::LabItemRow{});
    const std::vector<std::string> wrongColumns = Columns(Client::LabModel::ItemRowText(wrong));
    Assert::AreEqual(std::string("0"), wrongColumns.at(5));  // correct
    Assert::AreEqual(std::string("0"), wrongColumns.at(13)); // matched
    Assert::AreEqual(std::string("command,120,6.00,FireEngine 380 9 180,click\n"),
                     Client::LabModel::CommandRowText(120, 6.0, "FireEngine 380 9 180", "click"));
  }
};

} // namespace ClientTests
