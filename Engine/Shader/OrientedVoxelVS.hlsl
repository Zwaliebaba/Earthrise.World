// One oriented voxel becomes one screen-space quad, as an axis-aligned one does (ADR-001 decision
// 2): the rectangle that bounds its eight corners, turned by its rotation, under the orthographic
// camera. Collapse debris is drawn this way while it tumbles and where it comes to rest
// (Engine/OrientedVoxel.h).

#include "VoxelCommon.hlsli"

StructuredBuffer<OrientedVoxel> g_voxels : register(t0);

OrientedInterpolants main(uint _vertexId : SV_VertexID, uint _instanceId : SV_InstanceID)
{
  const OrientedVoxel voxel = g_voxels[_instanceId];

  float2 low = float2(1.0e30, 1.0e30);
  float2 high = float2(-1.0e30, -1.0e30);
  float nearestDepth = 0.0; // reversed Z: nearer is larger
  for (uint corner = 0; corner < 8; ++corner)
  {
    const float3 local = voxel.halfEdgeMeters * float3((corner & 1u) != 0 ? 1.0 : -1.0, (corner & 2u) != 0 ? 1.0 : -1.0,
                                                       (corner & 4u) != 0 ? 1.0 : -1.0);
    const float4 clip = mul(float4(voxel.centerMeters + Rotate(voxel.rotation, local), 1.0), g_frame.viewProjection);
    const float2 ndc = clip.xy / clip.w;
    low = min(low, ndc);
    high = max(high, ndc);
    nearestDepth = max(nearestDepth, clip.z / clip.w);
  }

  OrientedInterpolants output;
  output.position = float4((_vertexId & 1u) != 0 ? high.x : low.x, (_vertexId & 2u) != 0 ? high.y : low.y, min(nearestDepth, 1.0), 1.0);
  output.center = voxel.centerMeters;
  output.halfEdge = voxel.halfEdgeMeters;
  output.rotation = voxel.rotation;
  output.albedo = JitterLightness(g_palette.colors[voxel.paletteIndex & 255u].rgb, voxel.lightnessOffset);
  return output;
}
