#include "pch.h"

#include "Font.h"

#include "FontTable.h"

namespace Content
{

static_assert(Generated::GLYPHS.size() == FONT_GLYPH_COUNT);
static_assert(Generated::FONT_CELL == FONT_CELL_ROWS);
static_assert(Generated::FONT_BASELINE == FONT_BASELINE_ROW);

std::span<const Glyph> Glyphs() noexcept
{
  return Generated::GLYPHS;
}

const Glyph* FindGlyph(char32_t _codePoint) noexcept
{
  // The bake writes the glyphs in ascending code point.
  const std::span<const Glyph> glyphs = Glyphs();
  const auto found = std::ranges::lower_bound(glyphs, _codePoint, {}, &Glyph::codePoint);
  return found != glyphs.end() && found->codePoint == _codePoint ? &*found : nullptr;
}

} // namespace Content
