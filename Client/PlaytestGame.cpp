#include "pch.h"

#include "PlaytestGame.h"

namespace Client
{

namespace
{

struct CellGame
{
  bool control;
  bool mirror;
};

/// Each cell's two games: version order crossed with layout, the second game always on the other layout (input 4).
constexpr std::array<std::array<CellGame, 2>, PLAYTEST_CELLS> CELL_GAMES = {{
  {{{.control = false, .mirror = false}, {.control = true, .mirror = true}}},
  {{{.control = true, .mirror = false}, {.control = false, .mirror = true}}},
  {{{.control = false, .mirror = true}, {.control = true, .mirror = false}}},
  {{{.control = true, .mirror = true}, {.control = false, .mirror = false}}},
}};

constexpr std::uint32_t ID_LENGTH = 3;
constexpr std::uint32_t TENS = 10;

std::optional<std::string_view> GroupOf(char _letter) noexcept
{
  switch (_letter)
  {
  case 'S':
    return "rts";
  case 'N':
    return "novice";
  case 'X':
    return "pilot";
  default:
    return std::nullopt;
  }
}

bool IsDigit(char _character) noexcept
{
  return _character >= '0' && _character <= '9';
}

} // namespace

std::optional<PlaytestGame> PlaytestGameOf(std::string_view _id, std::uint32_t _game)
{
  if (_id.size() != ID_LENGTH || (_game != 1 && _game != 2) || !IsDigit(_id[1]) || !IsDigit(_id[2]))
  {
    return std::nullopt;
  }
  const std::optional<std::string_view> group = GroupOf(_id[0]);
  const auto number = (static_cast<std::uint32_t>(_id[1] - '0') * TENS) + static_cast<std::uint32_t>(_id[2] - '0');
  if (!group.has_value() || number == 0)
  {
    return std::nullopt;
  }
  const std::uint32_t cell = (number - 1) % PLAYTEST_CELLS;
  const CellGame& played = CELL_GAMES.at(cell).at(_game - 1);
  return PlaytestGame{.game = _game, .cell = cell, .control = played.control, .mirror = played.mirror, .group = *group};
}

} // namespace Client
