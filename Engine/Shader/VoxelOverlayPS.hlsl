// An overlay's voxel (ADR-021, decision 1): the same ray cast as VoxelPS.hlsl, so a box on a world voxel finds the same
// hit at the same depth, drawn flat in its palette colour at the overlay's alpha, with no light, corner shade or shadow.
// Its depth is tested against the scene's and written nowhere: the pipeline masks the write.

#include "VoxelCommon.hlsli"

struct PixelOutput
{
  float4 color : SV_Target0;
  float depth : SV_DepthLessEqual;
};

PixelOutput main(VoxelInterpolants _input)
{
  float3 hit;
  float3 normal;
  if (!CastRay(_input, hit, normal))
  {
    discard;
  }
  const float4 clip = mul(float4(hit, 1.0), g_frame.viewProjection);
  PixelOutput output;
  output.color = float4(_input.albedo, g_overlay.alpha);
  output.depth = clip.z / clip.w;
  return output;
}
