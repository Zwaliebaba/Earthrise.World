// Shared by the line pass's shaders (ADR-018, decision 3). The registers here match the root signature in
// Engine/LinePass.cpp, and the layouts match Engine/LinePass.h's records and the start of Engine::VoxelPass's frame
// constants; a change to one is a change to both.

#ifndef LINE_COMMON_HLSLI
#define LINE_COMMON_HLSLI

// The segments of a line model that are drawn: LinePass::MAX_SEGMENTS.
static const uint MAX_SEGMENTS = 4;

// The start of VoxelPass::FrameConstants, as Shader/VoxelCommon.hlsli lays it out; the line pass reads no further.
struct LineFrameConstants
{
  row_major float4x4 viewProjection;        // world to clip, for row vectors
  row_major float4x4 inverseViewProjection; // clip to world
  float4 rayDirection;                      // world, unit length
  float4 viewportPixels;                    // width, height, 1 / width, 1 / height
};

struct LineSegment
{
  float fromMeters;
  float toMeters;
  float4 color;      // linear RGBA; unused when factionColor is 1
  float widthPixels; // in the target's pixels
  uint factionColor; // 1: its instance's faction colour
  uint tapers;       // 1: it narrows to a point at its end
  float reserved;
};

struct LineModel
{
  uint firstSegment;
  uint segmentCount;
  float drawnLengthMeters;
  float dotBelowPixels;
  float dotWidthPixels;
  uint3 reserved;
};

struct LineInstance
{
  float3 positionMeters; // the middle of its drawn length
  uint lineModel;
  float3 direction; // unit length, from its tail to its head
  float reserved;
  float4 factionColor; // linear RGBA
};

struct LineInterpolants
{
  float4 position : SV_Position;
  nointerpolation float4 color : COLOR0;
};

ConstantBuffer<LineFrameConstants> g_frame : register(b0);
StructuredBuffer<LineModel> g_models : register(t0);
StructuredBuffer<LineSegment> g_segments : register(t1);
StructuredBuffer<LineInstance> g_instances : register(t2);

#endif // LINE_COMMON_HLSLI
