#pragma once

#include <cstdint>

namespace Sample
{

// A fixture for Build/CheckProjectFiles.py: a correct header and its .cpp in one functional filter.
inline constexpr std::uint32_t CHUNK_EDGE_VOXELS = 32;

/// The colour and position of one voxel, as the renderer receives it.
struct VoxelRecord
{
  std::uint32_t packedPositionVoxels;
  std::uint32_t paletteIndex;
};

[[nodiscard]] std::uint32_t PackPosition(std::uint32_t _xVoxels, std::uint32_t _yVoxels, std::uint32_t _zVoxels) noexcept;

} // namespace Sample
