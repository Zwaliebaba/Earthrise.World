#pragma once

#include <array>
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

/// The HSL lightness range an index's colour is jittered over: each voxel's offset falls within
/// plus or minus half of it (RendererLook.md §4). 0 where the palette has no entry.
[[nodiscard]] float PaletteJitter(std::uint8_t _index) noexcept;

/// Which corner shades a material takes (RendererLook.md §5). Terrain's are reduced, so that
/// terraces do not read as noise.
enum class CornerShadeSet : std::uint8_t
{
  Structure,
  Terrain
};

inline constexpr std::size_t CORNER_SHADE_SETS = 2;
inline constexpr std::size_t CORNER_SHADE_LEVELS = 4;

/// An index's corner-shade set; Structure where the palette has no entry.
[[nodiscard]] CornerShadeSet PaletteCornerShadeSet(std::uint8_t _index) noexcept;

/// A set's shade at each corner level, from 0, the most occluded, to 3, open.
[[nodiscard]] std::array<float, CORNER_SHADE_LEVELS> CornerShades(CornerShadeSet _set) noexcept;

} // namespace Content
