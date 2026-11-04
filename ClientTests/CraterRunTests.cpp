#include "pch.h"

#include "CraterModel.h"
#include "LabModel.h"
#include "Strings.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The crater items' flow (M5-D2; GameDesign.md, M5's values, item 12): the practice item first and the rest in the
// participant's order, each asked once it is set up, with no time limit and no result shown, and a row an answer.

namespace ClientTests
{

namespace
{

std::vector<Client::CraterItemSpec> Items()
{
  std::vector<Client::CraterItemSpec> items;
  const std::array<bool, 9> ram = {false, true, false, false, true, true, false, true, false}; // as Simulation::CraterItems
  items.reserve(ram.size());
  for (std::size_t index = 0; index < ram.size(); ++index)
  {
    items.push_back({.id = std::format("C{}", index), .keyRam = ram.at(index)});
  }
  return items;
}

std::vector<std::string> Fields(const std::string& _row)
{
  std::vector<std::string> fields;
  std::string field;
  for (const char character : _row)
  {
    if (character == ',' || character == '\n')
    {
      fields.push_back(std::exchange(field, {}));
    }
    else
    {
      field += character;
    }
  }
  return fields;
}

} // namespace

TEST_CLASS(CraterRunTests)
{
public:
  TEST_METHOD(ThePracticeItemComesFirstAndTheRestInTheSeedsOrder)
  {
    const std::vector<std::size_t> order = Client::LabModel::Order(9, 1);
    Client::CraterModel model(Items(), 1);
    std::vector<std::string> asked;
    for (std::size_t item = 0; item < 9; ++item)
    {
      const Client::CraterItemSpec* current = model.Current();
      Assert::IsTrue(current != nullptr);
      asked.push_back(current != nullptr ? current->id : std::string());
      model.Ready(0.0);
      Assert::IsTrue(model.Answer(Client::CraterAnswer::Ram, 1.0).has_value());
    }
    Assert::AreEqual(std::string("C0"), asked.front());
    for (std::size_t item = 0; item < 9; ++item)
    {
      Assert::AreEqual(std::format("C{}", order.at(item)), asked.at(item));
    }
    Assert::IsTrue(model.Finished());
    Assert::IsTrue(model.Current() == nullptr);
    Assert::AreEqual(std::string(Client::CRATER_FINISHED_BANNER), model.Mode().banner);
    Assert::IsTrue(Client::LabModel::Order(9, 1) != Client::LabModel::Order(9, 2));
  }

  TEST_METHOD(AnAnswerBeforeTheItemIsUpIsIgnored)
  {
    Client::CraterModel model(Items(), 7);
    Assert::IsFalse(model.Asking());
    Assert::AreEqual(std::string(Client::CRATER_SETTING_UP_BANNER), model.Mode().banner);
    Assert::IsFalse(model.Answer(Client::CraterAnswer::Ram, 1.0).has_value());
    Assert::AreEqual(std::size_t{1}, model.Started());
    model.Ready(2.0);
    Assert::IsTrue(model.Asking());
    const Client::HudMode question = model.Mode();
    Assert::AreEqual(std::string(Client::CRATER_QUESTION_BANNER), question.banner);
    Assert::AreEqual(std::size_t{2}, question.buttons.size());
    Assert::AreEqual(std::string(Client::RAM_BUTTON), question.buttons.front().label);
    Assert::AreEqual(std::string(Client::PRACTICE_OBJECTIVE), model.Objective().title);
    const std::optional<Client::CraterItemRow> row = model.Answer(Client::CraterAnswer::Trebuchet, 6.5);
    Assert::IsTrue(row.has_value());
    Assert::AreEqual(4.5, row.value_or(Client::CraterItemRow{}).secondsToAnswer, 1e-9);
    Assert::IsFalse(model.Asking()); // the next is set up, and nothing is answered back
    Assert::AreEqual(std::string("ITEM 1 OF 8"), model.Objective().title);
  }

  TEST_METHOD(ARowHasEveryColumnAndItsCorrectAgreesWithItsKey)
  {
    const std::string header = Client::CraterModel::LogHeader(
      {.participant = "S03", .buildId = 0x1234, .serverDataHash = 0xabcd, .itemsHash = 0x42, .startTime = "2026-11-04 09:00:00 UTC"});
    Assert::IsTrue(header.find("# participant: S03\n") != std::string::npos);
    Assert::IsTrue(header.ends_with("kind,id,key,answer,correct,secondsToAnswer\n"));
    const Client::CraterItemRow right{.id = "C4", .keyRam = true, .answer = Client::CraterAnswer::Ram, .secondsToAnswer = 3.25};
    const Client::CraterItemRow wrong{.id = "C2", .keyRam = false, .answer = Client::CraterAnswer::Ram, .secondsToAnswer = 1.0};
    Assert::IsTrue(right.Correct());
    Assert::IsFalse(wrong.Correct());
    const std::vector<std::string> fields = Fields(Client::CraterModel::ItemRowText(right));
    Assert::IsTrue(std::vector<std::string>{"item", "C4", "ram", "ram", "1", "3.250"} == fields);
    Assert::IsTrue(std::vector<std::string>{"item", "C2", "trebuchet", "ram", "0", "1.000"} ==
                   Fields(Client::CraterModel::ItemRowText(wrong)));
  }
};

} // namespace ClientTests
