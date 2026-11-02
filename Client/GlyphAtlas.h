#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Client
{

/// Where each glyph lies in the font's atlas (ADR-019, decision 2, amended by M4-B5): 16 by 9 cells of 10 x 10 texels,
/// 160 x 90, one cell per glyph in Content::Glyphs' order, each glyph's rows from its top row, 255 where a pixel is
/// set. The texels are built here once; FontAtlas uploads them.
class GlyphAtlas
{
public:
  static constexpr std::uint32_t COLUMNS = 16;
  static constexpr std::uint32_t ROWS = 9; // 131 glyphs overflow 8 rows' 128 cells (M4-B5)
  static constexpr std::uint32_t CELL_TEXELS = 10;
  static constexpr std::uint32_t WIDTH_TEXELS = COLUMNS * CELL_TEXELS;
  static constexpr std::uint32_t HEIGHT_TEXELS = ROWS * CELL_TEXELS;
  static constexpr std::uint8_t SET = 255;

  struct Cell
  {
    std::uint32_t xTexels; // its top-left texel
    std::uint32_t yTexels;
  };

  /// The cell of the glyph at _index in Content::Glyphs. Throws std::out_of_range beyond the atlas.
  [[nodiscard]] static Cell CellOf(std::size_t _index);

  /// Every texel, row by row from the top, WIDTH_TEXELS a row.
  [[nodiscard]] static std::vector<std::uint8_t> Texels();
};

} // namespace Client
