// The ray through the pixel meets the voxel's box, or the pixel is discarded (ADR-001 decision 4).
// The hit's depth is written conservatively: at or behind the quad's, so early depth rejection
// stays valid (ADR-011 decision 4). The hit is lit as VoxelCommon.hlsli's Lit says, over its
// corner shade.

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

  // The corner shade, read in the batch's own frame, where the record's corners were worked out.
  const float edge = 2.0 * _input.boxRadius;
  const float3 fraction = saturate((hit - (_input.boxCenter - _input.boxRadius)) / edge);
  const uint quarterTurns = (_input.shadeAndTurn >> 1) & 3u;
  const float shade =
    CornerShade(UnturnedDirection(normal, quarterTurns), UnturnedFraction(fraction, quarterTurns), _input.cornerLevels, _input.shadeAndTurn & 1u);

  PixelOutput output;
  output.color = float4(Lit(_input.albedo, shade, hit, normal), 1.0);
  output.depth = clip.z / clip.w;
  return output;
}
