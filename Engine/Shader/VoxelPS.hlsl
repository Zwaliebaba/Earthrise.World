// The ray through the pixel meets the voxel's box, or the pixel is discarded (ADR-001 decision 4).
// The hit's depth is written conservatively: at or behind the quad's, so early depth rejection
// stays valid (ADR-011 decision 4). The hit is lit as RendererLook.md §6 says: diffuse only, the
// sun's Lambert term and a hemisphere from sky to ground, over the albedo darkened by its corner
// shade, so the shade stays visible out of the sun.

#include "VoxelCommon.hlsli"

struct PixelOutput
{
  float4 color : SV_Target0;
  float depth : SV_DepthLessEqual;
};

PixelOutput main(VoxelInterpolants _input)
{
  // The orthographic ray: from the near plane at this pixel, along the camera's forward.
  const float2 ndc = float2(_input.position.x * g_frame.viewportPixels.z * 2.0 - 1.0, 1.0 - _input.position.y * g_frame.viewportPixels.w * 2.0);
  const float4 nearPoint = mul(float4(ndc, 1.0, 1.0), g_frame.inverseViewProjection);
  const float3 rayOrigin = nearPoint.xyz / nearPoint.w;
  const float3 rayDirection = g_frame.rayDirection.xyz;

  float hitDistance;
  float3 normal;
  if (!IntersectBox(_input.boxCenter, float3(_input.boxRadius, _input.boxRadius, _input.boxRadius), rayOrigin, rayDirection,
                    1.0 / rayDirection, hitDistance, normal))
  {
    discard;
  }

  const float3 hit = rayOrigin + rayDirection * hitDistance;
  const float4 clip = mul(float4(hit, 1.0), g_frame.viewProjection);

  // The corner shade, read in the batch's own frame, where the record's corners were worked out.
  const float edge = 2.0 * _input.boxRadius;
  const float3 fraction = saturate((hit - (_input.boxCenter - _input.boxRadius)) / edge);
  const uint quarterTurns = (_input.shadeAndTurn >> 1) & 3u;
  const float shade =
    CornerShade(UnturnedDirection(normal, quarterTurns), UnturnedFraction(fraction, quarterTurns), _input.cornerLevels, _input.shadeAndTurn & 1u);

  const float sunFacing = saturate(dot(normal, g_frame.lightDirection.xyz));
  const float3 hemisphere = lerp(g_frame.groundRadiance.rgb, g_frame.skyRadiance.rgb, 0.5 * normal.y + 0.5);

  PixelOutput output;
  output.color = float4(_input.albedo * shade * (g_frame.sunRadiance.rgb * sunFacing + hemisphere), 1.0);
  output.depth = clip.z / clip.w;
  return output;
}
