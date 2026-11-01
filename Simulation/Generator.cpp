#include "pch.h"

#include "Generator.h"

#include "SplitMix64.h"

namespace Simulation
{

namespace
{

// Palette indices (GameData/Server/Palette.json).
constexpr std::uint8_t GRASS = 1;
constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t COMMON_ROCK = 3;
constexpr std::uint8_t BUILDING_STONE = 4;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t TIMBER = 7;
constexpr std::uint8_t WATER = 9;
constexpr std::uint8_t LEAVES_A = 10;
constexpr std::uint8_t LEAVES_B = 11;

// Fixed point with 16 fraction bits, as M1's test ground (Simulation/TestWorld.cpp) uses.
constexpr std::int64_t ONE = std::int64_t{1} << 16;

// What each draw is for (ADR-013, decision 4): a salt per purpose, so that adding a draw never
// shifts another.
constexpr std::uint64_t RELIEF_SALT = 1;
constexpr std::uint64_t SOIL_SALT = 5;
constexpr std::uint64_t BEDROCK_SALT = 9;
constexpr std::uint64_t FOREST_SALT = 21;
constexpr std::uint64_t TREE_X_SALT = 22;
constexpr std::uint64_t TREE_Z_SALT = 23;
constexpr std::uint64_t TREE_KEEP_SALT = 24;
constexpr std::uint64_t TREE_HEIGHT_SALT = 25;
constexpr std::uint64_t TREE_LEAVES_SALT = 26;
constexpr std::uint64_t RIVER_SALT = 27;
constexpr std::uint64_t OUTCROP_SALT = 31;

// The land: M1's test ground, 6 m and up to 6 m more (GameDesign.md, M2's values, item 3).
constexpr std::int64_t LAND_BASE_VOXELS = 6;
constexpr std::int64_t LAND_RISE_VOXELS = 6;
// The river: a bed 2 m up, water to 4 m, so 2 m deep, and a ford of one voxel every FORD_PERIOD.
constexpr std::int32_t RIVER_BED_VOXELS = 2;
constexpr std::int32_t RIVER_WATER_TOP_VOXELS = 4;
constexpr std::int64_t RIVER_HALF_WIDTH = 9 * ONE / 2;  // water within 4.5 m of its middle
constexpr std::int64_t RIVER_FLAT_HALF_WIDTH = 4 * ONE; // the bed is flat within 4 m
constexpr std::int64_t VALLEY_HALF_WIDTH = 9 * ONE;     // the valley's sides reach out 9 m
constexpr std::int32_t FORD_PERIOD_VOXELS = 96;
constexpr std::int32_t FORD_LENGTH_VOXELS = 6;
// A ford's banks rise 1 m in 3 from its water, gentle enough for every navigation class, the
// machines' climb being the steepest (the owner, after M2's report; GENERATOR_VERSION 2). From the
// ford's ground at 3 m to the highest land, 12 m, they reach 27 m beyond the water; no tree stands
// within a canopy's reach of them.
constexpr std::int32_t FORD_GROUND_VOXELS = RIVER_WATER_TOP_VOXELS - 1;
constexpr std::int64_t FORD_BANK_RUN_CELLS = 3;
constexpr std::int64_t FORD_CLEAR_HALF_WIDTH = 34 * ONE;
constexpr std::int32_t MEANDER_PERIOD_VOXELS = 192;
constexpr std::int64_t MEANDER_VOXELS = 30;
// Outcrops of building stone where the resource seed's noise is highest, at most 3 m deep.
constexpr std::int64_t OUTCROP_THRESHOLD = ONE * 79 / 100;
constexpr std::int32_t OUTCROP_DEPTH_VOXELS = 3;
// Forest where its noise is high, a tree on a jittered grid of 5 m cells, 7 in 10 of them.
constexpr std::int64_t FOREST_THRESHOLD = ONE * 56 / 100;
constexpr std::int32_t TREE_CELL_VOXELS = 5;
constexpr std::uint32_t TREE_PERCENT = 70;
constexpr std::int32_t CANOPY_RADIUS_VOXELS = 2;
constexpr std::int32_t CANOPY_HEIGHT_VOXELS = 3;

/// A hash of two integers and a salt, from 0 up to ONE.
std::int64_t Hash(std::int64_t _x, std::int64_t _z, std::uint64_t _salt, std::uint64_t _seed) noexcept
{
  const std::uint64_t key = static_cast<std::uint64_t>(_x) ^ (static_cast<std::uint64_t>(_z) << 32);
  return static_cast<std::int64_t>(Core::KeyedDraw(_seed, _salt, key) & static_cast<std::uint64_t>(ONE - 1));
}

std::int64_t Smooth(std::int64_t _t) noexcept
{
  return _t * _t / ONE * (3 * ONE - 2 * _t) / ONE;
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

/// Three octaves, weighted as M1's test ground weights them, at _thousandths of a cycle a voxel.
std::int64_t Fbm(std::int32_t _x, std::int32_t _z, std::int64_t _thousandths, std::uint64_t _salt, std::uint64_t _seed) noexcept
{
  const std::int64_t x = static_cast<std::int64_t>(_x) * ONE * _thousandths / 1000;
  const std::int64_t z = static_cast<std::int64_t>(_z) * ONE * _thousandths / 1000;
  return (55 * ValueNoise(x, z, _salt, _seed) + 30 * ValueNoise(2 * x, 2 * z, _salt + 7, _seed) +
          15 * ValueNoise(4 * x, 4 * z, _salt + 13, _seed)) /
         100;
}

constexpr std::int32_t Round(std::int64_t _fixed) noexcept
{
  return static_cast<std::int32_t>((_fixed + ONE / 2) >> 16);
}

/// Whether row _z crosses the river at a ford.
constexpr bool IsFordRow(std::int32_t _z) noexcept
{
  return _z >= 0 && _z % FORD_PERIOD_VOXELS < FORD_LENGTH_VOXELS;
}

/// The river's middle at row _z: a meander about two fifths of the way across the map, shifted
/// along by the world seed.
std::int64_t RiverX(std::int32_t _z, std::int32_t _widthVoxels, std::uint64_t _seed) noexcept
{
  const auto shift = static_cast<std::int32_t>(Core::DrawBelow(Core::KeyedDraw(_seed, RIVER_SALT, 0), MEANDER_PERIOD_VOXELS));
  const std::int32_t wrapped = (_z + shift) % MEANDER_PERIOD_VOXELS;
  const std::int64_t phase = static_cast<std::int64_t>(wrapped) * ONE / MEANDER_PERIOD_VOXELS;
  const std::int64_t triangle = std::abs(4 * phase - 2 * ONE) - ONE; // from -1 to 1
  return static_cast<std::int64_t>(_widthVoxels) * ONE * 2 / 5 + MEANDER_VOXELS * triangle;
}

} // namespace

Generator::Generator(const Desc& _desc)
  : m_desc(_desc)
{
  if (m_desc.widthVoxels <= 0 || m_desc.depthVoxels <= 0 || m_desc.heightVoxels <= HIGHEST_SURFACE_VOXELS + 8 ||
      m_desc.widthVoxels % Protocol::CHUNK_EDGE_VOXELS != 0 || m_desc.depthVoxels % Protocol::CHUNK_EDGE_VOXELS != 0 ||
      m_desc.heightVoxels % Protocol::CHUNK_EDGE_VOXELS != 0)
  {
    throw std::invalid_argument("the generator's map is a whole number of chunks, and tall enough for its relief and trees");
  }
  const std::int32_t width = m_desc.widthVoxels;
  const std::int32_t depth = m_desc.depthVoxels;
  const std::uint64_t seed = m_desc.worldSeed;
  m_columns.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(depth));

