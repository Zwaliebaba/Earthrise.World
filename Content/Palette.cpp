#include "pch.h"

#include "Palette.h"

#include "PaletteTable.h"

namespace Content
{

static_assert(Generated::PALETTE_SRGB.size() == PALETTE_ENTRIES);

bool PaletteHasEntry(std::uint8_t _index) noexcept
{
  return Generated::PALETTE_PRESENT[_index];
}

std::uint32_t PaletteColorSrgb(std::uint8_t _index) noexcept
{
  return Generated::PALETTE_SRGB[_index];
}

} // namespace Content
