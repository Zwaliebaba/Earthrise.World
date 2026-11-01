#pragma once

#include "ChunkCoordinate.h"
#include "ChunkStore.h"
#include "MaterialRules.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace Simulation
{

/// What the support search needs to know about the world beyond its voxels (ADR-004, decision 2).
struct SupportRules
{
  const MaterialRules* materials;
  /// Any voxel this far or further below the generated surface is an anchor, as bedrock is.
  std::int32_t anchorDepthVoxels;
  /// The generated surface's height at a column, in voxels: where the generator left the ground,
  /// which edits do not move. Takes x and z.
  std::function<std::int32_t(std::int32_t, std::int32_t)> surfaceVoxels;
  /// The world's height in voxels. Nothing stands above it.
  std::int32_t heightVoxels;
};

/// A piece that lost support: voxels joined by their faces, in y, then z, then x order.
using Piece = std::vector<Protocol::VoxelPosition>;

struct SupportSearchResult
{
  std::vector<Piece> pieces;    // in the order of each piece's first voxel
  std::uint64_t voxelsSearched; // the size of the box the search read, for the bench
};

/// Finds what lost support after _edits (ADR-004, decision 2). A voxel's support distance is the
/// fewest horizontal steps on any path from it to an anchor that moves only down or sideways
/// through solid voxels, and a voxel whose material spans is supported while that distance is no
/// more than its span. An edit can change support only for voxels at or above it and within the
/// longest span of it horizontally, so only those are judged, and only the box twice that span
/// about the edits, from the anchors to the top of the world, is read. Nothing searches a whole
/// structure or the world.
[[nodiscard]] SupportSearchResult FindUnsupported(const ChunkStore& _store, const SupportRules& _rules,
                                                  std::span<const Protocol::VoxelPosition> _edits);

} // namespace Simulation
