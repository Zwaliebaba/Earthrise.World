#include "pch.h"

#include "Overlays.h"

#include "Look.h"
#include "Selection.h"
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

/// An opening lower than this does not let foot through: the reason the paper test gives, not the verdict, which is the
/// resolver's routeNow.
constexpr std::uint8_t FOOT_CLEARANCE_VOXELS = 2;
// IntegrityRows' order, which the readout's bars go by.
constexpr std::size_t HOLE_ROW = 0;
constexpr std::size_t GAP_ROW = 2;

/// _width over _fallWidth, or nothing when nothing falls within the section.
std::optional<float> Fill(std::uint8_t _width, std::uint8_t _fallWidth) noexcept
{
  if (_fallWidth == 0)
  {
    return std::nullopt;
  }
  return static_cast<float>(_width) / static_cast<float>(_fallWidth);
}

/// Whether the state holds the structure _id.
bool Holds(const Protocol::StateUpdate& _state, Protocol::EntityId _id)
{
  return _id != Protocol::NO_ENTITY && std::ranges::find(_state.structures, _id, &Protocol::StructureView::id) != _state.structures.end();
}

constexpr std::int32_t TILE_CELLS = Protocol::CHUNK_EDGE_VOXELS;
/// The most tiles the passability view covers, those nearest the middle of the screen: the default framing shows 35,
/// and the farthest zoom would ask for hundreds, each a reading and a box for each of its cells.
constexpr std::size_t MAX_VISIBLE_TILES = 36;

/// Whether bit z * 32 + x of a tile's bits is set.
bool ReachBit(const std::array<std::uint8_t, Protocol::TILE_REACH_BYTES>& _bits, std::int32_t _x, std::int32_t _z)
{
  const std::int32_t cell = (_z * TILE_CELLS) + _x;
  const auto index = static_cast<std::size_t>(cell);
  return ((_bits.at(index / 8) >> (index % 8)) & 1U) != 0;
}

/// The tile a voxel's column lies in.
std::int32_t TileOf(std::int32_t _voxels) noexcept
{
  return _voxels >= 0 ? _voxels / TILE_CELLS : ((_voxels + 1) / TILE_CELLS) - 1;
}

/// The least and the greatest of _points along _axis.
std::pair<float, float> Extent(const std::array<std::pair<float, float>, 4>& _points, const std::pair<float, float>& _axis) noexcept
{
  std::pair<float, float> extent{std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()};
  for (const std::pair<float, float>& point : _points)
  {
    const float along = (point.first * _axis.first) + (point.second * _axis.second);
    extent = {std::min(extent.first, along), std::max(extent.second, along)};
  }
  return extent;
}

