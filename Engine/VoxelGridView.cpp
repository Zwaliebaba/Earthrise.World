#include "pch.h"

#include "VoxelGridView.h"

namespace Engine
{

namespace
{

/// Each shaded face's outward normal and its two other axes, in VoxelFace's order.
struct FaceAxes
{
  std::array<std::int32_t, 3> normal;
  std::uint32_t uAxis;
  std::uint32_t wAxis;
};

constexpr std::array<FaceAxes, SHADED_FACES> FACES = {{{.normal = {-1, 0, 0}, .uAxis = 1, .wAxis = 2},
                                                       {.normal = {1, 0, 0}, .uAxis = 1, .wAxis = 2},
                                                       {.normal = {0, 1, 0}, .uAxis = 0, .wAxis = 2},
                                                       {.normal = {0, 0, -1}, .uAxis = 0, .wAxis = 1},
                                                       {.normal = {0, 0, 1}, .uAxis = 0, .wAxis = 1}}};

} // namespace

std::vector<VoxelRecord> BuildVisibleRecords(const VoxelGridView& _grid, const SolidOutside& _solidOutside, const RecordShading& _shading)
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
  const auto solidAt = [&solid](const std::array<std::int32_t, 3>& _cell) { return solid(_cell[0], _cell[1], _cell[2]); };

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
        if (!exposed)
        {
          continue;
        }

        VoxelShade shade = UNSHADED;
        for (std::uint32_t face = 0; face < SHADED_FACES; ++face)
        {
          const FaceAxes& axes = FACES[face];
          const std::array<std::int32_t, 3> front = {cellX + axes.normal[0], cellY + axes.normal[1], cellZ + axes.normal[2]};
          if (solidAt(front))
          {
            continue; // hidden behind its neighbour, so never hit: its corners stay open
          }
          for (std::uint32_t corner = 0; corner < FACE_CORNERS; ++corner)
          {
            // The three cells in front of the face at this corner: one step along u, one along w,
            // and the diagonal (RendererLook.md §5).
            std::array<std::int32_t, 3> side1 = front;
            std::array<std::int32_t, 3> side2 = front;
            side1[axes.uAxis] += (corner & 1U) != 0 ? 1 : -1;
            side2[axes.wAxis] += (corner & 2U) != 0 ? 1 : -1;
            std::array<std::int32_t, 3> diagonal = side1;
            diagonal[axes.wAxis] = side2[axes.wAxis];
            const bool first = solidAt(side1);
            const bool second = solidAt(side2);
            const int occluders = (first ? 1 : 0) + (second ? 1 : 0) + (solidAt(diagonal) ? 1 : 0);
            shade.cornerLevels[face][corner] = first && second ? std::uint8_t{0} : static_cast<std::uint8_t>(OPEN_CORNER - occluders);
          }
        }
        if (_shading.palette != nullptr)
        {
          // Floor division, so that cells either side of zero do not share a jitter cell.
          const std::uint32_t shift = _shading.jitterCellShift;
          const std::int32_t jitterX = (_shading.originCells[0] + cellX) >> shift;
          const std::int32_t jitterY = (_shading.originCells[1] + cellY) >> shift;
          const std::int32_t jitterZ = (_shading.originCells[2] + cellZ) >> shift;
          shade.lightnessOffset = (JitterHash(jitterX, jitterY, jitterZ, paletteIndex) - 0.5F) * _shading.palette->jitter[paletteIndex];
          shade.shadeTable = _shading.palette->shadeTable[paletteIndex];
        }
        records.push_back(MakeVoxelRecord(static_cast<std::uint32_t>(cellX), static_cast<std::uint32_t>(cellY),
                                          static_cast<std::uint32_t>(cellZ), paletteIndex, shade));
      }
    }
  }
  return records;
}

} // namespace Engine
