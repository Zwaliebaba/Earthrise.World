#include "pch.h"

#include "CommandCard.h"

#include "Strings.h"

namespace Client
{

namespace
{

// Each command's hotkey, label, icon glyph and rule (Hud.md §7; the font's icons at U+E000 on, M4-B5).
constexpr std::array<CommandLook, 12> COMMAND_LOOKS = {{
  {CardCommand::None, ' ', {}, 0, {}, false},
  {CardCommand::Move, 'M', MOVE_LABEL, U'\uE000', MOVE_RULE, true},
  {CardCommand::Stop, 'X', STOP_LABEL, U'\uE002', STOP_RULE, false},
  {CardCommand::Hold, 'H', HOLD_LABEL, U'\uE003', HOLD_RULE, false},
  {CardCommand::Attack, 'T', ATTACK_LABEL, U'\uE001', ATTACK_RULE, true},
  {CardCommand::Storm, 'R', STORM_LABEL, U'\uE006', STORM_RULE, true},
  {CardCommand::Split, 'P', SPLIT_LABEL, U'\uE019', SPLIT_RULE, false},
  {CardCommand::Merge, 'G', MERGE_LABEL, U'\uE01A', MERGE_RULE, false},
  {CardCommand::Trench, 'T', TRENCH_LABEL, U'\uE00F', TRENCH_RULE, true},
  {CardCommand::Fire, 'F', FIRE_LABEL, U'\uE004', FIRE_RULE, true},
  {CardCommand::Pack, 'P', PACK_LABEL, U'\uE005', PACK_RULE, false},
  {CardCommand::Deploy, 'P', DEPLOY_LABEL, U'\uE01B', DEPLOY_RULE, false},
}};

constexpr std::size_t CARD_SLOTS = 12; // 4 x 3 (Hud.md §7)
using CardLayout = std::array<CardCommand, CARD_SLOTS>;

constexpr CardCommand NONE = CardCommand::None;
// Hud.md §7's cards, slots 1 to 12. The trebuchet's fifth slot is PACK or DEPLOY by its state.
constexpr CardLayout SOLDIERS = {CardCommand::Move,
                                 CardCommand::Stop,
                                 CardCommand::Hold,
                                 CardCommand::Attack,
                                 CardCommand::Storm,
                                 CardCommand::Split,
                                 CardCommand::Merge,
                                 NONE,
                                 NONE,
                                 NONE,
                                 NONE,
                                 NONE};
constexpr CardLayout SAPPERS = {CardCommand::Move,
                                CardCommand::Stop,
                                CardCommand::Hold,
                                NONE,
                                NONE,
                                CardCommand::Split,
                                CardCommand::Merge,
                                CardCommand::Trench,
                                NONE,
                                NONE,
                                NONE,
                                NONE};
constexpr CardLayout RAM = {
  CardCommand::Move, CardCommand::Stop, CardCommand::Attack, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE};
constexpr CardLayout TREBUCHET = {
  CardCommand::Fire, CardCommand::Stop, NONE, NONE, CardCommand::Pack, NONE, NONE, NONE, NONE, NONE, NONE, NONE};

/// A code point as UTF-8, three bytes for the icons in the private use area.
std::string Utf8(char32_t _codePoint)
{
  std::string text;
  text.push_back(static_cast<char>(0xE0 | (_codePoint >> 12)));
  text.push_back(static_cast<char>(0x80 | ((_codePoint >> 6) & 0x3F)));
  text.push_back(static_cast<char>(0x80 | (_codePoint & 0x3F)));
  return text;
}

/// The active kind's squads in the update.
std::vector<const Protocol::SquadView*> ActiveSquads(const Selection& _selection, const Protocol::StateUpdate& _state)
{
  std::vector<const Protocol::SquadView*> squads;
  for (const Protocol::EntityId id : _selection.squads)
  {
    const auto squad = std::ranges::find(_state.squads, id, &Protocol::SquadView::id);
    if (squad != _state.squads.end() && _selection.activeKind == squad->kind)
    {
      squads.push_back(&*squad);
    }
  }
  return squads;
}

std::size_t FiguresOf(const Protocol::StateUpdate& _state, Protocol::EntityId _squad)
{
  return static_cast<std::size_t>(std::ranges::count(_state.units, _squad, &Protocol::UnitView::squad));
}

} // namespace

const CommandLook& LookOf(CardCommand _command) noexcept
{
  return COMMAND_LOOKS.at(static_cast<std::size_t>(_command));
}

HudCommandCard BuildCard(const Selection& _selection, const Protocol::StateUpdate& _state, CardCommand _mode)
{
  HudCommandCard card;
  if (!_selection.activeKind.has_value())
  {
    return card;
  }
  const Protocol::UnitKind kind = *_selection.activeKind;
  const std::vector<const Protocol::SquadView*> squads = ActiveSquads(_selection, _state);
  CardLayout layout = SOLDIERS;
  switch (kind)
  {
  case Protocol::UnitKind::Sapper:
    layout = SAPPERS;
    break;
  case Protocol::UnitKind::Ram:
    layout = RAM;
    break;
  case Protocol::UnitKind::Trebuchet:
    layout = TREBUCHET;
    break;
  case Protocol::UnitKind::Laborer:
    return card; // the laborers' cards are M6's
  default:
    break;
  }
  // The player's side and stockpile, and the selected trebuchets' engines.
  const auto player = std::ranges::find(_state.players, _state.player, &Protocol::PlayerView::player);
  const bool attacker = player == _state.players.end() || player->side == Protocol::Side::Attacker;
  const std::int32_t stone = player == _state.players.end() ? std::numeric_limits<std::int32_t>::max() : player->stoneHundredthsCubicMeters;
  std::vector<const Protocol::EngineView*> engines;
  for (const Protocol::SquadView* squad : squads)
  {
    const auto engine = std::ranges::find(_state.engines, squad->id, &Protocol::EngineView::squad);
    if (engine != _state.engines.end())
    {
      engines.push_back(&*engine);
    }
  }
  const auto anyEngine = [&engines](const auto& _test) { return std::ranges::any_of(engines, _test); };
  const bool packed = anyEngine([](const Protocol::EngineView* _engine) { return _engine->state == Protocol::EngineState::Packed; });
  const bool deployed = anyEngine([](const Protocol::EngineView* _engine) { return _engine->state == Protocol::EngineState::Deployed; });
  const bool loaded = anyEngine([](const Protocol::EngineView* _engine)
                                { return _engine->state == Protocol::EngineState::Deployed && _engine->reloadTicks == 0; });
  const bool full = std::ranges::all_of(squads, [&_state](const Protocol::SquadView* _squad)
                                        { return FiguresOf(_state, _squad->id) >= _squad->fullFigures; });
  const bool divisible =
    std::ranges::any_of(squads, [&_state](const Protocol::SquadView* _squad) { return FiguresOf(_state, _squad->id) > 1; });

  static constexpr std::array<std::string_view, 8> KIND_NAMES = {"LABORER",     "SPEARMAN", "ARCHER", "OUTRIDER",
                                                                 "MAN-AT-ARMS", "SAPPER",   "RAM",    "TREBUCHET"};
  card.kind = std::string(KIND_NAMES.at(static_cast<std::size_t>(kind)));
  for (std::size_t index = 0; index < layout.size(); ++index)
  {
    CardCommand command = layout.at(index);
    if (command == CardCommand::Storm && !attacker)
    {
      command = CardCommand::None; // the attacker's alone (Hud.md §7)
    }
    if (command == CardCommand::Pack && packed && !deployed)
    {
      command = CardCommand::Deploy;
    }
    if (command == CardCommand::None)
    {
      continue;
    }
    bool enabled = true;
    switch (command)
    {
    case CardCommand::Fire:
      enabled = loaded && stone >= SHOT_STONE_HUNDREDTHS;
      break;
    case CardCommand::Pack:
      enabled = deployed;
      break;
    case CardCommand::Deploy:
      enabled = packed;
      break;
    case CardCommand::Split:
      enabled = divisible;
      break;
    case CardCommand::Merge:
      enabled = squads.size() >= 2 && !full;
      break;
    default:
      break;
    }
    const CommandLook& look = LookOf(command);
    card.slots.at(index) = {.command = std::string(look.label),
                            .hotkey = std::string(1, look.hotkey),
                            .label = std::string(look.label),
                            .icon = Utf8(look.icon),
                            .enabled = enabled,
                            .active = command == _mode};
  }
  return card;
}

CardCommand CommandOf(const HudCommandSlot& _slot) noexcept
{
  const auto found = std::ranges::find_if(COMMAND_LOOKS, [&_slot](const CommandLook& _look)
                                          { return _look.command != CardCommand::None && _look.label == _slot.command; });
  return found == COMMAND_LOOKS.end() ? CardCommand::None : found->command;
}

std::string Tooltip(const HudCommandSlot& _slot)
{
  const CardCommand command = CommandOf(_slot);
  if (command == CardCommand::None)
  {
    return {};
  }
  return std::format(CARD_TOOLTIP, _slot.hotkey, _slot.label, LookOf(command).rule);
}

} // namespace Client
