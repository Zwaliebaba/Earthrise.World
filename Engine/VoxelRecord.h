#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace Engine
{

/// The faces a record shades: every face but the bottom one, which the camera, always above the
/// horizon, never sees (ADR-011, decision 2). Shader/VoxelCommon.hlsli numbers them the same.
enum class VoxelFace : std::uint8_t
{
  NegativeX,
  PositiveX,
  PositiveY,
  NegativeZ,
  PositiveZ
};

inline constexpr std::uint32_t SHADED_FACES = 5;
inline constexpr std::uint32_t FACE_CORNERS = 4;

/// The level of a face's corner that nothing occludes, of the four RendererLook.md §5 counts.
inline constexpr std::uint8_t OPEN_CORNER = 3;

/// One visible voxel as the voxel pass reads it (ADR-011, decision 6). Its layout is load-bearing:
/// Shader/VoxelCommon.hlsli decodes the same bits, and the two change together.
///
/// A face's corners are numbered u + 2w, where u and w are 0 or 1 along the face's two other axes,
/// in x, y, z order: y and z for an x face, x and z for the top, x and y for a z face. Each corner
/// holds its level, 0 to 3, in 2 bits from bit 2 x corner, and a face's four corners make a byte.
struct VoxelRecord
{
  /// x, y and z in the batch, 6 bits each from bit 0, then the palette index in bits 18 to 25, then
  /// in bit 26 which of the two corner-shade tables the corners' levels are read through.
  std::uint32_t cellAndPalette;
  /// The corner levels of faces 0 to 3, one byte each from the low byte.
  std::uint32_t cornerLevels;
  /// Face 4's corner levels in bits 0 to 7, then the HSL lightness offset in bits 8 to 15, signed,
  /// in 1/1024ths (RendererLook.md §4).
  std::uint32_t lastFaceAndJitter;
};

static_assert(sizeof(VoxelRecord) == 12, "the shaders read a record as a uint3");

/// The largest cell coordinate a record holds on each axis.
inline constexpr std::uint32_t VOXEL_RECORD_MAX_CELL = 63;

/// The steps of a record's lightness offset, and the largest it holds either way.
inline constexpr float LIGHTNESS_STEPS = 1024.0F;
inline constexpr float MAX_LIGHTNESS_OFFSET = 127.0F / LIGHTNESS_STEPS;

/// How a record is shaded: each face's four corner levels, which of the two corner-shade tables
/// they read through, and the lightness offset of its colour.
struct VoxelShade
{
  std::array<std::array<std::uint8_t, FACE_CORNERS>, SHADED_FACES> cornerLevels;
  std::uint8_t shadeTable;
  float lightnessOffset;
};

/// Every corner open, the first table, and no offset: a voxel drawn in its palette colour.
inline constexpr VoxelShade UNSHADED{.cornerLevels = {{{OPEN_CORNER, OPEN_CORNER, OPEN_CORNER, OPEN_CORNER},
                                                       {OPEN_CORNER, OPEN_CORNER, OPEN_CORNER, OPEN_CORNER},
                                                       {OPEN_CORNER, OPEN_CORNER, OPEN_CORNER, OPEN_CORNER},
                                                       {OPEN_CORNER, OPEN_CORNER, OPEN_CORNER, OPEN_CORNER},
                                                       {OPEN_CORNER, OPEN_CORNER, OPEN_CORNER, OPEN_CORNER}}},
                                     .shadeTable = 0,
                                     .lightnessOffset = 0.0F};

/// A record for the voxel at cell (_x, _y, _z) of its batch, shaded as _shade says. Coordinates
/// above VOXEL_RECORD_MAX_CELL and corner levels above 3 are masked, and the lightness offset is
/// rounded to its steps and clamped to MAX_LIGHTNESS_OFFSET, so the caller keeps within them.
[[nodiscard]] inline VoxelRecord MakeVoxelRecord(std::uint32_t _x, std::uint32_t _y, std::uint32_t _z, std::uint8_t _paletteIndex,
                                                 const VoxelShade& _shade = UNSHADED) noexcept
{
  const auto faceByte = [&_shade](std::uint32_t _face)
  {
    std::uint32_t levels = 0;
    for (std::uint32_t corner = 0; corner < FACE_CORNERS; ++corner)
    {
      levels |= (static_cast<std::uint32_t>(_shade.cornerLevels[_face][corner]) & 3U) << (2 * corner);
    }
    return levels;
  };
  const float steps =
    std::round(std::fmin(std::fmax(_shade.lightnessOffset, -MAX_LIGHTNESS_OFFSET), MAX_LIGHTNESS_OFFSET) * LIGHTNESS_STEPS);
  const auto offset = static_cast<std::uint32_t>(static_cast<std::int32_t>(steps)) & 0xffU;
  return {.cellAndPalette = (_x & VOXEL_RECORD_MAX_CELL) | ((_y & VOXEL_RECORD_MAX_CELL) << 6) | ((_z & VOXEL_RECORD_MAX_CELL) << 12) |
                            (static_cast<std::uint32_t>(_paletteIndex) << 18) |
                            ((static_cast<std::uint32_t>(_shade.shadeTable) & 1U) << 26),
          .cornerLevels = faceByte(0) | (faceByte(1) << 8) | (faceByte(2) << 16) | (faceByte(3) << 24),
          .lastFaceAndJitter = faceByte(4) | (offset << 8)};
}

/// A record's level at one corner of one face, as the shaders read it.
[[nodiscard]] constexpr std::uint8_t CornerLevel(const VoxelRecord& _record, VoxelFace _face, std::uint32_t _corner) noexcept
{
  const auto face = static_cast<std::uint32_t>(_face);
  const std::uint32_t levels = face < 4 ? _record.cornerLevels >> (8 * face) : _record.lastFaceAndJitter;
  return static_cast<std::uint8_t>((levels >> (2 * (_corner & 3U))) & 3U);
}

/// A record's lightness offset, as the shaders read it.
[[nodiscard]] constexpr float LightnessOffset(const VoxelRecord& _record) noexcept
{
  const auto steps = static_cast<std::int8_t>(static_cast<std::uint8_t>((_record.lastFaceAndJitter >> 8) & 0xffU));
  return static_cast<float>(steps) / LIGHTNESS_STEPS;
}

/// RendererLook.md §4's hash of a jitter cell and a material, in [0, 1]: wrapping 32-bit integer
/// arithmetic, so it is the same on every machine and matches the prototype's.
[[nodiscard]] constexpr float JitterHash(std::int32_t _cellX, std::int32_t _cellY, std::int32_t _cellZ, std::uint8_t _material) noexcept
{
  // The prototype's fold of three coordinates into two, kept so that colours match it exactly.
  const auto x = static_cast<std::uint32_t>(_cellX) * 7U + static_cast<std::uint32_t>(_cellY) * 131U;
  const auto y = static_cast<std::uint32_t>(_cellZ) * 3U;
  std::uint32_t h = x * 374761393U + y * 668265263U + static_cast<std::uint32_t>(_material) * 982451653U;
  h = (h ^ (h >> 13)) * 1274126177U;
  return static_cast<float>(static_cast<double>(h ^ (h >> 16)) / 4294967295.0);
}

} // namespace Engine
