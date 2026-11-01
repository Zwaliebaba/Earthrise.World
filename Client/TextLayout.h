#pragma once

#include "ScreenQuad.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Client
{

/// The two sizes of Hud.md §2: labels and values, and names, icons and big numbers.
enum class TextSize : std::uint8_t
{
  Label,
  Name
};

/// Text laid out on whole pixels (ADR-019, decision 4): a UTF-8 string measured and placed with Earthrise Mason's metrics
/// at a scale that is a whole number of screen pixels per font pixel, each glyph one quad of the atlas. A code point
/// the font lacks is a defect, not a fallback: it throws.
class TextLayout
{
public:
  /// Screen pixels per font pixel for _size at _uiScale (Hud.md §2): labels 2, 3 and 4, names 3, 4 and 6, at 1, 1.5 and 2.
  [[nodiscard]] static std::uint32_t PixelsPerFontPixel(TextSize _size, float _uiScale) noexcept;

  /// The code points of _utf8. Throws std::invalid_argument when it is not UTF-8.
  [[nodiscard]] static std::vector<char32_t> CodePoints(std::string_view _utf8);

  /// The width in screen pixels of _utf8 at _scale: the glyphs' advances, less the last glyph's gap after it. Throws
  /// std::invalid_argument for a code point the font lacks.
  [[nodiscard]] static std::uint32_t Measure(std::string_view _utf8, std::uint32_t _scale);

  /// The height of a line's cell at _scale, in screen pixels: the font's cell of ten rows.
  [[nodiscard]] static std::uint32_t CellPixels(std::uint32_t _scale) noexcept;

  /// Appends a quad for each glyph of _utf8 that has a pixel set, its cell's top-left corner starting at (_xPixels,
  /// _yPixels), in _color. Throws std::invalid_argument for a code point the font lacks.
  static void Lay(std::string_view _utf8, std::int32_t _xPixels, std::int32_t _yPixels, std::uint32_t _scale,
                  const std::array<float, 4>& _color, std::vector<ScreenQuad>& _quads);
};

} // namespace Client
