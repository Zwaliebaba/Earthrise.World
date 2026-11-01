#include "pch.h"

#include "Overlays.h"

#include "Look.h"
#include "Strings.h"
#include "TextLayout.h"
#include "TickClock.h"
#include "UiScale.h"

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

// The integrity label's padding and the gap between its lines, in logical pixels.
constexpr float LABEL_PADDING_LOGICAL_PIXELS = 6.0F;
constexpr float LABEL_LINE_GAP_LOGICAL_PIXELS = 4.0F;
/// An opening lower than this does not let foot through: the reason the paper test gives, not the verdict, which is the
/// resolver's routeNow.
constexpr std::uint8_t FOOT_CLEARANCE_VOXELS = 2;

ScreenQuad Solid(std::int32_t _x, std::int32_t _y, std::int32_t _width, std::int32_t _height, std::uint32_t _srgb, float _alpha)
{
  return {.xPixels = static_cast<float>(_x),
          .yPixels = static_cast<float>(_y),
          .widthPixels = static_cast<float>(_width),
          .heightPixels = static_cast<float>(_height),
          .u0 = 0.0F,
          .v0 = 0.0F,
          .u1 = 0.0F,
          .v1 = 0.0F,
          .color = LinearColor(_srgb, _alpha),
          .textured = false};
}

