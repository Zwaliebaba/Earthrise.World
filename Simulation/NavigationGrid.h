#pragma once

#include "ChunkCoordinate.h"
#include "ChunkStore.h"
#include "MaterialRules.h"
#include "Rules.h"

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace Simulation
{

/// How a unit moves: by its climb, headroom, width and fords (ADR-015, decision 2).
enum class NavigationClass : std::uint8_t
{
  Foot,
  Cavalry,
  Machines
};

inline constexpr std::size_t NAVIGATION_CLASS_COUNT = 3;

/// Where units can walk: a grid of 1 m cells in tiles of 32 x 32, one tile per chunk column (the
/// owner's choice of 2026-11-01; GameDesign.md, *A breach is a path*; ADR-015).
///
/// Each cell holds its column's one walkable surface, the highest cell with walkable ground under
/// it and two cells clear above it, of which only the lower may be water (a ford); the clear cells
/// above that surface, its headroom, up to the tallest class's need; and its ground: plain, a ford
/// or rubble. A structure is never ground, so nobody stands on a wall, and the space under an
/// overhang is never a column's surface (ADR-015, decision 1).
///
/// Each class has its own links to the eight neighbours (decision 2). A class stands where its
/// headroom fits and its fords allow. It climbs at most climbVoxels over a run of its climbRunCells
/// cells: a link is open when, along the link's line, the walkable ground rises or falls by at most
/// that over every run of climbRunCells steps that includes the link. A diagonal is open only when
/// the four straight links around it are, so no class cuts a corner. An edit rebuilds the tiles its
/// chunks touch, as far as the classes' margins reach (decision 4).
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

  /// What a cell's surface stands on, or in.
  enum class Ground : std::uint8_t
  {
    Plain,
    Ford,  // one voxel of water
    Rubble // the ground under it is rubble
  };

  /// One class's rule (ADR-015, decision 2), from RULE (GameDesign.md, M2's values, item 10).
  struct ClassRule
  {
    std::int32_t climbRunCells;
    std::int32_t headroomVoxels;
    std::int32_t widthVoxels;
    std::int32_t fordDepthVoxels;
    std::int32_t rubbleFactor; // how much rubble slows it, and costs its search
    std::int32_t fordFactor;
  };

  struct Desc
  {
    std::int32_t widthCells; // along x, from 0; a whole number of tiles
    std::int32_t depthCells; // along z, from 0
    std::int32_t heightVoxels;
    /// How far a class climbs over its run: 1 m, the slope of settled earth for foot soldiers, who
    /// climb it in each 1 m step (GameDesign.md, the paper tests' values, item 7).
    std::int32_t climbVoxels;
    /// The classes, from RULE. Without them every class walks as foot soldiers do, as M1's grid did.
    const Rules* rules = nullptr;
  };

  /// Starts with nothing walkable. Throws std::invalid_argument for a size that is not a whole
  /// number of tiles.
  NavigationGrid(const Desc& _desc, const MaterialRules& _materials);

  /// Rebuilds every tile.
  void RebuildAll(const ChunkStore& _store);

  /// Rebuilds one tile's cells, and the links of the cells around it, as far as the margin, that
  /// read them.
  void RebuildTile(const ChunkStore& _store, std::int32_t _tileX, std::int32_t _tileZ);

  /// The tiles whose cells or links _edits can change: those within the margin of each edited
  /// column. Each tile once, in z-then-x order.
  [[nodiscard]] std::vector<std::pair<std::int32_t, std::int32_t>> TilesTouchedBy(std::span<const Protocol::VoxelPosition> _edits) const;

  /// The height of the cell a unit stands in at a column, or NOT_WALKABLE.
  [[nodiscard]] std::int16_t WalkableHeight(std::int32_t _x, std::int32_t _z) const;

  /// Foot soldiers' links, as M1's grid gave them.
  [[nodiscard]] std::uint8_t Links(std::int32_t _x, std::int32_t _z) const;
  [[nodiscard]] std::uint8_t Links(std::int32_t _x, std::int32_t _z, NavigationClass _class) const;

  /// The clear cells above the surface, up to the tallest class's headroom; 0 where nothing walks.
  [[nodiscard]] std::uint8_t Headroom(std::int32_t _x, std::int32_t _z) const;
  [[nodiscard]] Ground GroundAt(std::int32_t _x, std::int32_t _z) const;

  /// Whether a class can stand in a cell: it is walkable, its headroom fits and its ford allows.
  [[nodiscard]] bool Usable(std::int32_t _x, std::int32_t _z, NavigationClass _class) const;

  [[nodiscard]] const ClassRule& Class(NavigationClass _class) const noexcept;

  /// How far a cell's links reach to the cells they read: the longest climb run, or the widest
  /// class's clearance square, whichever is larger.
  [[nodiscard]] std::int32_t MarginCells() const noexcept;

  [[nodiscard]] std::int32_t TilesAcross() const noexcept;
  [[nodiscard]] std::int32_t TilesDeep() const noexcept;
  [[nodiscard]] std::int32_t WidthCells() const noexcept;
  [[nodiscard]] std::int32_t DepthCells() const noexcept;

  /// The offsets of the eight neighbours, in the order of the link bits.
  static constexpr std::array<std::array<std::int32_t, 2>, 8> NEIGHBORS = {
    {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}}};

private:
  void RebuildCell(const ChunkStore& _store, std::int32_t _x, std::int32_t _z);
  void RebuildLinks(std::int32_t _minX, std::int32_t _minZ, std::int32_t _maxX, std::int32_t _maxZ);
  [[nodiscard]] bool StraightOpen(std::int32_t _x, std::int32_t _z, std::int32_t _dx, std::int32_t _dz, const ClassRule& _rule,
                                  std::size_t _class) const;
  [[nodiscard]] bool Inside(std::int32_t _x, std::int32_t _z) const noexcept;
  [[nodiscard]] std::size_t Index(std::int32_t _x, std::int32_t _z) const noexcept;

  Desc m_desc;
  const MaterialRules& m_materials;
  std::array<ClassRule, NAVIGATION_CLASS_COUNT> m_classes{};
  std::int32_t m_margin = 1;
  std::uint8_t m_headroomCap = 2;
  std::vector<std::int16_t> m_heights;
  std::vector<std::uint8_t> m_headroom;
  std::vector<Ground> m_ground;
  std::array<std::vector<std::uint8_t>, NAVIGATION_CLASS_COUNT> m_links;
};

} // namespace Simulation
