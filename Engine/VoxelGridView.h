#pragma once

#include "VoxelRecord.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace Engine
{

/// A dense box of palette indices, 0 for empty, stored x fastest, then z, then y: the order of the
/// layer files, of a chunk and of the baked tables (ADR-005). It holds no game concept (R9).
struct VoxelGridView
{
  std::span<const std::uint8_t> paletteIndices;
  std::uint32_t xVoxels;
  std::uint32_t yVoxels;
  std::uint32_t zVoxels;
};

/// Says whether the cell at (x, y, z), in voxels from the grid's origin and just outside it, is
/// solid. A chunk asks its neighbours; a model answers no. The parameters are unnamed: a function
/// type's names would be reserved identifiers (R1).
using SolidOutside = std::function<bool(std::int32_t, std::int32_t, std::int32_t)>;

/// A record for every solid cell with at least one empty face neighbour, in storage order: the
/// visible voxels ADR-011, decision 6 draws. Buried cells are left out. Throws
/// std::invalid_argument when the grid's span and size disagree or a size exceeds a record's
/// VOXEL_RECORD_MAX_CELL + 1.
[[nodiscard]] std::vector<VoxelRecord> BuildVisibleRecords(const VoxelGridView& _grid, const SolidOutside& _solidOutside);

} // namespace Engine
