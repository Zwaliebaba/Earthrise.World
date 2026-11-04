#pragma once

#include "ChunkCoordinate.h"

#include <cstdint>
#include <span>
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

/// Each of _boxes' joined faces, in _boxes' order, as Engine::WithJoinedFaces takes them: the faces that touch another of
/// _boxes, so that a ray crossing boxes that touch is tinted once (ADR-021, decision 1, amended). Cells lie within 2^20
/// voxels of the origin on each axis.
[[nodiscard]] std::vector<std::uint8_t> JoinedFacesOf(std::span<const OverlayBox> _boxes);

} // namespace Client
