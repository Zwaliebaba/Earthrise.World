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
    m_materials(&_materials)
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
  std::int32_t run = 1;
  std::int32_t width = 1;
  for (const ClassRule& rule : m_classes)
  {
    if (rule.climbRunCells < 1 || rule.climbRunCells > TILE_EDGE_CELLS || rule.headroomVoxels < 2 || rule.headroomVoxels > UINT8_MAX ||
        rule.widthVoxels < 1 || rule.widthVoxels > TILE_EDGE_CELLS || rule.rubbleFactor < 1 || rule.fordFactor < 1)
    {
      throw std::invalid_argument("a navigation class needs a run, a headroom of 2 or more, a width and its factors");
    }
    run = std::max(run, rule.climbRunCells);
    width = std::max(width, rule.widthVoxels);
    headroom = std::max(headroom, rule.headroomVoxels);
  }
  m_headroomCap = static_cast<std::uint8_t>(headroom);
  m_clearanceCap = static_cast<std::uint8_t>(width);
  m_clearanceReach = width - 1;
  m_margin = run + m_clearanceReach;
  const std::size_t cells = static_cast<std::size_t>(m_desc.widthCells) * static_cast<std::size_t>(m_desc.depthCells);
  m_heights.assign(cells, NOT_WALKABLE);
  m_headroom.assign(cells, 0);
  m_ground.assign(cells, Ground::Plain);
  for (std::vector<std::uint8_t>& links : m_links)
  {
    links.assign(cells, 0);
  }
  for (std::vector<std::uint8_t>& clearance : m_clearance)
  {
    clearance.assign(cells, 0);
  }
  const auto tiles = static_cast<std::size_t>(TilesAcross()) * static_cast<std::size_t>(TilesDeep());
  for (std::size_t navigationClass = 0; navigationClass < NAVIGATION_CLASS_COUNT; ++navigationClass)
  {
    m_components.at(navigationClass).assign(cells, 0);
    m_componentCounts.at(navigationClass).assign(tiles, 0);
    m_componentOffsets.at(navigationClass).assign(tiles + 1, 0);
  }
  m_tileChanged.assign(tiles, 1);
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
  RebuildClearance(0, 0, m_desc.widthCells - 1, m_desc.depthCells - 1);
  RebuildLinks(0, 0, m_desc.widthCells - 1, m_desc.depthCells - 1);
  std::ranges::fill(m_tileChanged, 1);
  m_reachCurrent = false;
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
  // The cells whose squares reach into the tile redo their clearance, and the cells as far as the
  // margin, whose links read those, their links.
  RebuildClearance(minX - m_clearanceReach, minZ - m_clearanceReach, maxX, maxZ);
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

bool NavigationGrid::Standable(std::int32_t _x, std::int32_t _z, NavigationClass _class) const
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

std::uint8_t NavigationGrid::Clearance(std::int32_t _x, std::int32_t _z, NavigationClass _class) const
{
  if (!Inside(_x, _z))
  {
    return 0;
  }
  return m_clearance.at(static_cast<std::size_t>(_class))[Index(_x, _z)];
}

bool NavigationGrid::Usable(std::int32_t _x, std::int32_t _z, NavigationClass _class) const
{
  return Standable(_x, _z, _class) && Clearance(_x, _z, _class) >= Class(_class).widthVoxels;
}

const NavigationGrid::ClassRule& NavigationGrid::Class(NavigationClass _class) const noexcept
{
  return m_classes[static_cast<std::size_t>(_class)];
}

std::int32_t NavigationGrid::Factor(std::int32_t _x, std::int32_t _z, NavigationClass _class) const
{
  const ClassRule& rule = Class(_class);
  switch (GroundAt(_x, _z))
  {
  case Ground::Rubble:
    return rule.rubbleFactor;
  case Ground::Ford:
    return rule.fordFactor;
  case Ground::Plain:
    break;
  }
  return 1;
}