/// Whether the convex quad _quad, in x and z, overlaps the rectangle from _low to _high: no axis separates them, of the
/// two the rectangle's sides lie along and the four the quad's edges are normal to.
bool QuadOverlaps(const std::array<std::pair<float, float>, 4>& _quad, std::pair<float, float> _low, std::pair<float, float> _high)
{
  const std::array<std::pair<float, float>, 4> box = {{_low, {_high.first, _low.second}, _high, {_low.first, _high.second}}};
  std::array<std::pair<float, float>, 6> axes = {{{1.0F, 0.0F}, {0.0F, 1.0F}}};
  for (std::size_t corner = 0; corner < _quad.size(); ++corner)
  {
    const std::pair<float, float>& from = _quad.at(corner);
    const std::pair<float, float>& to = _quad.at((corner + 1) % _quad.size());
    axes.at(2 + corner) = {from.second - to.second, to.first - from.first};
  }
  return std::ranges::all_of(axes,
                             [&_quad, &box](const std::pair<float, float>& _axis)
                             {
                               const std::pair<float, float> quad = Extent(_quad, _axis);
                               const std::pair<float, float> rectangle = Extent(box, _axis);
                               return quad.second > rectangle.first && rectangle.second > quad.first;
                             });
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
  // The breach, once: the first reading that has the goal's cell reached.
  if (m_breachGoal.has_value() && !m_breachAnnounced)
  {
    const Protocol::VoxelPosition goal = *m_breachGoal;
    const Protocol::TileReachReading* reach = _session.Reach(TileOf(goal.xVoxels), TileOf(goal.zVoxels));
    if (reach != nullptr &&
        ReachBit(reach->reached, goal.xVoxels - (TileOf(goal.xVoxels) * TILE_CELLS), goal.zVoxels - (TileOf(goal.zVoxels) * TILE_CELLS)))
    {
      _alerts.Push({.text = std::string(BREACH_OPEN_ALERT),
                    .sub = "",
                    .location = goal,
                    .urgent = false,
                    .expiresSeconds = _nowSeconds + HudAlerts::LIFETIME_SECONDS});
      m_breachAnnounced = true;
    }
  }
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
  // The rows that changed, for the readout's flash.
  for (const Protocol::StructureView& structure : state.structures)
  {
    const Protocol::IntegrityReading* reading = _session.Integrity(structure.id);
    if (reading == nullptr)
    {
      continue;
    }
    std::vector<std::string> texts;
    for (IntegrityRow& row : IntegrityRows(*reading))
    {
      texts.push_back(std::move(row.text));
    }
    const auto seen = m_seenRows.find(structure.id);
    if (seen == m_seenRows.end())
    {
      const std::size_t count = texts.size();
      m_seenRows.emplace(structure.id, SeenRows{.texts = std::move(texts),
                                                .changedSeconds = std::vector<double>(count, -std::numeric_limits<double>::infinity())});
      continue;
    }
    SeenRows& rows = seen->second;
    rows.changedSeconds.resize(texts.size(), -std::numeric_limits<double>::infinity());
    for (std::size_t index = 0; index < texts.size(); ++index)
    {
      if (index >= rows.texts.size() || rows.texts.at(index) != texts.at(index))
      {
        rows.changedSeconds.at(index) = _nowSeconds;
      }
    }
    rows.texts = std::move(texts);
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
  // GROUND matters only where a gap lies under a section that can be undermined; elsewhere it is dim (M3-H1).
  const std::uint32_t groundSrgb = _reading.canBeUndermined && _reading.gapRunVoxels != 0 ? BAD_SRGB : DIM_SRGB;
  rows.push_back(_reading.canBeUndermined ? IntegrityRow{.text = std::format(GROUND_UNDERMINED, ground), .srgb = groundSrgb}
                                          : IntegrityRow{.text = std::format(GROUND_SAFE, ground), .srgb = groundSrgb});
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

void Overlays::Hover(Protocol::EntityId _section) noexcept
{
  m_hovered = _section;
}

void Overlays::Select(Protocol::EntityId _section) noexcept
{
  m_selected = _section;
}

void Overlays::SelectedFirst(bool _selectedFirst) noexcept
{
  m_selectedFirst = _selectedFirst;
}

Protocol::EntityId Overlays::Focused(const Session& _session) const
{
  const Protocol::StateUpdate& state = _session.State();
  if (m_selectedFirst)
  {
    return Holds(state, m_selected) ? m_selected : (Holds(state, m_hovered) ? m_hovered : Protocol::NO_ENTITY);
  }
  if (Holds(state, m_hovered))
  {
    return m_hovered;
  }
  if (Holds(state, m_selected))
  {
    return m_selected;
  }
  const auto target = std::ranges::find(state.structures, TARGET_NAME, &Protocol::StructureView::name);
  return target != state.structures.end() ? target->id : Protocol::NO_ENTITY;
}

std::optional<HudReadout> Overlays::Readout(const Session& _session, double _nowSeconds, bool _integrityView) const
{
  const Protocol::EntityId focused = _integrityView ? Focused(_session) : Protocol::NO_ENTITY;
  const Protocol::IntegrityReading* reading = focused != Protocol::NO_ENTITY ? _session.Integrity(focused) : nullptr;
  const auto structure = std::ranges::find(_session.State().structures, focused, &Protocol::StructureView::id);
  if (reading == nullptr || structure == _session.State().structures.end())
  {
    return std::nullopt;
  }
  HudReadout readout{.title = structure->name.empty() ? structure->templateId : structure->name, .rows = {}};
  const auto seen = m_seenRows.find(focused);
  const std::vector<IntegrityRow> rows = IntegrityRows(*reading);
  for (std::size_t index = 0; index < rows.size(); ++index)
  {
    const std::string_view text(rows.at(index).text);
    const std::size_t space = text.find(' ');
    const bool flashing = seen != m_seenRows.end() && index < seen->second.changedSeconds.size() &&
                          _nowSeconds - seen->second.changedSeconds.at(index) < FLASH_SECONDS;
    readout.rows.push_back({.key = std::string(text.substr(0, space)),
                            .value = space == std::string_view::npos ? std::string() : std::string(text.substr(space + 1)),
                            .srgb = rows.at(index).srgb,
                            .flashing = flashing,
                            .fill = index == HOLE_ROW  ? Fill(reading->holeWidthVoxels, reading->holeFallWidthVoxels)
                                    : index == GAP_ROW ? Fill(reading->gapRunVoxels, reading->gapFallRunVoxels)
                                                       : std::nullopt});
  }
  // The lock, live from the update rather than the reading, which is asked only when the voxels change. Layout A's alone:
  // nobody repairs in the lab, whose readout keeps the paper test's rows (GameDesign.md, M3's values, item 5).
  if (m_selectedFirst && structure->repairLockTicks > 0)
  {
    readout.rows.push_back({.key = std::string(REPAIR_KEY),
                            .value = RepairValue(structure->repairLockTicks),
                            .srgb = TEXT_SRGB,
                            .flashing = false,
                            .fill = std::nullopt});
  }
  return readout;
}

std::optional<std::string> Overlays::WeakValue(const Protocol::IntegrityReading& _reading)
{
  const bool hole = _reading.holeWidthVoxels > 0 && _reading.holeFallWidthVoxels > 0;
  const bool gap = _reading.gapRunVoxels > 0 && _reading.gapFallRunVoxels > 0;
  // Nearer its fall: the larger share of its fall width, compared without dividing.
  const bool holeNearer = hole && (!gap || static_cast<std::uint32_t>(_reading.holeWidthVoxels) * _reading.gapFallRunVoxels >=
                                             static_cast<std::uint32_t>(_reading.gapRunVoxels) * _reading.holeFallWidthVoxels);
  if (holeNearer)
  {
    return std::format(WEAK_HOLE, _reading.holeWidthVoxels, _reading.holeFallWidthVoxels);
  }
  if (gap)
  {
    return std::format(WEAK_GAP, _reading.gapRunVoxels, _reading.gapFallRunVoxels);
  }
  return std::nullopt;
}

std::string Overlays::RepairValue(std::uint32_t _lockTicks)
{
  return _lockTicks == 0 ? std::string(REPAIR_READY)
                         : std::format(REPAIR_LOCKED, (_lockTicks + TickClock::TICKS_PER_SECOND - 1) / TickClock::TICKS_PER_SECOND);
}

std::vector<HudHealthBar> Overlays::HealthBars(const Session& _session, const DirectX::XMFLOAT4X4& _viewProjection,
                                               std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  std::vector<HudHealthBar> bars;
  for (const Protocol::StructureView& structure : _session.State().structures)
  {
    if (structure.maxHitPoints <= 0 || structure.templateId != "T3")
    {
      continue;
    }
    const DirectX::XMFLOAT3 anchor{
      static_cast<float>(structure.originVoxels.xVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.xVoxels)),
      static_cast<float>(structure.originVoxels.yVoxels + structure.extentVoxels.yVoxels) + 1.0F,
      static_cast<float>(structure.originVoxels.zVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.zVoxels))};
    if (const std::optional<DirectX::XMFLOAT2> at = Selector::Project(_viewProjection, anchor, _widthPixels, _heightPixels); at.has_value())
    {
      bars.push_back(
        {.anchorPixels = *at,
         .fraction = std::clamp(static_cast<float>(structure.hitPoints) / static_cast<float>(structure.maxHitPoints), 0.0F, 1.0F)});
    }
  }
  return bars;
}

