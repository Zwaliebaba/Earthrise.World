#pragma once

#include <cstdint>

namespace Engine
{

/// One visible voxel as the voxel pass reads it (ADR-011, decision 6). Its layout is load-bearing:
/// Shader/VoxelCommon.hlsli decodes the same bits, and the two change together.
struct VoxelRecord
{
  std::uint32_t cellAndPalette; // x, y and z in the batch, 6 bits each from bit 0, then the palette index in bits 18 to 25
  std::uint32_t shading;        // the look's baked corner shading and jitter (M1 task 7); zero until then
};

static_assert(sizeof(VoxelRecord) == 8, "the shaders read a record as a uint2");

/// The largest cell coordinate a record holds on each axis.
inline constexpr std::uint32_t VOXEL_RECORD_MAX_CELL = 63;

/// A record for the voxel at cell (_x, _y, _z) of its batch. Coordinates above
/// VOXEL_RECORD_MAX_CELL are masked, so the caller keeps batches within it.
[[nodiscard]] constexpr VoxelRecord MakeVoxelRecord(std::uint32_t _x, std::uint32_t _y, std::uint32_t _z,
                                                    std::uint8_t _paletteIndex) noexcept
{
  return {.cellAndPalette = (_x & VOXEL_RECORD_MAX_CELL) | ((_y & VOXEL_RECORD_MAX_CELL) << 6) | ((_z & VOXEL_RECORD_MAX_CELL) << 12) |
                            (static_cast<std::uint32_t>(_paletteIndex) << 18),
          .shading = 0};
}

} // namespace Engine