void NavigationGrid::UpdateReach()
{
  if (m_reachCurrent)
  {
    return;
  }
  for (std::int32_t tileZ = 0; tileZ < TilesDeep(); ++tileZ)
  {
    for (std::int32_t tileX = 0; tileX < TilesAcross(); ++tileX)
    {
      const std::size_t tile = TileIndex(tileX * TILE_EDGE_CELLS, tileZ * TILE_EDGE_CELLS);
      if (m_tileChanged[tile] == 0)
      {
        continue;
      }
      for (std::size_t navigationClass = 0; navigationClass < NAVIGATION_CLASS_COUNT; ++navigationClass)
      {
        LabelTile(navigationClass, tileX, tileZ);
      }
      m_tileChanged[tile] = 0;
    }
  }
  for (std::size_t navigationClass = 0; navigationClass < NAVIGATION_CLASS_COUNT; ++navigationClass)
  {
    const std::vector<std::uint16_t>& counts = m_componentCounts.at(navigationClass);
    std::vector<std::size_t>& offsets = m_componentOffsets.at(navigationClass);
    for (std::size_t tile = 0; tile < counts.size(); ++tile)
    {
      offsets[tile + 1] = offsets[tile] + counts[tile];
    }
  }
  m_reachCurrent = true;
}

void NavigationGrid::LabelTile(std::size_t _class, std::int32_t _tileX, std::int32_t _tileZ)
{
  // Components from 1, in cell order, each flooded along the class's links that stay in the tile.
  // A link is open only between cells both of which the class can enter, and the climb rule reads
  // the same both ways, so the links join cells symmetrically.
  std::vector<std::uint16_t>& components = m_components.at(_class);
  const std::vector<std::uint8_t>& links = m_links.at(_class);
  const NavigationClass which = CLASSES.at(_class);
  const std::int32_t minX = _tileX * TILE_EDGE_CELLS;
  const std::int32_t minZ = _tileZ * TILE_EDGE_CELLS;
  for (std::int32_t z = minZ; z < minZ + TILE_EDGE_CELLS; ++z)
  {
    for (std::int32_t x = minX; x < minX + TILE_EDGE_CELLS; ++x)
    {
      components[Index(x, z)] = 0;
    }
  }
  std::uint16_t next = 1;
  std::vector<std::pair<std::int32_t, std::int32_t>> open;
  for (std::int32_t z = minZ; z < minZ + TILE_EDGE_CELLS; ++z)
  {
    for (std::int32_t x = minX; x < minX + TILE_EDGE_CELLS; ++x)
    {
      if (components[Index(x, z)] != 0 || !Usable(x, z, which))
      {
        continue;
      }
      components[Index(x, z)] = next;
      open.assign(1, {x, z});
      while (!open.empty())
      {
        const auto [fromX, fromZ] = open.back();
        open.pop_back();
        const std::uint8_t from = links[Index(fromX, fromZ)];
        for (std::size_t direction = 0; direction < NEIGHBORS.size(); ++direction)
        {
          const std::int32_t toX = fromX + NEIGHBORS.at(direction)[0];
          const std::int32_t toZ = fromZ + NEIGHBORS.at(direction)[1];
          if ((from & (1U << direction)) == 0 || TileOf(toX) != _tileX || TileOf(toZ) != _tileZ || components[Index(toX, toZ)] != 0)
          {
            continue;
          }
          components[Index(toX, toZ)] = next;
          open.emplace_back(toX, toZ);
        }
      }
      ++next;
    }
  }
  m_componentCounts.at(_class)[TileIndex(minX, minZ)] = static_cast<std::uint16_t>(next - 1);
}

