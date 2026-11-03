#include "pch.h"

#include "SelectionPanel.h"

#include "Look.h"
#include "Overlays.h"
#include "Portraits.h"
#include "Strings.h"
#include "WorldModels.h"

namespace Client
{

namespace
{

constexpr std::uint32_t TICKS_PER_SECOND = 20; // ADR-013

/// A kind's name, its tile's short label and its stat rows' keys and values (Hud.md §5).
struct KindLook
{
  Protocol::UnitKind kind;
  std::string_view name;
  std::string_view label;
  std::array<std::pair<std::string_view, std::string_view>, 2> rows;
};

constexpr std::array<KindLook, 8> KIND_LOOKS = {{
  {Protocol::UnitKind::Trebuchet, TREBUCHET_NAME, TREB_LABEL, {{{RANGE_KEY, TREBUCHET_RANGE}, {SHOT_KEY, TREBUCHET_SHOT}}}},
  {Protocol::UnitKind::Ram, RAM_NAME, RAM_LABEL, {{{TARGET_KEY, RAM_TARGET}, {ARMOR_KEY, RAM_ARMOR}}}},
  {Protocol::UnitKind::ManAtArms, MEN_AT_ARMS_NAME, ARMS_LABEL, {{{STRONG_KEY, MEN_AT_ARMS_STRONG}, {ROLE_KEY, MEN_AT_ARMS_ROLE}}}},
  {Protocol::UnitKind::Spearman, SPEARMEN_NAME, SPEAR_LABEL, {{{STRONG_KEY, SPEARMEN_STRONG}, {WEAK_KEY, SPEARMEN_WEAK}}}},
  {Protocol::UnitKind::Archer, ARCHERS_NAME, ARCHER_LABEL, {{{HEIGHT_KEY, ARCHERS_HEIGHT}, {WEAK_KEY, ARCHERS_WEAK}}}},
  {Protocol::UnitKind::Outrider, OUTRIDERS_NAME, RIDER_LABEL, {{{STRONG_KEY, OUTRIDERS_STRONG}, {WEAK_KEY, OUTRIDERS_WEAK}}}},
  {Protocol::UnitKind::Sapper, SAPPERS_NAME, SAPPER_LABEL, {{{DIGS_KEY, SAPPERS_DIGS}, {ARMOR_KEY, SAPPERS_ARMOR}}}},
  {Protocol::UnitKind::Laborer, LABORER_NAME, LABOR_LABEL, {{{ROLE_KEY, LABORER_ROLE}, {ARMOR_KEY, NO_ARMOR}}}},
}};

const KindLook& LookOf(Protocol::UnitKind _kind)
{
  const auto found = std::ranges::find(KIND_LOOKS, _kind, &KindLook::kind);
  return found == KIND_LOOKS.end() ? KIND_LOOKS.at(3) : *found;
}

/// A structure's name at 30 px and its icon glyph: the templates by Hud.md's short names.
struct StructureLook
{
  std::string_view templateId;
  std::string_view name;
  char32_t icon;
};

constexpr std::array<StructureLook, 9> STRUCTURE_LOOKS = {{{"T1", "HALL", U'\uE018'},
                                                           {"T2", "KEEP", U'\uE018'},
                                                           {"T3", "WALL", U'\uE01C'},
                                                           {"T4", "TOWER", U'\uE011'},
                                                           {"T5", "GATEHOUSE", U'\uE012'},
                                                           {"T6", "PALISADE", U'\uE013'},
                                                           {"T7", "PALISADE GATE", U'\uE012'},
                                                           {"T8", "QUARRY YARD", U'\uE014'},
                                                           {"T9", "LUMBER CAMP", U'\uE015'}}};

/// A code point as UTF-8; the icons are in the private use area, three bytes each.
std::string Utf8(char32_t _codePoint)
{
  std::string text;
  if (_codePoint < 0x80)
  {
    text.push_back(static_cast<char>(_codePoint));
  }
  else if (_codePoint < 0x800)
  {
    text.push_back(static_cast<char>(0xC0 | (_codePoint >> 6)));
    text.push_back(static_cast<char>(0x80 | (_codePoint & 0x3F)));
  }
  else
  {
    text.push_back(static_cast<char>(0xE0 | (_codePoint >> 12)));
    text.push_back(static_cast<char>(0x80 | ((_codePoint >> 6) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | (_codePoint & 0x3F)));
  }
  return text;
}

std::string Garrison(const Protocol::StructureView& _structure, bool _enemy)
{
  if (_enemy)
  {
    return std::format(OUT_OF, UNKNOWN_COUNT, _structure.garrisonCapacity);
  }
  return std::format(OUT_OF, _structure.garrisonCount, _structure.garrisonCapacity);
}

HudPanel StructurePanel(const Protocol::StructureView& _structure, const Protocol::StateUpdate& _state,
                        const Protocol::IntegrityReading* _reading)
{
  HudPanel panel;
  panel.shows = HudPanel::Shows::One;
  const auto look = std::ranges::find(STRUCTURE_LOOKS, std::string_view(_structure.templateId), &StructureLook::templateId);
  panel.name = look == STRUCTURE_LOOKS.end() ? _structure.name : std::string(look->name);
  panel.portrait.icon = Utf8(look == STRUCTURE_LOOKS.end() ? U'\uE01C' : look->icon);
  panel.enemy = _structure.owner != _state.player;
  const auto row = [&panel](std::string_view _key, std::string _value)
  { panel.rows.push_back({.key = std::string(_key), .value = std::move(_value), .srgb = SUB_SRGB}); };
  // The critical part's row, or in the control HEALTH hp / max in its place (ADR-028, decision 6; M4-J3): the control's
  // structures carry hit points, and the voxel version's none.
  const auto part = [&_structure, &row](std::string_view _key)
  {
    if (_structure.maxHitPoints > 0)
    {
      row(HEALTH_KEY, std::format(OUT_OF, _structure.hitPoints, _structure.maxHitPoints));
    }
    else
    {
      row(_key, std::format(PERCENT_VALUE, _structure.criticalPercent));
    }
  };
  const std::string_view id = _structure.templateId;
  if (id == "T4")
  {
    row(GARRISON_KEY, Garrison(_structure, panel.enemy));
    part(PLATFORM_KEY);
    row(REPAIR_KEY, Overlays::RepairValue(_structure.repairLockTicks));
  }
  else if (id == "T5")
  {
    row(GARRISON_KEY, Garrison(_structure, panel.enemy));
    part(GATE_KEY);
    row(REPAIR_KEY, Overlays::RepairValue(_structure.repairLockTicks));
  }
  else if (id == "T2")
  {
    const std::uint32_t seconds = _state.trial.holdTicksNeeded / TICKS_PER_SECOND;
    part(TOWER_KEY);
    row(GARRISON_KEY, Garrison(_structure, panel.enemy));
    row(TO_WIN_KEY, std::format(TO_WIN_VALUE, seconds / 60, seconds % 60));
  }
  else
  {
    row(NAME_KEY, _structure.name);
    if (id == "T3")
    {
      const std::optional<std::string> weak = _reading != nullptr ? Overlays::WeakValue(*_reading) : std::nullopt;
      if (!_structure.opened && weak.has_value())
      {
        row(WEAK_KEY, *weak);
      }
      else
      {
        row(OPEN_KEY, std::string(_structure.opened ? OPEN_YES : OPEN_NO));
      }
    }
    row(REPAIR_KEY, Overlays::RepairValue(_structure.repairLockTicks));
  }
  return panel;
}

/// A squad's figures in the update, and their average hit points over their most.
struct Figures
{
  std::size_t count = 0;
  std::int32_t hitPoints = 0;
  std::int32_t maxHitPoints = 0;
};

Figures FiguresOf(const Protocol::StateUpdate& _state, Protocol::EntityId _squad)
{
  Figures figures;
  for (const Protocol::UnitView& unit : _state.units)
  {
    if (unit.squad == _squad)
    {
      ++figures.count;
      figures.hitPoints += unit.hitPoints;
      figures.maxHitPoints += unit.maxHitPoints;
    }
  }
  return figures;
}

std::optional<float> Fraction(const Figures& _figures)
{
  return _figures.maxHitPoints > 0
           ? std::optional<float>(static_cast<float>(_figures.hitPoints) / static_cast<float>(_figures.maxHitPoints))
           : std::nullopt;
}

HudPanel SquadPanel(const Protocol::SquadView& _squad, const Protocol::StateUpdate& _state)
{
  HudPanel panel;
  panel.shows = HudPanel::Shows::One;
  const KindLook& look = LookOf(_squad.kind);
  panel.name = std::string(look.name);
  panel.portrait.cell = PortraitCell(WorldModels::UnitModel(_squad.kind));
  panel.enemy = _squad.owner != _state.player;
  const Figures figures = FiguresOf(_state, _squad.id);
  panel.hit = Fraction(figures);
  if (figures.count > 0)
  {
    const auto count = static_cast<std::int32_t>(figures.count);
    panel.hitText = std::format(OUT_OF, figures.hitPoints / count, figures.maxHitPoints / count);
  }
  if (_squad.fullFigures > 1)
  {
    panel.figures = std::format(OUT_OF, figures.count, _squad.fullFigures);
  }
  const auto row = [&panel](std::string_view _key, std::string_view _value)
  { panel.rows.push_back({.key = std::string(_key), .value = std::string(_value), .srgb = SUB_SRGB}); };
  if (_squad.kind == Protocol::UnitKind::Trebuchet)
  {
    const auto engine = std::ranges::find(_state.engines, _squad.id, &Protocol::EngineView::squad);
    const Protocol::EngineState state = engine == _state.engines.end() ? Protocol::EngineState::Packed : engine->state;
    row(STATE_KEY, state == Protocol::EngineState::Deployed  ? STATE_DEPLOYED
                   : state == Protocol::EngineState::Packing ? STATE_PACKING
                   : state == Protocol::EngineState::Packed  ? STATE_PACKED
                                                             : STATE_DEPLOYING);
  }
  for (const auto& [key, value] : look.rows)
  {
    row(key, value);
  }
  // While it moves, its second row reads ORDER Moving (Hud.md §5).
  if (_squad.order == Protocol::SquadOrder::Move && _squad.hasGoal && !_squad.shortOfGoal && panel.rows.size() > 1)
  {
    panel.rows.at(1) = {.key = std::string(ORDER_KEY), .value = std::string(ORDER_MOVING), .srgb = SUB_SRGB};
  }
  return panel;
}

} // namespace

HudPanel BuildPanel(const Selection& _selection, const Protocol::StateUpdate& _state, const Protocol::IntegrityReading* _reading)
{
  if (_selection.structure != Protocol::NO_ENTITY)
  {
    const auto structure = std::ranges::find(_state.structures, _selection.structure, &Protocol::StructureView::id);
    return structure == _state.structures.end()
             ? HudPanel{}
             : StructurePanel(*structure, _state, _reading != nullptr && _reading->structure == structure->id ? _reading : nullptr);
  }
  std::vector<const Protocol::SquadView*> squads;
  for (const Protocol::EntityId id : _selection.squads)
  {
    const auto squad = std::ranges::find(_state.squads, id, &Protocol::SquadView::id);
    if (squad != _state.squads.end())
    {
      squads.push_back(&*squad);
    }
  }
  if (squads.empty())
  {
    return {};
  }
  if (squads.size() == 1)
  {
    return SquadPanel(*squads.front(), _state);
  }
  HudPanel panel;
  panel.shows = HudPanel::Shows::Several;
  panel.title = std::format(SEVERAL_SQUADS, squads.size());
  for (const Protocol::UnitKind kind : Selector::Kinds(_selection, _state))
  {
    Figures total;
    std::size_t count = 0;
    for (const Protocol::SquadView* squad : squads)
    {
      if (squad->kind == kind)
      {
        ++count;
        const Figures figures = FiguresOf(_state, squad->id);
        total.count += figures.count;
        total.hitPoints += figures.hitPoints;
        total.maxHitPoints += figures.maxHitPoints;
      }
    }
    panel.tiles.push_back({.portrait = {.cell = PortraitCell(WorldModels::UnitModel(kind)), .icon = {}},
                           .count = std::to_string(count),
                           .figures = std::to_string(total.count),
                           .hit = Fraction(total),
                           .label = std::string(LookOf(kind).label),
                           .active = _selection.activeKind == kind});
  }
  return panel;
}

} // namespace Client
