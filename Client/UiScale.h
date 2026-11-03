#pragma once

#include <cstdint>
#include <string_view>

namespace Client
{

/// The UI scale of Hud.md §2's bands (GameDesign.md, M3's values, item 9): 100 to 124 % of 96 DPI gives 1, 125 to
/// 174 % gives 1.5, and 175 % and above gives 2. Every HUD size is a logical size times it, rounded to a whole pixel.
[[nodiscard]] float UiScaleForDpi(std::uint32_t _dpi) noexcept;

/// The width layout A's deck needs, in logical pixels (Hud.md §2 and §3): its padding, the minimap, the selection at its
/// narrowest, the command card at its narrowest and the gaps between them.
inline constexpr float DECK_MIN_LOGICAL_PIXELS = 972.0F;

/// _uiScale capped at the largest of the steps, 2, 1.5 and 1, that leaves a target _widthPixels wide the deck's
/// DECK_MIN_LOGICAL_PIXELS (MvpPlan.md, M4, input 21): 1 at 1280 px and 1.5 at 1920 px, whatever the DPI or the option.
/// 1 when even that does not.
[[nodiscard]] float CappedUiScale(float _uiScale, std::uint32_t _widthPixels) noexcept;

/// The scale --ui-scale names: 1, 1.5 or 2. Throws std::invalid_argument for anything else.
[[nodiscard]] float UiScaleFromOption(std::string_view _value);

/// _logicalPixels at _uiScale, rounded to a whole pixel.
[[nodiscard]] std::int32_t ScaledPixels(float _logicalPixels, float _uiScale) noexcept;

} // namespace Client