  // The relief, the river and its fords, bedrock and soil, from the world seed alone.
  std::vector<std::int64_t> fromRiver(m_columns.size());
  for (std::int32_t z = 0; z < depth; ++z)
  {
    const std::int64_t riverX = RiverX(z, width, seed);
    const bool ford = IsFordRow(z);
    for (std::int32_t x = 0; x < width; ++x)
    {
      const std::size_t at = ColumnIndex(x, z);
      std::int64_t land = LAND_BASE_VOXELS * ONE + LAND_RISE_VOXELS * Fbm(x, z, 35, RELIEF_SALT, seed);
      const std::int64_t distance = std::abs(static_cast<std::int64_t>(x) * ONE + ONE / 2 - riverX);
      fromRiver[at] = distance;
      if (ford)
      {
        land = std::min(land, FORD_GROUND_VOXELS * ONE + std::max<std::int64_t>(0, distance - RIVER_HALF_WIDTH) / FORD_BANK_RUN_CELLS);
      }
      else if (distance < VALLEY_HALF_WIDTH)
      {
        land = std::min(land, std::max(RIVER_BED_VOXELS * ONE, RIVER_BED_VOXELS * ONE + (distance - RIVER_FLAT_HALF_WIDTH) * 11 / 10));
      }
      Column& column = m_columns[at];
      std::int32_t surface = std::clamp(Round(land), LOWEST_SURFACE_VOXELS, HIGHEST_SURFACE_VOXELS);
      std::int32_t waterTop = surface;
      if (distance < RIVER_HALF_WIDTH)
      {
        if (ford)
        {
          surface = std::max(surface, RIVER_WATER_TOP_VOXELS - 1); // one voxel of water
        }
        waterTop = std::max(surface, RIVER_WATER_TOP_VOXELS);
      }
      const std::int32_t bedrockTop = std::min(surface, 1 + Round(3 * Fbm(x, z, 50, BEDROCK_SALT, seed)));
      // Soil from 1 m to 8 m: thin over rock where its noise is low, down to bedrock where it is high.
      const std::int32_t soil = std::clamp(1 + Round(14 * (Fbm(x, z, 30, SOIL_SALT, seed) - ONE * 35 / 100)), 1, 8);
      column = {.surface = static_cast<std::int16_t>(surface),
                .bedrockTop = static_cast<std::int16_t>(bedrockTop),
                .soilBottom = static_cast<std::int16_t>(std::max(bedrockTop, surface - soil)),
                .stoneBottom = static_cast<std::int16_t>(surface),
                .waterTop = static_cast<std::int16_t>(waterTop),
                .trunkTop = static_cast<std::int16_t>(surface),
                .leavesBottom = 0,
                .leavesTop = 0,
                .leavesIndex = 0,
                .grass = false};
    }
  }

