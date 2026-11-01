#pragma once

#include "Chunk.h"
#include "ChunkCoordinate.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace Simulation
{

/// The version of what the generator makes. Its output for a version never changes once a world
/// has to survive an update (ADR-003, decision 4); until then there is one version, and it may.
inline constexpr std::uint32_t GENERATOR_VERSION = 1;

/// Any voxel this many or more below the generated surface is an anchor, as bedrock is (ADR-004,
/// decision 2). A depth the generator fixes, so it counts as the generator's (ADR-016).
inline constexpr std::int32_t ANCHOR_DEPTH_VOXELS = 8;

/// The MVP map's generator (ADR-003; GameDesign.md, M2's values, item 3): integer-only, a pure
/// function of the two seeds and the chunk coordinate.
///
/// The constructor computes the world-scale layer from the seeds alone. From the world seed: the
/// relief and soil depths of M1's test ground carried over the map, a bedrock depth that varies so
/// that thin soil over rock and deep earth both occur, one river with one-voxel fords, and forest.
/// From the resource seed: sparse outcrops of building stone, which take the place of the ground
/// they lie in and never change its shape (ADR-003, *Consequences*). Earth and grass lie only where
/// no four-neighbour's top is more than one voxel lower; steeper ground is rock, so nothing the
/// generator leaves would slump. Grass covers the earth where nothing stands on it.
///
/// GenerateChunk reads only the layer and its coordinate, so the chunks may be made in any order.
class Generator
{
public:
  static constexpr std::int32_t MAP_WIDTH_VOXELS = 512; // the MVP map, 512 m x 512 m and 64 m high
  static constexpr std::int32_t MAP_HEIGHT_VOXELS = 64;
  static constexpr std::int32_t MAP_DEPTH_VOXELS = 512;
  /// The land's relief, M1's test ground's: 6 m to 12 m, cut by the river's valley down to 2 m.
  static constexpr std::int32_t LOWEST_SURFACE_VOXELS = 2;
  static constexpr std::int32_t HIGHEST_SURFACE_VOXELS = 12;

  struct Desc
  {
    std::uint64_t worldSeed;
    std::uint64_t resourceSeed;
    std::int32_t widthVoxels; // a whole number of chunks on each axis
    std::int32_t heightVoxels;
    std::int32_t depthVoxels;
  };

  /// Computes the layer. Throws std::invalid_argument for a size that is not a whole number of
  /// chunks, or too low for the relief.
  explicit Generator(const Desc& _desc);

  /// Fills _chunk, which arrives empty, with the generated voxels at _coordinate. A chunk outside
  /// the map stays empty.
  void GenerateChunk(Protocol::ChunkCoordinate _coordinate, Protocol::Chunk& _chunk) const;

  /// The generated ground's surface at a column: the height of the first voxel above the ground,
  /// with the river's water and the trees left out. Clamped to the map's edge.
  [[nodiscard]] std::int32_t SurfaceAt(std::int32_t _x, std::int32_t _z) const;

  [[nodiscard]] const Desc& Description() const noexcept;

  /// The map's size in chunks.
  [[nodiscard]] std::int32_t WidthChunks() const noexcept;
  [[nodiscard]] std::int32_t HeightChunks() const noexcept;
  [[nodiscard]] std::int32_t DepthChunks() const noexcept;

private:
  /// What the layer holds for one column.
  struct Column
  {
    std::int16_t surface;      // the first voxel above the ground
    std::int16_t bedrockTop;   // bedrock below this height
    std::int16_t soilBottom;   // earth from here to the surface; rock between bedrock and this
    std::int16_t stoneBottom;  // building stone from here to the surface, or the surface for none
    std::int16_t waterTop;     // water from the surface to below this height
    std::int16_t trunkTop;     // timber from the surface to below this height
    std::int16_t leavesBottom; // leaves from here to below leavesTop, or none when they are equal
    std::int16_t leavesTop;
    std::uint8_t leavesIndex;
    bool grass; // the top of the soil is grass
  };

  [[nodiscard]] std::size_t ColumnIndex(std::int32_t _x, std::int32_t _z) const noexcept;
  [[nodiscard]] std::uint8_t VoxelAt(const Column& _column, std::int32_t _y) const noexcept;

  Desc m_desc;
  std::vector<Column> m_columns;
};

} // namespace Simulation
