// The scene at its render scale, sampled bilinearly at this pixel's centre (ADR-011 decision 3).
// The source's sRGB view decodes before the filter, and the target's sRGB view encodes after it, so
// the filter blends linear light. At the same size each pixel lands on a texel's centre and copies it.

#include "UpscaleCommon.hlsli"

float4 main(UpscaleInterpolants _input) : SV_Target0
{
  return g_source.SampleLevel(g_bilinear, _input.uv, 0.0);
}
