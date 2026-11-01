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
  // The light's radiance, linear, its intensity and Lambert's 1 / pi already in: the sun's, and the
  // hemisphere's from the sky above and the ground below (RendererLook.md §6).
  float4 sunRadiance;
  float4 skyRadiance;
  float4 groundRadiance;
  float4 cornerShades[2]; // each table's shade at corner levels 0 to 3 (RendererLook.md §5)
  row_major float4x4 shadowViewProjection; // the sun's, for both shadow maps
  // x: the normal offset in meters; y: the depth bias; z: a static-map texel; w: a dynamic-map
  // texel, or 0 for no shadows (VoxelPass.h).
  float4 shadowParameters;
};

struct PaletteConstants
{
  float4 colors[256]; // linear RGB, by palette index
};

struct BatchConstants
{
  float3 originMeters; // the minimum corner of the batch's footprint, after any turn
  float voxelEdgeMeters;
  uint turn;          // quarter turns clockwise seen from above in bits 0-1; the width less one in
                      // cells in bits 2-7 and the depth less one in bits 8-13, before the turn
  uint swapFromIndex; // drawn as swapToIndex; 0 swaps nothing
  uint swapToIndex;
};

ConstantBuffer<FrameConstants> g_frame : register(b0);
ConstantBuffer<PaletteConstants> g_palette : register(b1);
ConstantBuffer<BatchConstants> g_batch : register(b2);
StructuredBuffer<uint3> g_records : register(t0);

