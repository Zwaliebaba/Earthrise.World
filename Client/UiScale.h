#pragma once

#include <cstdint>
#include <string_view>

namespace Client
{

/// The UI scale of Hud.md §2's bands (GameDesign.md, M3's values, item 9): 100 to 124 % of 96 DPI gives 1, 125 to
/// 174 % gives 1.5, and 175 % and above gives 2. Every HUD size is a logical size times it, rounded to a whole pixel.
[[nodiscard]] float UiScaleForDpi(std::uint32_t _dpi) noexcept;

/// The scale --ui-scale names: 1, 1.5 or 2. Throws std::invalid_argument for anything else.
[[nodiscard]] float UiScaleFromOption(std::string_view _value);

/// _logicalPixels at _uiScale, rounded to a whole pixel.
[[nodiscard]] std::int32_t ScaledPixels(float _logicalPixels, float _uiScale) noexcept;

} // namespace Client
