// A quad's colour, linear, or a glyph's: the atlas's texel under the pixel's centre, point-sampled by a load, gives the
// alpha (ADR-019, decision 3). The target's sRGB view blends in linear light.

#include "InterfaceCommon.hlsli"

float4 main(InterfaceInterpolants _input) : SV_Target0
{
  if (_input.textured == 0)
  {
    return _input.color;
  }
  const float coverage = g_atlas.Load(int3(int2(floor(_input.atlasTexels)), 0));
  return float4(_input.color.rgb, _input.color.a * coverage);
}
