#pragma once

#include <DirectXMath.h>

#include <array>
#include <cstdint>

namespace Engine
{

/// One voxel at any orientation, drawn through VoxelPass's oriented pipelines: collapse debris as
/// it tumbles and where it comes to rest (RendererLook.md §1; the owner's choice of 2026-11-01,
/// M1's open values, item 7). Its layout is load-bearing: Shader/VoxelCommon.hlsli's OrientedVoxel
/// reads the same 48 bytes.
struct OrientedVoxel
{
  DirectX::XMFLOAT3 centerMeters;
  float halfEdgeMeters;
  DirectX::XMFLOAT4 rotation; // a unit quaternion, x, y, z and w, from the voxel's own axes to the world's
  std::uint32_t paletteIndex;
  float lightnessOffset; // HSL, as a VoxelRecord's
  std::array<std::uint32_t, 2> reserved;
};

static_assert(sizeof(OrientedVoxel) == 48, "the shaders read an oriented voxel as 48 bytes");

} // namespace Engine
