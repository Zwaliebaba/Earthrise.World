#include "pch.h"

#include "VoxelGridView.h"

namespace Engine
{

std::vector<VoxelRecord> BuildVisibleRecords(const VoxelGridView& _grid, const SolidOutside& _solidOutside)
{
  constexpr std::uint32_t MAX_EDGE = VOXEL_RECORD_MAX_CELL + 1;
  if (_grid.xVoxels > MAX_EDGE || _grid.yVoxels > MAX_EDGE || _grid.zVoxels > MAX_EDGE ||
      _grid.paletteIndices.size() != static_cast<std::size_t>(_grid.xVoxels) * _grid.yVoxels * _grid.zVoxels)
  {
    throw std::invalid_argument("a voxel grid's cells and its size disagree, or it is larger than a record can address");
  }

  const auto x = static_cast<std::int32_t>(_grid.xVoxels);
  const auto y = static_cast<std::int32_t>(_grid.yVoxels);
  const auto z = static_cast<std::int32_t>(_grid.zVoxels);
  const auto solid = [&](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    if (_x < 0 || _y < 0 || _z < 0 || _x >= x || _y >= y || _z >= z)
    {
      return _solidOutside(_x, _y, _z);
    }
    return _grid.paletteIndices[(static_cast<std::size_t>(_y) * static_cast<std::size_t>(z) + static_cast<std::size_t>(_z)) *
                                  static_cast<std::size_t>(x) +
                                static_cast<std::size_t>(_x)] != 0;
  };

  std::vector<VoxelRecord> records;
  std::size_t index = 0;
  for (std::int32_t cellY = 0; cellY < y; ++cellY)
  {
    for (std::int32_t cellZ = 0; cellZ < z; ++cellZ)
    {
      for (std::int32_t cellX = 0; cellX < x; ++cellX, ++index)
      {
        const std::uint8_t paletteIndex = _grid.paletteIndices[index];
        if (paletteIndex == 0)
        {
          continue;
        }
        const bool exposed = !solid(cellX - 1, cellY, cellZ) || !solid(cellX + 1, cellY, cellZ) || !solid(cellX, cellY - 1, cellZ) ||
                             !solid(cellX, cellY + 1, cellZ) || !solid(cellX, cellY, cellZ - 1) || !solid(cellX, cellY, cellZ + 1);
        if (exposed)
        {
          records.push_back(MakeVoxelRecord(static_cast<std::uint32_t>(cellX), static_cast<std::uint32_t>(cellY),
                                            static_cast<std::uint32_t>(cellZ), paletteIndex));
        }
      }
    }
  }
  return records;
}

} // namespace Engine
