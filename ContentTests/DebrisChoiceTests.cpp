#include "pch.h"

#include "Catalog.h"
#include "DebrisChoice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The debris table as Content bakes it (M3-B8; GameDesign.md, M3's values, item 16), from GameData/Manifest.json's
// debrisByRubble.

namespace ContentTests
{

TEST_CLASS(DebrisChoiceTests)
{
public:
  TEST_METHOD(HoldsFourChoicesInAscendingRubbleIndex)
  {
    const std::span<const Content::DebrisChoice> choices = Content::DebrisChoices();
    Assert::AreEqual(std::size_t{4}, choices.size());
    constexpr std::array<std::uint8_t, 4> RUBBLE = {8, 25, 26, 28};
    for (std::size_t index = 0; index < RUBBLE.size(); ++index)
    {
      Assert::AreEqual(RUBBLE.at(index), choices[index].rubbleIndex);
    }
  }

  TEST_METHOD(ScattersBeamsAndSplintersUntintedOnTimberRubble)
  {
    const Content::DebrisChoice* choice = Content::FindDebrisChoice(25);
    Assert::IsNotNull(choice);
    constexpr std::array<std::string_view, 2> MODELS = {"D4", "D5"};
    Assert::IsTrue(std::ranges::equal(MODELS, choice->models));
    Assert::AreEqual(std::uint8_t{0}, choice->tintIndex);
  }

  TEST_METHOD(TintsTheStonesOnRockRubbleItsOwnColor)
  {
    const Content::DebrisChoice* choice = Content::FindDebrisChoice(28);
    Assert::IsNotNull(choice);
    constexpr std::array<std::string_view, 3> MODELS = {"D1", "D2", "D3"};
    Assert::IsTrue(std::ranges::equal(MODELS, choice->models));
    Assert::AreEqual(std::uint8_t{28}, choice->tintIndex);
  }

  TEST_METHOD(NamesOnlyModelsTheCatalogHolds)
  {
    for (const Content::DebrisChoice& choice : Content::DebrisChoices())
    {
      for (const std::string_view model : choice.models)
      {
        Assert::IsNotNull(Content::FindModel(model));
      }
    }
  }

  TEST_METHOD(HasNoChoiceForMasonry)
  {
    Assert::IsNull(Content::FindDebrisChoice(6));
  }
};

} // namespace ContentTests
