#pragma once

#include <cstdint>

namespace Content
{

/// The shared hash (ADR-008, decision 5): Core::Fnv1a64 over the canonical encoding below of the client view of the
/// shared sources, the palette and the templates. Tools/BakeContent.py computes it and compiles it in here; the
/// server half of the bake (M2) writes the same value into ServerData, and the game refuses to start, or a client to
/// connect, when the two differ. ContentTests recomputes it from Palette.h and Catalog.h, so the bake and these
/// tables cannot drift apart.
///
/// The canonical encoding. Every integer is little-endian, nothing is padded, and the fields come in this order:
///   1. for each palette index from 0 to 255: u8 1 if PaletteHasEntry, else 0; then u32 PaletteColorSrgb;
///   2. u32 the number of templates;
///   3. for each template, in Templates() order: u32 the byte length of its id, then the id's UTF-8 bytes; u32 the
///      byte length of its name, then the name's UTF-8 bytes; u32 grid.widthVoxels, grid.heightVoxels,
///      grid.depthVoxels and grid.voxelEdgeMillimeters; u32 stoneM3 and timberM3; then every u8 of grid.indices, in
///      order.
/// Models are client content, not shared, so the hash does not cover them.
[[nodiscard]] std::uint64_t SharedHash() noexcept;

} // namespace Content