std::vector<HudWorldLabel> Overlays::WorldLabels(const Session& _session, const DirectX::XMFLOAT4X4& _viewProjection,
                                                 std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _viewHeightMeters,
                                                 Protocol::EntityId _selected, bool _integrityView)
{
  std::vector<HudWorldLabel> labels;
  if (!_integrityView)
  {
    return labels;
  }
  const Protocol::StateUpdate& state = _session.State();
  const bool namesAlone = _viewHeightMeters > NAMES_ALONE_METERS;
  for (const Protocol::StructureView& structure : state.structures)
  {
    const DirectX::XMFLOAT3 anchor{
      static_cast<float>(structure.originVoxels.xVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.xVoxels)),
      static_cast<float>(structure.originVoxels.yVoxels + structure.extentVoxels.yVoxels) + 1.0F,
      static_cast<float>(structure.originVoxels.zVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.zVoxels))};
    const std::optional<DirectX::XMFLOAT2> at = Selector::Project(_viewProjection, anchor, _widthPixels, _heightPixels);
    if (!at.has_value())
    {
      continue;
    }
    const std::string name = structure.name.empty() ? structure.templateId : structure.name;
    const bool falls = std::ranges::any_of(state.collapses, [&structure](const Protocol::CollapseView& _collapse)
                                           { return _collapse.structure == structure.id; });
    const bool selected = structure.id == _selected;
    std::string line = name;
    if (!namesAlone || selected)
    {
      const auto add = [&line](std::string_view _part)
      {
        line += LABEL_SEPARATOR;
        line += _part;
      };
      const Protocol::IntegrityReading* reading = _session.Integrity(structure.id);
      if (falls)
      {
        add(FALLS);
      }
      else if (structure.opened)
      {
        add(LABEL_OPEN);
      }
      else if (const std::optional<std::string> weak = reading != nullptr ? WeakValue(*reading) : std::nullopt; weak.has_value())
      {
        add(std::format("{} {}", WEAK_KEY, *weak));
      }
      if (structure.repairLockTicks > 0)
      {
        add(RepairValue(structure.repairLockTicks));
      }
    }
    labels.push_back(
      {.line = std::move(line), .name = name, .anchorPixels = *at, .srgb = falls ? BAD_SRGB : TEXT_SRGB, .selected = selected});
  }
  // The selected first, then nearest the screen's middle, then in id order, which the update's order already is.
  const DirectX::XMFLOAT2 middle{0.5F * static_cast<float>(_widthPixels), 0.5F * static_cast<float>(_heightPixels)};
  const auto away = [&middle](const HudWorldLabel& _label)
  {
    const float x = _label.anchorPixels.x - middle.x;
    const float y = _label.anchorPixels.y - middle.y;
    return (x * x) + (y * y);
  };
  std::ranges::stable_sort(labels, [&away](const HudWorldLabel& _a, const HudWorldLabel& _b)
                           { return _a.selected != _b.selected ? _a.selected : away(_a) < away(_b); });
  return labels;
}

