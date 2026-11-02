#include "pch.h"

#include "Font.h"
#include "GlyphAtlas.h"
#include "TextLayout.h"
#include "UiScale.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Text on whole pixels (M3-E1; ADR-019, decisions 2, 4 and 5; Hud.md §2).

namespace ClientTests
{

TEST_CLASS(TextLayoutTests)
{
public:
  TEST_METHOD(MeasuresTheAdvancesLessTheLastGap)
  {
    // W and 2 are 5 wide with an advance of 6: 6 + 5 font pixels, 22 screen pixels at 2.
    Assert::AreEqual(22U, Client::TextLayout::Measure("W2", 2));
    Assert::AreEqual(0U, Client::TextLayout::Measure("", 2));
  }

  TEST_METHOD(LaysEveryQuadOnWholePixels)
  {
    std::vector<Client::ScreenQuad> quads;
    Client::TextLayout::Lay("W2 · ↺", 7, 11, 3, {1.0F, 1.0F, 1.0F, 1.0F}, quads);
    Assert::AreEqual(std::size_t{4}, quads.size()); // the space draws nothing
    for (const Client::ScreenQuad& quad : quads)
    {
      for (const float value : {quad.xPixels, quad.yPixels, quad.widthPixels, quad.heightPixels})
      {
        Assert::AreEqual(std::floor(value), value);
      }
      Assert::AreEqual(30.0F, quad.heightPixels);
      Assert::IsTrue(quad.textured);
    }
    Assert::AreEqual(7.0F, quads[0].xPixels);
    Assert::AreEqual(7.0F + 18.0F, quads[1].xPixels); // after W's advance of 6 at 3
  }

  TEST_METHOD(RefusesACodePointTheFontLacks)
  {
    Assert::ExpectException<std::invalid_argument>([] { static_cast<void>(Client::TextLayout::Measure("$5", 2)); });
    Assert::ExpectException<std::invalid_argument>([] { static_cast<void>(Client::TextLayout::CodePoints("\xff")); });
  }

  TEST_METHOD(PicksTheUiScaleFromTheDisplaysDpi)
  {
    // 120 DPI is 125 % and 168 DPI is 175 %, each the first of its band.
    Assert::AreEqual(1.0F, Client::UiScaleForDpi(96));
    Assert::AreEqual(1.0F, Client::UiScaleForDpi(119));
    Assert::AreEqual(1.5F, Client::UiScaleForDpi(120));
    Assert::AreEqual(1.5F, Client::UiScaleForDpi(144));
    Assert::AreEqual(2.0F, Client::UiScaleForDpi(168));
    Assert::AreEqual(2.0F, Client::UiScaleForDpi(192));
    Assert::AreEqual(1.5F, Client::UiScaleFromOption("1.5"));
    Assert::ExpectException<std::invalid_argument>([] { static_cast<void>(Client::UiScaleFromOption("1.25")); });
  }

  TEST_METHOD(KeepsNamesLargerThanLabelsAtEveryScale)
  {
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 3> expected = {{{2, 3}, {3, 4}, {4, 6}}};
    const std::array<float, 3> scales = {1.0F, 1.5F, 2.0F};
    for (std::size_t index = 0; index < scales.size(); ++index)
    {
      Assert::AreEqual(expected.at(index).first, Client::TextLayout::PixelsPerFontPixel(Client::TextSize::Label, scales.at(index)));
      Assert::AreEqual(expected.at(index).second, Client::TextLayout::PixelsPerFontPixel(Client::TextSize::Name, scales.at(index)));
    }
  }

  TEST_METHOD(BuildsTheAtlasFromTheGlyphsBitmaps)
  {
    // The digit 1's first row, ..#.., in its cell from its top row 1.
    const std::vector<std::uint8_t> texels = Client::GlyphAtlas::Texels();
    Assert::AreEqual(std::size_t{Client::GlyphAtlas::WIDTH_TEXELS} * Client::GlyphAtlas::HEIGHT_TEXELS, texels.size());
    const std::span<const Content::Glyph> glyphs = Content::Glyphs();
    const auto one = std::ranges::find(glyphs, U'1', &Content::Glyph::codePoint);
    const Client::GlyphAtlas::Cell cell = Client::GlyphAtlas::CellOf(static_cast<std::size_t>(one - glyphs.begin()));
    const std::size_t rowStart = static_cast<std::size_t>(cell.yTexels + 1) * Client::GlyphAtlas::WIDTH_TEXELS + cell.xTexels;
    const std::array<std::uint8_t, 5> row = {0, 0, Client::GlyphAtlas::SET, 0, 0};
    Assert::IsTrue(std::ranges::equal(row, std::span(texels).subspan(rowStart, row.size())));
  }
};

} // namespace ClientTests
