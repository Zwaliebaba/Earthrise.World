// The ray through the pixel meets the oriented voxel, turned into its own frame, or the pixel is
// discarded; the hit is lit as an axis-aligned voxel's is, and its depth is written conservatively
// (ADR-001 decision 4, ADR-011 decision 4). A voxel that carries corner levels, as a turned model's
// do, is shaded by them in its own frame, where they were worked out, as VoxelPS.hlsl shades a
// record's in its batch's; one that carries none, as debris, shades as open (ADR-030, decision 3).
// Either is shaded when its column is not seen now (ADR-025, decision 8).

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
  float3 localHit;
  float3 localNormal;
  if (!CastOrientedRayLocal(_input, hit, normal, localHit, localNormal))
  {
    discard;
  }
  const float4 clip = mul(float4(hit, 1.0), g_frame.viewProjection);
  float shade = 1.0;
  if ((_input.shade & 2u) != 0u)
  {
    const float3 fraction = saturate((localHit + _input.halfEdge) / (2.0 * _input.halfEdge));
    shade = CornerShade(localNormal, fraction, _input.cornerLevels, _input.shade & 1u);
  }
  PixelOutput output;
  output.color = float4(Lit(_input.albedo, shade, hit, normal) * ColumnShade(_input.center), 1.0);
  output.depth = clip.z / clip.w;
  return output;
}
