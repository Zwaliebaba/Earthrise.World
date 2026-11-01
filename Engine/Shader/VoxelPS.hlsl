// The ray through the pixel meets the voxel's box, or the pixel is discarded (ADR-001 decision 4).
// The hit's depth is written conservatively: at or behind the quad's, so early depth rejection
// stays valid (ADR-011 decision 4). The hit is lit as RendererLook.md §6 says: diffuse only, the
// sun's Lambert term, dimmed where the shadow maps say the sun does not reach, and a hemisphere from
// sky to ground, over the albedo darkened by its corner shade, so the shade stays visible in shadow.

#include "VoxelCommon.hlsli"

Texture2D<float> g_staticShadow : register(t1);  // the world's, drawn when it changes
Texture2D<float> g_dynamicShadow : register(t2); // the models', drawn each frame
SamplerComparisonState g_shadowSampler : register(s0);

struct PixelOutput
{
  float4 color : SV_Target0;
  float depth : SV_DepthLessEqual;
};

// The share of 3 x 3 taps, each itself a blend of 2 x 2 comparisons, at which the map has nothing
// nearer the sun than _depth: percentage-closer filtering, for a soft edge (RendererLook.md §6).
float LitShare(Texture2D<float> _map, float2 _uv, float _depth, float _texel)
{
  float lit = 0.0;
  [unroll] for (int y = -1; y <= 1; ++y)
  {
    [unroll] for (int x = -1; x <= 1; ++x)
    {
      lit += _map.SampleCmpLevelZero(g_shadowSampler, _uv + float2(x, y) * _texel, _depth);
    }
  }
  return lit / 9.0;
}

// How much of the sun reaches a hit: 1 in the open, 0 in shadow. The hit is first moved out along
// its normal, so that a face does not shadow itself, and its depth in the sun's view is biased the
// same way, nearer the sun.
float SunVisibility(float3 _hit, float3 _normal)
{
  const float4 parameters = g_frame.shadowParameters;
  if (parameters.w == 0.0)
  {
    return 1.0;
  }
  const float4 clip = mul(float4(_hit + _normal * parameters.x, 1.0), g_frame.shadowViewProjection);
  const float2 uv = float2(0.5 * clip.x + 0.5, 0.5 - 0.5 * clip.y);
  const float depth = clip.z + parameters.y;
  return min(LitShare(g_staticShadow, uv, depth, parameters.z), LitShare(g_dynamicShadow, uv, depth, parameters.w));
}

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

  const float sunFacing = saturate(dot(normal, g_frame.lightDirection.xyz));
  const float sun = sunFacing > 0.0 ? sunFacing * SunVisibility(hit, normal) : 0.0;
  const float3 hemisphere = lerp(g_frame.groundRadiance.rgb, g_frame.skyRadiance.rgb, 0.5 * normal.y + 0.5);

  PixelOutput output;
  output.color = float4(_input.albedo * shade * (g_frame.sunRadiance.rgb * sun + hemisphere), 1.0);
  output.depth = clip.z / clip.w;
  return output;
}
