// Shared by the voxel pass's shaders (ADR-001, ADR-011 decision 6). The layouts here match
// Engine/VoxelPass.h and Engine/VoxelRecord.h, and a change to one is a change to both.

#ifndef VOXEL_COMMON_HLSLI
#define VOXEL_COMMON_HLSLI

struct FrameConstants
{
  row_major float4x4 viewProjection;        // world to clip, for row vectors
  row_major float4x4 inverseViewProjection; // clip to world
  float4 rayDirection;                      // world, unit length: the orthographic camera's forward
  float4 viewportPixels;                    // width, height, 1 / width, 1 / height
  float4 lightDirection;                    // world, unit length, towards the light
};

struct PaletteConstants
{
  float4 colors[256]; // linear RGB, by palette index
};

struct BatchConstants
{
  float3 originMeters; // the batch's minimum corner
  float voxelEdgeMeters;
};

ConstantBuffer<FrameConstants> g_frame : register(b0);
ConstantBuffer<PaletteConstants> g_palette : register(b1);
ConstantBuffer<BatchConstants> g_batch : register(b2);
StructuredBuffer<uint2> g_records : register(t0);

struct VoxelInterpolants
{
  float4 position : SV_Position;
  nointerpolation float3 boxCenter : BOX_CENTER;
  nointerpolation float boxRadius : BOX_RADIUS;
  nointerpolation uint paletteIndex : PALETTE_INDEX;
};

// A record's first word: the cell's x, y and z in the batch, 6 bits each from bit 0, then the
// palette index in bits 18 to 25 (VoxelRecord.h).
uint3 RecordCell(uint _word)
{
  return uint3(_word & 63u, (_word >> 6) & 63u, (_word >> 12) & 63u);
}

uint RecordPaletteIndex(uint _word)
{
  return (_word >> 18) & 255u;
}

float MaxComponent(float3 _value)
{
  return max(max(_value.x, _value.y), _value.z);
}

// Majercik et al. 2018, Listing 5, for an axis-aligned box (oriented = false). Returns whether the
// ray hits the box, the distance along the ray to the hit, and the outward normal of the face hit.
// One addition: a ray that starts inside the box, as one does where a voxel crosses the near plane,
// hits at distance zero facing the camera, so the voxel is cut cleanly rather than vanishing or
// showing its far face (AGENTS.md §3).
bool IntersectBox(float3 _boxCenter, float3 _boxRadius, float3 _rayOrigin, float3 _rayDirection, float3 _inverseRayDirection,
                  out float _distance, out float3 _normal)
{
  float3 origin = _rayOrigin - _boxCenter;
  if (MaxComponent(abs(origin) / _boxRadius) < 1.0)
  {
    _distance = 0.0;
    _normal = -_rayDirection;
    return true;
  }

  float3 sgn = -sign(_rayDirection);
  // The distance to each of the three planes that can be front faces.
  float3 d = (_boxRadius * sgn - origin) * _inverseRayDirection;

  bool3 test = bool3((d.x >= 0.0) && all(abs(origin.yz + _rayDirection.yz * d.x) < _boxRadius.yz),
                     (d.y >= 0.0) && all(abs(origin.zx + _rayDirection.zx * d.y) < _boxRadius.zx),
                     (d.z >= 0.0) && all(abs(origin.xy + _rayDirection.xy * d.z) < _boxRadius.xy));
  sgn = test.x ? float3(sgn.x, 0.0, 0.0) : (test.y ? float3(0.0, sgn.y, 0.0) : float3(0.0, 0.0, test.z ? sgn.z : 0.0));

  _distance = (sgn.x != 0.0) ? d.x : ((sgn.y != 0.0) ? d.y : d.z);
  _normal = sgn;
  return (sgn.x != 0.0) || (sgn.y != 0.0) || (sgn.z != 0.0);
}

#endif
