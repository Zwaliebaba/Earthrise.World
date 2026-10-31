#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace Content
{

/// A baked voxel shape: a box of palette indices (ADR-005, decision 5), trimmed to its occupied voxels, so that their
/// minimum corner is the origin (ADR-005, decision 2). y is up and +z is the front, as in the layer files.
struct VoxelGrid
{
  static constexpr std::uint8_t EMPTY = 0;

  std::uint32_t widthVoxels;  // along x
  std::uint32_t heightVoxels; // along y
  std::uint32_t depthVoxels;  // along z
  /// 1000 on the world grid and 500 for a model (ADR-006). An integer, so that it hashes exactly (ADR-008, decision 5).
  std::uint32_t voxelEdgeMillimeters;
  /// One palette index per voxel, x fastest, then z, then y, as a layer file's layers[y][z][x]; EMPTY where none is.
  std::span<const std::uint8_t> indices;

  /// The palette index at a position counted in voxels from the origin. Throws std::out_of_range outside the grid.
  [[nodiscard]] constexpr std::uint8_t At(std::uint32_t _xVoxels, std::uint32_t _yVoxels, std::uint32_t _zVoxels) const
  {
    if (_xVoxels >= widthVoxels || _yVoxels >= heightVoxels || _zVoxels >= depthVoxels)
    {
      throw std::out_of_range("VoxelGrid::At: the position is outside the grid");
    }
    const std::size_t row = _zVoxels + (static_cast<std::size_t>(depthVoxels) * _yVoxels);
    return indices[_xVoxels + (static_cast<std::size_t>(widthVoxels) * row)];
  }
};

} // namespace Content
