#pragma once

#include "ChunkCoordinate.h"

#include <cstdint>
#include <vector>

namespace Client
{

/// One box of a world overlay (ADR-021, decision 1): a world voxel's cell, drawn flat in a palette entry of
/// Client/Look.h's overlays. A box on a world voxel draws over its visible faces, and one in the open, as the
/// passability view's are, draws where nothing hides it.
struct OverlayBox
{
  Protocol::VoxelPosition cellVoxels;
  std::uint8_t paletteIndex;
};

/// Boxes drawn at one alpha, in one overlay draw.
struct OverlayLayer
{
  float alpha;
  std::vector<OverlayBox> boxes;
};

} // namespace Client
