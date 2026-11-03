#pragma once

#include "HudModel.h"
#include "StateUpdate.h"

#include <optional>
#include <vector>

namespace Client
{

/// What the Siege trial puts over the HUD's own (Hud.md §10; M4-J1), from the update alone, and its pause (MvpPlan.md,
/// M4, input 25), which Earthrise's loop drives each frame.
class SiegeModel
{
public:
  /// TAKE THE KEEP, until the storming clock runs: then HOLD THE KEEP'S FOOT, its detail the clock and what it needs as
  /// 0:42 / 2:00, in off text while the foot is contested, and its progress the clock's share.
  [[nodiscard]] static HudObjective Objective(const Protocol::StateUpdate& _state);

  /// STONE m³, the player's stockpile to a tenth, in bad below one shot's stone; ARMY n, the player's figures and
  /// engines in the update; and TIME m:ss, the time limit left.
  [[nodiscard]] static std::vector<HudResource> Resources(const Protocol::StateUpdate& _state);

  /// Once the trial has ended, THE KEEP HAS FALLEN, TIME HAS RUN OUT or YOUR ARMY IS GONE, with the game time it ended
  /// at; nothing while it runs.
  [[nodiscard]] static std::optional<HudMode> EndBanner(const Protocol::StateUpdate& _state);

  /// Pause or F10: the clock stops, and nothing else does. The camera, the selection, the card and the views go on.
  void TogglePause() noexcept;
  [[nodiscard]] bool Paused() const noexcept;
  /// PAUSED, while paused.
  [[nodiscard]] std::optional<HudMode> PauseBanner() const;

private:
  bool m_paused = false;
};

} // namespace Client