Protocol::EntityId Overlays::SectionAt(const Protocol::StateUpdate& _state, const Protocol::VoxelPosition& _voxels)
{
  const auto inside = [](std::int32_t _at, std::int32_t _origin, std::int32_t _extent)
  { return _at >= _origin && _at < _origin + _extent; };
  for (const Protocol::StructureView& structure : _state.structures)
  {
    if (inside(_voxels.xVoxels, structure.originVoxels.xVoxels, structure.extentVoxels.xVoxels) &&
        inside(_voxels.yVoxels, structure.originVoxels.yVoxels, structure.extentVoxels.yVoxels) &&
        inside(_voxels.zVoxels, structure.originVoxels.zVoxels, structure.extentVoxels.zVoxels))
    {
      return structure.id;
    }
  }
  return Protocol::NO_ENTITY;
}

void Overlays::WatchBreach(const Protocol::VoxelPosition& _goalVoxels)
{
  m_breachGoal = _goalVoxels;
  m_breachAnnounced = false;
}

void Overlays::AskReach(Session& _session, std::span<const Protocol::ChunkSnapshot> _snapshots, std::span<const Tile> _visible,
                        std::uint8_t _player, bool _passabilityView)
{
  m_visible.assign(_visible.begin(), _visible.end());
  // The start: the cell the first figure of the player's squad stands in, an engine's body apart (M4-C1).
  const Protocol::StateUpdate& state = _session.State();
  std::optional<Protocol::VoxelPosition> start;
  for (const Protocol::SquadView& squad : state.squads)
  {
    const bool body = squad.kind == Protocol::UnitKind::Ram || squad.kind == Protocol::UnitKind::Trebuchet;
    if (squad.owner == _player && !squad.units.empty() && !body)
    {
      const auto unit = std::ranges::find(state.units, squad.units.front(), &Protocol::UnitView::id);
      if (unit != state.units.end())
      {
        start = unit->cellVoxels;
      }
      break;
    }
  }
  if (!start.has_value())
  {
    return;
  }
  if (!m_reachStart.has_value() || !(*m_reachStart == *start) || (_passabilityView && !m_reachAsked))
  {
    m_reachTiles.clear(); // every tile again, from the new start or as the view comes on
  }
  m_reachStart = start;
  m_reachAsked = _passabilityView;

  std::set<Tile> wanted;
  if (_passabilityView)
  {
    wanted.insert(_visible.begin(), _visible.end());
  }
  if (m_breachGoal.has_value())
  {
    wanted.emplace(TileOf(m_breachGoal->xVoxels), TileOf(m_breachGoal->zVoxels));
  }
  // A tile that left the screen is asked again when it comes back.
  std::erase_if(m_reachTiles, [&wanted](const Tile& _tile) { return !wanted.contains(_tile); });
  for (const Tile& tile : wanted)
  {
    const bool changed = std::ranges::any_of(
      _snapshots, [&tile](const Protocol::ChunkSnapshot& _snapshot)
      { return std::abs(_snapshot.coordinate.xChunks - tile.first) <= 1 && std::abs(_snapshot.coordinate.zChunks - tile.second) <= 1; });
    if (!m_reachTiles.contains(tile) || changed)
    {
      _session.Ask(Protocol::TileReach{
        .navigationClass = Protocol::NavigationClass::Foot, .startCellVoxels = *start, .tileX = tile.first, .tileZ = tile.second});
      m_reachTiles.insert(tile);
    }
  }
}

