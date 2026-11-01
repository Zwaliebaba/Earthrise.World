#pragma once

#include "ChunkCoordinate.h"
#include "ChunkStore.h"
#include "MaterialRules.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace Simulation
{

/// Where units can walk: a grid of 1 m cells in tiles of 32 x 32, one tile per chunk column (the
/// owner's choice of 2026-11-01; GameDesign.md, *A breach is a path*). Each cell holds its column's
/// one walkable surface, the highest cell a unit fits in, 1 m wide and 2 m high, standing on ground
/// rather than on a structure, and its links to its eight neighbours. An edit rebuilds the tiles its
/// chunks touch.
class NavigationGrid
{
public:
  static constexpr std::int32_t TILE_EDGE_CELLS = Protocol::CHUNK_EDGE_VOXELS;
  static constexpr std::int16_t NOT_WALKABLE = -1;

  /// The link bits, one for each neighbour.
  enum Link : std::uint8_t
  {
    North = 1U << 0, // +z
    NorthEast = 1U << 1,
    East = 1U << 2, // +x
    SouthEast = 1U << 3,
    South = 1U << 4,
    SouthWest = 1U << 5,
    West = 1U << 6,
    NorthWest = 1U << 7
  };

  struct Desc
  {
    std::int32_t widthCells; // along x, from 0; a whole number of tiles
    std::int32_t depthCells; // along z, from 0
    std::int32_t heightVoxels;
    /// How far a unit climbs in one step to a neighbour: foot soldiers climb 1 m in each 1 m step,
    /// the slope of settled earth (GameDesign.md, the paper tests' values, item 7).
    std::int32_t climbVoxels;
  };

  /// Starts with nothing walkable. Throws std::invalid_argument for a size that is not a whole
  /// number of tiles.
  NavigationGrid(const Desc& _desc, const MaterialRules& _materials);

  /// Rebuilds every tile.
  void RebuildAll(const ChunkStore& _store);

  /// Rebuilds one tile's cells, and the links of the cells around it that point into it.
  void RebuildTile(const ChunkStore& _store, std::int32_t _tileX, std::int32_t _tileZ);

  /// The tiles whose cells _edits can change: each edited column's tile, and its neighbour's where
  /// the column lies on the tile's edge, since a neighbour's links read the column. Each tile once,
  /// in x-then-z order.
  [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> TilesTouchedBy(std::span<const Protocol::VoxelPosition> _edits) const;

  /// The height of the cell a unit stands in at a column, or NOT_WALKABLE.
  [[nodiscard]] std::int16_t WalkableHeight(std::int32_t _x, std::int32_t _z) const;
  [[nodiscard]] std::uint8_t Links(std::int32_t _x, std::int32_t _z) const;

  [[nodiscard]] std::int32_t TilesAcross() const noexcept;
  [[nodiscard]] std::int32_t TilesDeep() const noexcept;

private:
  [[nodiscard]] std::int16_t ColumnHeight(const ChunkStore& _store, std::int32_t _x, std::int32_t _z) const;
  void RebuildLinks(std::int32_t _minX, std::int32_t _minZ, std::int32_t _maxX, std::int32_t _maxZ);
  [[nodiscard]] bool Inside(std::int32_t _x, std::int32_t _z) const noexcept;
  [[nodiscard]] std::size_t Index(std::int32_t _x, std::int32_t _z) const noexcept;

  Desc m_desc;
  const MaterialRules& m_materials;
  std::vector<std::int16_t> m_heights;
  std::vector<std::uint8_t> m_links;
};

} // namespace Simulation
