#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <string_view>

namespace Client
{

/// Where a point falls on the screen, against layout A's top bar and deck (Hud.md §3; MvpPlan.md, M4, input 25).
enum class ScreenPlace : std::uint8_t
{
  Uncovered,
  Deck,
  TopBar,
  Off
};

/// Where a structure is on a frame, for the trial's log's onscreen and framed rows (M4-J2).
struct Coverage
{
  ScreenPlace place;             // of its box's centre
  std::int32_t deckSharePercent; // of its footprint's projected box, the share the deck covers, 0 to 100
};

/// Where the structure in the box from _minMeters to _maxMeters is on a target _widthPixels by _heightPixels drawn with
/// _viewProjection, whose top _topBarPixels the top bar covers and whose bottom _deckPixels the deck does: its centre's
/// place, off behind the camera or outside the target; and the share of its footprint's projected box, the bounds of
/// its ground face's four corners on the screen, below the deck's top.
[[nodiscard]] Coverage CoverageOf(const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                  float _topBarPixels, float _deckPixels, const DirectX::XMFLOAT3& _minMeters,
                                  const DirectX::XMFLOAT3& _maxMeters) noexcept;

/// The log's word for each: uncovered, deck, topbar and off.
[[nodiscard]] std::string_view PlaceName(ScreenPlace _place) noexcept;

} // namespace Client
