#include "pch.h"

#include "TextLayout.h"

#include "Font.h"
#include "GlyphAtlas.h"

namespace Client
{

namespace
{

constexpr std::uint32_t LABEL_PIXELS_AT_ONE = 2;
constexpr std::uint32_t NAME_PIXELS_AT_ONE = 3;

/// The glyph for _codePoint and its index in Content::Glyphs, or throws for one the font lacks.
std::pair<const Content::Glyph*, std::size_t> GlyphOf(char32_t _codePoint)
{
  const std::span<const Content::Glyph> glyphs = Content::Glyphs();
  const auto found = std::ranges::lower_bound(glyphs, _codePoint, {}, &Content::Glyph::codePoint);
  if (found == glyphs.end() || found->codePoint != _codePoint)
  {
    throw std::invalid_argument(std::format("Earthrise Mason has no glyph for U+{:04X}", static_cast<std::uint32_t>(_codePoint)));
  }
  return {&*found, static_cast<std::size_t>(found - glyphs.begin())};
}

} // namespace

std::uint32_t TextLayout::PixelsPerFontPixel(TextSize _size, float _uiScale) noexcept
{
  // Rounded down to a whole pixel, so names stay larger than labels at 1.5 (Hud.md §2).
  const std::uint32_t atOne = _size == TextSize::Name ? NAME_PIXELS_AT_ONE : LABEL_PIXELS_AT_ONE;
  return std::max(1U, static_cast<std::uint32_t>(std::floor(static_cast<float>(atOne) * _uiScale)));
}

std::vector<char32_t> TextLayout::CodePoints(std::string_view _utf8)
{
  std::vector<char32_t> codePoints;
  for (std::size_t at = 0; at < _utf8.size();)
  {
    const auto lead = static_cast<std::uint8_t>(_utf8[at]);
    const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
    if (length == 0 || at + length > _utf8.size())
    {
      throw std::invalid_argument("text that is not UTF-8");
    }
    char32_t codePoint = length == 1 ? lead : lead & (0xFFU >> (length + 1));
    for (std::size_t next = 1; next < length; ++next)
    {
      const auto continuation = static_cast<std::uint8_t>(_utf8[at + next]);
      if ((continuation >> 6) != 0x2)
      {
        throw std::invalid_argument("text that is not UTF-8");
      }
      codePoint = (codePoint << 6) | (continuation & 0x3FU);
    }
    codePoints.push_back(codePoint);
    at += length;
  }
  return codePoints;
}

std::uint32_t TextLayout::Measure(std::string_view _utf8, std::uint32_t _scale)
{
  std::uint32_t fontPixels = 0;
  std::uint32_t lastGap = 0;
  for (const char32_t codePoint : CodePoints(_utf8))
  {
    const Content::Glyph& glyph = *GlyphOf(codePoint).first;
    fontPixels += glyph.advancePixels;
    lastGap = glyph.advancePixels > glyph.widthPixels ? glyph.advancePixels - glyph.widthPixels : 0U;
  }
  return (fontPixels - lastGap) * _scale;
}

std::uint32_t TextLayout::CellPixels(std::uint32_t _scale) noexcept
{
  return Content::FONT_CELL_ROWS * _scale;
}

void TextLayout::Lay(std::string_view _utf8, std::int32_t _xPixels, std::int32_t _yPixels, std::uint32_t _scale,
                     const std::array<float, 4>& _color, std::vector<ScreenQuad>& _quads)
{
  const auto scale = static_cast<std::int32_t>(_scale);
  std::int32_t pen = _xPixels;
  for (const char32_t codePoint : CodePoints(_utf8))
  {
    const auto [glyph, index] = GlyphOf(codePoint);
    if (std::ranges::any_of(glyph->rows, [](std::uint16_t _row) { return _row != 0; }))
    {
      const GlyphAtlas::Cell cell = GlyphAtlas::CellOf(index);
      _quads.push_back({.xPixels = static_cast<float>(pen),
                        .yPixels = static_cast<float>(_yPixels),
                        .widthPixels = static_cast<float>(glyph->widthPixels * _scale),
                        .heightPixels = static_cast<float>(CellPixels(_scale)),
                        .u0 = static_cast<float>(cell.xTexels),
                        .v0 = static_cast<float>(cell.yTexels),
                        .u1 = static_cast<float>(cell.xTexels + glyph->widthPixels),
                        .v1 = static_cast<float>(cell.yTexels + GlyphAtlas::CELL_TEXELS),
                        .color = _color,
                        .textured = true});
    }
    pen += static_cast<std::int32_t>(glyph->advancePixels) * scale;
  }
}

} // namespace Client
