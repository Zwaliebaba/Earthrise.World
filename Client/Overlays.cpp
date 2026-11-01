#include "pch.h"

#include "Overlays.h"

#include "Look.h"
#include "Strings.h"
#include "TickClock.h"

#include <limits>
#include <numbers>

namespace Client
{

namespace
{

/// The structure's name, or A WALL when the collapse has none or the state no longer holds it.
std::string_view NameOf(const Protocol::StateUpdate& _state, const Protocol::CollapseView& _collapse) noexcept
{
  const auto found = std::ranges::find(_state.structures, _collapse.structure, &Protocol::StructureView::id);
  return found == _state.structures.end() || found->name.empty() ? UNNAMED_WALL : std::string_view(found->name);
}

/// Where the camera jumps for a collapse: the middle of its piece's box, at its foot.
Protocol::VoxelPosition PieceFoot(const Protocol::CollapseView& _collapse) noexcept
{
  if (_collapse.piece.empty())
  {
    return {};
  }
  Protocol::VoxelPosition low = _collapse.piece.front();
  Protocol::VoxelPosition high = low;
  for (const Protocol::VoxelPosition& voxel : _collapse.piece)
  {
    low = {std::min(low.xVoxels, voxel.xVoxels), std::min(low.yVoxels, voxel.yVoxels), std::min(low.zVoxels, voxel.zVoxels)};
    high = {std::max(high.xVoxels, voxel.xVoxels), std::max(high.yVoxels, voxel.yVoxels), std::max(high.zVoxels, voxel.zVoxels)};
  }
  return {(low.xVoxels + high.xVoxels) / 2, low.yVoxels, (low.zVoxels + high.zVoxels) / 2};
}

} // namespace

std::string Overlays::WillFallText(const Protocol::StateUpdate& _state, const Protocol::CollapseView& _collapse)
{
  const std::uint32_t seconds = (_collapse.warningTicksLeft + TickClock::TICKS_PER_SECOND - 1) / TickClock::TICKS_PER_SECOND;
  return std::format(WILL_FALL_ALERT, NameOf(_state, _collapse), seconds);
}

std::string Overlays::HasFallenText(const Protocol::StateUpdate& _state, const Protocol::CollapseView& _collapse)
{
  return std::format(HAS_FALLEN_ALERT, NameOf(_state, _collapse));
}

void Overlays::Update(const Session& _session, double _nowSeconds, HudAlerts& _alerts)
{
  const Protocol::StateUpdate& state = _session.State();
  for (const Protocol::CollapseView& collapse : state.collapses)
  {
    // The countdown stays until the fall replaces it; the fall's alert stays its lifetime.
    const bool warning = collapse.warningTicksLeft > 0;
    _alerts.Raise(collapse.id,
                  {.text = warning ? WillFallText(state, collapse) : HasFallenText(state, collapse),
                   .sub = "",
                   .location = PieceFoot(collapse),
                   .urgent = true,
                   .expiresSeconds = warning ? std::numeric_limits<double>::infinity() : _nowSeconds + HudAlerts::LIFETIME_SECONDS});
  }
}

std::vector<OverlayLayer> Overlays::Layers(const Session& _session, double _seconds, bool _integrityView) const
{
  std::vector<OverlayLayer> layers;
  if (!_integrityView)
  {
    return layers;
  }
  // The piece pulses with a period of PIECE_PULSE_SECONDS, from its least alpha up and back.
  const double phase = std::fmod(_seconds, PIECE_PULSE_SECONDS) / PIECE_PULSE_SECONDS;
  const auto pulse = static_cast<float>(0.5 - (0.5 * std::cos(2.0 * std::numbers::pi * phase)));
  OverlayLayer pieces{.alpha = PIECE_MIN_ALPHA + ((PIECE_MAX_ALPHA - PIECE_MIN_ALPHA) * pulse), .boxes = {}};
  OverlayLayer strips{.alpha = STRIP_ALPHA, .boxes = {}};
  for (const Protocol::CollapseView& collapse : _session.State().collapses)
  {
    if (collapse.warningTicksLeft == 0)
    {
      continue;
    }
    for (const Protocol::VoxelPosition& voxel : collapse.piece)
    {
      pieces.boxes.push_back({.cellVoxels = voxel, .paletteIndex = OVERLAY_BAD_INDEX});
    }
    // A strip's cell is the one above its ground, where units are hit: its box is the ground's voxel under it.
    for (const Protocol::VoxelPosition& cell : collapse.strip)
    {
      strips.boxes.push_back({.cellVoxels = {cell.xVoxels, cell.yVoxels - 1, cell.zVoxels}, .paletteIndex = OVERLAY_BAD_INDEX});
    }
  }
  layers.push_back(std::move(strips));
  layers.push_back(std::move(pieces));
  return layers;
}

} // namespace Client
