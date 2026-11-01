// The ray through the pixel meets the oriented voxel, turned into its own frame, or the pixel is
// discarded; the hit is lit as an axis-aligned voxel's is, with no corner shade, and its depth is
// written conservatively (ADR-001 decision 4, ADR-011 decision 4).

#include "VoxelCommon.hlsli"

struct PixelOutput
{
  float4 color : SV_Target0;
  float depth : SV_DepthLessEqual;
};

PixelOutput main(OrientedInterpolants _input)
{
  float3 hit;
  float3 normal;
  if (!CastOrientedRay(_input, hit, normal))
  {
    discard;
  }
  const float4 clip = mul(float4(hit, 1.0), g_frame.viewProjection);
  PixelOutput output;
  output.color = float4(Lit(_input.albedo, 1.0, hit, normal), 1.0);
  output.depth = clip.z / clip.w;
  return output;
}