std::int64_t NavigationGrid::NearestReachable(std::int64_t _start, std::int64_t _goal, NavigationClass _class) const
{
  if (!m_reachCurrent)
  {
    throw std::logic_error("reach is asked for before UpdateReach has caught up with the grid's rebuilds");
  }
  const std::int32_t width = m_desc.widthCells;
  const auto startX = static_cast<std::int32_t>(_start % width);
  const auto startZ = static_cast<std::int32_t>(_start / width);
  const auto goalX = static_cast<std::int32_t>(_goal % width);
  const auto goalZ = static_cast<std::int32_t>(_goal / width);
  if (!Inside(startX, startZ) || !Inside(goalX, goalZ))
  {
    throw std::out_of_range("reach between cells outside the grid");
  }
  const auto navigationClass = static_cast<std::size_t>(_class);
  const std::vector<std::uint16_t>& components = m_components.at(navigationClass);
  const std::vector<std::uint8_t>& links = m_links.at(navigationClass);
  const std::vector<std::size_t>& offsets = m_componentOffsets.at(navigationClass);
  if (components[Index(startX, startZ)] == 0)
  {
    return _start;
  }
  // Each component is numbered across the whole grid by its tile's offset; a walk from the start's
  // marks every one its tile's border links join it to, until the goal's is marked or none is left.
  const auto numberOf = [&](std::int32_t _x, std::int32_t _z) { return offsets[TileIndex(_x, _z)] + components[Index(_x, _z)] - 1; };
  std::vector<std::uint8_t> reached(offsets.back(), 0);
  std::vector<std::uint8_t> tileReached(m_tileChanged.size(), 0);
  std::vector<std::tuple<std::int32_t, std::int32_t, std::uint16_t>> open; // tile x, tile z, component
  const std::size_t goalNumber = components[Index(goalX, goalZ)] == 0 ? SIZE_MAX : numberOf(goalX, goalZ);
  reached[numberOf(startX, startZ)] = 1;
  tileReached[TileIndex(startX, startZ)] = 1;
  open.emplace_back(TileOf(startX), TileOf(startZ), components[Index(startX, startZ)]);
  while (!open.empty())
  {
    if (goalNumber != SIZE_MAX && reached[goalNumber] != 0)
    {
      return _goal;
    }
    const auto [tileX, tileZ, component] = open.back();
    open.pop_back();
    const std::int32_t minX = tileX * TILE_EDGE_CELLS;
    const std::int32_t minZ = tileZ * TILE_EDGE_CELLS;
    const std::int32_t maxX = minX + TILE_EDGE_CELLS - 1;
    const std::int32_t maxZ = minZ + TILE_EDGE_CELLS - 1;
    for (std::int32_t z = minZ; z <= maxZ; ++z)
    {
      // The tile's border: its first and last rows whole, and the two end cells of every other.
      const bool edgeRow = z == minZ || z == maxZ;
      for (std::int32_t x = minX; x <= maxX; x += edgeRow ? 1 : TILE_EDGE_CELLS - 1)
      {
        if (components[Index(x, z)] != component)
        {
          continue;
        }
        const std::uint8_t from = links[Index(x, z)];
        for (std::size_t direction = 0; direction < NEIGHBORS.size(); ++direction)
        {
          const std::int32_t toX = x + NEIGHBORS.at(direction)[0];
          const std::int32_t toZ = z + NEIGHBORS.at(direction)[1];
          if ((from & (1U << direction)) == 0 || (TileOf(toX) == tileX && TileOf(toZ) == tileZ))
          {
            continue;
          }
          const std::size_t number = numberOf(toX, toZ);
          if (reached[number] == 0)
          {
            reached[number] = 1;
            tileReached[TileIndex(toX, toZ)] = 1;
            open.emplace_back(TileOf(toX), TileOf(toZ), components[Index(toX, toZ)]);
          }
        }
      }
    }
  }
  if (goalNumber != SIZE_MAX && reached[goalNumber] != 0)
  {
    return _goal;
  }
  // The goal is out of reach. The tiles reached, nearest the goal first by the octile distance to
  // their nearest cell, which no cell of theirs can beat; the scan stops at the first tile that
  // cannot reach the best found, and an equal distance is still scanned for the lower index.
  const auto octile = [&](std::int32_t _dx, std::int32_t _dz)
  {
    const std::int64_t dx = std::abs(_dx);
    const std::int64_t dz = std::abs(_dz);
    return 10 * std::max(dx, dz) + 4 * std::min(dx, dz);
  };
  std::vector<std::tuple<std::int64_t, std::int32_t, std::int32_t>> tiles; // bound, tile z, tile x
  for (std::int32_t tileZ = 0; tileZ < TilesDeep(); ++tileZ)
  {
    for (std::int32_t tileX = 0; tileX < TilesAcross(); ++tileX)
    {
      if (tileReached[TileIndex(tileX * TILE_EDGE_CELLS, tileZ * TILE_EDGE_CELLS)] == 0)
      {
        continue;
      }
      const std::int32_t minX = tileX * TILE_EDGE_CELLS;
      const std::int32_t minZ = tileZ * TILE_EDGE_CELLS;
      const std::int32_t dx = std::max({0, minX - goalX, goalX - (minX + TILE_EDGE_CELLS - 1)});
      const std::int32_t dz = std::max({0, minZ - goalZ, goalZ - (minZ + TILE_EDGE_CELLS - 1)});
      tiles.emplace_back(octile(dx, dz), tileZ, tileX);
    }
  }
  std::ranges::sort(tiles);
  std::int64_t best = _start;
  std::int64_t bestDistance = octile(startX - goalX, startZ - goalZ);
  for (const auto& [bound, tileZ, tileX] : tiles)
  {
    if (bound > bestDistance)
    {
      break;
    }
    for (std::int32_t z = tileZ * TILE_EDGE_CELLS; z < (tileZ + 1) * TILE_EDGE_CELLS; ++z)
    {
      for (std::int32_t x = tileX * TILE_EDGE_CELLS; x < (tileX + 1) * TILE_EDGE_CELLS; ++x)
      {
        if (components[Index(x, z)] == 0 || reached[numberOf(x, z)] == 0)
        {
          continue;
        }
        const std::int64_t distance = octile(x - goalX, z - goalZ);
        const std::int64_t cell = std::int64_t{z} * width + x;
        if (distance < bestDistance || (distance == bestDistance && cell < best))
        {
          best = cell;
          bestDistance = distance;
        }
      }
    }
  }
  return best;
}

