// One triangle that covers the whole target, made from the vertex index alone: no vertex buffer.
// Its corners are at (-1, 1), (3, 1) and (-1, -3) in clip space, so the target's square is inside
// it, and the texture coordinate runs 0 to 1 across the target.

#include "UpscaleCommon.hlsli"

UpscaleInterpolants main(uint _vertexId : SV_VertexID)
{
  const float2 uv = float2(float((_vertexId << 1) & 2), float(_vertexId & 2));
  UpscaleInterpolants output;
  output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
  output.uv = uv;
  return output;
}
