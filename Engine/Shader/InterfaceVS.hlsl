// One quad an instance, two triangles from the vertex index alone: no vertex buffer (ADR-019, decision 3). The corners
// run (0, 0), (1, 0), (0, 1), (1, 1) as a strip, and the quad's rectangle places them on the target's pixels.

#include "InterfaceCommon.hlsli"

InterfaceInterpolants main(uint _vertexId : SV_VertexID, uint _instanceId : SV_InstanceID)
{
  const InterfaceQuad quad = g_quads[_instanceId];
  const float2 corner = float2(float(_vertexId & 1), float(_vertexId >> 1));
  const float2 pixel = quad.rectanglePixels.xy + corner * quad.rectanglePixels.zw;
  InterfaceInterpolants output;
  output.position = float4(pixel.x / g_constants.targetPixels.x * 2.0 - 1.0, 1.0 - pixel.y / g_constants.targetPixels.y * 2.0, 0.0, 1.0);
  output.atlasTexels = lerp(quad.atlasTexels.xy, quad.atlasTexels.zw, corner);
  output.color = quad.color;
  output.textured = quad.textured;
  return output;
}
