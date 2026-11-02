#include "pch.h"

#include "GlyphAtlas.h"

#include "Font.h"

namespace Client
{

static_assert(GlyphAtlas::CELL_TEXELS == Content::FONT_CELL_ROWS, "a cell is the font's cell");
static_assert(Content::FONT_GLYPH_COUNT <= GlyphAtlas::COLUMNS * GlyphAtlas::ROWS, "every glyph has a cell");

GlyphAtlas::Cell GlyphAtlas::CellOf(std::size_t _index)
{
  if (_index >= std::size_t{COLUMNS} * ROWS)
  {
    throw std::out_of_range("the atlas has no cell for that glyph");
  }
  return {.xTexels = static_cast<std::uint32_t>(_index % COLUMNS) * CELL_TEXELS,
          .yTexels = static_cast<std::uint32_t>(_index / COLUMNS) * CELL_TEXELS};
}

std::vector<std::uint8_t> GlyphAtlas::Texels()
{
  std::vector<std::uint8_t> texels(std::size_t{WIDTH_TEXELS} * HEIGHT_TEXELS, 0);
  const std::span<const Content::Glyph> glyphs = Content::Glyphs();
  for (std::size_t index = 0; index < glyphs.size(); ++index)
  {
    const Content::Glyph& glyph = glyphs[index];
    const Cell cell = CellOf(index);
    for (std::size_t row = 0; row < glyph.rows.size(); ++row)
    {
      for (std::uint32_t x = 0; x < glyph.widthPixels; ++x)
      {
        if ((glyph.rows[row] >> x) & 1U)
        {
          const std::size_t y = cell.yTexels + glyph.topRow + row;
          texels.at((y * WIDTH_TEXELS) + cell.xTexels + x) = SET;
        }
      }
    }
  }
  return texels;
}

} // namespace Client
