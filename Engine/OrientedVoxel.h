#pragma once

#include <DirectXMath.h>

#include "VoxelRecord.h"

#include <cstdint>

namespace Engine
{

/// One voxel at any orientation, drawn through VoxelPass's oriented pipelines: collapse debris as
/// it tumbles and where it comes to rest (RendererLook.md §1; the owner's choice of 2026-11-01,
/// M1's open values, item 7), and unit and engine models turned to any angle (ADR-030). Its layout
/// is load-bearing: Shader/VoxelCommon.hlsli's OrientedVoxel reads the same 48 bytes.
struct OrientedVoxel
{
  DirectX::XMFLOAT3 centerMeters;
  float halfEdgeMeters;
  DirectX::XMFLOAT4 rotation; // a unit quaternion, x, y, z and w, from the voxel's own axes to the world's
  std::uint32_t paletteIndex;
  float lightnessOffset; // HSL, as a VoxelRecord's
  /// The corner levels of faces 0 to 3 in the voxel's own frame, a byte each, as VoxelRecord::cornerLevels holds them.
  std::uint32_t cornerLevels;
  /// Face 4's corner levels in bits 0 to 7, the corner-shade table in bit 8, and ORIENTED_CARRIES_LEVELS. With that bit
  /// clear, as debris has it, every corner shades as open (ADR-030, decision 3).
  std::uint32_t lastFaceAndShade;
};

static_assert(sizeof(OrientedVoxel) == 48, "the shaders read an oriented voxel as 48 bytes");

/// The bit of OrientedVoxel::lastFaceAndShade that says its corners shade, and the one that holds its table.
inline constexpr std::uint32_t ORIENTED_SHADE_TABLE = 1U << 8;
inline constexpr std::uint32_t ORIENTED_CARRIES_LEVELS = 1U << 9;

/// _record's voxel at _centerMeters, _halfEdgeMeters from its centre to a face, turned by the unit quaternion _rotation:
/// its palette index, its lightness offset, and its corner levels and table, which the oriented pixel shader reads in
/// the voxel's own frame, as the record's are read in its batch's (ADR-030, decisions 2 and 3).
[[nodiscard]] inline OrientedVoxel OrientedVoxelOf(const VoxelRecord& _record, const DirectX::XMFLOAT3& _centerMeters,
                                                   float _halfEdgeMeters, const DirectX::XMFLOAT4& _rotation) noexcept
{
  constexpr std::uint32_t PALETTE_SHIFT = 18;
  constexpr std::uint32_t TABLE_SHIFT = 26;
  const std::uint32_t table = ((_record.cellAndPalette >> TABLE_SHIFT) & 1U) != 0 ? ORIENTED_SHADE_TABLE : 0U;
  return {.centerMeters = _centerMeters,
          .halfEdgeMeters = _halfEdgeMeters,
          .rotation = _rotation,
          .paletteIndex = (_record.cellAndPalette >> PALETTE_SHIFT) & 0xffU,
          .lightnessOffset = LightnessOffset(_record),
          .cornerLevels = _record.cornerLevels,
          .lastFaceAndShade = (_record.lastFaceAndJitter & 0xffU) | table | ORIENTED_CARRIES_LEVELS};
}

} // namespace Engine