/// A row's text laid out at _x: its key in dim and its value in its colour, or all of it in its colour when it is a
/// key alone.
void LayRow(const IntegrityRow& _row, std::int32_t _x, std::int32_t _y, std::uint32_t _scale, std::vector<ScreenQuad>& _quads)
{
  const std::string_view text(_row.text);
  const std::size_t space = text.find(' ');
  if (space == std::string_view::npos)
  {
    TextLayout::Lay(text, _x, _y, _scale, LinearColor(_row.srgb), _quads);
    return;
  }
  // The value starts where it would in the whole string: after the key's and the space's advances.
  const std::string_view key = text.substr(0, space + 1);
  TextLayout::Lay(key, _x, _y, _scale, LinearColor(DIM_SRGB), _quads);
  TextLayout::Lay(text.substr(space + 1), _x + static_cast<std::int32_t>(TextLayout::Advance(key, _scale)), _y, _scale,
                  LinearColor(_row.srgb), _quads);
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

std::vector<IntegrityRow> Overlays::IntegrityRows(const Protocol::IntegrityReading& _reading)
{
  const auto meters = [](std::uint8_t _voxels) { return static_cast<std::uint32_t>(_voxels); };
  std::vector<IntegrityRow> rows;
  if (_reading.holeWidthVoxels == 0)
  {
    rows.push_back({.text = std::string(HOLE_NONE), .srgb = TEXT_SRGB});
  }
  else if (_reading.holeFallWidthVoxels == 0)
  {
    rows.push_back({.text = std::format(HOLE_HOLDS, meters(_reading.holeWidthVoxels)), .srgb = TEXT_SRGB});
  }
  else
  {
    rows.push_back(
      {.text = std::format(HOLE_FALLS_AT, meters(_reading.holeWidthVoxels), meters(_reading.holeFallWidthVoxels)), .srgb = TEXT_SRGB});
  }
  if (!_reading.hasOpening)
  {
    rows.push_back({.text = std::string(OPENING_NONE), .srgb = TEXT_SRGB});
  }
  else
  {
    const std::string_view floorName = _reading.openingFloor == Protocol::OpeningFloor::Stone    ? FLOOR_STONE
                                       : _reading.openingFloor == Protocol::OpeningFloor::Rubble ? FLOOR_RUBBLE
                                                                                                 : FLOOR_GROUND;
    const std::string floor =
      _reading.openingFloorVoxels == 0 ? std::string(floorName) : std::format(FLOOR_UP, floorName, meters(_reading.openingFloorVoxels));
    const std::string_view verdict = _reading.routeNow                                        ? ROUTE_NOW
                                     : _reading.openingFloor == Protocol::OpeningFloor::Stone ? NO_FOOTING
                                     : _reading.openingClearVoxels < FOOT_CLEARANCE_VOXELS    ? TOO_LOW
                                                                                              : NO_ROUTE;
    rows.push_back({.text = std::format(OPENING, floor, meters(_reading.openingClearVoxels), verdict), .srgb = TEXT_SRGB});
  }
  if (_reading.gapRunVoxels == 0)
  {
    rows.push_back({.text = std::string(GAP_NONE), .srgb = TEXT_SRGB});
  }
  else if (_reading.gapFallRunVoxels == 0)
  {
    rows.push_back({.text = std::format(GAP_HOLDS, meters(_reading.gapRunVoxels)), .srgb = TEXT_SRGB});
  }
  else
  {
    rows.push_back(
      {.text = std::format(GAP_FALLS_AT, meters(_reading.gapRunVoxels), meters(_reading.gapFallRunVoxels)), .srgb = TEXT_SRGB});
  }
  const std::string_view ground = _reading.ground == Protocol::GroundKind::Earth  ? GROUND_EARTH
                                  : _reading.ground == Protocol::GroundKind::Rock ? GROUND_ROCK
                                                                                  : GROUND_BEDROCK;
  rows.push_back(_reading.canBeUndermined ? IntegrityRow{.text = std::format(GROUND_UNDERMINED, ground), .srgb = BAD_SRGB}
                                          : IntegrityRow{.text = std::format(GROUND_SAFE, ground), .srgb = GOOD_SRGB});
  if (_reading.falling)
  {
    rows.push_back({.text = std::string(FALLS), .srgb = BAD_SRGB});
  }
  return rows;
}

void Overlays::Ask(Session& _session, std::span<const Protocol::ChunkSnapshot> _snapshots, bool _integrityView)
{
  if (!_integrityView)
  {
    m_integrityAsked = false;
    return;
  }
  const Protocol::StateUpdate& state = _session.State();
  const bool turnedOn = !m_integrityAsked;
  m_integrityAsked = true;

  // The structures a collapse started, fell or ended for since the last frame.
  std::map<Protocol::EntityId, std::pair<Protocol::EntityId, bool>> phases;
  for (const Protocol::CollapseView& collapse : state.collapses)
  {
    phases.emplace(collapse.id, std::pair{collapse.structure, collapse.warningTicksLeft > 0});
  }
  std::set<Protocol::EntityId> touched;
  for (const auto& [id, phase] : phases)
  {
    const auto before = m_collapsePhases.find(id);
    if (before == m_collapsePhases.end() || before->second != phase)
    {
      touched.insert(phase.first);
    }
  }
  for (const auto& [id, phase] : m_collapsePhases)
  {
    if (!phases.contains(id))
    {
      touched.insert(phase.first);
    }
  }
  m_collapsePhases = std::move(phases);

  for (const Protocol::StructureView& structure : state.structures)
  {
    // A chunk under or beside it: its box grown by a voxel, as the ground under it is part of its reading.
    const auto overlaps = [&structure](const Protocol::ChunkSnapshot& _snapshot)
    {
      const std::array<std::int32_t, 3> chunk = {_snapshot.coordinate.xChunks, _snapshot.coordinate.yChunks, _snapshot.coordinate.zChunks};
      const std::array<std::int32_t, 3> low = {structure.originVoxels.xVoxels, structure.originVoxels.yVoxels,
                                               structure.originVoxels.zVoxels};
      const std::array<std::int32_t, 3> size = {structure.extentVoxels.xVoxels, structure.extentVoxels.yVoxels,
                                                structure.extentVoxels.zVoxels};
      for (std::size_t axis = 0; axis < chunk.size(); ++axis)
      {
        const std::int32_t chunkLow = chunk.at(axis) * Protocol::CHUNK_EDGE_VOXELS;
        if (low.at(axis) - 1 >= chunkLow + Protocol::CHUNK_EDGE_VOXELS || low.at(axis) + size.at(axis) + 1 <= chunkLow)
        {
          return false;
        }
      }
      return true;
    };
    if (turnedOn || !m_asked.contains(structure.id) || touched.contains(structure.id) || std::ranges::any_of(_snapshots, overlaps))
    {
      _session.Ask(Protocol::SectionIntegrity{.structure = structure.id});
      m_asked.insert(structure.id);
    }
  }
}

std::vector<PlacedLabel> Overlays::LayLabels(const Session& _session, const DirectX::XMFLOAT4X4& _viewProjection,
                                             std::uint32_t _widthPixels, std::uint32_t _heightPixels, std::int32_t _topPixels,
                                             float _uiScale, bool _integrityView, std::vector<ScreenQuad>& _quads) const
{
  std::vector<PlacedLabel> placed;
  if (!_integrityView || _widthPixels == 0 || _heightPixels == 0)
  {
    return placed;
  }
  const auto width = static_cast<std::int32_t>(_widthPixels);
  const auto height = static_cast<std::int32_t>(_heightPixels);
  const DirectX::XMMATRIX viewProjection = DirectX::XMLoadFloat4x4(&_viewProjection);

  // Each section with a reading whose anchor, a meter above the middle of its top, is on screen, nearest first.
  struct Anchor
  {
    const Protocol::StructureView* structure;
    const Protocol::IntegrityReading* reading;
    std::int32_t xPixels;
    std::int32_t yPixels;
    float depth; // reversed Z: nearer is larger
  };
  std::vector<Anchor> anchors;
  for (const Protocol::StructureView& structure : _session.State().structures)
  {
    const Protocol::IntegrityReading* reading = _session.Integrity(structure.id);
    if (reading == nullptr)
    {
      continue;
    }
    const DirectX::XMVECTOR top = DirectX::XMVectorSet(
      static_cast<float>(structure.originVoxels.xVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.xVoxels)),
      static_cast<float>(structure.originVoxels.yVoxels + structure.extentVoxels.yVoxels + 1),
      static_cast<float>(structure.originVoxels.zVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.zVoxels)), 1.0F);
    DirectX::XMFLOAT4 clip{};
    DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(top, viewProjection));
    if (clip.w <= 0.0F)
    {
      continue;
    }
    const float x = (clip.x / clip.w + 1.0F) * 0.5F * static_cast<float>(width);
    const float y = (1.0F - clip.y / clip.w) * 0.5F * static_cast<float>(height);
    if (x < 0.0F || x >= static_cast<float>(width) || y < 0.0F || y >= static_cast<float>(height))
    {
      continue;
    }
    anchors.push_back({.structure = &structure,
                       .reading = reading,
                       .xPixels = static_cast<std::int32_t>(std::floor(x)),
                       .yPixels = static_cast<std::int32_t>(std::floor(y)),
                       .depth = clip.z / clip.w});
  }
  std::ranges::stable_sort(anchors, [](const Anchor& _left, const Anchor& _right) { return _left.depth > _right.depth; });

  const std::uint32_t nameScale = TextLayout::PixelsPerFontPixel(TextSize::Name, _uiScale);
  const std::uint32_t labelScale = TextLayout::PixelsPerFontPixel(TextSize::Label, _uiScale);
  const std::int32_t padding = ScaledPixels(LABEL_PADDING_LOGICAL_PIXELS, _uiScale);
  const std::int32_t lineGap = ScaledPixels(LABEL_LINE_GAP_LOGICAL_PIXELS, _uiScale);
  const auto nameCell = static_cast<std::int32_t>(TextLayout::CellPixels(nameScale));
  const auto labelCell = static_cast<std::int32_t>(TextLayout::CellPixels(labelScale));
  const auto overlapsPlaced = [&placed](std::int32_t _x, std::int32_t _y, std::int32_t _w, std::int32_t _h)
  {
    return std::ranges::any_of(placed,
                               [&](const PlacedLabel& _other)
                               {
                                 return _x < _other.xPixels + _other.widthPixels && _other.xPixels < _x + _w &&
                                        _y < _other.yPixels + _other.heightPixels && _other.yPixels < _y + _h;
                               });
  };
  for (const Anchor& anchor : anchors)
  {
    const std::string name = anchor.structure->name.empty() ? anchor.structure->templateId : anchor.structure->name;
    const std::vector<IntegrityRow> rows = IntegrityRows(*anchor.reading);
    const auto nameWidth = static_cast<std::int32_t>(TextLayout::Measure(name, nameScale));
    std::int32_t rowsWidth = 0;
    for (const IntegrityRow& row : rows)
    {
      rowsWidth = std::max(rowsWidth, static_cast<std::int32_t>(TextLayout::Measure(row.text, labelScale)));
    }
    // Its block sits on the anchor, centred, kept inside the screen below the top bar.
    const auto place = [&](bool _nameOnly)
    {
      const std::int32_t blockWidth = (2 * padding) + std::max(nameWidth, _nameOnly ? 0 : rowsWidth);
      const std::int32_t blockHeight =
        (2 * padding) + nameCell + (_nameOnly ? 0 : static_cast<std::int32_t>(rows.size()) * (lineGap + labelCell));
      const std::int32_t x = std::clamp(anchor.xPixels - (blockWidth / 2), 0, std::max(0, width - blockWidth));
      const std::int32_t y = std::clamp(anchor.yPixels - blockHeight, std::min(_topPixels, std::max(0, height - blockHeight)),
                                        std::max(0, height - blockHeight));
      return PlacedLabel{.structure = anchor.structure->id,
                         .xPixels = x,
                         .yPixels = y,
                         .widthPixels = blockWidth,
                         .heightPixels = blockHeight,
                         .nameOnly = _nameOnly};
    };
    PlacedLabel label = place(false);
    if (overlapsPlaced(label.xPixels, label.yPixels, label.widthPixels, label.heightPixels))
    {
      label = place(true);
      if (overlapsPlaced(label.xPixels, label.yPixels, label.widthPixels, label.heightPixels))
      {
        continue;
      }
    }
    _quads.push_back(Solid(label.xPixels, label.yPixels, label.widthPixels, label.heightPixels, PANEL_SRGB, PANEL_ALPHA));
    TextLayout::Lay(name, label.xPixels + padding, label.yPixels + padding, nameScale, LinearColor(TEXT_SRGB), _quads);
    if (!label.nameOnly)
    {
      std::int32_t y = label.yPixels + padding + nameCell;
      for (const IntegrityRow& row : rows)
      {
        y += lineGap;
        LayRow(row, label.xPixels + padding, y, labelScale, _quads);
        y += labelCell;
      }
    }
    placed.push_back(label);
  }
  return placed;
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
  // Each section's weak point, its box both corners included (ADR-021, decision 4).
  OverlayLayer weakPoints{.alpha = WEAK_POINT_ALPHA, .boxes = {}};
  for (const Protocol::StructureView& structure : _session.State().structures)
  {
    const Protocol::IntegrityReading* reading = _session.Integrity(structure.id);
    if (reading == nullptr)
    {
      continue;
    }
    for (std::int32_t y = reading->weakMinVoxels.yVoxels; y <= reading->weakMaxVoxels.yVoxels; ++y)
    {
      for (std::int32_t z = reading->weakMinVoxels.zVoxels; z <= reading->weakMaxVoxels.zVoxels; ++z)
      {
        for (std::int32_t x = reading->weakMinVoxels.xVoxels; x <= reading->weakMaxVoxels.xVoxels; ++x)
        {
          weakPoints.boxes.push_back({.cellVoxels = {x, y, z}, .paletteIndex = OVERLAY_ACCENT_INDEX});
        }
      }
    }
  }
  layers.push_back(std::move(weakPoints));
  layers.push_back(std::move(strips));
  layers.push_back(std::move(pieces));
  return layers;
}

} // namespace Client
