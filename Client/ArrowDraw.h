#pragma once

#include <DirectXMath.h>

#include "StateUpdate.h"

#include <cstdint>
#include <vector>

namespace Client
{

/// One arrow as the line pass draws it (M4-H2; ADR-018, decision 3), from its shot's last two positions.
struct ArrowDrawn
{
  DirectX::XMFLOAT3 middleMeters; // between its last two positions at the clock's fraction, never snapped
  DirectX::XMFLOAT3 direction;    // unit length, from its tail to its head: along its flight
  std::uint8_t sideColorIndex;    // its shooter's faction entry (Look.h)
};

/// Replaces _arrows with each arrow in flight in _last that was in flight in _previous too: drawn between the two at
/// _tickFraction, a tick behind, never snapped (MvpPlan.md, M4, input 20), pointing from the first to the second. An
/// arrow flies a straight line, so its last two positions give its flight; one new in _last has only one and is drawn
/// from its next update, a tick later. Stones are voxel models (WorldModels).
void PlaceArrows(const Protocol::StateUpdate& _previous, const Protocol::StateUpdate& _last, double _tickFraction,
                 std::vector<ArrowDrawn>& _arrows);

} // namespace Client
