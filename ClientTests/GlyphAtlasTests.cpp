#include "pch.h"

#include "Font.h"
#include "GlyphAtlas.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The glyph atlas's cells (ADR-019, decision 2, amended by M4-B5): 16 by 9 cells of 10 texels, one a glyph in code-point
// order.

namespace ClientTests
{

TEST_CLASS(GlyphAtlasTests)
{
public:
  TEST_METHOD(PutsTheHundredAndThirtyFirstGlyphInTheNinthRowsThirdCell)
  {
    Assert::AreEqual(std::size_t{131}, std::size_t{Content::FONT_GLYPH_COUNT});
    const Client::GlyphAtlas::Cell cell = Client::GlyphAtlas::CellOf(Content::FONT_GLYPH_COUNT - 1);
    Assert::AreEqual(20U, cell.xTexels); // the third cell
    Assert::AreEqual(80U, cell.yTexels); // the ninth row
  }

  TEST_METHOD(IsTallEnoughForEveryGlyph)
  {
    Assert::AreEqual(160U, Client::GlyphAtlas::WIDTH_TEXELS);
    Assert::AreEqual(90U, Client::GlyphAtlas::HEIGHT_TEXELS);
    Assert::ExpectException<std::out_of_range>([] { static_cast<void>(Client::GlyphAtlas::CellOf(std::size_t{16} * 9)); });
  }

  TEST_METHOD(DrawsTheWallIconInItsCell)
  {
    // WALL, U+E01C, the 131st glyph: its second row is merlons, #.#.#.#.#, 9 wide, at the cell's second texel row.
    const std::vector<std::uint8_t> texels = Client::GlyphAtlas::Texels();
    const Client::GlyphAtlas::Cell cell = Client::GlyphAtlas::CellOf(Content::FONT_GLYPH_COUNT - 1);
    for (std::uint32_t x = 0; x < 9; ++x)
    {
      const std::size_t at = (std::size_t{cell.yTexels + 1} * Client::GlyphAtlas::WIDTH_TEXELS) + cell.xTexels + x;
      Assert::AreEqual(x % 2 == 0 ? Client::GlyphAtlas::SET : std::uint8_t{0}, texels.at(at));
    }
  }
};

} // namespace ClientTests
