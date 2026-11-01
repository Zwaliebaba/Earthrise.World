#include "pch.h"

#include "TestWorld.h"

namespace Simulation
{

namespace
{

// The v5 scene's frame (Design/Reference/SiegePrototypeV2/reference, "Earthrise Siege Scene v5"),
// in its own 128 m square: the castle's ground at 15 m, and the castle's middle at (74, 61).
constexpr std::int32_t SCENE = TestWorld::SCENE_EDGE_VOXELS;
constexpr std::int32_t HEIGHT = TestWorld::HEIGHT_VOXELS;
constexpr std::int32_t G = TestWorld::CASTLE_GROUND_VOXELS;
constexpr std::int32_t CASTLE_X_VOXELS = 74;
constexpr std::int32_t CASTLE_Z_VOXELS = 61;

// Palette indices (GameData/Server/Palette.json).
constexpr std::uint8_t GRASS = 1;
constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t COMMON_ROCK = 3;
constexpr std::uint8_t BUILDING_STONE = 4;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t TIMBER = 7;
constexpr std::uint8_t STONE_RUBBLE = 8;
constexpr std::uint8_t WATER = 9;
constexpr std::uint8_t LEAVES_A = 10;
constexpr std::uint8_t LEAVES_B = 11;
constexpr std::uint8_t ROOF = 12;
constexpr std::uint8_t FIELD_A = 13;
constexpr std::uint8_t FIELD_B = 14;
constexpr std::uint8_t ROAD = 15;
constexpr std::uint8_t OPENING = 20;

// The section of the west curtain that collapses: the one just north of the breach.
constexpr std::int32_t COLLAPSE_X_VOXELS = 58;
constexpr std::int32_t COLLAPSE_Z_VOXELS = 66;
// Quarrying digs no lower than this, so a quarry keeps a floor above the bedrock.
constexpr std::int32_t QUARRY_FLOOR_VOXELS = 4;
// The collapse's rubble heap is at most this high above the castle's ground.
constexpr std::int32_t HEAP_HEIGHT_VOXELS = 4;

// ── A seeded integer height field ─────────────────────────────────────────────────────────────
// Fixed point with 16 fraction bits, from one hash, so the world is the same on every machine
// (GameDesign.md, *Starting the MVP*, item 5). It stands in for the v5 scene's floating-point noise
// and is matched by eye.

constexpr std::int64_t ONE = std::int64_t{1} << 16;

// SplitMix64's finalizer.
constexpr std::uint64_t Mix(std::uint64_t _value) noexcept
{
  _value += 0x9e3779b97f4a7c15ULL;
  _value = (_value ^ (_value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  _value = (_value ^ (_value >> 27)) * 0x94d049bb133111ebULL;
  return _value ^ (_value >> 31);
}

/// A hash of two integers and a salt, from 0 up to ONE.
std::int64_t Hash(std::int64_t _x, std::int64_t _z, std::uint64_t _salt, std::uint64_t _seed) noexcept
{
  const std::uint64_t key = static_cast<std::uint64_t>(_x) ^ (static_cast<std::uint64_t>(_z) << 32);
  return static_cast<std::int64_t>(Mix(_seed ^ Mix(_salt ^ Mix(key))) & static_cast<std::uint64_t>(ONE - 1));
}

std::int64_t Smooth(std::int64_t _t) noexcept
{
  return _t * _t / ONE * (3 * ONE - 2 * _t) / ONE;
}

/// 0 below _from and 1 beyond _to, smoothly between; _from may be the larger.
std::int64_t SmoothStep(std::int64_t _from, std::int64_t _to, std::int64_t _value) noexcept
{
  return Smooth(std::clamp((_value - _from) * ONE / (_to - _from), std::int64_t{0}, ONE));
}

/// Value noise at a point in fixed point, from 0 up to ONE.
std::int64_t ValueNoise(std::int64_t _x, std::int64_t _z, std::uint64_t _salt, std::uint64_t _seed) noexcept
{
  const std::int64_t cellX = _x >> 16; // an arithmetic shift, so it floors
  const std::int64_t cellZ = _z >> 16;
  const std::int64_t u = Smooth(_x & (ONE - 1));
  const std::int64_t w = Smooth(_z & (ONE - 1));
  const std::int64_t a = Hash(cellX, cellZ, _salt, _seed);
  const std::int64_t b = Hash(cellX + 1, cellZ, _salt, _seed);
  const std::int64_t c = Hash(cellX, cellZ + 1, _salt, _seed);
  const std::int64_t d = Hash(cellX + 1, cellZ + 1, _salt, _seed);
  return a + (b - a) * u / ONE + (c - a) * w / ONE + (a - b - c + d) * u / ONE * w / ONE;
}

/// Three octaves, weighted as the v5 scene weights them.
std::int64_t Fbm(std::int64_t _x, std::int64_t _z, std::uint64_t _salt, std::uint64_t _seed) noexcept
{
  return (55 * ValueNoise(_x, _z, _salt, _seed) + 30 * ValueNoise(2 * _x, 2 * _z, _salt + 7, _seed) +
          15 * ValueNoise(4 * _x, 4 * _z, _salt + 13, _seed)) /
         100;
}

/// A voxel coordinate scaled by _thousandths, in fixed point: the v5 scene's noise frequencies.
constexpr std::int64_t Scaled(std::int32_t _voxels, std::int64_t _thousandths) noexcept
{
  return static_cast<std::int64_t>(_voxels) * ONE * _thousandths / 1000;
}

constexpr std::int32_t Round(std::int64_t _fixed) noexcept
{
  return static_cast<std::int32_t>((_fixed + ONE / 2) >> 16);
}

std::int64_t IntegerSqrt(std::int64_t _value) noexcept
{
  if (_value <= 0)
  {
    return 0;
  }
  // A float guess, corrected to the exact integer root, so the result does not depend on the FPU.
  auto root = static_cast<std::int64_t>(std::sqrt(static_cast<double>(_value)));
  while (root * root > _value)
  {
    --root;
  }
  while ((root + 1) * (root + 1) <= _value)
  {
    ++root;
  }
  return root;
}

/// The length of a fixed-point vector, in fixed point.
std::int64_t Length(std::int64_t _x, std::int64_t _z) noexcept
{
  return IntegerSqrt(_x * _x + _z * _z);
}

/// The river's middle at row _z of the v5 scene: its gentle curve, with a triangle wave of the
/// same period for its sine.
std::int64_t RiverX(std::int32_t _z) noexcept
{
  constexpr std::int32_t PERIOD_VOXELS = 105;
  const std::int32_t wrapped = ((_z + PERIOD_VOXELS / 4) % PERIOD_VOXELS + PERIOD_VOXELS) % PERIOD_VOXELS;
  const std::int64_t phase = wrapped * ONE / PERIOD_VOXELS;
  const std::int64_t triangle = std::abs(4 * phase - 2 * ONE) - ONE;
  return 12 * ONE + static_cast<std::int64_t>(_z) * ONE * 12 / 100 + 5 * triangle;
}

// ── Writing into the store ────────────────────────────────────────────────────────────────────

/// Where one copy of the v5 scene stands in a world: its 128 m square's corner. It draws in the
/// scene's own coordinates, and counts what it changes.
class Site
{
public:
  Site(ChunkStore& _store, const TestWorld& _world, std::int32_t _originX, std::int32_t _originZ, std::uint64_t& _changed)
    : m_store(_store),
      m_world(_world),
      m_originX(_originX),
      m_originZ(_originZ),
      m_changed(_changed)
  {
  }

  void Set(std::int32_t _x, std::int32_t _y, std::int32_t _z, std::uint8_t _index) const
  {
    const std::int32_t x = _x + m_originX;
    const std::int32_t z = _z + m_originZ;
    if (x >= 0 && x < m_world.edgeVoxels && z >= 0 && z < m_world.edgeVoxels && _y >= 0 && _y < HEIGHT)
    {
      const Protocol::VoxelPosition position{.xVoxels = x, .yVoxels = _y, .zVoxels = z};
      if (m_store.At(position) != _index)
      {
        m_store.Set(position, _index);
        ++m_changed;
      }
    }
  }

  [[nodiscard]] std::uint8_t At(std::int32_t _x, std::int32_t _y, std::int32_t _z) const
  {
    const std::int32_t x = _x + m_originX;
    const std::int32_t z = _z + m_originZ;
    if (x < 0 || x >= m_world.edgeVoxels || z < 0 || z >= m_world.edgeVoxels || _y < 0 || _y >= HEIGHT)
    {
      return 0;
    }
    return m_store.At({.xVoxels = x, .yVoxels = _y, .zVoxels = z});
  }

  /// Fills the box from the first corner to the second, both included.
  void Box(std::int32_t _x0, std::int32_t _x1, std::int32_t _y0, std::int32_t _y1, std::int32_t _z0, std::int32_t _z1,
           std::uint8_t _index) const
  {
    for (std::int32_t y = _y0; y <= _y1; ++y)
    {
      for (std::int32_t z = _z0; z <= _z1; ++z)
      {
        for (std::int32_t x = _x0; x <= _x1; ++x)
        {
          Set(x, y, z, _index);
        }
      }
    }
  }

  /// The generated surface under a column of the scene.
  [[nodiscard]] std::int32_t Height(std::int32_t _x, std::int32_t _z) const
  {
    return m_world.SurfaceAt(_x + m_originX, _z + m_originZ);
  }

private:
  ChunkStore& m_store;
  const TestWorld& m_world;
  std::int32_t m_originX;
  std::int32_t m_originZ;
  std::uint64_t& m_changed;
};

TemplateShape Template(const TemplateLookup& _templates, std::string_view _id)
{
  const std::optional<TemplateShape> found = _templates ? _templates(_id) : std::nullopt;
  if (!found)
  {
    throw std::runtime_error("the test world needs a template the lookup does not have");
  }
  return *found;
}

/// A cell of a footprint _width by _depth cells turned by _quarterTurns, clockwise seen from above,
/// about the footprint, as the voxel pass turns a model (VoxelPass.h, Batch).
std::pair<std::int32_t, std::int32_t> Turned(std::int32_t _x, std::int32_t _z, std::int32_t _width, std::int32_t _depth,
                                             std::uint32_t _quarterTurns) noexcept
{
  switch (_quarterTurns % 4)
  {
  case 1:
    return {_z, _width - 1 - _x};
  case 2:
    return {_width - 1 - _x, _depth - 1 - _z};
  case 3:
    return {_depth - 1 - _z, _x};
  default:
    return {_x, _z};
  }
}

/// Stamps a template with its turned footprint's minimum corner at the voxel given. Its front, +z
/// as authored, faces +z, +x, -z or -x after none to three quarter turns.
void Place(const Site& _site, const TemplateShape& _shape, std::int32_t _x, std::int32_t _y, std::int32_t _z, std::uint32_t _quarterTurns)
{
  const auto width = static_cast<std::int32_t>(_shape.widthVoxels);
  const auto depth = static_cast<std::int32_t>(_shape.depthVoxels);
  for (std::uint32_t y = 0; y < _shape.heightVoxels; ++y)
  {
    for (std::int32_t z = 0; z < depth; ++z)
    {
      for (std::int32_t x = 0; x < width; ++x)
      {
        const std::uint8_t index = _shape.At(static_cast<std::uint32_t>(x), y, static_cast<std::uint32_t>(z));
        if (index != 0)
        {
          const auto [turnedX, turnedZ] = Turned(x, z, width, depth, _quarterTurns);
          _site.Set(_x + turnedX, _y + static_cast<std::int32_t>(y), _z + turnedZ, index);
        }
      }
    }
  }
}

// ── The scene's parts, in the order the v5 scene builds them ─────────────────────────────────

/// Where a castle's scene square starts, in world voxels.
struct SiteOrigin
{
  std::int32_t xVoxels;
  std::int32_t zVoxels;
};

/// The ground over the whole world: a gentle field, cut by the v5 scene's river, in layers of
/// bedrock, rock, earth and grass, and raised to the v5 scene's plateau under the castle of each of
/// _sites. Fills the world's surface.
void BuildGround(ChunkStore& _store, TestWorld& _world, std::uint64_t _seed, std::span<const SiteOrigin> _sites)
{
  constexpr std::int32_t PLATEAU_EDGE_VOXELS = 42; // where the plateau meets the field
  constexpr std::int32_t PLATEAU_TOP_VOXELS = 27;  // the flat top's radius
  const std::int32_t edge = _world.edgeVoxels;
  const auto column = [edge](std::int32_t _x, std::int32_t _z)
  { return static_cast<std::size_t>(_z) * static_cast<std::size_t>(edge) + static_cast<std::size_t>(_x); };

  // The field, in the scene's own coordinates, so that the scene is the same wherever it stands.
  std::vector<std::int64_t> heights(static_cast<std::size_t>(edge) * static_cast<std::size_t>(edge));
  for (std::int32_t worldZ = 0; worldZ < edge; ++worldZ)
  {
    for (std::int32_t worldX = 0; worldX < edge; ++worldX)
    {
      const std::int32_t x = worldX - _world.sceneXVoxels;
      const std::int32_t z = worldZ - _world.sceneZVoxels;
      heights[column(worldX, worldZ)] = 6 * ONE + 6 * Fbm(Scaled(x, 35), Scaled(z, 35), 1, _seed);
    }
  }
  // Each castle's plateau. Beyond its edge the blend is nothing, so only the box about it is touched.
  for (const SiteOrigin& site : _sites)
  {
    const std::int32_t castleX = site.xVoxels + CASTLE_X_VOXELS;
    const std::int32_t castleZ = site.zVoxels + CASTLE_Z_VOXELS;
    for (std::int32_t worldZ = std::max(0, castleZ - PLATEAU_EDGE_VOXELS); worldZ <= std::min(edge - 1, castleZ + PLATEAU_EDGE_VOXELS);
         ++worldZ)
    {
      for (std::int32_t worldX = std::max(0, castleX - PLATEAU_EDGE_VOXELS); worldX <= std::min(edge - 1, castleX + PLATEAU_EDGE_VOXELS);
           ++worldX)
      {
        const std::int64_t fromCastle = Length((worldX - castleX) * ONE, (worldZ - castleZ) * ONE);
        std::int64_t& height = heights[column(worldX, worldZ)];
        height += (G * ONE - height) * SmoothStep(PLATEAU_EDGE_VOXELS * ONE, PLATEAU_TOP_VOXELS * ONE, fromCastle) / ONE;
      }
    }
  }
  _world.surfaceVoxels.clear();
  _world.surfaceVoxels.reserve(heights.size());
  for (std::int32_t worldZ = 0; worldZ < edge; ++worldZ)
  {
    for (std::int32_t worldX = 0; worldX < edge; ++worldX)
    {
      const std::int32_t x = worldX - _world.sceneXVoxels;
      const std::int32_t z = worldZ - _world.sceneZVoxels;
      std::int64_t height = heights[column(worldX, worldZ)];
      const std::int64_t fromRiver = std::abs(x * ONE - RiverX(z));
      if (fromRiver < 9 * ONE)
      {
        height = std::min(height, std::max(2 * ONE, 2 * ONE + (fromRiver - 4 * ONE) * 11 / 10));
      }
      const std::int32_t top = Round(height);
      _world.surfaceVoxels.push_back(top);

      const std::int32_t earth =
        std::max(1, Round(ONE + Fbm(Scaled(x, 80), Scaled(z, 80), 5, _seed) * 5 - (x > CASTLE_X_VOXELS ? 2 * ONE : 0)));
      const std::int32_t bedrock = 1 + Round(Fbm(Scaled(x, 50), Scaled(z, 50), 9, _seed) * 3);
      for (std::int32_t y = 0; y < top; ++y)
      {
        const std::uint8_t layer = y < bedrock ? BEDROCK : (y >= top - earth ? (y == top - 1 ? GRASS : EARTH) : COMMON_ROCK);
        _store.Set({.xVoxels = worldX, .yVoxels = y, .zVoxels = worldZ}, layer);
      }
      if (fromRiver < 9 * ONE / 2)
      {
        for (std::int32_t y = top; y < 4; ++y)
        {
          _store.Set({.xVoxels = worldX, .yVoxels = y, .zVoxels = worldZ}, WATER);
        }
      }
    }
  }
}

/// The body of building stone under the ground north of the castle, and whether a column of it is
/// within the body: the quarry's stone and where it is dug.
bool InQuarryBody(std::int32_t _x, std::int32_t _z) noexcept
{
  return Length((_x - 74) * ONE * 2 / 3, (_z - 93) * ONE) <= 17 * ONE;
}

/// The terraced quarry: a body of building stone under the ground north of the castle, a pit cut in
/// steps of 2 m with a ramp down its south side, and the quarry yard beside it.
void BuildQuarry(const Site& _site, const TemplateLookup& _templates, std::uint64_t _seed)
{
  for (std::int32_t z = 76; z < 118; ++z)
  {
    for (std::int32_t x = 46; x < 104; ++x)
    {
      const std::int64_t reach = Length((x - 74) * ONE * 2 / 3, (z - 93) * ONE) + Fbm(Scaled(x, 200), Scaled(z, 200), 31, _seed) * 4;
      if (reach > 17 * ONE)
      {
        continue;
      }
      const std::int32_t top = _site.Height(x, z) - (reach < 8 * ONE ? 0 : 1);
      for (std::int32_t y = 3; y < top; ++y)
      {
        _site.Set(x, y, z, BUILDING_STONE);
      }
    }
  }
  for (std::int32_t z = 85; z < 103; ++z)
  {
    for (std::int32_t x = 61; x < 89; ++x)
    {
      const std::int32_t inset = std::min({x - 61, 88 - x, z - 85, 102 - z});
      std::int32_t floor = 13 - std::min(inset / 2, 4) * 2;
      if (z < 87 && x >= 63)
      {
        floor = std::max(floor, 13 - std::min(8, (x - 63) / 2)); // the ramp
      }
      _site.Box(x, x, floor, HEIGHT - 1, z, z, 0);
    }
  }
  // The quarry yard, T8, beside the pit, facing it. The template wins over the v5 scene's loose
  // stacks of stone, and holds its stock. Its footprint is levelled on its highest column.
  const TemplateShape yard = Template(_templates, "T8");
  constexpr std::int32_t YARD_X = 92;
  constexpr std::int32_t YARD_Z = 88;
  const auto yardWidth = static_cast<std::int32_t>(yard.depthVoxels); // turned a quarter
  const auto yardDepth = static_cast<std::int32_t>(yard.widthVoxels);
  std::int32_t yardBase = 0;
  for (std::int32_t z = YARD_Z; z < YARD_Z + yardDepth; ++z)
  {
    for (std::int32_t x = YARD_X; x < YARD_X + yardWidth; ++x)
    {
      yardBase = std::max(yardBase, _site.Height(x, z));
    }
  }
  for (std::int32_t z = YARD_Z; z < YARD_Z + yardDepth; ++z)
  {
    for (std::int32_t x = YARD_X; x < YARD_X + yardWidth; ++x)
    {
      _site.Box(x, x, _site.Height(x, z), yardBase - 1, z, z, EARTH);
    }
  }
  Place(_site, yard, YARD_X, yardBase, YARD_Z, 3);
}

/// The road from the gate to the scene's east edge, and the fields to the north-east.
void BuildRoadAndFields(const Site& _site, std::uint64_t _seed)
{
  for (std::int32_t x = 93; x < SCENE; ++x)
  {
    for (std::int32_t z = 59; z < 64; ++z)
    {
      const std::int32_t top = _site.Height(x, z) - 1;
      if (_site.At(x, top, z) == GRASS)
      {
        _site.Set(x, top, z, ROAD);
      }
    }
  }
  for (std::int32_t z = 88; z < 124; ++z)
  {
    for (std::int32_t x = 104; x < 124; ++x)
    {
      const std::int32_t top = _site.Height(x, z) - 1;
      if (_site.At(x, top, z) == GRASS && Fbm(Scaled(x, 50), Scaled(z, 50), 41, _seed) > ONE * 35 / 100)
      {
        _site.Set(x, top, z, (z / 3) % 2 != 0 ? FIELD_A : FIELD_B);
      }
    }
  }
}

/// The ditch and its rampart before the west wall, open where the breach's approach crosses them.
/// The rampart is fresh, so bare earth: only the generator places grass (RendererLook.md).
void BuildDitchAndRampart(const Site& _site)
{
  for (std::int32_t z = 34; z < 89; ++z)
  {
    if (z >= 54 && z <= 68)
    {
      continue;
    }
    for (std::int32_t x = 46; x < 57; ++x)
    {
      const std::int32_t ground = _site.Height(x, z);
      // Distances from the ditch's and the rampart's middles, in half meters: 50.5 m and 54.5 m.
      const std::int32_t fromDitch = std::abs(2 * x + 1 - 101);
      const std::int32_t ditch = std::clamp((5 - fromDitch + 1) / 2, 0, 2);
      if (ditch > 0)
      {
        _site.Box(x, x, ground - ditch, ground - 1, z, z, 0);
        continue;
      }
      const std::int32_t fromRampart = std::abs(2 * x + 1 - 109);
      const std::int32_t rampart = std::clamp((4 - fromRampart + 1) / 2, 0, 2);
      _site.Box(x, x, ground, ground + rampart - 1, z, z, EARTH);
    }
  }
}

/// A cottage, which GameData lacks, drawn as the v5 scene draws it: timber walls 3 m high with a
/// door, and a stepped roof.
void BuildCottage(const Site& _site, std::int32_t _x, std::int32_t _z, std::int32_t _width, std::int32_t _depth)
{
  _site.Box(_x, _x + _width - 1, G, G + 2, _z, _z + _depth - 1, TIMBER);
  _site.Box(_x + _width / 2, _x + _width / 2, G, G + 1, _z, _z, OPENING);
  _site.Box(_x - 1, _x + _width, G + 3, G + 3, _z - 1, _z + _depth, ROOF);
  _site.Box(_x - 1, _x + _width, G + 4, G + 4, _z, _z + _depth - 1, ROOF);
  _site.Box(_x - 1, _x + _width, G + 5, G + 5, _z + 1, _z + _depth - 2, ROOF);
}

/// The castle from the templates, which win over the v5 scene's sizes: T4 towers at the corners,
/// three T3 sections between each pair, the T5 gatehouse in the east wall facing the road, the T2
/// keep with its forebuilding towards the gate, and the v5 scene's cottages inside.
void BuildCastle(const Site& _site, const TemplateLookup& _templates)
{
  const TemplateShape wall = Template(_templates, "T3");
  const TemplateShape tower = Template(_templates, "T4");
  const TemplateShape gatehouse = Template(_templates, "T5");
  const TemplateShape keep = Template(_templates, "T2");

  for (const auto& [x, z] : {std::pair{56, 43}, std::pair{87, 43}, std::pair{56, 74}, std::pair{87, 74}})
  {
    Place(_site, tower, x, G, z, 0);
  }
  for (const std::int32_t z : {50, 58, 66})
  {
    Place(_site, wall, 58, G, z, 3); // west, its front facing -x
  }
  for (const std::int32_t x : {63, 71, 79})
  {
    Place(_site, wall, x, G, 76, 0); // north
    Place(_site, wall, x, G, 45, 2); // south
  }
  // East: a section each side of the gatehouse. The southern one overlaps the tower by a meter,
  // because the gatehouse leaves 15 m for two 8 m sections.
  Place(_site, wall, 89, G, 49, 1);
  Place(_site, wall, 89, G, 66, 1);
  Place(_site, gatehouse, 87, G, 57, 1);
  Place(_site, keep, 68, G, 55, 1);

  BuildCottage(_site, 62, 50, 5, 4);
  BuildCottage(_site, 62, 69, 5, 4);
  BuildCottage(_site, 82, 50, 5, 4);
  BuildCottage(_site, 82, 70, 5, 4);
  BuildCottage(_site, 70, 70, 5, 4);
}

/// The breach in the west wall, widening as it rises, its craters, and the rubble ramp at its foot.
void BuildBreach(const Site& _site, std::uint64_t _seed)
{
  for (std::int32_t y = G - 1; y <= G + 8; ++y)
  {
    for (std::int32_t z = 50; z < 72; ++z)
    {
      // Its half-width: 1.5 m at the foot, 0.62 m more each meter up, and a ragged edge.
      const std::int64_t halfWidth =
        3 * ONE / 2 + std::max(0, y - G) * ONE * 62 / 100 + (Fbm(Scaled(z, 500), Scaled(y, 500), 77, _seed) - ONE / 2) * 12 / 10;
      if (std::abs((2 * z + 1) * ONE / 2 - 61 * ONE) < halfWidth)
      {
        _site.Box(58, 60, y, y, z, z, 0);
      }
    }
  }
  // Craters from the engines: masonry within each sphere goes.
  struct Crater
  {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;
    std::int64_t radius;
  };
  for (const Crater& crater : {Crater{.x = 576, .y = 186, .z = 705, .radius = 16}, Crater{.x = 556, .y = 225, .z = 470, .radius = 15},
                               Crater{.x = 577, .y = 202, .z = 515, .radius = 13}})
  {
    // In tenths of a meter.
    for (std::int64_t y = (crater.y - crater.radius) / 10; y <= (crater.y + crater.radius) / 10; ++y)
    {
      for (std::int64_t z = (crater.z - crater.radius) / 10; z <= (crater.z + crater.radius) / 10; ++z)
      {
        for (std::int64_t x = (crater.x - crater.radius) / 10; x <= (crater.x + crater.radius) / 10; ++x)
        {
          const std::int64_t dx = 10 * x + 5 - crater.x;
          const std::int64_t dy = 10 * y + 5 - crater.y;
          const std::int64_t dz = 10 * z + 5 - crater.z;
          const auto vx = static_cast<std::int32_t>(x);
          const auto vy = static_cast<std::int32_t>(y);
          const auto vz = static_cast<std::int32_t>(z);
          if (dx * dx + dy * dy + dz * dz < crater.radius * crater.radius && _site.At(vx, vy, vz) == MASONRY)
          {
            _site.Set(vx, vy, vz, 0);
          }
        }
      }
    }
  }
  // The rubble ramp: up to 3 m high in an ellipse 17 m by 11 m about the breach's foot, mostly stone
  // rubble with blocks of masonry in it.
  for (std::int32_t z = 52; z < 70; ++z)
  {
    for (std::int32_t x = 48; x < 68; ++x)
    {
      const std::int64_t reach = Length(((2 * x + 1) * ONE / 2 - 59 * ONE) * 2 / 17, ((2 * z + 1) * ONE / 2 - 61 * ONE) * 10 / 56);
      if (reach >= ONE)
      {
        continue;
      }
      const std::int32_t height =
        std::max(0, Round(3 * (ONE - reach) + (Fbm(Scaled(x, 700), Scaled(z, 700), 51, _seed) - ONE / 2) * 8 / 10));
      const std::int32_t ground = _site.Height(x, z);
      for (std::int32_t k = 0; k < height; ++k)
      {
        if (_site.At(x, ground + k, z) == 0)
        {
          _site.Set(x, ground + k, z, Hash(x, z * 7 + k, 52, _seed) < ONE * 12 / 100 ? MASONRY : STONE_RUBBLE);
        }
      }
    }
  }
}

/// A tree: a timber trunk 2 to 4 m high under a crown of leaves 1.3 to 2.2 m in radius.
void BuildTree(const Site& _site, std::int32_t _x, std::int32_t _z, std::int32_t _ground, std::uint64_t _seed)
{
  const std::int32_t trunk = 2 + static_cast<std::int32_t>(Hash(_x, _z, 4, _seed) * 3 / ONE);
  const std::uint8_t leaves = Hash(_x, _z, 5, _seed) < ONE / 2 ? LEAVES_A : LEAVES_B;
  // The crown's radius, in tenths of a meter, about a point above the trunk.
  const std::int64_t radius = 13 + Hash(_x, _z, 6, _seed) * 9 / ONE;
  const std::int32_t crown = _ground + trunk + 1;
  _site.Box(_x, _x, _ground, _ground + trunk - 1, _z, _z, TIMBER);
  for (std::int32_t dy = -2; dy <= 2; ++dy)
  {
    for (std::int32_t dz = -2; dz <= 2; ++dz)
    {
      for (std::int32_t dx = -2; dx <= 2; ++dx)
      {
        // Flattened a little: a meter up counts 1.2 m.
        const std::int64_t reach = 100 * (dx * dx + dz * dz) + 144 * dy * dy;
        if (reach <= radius * radius && _site.At(_x + dx, crown + dy, _z + dz) == 0)
        {
          _site.Set(_x + dx, crown + dy, _z + dz, leaves);
        }
      }
    }
  }
}

/// The forest, in 1 m world voxels as ADR-006 puts trees. In the v5 scene's square: west and south
/// of the castle and along the north-west, clear of the castle, the river, the fields and the
/// attackers' camp. Beyond the square, wherever the same noise allows, clear of the river.
void BuildForest(const Site& _site, std::int32_t _edgeVoxels, std::int32_t _sceneX, std::int32_t _sceneZ, std::uint64_t _seed)
{
  for (std::int32_t z = 2 - _sceneZ; z < _edgeVoxels - 2 - _sceneZ; ++z)
  {
    for (std::int32_t x = 2 - _sceneX; x < _edgeVoxels - 2 - _sceneX; ++x)
    {
      const bool inScene = x >= 2 && x < 120 && z >= 2 && z < 122;
      const bool beyondScene = x < 0 || x >= SCENE || z < 0 || z >= SCENE;
      if (!inScene && !beyondScene)
      {
        continue; // the scene square's own margin
      }
      const std::int64_t fromRiver = std::abs(x * ONE - RiverX(z));
      if (fromRiver < 7 * ONE)
      {
        continue;
      }
      if (inScene)
      {
        const std::int64_t fromCastle = Length((x - CASTLE_X_VOXELS) * ONE, (z - CASTLE_Z_VOXELS) * ONE);
        // The v5 scene's clearings, and the attackers' camp, where the trebuchets, the mantlets and
        // the reserves stand.
        const bool camp = x >= 24 && x < 48 && z >= 44 && z < 80;
        if (fromCastle < 31 * ONE || (x > 90 && z > 84) || camp || !(x + z < 100 || (z > 92 && x < 60)))
        {
          continue;
        }
      }
      if (Fbm(Scaled(x, 60), Scaled(z, 60), 21, _seed) < ONE / 2 || Hash(x, z, 3, _seed) > ONE * 9 / 100)
      {
        continue;
      }
      const std::int32_t ground = _site.Height(x, z);
      if (_site.At(x, ground - 1, z) != GRASS)
      {
        continue;
      }
      if (Hash(x, z, 8, _seed) < ONE / 5 && x + z > 70)
      {
        _site.Set(x, ground, z, TIMBER); // a stump
        continue;
      }
      BuildTree(_site, x, z, ground, _seed);
    }
  }
}

/// Digs _voxels out of the quarry's stone body, the highest ground first, down to its floor.
void DigQuarry(const Site& _site, std::uint64_t _voxels)
{
  std::vector<std::pair<std::int32_t, std::int32_t>> columns;
  for (std::int32_t z = 82; z < 118; ++z)
  {
    for (std::int32_t x = 46; x < 104; ++x)
    {
      if (InQuarryBody(x, z))
      {
        columns.emplace_back(x, z);
      }
    }
  }
  std::uint64_t dug = 0;
  for (std::int32_t y = HEIGHT - 1; y >= QUARRY_FLOOR_VOXELS && dug < _voxels; --y)
  {
    for (const auto& [x, z] : columns)
    {
      if (dug == _voxels)
      {
        break;
      }
      if (_site.At(x, y, z) != 0)
      {
        _site.Set(x, y, z, 0);
        ++dug;
      }
    }
  }
}

} // namespace

std::int32_t TestWorld::SurfaceAt(std::int32_t _x, std::int32_t _z) const
{
  const std::int32_t x = std::clamp(_x, 0, edgeVoxels - 1);
  const std::int32_t z = std::clamp(_z, 0, edgeVoxels - 1);
  return surfaceVoxels[static_cast<std::size_t>(z) * static_cast<std::size_t>(edgeVoxels) + static_cast<std::size_t>(x)];
}

float TestWorld::CastleXMeters() const noexcept
{
  return static_cast<float>(sceneXVoxels + CASTLE_X_VOXELS);
}

float TestWorld::CastleZMeters() const noexcept
{
  return static_cast<float>(sceneZVoxels + CASTLE_Z_VOXELS);
}

TestWorld BuildTestWorld(ChunkStore& _store, std::int32_t _edgeVoxels, std::uint64_t _seed, const TemplateLookup& _templates)
{
  if (_edgeVoxels < SCENE || _edgeVoxels % Protocol::CHUNK_EDGE_VOXELS != 0)
  {
    throw std::invalid_argument("a test world is at least 128 m across and a whole number of chunks");
  }
  TestWorld world{.edgeVoxels = _edgeVoxels,
                  .sceneXVoxels = (_edgeVoxels - SCENE) / 2,
                  .sceneZVoxels = (_edgeVoxels - SCENE) / 2,
                  .surfaceVoxels = {},
                  .editedVoxels = 0};
  const std::array<SiteOrigin, 1> scene{{{.xVoxels = world.sceneXVoxels, .zVoxels = world.sceneZVoxels}}};
  BuildGround(_store, world, _seed, scene);
  std::uint64_t generated = 0;
  const Site generator(_store, world, world.sceneXVoxels, world.sceneZVoxels, generated);
  const Site edits(_store, world, world.sceneXVoxels, world.sceneZVoxels, world.editedVoxels);
  BuildQuarry(edits, _templates, _seed);
  BuildRoadAndFields(generator, _seed);
  BuildDitchAndRampart(edits);
  BuildCastle(edits, _templates);
  BuildBreach(edits, _seed);
  BuildForest(generator, _edgeVoxels, world.sceneXVoxels, world.sceneZVoxels, _seed);
  return world;
}

TestWorld BuildSeasonWorld(ChunkStore& _store, std::int32_t _edgeVoxels, std::int32_t _holdings, std::uint64_t _seed,
                           const TemplateLookup& _templates)
{
  constexpr std::int32_t HOLDINGS_ACROSS = 8;
  constexpr std::size_t BREACHED_HOLDINGS = 12; // a dozen sieges
  constexpr std::int32_t SECOND_SITE_VOXELS = 192;
  if (_edgeVoxels % Protocol::CHUNK_EDGE_VOXELS != 0 || _holdings <= 0 || _holdings % HOLDINGS_ACROSS != 0)
  {
    throw std::invalid_argument("a season world is a whole number of chunks, with holdings in rows of 8");
  }
  const std::int32_t rows = _holdings / HOLDINGS_ACROSS;
  const std::int32_t spacingX = _edgeVoxels / HOLDINGS_ACROSS;
  const std::int32_t spacingZ = _edgeVoxels / rows;
  if (spacingX < SCENE || spacingZ < SCENE + SECOND_SITE_VOXELS)
  {
    throw std::invalid_argument("a season world must have room for each holding's two sites");
  }
  // The first holding's first site stands as the scene, so the bench's breach is made there.
  TestWorld world{.edgeVoxels = _edgeVoxels,
                  .sceneXVoxels = (spacingX - SCENE) / 2,
                  .sceneZVoxels = (spacingZ - SCENE - SECOND_SITE_VOXELS) / 2,
                  .surfaceVoxels = {},
                  .editedVoxels = 0};
  // The lord builds twice over: a second castle a little way off, as a holding grows. Each stands on
  // a plateau the generator raised, as the scene's castle does, so its footprints are level already.
  std::vector<SiteOrigin> sites;
  for (std::int32_t holding = 0; holding < _holdings; ++holding)
  {
    const std::int32_t originX = (holding % HOLDINGS_ACROSS) * spacingX + (spacingX - SCENE) / 2;
    const std::int32_t originZ = (holding / HOLDINGS_ACROSS) * spacingZ + (spacingZ - SCENE - SECOND_SITE_VOXELS) / 2;
    sites.push_back({.xVoxels = originX, .zVoxels = originZ});
    sites.push_back({.xVoxels = originX, .zVoxels = originZ + SECOND_SITE_VOXELS});
  }
  BuildGround(_store, world, _seed, sites);
  for (std::size_t index = 0; index < sites.size(); ++index)
  {
    const Site lord(_store, world, sites[index].xVoxels, sites[index].zVoxels, world.editedVoxels);
    BuildDitchAndRampart(lord);
    const std::uint64_t before = world.editedVoxels;
    BuildCastle(lord, _templates);
    // Its stone comes out of a quarry beside it, as much as it built with.
    DigQuarry(lord, world.editedVoxels - before);
    // A holding's first site is the one besieged.
    const std::size_t holding = index / 2;
    if (index % 2 == 0 && holding < BREACHED_HOLDINGS)
    {
      BuildBreach(lord, _seed + holding);
    }
  }
  return world;
}

std::int32_t TopSolidAtOrBelow(const ChunkStore& _store, std::int32_t _x, std::int32_t _z, std::int32_t _fromVoxels)
{
  for (std::int32_t y = _fromVoxels; y >= 0; --y)
  {
    if (_store.At({.xVoxels = _x, .yVoxels = y, .zVoxels = _z}) != 0)
    {
      return y;
    }
  }
  return -1;
}

std::array<Protocol::VoxelPosition, 8> HalfMeterVoxels(Protocol::VoxelPosition _oneMeter) noexcept
{
  std::array<Protocol::VoxelPosition, 8> voxels{};
  for (std::int32_t sub = 0; sub < 8; ++sub)
  {
    voxels[static_cast<std::size_t>(sub)] = {.xVoxels = 2 * _oneMeter.xVoxels + (sub & 1),
                                             .yVoxels = 2 * _oneMeter.yVoxels + ((sub >> 1) & 1),
                                             .zVoxels = 2 * _oneMeter.zVoxels + ((sub >> 2) & 1)};
  }
  return voxels;
}

void SplitToHalfMeters(const ChunkStore& _oneMeter, std::int32_t _minXVoxels, std::int32_t _minZVoxels, std::int32_t _maxXVoxels,
                       std::int32_t _maxZVoxels, ChunkStore& _halfMeter)
{
  for (std::int32_t y = 0; y < HEIGHT; ++y)
  {
    for (std::int32_t z = _minZVoxels; z <= _maxZVoxels; ++z)
    {
      for (std::int32_t x = _minXVoxels; x <= _maxXVoxels; ++x)
      {
        const Protocol::VoxelPosition voxel{.xVoxels = x, .yVoxels = y, .zVoxels = z};
        const std::uint8_t index = _oneMeter.At(voxel);
        if (index == 0)
        {
          continue;
        }
        for (const Protocol::VoxelPosition& half : HalfMeterVoxels(voxel))
        {
          _halfMeter.Set(half, index);
        }
      }
    }
  }
}

void SplitChanges(const ChunkStore& _oneMeter, std::span<const Protocol::VoxelPosition> _changed, ChunkStore& _halfMeter)
{
  for (const Protocol::VoxelPosition& voxel : _changed)
  {
    const std::uint8_t index = _oneMeter.At(voxel);
    for (const Protocol::VoxelPosition& half : HalfMeterVoxels(voxel))
    {
      _halfMeter.Set(half, index);
    }
  }
}

TestWorldEdits::TestWorldEdits(const ChunkStore& _store, const TestWorld& _world, std::uint64_t _seed)
  : m_sceneXVoxels(_world.sceneXVoxels),
    m_sceneZVoxels(_world.sceneZVoxels),
    m_collapseId(Mix(_seed ^ Mix(COLLAPSE_FRAME)))
{
  // The quarry's columns: the stone body north of the castle, clear of the castle's north towers
  // and of the quarry yard, in world coordinates.
  for (std::int32_t z = 82; z < 118; ++z)
  {
    for (std::int32_t x = 46; x < 104; ++x)
    {
      if (!InQuarryBody(x, z) || (x >= 91 && z >= 87 && z <= 95))
      {
        continue;
      }
      const std::int32_t worldX = x + m_sceneXVoxels;
      const std::int32_t worldZ = z + m_sceneZVoxels;
      m_quarry.push_back({.xVoxels = worldX, .zVoxels = worldZ, .topVoxels = TopSolidAtOrBelow(_store, worldX, worldZ, HEIGHT - 1)});
    }
  }
  // A seeded order, which breaks ties between columns of equal height, so each layer goes in a
  // scatter rather than a sweep. The order is of the scene's own coordinates, so it is the same
  // wherever the scene stands.
  std::ranges::sort(m_quarry, {},
                    [this, _seed](const Column& _column)
                    {
                      const auto x = static_cast<std::uint64_t>(_column.xVoxels - m_sceneXVoxels);
                      const auto z = static_cast<std::uint32_t>(_column.zVoxels - m_sceneZVoxels);
                      return Mix(_seed ^ Mix((x << 32) | z));
                    });
}

std::vector<Protocol::VoxelPosition> TestWorldEdits::Apply(ChunkStore& _store, std::uint32_t _frame)
{
  std::vector<Protocol::VoxelPosition> changed;
  if (_frame >= LAST_FRAME)
  {
    return changed;
  }
  // 94 a second at 60 frames a second: one or two each frame, exactly 94 in each second.
  const std::uint32_t voxels =
    (_frame + 1) * QUARRY_VOXELS_PER_SECOND / FRAMES_PER_SECOND - _frame * QUARRY_VOXELS_PER_SECOND / FRAMES_PER_SECOND;
  Quarry(_store, voxels, changed);
  if (_frame == COLLAPSE_FRAME)
  {
    const std::vector<Protocol::VoxelPosition> collapse = Collapse(_store, &m_collapse);
    changed.insert(changed.end(), collapse.begin(), collapse.end());
  }
  return changed;
}

void TestWorldEdits::Quarry(ChunkStore& _store, std::uint32_t _voxels, std::vector<Protocol::VoxelPosition>& _changed)
{
  // The quarry works down from its highest ground: each voxel dug is the top of the highest column,
  // so the rim comes down to the terraces and the pit widens.
  for (std::uint32_t dug = 0; dug < _voxels; ++dug)
  {
    const auto highest = std::ranges::max_element(m_quarry, {}, &Column::topVoxels); // the first of equals
    if (highest == m_quarry.end() || highest->topVoxels < QUARRY_FLOOR_VOXELS)
    {
      break;
    }
    const Protocol::VoxelPosition voxel{.xVoxels = highest->xVoxels, .yVoxels = highest->topVoxels, .zVoxels = highest->zVoxels};
    _store.Set(voxel, 0);
    _changed.push_back(voxel);
    highest->topVoxels = TopSolidAtOrBelow(_store, highest->xVoxels, highest->zVoxels, highest->topVoxels - 1);
  }
}

const std::vector<FallenVoxel>& TestWorldEdits::Fallen() const noexcept
{
  return m_collapse.fallen;
}

const std::vector<Protocol::VoxelPosition>& TestWorldEdits::Rubble() const noexcept
{
  return m_collapse.rubble;
}

std::uint64_t TestWorldEdits::CollapseId() const noexcept
{
  return m_collapseId;
}

std::vector<Protocol::VoxelPosition> TestWorldEdits::Collapse(ChunkStore& _store, CollapseRecord* _record) const
{
  // The section falls: every voxel it still has goes, and as much stone rubble is heaped at its
  // foot outside, highest by the wall.
  std::vector<Protocol::VoxelPosition> changed;
  const std::int32_t sectionX = COLLAPSE_X_VOXELS + m_sceneXVoxels;
  const std::int32_t sectionZ = COLLAPSE_Z_VOXELS + m_sceneZVoxels;
  std::uint32_t fallen = 0;
  for (std::int32_t y = G; y < G + 7; ++y)
  {
    for (std::int32_t z = sectionZ; z < sectionZ + 8; ++z)
    {
      for (std::int32_t x = sectionX; x < sectionX + 2; ++x)
      {
        const Protocol::VoxelPosition voxel{.xVoxels = x, .yVoxels = y, .zVoxels = z};
        const std::uint8_t index = _store.At(voxel);
        if (index != 0)
        {
          if (_record != nullptr)
          {
            _record->fallen.push_back({.position = voxel, .paletteIndex = index});
          }
          _store.Set(voxel, 0);
          changed.push_back(voxel);
          ++fallen;
        }
      }
    }
  }
  struct HeapColumn
  {
    std::int32_t x;
    std::int32_t z;
    std::int32_t tenthsFromWall; // from the section's foot, in tenths of a meter
  };
  std::vector<HeapColumn> heap;
  for (std::int32_t z = sectionZ - 3; z < sectionZ + 11; ++z)
  {
    for (std::int32_t x = sectionX - 8; x < sectionX; ++x)
    {
      const std::int64_t dx = 10 * (sectionX - x) - 5;
      const std::int64_t dz = std::max<std::int64_t>(0, std::abs(10 * z + 5 - 10 * (sectionZ + 4)) - 40);
      heap.push_back({.x = x, .z = z, .tenthsFromWall = static_cast<std::int32_t>(IntegerSqrt(dx * dx + dz * dz))});
    }
  }
  std::ranges::sort(heap, {}, &HeapColumn::tenthsFromWall);
  std::uint32_t placed = 0;
  for (std::int32_t layer = 0; placed < fallen && layer < HEIGHT; ++layer)
  {
    for (const HeapColumn& column : heap)
    {
      // 4 m high by the wall, a meter lower each 2 m out, and at least one layer everywhere; never on
      // what stands taller than the heap, such as the corner tower, which the heap's box overlaps.
      const std::int32_t height = std::max(1, 4 - column.tenthsFromWall / 20);
      const std::int32_t top = TopSolidAtOrBelow(_store, column.x, column.z, HEIGHT - 1);
      if (placed == fallen || layer >= height || top >= G + HEAP_HEIGHT_VOXELS)
      {
        continue;
      }
      const Protocol::VoxelPosition voxel{.xVoxels = column.x, .yVoxels = top + 1, .zVoxels = column.z};
      _store.Set(voxel, STONE_RUBBLE);
      changed.push_back(voxel);
      if (_record != nullptr)
      {
        _record->rubble.push_back(voxel);
      }
      ++placed;
    }
  }
  return changed;
}

} // namespace Simulation
