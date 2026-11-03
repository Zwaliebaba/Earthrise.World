#pragma once

#include "HudModel.h"
#include "Selection.h"
#include "StateUpdate.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Client
{

/// What a card's slot commands (Hud.md §7; M4-I3): the kinds M4 has, soldiers, sappers, the ram and the trebuchet.
enum class CardCommand : std::uint8_t
{
  None,
  Move,
  Stop,
  Hold,
  Attack,
  Storm,
  Split,
  Merge,
  Trench,
  Fire,
  Pack,
  Deploy
};

/// A shot costs 1.5 m³ of stone (GameDesign.md, M4's values, item 5), in hundredths as the update counts it: FIRE is
/// disabled below it.
inline constexpr std::int32_t SHOT_STONE_HUNDREDTHS = 150;

/// A command's slot as the card shows it: its hotkey, label, icon and one-line rule (Hud.md §7).
struct CommandLook
{
  CardCommand command;
  char hotkey;
  std::string_view label;
  char32_t icon;
  std::string_view rule;
  bool targeted; // a targeting mode, which a right click ends, rather than an order at once
};

/// _command's look; None's is an empty slot's.
[[nodiscard]] const CommandLook& LookOf(CardCommand _command) noexcept;

/// The card for the selection's active kind, from the update, with the slot of _mode active: the slots in Hud.md §7's
/// order, STORM only for the attacker, and each disabled as it says: FIRE while no selected trebuchet is deployed and
/// loaded or the stockpile is below a shot's stone, PACK and DEPLOY while one packs or deploys, SPLIT while every
/// squad of the kind has one figure, MERGE below two squads of it or while all are full. Empty for no active kind.
[[nodiscard]] HudCommandCard BuildCard(const Selection& _selection, const Protocol::StateUpdate& _state, CardCommand _mode);

/// The command a slot of _card holds, as its string reads, or None.
[[nodiscard]] CardCommand CommandOf(const HudCommandSlot& _slot) noexcept;

/// The slot's tooltip: hotkey, label and rule (Hud.md §7); empty for an empty slot.
[[nodiscard]] std::string Tooltip(const HudCommandSlot& _slot);

} // namespace Client