struct VoxelInterpolants
{
  // A pixel shader that writes SV_DepthLessEqual must read the position at the centroid, without
  // perspective (DXIL validation); with one sample per pixel that is the pixel's centre.
  noperspective centroid float4 position : SV_Position;
  nointerpolation float3 boxCenter : BOX_CENTER;
  nointerpolation float boxRadius : BOX_RADIUS;
  nointerpolation float3 albedo : ALBEDO;              // linear, jittered
  nointerpolation uint2 cornerLevels : CORNER_LEVELS; // the record's second word, and face 4's byte
  nointerpolation uint shadeAndTurn : SHADE_AND_TURN; // the shade table in bit 0, the batch's quarter turns in bits 1-2
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

// Which corner-shade table the record's corners read through: bit 26 of its first word.
uint RecordShadeTable(uint _word)
{
  return (_word >> 26) & 1u;
}

// The record's HSL lightness offset: bits 8 to 15 of its third word, signed, in 1/1024ths.
float RecordLightnessOffset(uint _word)
{
  return float(asint(_word << 16) >> 24) / 1024.0;
}

// three.js r160's Color.getHSL and setHSL, which the prototype's offsetHSL uses, on linear RGB: its
// working colour space (RendererLook.md §4).
float3 RgbToHsl(float3 _rgb)
{
  const float maxChannel = max(max(_rgb.r, _rgb.g), _rgb.b);
  const float minChannel = min(min(_rgb.r, _rgb.g), _rgb.b);
  const float lightness = 0.5 * (minChannel + maxChannel);
  if (minChannel == maxChannel)
  {
    return float3(0.0, 0.0, lightness);
  }
  const float delta = maxChannel - minChannel;
  const float saturation = lightness <= 0.5 ? delta / (maxChannel + minChannel) : delta / (2.0 - maxChannel - minChannel);
  float hue;
  if (maxChannel == _rgb.r)
  {
    hue = (_rgb.g - _rgb.b) / delta + (_rgb.g < _rgb.b ? 6.0 : 0.0);
  }
  else if (maxChannel == _rgb.g)
  {
    hue = (_rgb.b - _rgb.r) / delta + 2.0;
  }
  else
  {
    hue = (_rgb.r - _rgb.g) / delta + 4.0;
  }
  return float3(hue / 6.0, saturation, lightness);
}

float HueToRgb(float _p, float _q, float _t)
{
  float t = _t;
  if (t < 0.0)
  {
    t += 1.0;
  }
  if (t > 1.0)
  {
    t -= 1.0;
  }
  if (t < 1.0 / 6.0)
  {
    return _p + (_q - _p) * 6.0 * t;
  }
  if (t < 0.5)
  {
    return _q;
  }
  if (t < 2.0 / 3.0)
  {
    return _p + (_q - _p) * 6.0 * (2.0 / 3.0 - t);
  }
  return _p;
}

float3 HslToRgb(float3 _hsl)
{
  const float hue = _hsl.x - floor(_hsl.x);
  const float saturation = saturate(_hsl.y);
  const float lightness = saturate(_hsl.z);
  if (saturation == 0.0)
  {
    return float3(lightness, lightness, lightness);
  }
  const float p = lightness <= 0.5 ? lightness * (1.0 + saturation) : lightness + saturation - lightness * saturation;
  const float q = 2.0 * lightness - p;
  return float3(HueToRgb(q, p, hue + 1.0 / 3.0), HueToRgb(q, p, hue), HueToRgb(q, p, hue - 1.0 / 3.0));
}

// A palette colour with its HSL lightness moved by _offset, hue and saturation kept.
float3 JitterLightness(float3 _color, float _offset)
{
  if (_offset == 0.0)
  {
    return _color;
  }
  const float3 hsl = RgbToHsl(_color);
  return HslToRgb(float3(hsl.xy, hsl.z + _offset));
}

// A direction or a point in a cell, turned back from the world into the batch's own frame: the
// inverse of TurnedCell's quarter turns, clockwise seen from above, +z towards +x.
float3 UnturnedDirection(float3 _world, uint _quarterTurns)
{
  if (_quarterTurns == 1u)
  {
    return float3(-_world.z, _world.y, _world.x);
  }
  if (_quarterTurns == 2u)
  {
    return float3(-_world.x, _world.y, -_world.z);
  }
  if (_quarterTurns == 3u)
  {
    return float3(_world.z, _world.y, -_world.x);
  }
  return _world;
}

float3 UnturnedFraction(float3 _world, uint _quarterTurns)
{
  if (_quarterTurns == 1u)
  {
    return float3(1.0 - _world.z, _world.y, _world.x);
  }
  if (_quarterTurns == 2u)
  {
    return float3(1.0 - _world.x, _world.y, 1.0 - _world.z);
  }
  if (_quarterTurns == 3u)
  {
    return float3(_world.z, _world.y, 1.0 - _world.x);
  }
  return _world;
}

// The corner shade at a point on a face (RendererLook.md §5): the face's four corner levels, read
// through the record's table and blended bilinearly. Faces are numbered as VoxelRecord.h's
// VoxelFace, -x, +x, +y, -z, +z; a corner is u + 2w, along the face's two other axes in x, y, z
// order. The bottom face holds no levels and is never seen, so it shades as open.
float CornerShade(float3 _localNormal, float3 _localFraction, uint2 _cornerLevels, uint _shadeTable)
{
  const float3 axis = abs(_localNormal);
  uint face;
  float2 uw;
  if (axis.x >= axis.y && axis.x >= axis.z)
  {
    face = _localNormal.x < 0.0 ? 0u : 1u;
    uw = _localFraction.yz;
  }
  else if (axis.y >= axis.z)
  {
    if (_localNormal.y < 0.0)
    {
      return 1.0;
    }
    face = 2u;
    uw = _localFraction.xz;
  }
  else
  {
    face = _localNormal.z < 0.0 ? 3u : 4u;
    uw = _localFraction.xy;
  }
  const uint levels = face < 4u ? (_cornerLevels.x >> (8u * face)) & 255u : _cornerLevels.y & 255u;
  const float4 table = g_frame.cornerShades[_shadeTable];
  const float corner0 = table[levels & 3u];
  const float corner1 = table[(levels >> 2) & 3u];
  const float corner2 = table[(levels >> 4) & 3u];
  const float corner3 = table[(levels >> 6) & 3u];
  return lerp(lerp(corner0, corner1, uw.x), lerp(corner2, corner3, uw.x), uw.y);
}

// A cell turned by the batch's quarter turns, clockwise seen from above, so that one turns +z
// towards +x, about the batch's footprint: the turned footprint keeps its minimum corner at the
// origin. VoxelPass.h's Batch states the same.
uint3 TurnedCell(uint3 _cell, uint _turn)
{
  const uint quarterTurns = _turn & 3u;
  const uint lastX = (_turn >> 2) & 63u;
  const uint lastZ = (_turn >> 8) & 63u;
  if (quarterTurns == 1u)
  {
    return uint3(_cell.z, _cell.y, lastX - _cell.x);
  }
  if (quarterTurns == 2u)
  {
    return uint3(lastX - _cell.x, _cell.y, lastZ - _cell.z);
  }
  if (quarterTurns == 3u)
  {
    return uint3(lastZ - _cell.z, _cell.y, _cell.x);
  }
  return _cell;
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

// The orthographic ray through this pixel, from the near plane along the frame's forward, against
// the voxel's box: the camera's in the voxel pass, the sun's in the shadow passes. Returns whether
// it hits, and the hit and the outward normal of the face it hits.
bool CastRay(VoxelInterpolants _input, out float3 _hit, out float3 _normal)
{
  const float2 ndc = float2(_input.position.x * g_frame.viewportPixels.z * 2.0 - 1.0, 1.0 - _input.position.y * g_frame.viewportPixels.w * 2.0);
  const float4 nearPoint = mul(float4(ndc, 1.0, 1.0), g_frame.inverseViewProjection);
  const float3 rayOrigin = nearPoint.xyz / nearPoint.w;
  const float3 rayDirection = g_frame.rayDirection.xyz;
  float hitDistance;
  const bool hits = IntersectBox(_input.boxCenter, float3(_input.boxRadius, _input.boxRadius, _input.boxRadius), rayOrigin, rayDirection,
                                 1.0 / rayDirection, hitDistance, _normal);
  _hit = rayOrigin + rayDirection * hitDistance;
  return hits;
}

#endif
