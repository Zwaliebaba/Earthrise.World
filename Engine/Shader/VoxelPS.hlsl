// The ray through the pixel meets the voxel's box, or the pixel is discarded (ADR-001 decision 4).
// The hit's depth is written conservatively: at or behind the quad's, so early depth rejection
// stays valid (ADR-011 decision 4).

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

  // A placeholder light until the look's pass (MvpPlan.md, M1 task 7): a fixed ambient term and
  // Lambert from one direction, enough to tell the faces apart.
  const float3 albedo = g_palette.colors[_input.paletteIndex].rgb;
  const float lambert = saturate(dot(normal, g_frame.lightDirection.xyz));

  PixelOutput output;
  output.color = float4(albedo * (0.35 + 0.65 * lambert), 1.0);
  output.depth = clip.z / clip.w;
  return output;
}
