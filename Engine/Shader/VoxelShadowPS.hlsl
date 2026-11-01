// The sun's shadow passes (ADR-001 decision 5, ADR-011 decision 9): the voxel pass's quads and its
// ray-box test, cast along the sun's direction, writing only the depth of the hit, conservatively,
// as the voxel pass does.

#include "VoxelCommon.hlsli"

float main(VoxelInterpolants _input) : SV_DepthLessEqual
{
  float3 hit;
  float3 normal;
  if (!CastRay(_input, hit, normal))
  {
    discard;
  }
  const float4 clip = mul(float4(hit, 1.0), g_frame.viewProjection);
  return clip.z / clip.w;
}
