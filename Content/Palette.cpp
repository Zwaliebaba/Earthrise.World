#include "pch.h"

#include "Palette.h"

#include "PaletteTable.h"

namespace Content
{

static_assert(Generated::PALETTE_SRGB.size() == PALETTE_ENTRIES);
static_assert(Generated::CORNER_SHADES.size() == CORNER_SHADE_SETS);

bool PaletteHasEntry(std::uint8_t _index) noexcept
{
  return Generated::PALETTE_PRESENT[_index];
}

std::uint32_t PaletteColorSrgb(std::uint8_t _index) noexcept
{
  return Generated::PALETTE_SRGB[_index];
}

float PaletteJitter(std::uint8_t _index) noexcept
{
  return Generated::PALETTE_JITTER[_index];
}

CornerShadeSet PaletteCornerShadeSet(std::uint8_t _index) noexcept
{
  return static_cast<CornerShadeSet>(Generated::PALETTE_SHADE_SET[_index]);
}

std::array<float, CORNER_SHADE_LEVELS> CornerShades(CornerShadeSet _set) noexcept
{
  return Generated::CORNER_SHADES[static_cast<std::size_t>(_set)];
}

} // namespace Content
