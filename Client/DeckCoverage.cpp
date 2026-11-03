#include "pch.h"

#include "DeckCoverage.h"

#include "Selection.h"

namespace Client
{

Coverage CoverageOf(const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                    float _topBarPixels, float _deckPixels, const DirectX::XMFLOAT3& _minMeters,
                    const DirectX::XMFLOAT3& _maxMeters) noexcept
{
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  const float deckTop = height - _deckPixels;
  // The centre's place.
  const DirectX::XMFLOAT3 center{0.5F * (_minMeters.x + _maxMeters.x), 0.5F * (_minMeters.y + _maxMeters.y),
                                 0.5F * (_minMeters.z + _maxMeters.z)};
  ScreenPlace place = ScreenPlace::Off;
  if (const std::optional<DirectX::XMFLOAT2> at = Selector::Project(_viewProjection, center, _widthPixels, _heightPixels);
      at.has_value() && at->x >= 0.0F && at->x <= width && at->y >= 0.0F && at->y <= height)
  {
    place = at->y < _topBarPixels ? ScreenPlace::TopBar : (at->y > deckTop ? ScreenPlace::Deck : ScreenPlace::Uncovered);
  }
  // The footprint's projected box: its ground face's four corners' bounds, and the share of it below the deck's top.
  float top = std::numeric_limits<float>::max();
  float bottom = std::numeric_limits<float>::lowest();
  bool behind = false;
  for (const auto& [x, z] : {std::pair{_minMeters.x, _minMeters.z}, std::pair{_maxMeters.x, _minMeters.z},
                             std::pair{_minMeters.x, _maxMeters.z}, std::pair{_maxMeters.x, _maxMeters.z}})
  {
    const std::optional<DirectX::XMFLOAT2> corner = Selector::Project(_viewProjection, {x, _minMeters.y, z}, _widthPixels, _heightPixels);
    if (!corner.has_value())
    {
      behind = true;
      continue;
    }
    top = std::min(top, corner->y);
    bottom = std::max(bottom, corner->y);
  }
  std::int32_t share = 0;
  if (!behind && bottom > top)
  {
    const float covered = std::clamp(bottom - std::max(top, deckTop), 0.0F, bottom - top);
    share = static_cast<std::int32_t>(std::lround(100.0F * covered / (bottom - top)));
  }
  else if (!behind && bottom >= deckTop)
  {
    share = 100; // a footprint seen edge on, below the deck's top
  }
  return {.place = place, .deckSharePercent = share};
}

std::string_view PlaceName(ScreenPlace _place) noexcept
{
  switch (_place)
  {
  case ScreenPlace::Uncovered:
    return "uncovered";
  case ScreenPlace::Deck:
    return "deck";
  case ScreenPlace::TopBar:
    return "topbar";
  case ScreenPlace::Off:
    break;
  }
  return "off";
}

} // namespace Client
