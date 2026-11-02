#pragma once

#include <cstdint>
#include <span>

namespace Content
{

/// A glyph of Earthrise Mason (Hud.md §9; ADR-019, decision 1), baked from
/// GameData/Client/Fonts/EarthriseMason-glyphs.json. Its rows run from topRow down, each a mask with bit x set for a
/// pixel at column x, in font pixels.
struct Glyph
{
  char32_t codePoint;
  std::uint8_t widthPixels;  // font pixels
  std::uint8_t heightPixels; // the rows it has
  std::uint8_t topRow;       // its first row in the FONT_CELL_ROWS cell
  std::uint8_t advancePixels;
  bool icon;
  std::span<const std::uint16_t> rows; // bit x set for a pixel at column x
};

/// The cell every glyph sits in: one row above the caps and two below the baseline.
inline constexpr std::uint32_t FONT_CELL_ROWS = 10;
inline constexpr std::uint32_t FONT_BASELINE_ROW = 7;
inline constexpr std::uint32_t FONT_GLYPH_COUNT = 131; // M4-B5 added four icons

/// Every glyph, in ascending code point.
[[nodiscard]] std::span<const Glyph> Glyphs() noexcept;

/// The glyph for _codePoint, or nullptr when the font lacks it.
[[nodiscard]] const Glyph* FindGlyph(char32_t _codePoint) noexcept;

} // namespace Content
