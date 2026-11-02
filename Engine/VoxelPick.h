#pragma once

#include <DirectXMath.h>

#include "VoxelRecord.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace Engine
{

/// A ray to pick along, in meters.
struct PickRay
{
  DirectX::XMFLOAT3 originMeters;
  DirectX::XMFLOAT3 direction; // unit length
};

/// The cell a pick hit, the face it entered by, and how far along the ray.
struct PickHit
{
  std::array<std::int32_t, 3> cell;
  VoxelFace face;
  float distanceMeters;
};

/// Whether the cell at x, y and z is solid. The parameters are unnamed, as SolidOutside's (R1).
using SolidAt = std::function<bool(std::int32_t, std::int32_t, std::int32_t)>;

/// The first solid cell of a grid of cells _voxelEdgeMeters on an edge that _ray meets within _maxDistanceMeters, by the
/// traversal of Amanatides and Woo in float, or nothing. A ray that starts in a solid cell hits it at distance 0, by
/// the face that looks back along the ray's largest component. The camera looks down, so a ray never enters a cell by
/// its bottom face, which VoxelFace does not name: a rising ray throws std::invalid_argument. It runs on the CPU and
/// may disagree with the voxel pass at a box's edge (ADR-001, *Consequences*), so nothing in play may rest on that.
[[nodiscard]] std::optional<PickHit> PickVoxel(const PickRay& _ray, float _voxelEdgeMeters, float _maxDistanceMeters,
                                               const SolidAt& _solidAt);

} // namespace Engine