OverlayLayer Overlays::PassabilityLayer(const Session& _session, bool _passabilityView) const
{
  OverlayLayer layer{.alpha = PASSABILITY_ALPHA, .boxes = {}};
  if (!_passabilityView)
  {
    return layer;
  }
  for (const auto& [tileX, tileZ] : m_visible)
  {
    const Protocol::TileReachReading* reach = _session.Reach(tileX, tileZ);
    if (reach == nullptr)
    {
      continue;
    }
    for (std::int32_t z = 0; z < TILE_CELLS; ++z)
    {
      for (std::int32_t x = 0; x < TILE_CELLS; ++x)
      {
        const std::int32_t cell = (z * TILE_CELLS) + x;
        const std::int16_t surface = reach->surfaceVoxels.at(static_cast<std::size_t>(cell));
        if (!ReachBit(reach->usable, x, z) || surface == Protocol::NO_SURFACE_VOXELS)
        {
          continue;
        }
        layer.boxes.push_back({.cellVoxels = {(tileX * TILE_CELLS) + x, surface, (tileZ * TILE_CELLS) + z},
                               .paletteIndex = ReachBit(reach->reached, x, z) ? OVERLAY_GOOD_INDEX : OVERLAY_DIM_INDEX});
      }
    }
  }
  return layer;
}

