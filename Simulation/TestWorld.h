#pragma once

#include "ChunkCoordinate.h"
#include "ChunkStore.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Simulation
{

/// A template's shape as Simulation reads it: its box of palette indices, x fastest, then z, then
/// y, as a layer file's layers[y][z][x], 0 where it is empty.
struct TemplateShape
{
  std::uint32_t widthVoxels;
  std::uint32_t heightVoxels;
  std::uint32_t depthVoxels;
  std::span<const std::uint8_t> indices;

  [[nodiscard]] std::uint8_t At(std::uint32_t _x, std::uint32_t _y, std::uint32_t _z) const
  {
    return indices[static_cast<std::size_t>(_x) +
                   static_cast<std::size_t>(widthVoxels) * (_z + static_cast<std::size_t>(depthVoxels) * _y)];
  }
};

/// Finds a template's shape by its id, or nothing. In M1 the spike and the bench pass Content's
/// templates through it; from M2 the server bake's take their place, and Simulation reads no
/// client content (the owner's choice of 2026-11-01; ADR-010, amended).
using TemplateLookup = std::function<std::optional<TemplateShape>(std::string_view)>;

/// The M1 stand-in for the generator, which is M2's: the v5 test scene's world (MvpPlan.md, M1
/// task 9) on a seeded integer height field, for the renderer spike at 128 m and for SimulationBench
/// at 512 m, the MVP map's size, with the v5 scene's square at its middle (GameDesign.md, M1's
/// open values, item 5).
struct TestWorld
{
  static constexpr std::int32_t HEIGHT_VOXELS = 48;
  static constexpr std::int32_t SCENE_EDGE_VOXELS = 128;
  static constexpr std::int32_t CASTLE_GROUND_VOXELS = 15;

  std::int32_t edgeVoxels;
  std::int32_t sceneXVoxels; // where the v5 scene's square starts
  std::int32_t sceneZVoxels;
  std::vector<std::int32_t> surfaceVoxels; // the generated surface at each column, before templates and digs
  std::uint64_t editedVoxels;              // voxels that differ from the generated ground: what templates and digs changed

  /// The generated surface at a column, clamped to the world's edge: what the support search's
  /// anchor depth is counted from.
  [[nodiscard]] std::int32_t SurfaceAt(std::int32_t _x, std::int32_t _z) const;

  /// The middle of the castle, where the spike's camera looks, in meters.
  [[nodiscard]] float CastleXMeters() const noexcept;
  [[nodiscard]] float CastleZMeters() const noexcept;
};

/// Builds the v5 scene's world into _store: the ground, the quarry, the road and fields, the ditch
/// and rampart, the castle from _templates, the breach and the forest, with the scene's 128 m
/// square at the middle of an _edgeVoxels world, 128 or more and a whole number of chunks. Throws
/// std::runtime_error when a template it needs is not found, and std::invalid_argument for an edge
/// it cannot take.
[[nodiscard]] TestWorld BuildTestWorld(ChunkStore& _store, std::int32_t _edgeVoxels, std::uint64_t _seed, const TemplateLookup& _templates);

/// The synthetic world of the renderer spike's question 6: _edgeVoxels across on the same ground,
/// carrying a season's edits (GameDesign.md, M1's open values, item 4) where holdings would stand.
/// Each of _holdings lords builds the test castle twice over, at two sites a little apart, quarries
/// as much stone beside each, and digs its ditch; a dozen of them are breached. Each castle stands
/// on the scene's plateau, which the generator raises, so no footprint needs levelling first.
/// editedVoxels counts what all that changed.
[[nodiscard]] TestWorld BuildSeasonWorld(ChunkStore& _store, std::int32_t _edgeVoxels, std::int32_t _holdings, std::uint64_t _seed,
                                         const TemplateLookup& _templates);

/// The height of the highest solid voxel in a column at or below _fromVoxels, or -1 for none.
[[nodiscard]] std::int32_t TopSolidAtOrBelow(const ChunkStore& _store, std::int32_t _x, std::int32_t _z, std::int32_t _fromVoxels);

// The 0.5 m world of ADR-006's comparison is the 1 m world with each voxel split into eight, so the
// two differ only in voxel size (GameDesign.md, M1's open values, item 8).

/// The eight 0.5 m voxels a 1 m voxel covers, at twice its coordinates.
[[nodiscard]] std::array<Protocol::VoxelPosition, 8> HalfMeterVoxels(Protocol::VoxelPosition _oneMeter) noexcept;

/// Copies the columns from (_minXVoxels, _minZVoxels) to (_maxXVoxels, _maxZVoxels), both included and
/// all of the test world's height, from a 1 m store into _halfMeter, each voxel split into eight.
void SplitToHalfMeters(const ChunkStore& _oneMeter, std::int32_t _minXVoxels, std::int32_t _minZVoxels, std::int32_t _maxXVoxels,
                       std::int32_t _maxZVoxels, ChunkStore& _halfMeter);

/// Makes in _halfMeter the changes _changed made in _oneMeter: each changed voxel's eight take what
/// it now holds.
void SplitChanges(const ChunkStore& _oneMeter, std::span<const Protocol::VoxelPosition> _changed, ChunkStore& _halfMeter);

/// The test world's scripted edits, stepped by frame at 60 a second for the measured path's 60 s,
/// then stopped (GameDesign.md, *Starting the MVP*, item 3): quarrying at the four-faction peak the
/// Skirmish tempo implies, and the section of the west curtain just north of the breach collapsing
/// at 10 s into a heap of rubble at its foot.
class TestWorldEdits
{
public:
  static constexpr std::uint32_t FRAMES_PER_SECOND = 60;
  /// 94,000 voxel edits a second at 10 cm (the design review, F-51) are 94 at 1 m.
  static constexpr std::uint32_t QUARRY_VOXELS_PER_SECOND = 94;
  static constexpr std::uint32_t COLLAPSE_FRAME = 10 * FRAMES_PER_SECOND;
  static constexpr std::uint32_t LAST_FRAME = 60 * FRAMES_PER_SECOND;

  /// Reads the quarry's surface from _store, which BuildTestWorld has filled as _world.
  TestWorldEdits(const ChunkStore& _store, const TestWorld& _world, std::uint64_t _seed);

  /// Makes frame _frame's edits in _store, and returns the voxels it changed.
  std::vector<Protocol::VoxelPosition> Apply(ChunkStore& _store, std::uint32_t _frame);

  /// The collapse alone, as the bench's breach: the section's voxels that fall and the rubble they
  /// become, all as changed voxels.
  [[nodiscard]] std::vector<Protocol::VoxelPosition> Collapse(ChunkStore& _store) const;

private:
  struct Column
  {
    std::int32_t xVoxels;
    std::int32_t zVoxels;
    std::int32_t topVoxels; // the height of its highest solid voxel
  };

  void Quarry(ChunkStore& _store, std::uint32_t _voxels, std::vector<Protocol::VoxelPosition>& _changed);

  std::int32_t m_sceneXVoxels;
  std::int32_t m_sceneZVoxels;
  std::vector<Column> m_quarry; // in the seeded order that breaks ties between equal heights
};

} // namespace Simulation
