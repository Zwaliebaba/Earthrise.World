#include "pch.h"

#include "SceneScript.h"

#include "Catalog.h"

namespace Spike
{

namespace
{

// The v5 scene's frame (Design/Reference/SiegePrototypeV2/reference, "Earthrise Siege Scene v5"):
// a 128 m holding, 48 m of air, the castle's ground at 15 m, and the castle's middle at (74, 61).
constexpr std::int32_t EDGE_VOXELS = 128;
constexpr std::int32_t HEIGHT_VOXELS = 48;
constexpr std::int32_t CASTLE_GROUND_VOXELS = 15;
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
// Quarrying digs no lower than this, so the quarry keeps a floor above the bedrock.
constexpr std::int32_t QUARRY_FLOOR_VOXELS = 4;

// Walkers: a circle 2 m across at walking pace (the owner's choice of 2026-11-01).
constexpr float WALK_RADIUS_METERS = 1.0F;
constexpr float WALK_METERS_PER_SECOND = 1.4F;
// How far above where it stands a walker may climb, so that a loop past a wall stays on the ground.
constexpr std::int32_t WALK_CLIMB_VOXELS = 3;

// ── A seeded integer height field ─────────────────────────────────────────────────────────────
// Fixed point with 16 fraction bits, from one hash, so the scene is the same on every machine
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

/// The same, as a float from 0 to 1, for placing models.
float Hash01(std::int64_t _x, std::int64_t _z, std::uint64_t _salt, std::uint64_t _seed) noexcept
{
  return static_cast<float>(Hash(_x, _z, _salt, _seed)) / static_cast<float>(ONE);
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

/// The river's middle at row _z: the v5 scene's gentle curve, with a triangle wave of the same period
/// for its sine.
std::int64_t RiverX(std::int32_t _z) noexcept
{
  constexpr std::int32_t PERIOD_VOXELS = 105;
  const std::int64_t phase = ((_z + PERIOD_VOXELS / 4) % PERIOD_VOXELS) * ONE / PERIOD_VOXELS;
  const std::int64_t triangle = std::abs(4 * phase - 2 * ONE) - ONE;
  return 12 * ONE + static_cast<std::int64_t>(_z) * ONE * 12 / 100 + 5 * triangle;
}

// ── Writing into the store ────────────────────────────────────────────────────────────────────

class Writer
{
public:
  explicit Writer(Simulation::ChunkStore& _store)
    : m_store(_store)
  {
  }

  void Set(std::int32_t _x, std::int32_t _y, std::int32_t _z, std::uint8_t _index) const
  {
    if (_x >= 0 && _x < EDGE_VOXELS && _z >= 0 && _z < EDGE_VOXELS && _y >= 0 && _y < HEIGHT_VOXELS)
    {
      m_store.Set({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z}, _index);
    }
  }

  [[nodiscard]] std::uint8_t At(std::int32_t _x, std::int32_t _y, std::int32_t _z) const
  {
    if (_x < 0 || _x >= EDGE_VOXELS || _z < 0 || _z >= EDGE_VOXELS || _y < 0 || _y >= HEIGHT_VOXELS)
    {
      return 0;
    }
    return m_store.At({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z});
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

private:
  Simulation::ChunkStore& m_store;
};

/// The height of the highest solid voxel in a column at or below _fromVoxels, or -1 for none.
std::int32_t TopAtOrBelow(const Simulation::ChunkStore& _store, std::int32_t _x, std::int32_t _z, std::int32_t _fromVoxels)
{
  if (_x < 0 || _x >= EDGE_VOXELS || _z < 0 || _z >= EDGE_VOXELS)
  {
    return -1;
  }
  for (std::int32_t y = std::min(_fromVoxels, HEIGHT_VOXELS - 1); y >= 0; --y)
  {
    if (_store.At({.xVoxels = _x, .yVoxels = y, .zVoxels = _z}) != 0)
    {
      return y;
    }
  }
  return -1;
}

const Content::TemplateView& Template(std::string_view _id)
{
  const Content::TemplateView* found = Content::FindTemplate(_id);
  if (found == nullptr)
  {
    throw std::runtime_error("the test scene needs a template Content does not have");
  }
  return *found;
}

const Content::ModelView& Model(std::string_view _id)
{
  const Content::ModelView* found = Content::FindModel(_id);
  if (found == nullptr)
  {
    throw std::runtime_error("the test scene needs a model Content does not have");
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
void Place(const Writer& _writer, const Content::TemplateView& _template, std::int32_t _x, std::int32_t _y, std::int32_t _z,
           std::uint32_t _quarterTurns)
{
  const Content::VoxelGrid& grid = _template.grid;
  const auto width = static_cast<std::int32_t>(grid.widthVoxels);
  const auto depth = static_cast<std::int32_t>(grid.depthVoxels);
  for (std::uint32_t y = 0; y < grid.heightVoxels; ++y)
  {
    for (std::int32_t z = 0; z < depth; ++z)
    {
      for (std::int32_t x = 0; x < width; ++x)
      {
        const std::uint8_t index = grid.At(static_cast<std::uint32_t>(x), y, static_cast<std::uint32_t>(z));
        if (index != Content::VoxelGrid::EMPTY)
        {
          const auto [turnedX, turnedZ] = Turned(x, z, width, depth, _quarterTurns);
          _writer.Set(_x + turnedX, _y + static_cast<std::int32_t>(y), _z + turnedZ, index);
        }
      }
    }
  }
}

/// A model's footprint in meters after a turn.
std::pair<float, float> Footprint(const Content::ModelView& _model, std::uint32_t _quarterTurns) noexcept
{
  const float edge = static_cast<float>(_model.grid.voxelEdgeMillimeters) / 1000.0F;
  const float width = static_cast<float>(_model.grid.widthVoxels) * edge;
  const float depth = static_cast<float>(_model.grid.depthVoxels) * edge;
  return _quarterTurns % 2 == 0 ? std::pair{width, depth} : std::pair{depth, width};
}

/// The top of the ground under a footprint: the highest of its columns' tops, in meters.
float GroundUnder(const Simulation::ChunkStore& _store, float _xMeters, float _zMeters, float _widthMeters, float _depthMeters)
{
  std::int32_t top = 0;
  for (auto z = static_cast<std::int32_t>(std::floor(_zMeters)); static_cast<float>(z) < _zMeters + _depthMeters; ++z)
  {
    for (auto x = static_cast<std::int32_t>(std::floor(_xMeters)); static_cast<float>(x) < _xMeters + _widthMeters; ++x)
    {
      top = std::max(top, TopAtOrBelow(_store, x, z, HEIGHT_VOXELS - 1) + 1);
    }
  }
  return static_cast<float>(top);
}

// ── The scene's parts, in the order the v5 scene builds them ─────────────────────────────────

/// The ground: a gentle field raised to a plateau under the castle, cut by a river to the west, in
/// layers of bedrock, rock, earth and grass. Returns each column's height.
std::vector<std::int32_t> BuildGround(const Writer& _writer, std::uint64_t _seed)
{
  std::vector<std::int32_t> heights(static_cast<std::size_t>(EDGE_VOXELS) * EDGE_VOXELS);
  for (std::int32_t z = 0; z < EDGE_VOXELS; ++z)
  {
    for (std::int32_t x = 0; x < EDGE_VOXELS; ++x)
    {
      const std::int64_t base = 6 * ONE + 6 * Fbm(Scaled(x, 35), Scaled(z, 35), 1, _seed);
      const std::int64_t fromCastle = Length((x - CASTLE_X_VOXELS) * ONE, (z - CASTLE_Z_VOXELS) * ONE);
      std::int64_t height = base + (CASTLE_GROUND_VOXELS * ONE - base) * SmoothStep(42 * ONE, 27 * ONE, fromCastle) / ONE;
      const std::int64_t fromRiver = std::abs(x * ONE - RiverX(z));
      if (fromRiver < 9 * ONE)
      {
        height = std::min(height, std::max(2 * ONE, 2 * ONE + (fromRiver - 4 * ONE) * 11 / 10));
      }
      const std::int32_t top = Round(height);
      heights[static_cast<std::size_t>(z) * EDGE_VOXELS + static_cast<std::size_t>(x)] = top;

      const std::int32_t earth =
        std::max(1, Round(ONE + Fbm(Scaled(x, 80), Scaled(z, 80), 5, _seed) * 5 - (x > CASTLE_X_VOXELS ? 2 * ONE : 0)));
      const std::int32_t bedrock = 1 + Round(Fbm(Scaled(x, 50), Scaled(z, 50), 9, _seed) * 3);
      for (std::int32_t y = 0; y < top; ++y)
      {
        const std::uint8_t layer = y < bedrock ? BEDROCK : (y >= top - earth ? (y == top - 1 ? GRASS : EARTH) : COMMON_ROCK);
        _writer.Set(x, y, z, layer);
      }
      if (fromRiver < 9 * ONE / 2)
      {
        for (std::int32_t y = top; y < 4; ++y)
        {
          _writer.Set(x, y, z, WATER);
        }
      }
    }
  }
  return heights;
}

/// The terraced quarry: a body of building stone under the ground north of the castle, a pit cut in
/// steps of 2 m with a ramp down its south side, and the quarry yard beside it.
void BuildQuarry(const Writer& _writer, const std::vector<std::int32_t>& _heights, std::uint64_t _seed)
{
  const auto heightAt = [&_heights](std::int32_t _x, std::int32_t _z)
  {
    return _heights[static_cast<std::size_t>(std::clamp(_z, 0, EDGE_VOXELS - 1)) * EDGE_VOXELS +
                    static_cast<std::size_t>(std::clamp(_x, 0, EDGE_VOXELS - 1))];
  };
  for (std::int32_t z = 76; z < 118; ++z)
  {
    for (std::int32_t x = 46; x < 104; ++x)
    {
      const std::int64_t reach = Length((x - 74) * ONE * 2 / 3, (z - 93) * ONE) + Fbm(Scaled(x, 200), Scaled(z, 200), 31, _seed) * 4;
      if (reach > 17 * ONE)
      {
        continue;
      }
      const std::int32_t top = heightAt(x, z) - (reach < 8 * ONE ? 0 : 1);
      for (std::int32_t y = 3; y < top; ++y)
      {
        _writer.Set(x, y, z, BUILDING_STONE);
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
      _writer.Box(x, x, floor, HEIGHT_VOXELS - 1, z, z, 0);
    }
  }
  // The quarry yard, T8, beside the pit, facing it. The template wins over the v5 scene's loose
  // stacks of stone, and holds its stock. Its footprint is levelled on its highest column.
  const Content::TemplateView& yard = Template("T8");
  constexpr std::int32_t YARD_X = 92;
  constexpr std::int32_t YARD_Z = 88;
  const auto yardWidth = static_cast<std::int32_t>(yard.grid.depthVoxels); // turned a quarter
  const auto yardDepth = static_cast<std::int32_t>(yard.grid.widthVoxels);
  std::int32_t yardBase = 0;
  for (std::int32_t z = YARD_Z; z < YARD_Z + yardDepth; ++z)
  {
    for (std::int32_t x = YARD_X; x < YARD_X + yardWidth; ++x)
    {
      yardBase = std::max(yardBase, heightAt(x, z));
    }
  }
  for (std::int32_t z = YARD_Z; z < YARD_Z + yardDepth; ++z)
  {
    for (std::int32_t x = YARD_X; x < YARD_X + yardWidth; ++x)
    {
      _writer.Box(x, x, heightAt(x, z), yardBase - 1, z, z, EARTH);
    }
  }
  Place(_writer, yard, YARD_X, yardBase, YARD_Z, 3);
}

/// The road from the gate to the east edge, and the fields to the north-east.
void BuildRoadAndFields(const Writer& _writer, const std::vector<std::int32_t>& _heights, std::uint64_t _seed)
{
  const auto topAt = [&_heights](std::int32_t _x, std::int32_t _z)
  { return _heights[static_cast<std::size_t>(_z) * EDGE_VOXELS + static_cast<std::size_t>(_x)] - 1; };
  for (std::int32_t x = 93; x < EDGE_VOXELS; ++x)
  {
    for (std::int32_t z = 59; z < 64; ++z)
    {
      if (_writer.At(x, topAt(x, z), z) == GRASS)
      {
        _writer.Set(x, topAt(x, z), z, ROAD);
      }
    }
  }
  for (std::int32_t z = 88; z < 124; ++z)
  {
    for (std::int32_t x = 104; x < 124; ++x)
    {
      if (_writer.At(x, topAt(x, z), z) == GRASS && Fbm(Scaled(x, 50), Scaled(z, 50), 41, _seed) > ONE * 35 / 100)
      {
        _writer.Set(x, topAt(x, z), z, (z / 3) % 2 != 0 ? FIELD_A : FIELD_B);
      }
    }
  }
}

/// The ditch and its rampart before the west wall, open where the breach's approach crosses them.
/// The rampart is fresh, so bare earth: only the generator places grass (RendererLook.md).
void BuildDitchAndRampart(const Writer& _writer, const std::vector<std::int32_t>& _heights)
{
  for (std::int32_t z = 34; z < 89; ++z)
  {
    if (z >= 54 && z <= 68)
    {
      continue;
    }
    for (std::int32_t x = 46; x < 57; ++x)
    {
      const std::int32_t ground = _heights[static_cast<std::size_t>(z) * EDGE_VOXELS + static_cast<std::size_t>(x)];
      // Distances from the ditch's and the rampart's middles, in half meters: 50.5 m and 54.5 m.
      const std::int32_t fromDitch = std::abs(2 * x + 1 - 101);
      const std::int32_t ditch = std::clamp((5 - fromDitch + 1) / 2, 0, 2);
      if (ditch > 0)
      {
        _writer.Box(x, x, ground - ditch, ground - 1, z, z, 0);
        continue;
      }
      const std::int32_t fromRampart = std::abs(2 * x + 1 - 109);
      const std::int32_t rampart = std::clamp((4 - fromRampart + 1) / 2, 0, 2);
      _writer.Box(x, x, ground, ground + rampart - 1, z, z, EARTH);
    }
  }
}

/// A cottage, which GameData lacks, drawn as the v5 scene draws it: timber walls 3 m high with a
/// door, and a stepped roof.
void BuildCottage(const Writer& _writer, std::int32_t _x, std::int32_t _z, std::int32_t _width, std::int32_t _depth)
{
  constexpr std::int32_t G = CASTLE_GROUND_VOXELS;
  _writer.Box(_x, _x + _width - 1, G, G + 2, _z, _z + _depth - 1, TIMBER);
  _writer.Box(_x + _width / 2, _x + _width / 2, G, G + 1, _z, _z, OPENING);
  _writer.Box(_x - 1, _x + _width, G + 3, G + 3, _z - 1, _z + _depth, ROOF);
  _writer.Box(_x - 1, _x + _width, G + 4, G + 4, _z, _z + _depth - 1, ROOF);
  _writer.Box(_x - 1, _x + _width, G + 5, G + 5, _z + 1, _z + _depth - 2, ROOF);
}

/// The castle from GameData's templates, which win over the v5 scene's sizes: T4 towers at the
/// corners, three T3 sections between each pair, the T5 gatehouse in the east wall facing the road,
/// the T2 keep with its forebuilding towards the gate, and the v5 scene's cottages inside.
void BuildCastle(const Writer& _writer)
{
  constexpr std::int32_t G = CASTLE_GROUND_VOXELS;
  const Content::TemplateView& wall = Template("T3");
  const Content::TemplateView& tower = Template("T4");
  const Content::TemplateView& gatehouse = Template("T5");
  const Content::TemplateView& keep = Template("T2");

  for (const auto& [x, z] : {std::pair{56, 43}, std::pair{87, 43}, std::pair{56, 74}, std::pair{87, 74}})
  {
    Place(_writer, tower, x, G, z, 0);
  }
  for (const std::int32_t z : {50, 58, 66})
  {
    Place(_writer, wall, 58, G, z, 3); // west, its front facing -x
  }
  for (const std::int32_t x : {63, 71, 79})
  {
    Place(_writer, wall, x, G, 76, 0); // north
    Place(_writer, wall, x, G, 45, 2); // south
  }
  // East: a section each side of the gatehouse. The southern one overlaps the tower by a meter,
  // because the gatehouse leaves 15 m for two 8 m sections.
  Place(_writer, wall, 89, G, 49, 1);
  Place(_writer, wall, 89, G, 66, 1);
  Place(_writer, gatehouse, 87, G, 57, 1);
  Place(_writer, keep, 68, G, 55, 1);

  BuildCottage(_writer, 62, 50, 5, 4);
  BuildCottage(_writer, 62, 69, 5, 4);
  BuildCottage(_writer, 82, 50, 5, 4);
  BuildCottage(_writer, 82, 70, 5, 4);
  BuildCottage(_writer, 70, 70, 5, 4);
}

/// The breach in the west wall, widening as it rises, its craters, and the rubble ramp at its foot.
void BuildBreach(const Writer& _writer, const std::vector<std::int32_t>& _heights, std::uint64_t _seed)
{
  constexpr std::int32_t G = CASTLE_GROUND_VOXELS;
  for (std::int32_t y = G - 1; y <= G + 8; ++y)
  {
    for (std::int32_t z = 50; z < 72; ++z)
    {
      // Its half-width: 1.5 m at the foot, 0.62 m more each meter up, and a ragged edge.
      const std::int64_t halfWidth =
        3 * ONE / 2 + std::max(0, y - G) * ONE * 62 / 100 + (Fbm(Scaled(z, 500), Scaled(y, 500), 77, _seed) - ONE / 2) * 12 / 10;
      if (std::abs((2 * z + 1) * ONE / 2 - 61 * ONE) < halfWidth)
      {
        _writer.Box(58, 60, y, y, z, z, 0);
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
          if (dx * dx + dy * dy + dz * dz < crater.radius * crater.radius && _writer.At(vx, vy, vz) == MASONRY)
          {
            _writer.Set(vx, vy, vz, 0);
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
      const std::int32_t ground = _heights[static_cast<std::size_t>(z) * EDGE_VOXELS + static_cast<std::size_t>(x)];
      for (std::int32_t k = 0; k < height; ++k)
      {
        if (_writer.At(x, ground + k, z) == 0)
        {
          _writer.Set(x, ground + k, z, Hash(x, z * 7 + k, 52, _seed) < ONE * 12 / 100 ? MASONRY : STONE_RUBBLE);
        }
      }
    }
  }
}

/// The forest, in 1 m world voxels as ADR-006 puts trees: west and south of the castle and along the
/// north-west, clear of the castle, the river and the fields, with a few stumps.
void BuildForest(const Writer& _writer, const std::vector<std::int32_t>& _heights, std::uint64_t _seed)
{
  for (std::int32_t z = 2; z < 122; ++z)
  {
    for (std::int32_t x = 2; x < 120; ++x)
    {
      const std::int64_t fromCastle = Length((x - CASTLE_X_VOXELS) * ONE, (z - CASTLE_Z_VOXELS) * ONE);
      const std::int64_t fromRiver = std::abs(x * ONE - RiverX(z));
      // The v5 scene's clearings, and the attackers' camp, where the trebuchets, the mantlets and
      // the reserves stand.
      const bool camp = x >= 24 && x < 48 && z >= 44 && z < 80;
      if (fromCastle < 31 * ONE || fromRiver < 7 * ONE || (x > 90 && z > 84) || camp)
      {
        continue;
      }
      if (!(x + z < 100 || (z > 92 && x < 60)))
      {
        continue;
      }
      if (Fbm(Scaled(x, 60), Scaled(z, 60), 21, _seed) < ONE / 2 || Hash(x, z, 3, _seed) > ONE * 9 / 100)
      {
        continue;
      }
      const std::int32_t ground = _heights[static_cast<std::size_t>(z) * EDGE_VOXELS + static_cast<std::size_t>(x)];
      if (_writer.At(x, ground - 1, z) != GRASS)
      {
        continue;
      }
      if (Hash(x, z, 8, _seed) < ONE / 5 && x + z > 70)
      {
        _writer.Set(x, ground, z, TIMBER); // a stump
        continue;
      }
      const std::int32_t trunk = 2 + static_cast<std::int32_t>(Hash(x, z, 4, _seed) * 3 / ONE);
      const std::uint8_t leaves = Hash(x, z, 5, _seed) < ONE / 2 ? LEAVES_A : LEAVES_B;
      // The crown's radius, 1.3 to 2.2 m, in tenths of a meter, about a point above the trunk.
      const std::int64_t radius = 13 + Hash(x, z, 6, _seed) * 9 / ONE;
      const std::int32_t crown = ground + trunk + 1;
      _writer.Box(x, x, ground, ground + trunk - 1, z, z, TIMBER);
      for (std::int32_t dy = -2; dy <= 2; ++dy)
      {
        for (std::int32_t dz = -2; dz <= 2; ++dz)
        {
          for (std::int32_t dx = -2; dx <= 2; ++dx)
          {
            // Flattened a little: a meter up counts 1.2 m.
            const std::int64_t reach = 100 * (dx * dx + dz * dz) + 144 * dy * dy;
            if (reach <= radius * radius && _writer.At(x + dx, crown + dy, z + dz) == 0)
            {
              _writer.Set(x + dx, crown + dy, z + dz, leaves);
            }
          }
        }
      }
    }
  }
}

/// A group of units scattered about a point, as the v5 scene scatters them, at least 1.1 m apart.
struct Group
{
  float xMeters;
  float zMeters;
  int count;
  float xRadiusMeters;
  float zRadiusMeters;
  std::uint8_t sideColorIndex;
  std::uint64_t salt;
  float spearShare;           // spearmen among them
  std::string_view otherUnit; // the rest
};

void Scatter(const Simulation::ChunkStore& _store, const Group& _group, std::uint64_t _seed, std::vector<std::pair<float, float>>& _taken,
             std::vector<Walker>& _walkers)
{
  int remaining = _group.count;
  // Up to six tries a unit, where the v5 scene allows three, because it places on fewer
  // collisions: its count comes close to the plan's 95 soldiers.
  for (int attempt = 0; attempt < _group.count * 6 && remaining > 0; ++attempt)
  {
    const float x =
      _group.xMeters + (Hash01(attempt, static_cast<std::int64_t>(_group.salt), 71, _seed) - 0.5F) * 2.0F * _group.xRadiusMeters;
    const float z =
      _group.zMeters + (Hash01(attempt, static_cast<std::int64_t>(_group.salt), 72, _seed) - 0.5F) * 2.0F * _group.zRadiusMeters;
    if (std::ranges::any_of(_taken, [x, z](const std::pair<float, float>& _other)
                            { return std::hypot(_other.first - x, _other.second - z) < 1.1F; }))
    {
      continue;
    }
    _taken.emplace_back(x, z);
    const auto columnX = static_cast<std::int32_t>(std::floor(x));
    const auto columnZ = static_cast<std::int32_t>(std::floor(z));
    const std::int32_t top = TopAtOrBelow(_store, columnX, columnZ, HEIGHT_VOXELS - 1);
    --remaining;
    if (top < 0 || _store.At({.xVoxels = columnX, .yVoxels = top, .zVoxels = columnZ}) == WATER)
    {
      continue; // the v5 scene stands nobody in the river
    }
    const bool spear = Hash01(attempt, static_cast<std::int64_t>(_group.salt), 73, _seed) < _group.spearShare;
    _walkers.push_back({.modelId = spear ? std::string_view{"U2"} : _group.otherUnit,
                        .centerXMeters = x,
                        .centerZMeters = z,
                        .phaseRadians = 6.2831853F * Hash01(attempt, static_cast<std::int64_t>(_group.salt), 74, _seed),
                        .homeGroundVoxels = top + 1,
                        .sideColorIndex = _group.sideColorIndex});
  }
}

std::vector<Walker> BuildUnits(const Simulation::ChunkStore& _store, std::uint64_t _seed)
{
  // The v5 scene's groups: the attackers at the breach, behind the mantlets, in reserve and with
  // the trebuchets; the defenders inside the breach; and the labourers in the quarry and fields.
  // Spearmen are U2, the v5 scene's figures without spears men-at-arms (U5) or archers (U3).
  constexpr std::uint8_t RED = ATTACKER_COLOR_INDEX;
  constexpr std::uint8_t BLUE = DEFENDER_COLOR_INDEX;
  const std::array<Group, 12> groups = {
    Group{54.0F, 61.0F, 18, 3.5F, 3.0F, RED, 1, 0.5F, "U5"}, Group{59.0F, 61.0F, 5, 0.6F, 1.2F, RED, 11, 0.3F, "U5"},
    Group{42.0F, 55.5F, 7, 1.2F, 2.0F, RED, 2, 0.6F, "U5"},  Group{42.0F, 66.5F, 7, 1.2F, 2.0F, RED, 3, 0.6F, "U5"},
    Group{38.0F, 61.0F, 18, 3.0F, 4.0F, RED, 4, 0.8F, "U5"}, Group{30.0F, 54.0F, 5, 2.0F, 1.5F, RED, 9, 0.0F, "U3"},
    Group{32.0F, 74.5F, 5, 2.0F, 1.2F, RED, 10, 0.0F, "U3"}, Group{64.0F, 61.0F, 18, 2.5F, 4.5F, BLUE, 5, 0.7F, "U5"},
    Group{66.0F, 52.0F, 6, 2.0F, 1.5F, BLUE, 6, 0.5F, "U3"}, Group{71.0F, 61.0F, 6, 1.0F, 3.0F, BLUE, 12, 0.9F, "U5"},
    Group{75.0F, 94.0F, 14, 11.0F, 7.0F, 0, 7, 0.0F, "U1"},  Group{114.0F, 106.0F, 7, 8.0F, 12.0F, 0, 8, 0.0F, "U1"}};
  std::vector<std::pair<float, float>> taken;
  std::vector<Walker> walkers;
  for (const Group& group : groups)
  {
    Scatter(_store, group, _seed, taken, walkers);
  }
  return walkers;
}

/// A fixed model stood on the ground under its footprint.
ModelPlacement Stand(const Simulation::ChunkStore& _store, std::string_view _id, float _xMeters, float _zMeters,
                     std::uint32_t _quarterTurns, std::uint8_t _sideColorIndex)
{
  const auto [width, depth] = Footprint(Model(_id), _quarterTurns);
  return {.modelId = _id,
          .xMeters = _xMeters,
          .yMeters = GroundUnder(_store, _xMeters, _zMeters, width, depth),
          .zMeters = _zMeters,
          .quarterTurns = _quarterTurns,
          .sideColorIndex = _sideColorIndex};
}

} // namespace

TestScene BuildTestScene(Simulation::ChunkStore& _store, std::uint64_t _seed)
{
  const Writer writer(_store);
  const std::vector<std::int32_t> heights = BuildGround(writer, _seed);
  BuildQuarry(writer, heights, _seed);
  BuildRoadAndFields(writer, heights, _seed);
  BuildDitchAndRampart(writer, heights);
  BuildCastle(writer);
  BuildBreach(writer, heights, _seed);
  BuildForest(writer, heights, _seed);

  TestScene scene{.fixedModels = {},
                  .walkers = BuildUnits(_store, _seed),
                  .centerXMeters = static_cast<float>(CASTLE_X_VOXELS),
                  .centerYMeters = static_cast<float>(CASTLE_GROUND_VOXELS + 3),
                  .centerZMeters = static_cast<float>(CASTLE_Z_VOXELS)};
  // The two trebuchets west of the ditch, their counterweights to the east as in the v5 scene.
  scene.fixedModels.push_back(Stand(_store, "S3", 28.0F, 49.0F, 0, 0));
  scene.fixedModels.push_back(Stand(_store, "S3", 30.0F, 69.5F, 0, 0));
  // The mantlets before the ditch, turned to face the castle.
  for (const auto& [x, z] : {std::pair{45.0F, 53.5F}, std::pair{44.0F, 57.5F}, std::pair{45.0F, 64.5F}, std::pair{44.0F, 68.5F}})
  {
    scene.fixedModels.push_back(Stand(_store, "P1", x - 1.0F, z, 1, 0));
  }
  // The defenders' banner on top of the keep's great tower.
  const std::int32_t keepTop = TopAtOrBelow(_store, 74, 61, HEIGHT_VOXELS - 1) + 1;
  scene.fixedModels.push_back({.modelId = "P2",
                               .xMeters = 74.0F,
                               .yMeters = static_cast<float>(keepTop),
                               .zMeters = 61.0F,
                               .quarterTurns = 0,
                               .sideColorIndex = DEFENDER_COLOR_INDEX});
  return scene;
}

void PlaceWalkers(const Simulation::ChunkStore& _store, const TestScene& _scene, std::uint32_t _frame,
                  std::vector<ModelPlacement>& _placements)
{
  const float seconds = static_cast<float>(_frame) / static_cast<float>(SceneEdits::FRAMES_PER_SECOND);
  const float turned = seconds * WALK_METERS_PER_SECOND / WALK_RADIUS_METERS;
  for (const Walker& walker : _scene.walkers)
  {
    const float angle = walker.phaseRadians + turned;
    const float x = walker.centerXMeters + WALK_RADIUS_METERS * std::cos(angle);
    const float z = walker.centerZMeters + WALK_RADIUS_METERS * std::sin(angle);
    // It walks anticlockwise seen from above, so it heads along (-sin, cos). Its front, +z as
    // authored, takes the nearest quarter turn: 0 faces +z, 1 faces +x, 2 faces -z and 3 faces -x.
    const float headingX = -std::sin(angle);
    const float headingZ = std::cos(angle);
    std::uint32_t quarterTurns = 0;
    if (std::abs(headingZ) >= std::abs(headingX))
    {
      quarterTurns = headingZ >= 0.0F ? 0 : 2;
    }
    else
    {
      quarterTurns = headingX >= 0.0F ? 1 : 3;
    }
    const auto [width, depth] = Footprint(Model(walker.modelId), quarterTurns);
    const std::int32_t top = TopAtOrBelow(_store, static_cast<std::int32_t>(std::floor(x)), static_cast<std::int32_t>(std::floor(z)),
                                          walker.homeGroundVoxels + WALK_CLIMB_VOXELS - 1);
    _placements.push_back({.modelId = walker.modelId,
                           .xMeters = x - 0.5F * width,
                           .yMeters = static_cast<float>(top + 1),
                           .zMeters = z - 0.5F * depth,
                           .quarterTurns = quarterTurns,
                           .sideColorIndex = walker.sideColorIndex});
  }
}

SceneEdits::SceneEdits(const Simulation::ChunkStore& _store, std::uint64_t _seed)
  : m_seed(_seed)
{
  // The quarry's columns: the stone body north of the castle, clear of the castle's north towers
  // and of the quarry yard.
  for (std::int32_t z = 82; z < 118; ++z)
  {
    for (std::int32_t x = 46; x < 104; ++x)
    {
      const std::int64_t reach = Length((x - 74) * ONE * 2 / 3, (z - 93) * ONE);
      if (reach > 17 * ONE || (x >= 91 && z >= 87 && z <= 95))
      {
        continue;
      }
      m_quarry.push_back({.xVoxels = x, .zVoxels = z, .topVoxels = TopAtOrBelow(_store, x, z, HEIGHT_VOXELS - 1)});
    }
  }
  // A seeded order, which breaks ties between columns of equal height, so each layer goes in a
  // scatter rather than a sweep.
  std::ranges::sort(
    m_quarry, {}, [this](const Column& _column)
    { return Mix(m_seed ^ Mix((static_cast<std::uint64_t>(_column.xVoxels) << 32) | static_cast<std::uint32_t>(_column.zVoxels))); });
}

std::uint32_t SceneEdits::Apply(Simulation::ChunkStore& _store, std::uint32_t _frame)
{
  if (_frame >= LAST_FRAME)
  {
    return 0;
  }
  // 94 a second at 60 frames a second: one or two each frame, exactly 94 in each second.
  const std::uint32_t voxels =
    (_frame + 1) * QUARRY_VOXELS_PER_SECOND / FRAMES_PER_SECOND - _frame * QUARRY_VOXELS_PER_SECOND / FRAMES_PER_SECOND;
  std::uint32_t changed = Quarry(_store, voxels);
  if (_frame == COLLAPSE_FRAME)
  {
    changed += Collapse(_store);
  }
  return changed;
}

std::uint32_t SceneEdits::Quarry(Simulation::ChunkStore& _store, std::uint32_t _voxels)
{
  // The quarry works down from its highest ground: each voxel dug is the top of the highest column,
  // so the rim comes down to the terraces and the pit widens.
  std::uint32_t dug = 0;
  for (; dug < _voxels; ++dug)
  {
    const auto highest = std::ranges::max_element(m_quarry, {}, &Column::topVoxels); // the first of equals
    if (highest == m_quarry.end() || highest->topVoxels < QUARRY_FLOOR_VOXELS)
    {
      break;
    }
    _store.Set({.xVoxels = highest->xVoxels, .yVoxels = highest->topVoxels, .zVoxels = highest->zVoxels}, 0);
    highest->topVoxels = TopAtOrBelow(_store, highest->xVoxels, highest->zVoxels, highest->topVoxels - 1);
  }
  return dug;
}

std::uint32_t SceneEdits::Collapse(Simulation::ChunkStore& _store) const
{
  // The section falls: every voxel it still has goes, and as much stone rubble is heaped at its
  // foot outside, highest by the wall.
  constexpr std::int32_t G = CASTLE_GROUND_VOXELS;
  const Writer writer(_store);
  std::uint32_t fallen = 0;
  for (std::int32_t y = G; y < G + 7; ++y)
  {
    for (std::int32_t z = COLLAPSE_Z_VOXELS; z < COLLAPSE_Z_VOXELS + 8; ++z)
    {
      for (std::int32_t x = COLLAPSE_X_VOXELS; x < COLLAPSE_X_VOXELS + 2; ++x)
      {
        if (writer.At(x, y, z) != 0)
        {
          writer.Set(x, y, z, 0);
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
  for (std::int32_t z = COLLAPSE_Z_VOXELS - 3; z < COLLAPSE_Z_VOXELS + 11; ++z)
  {
    for (std::int32_t x = COLLAPSE_X_VOXELS - 8; x < COLLAPSE_X_VOXELS; ++x)
    {
      const std::int64_t dx = 10 * (COLLAPSE_X_VOXELS - x) - 5;
      const std::int64_t dz = std::max<std::int64_t>(0, std::abs(10 * z + 5 - 10 * (COLLAPSE_Z_VOXELS + 4)) - 40);
      heap.push_back({.x = x, .z = z, .tenthsFromWall = static_cast<std::int32_t>(IntegerSqrt(dx * dx + dz * dz))});
    }
  }
  std::ranges::sort(heap, {}, &HeapColumn::tenthsFromWall);
  std::uint32_t placed = 0;
  for (std::int32_t layer = 0; placed < fallen && layer < HEIGHT_VOXELS; ++layer)
  {
    for (const HeapColumn& column : heap)
    {
      // 4 m high by the wall, a meter lower each 2 m out, and at least one layer everywhere.
      const std::int32_t height = std::max(1, 4 - column.tenthsFromWall / 20);
      if (placed == fallen || layer >= height)
      {
        continue;
      }
      const std::int32_t top = TopAtOrBelow(_store, column.x, column.z, HEIGHT_VOXELS - 1);
      writer.Set(column.x, top + 1, column.z, STONE_RUBBLE);
      ++placed;
    }
  }
  return fallen + placed;
}

} // namespace Spike
