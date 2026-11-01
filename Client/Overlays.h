#pragma once

#include "HudAlerts.h"
#include "OverlayBox.h"
#include "Session.h"
#include "StateUpdate.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Client
{

/// The readability views, built from what the session was told (ADR-021, decisions 3 and 4): the collapse preview and
/// its alerts (M3-F1). The views are boxes for the overlay pipeline; the alerts are the HUD's, and they are raised
/// whether or not a view is on.
class Overlays
{
public:
  /// Reads the session's state for the frame at _nowSeconds: raises each collapse's alert, W2 WILL FALL IN n S while it
  /// is in warning, counting down, and W2 HAS FALLEN once it falls, both urgent and at the piece.
  void Update(const Session& _session, double _nowSeconds, HudAlerts& _alerts);

  /// The overlay layers for the frame at _seconds. While _integrityView is on, each collapse in warning has its piece
  /// in bad, pulsing over a second, and its strip's cells in bad on the ground under them.
  [[nodiscard]] std::vector<OverlayLayer> Layers(const Session& _session, double _seconds, bool _integrityView) const;

  /// A collapse's countdown, in whole seconds rounded up, named for _structure, or A WALL when it has none.
  [[nodiscard]] static std::string WillFallText(const Protocol::StateUpdate& _state, const Protocol::CollapseView& _collapse);
  [[nodiscard]] static std::string HasFallenText(const Protocol::StateUpdate& _state, const Protocol::CollapseView& _collapse);
};

} // namespace Client
