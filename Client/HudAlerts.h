#pragma once

#include "HudModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Client
{

/// The alerts the HUD shows (Hud.md §3 and §10): newest first, at most SHOWN, each until its time runs out. A fourth
/// pushes the oldest out. One raised under a key, such as a collapse's countdown, is replaced where it stands each time
/// it is raised again.
class HudAlerts
{
public:
  static constexpr std::size_t SHOWN = 3;
  /// How long an alert that is not replaced stays, in seconds, as the HUD prototype keeps one.
  static constexpr double LIFETIME_SECONDS = 7.0;

  void Push(HudAlert _alert);

  /// Replaces the alert last raised under _key, where it stands, or pushes _alert as the newest when there is none.
  void Raise(std::uint64_t _key, HudAlert _alert);

  /// Drops every alert whose time ran out by _nowSeconds.
  void Expire(double _nowSeconds);

  /// Newest first.
  [[nodiscard]] std::vector<HudAlert> Shown() const;

private:
  struct Entry
  {
    std::optional<std::uint64_t> key;
    HudAlert alert;
  };

  void PushEntry(Entry _entry);

  std::vector<Entry> m_entries;
};

} // namespace Client
