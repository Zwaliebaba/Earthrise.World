#include "pch.h"

#include "Selection.h"

#include "Interpolation.h"

#include <set>
#include <tuple>

namespace Client
{

namespace
{

/// A squad's footprint reaches one cell out from each figure's cell (Hud.md §5).
constexpr std::int32_t FOOTPRINT_REACH_CELLS = 1;

const Protocol::SquadView* FindSquad(const Protocol::StateUpdate& _state, Protocol::EntityId _squad)
{
  const auto found = std::ranges::find(_state.squads, _squad, &Protocol::SquadView::id);
  return found == _state.squads.end() ? nullptr : &*found;
}

/// Whether the update holds _squad with a figure in a cell: alive, seen and not garrisoned.
bool Selectable(const Protocol::StateUpdate& _state, Protocol::EntityId _squad)
{
  const Protocol::SquadView* squad = FindSquad(_state, _squad);
  return squad != nullptr && squad->garrisonedIn == Protocol::NO_ENTITY &&
         std::ranges::any_of(_state.units, [_squad](const Protocol::UnitView& _unit) { return _unit.squad == _squad; });
}

/// Where each figure of each squad stands, its feet, by squad.
std::vector<std::pair<Protocol::EntityId, DirectX::XMFLOAT3>> Feet(const Protocol::StateUpdate& _state)
{
  std::vector<std::pair<Protocol::EntityId, DirectX::XMFLOAT3>> feet;
  feet.reserve(_state.units.size());
  for (const Protocol::UnitView& unit : _state.units)
  {
    feet.emplace_back(unit.squad, Meters(unit.position));
  }
  return feet;
}

} // namespace

bool Selection::Empty() const noexcept
{
  return squads.empty() && structure == Protocol::NO_ENTITY;
}

std::optional<Protocol::EntityId> Selector::SquadAt(const Engine::PickRay& _ray, const Protocol::StateUpdate& _state)
{
  std::optional<Protocol::EntityId> nearest;
  float nearestMeters = std::numeric_limits<float>::infinity();
  if (_ray.direction.y == 0.0F)
  {
    return nearest; // a level ray meets no plane of feet
  }
  for (const Protocol::UnitView& unit : _state.units)
  {
    if (unit.kind == Protocol::UnitKind::Ram || unit.kind == Protocol::UnitKind::Trebuchet || !Selectable(_state, unit.squad))
    {
      continue;
    }
    const DirectX::XMFLOAT3 feet = Meters(unit.position);
    const float along = (feet.y - _ray.originMeters.y) / _ray.direction.y;
    if (along <= 0.0F || along >= nearestMeters)
    {
      continue;
    }
    const auto x = static_cast<std::int32_t>(std::floor(_ray.originMeters.x + (along * _ray.direction.x)));
    const auto z = static_cast<std::int32_t>(std::floor(_ray.originMeters.z + (along * _ray.direction.z)));
    if (std::abs(x - unit.cellVoxels.xVoxels) <= FOOTPRINT_REACH_CELLS && std::abs(z - unit.cellVoxels.zVoxels) <= FOOTPRINT_REACH_CELLS)
    {
      nearest = unit.squad;
      nearestMeters = along;
    }
  }
  return nearest;
}

std::optional<DirectX::XMFLOAT2> Selector::Project(const DirectX::XMFLOAT4X4& _viewProjection, const DirectX::XMFLOAT3& _point,
                                                   std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  const auto& m = _viewProjection.m;
  const float x = (_point.x * m[0][0]) + (_point.y * m[1][0]) + (_point.z * m[2][0]) + m[3][0];
  const float y = (_point.x * m[0][1]) + (_point.y * m[1][1]) + (_point.z * m[2][1]) + m[3][1];
  const float w = (_point.x * m[0][3]) + (_point.y * m[1][3]) + (_point.z * m[2][3]) + m[3][3];
  if (w <= 0.0F)
  {
    return std::nullopt;
  }
  return DirectX::XMFLOAT2{(((x / w) + 1.0F) * 0.5F) * static_cast<float>(_widthPixels),
                           ((1.0F - (y / w)) * 0.5F) * static_cast<float>(_heightPixels)};
}

std::vector<Protocol::EntityId> Selector::SquadsInBox(const Protocol::StateUpdate& _state, std::uint8_t _owner,
                                                      const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels,
                                                      std::uint32_t _heightPixels, std::int32_t _x0, std::int32_t _y0, std::int32_t _x1,
                                                      std::int32_t _y1)
{
  const auto left = static_cast<float>(std::min(_x0, _x1));
  const auto right = static_cast<float>(std::max(_x0, _x1));
  const auto top = static_cast<float>(std::min(_y0, _y1));
  const auto bottom = static_cast<float>(std::max(_y0, _y1));
  std::vector<Protocol::EntityId> squads;
  for (const auto& [squad, feet] : Feet(_state))
  {
    const Protocol::SquadView* view = FindSquad(_state, squad);
    if (view == nullptr || view->owner != _owner || std::ranges::find(squads, squad) != squads.end() || !Selectable(_state, squad))
    {
      continue;
    }
    const std::optional<DirectX::XMFLOAT2> at = Project(_viewProjection, feet, _widthPixels, _heightPixels);
    if (at.has_value() && at->x >= left && at->x <= right && at->y >= top && at->y <= bottom)
    {
      squads.push_back(squad);
    }
  }
  return squads;
}

void Selector::Click(Selection& _selection, const Protocol::StateUpdate& _state, std::optional<Protocol::EntityId> _squad,
                     Protocol::EntityId _structure, bool _toggle)
{
  if (_squad.has_value())
  {
    const std::array<Protocol::EntityId, 1> one = {*_squad};
    if (_toggle && std::ranges::find(_selection.squads, *_squad) != _selection.squads.end())
    {
      std::erase(_selection.squads, *_squad);
      Prune(_selection, _state);
      return;
    }
    Select(_selection, _state, one, _toggle);
    return;
  }
  if (_structure != Protocol::NO_ENTITY)
  {
    _selection = {.squads = {}, .structure = _structure, .activeKind = std::nullopt};
    return;
  }
  if (!_toggle)
  {
    _selection = {};
  }
}

void Selector::Select(Selection& _selection, const Protocol::StateUpdate& _state, std::span<const Protocol::EntityId> _squads, bool _toggle)
{
  if (!_toggle)
  {
    _selection = {};
  }
  _selection.structure = Protocol::NO_ENTITY; // a structure is selected alone
  for (const Protocol::EntityId squad : _squads)
  {
    if (Selectable(_state, squad) && std::ranges::find(_selection.squads, squad) == _selection.squads.end())
    {
      _selection.squads.push_back(squad);
    }
  }
  Prune(_selection, _state);
}

void Selector::SelectKind(Selection& _selection, const Protocol::StateUpdate& _state, Protocol::EntityId _squad,
                          const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const Protocol::SquadView* clicked = FindSquad(_state, _squad);
  if (clicked == nullptr)
  {
    return;
  }
  std::vector<Protocol::EntityId> squads = SquadsInBox(_state, clicked->owner, _viewProjection, _widthPixels, _heightPixels, 0, 0,
                                                       static_cast<std::int32_t>(_widthPixels), static_cast<std::int32_t>(_heightPixels));
  std::erase_if(squads,
                [&_state, clicked](Protocol::EntityId _other)
                {
                  const Protocol::SquadView* other = FindSquad(_state, _other);
                  return other == nullptr || other->kind != clicked->kind;
                });
  Select(_selection, _state, squads, false);
  _selection.activeKind = clicked->kind;
}

std::vector<Protocol::UnitKind> Selector::Kinds(const Selection& _selection, const Protocol::StateUpdate& _state)
{
  std::vector<Protocol::UnitKind> kinds;
  for (const Protocol::UnitKind kind : KIND_ORDER)
  {
    const bool held = std::ranges::any_of(_selection.squads,
                                          [&_state, kind](Protocol::EntityId _squad)
                                          {
                                            const Protocol::SquadView* squad = FindSquad(_state, _squad);
                                            return squad != nullptr && squad->kind == kind;
                                          });
    if (held)
    {
      kinds.push_back(kind);
    }
  }
  return kinds;
}

void Selector::CycleKind(Selection& _selection, const Protocol::StateUpdate& _state)
{
  const std::vector<Protocol::UnitKind> kinds = Kinds(_selection, _state);
  if (kinds.empty())
  {
    _selection.activeKind.reset();
    return;
  }
  const auto active = _selection.activeKind.has_value() ? std::ranges::find(kinds, *_selection.activeKind) : kinds.end();
  _selection.activeKind = active == kinds.end() || active + 1 == kinds.end() ? kinds.front() : *(active + 1);
}

std::vector<OverlayBox> Selector::Marks(const Selection& _selection, const Protocol::StateUpdate& _state, std::uint8_t _paletteIndex)
{
  std::vector<OverlayBox> marks;
  std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> marked;
  for (const Protocol::EntityId id : _selection.squads)
  {
    const Protocol::SquadView* squad = FindSquad(_state, id);
    if (squad == nullptr || squad->garrisonedIn != Protocol::NO_ENTITY || squad->kind == Protocol::UnitKind::Ram ||
        squad->kind == Protocol::UnitKind::Trebuchet)
    {
      continue;
    }
    for (const Protocol::UnitView& unit : _state.units)
    {
      if (unit.squad != id)
      {
        continue;
      }
      // The figure's cell is the one above its ground, as a collapse's strip's is (Overlays.cpp).
      const Protocol::VoxelPosition ground{
        .xVoxels = unit.cellVoxels.xVoxels, .yVoxels = unit.cellVoxels.yVoxels - 1, .zVoxels = unit.cellVoxels.zVoxels};
      if (marked.emplace(ground.xVoxels, ground.yVoxels, ground.zVoxels).second)
      {
        marks.push_back({.cellVoxels = ground, .paletteIndex = _paletteIndex});
      }
    }
  }
  return marks;
}

std::vector<Protocol::EntityId> Selector::Engines(const Selection& _selection, const Protocol::StateUpdate& _state)
{
  std::vector<Protocol::EntityId> engines;
  for (const Protocol::EngineView& engine : _state.engines)
  {
    if (std::ranges::find(_selection.squads, engine.squad) != _selection.squads.end())
    {
      engines.push_back(engine.id);
    }
  }
  return engines;
}

void Selector::Prune(Selection& _selection, const Protocol::StateUpdate& _state)
{
  std::erase_if(_selection.squads, [&_state](Protocol::EntityId _squad) { return !Selectable(_state, _squad); });
  if (_selection.structure != Protocol::NO_ENTITY &&
      std::ranges::find(_state.structures, _selection.structure, &Protocol::StructureView::id) == _state.structures.end())
  {
    _selection.structure = Protocol::NO_ENTITY;
  }
  const std::vector<Protocol::UnitKind> kinds = Kinds(_selection, _state);
  if (kinds.empty())
  {
    _selection.activeKind.reset();
  }
  else if (!_selection.activeKind.has_value() || std::ranges::find(kinds, *_selection.activeKind) == kinds.end())
  {
    _selection.activeKind = kinds.front();
  }
}

} // namespace Client
