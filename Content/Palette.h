#pragma once

#include <cstddef>
#include <cstdint>

namespace Content
{

inline constexpr std::size_t PALETTE_ENTRIES = 256;

/// Whether the palette defines a colour for an index. Index 0 is empty and has none.
[[nodiscard]] bool PaletteHasEntry(std::uint8_t _index) noexcept;

/// An index's colour as 0xRRGGBB in sRGB, or 0 where the palette has no entry. Baked at build time
/// from GameData/Server/Palette.json (ADR-005, ADR-008); never typed by hand.
[[nodiscard]] std::uint32_t PaletteColorSrgb(std::uint8_t _index) noexcept;

} // namespace Content