std::vector<Tile> Overlays::VisibleTiles(const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels,
                                         std::uint32_t _heightPixels, float _groundMeters)
{
  std::vector<Tile> tiles;
  if (_widthPixels == 0 || _heightPixels == 0)
  {
    return tiles;
  }
  // The screen's corners, brought down along the view to the ground: the view's footprint, a convex quad.
  const DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&_viewProjection));
  constexpr std::array<std::pair<float, float>, 4> CORNERS = {{{-1.0F, 1.0F}, {1.0F, 1.0F}, {1.0F, -1.0F}, {-1.0F, -1.0F}}};
  std::array<std::pair<float, float>, 4> quad{};
  for (std::size_t corner = 0; corner < CORNERS.size(); ++corner)
  {
    const auto [x, y] = CORNERS.at(corner);
    DirectX::XMFLOAT3 nearPoint{};
    DirectX::XMFLOAT3 farPoint{};
    DirectX::XMStoreFloat3(&nearPoint, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(x, y, 1.0F, 1.0F), inverse));
    DirectX::XMStoreFloat3(&farPoint, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(x, y, 0.0F, 1.0F), inverse));
    const float down = farPoint.y - nearPoint.y;
    if (!(down < 0.0F))
    {
      return tiles; // the view does not come down to the ground
    }
    const float along = (_groundMeters - nearPoint.y) / down;
    quad.at(corner) = {nearPoint.x + (along * (farPoint.x - nearPoint.x)), nearPoint.z + (along * (farPoint.z - nearPoint.z))};
  }
  float lowX = std::numeric_limits<float>::max();
  float lowZ = std::numeric_limits<float>::max();
  float highX = std::numeric_limits<float>::lowest();
  float highZ = std::numeric_limits<float>::lowest();
  for (const auto& [x, z] : quad)
  {
    lowX = std::min(lowX, x);
    lowZ = std::min(lowZ, z);
    highX = std::max(highX, x);
    highZ = std::max(highZ, z);
  }
  const auto tileOf = [](float _meters) { return static_cast<std::int32_t>(std::floor(_meters / static_cast<float>(TILE_CELLS))); };
  const std::int32_t firstX = std::max(0, tileOf(lowX));
  const std::int32_t firstZ = std::max(0, tileOf(lowZ));
  const std::int32_t lastX = tileOf(highX);
  const std::int32_t lastZ = tileOf(highZ);
  for (std::int32_t tileZ = firstZ; tileZ <= lastZ; ++tileZ)
  {
    for (std::int32_t tileX = firstX; tileX <= lastX; ++tileX)
    {
      const std::pair low{static_cast<float>(tileX * TILE_CELLS), static_cast<float>(tileZ * TILE_CELLS)};
      const std::pair high{static_cast<float>((tileX + 1) * TILE_CELLS), static_cast<float>((tileZ + 1) * TILE_CELLS)};
      if (QuadOverlaps(quad, low, high))
      {
        tiles.emplace_back(tileX, tileZ);
      }
    }
  }
  // The nearest the middle of the footprint, at most MAX_VISIBLE_TILES, in z-then-x order.
  if (tiles.size() > MAX_VISIBLE_TILES)
  {
    float middleX = 0.0F;
    float middleZ = 0.0F;
    for (const auto& [x, z] : quad)
    {
      middleX += x / static_cast<float>(quad.size());
      middleZ += z / static_cast<float>(quad.size());
    }
    const auto distance = [middleX, middleZ](const Tile& _tile)
    {
      const float x = (static_cast<float>(_tile.first) + 0.5F) * static_cast<float>(TILE_CELLS) - middleX;
      const float z = (static_cast<float>(_tile.second) + 0.5F) * static_cast<float>(TILE_CELLS) - middleZ;
      return (x * x) + (z * z);
    };
    std::ranges::stable_sort(tiles, [&distance](const Tile& _left, const Tile& _right) { return distance(_left) < distance(_right); });
    tiles.resize(MAX_VISIBLE_TILES);
    std::ranges::sort(tiles, [](const Tile& _left, const Tile& _right)
                      { return std::pair{_left.second, _left.first} < std::pair{_right.second, _right.first}; });
  }
  return tiles;
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