  // Earth only where no four-neighbour's ground is more than a voxel lower; steeper ground is rock,
  // so nothing the generator leaves would slump (ADR-003, *Consequences*). Water and trees only
  // raise a neighbour's top, so judging by the ground is the stricter rule.
  constexpr std::array<std::array<std::int32_t, 2>, 4> SIDES = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
  for (std::int32_t z = 0; z < depth; ++z)
  {
    for (std::int32_t x = 0; x < width; ++x)
    {
      Column& column = m_columns[ColumnIndex(x, z)];
      for (const auto& [dx, dz] : SIDES)
      {
        const std::int32_t nx = x + dx;
        const std::int32_t nz = z + dz;
        if (nx >= 0 && nx < width && nz >= 0 && nz < depth && m_columns[ColumnIndex(nx, nz)].surface < column.surface - 1)
        {
          column.soilBottom = column.surface;
        }
      }
    }
  }

  // Outcrops of building stone, from the resource seed: in place of the ground, never above it.
  for (std::int32_t z = 0; z < depth; ++z)
  {
    for (std::int32_t x = 0; x < width; ++x)
    {
      Column& column = m_columns[ColumnIndex(x, z)];
      if (column.waterTop == column.surface && Fbm(x, z, 25, OUTCROP_SALT, m_desc.resourceSeed) > OUTCROP_THRESHOLD)
      {
        column.stoneBottom = static_cast<std::int16_t>(std::max<std::int32_t>(column.bedrockTop, column.surface - OUTCROP_DEPTH_VOXELS));
      }
      column.grass = column.soilBottom < column.stoneBottom && column.stoneBottom == column.surface && column.waterTop == column.surface;
    }
  }

