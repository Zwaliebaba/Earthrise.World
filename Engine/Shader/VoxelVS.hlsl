// One voxel becomes one screen-space quad (ADR-001 decision 2, ADR-011 decision 6). Under the
// orthographic camera the rectangle that bounds the box's eight projected corners is exact, so
// neither the quadric bounds nor the edge clipping the paper uses for perspective is needed.

#include "VoxelCommon.hlsli"

VoxelInterpolants main(uint _vertexId : SV_VertexID, uint _instanceId : SV_InstanceID)
{
  const uint2 record = g_records[_instanceId];
  const float3 boxMin = g_batch.originMeters + float3(TurnedCell(RecordCell(record.x), g_batch.turn)) * g_batch.voxelEdgeMeters;
  const float3 boxMax = boxMin + g_batch.voxelEdgeMeters;

  float2 low = float2(1.0e30, 1.0e30);
  float2 high = float2(-1.0e30, -1.0e30);
  float nearestDepth = 0.0; // reversed Z: nearer is larger
  for (uint corner = 0; corner < 8; ++corner)
  {
    const float3 point3 = float3((corner & 1u) != 0 ? boxMax.x : boxMin.x, (corner & 2u) != 0 ? boxMax.y : boxMin.y,
                                 (corner & 4u) != 0 ? boxMax.z : boxMin.z);
    const float4 clip = mul(float4(point3, 1.0), g_frame.viewProjection);
    const float2 ndc = clip.xy / clip.w;
    low = min(low, ndc);
    high = max(high, ndc);
    nearestDepth = max(nearestDepth, clip.z / clip.w);
  }

  VoxelInterpolants output;
  // The quad sits at the depth of the box's nearest corner, clamped to the near plane so that a
  // voxel crossing it is not clipped away; every hit lies at or behind it (SV_DepthLessEqual).
  output.position = float4((_vertexId & 1u) != 0 ? high.x : low.x, (_vertexId & 2u) != 0 ? high.y : low.y, min(nearestDepth, 1.0), 1.0);
  output.boxCenter = boxMin + 0.5 * g_batch.voxelEdgeMeters;
  output.boxRadius = 0.5 * g_batch.voxelEdgeMeters;
  const uint paletteIndex = RecordPaletteIndex(record.x);
  output.paletteIndex = paletteIndex == g_batch.swapFromIndex ? g_batch.swapToIndex : paletteIndex;
  return output;
}
