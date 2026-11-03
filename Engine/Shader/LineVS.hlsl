// Six vertices a segment, two triangles, for MAX_SEGMENTS segments an instance, from the vertex index alone: no vertex
// buffer (ADR-018, decision 3). Each segment of the instance's model becomes a quad about its stretch of the line, its
// width in the target's pixels across the line as it lies on screen, narrowing to a point at its end when it tapers. A
// line shorter on screen than its model's dotBelowPixels is drawn whole as a square dot of dotWidthPixels about its
// middle, in its longest segment's colour, the shaft's. The vertices of segments a model lacks, or of every segment but
// the first when the line is a dot, meet at one point and draw nothing.

#include "LineCommon.hlsli"

float4 SegmentColor(LineSegment _segment, LineInstance _instance)
{
  return _segment.factionColor != 0 ? _instance.factionColor : _segment.color;
}

// A clip position moved by _offsetPixels across the target, its depth kept.
float4 Offset(float4 _clip, float2 _offsetPixels)
{
  return float4(_clip.xy + _offsetPixels * 2.0 * g_frame.viewportPixels.zw * _clip.w, _clip.z, _clip.w);
}

LineInterpolants main(uint _vertexId : SV_VertexID, uint _instanceId : SV_InstanceID)
{
  // The two triangles of a quad: (near, -), (near, +), (far, -), then (far, -), (near, +), (far, +).
  static const uint FAR_END[6] = {0, 0, 1, 1, 0, 1};
  static const float SIDE[6] = {-1.0, 1.0, -1.0, -1.0, 1.0, 1.0};

  const LineInstance instance = g_instances[_instanceId];
  const LineModel model = g_models[instance.lineModel];
  const uint slot = _vertexId / 6;
  const uint corner = _vertexId % 6;

  LineInterpolants output;
  output.position = float4(0.0, 0.0, 0.0, 1.0);
  output.color = float4(0.0, 0.0, 0.0, 1.0);

  const float lengthMeters = max(model.drawnLengthMeters, 1.0e-6);
  const float3 tail = instance.positionMeters - instance.direction * (lengthMeters * 0.5);
  const float4 tailClip = mul(float4(tail, 1.0), g_frame.viewProjection);
  const float4 headClip = mul(float4(tail + instance.direction * lengthMeters, 1.0), g_frame.viewProjection);
  // Clip x and y in the target's pixels: half the viewport a unit of normalized device coordinates.
  const float2 halfViewport = g_frame.viewportPixels.xy * 0.5;
  const float2 alongPixels = (headClip.xy / headClip.w - tailClip.xy / tailClip.w) * halfViewport;
  const float lengthPixels = length(alongPixels);
  const uint segments = min(model.segmentCount, MAX_SEGMENTS);

  if (lengthPixels < model.dotBelowPixels)
  {
    if (slot != 0 || segments == 0)
    {
      return output;
    }
    uint longest = 0;
    float longestMeters = -1.0;
    for (uint index = 0; index < segments; ++index)
    {
      const LineSegment candidate = g_segments[model.firstSegment + index];
      if (candidate.toMeters - candidate.fromMeters > longestMeters)
      {
        longestMeters = candidate.toMeters - candidate.fromMeters;
        longest = index;
      }
    }
    // The dot's corners: across and along the screen's axes, half its width each way.
    const float4 middle = mul(float4(instance.positionMeters, 1.0), g_frame.viewProjection);
    const float halfWidth = model.dotWidthPixels * 0.5;
    const float2 offset = float2(FAR_END[corner] == 0 ? -halfWidth : halfWidth, SIDE[corner] * halfWidth);
    output.position = Offset(middle, offset);
    output.color = SegmentColor(g_segments[model.firstSegment + longest], instance);
    return output;
  }
  if (slot >= segments)
  {
    return output;
  }

  const LineSegment segment = g_segments[model.firstSegment + slot];
  const float2 along = lengthPixels > 0.0 ? alongPixels / lengthPixels : float2(1.0, 0.0);
  const float2 across = float2(-along.y, along.x);
  const float4 nearClip = lerp(tailClip, headClip, segment.fromMeters / lengthMeters);
  const float4 farClip = lerp(tailClip, headClip, segment.toMeters / lengthMeters);
  const float nearHalf = segment.widthPixels * 0.5;
  const float farHalf = segment.tapers != 0 ? 0.0 : nearHalf;
  const bool atFar = FAR_END[corner] != 0;
  output.position = Offset(atFar ? farClip : nearClip, across * (SIDE[corner] * (atFar ? farHalf : nearHalf)));
  output.color = SegmentColor(segment, instance);
  return output;
}