  // Forest, from the world seed: a tree in most 5 m cells where the forest's noise is high, standing
  // on grass away from the river and its fords' banks.
  for (std::int32_t cellZ = 0; cellZ * TREE_CELL_VOXELS < depth; ++cellZ)
  {
    for (std::int32_t cellX = 0; cellX * TREE_CELL_VOXELS < width; ++cellX)
    {
      const std::uint64_t key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellZ)) << 32) | static_cast<std::uint32_t>(cellX);
      const std::int32_t x =
        cellX * TREE_CELL_VOXELS + static_cast<std::int32_t>(Core::DrawBelow(Core::KeyedDraw(seed, TREE_X_SALT, key), TREE_CELL_VOXELS));
      const std::int32_t z =
        cellZ * TREE_CELL_VOXELS + static_cast<std::int32_t>(Core::DrawBelow(Core::KeyedDraw(seed, TREE_Z_SALT, key), TREE_CELL_VOXELS));
      if (x >= width || z >= depth)
      {
        continue;
      }
      Column& trunk = m_columns[ColumnIndex(x, z)];
      bool nearFord = false;
      for (std::int32_t dz = -CANOPY_RADIUS_VOXELS; dz <= CANOPY_RADIUS_VOXELS; ++dz)
      {
        nearFord = nearFord || IsFordRow(z + dz);
      }
      nearFord = nearFord && fromRiver[ColumnIndex(x, z)] < FORD_CLEAR_HALF_WIDTH;
      if (!trunk.grass || nearFord || fromRiver[ColumnIndex(x, z)] < 2 * VALLEY_HALF_WIDTH ||
          Core::DrawBelow(Core::KeyedDraw(seed, TREE_KEEP_SALT, key), 100) >= TREE_PERCENT ||
          Fbm(x, z, 12, FOREST_SALT, seed) <= FOREST_THRESHOLD)
      {
        continue;
      }
      const std::int32_t trunkTop =
        trunk.surface + 2 + static_cast<std::int32_t>(Core::DrawBelow(Core::KeyedDraw(seed, TREE_HEIGHT_SALT, key), 3));
      const std::uint8_t leaves = Core::DrawBelow(Core::KeyedDraw(seed, TREE_LEAVES_SALT, key), 2) == 0 ? LEAVES_A : LEAVES_B;
      trunk.trunkTop = static_cast<std::int16_t>(trunkTop);
      trunk.grass = false; // the trunk stands on earth
      const std::int32_t canopyBottom = trunkTop - 1;
      for (std::int32_t dz = -CANOPY_RADIUS_VOXELS; dz <= CANOPY_RADIUS_VOXELS; ++dz)
      {
        for (std::int32_t dx = -CANOPY_RADIUS_VOXELS; dx <= CANOPY_RADIUS_VOXELS; ++dx)
        {
          const std::int32_t cx = x + dx;
          const std::int32_t cz = z + dz;
          if (cx < 0 || cx >= width || cz < 0 || cz >= depth || dx * dx + dz * dz > CANOPY_RADIUS_VOXELS * CANOPY_RADIUS_VOXELS + 1)
          {
            continue;
          }
          Column& canopy = m_columns[ColumnIndex(cx, cz)];
          const std::int32_t top = canopyBottom + CANOPY_HEIGHT_VOXELS + (dx == 0 && dz == 0 ? 1 : 0);
          if (canopy.leavesTop == canopy.leavesBottom)
          {
            canopy.leavesBottom = static_cast<std::int16_t>(canopyBottom);
            canopy.leavesTop = static_cast<std::int16_t>(top);
            canopy.leavesIndex = leaves;
          }
          else
          {
            canopy.leavesBottom = std::min(canopy.leavesBottom, static_cast<std::int16_t>(canopyBottom));
            canopy.leavesTop = std::max(canopy.leavesTop, static_cast<std::int16_t>(top));
          }
        }
      }
    }
  }
}

