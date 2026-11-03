#include "pch.h"

#include "FogMask.h"

namespace Client
{

std::vector<std::uint8_t> ColumnMask(const Protocol::StateUpdate& _state)
{
  if (_state.visibleCells.empty() || _state.widthCells <= 0 || _state.depthCells <= 0)
  {
    return {};
  }
  constexpr std::size_t WORD_BITS = 64;
  const std::size_t cells = static_cast<std::size_t>(_state.widthCells) * static_cast<std::size_t>(_state.depthCells);
  std::vector<std::uint8_t> texels(cells, std::uint8_t{0});
  for (std::size_t cell = 0; cell < cells; ++cell)
  {
    const std::size_t word = cell / WORD_BITS;
    if (word < _state.visibleCells.size() && ((_state.visibleCells[word] >> (cell % WORD_BITS)) & 1U) != 0)
    {
      texels[cell] = SEEN_TEXEL;
    }
  }
  return texels;
}

} // namespace Client
