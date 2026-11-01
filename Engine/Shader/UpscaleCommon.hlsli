// Shared by the upscale pass's shaders (ADR-011 decision 3). The registers here match the root
// signature in Engine/UpscalePass.cpp, and a change to one is a change to both.

#ifndef UPSCALE_COMMON_HLSLI
#define UPSCALE_COMMON_HLSLI

struct UpscaleInterpolants
{
  float4 position : SV_Position;
  float2 uv : TEXCOORD0; // the source's texture coordinate: 0 at its top-left edge, 1 at its bottom-right
};

Texture2D<float4> g_source : register(t0); // read through an sRGB view, so samples are linear
SamplerState g_bilinear : register(s0);    // a static sampler: bilinear, clamped at the edges

#endif // UPSCALE_COMMON_HLSLI
