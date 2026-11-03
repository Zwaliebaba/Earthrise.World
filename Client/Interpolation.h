#pragma once

#include <DirectXMath.h>

#include "FixedPosition.h"

#include <limits>

namespace Client
{

/// A unit that moved further than this in a tick is drawn at its new place at once: a drop, a lift or a jump, where a
/// step moves a figure about 7 cm (MvpPlan.md, M4, input 20).
inline constexpr float UNIT_SNAP_METERS = 1.5F;
/// A shot never snaps: an arrow flies about 2 m a tick and a stone more.
inline constexpr float NEVER_SNAP_METERS = std::numeric_limits<float>::infinity();

/// _position in meters, as the renderer takes them.
[[nodiscard]] DirectX::XMFLOAT3 Meters(const Protocol::FixedPosition& _position) noexcept;

/// Where an entity is drawn (MvpPlan.md, M4, input 20; ADR-020, decision 4, amended): between its positions in the
/// previous and the last update at the clock's fraction of a tick, so a tick behind, or at its last when it had none
/// before, _previous null, or moved more than _snapMeters in the tick. Units pass UNIT_SNAP_METERS and shots
/// NEVER_SNAP_METERS. _tickFraction is clamped to 0 to 1.
[[nodiscard]] DirectX::XMFLOAT3 DrawnPosition(const Protocol::FixedPosition* _previous, const Protocol::FixedPosition& _last,
                                              double _tickFraction, float _snapMeters) noexcept;

} // namespace Client