std::size_t NavigationGrid::Components(NavigationClass _class) const
{
  return m_componentOffsets.at(static_cast<std::size_t>(_class)).back();
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
  // The column's palette indices, read a chunk at a time rather than a voxel at a time.
  m_column.assign(static_cast<std::size_t>(m_desc.heightVoxels) + 1, Protocol::Chunk::EMPTY);
  const Protocol::VoxelPosition inside = Protocol::PositionInChunk({.xVoxels = _x, .yVoxels = 0, .zVoxels = _z});
  const Protocol::ChunkCoordinate base = Protocol::ChunkContaining({.xVoxels = _x, .yVoxels = 0, .zVoxels = _z});
  for (std::int32_t chunkY = 0; chunkY * Protocol::CHUNK_EDGE_VOXELS < m_desc.heightVoxels; ++chunkY)
  {
    const Protocol::Chunk* chunk =
      _store.Find(Protocol::PackChunkKey({.xChunks = base.xChunks, .yChunks = chunkY, .zChunks = base.zChunks}));
    if (chunk == nullptr)
    {
      continue;
    }
    for (std::int32_t y = 0; y < Protocol::CHUNK_EDGE_VOXELS && chunkY * Protocol::CHUNK_EDGE_VOXELS + y < m_desc.heightVoxels; ++y)
    {
      const std::int32_t height = chunkY * Protocol::CHUNK_EDGE_VOXELS + y;
      m_column[static_cast<std::size_t>(height)] = chunk->At(inside.xVoxels, y, inside.zVoxels);
    }
  }
  const auto rule = [this](std::int32_t _y) -> const MaterialRule&
  { return (*m_materials)[_y >= m_desc.heightVoxels ? Protocol::Chunk::EMPTY : m_column[static_cast<std::size_t>(_y)]]; };
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

void NavigationGrid::RebuildClearance(std::int32_t _minX, std::int32_t _minZ, std::int32_t _maxX, std::int32_t _maxZ)
{
  for (std::size_t navigationClass = 0; navigationClass < NAVIGATION_CLASS_COUNT; ++navigationClass)
  {
    const NavigationClass which = CLASSES.at(navigationClass);
    std::vector<std::uint8_t>& clearance = m_clearance.at(navigationClass);
    for (std::int32_t z = std::max(0, _minZ); z <= std::min(m_desc.depthCells - 1, _maxZ); ++z)
    {
      for (std::int32_t x = std::max(0, _minX); x <= std::min(m_desc.widthCells - 1, _maxX); ++x)
      {
        // The widest square, from 1 up to the cap, whose every cell the class stands in within 1 m
        // of this cell's height: each step adds the square's next row and column.
        std::uint8_t width = 0;
        const std::int16_t height = m_heights[Index(x, z)];
        while (width < m_clearanceCap)
        {
          const std::int32_t edge = width;
          bool fits = true;
          for (std::int32_t step = 0; step <= edge && fits; ++step)
          {
            for (const auto& [cellX, cellZ] : {std::pair{x + edge, z + step}, std::pair{x + step, z + edge}})
            {
              fits = fits && Standable(cellX, cellZ, which) && std::abs(m_heights[Index(cellX, cellZ)] - height) <= m_desc.climbVoxels;
            }
          }
          if (!fits)
          {
            break;
          }
          ++width;
        }
        if (clearance[Index(x, z)] != width)
        {
          clearance[Index(x, z)] = width;
          MarkChanged(x, z);
        }
      }
    }
  }
}

void NavigationGrid::RebuildLinks(std::int32_t _minX, std::int32_t _minZ, std::int32_t _maxX, std::int32_t _maxZ)
{
  const std::int32_t lowX = std::max(0, _minX);
  const std::int32_t lowZ = std::max(0, _minZ);
  const std::int32_t highX = std::min(m_desc.widthCells - 1, _maxX);
  const std::int32_t highZ = std::min(m_desc.depthCells - 1, _maxZ);
  if (lowX > highX || lowZ > highZ)
  {
    return;
  }
  // The straight links first, over the region and a ring about it, so each diagonal's corner checks
  // read them rather than work them out again.
  const std::int32_t ringLowX = lowX - 1;
  const std::int32_t ringLowZ = lowZ - 1;
  const std::int32_t ringWidth = highX - lowX + 3;
  const std::int32_t ringDepth = highZ - lowZ + 3;
  const auto ringIndex = [&](std::int32_t _x, std::int32_t _z)
  { return static_cast<std::size_t>(_z - ringLowZ) * static_cast<std::size_t>(ringWidth) + static_cast<std::size_t>(_x - ringLowX); };
  for (std::size_t navigationClass = 0; navigationClass < NAVIGATION_CLASS_COUNT; ++navigationClass)
  {
    const ClassRule& rule = m_classes.at(navigationClass);
    m_straight.assign(static_cast<std::size_t>(ringWidth) * static_cast<std::size_t>(ringDepth), 0);
    for (std::int32_t z = ringLowZ; z < ringLowZ + ringDepth; ++z)
    {
      for (std::int32_t x = ringLowX; x < ringLowX + ringWidth; ++x)
      {
        if (!Inside(x, z))
        {
          continue;
        }
        std::uint8_t straight = 0;
        for (std::size_t direction = 0; direction < NEIGHBORS.size(); direction += 2)
        {
          if (StraightOpen(x, z, NEIGHBORS.at(direction)[0], NEIGHBORS.at(direction)[1], rule, navigationClass))
          {
            straight = static_cast<std::uint8_t>(straight | (1U << direction));
          }
        }
        m_straight[ringIndex(x, z)] = straight;
      }
    }
    const auto straightOpen = [&](std::int32_t _x, std::int32_t _z, std::uint32_t _bit)
    { return Inside(_x, _z) && (m_straight[ringIndex(_x, _z)] & _bit) != 0; };
    std::vector<std::uint8_t>& classLinks = m_links.at(navigationClass);
    for (std::int32_t z = lowZ; z <= highZ; ++z)
    {
      for (std::int32_t x = lowX; x <= highX; ++x)
      {
        std::uint8_t links = m_straight[ringIndex(x, z)];
        for (std::size_t direction = 1; direction < NEIGHBORS.size(); direction += 2)
        {
          const auto [dx, dz] = NEIGHBORS.at(direction);
          // No cutting a corner: the four straight steps around the diagonal must be open too.
          const std::uint32_t alongX = dx > 0 ? East : West;
          const std::uint32_t alongZ = dz > 0 ? North : South;
          const bool corners =
            straightOpen(x, z, alongX) && straightOpen(x, z, alongZ) && straightOpen(x + dx, z, alongZ) && straightOpen(x, z + dz, alongX);
          if (corners && StraightOpen(x, z, dx, dz, rule, navigationClass))
          {
            links = static_cast<std::uint8_t>(links | (1U << direction));
          }
        }
        if (classLinks[Index(x, z)] != links)
        {
          classLinks[Index(x, z)] = links;
          MarkChanged(x, z);
        }
      }
    }
  }
}

void NavigationGrid::MarkChanged(std::int32_t _x, std::int32_t _z)
{
  // A cell's component reads only its links and whether the class can enter it, which is its
  // clearance against the class's width, so a tile in which neither changed keeps its components.
  m_tileChanged[TileIndex(_x, _z)] = 1;
  m_reachCurrent = false;
}

bool NavigationGrid::Inside(std::int32_t _x, std::int32_t _z) const noexcept
{
  return _x >= 0 && _x < m_desc.widthCells && _z >= 0 && _z < m_desc.depthCells;
}

std::size_t NavigationGrid::Index(std::int32_t _x, std::int32_t _z) const noexcept
{
  return static_cast<std::size_t>(_z) * static_cast<std::size_t>(m_desc.widthCells) + static_cast<std::size_t>(_x);
}

std::size_t NavigationGrid::TileIndex(std::int32_t _x, std::int32_t _z) const noexcept
{
  return static_cast<std::size_t>(TileOf(_z)) * static_cast<std::size_t>(TilesAcross()) + static_cast<std::size_t>(TileOf(_x));
}

} // namespace Simulation
