#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace Client
{

/// One game of the Siege playtest (M5-C1; MvpPlan-M5.md, *The session*; GameDesign.md, M5's values, items 3 and 4):
/// which version and which layout a participant plays it in, from their id alone, so that the facilitator never types a
/// version where the participant can see it.
struct PlaytestGame
{
  std::uint32_t game;     // 1 or 2
  std::uint32_t cell;     // (number - 1) mod 4, the id's number its two digits
  bool control;           // the control version, or the voxel one
  bool mirror;            // SiegeMirror, or Siege
  std::string_view group; // "rts" for S, "novice" for N, "pilot" for X
};

/// The cells crossing version order with layout (input 4).
inline constexpr std::uint32_t PLAYTEST_CELLS = 4;

/// Game _game, 1 or 2, of the participant _id: S, N or X and two digits from 01. Cell 0 plays the voxel version on
/// Siege and then the control on SiegeMirror; cell 1 the control on Siege, then the voxel version on SiegeMirror; cell 2
/// the voxel version on SiegeMirror, then the control on Siege; cell 3 the control on SiegeMirror, then the voxel
/// version on Siege. None for an id of another form or another game.
[[nodiscard]] std::optional<PlaytestGame> PlaytestGameOf(std::string_view _id, std::uint32_t _game);

} // namespace Client