void Generator::GenerateChunk(Protocol::ChunkCoordinate _coordinate, Protocol::Chunk& _chunk) const
{
  constexpr std::int32_t EDGE = Protocol::CHUNK_EDGE_VOXELS;
  const std::int32_t minX = _coordinate.xChunks * EDGE;
  const std::int32_t minY = _coordinate.yChunks * EDGE;
  const std::int32_t minZ = _coordinate.zChunks * EDGE;
  if (minX < 0 || minX >= m_desc.widthVoxels || minY < 0 || minY >= m_desc.heightVoxels || minZ < 0 || minZ >= m_desc.depthVoxels)
  {
    return;
  }
  for (std::int32_t z = 0; z < EDGE; ++z)
  {
    for (std::int32_t x = 0; x < EDGE; ++x)
    {
      const Column& column = m_columns[ColumnIndex(minX + x, minZ + z)];
      const std::int32_t top = std::max({std::int32_t{column.waterTop}, std::int32_t{column.trunkTop}, std::int32_t{column.leavesTop}});
      for (std::int32_t y = 0; y < EDGE && minY + y < top; ++y)
      {
        const std::uint8_t index = VoxelAt(column, minY + y);
        if (index != Protocol::Chunk::EMPTY)
        {
          _chunk.Set(x, y, z, index);
        }
      }
    }
  }
}

std::int32_t Generator::SurfaceAt(std::int32_t _x, std::int32_t _z) const
{
  return m_columns[ColumnIndex(std::clamp(_x, 0, m_desc.widthVoxels - 1), std::clamp(_z, 0, m_desc.depthVoxels - 1))].surface;
}

const Generator::Desc& Generator::Description() const noexcept
{
  return m_desc;
}

std::int32_t Generator::WidthChunks() const noexcept
{
  return m_desc.widthVoxels / Protocol::CHUNK_EDGE_VOXELS;
}

std::int32_t Generator::HeightChunks() const noexcept
{
  return m_desc.heightVoxels / Protocol::CHUNK_EDGE_VOXELS;
}

std::int32_t Generator::DepthChunks() const noexcept
{
  return m_desc.depthVoxels / Protocol::CHUNK_EDGE_VOXELS;
}

std::size_t Generator::ColumnIndex(std::int32_t _x, std::int32_t _z) const noexcept
{
  return static_cast<std::size_t>(_z) * static_cast<std::size_t>(m_desc.widthVoxels) + static_cast<std::size_t>(_x);
}

std::uint8_t Generator::VoxelAt(const Column& _column, std::int32_t _y) const noexcept
{
  if (_y < _column.bedrockTop)
  {
    return BEDROCK;
  }
  if (_y < _column.surface)
  {
    if (_y >= _column.stoneBottom)
    {
      return BUILDING_STONE;
    }
    if (_y >= _column.soilBottom)
    {
      return _column.grass && _y == _column.surface - 1 ? GRASS : EARTH;
    }
    return COMMON_ROCK;
  }
  if (_y < _column.waterTop)
  {
    return WATER;
  }
  if (_y < _column.trunkTop)
  {
    return TIMBER;
  }
  if (_y >= _column.leavesBottom && _y < _column.leavesTop)
  {
    return _column.leavesIndex;
  }
  return Protocol::Chunk::EMPTY;
}

} // namespace Simulation
