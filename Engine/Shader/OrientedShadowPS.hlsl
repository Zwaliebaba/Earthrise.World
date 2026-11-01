// Oriented voxels into a shadow map: their ray-box test cast along the sun's direction, writing only
// the depth of the hit, as VoxelShadowPS.hlsl does for axis-aligned ones.

#include "VoxelCommon.hlsli"

float main(OrientedInterpolants _input) : SV_DepthLessEqual
{
  float3 hit;
  float3 normal;
  if (!CastOrientedRay(_input, hit, normal))
  {
    discard;
  }
  const float4 clip = mul(float4(hit, 1.0), g_frame.viewProjection);
  return clip.z / clip.w;
}
