#include "pch.h"

#include "NavigationGrid.h"

namespace Simulation
{

namespace
{

// The eight neighbours, in the order of the link bits: north, then clockwise seen from above.
constexpr std::array<std::array<std::int32_t, 2>, 8> NEIGHBORS = {{{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}}};

constexpr std::int32_t FloorDivide(std::int32_t _value, std::int32_t _divisor) noexcept
{
  return _value >= 0 ? _value / _divisor : -((-_value + _divisor - 1) / _divisor);
}

} // namespace

NavigationGrid::NavigationGrid(const Desc& _desc, const MaterialRules& _materials)
  : m_desc(_desc),
    m_materials(_materials)
{
  if (m_desc.widthCells <= 0 || m_desc.depthCells <= 0 || m_desc.widthCells % TILE_EDGE_CELLS != 0 ||
      m_desc.depthCells % TILE_EDGE_CELLS != 0 || m_desc.heightVoxels < 2)
  {
    throw std::invalid_argument("a navigation grid is a whole number of 32-cell tiles across, and at least 2 voxels high");
  }
  const std::size_t cells = static_cast<std::size_t>(m_desc.widthCells) * static_cast<std::size_t>(m_desc.depthCells);
  m_heights.assign(cells, NOT_WALKABLE);
  m_links.assign(cells, 0);
}

void NavigationGrid::RebuildAll(const ChunkStore& _store)
{
  for (std::int32_t z = 0; z < m_desc.depthCells; ++z)
  {
    for (std::int32_t x = 0; x < m_desc.widthCells; ++x)
    {
      m_heights[Index(x, z)] = ColumnHeight(_store, x, z);
    }
  }
  RebuildLinks(0, 0, m_desc.widthCells - 1, m_desc.depthCells - 1);
}

void NavigationGrid::RebuildTile(const ChunkStore& _store, std::int32_t _tileX, std::int32_t _tileZ)
{
  if (_tileX < 0 || _tileX >= TilesAcross() || _tileZ < 0 || _tileZ >= TilesDeep())
  {
    throw std::out_of_range("a navigation tile outside the grid");
  }
  const std::int32_t minX = _tileX * TILE_EDGE_CELLS;
  const std::int32_t minZ = _tileZ * TILE_EDGE_CELLS;
  const std::int32_t maxX = minX + TILE_EDGE_CELLS - 1;
  const std::int32_t maxZ = minZ + TILE_EDGE_CELLS - 1;
  for (std::int32_t z = minZ; z <= maxZ; ++z)
  {
    for (std::int32_t x = minX; x <= maxX; ++x)
    {
      m_heights[Index(x, z)] = ColumnHeight(_store, x, z);
    }
  }
  // The ring of cells around the tile links into it, so their links are redone too.
  RebuildLinks(minX - 1, minZ - 1, maxX + 1, maxZ + 1);
}

std::vector<std::pair<std::int32_t, std::int32_t>> NavigationGrid::TilesTouchedBy(std::span<const Protocol::VoxelPosition> _edits) const
{
  std::vector<std::pair<std::int32_t, std::int32_t>> tiles;
  const auto add = [this, &tiles](std::int32_t _x, std::int32_t _z)
  {
    const std::int32_t tileX = FloorDivide(_x, TILE_EDGE_CELLS);
    const std::int32_t tileZ = FloorDivide(_z, TILE_EDGE_CELLS);
    if (tileX >= 0 && tileX < TilesAcross() && tileZ >= 0 && tileZ < TilesDeep())
    {
      tiles.emplace_back(tileX, tileZ);
    }
  };
  for (const Protocol::VoxelPosition& edit : _edits)
  {
    // The column's tile, and any tile one cell away, whose links read the column.
    for (std::int32_t dz = -1; dz <= 1; ++dz)
    {
      for (std::int32_t dx = -1; dx <= 1; ++dx)
      {
        add(edit.xVoxels + dx, edit.zVoxels + dz);
      }
    }
  }
  std::ranges::sort(tiles, [](const auto& _a, const auto& _b) { return std::tie(_a.second, _a.first) < std::tie(_b.second, _b.first); });
  const auto [first, last] = std::ranges::unique(tiles);
  tiles.erase(first, last);
  return tiles;
}

std::int16_t NavigationGrid::WalkableHeight(std::int32_t _x, std::int32_t _z) const
{
  if (!Inside(_x, _z))
  {
    throw std::out_of_range("a navigation cell outside the grid");
  }
  return m_heights[Index(_x, _z)];
}

std::uint8_t NavigationGrid::Links(std::int32_t _x, std::int32_t _z) const
{
  if (!Inside(_x, _z))
  {
    throw std::out_of_range("a navigation cell outside the grid");
  }
  return m_links[Index(_x, _z)];
}

std::int32_t NavigationGrid::TilesAcross() const noexcept
{
  return m_desc.widthCells / TILE_EDGE_CELLS;
}

std::int32_t NavigationGrid::TilesDeep() const noexcept
{
  return m_desc.depthCells / TILE_EDGE_CELLS;
}

std::int16_t NavigationGrid::ColumnHeight(const ChunkStore& _store, std::int32_t _x, std::int32_t _z) const
{
  // The highest cell with ground under it and room for a unit: two cells clear, of which only the
  // lower may be water (a ford). A structure is never ground, so the top of a wall is not walkable,
  // while a gate's passage under its arch is, and the space under a rock overhang is not the
  // column's surface, because the rock's top is higher (GameDesign.md, *A breach is a path*).
  const auto rule = [&](std::int32_t _y) -> const MaterialRule&
  { return m_materials[_y >= m_desc.heightVoxels ? 0 : _store.At({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z})]; };
  for (std::int32_t y = m_desc.heightVoxels - 1; y >= 1; --y)
  {
    const MaterialRule& cell = rule(y);
    if (cell.support != Support::None)
    {
      continue;
    }
    const MaterialRule& above = rule(y + 1);
    if (above.support != Support::None || above.liquid)
    {
      continue;
    }
    if (rule(y - 1).walkableGround)
    {
      return static_cast<std::int16_t>(y);
    }
  }
  return NOT_WALKABLE;
}

void NavigationGrid::RebuildLinks(std::int32_t _minX, std::int32_t _minZ, std::int32_t _maxX, std::int32_t _maxZ)
{
  const auto passable = [this](std::int32_t _fromX, std::int32_t _fromZ, std::int32_t _toX, std::int32_t _toZ)
  {
    if (!Inside(_toX, _toZ))
    {
      return false;
    }
    const std::int16_t from = m_heights[Index(_fromX, _fromZ)];
    const std::int16_t to = m_heights[Index(_toX, _toZ)];
    return from != NOT_WALKABLE && to != NOT_WALKABLE && std::abs(from - to) <= m_desc.climbVoxels;
  };
  for (std::int32_t z = std::max(0, _minZ); z <= std::min(m_desc.depthCells - 1, _maxZ); ++z)
  {
    for (std::int32_t x = std::max(0, _minX); x <= std::min(m_desc.widthCells - 1, _maxX); ++x)
    {
      std::uint8_t links = 0;
      for (std::size_t direction = 0; direction < NEIGHBORS.size(); ++direction)
      {
        const auto [dx, dz] = NEIGHBORS[direction];
        bool linked = passable(x, z, x + dx, z + dz);
        if (linked && dx != 0 && dz != 0)
        {
          // No cutting a corner: both cells beside the diagonal must be passable on the way.
          linked = passable(x, z, x + dx, z) && passable(x, z, x, z + dz) && passable(x + dx, z, x + dx, z + dz) &&
                   passable(x, z + dz, x + dx, z + dz);
        }
        if (linked)
        {
          links = static_cast<std::uint8_t>(links | (1U << direction));
        }
      }
      m_links[Index(x, z)] = links;
    }
  }
}

bool NavigationGrid::Inside(std::int32_t _x, std::int32_t _z) const noexcept
{
  return _x >= 0 && _x < m_desc.widthCells && _z >= 0 && _z < m_desc.depthCells;
}

std::size_t NavigationGrid::Index(std::int32_t _x, std::int32_t _z) const noexcept
{
  return static_cast<std::size_t>(_z) * static_cast<std::size_t>(m_desc.widthCells) + static_cast<std::size_t>(_x);
}

} // namespace Simulation
