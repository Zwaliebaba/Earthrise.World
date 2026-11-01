#pragma once

#include "ChunkCoordinate.h"

#include <cstdint>

namespace Protocol
{

/// A voxel and its palette index: one a collapse writes, a rubble voxel landing or a structure voxel
/// crushed in place, or one that fell, as it was. It crosses the boundary in a collapse's view
/// (ADR-020, decision 3).
struct PlacedVoxel
{
  VoxelPosition position;
  std::uint8_t paletteIndex;

  friend constexpr bool operator==(const PlacedVoxel&, const PlacedVoxel&) = default;
};

} // namespace Protocol
