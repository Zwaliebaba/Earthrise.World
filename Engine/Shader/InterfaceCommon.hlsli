// Shared by the interface pass's shaders (ADR-019, decision 3). The registers here match the root signature in
// Engine/InterfacePass.cpp, and a change to one is a change to both. A quad's layout matches Engine::InterfaceQuad.

#ifndef INTERFACE_COMMON_HLSLI
#define INTERFACE_COMMON_HLSLI

struct InterfaceQuad
{
  float4 rectanglePixels; // x, y, width, height, top-left origin, in the target's pixels
  float4 atlasTexels;     // u0, v0, u1, v1, in the atlas's texels
  float4 color;           // linear RGBA
  uint textured;          // 1: alpha is the atlas's red channel times color.a; 0: solid
  uint3 reserved;
};

struct InterfaceConstants
{
  float2 targetPixels; // the target's width and height
  float2 unused;
};

struct InterfaceInterpolants
{
  float4 position : SV_Position;
  float2 atlasTexels : TEXCOORD0;
  nointerpolation float4 color : COLOR0;
  nointerpolation uint textured : TEXCOORD1;
};

ConstantBuffer<InterfaceConstants> g_constants : register(b0);
StructuredBuffer<InterfaceQuad> g_quads : register(t0);
Texture2D<float> g_atlas : register(t1); // R8_UNORM

#endif // INTERFACE_COMMON_HLSLI
