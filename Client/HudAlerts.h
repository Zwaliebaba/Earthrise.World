#pragma once

#include "HudModel.h"

#include <cstddef>
#include <vector>

namespace Client
{

/// The alerts the HUD shows (Hud.md §3 and §10): newest first, at most SHOWN, each until its time runs out. A fourth
/// pushes the oldest out.
class HudAlerts
{
public:
  static constexpr std::size_t SHOWN = 3;
  /// How long an alert that is not replaced stays, in seconds, as the HUD prototype keeps one.
  static constexpr double LIFETIME_SECONDS = 7.0;

  void Push(HudAlert _alert);

  /// Drops every alert whose time ran out by _nowSeconds.
  void Expire(double _nowSeconds);

  [[nodiscard]] const std::vector<HudAlert>& Shown() const noexcept;

private:
  std::vector<HudAlert> m_alerts;
};

} // namespace Client
