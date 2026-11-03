// The ray through the pixel meets the voxel's box, or the pixel is discarded (ADR-001 decision 4).
// The hit's depth is written conservatively: at or behind the quad's, so early depth rejection
// stays valid (ADR-011 decision 4). The hit is lit as VoxelCommon.hlsli's Lit says, over its
// corner shade, and shaded when its column is not seen now (ADR-025, decision 8).

#include "VoxelCommon.hlsli"

// The column shade of the voxel about _boxCenter: its column's, the 1 m cell its centre stands over, when the mask has
// it and its texel is 0; 1 otherwise, and with no mask.
float ColumnShade(float3 _boxCenter)
{
  if (g_overlay.shadeWidthCells == 0u)
  {
    return 1.0;
  }
  const int2 column = int2(floor(_boxCenter.xz));
  if (column.x < 0 || column.y < 0 || column.x >= int(g_overlay.shadeWidthCells) || column.y >= int(g_overlay.shadeDepthCells))
  {
    return 1.0;
  }
  return g_columnMask.Load(int3(column, 0)) > 0.0 ? 1.0 : g_overlay.columnShade;
}

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
  output.color = float4(Lit(_input.albedo, shade, hit, normal) * ColumnShade(_input.boxCenter), 1.0);
  output.depth = clip.z / clip.w;
  return output;
}
