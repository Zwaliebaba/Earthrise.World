#include "pch.h"

#include "Font.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Earthrise Mason as Content bakes it (M3-B8; ADR-019, decision 1; Hud.md §9).

namespace ContentTests
{

namespace
{

constexpr char32_t FIRST_ICON = 0xE000;
constexpr char32_t LAST_ICON = 0xE018;
constexpr std::uint8_t ICON_PIXELS = 9;

} // namespace

TEST_CLASS(FontTests)
{
public:
  TEST_METHOD(HoldsItsGlyphsInAscendingCodePoint)
  {
    const std::span<const Content::Glyph> glyphs = Content::Glyphs();
    Assert::AreEqual(std::size_t{Content::FONT_GLYPH_COUNT}, glyphs.size());
    Assert::IsTrue(std::ranges::is_sorted(glyphs, std::ranges::less{}, &Content::Glyph::codePoint));
    Assert::IsTrue(std::ranges::adjacent_find(glyphs, std::ranges::equal_to{}, &Content::Glyph::codePoint) == glyphs.end());
  }

  TEST_METHOD(BakesTheDigitOneAsItsBitmap)
  {
    // ..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###.
    const Content::Glyph* one = Content::FindGlyph(U'1');
    Assert::IsNotNull(one);
    constexpr std::array<std::uint16_t, 7> ROWS = {4, 6, 4, 4, 4, 4, 14};
    Assert::IsTrue(std::ranges::equal(ROWS, one->rows));
    Assert::AreEqual(std::uint8_t{7}, one->heightPixels);
    Assert::AreEqual(std::uint8_t{5}, one->widthPixels);
    Assert::AreEqual(std::uint8_t{1}, one->topRow);
    Assert::AreEqual(std::uint8_t{6}, one->advancePixels);
    Assert::IsFalse(one->icon);
  }

  TEST_METHOD(LacksTheDollarSign)
  {
    Assert::IsNull(Content::FindGlyph(U'$'));
  }

  TEST_METHOD(HoldsEveryCommandIconNineWide)
  {
    for (char32_t code = FIRST_ICON; code <= LAST_ICON; ++code)
    {
      const Content::Glyph* icon = Content::FindGlyph(code);
      Assert::IsNotNull(icon);
      Assert::IsTrue(icon->icon);
      Assert::AreEqual(ICON_PIXELS, icon->widthPixels);
    }
  }

  TEST_METHOD(FitsEveryGlyphInTheCell)
  {
    for (const Content::Glyph& glyph : Content::Glyphs())
    {
      Assert::AreEqual(std::size_t{glyph.heightPixels}, glyph.rows.size());
      Assert::IsTrue(std::uint32_t{glyph.topRow} + glyph.heightPixels <= Content::FONT_CELL_ROWS);
      Assert::IsTrue(std::ranges::all_of(glyph.rows, [&glyph](std::uint16_t _row) { return (_row >> glyph.widthPixels) == 0; }));
    }
  }
};

} // namespace ContentTests
