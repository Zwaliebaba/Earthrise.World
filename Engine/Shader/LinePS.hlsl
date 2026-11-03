// A line's colour as it is: unlit and unshadowed, with no Lambert term or corner shade (ADR-018, decision 3).

#include "LineCommon.hlsli"

float4 main(LineInterpolants _input) : SV_Target0
{
  return float4(_input.color.rgb, 1.0);
}
