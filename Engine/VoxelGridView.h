#pragma once

#include "VoxelRecord.h"

#include <array>
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

/// What each palette index is shaded with (RendererLook.md §4 and §5): the HSL lightness range
/// its colour is jittered over, and the corner-shade table its corner levels read through.
struct PaletteShading
{
  std::array<float, 256> jitter;
  std::array<std::uint8_t, 256> shadeTable;
};

/// How a grid's records are shaded. With no palette shading, nothing is jittered and every record
/// reads the first table; corner levels are always worked out.
struct RecordShading
{
  const PaletteShading* palette;
  /// Where the grid's first cell sits, in cells: a chunk's place in the world, so that a voxel
  /// keeps its shade whichever chunk holds it, or 0 for a model.
  std::array<std::int32_t, 3> originCells;
  /// A jitter cell is 2 to this power cells on an edge: 0, or 1 for 0.5 m cells jittered as the
  /// 1 m world voxel they split from (RendererLook.md §4).
  std::uint32_t jitterCellShift;
  /// Cells the corner levels also count as solid, in or beyond the grid, or null for none: for a
  /// model, the world's voxels where it stands, so that it is shaded where it touches the world
  /// (RendererLook.md §5). Which faces are exposed is the grid's alone.
  const SolidOutside* alsoSolid;
};

/// A record for every solid cell with at least one empty face neighbour, in storage order: the
/// visible voxels ADR-011, decision 6 draws, each with its corner levels and jitter. Buried cells
/// are left out. A corner's level counts the three cells in front of the face at that corner,
/// within the grid or through _solidOutside (RendererLook.md §5). Throws std::invalid_argument when
/// the grid's span and size disagree or a size exceeds a record's VOXEL_RECORD_MAX_CELL + 1.
[[nodiscard]] std::vector<VoxelRecord> BuildVisibleRecords(const VoxelGridView& _grid, const SolidOutside& _solidOutside,
                                                           const RecordShading& _shading = {});

} // namespace Engine
