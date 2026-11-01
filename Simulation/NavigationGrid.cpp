#include "pch.h"

#include "NavigationGrid.h"

#include "IntegerMath.h"

namespace Simulation
{

namespace
{

constexpr std::array<NavigationClass, NAVIGATION_CLASS_COUNT> CLASSES = {NavigationClass::Foot, NavigationClass::Cavalry,
                                                                         NavigationClass::Machines};

std::int32_t TileOf(std::int32_t _cell) noexcept
{
  return static_cast<std::int32_t>(Core::FloorDivide(_cell, NavigationGrid::TILE_EDGE_CELLS));
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
  if (m_desc.rules == nullptr)
  {
    // M1's grid: every class walks as foot soldiers do.
    m_classes.fill({.climbRunCells = 1, .headroomVoxels = 2, .widthVoxels = 1, .fordDepthVoxels = 1, .rubbleFactor = 1, .fordFactor = 1});
  }
  else
  {
    const Rules& rules = *m_desc.rules;
    m_classes = {{{.climbRunCells = rules.footClimbRunCells,
                   .headroomVoxels = rules.footHeadroomVoxels,
                   .widthVoxels = rules.footWidthVoxels,
                   .fordDepthVoxels = rules.footFordDepthVoxels,
                   .rubbleFactor = rules.footRubbleFactor,
                   .fordFactor = rules.footFordFactor},
                  {.climbRunCells = rules.cavalryClimbRunCells,
                   .headroomVoxels = rules.cavalryHeadroomVoxels,
                   .widthVoxels = rules.cavalryWidthVoxels,
                   .fordDepthVoxels = rules.cavalryFordDepthVoxels,
                   .rubbleFactor = rules.cavalryRubbleFactor,
                   .fordFactor = rules.cavalryFordFactor},
                  {.climbRunCells = rules.machinesClimbRunCells,
                   .headroomVoxels = rules.machinesHeadroomVoxels,
                   .widthVoxels = rules.machinesWidthVoxels,
                   .fordDepthVoxels = rules.machinesFordDepthVoxels,
                   .rubbleFactor = rules.machinesRubbleFactor,
                   .fordFactor = rules.machinesFordFactor}}};
  }
  std::int32_t headroom = 2;
  for (const ClassRule& rule : m_classes)
  {
    if (rule.climbRunCells < 1 || rule.climbRunCells > TILE_EDGE_CELLS || rule.headroomVoxels < 2 || rule.headroomVoxels > UINT8_MAX ||
        rule.widthVoxels < 1 || rule.widthVoxels > TILE_EDGE_CELLS || rule.rubbleFactor < 1 || rule.fordFactor < 1)
    {
      throw std::invalid_argument("a navigation class needs a run, a headroom of 2 or more, a width and its factors");
    }
    m_margin = std::max({m_margin, rule.climbRunCells, rule.widthVoxels - 1});
    headroom = std::max(headroom, rule.headroomVoxels);
  }
  m_headroomCap = static_cast<std::uint8_t>(headroom);
  const std::size_t cells = static_cast<std::size_t>(m_desc.widthCells) * static_cast<std::size_t>(m_desc.depthCells);
  m_heights.assign(cells, NOT_WALKABLE);
  m_headroom.assign(cells, 0);
  m_ground.assign(cells, Ground::Plain);
  for (std::vector<std::uint8_t>& links : m_links)
  {
    links.assign(cells, 0);
  }
}

void NavigationGrid::RebuildAll(const ChunkStore& _store)
{
  for (std::int32_t z = 0; z < m_desc.depthCells; ++z)
  {
    for (std::int32_t x = 0; x < m_desc.widthCells; ++x)
    {
      RebuildCell(_store, x, z);
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
      RebuildCell(_store, x, z);
    }
  }
  // The cells around the tile, as far as the margin, read its cells, so their links are redone too.
  RebuildLinks(minX - m_margin, minZ - m_margin, maxX + m_margin, maxZ + m_margin);
}

std::vector<std::pair<std::int32_t, std::int32_t>> NavigationGrid::TilesTouchedBy(std::span<const Protocol::VoxelPosition> _edits) const
{
  std::vector<std::pair<std::int32_t, std::int32_t>> tiles;
  for (const Protocol::VoxelPosition& edit : _edits)
  {
    // The column's tile, and any tile within the margin, whose links read the column.
    const std::int32_t lowX = std::max(0, TileOf(edit.xVoxels - m_margin));
    const std::int32_t highX = std::min(TilesAcross() - 1, TileOf(edit.xVoxels + m_margin));
    const std::int32_t lowZ = std::max(0, TileOf(edit.zVoxels - m_margin));
    const std::int32_t highZ = std::min(TilesDeep() - 1, TileOf(edit.zVoxels + m_margin));
    for (std::int32_t tileZ = lowZ; tileZ <= highZ; ++tileZ)
    {
      for (std::int32_t tileX = lowX; tileX <= highX; ++tileX)
      {
        tiles.emplace_back(tileX, tileZ);
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
  return Links(_x, _z, NavigationClass::Foot);
}

std::uint8_t NavigationGrid::Links(std::int32_t _x, std::int32_t _z, NavigationClass _class) const
{
  if (!Inside(_x, _z))
  {
    throw std::out_of_range("a navigation cell outside the grid");
  }
  return m_links.at(static_cast<std::size_t>(_class))[Index(_x, _z)];
}

std::uint8_t NavigationGrid::Headroom(std::int32_t _x, std::int32_t _z) const
{
  if (!Inside(_x, _z))
  {
    throw std::out_of_range("a navigation cell outside the grid");
  }
  return m_headroom[Index(_x, _z)];
}

NavigationGrid::Ground NavigationGrid::GroundAt(std::int32_t _x, std::int32_t _z) const
{
  if (!Inside(_x, _z))
  {
    throw std::out_of_range("a navigation cell outside the grid");
  }
  return m_ground[Index(_x, _z)];
}

bool NavigationGrid::Usable(std::int32_t _x, std::int32_t _z, NavigationClass _class) const
{
  if (!Inside(_x, _z))
  {
    return false;
  }
  const std::size_t at = Index(_x, _z);
  const ClassRule& rule = m_classes.at(static_cast<std::size_t>(_class));
  return m_heights[at] != NOT_WALKABLE && m_headroom[at] >= rule.headroomVoxels &&
         (m_ground[at] != Ground::Ford || rule.fordDepthVoxels >= 1);
}

const NavigationGrid::ClassRule& NavigationGrid::Class(NavigationClass _class) const noexcept
{
  return m_classes[static_cast<std::size_t>(_class)];
}

std::int32_t NavigationGrid::MarginCells() const noexcept
{
  return m_margin;
}

std::int32_t NavigationGrid::TilesAcross() const noexcept
{
  return m_desc.widthCells / TILE_EDGE_CELLS;
}

std::int32_t NavigationGrid::TilesDeep() const noexcept
{
  return m_desc.depthCells / TILE_EDGE_CELLS;
}

std::int32_t NavigationGrid::WidthCells() const noexcept
{
  return m_desc.widthCells;
}

std::int32_t NavigationGrid::DepthCells() const noexcept
{
  return m_desc.depthCells;
}

void NavigationGrid::RebuildCell(const ChunkStore& _store, std::int32_t _x, std::int32_t _z)
{
  // The highest cell with ground under it and room for a unit: two cells clear, of which only the
  // lower may be water (a ford). A structure is never ground, so the top of a wall is not walkable,
  // while a gate's passage under its arch is, and the space under a rock overhang is not the
  // column's surface, because the rock's top is higher (GameDesign.md, *A breach is a path*).
  const auto rule = [&](std::int32_t _y) -> const MaterialRule&
  { return m_materials[_y >= m_desc.heightVoxels ? 0 : _store.At({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z})]; };
  const std::size_t at = Index(_x, _z);
  m_heights[at] = NOT_WALKABLE;
  m_headroom[at] = 0;
  m_ground[at] = Ground::Plain;
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
    const MaterialRule& below = rule(y - 1);
    if (!below.walkableGround)
    {
      continue;
    }
    m_heights[at] = static_cast<std::int16_t>(y);
    std::uint8_t clear = 0;
    while (clear < m_headroomCap && rule(y + clear).support == Support::None)
    {
      ++clear;
    }
    m_headroom[at] = clear;
    if (cell.liquid)
    {
      m_ground[at] = Ground::Ford;
    }
    else if (below.behavior == Behavior::StoneRubble || below.behavior == Behavior::WasteRubble)
    {
      m_ground[at] = Ground::Rubble;
    }
    return;
  }
}

bool NavigationGrid::StraightOpen(std::int32_t _x, std::int32_t _z, std::int32_t _dx, std::int32_t _dz, const ClassRule& _rule,
                                  std::size_t _class) const
{
  // From (_x, _z) one step along (_dx, _dz): both ends usable, and every run of climbRunCells steps
  // along the line that includes the step rises or falls by at most climbVoxels over its walkable
  // ground (ADR-015, decision 2).
  const auto navigationClass = CLASSES.at(_class);
  if (!Usable(_x, _z, navigationClass) || !Usable(_x + _dx, _z + _dz, navigationClass))
  {
    return false;
  }
  const std::int32_t run = _rule.climbRunCells;
  for (std::int32_t start = 1 - run; start <= 0; ++start)
  {
    std::int32_t lowest = INT32_MAX;
    std::int32_t highest = INT32_MIN;
    for (std::int32_t step = start; step <= start + run; ++step)
    {
      const std::int32_t x = _x + step * _dx;
      const std::int32_t z = _z + step * _dz;
      if (!Inside(x, z) || m_heights[Index(x, z)] == NOT_WALKABLE)
      {
        continue;
      }
      lowest = std::min<std::int32_t>(lowest, m_heights[Index(x, z)]);
      highest = std::max<std::int32_t>(highest, m_heights[Index(x, z)]);
    }
    if (highest - lowest > m_desc.climbVoxels)
    {
      return false;
    }
  }
  return true;
}

void NavigationGrid::RebuildLinks(std::int32_t _minX, std::int32_t _minZ, std::int32_t _maxX, std::int32_t _maxZ)
{
  for (std::size_t navigationClass = 0; navigationClass < NAVIGATION_CLASS_COUNT; ++navigationClass)
  {
    const ClassRule& rule = m_classes.at(navigationClass);
    std::vector<std::uint8_t>& classLinks = m_links.at(navigationClass);
    for (std::int32_t z = std::max(0, _minZ); z <= std::min(m_desc.depthCells - 1, _maxZ); ++z)
    {
      for (std::int32_t x = std::max(0, _minX); x <= std::min(m_desc.widthCells - 1, _maxX); ++x)
      {
        std::uint8_t links = 0;
        for (std::size_t direction = 0; direction < NEIGHBORS.size(); ++direction)
        {
          const auto [dx, dz] = NEIGHBORS.at(direction);
          bool linked = StraightOpen(x, z, dx, dz, rule, navigationClass);
          if (linked && dx != 0 && dz != 0)
          {
            // No cutting a corner: the four straight steps around the diagonal must be open too.
            linked = StraightOpen(x, z, dx, 0, rule, navigationClass) && StraightOpen(x, z, 0, dz, rule, navigationClass) &&
                     StraightOpen(x + dx, z, 0, dz, rule, navigationClass) && StraightOpen(x, z + dz, dx, 0, rule, navigationClass);
          }
          if (linked)
          {
            links = static_cast<std::uint8_t>(links | (1U << direction));
          }
        }
        classLinks[Index(x, z)] = links;
      }
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
