// A turned model's silhouette (ADR-030, decision 4; ADR-021, decision 6, amended): the same ray cast as
// OrientedVoxelPS.hlsl, so the hit is the voxel's own, drawn flat in its palette colour at the silhouette's alpha, with
// no light, corner shade or shadow. The pipeline draws it only where the depth test fails, behind what the depth buffer
// holds, and writes no depth, as the silhouette pipeline does for records.

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
  output.color = float4(_input.albedo, g_overlay.alpha);
  output.depth = clip.z / clip.w;
  return output;
}
