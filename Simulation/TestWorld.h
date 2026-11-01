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
/// y, 0 where it is empty. Its front is +z, and it is the layer file mirrored in x, as the bake
/// writes it, so that it looks as drawn in the world's frame (ADR-017).
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

/// Finds a template's shape by its id, or nothing. The spike and the bench pass the server bake's
/// templates through it (ServerData::Templates), so Simulation reads no client content (the owner's
/// choice of 2026-11-01; ADR-010, amended).
using TemplateLookup = std::function<std::optional<TemplateShape>(std::string_view)>;

/// A voxel a collapse brought down, and what it was: what the client scatters the collapse's debris
/// from (RendererLook.md §1).
struct FallenVoxel
{
  Protocol::VoxelPosition position;
  std::uint8_t paletteIndex;
};

/// What a collapse did: the voxels that fell, with what they were, and the rubble they became.
struct CollapseRecord
{
  std::vector<FallenVoxel> fallen;
  std::vector<Protocol::VoxelPosition> rubble;
};

/// The M1 stand-in for the generator, which is M2's: the v5 test scene's world (MvpPlan.md, M1
/// task 9) on a seeded integer height field, for the renderer spike at 128 m and for SimulationBench
/// at 512 m, the MVP map's size, with the v5 scene's square at its middle (GameDesign.md, M1's
/// open values, item 5).
///
/// The scene is stated in the frame its prototype draws in, where +z is south, and the world's +z
/// is north, so the square is mirrored north to south on its way in (ADR-017). These functions take
/// a scene coordinate to the world's.
struct TestWorld
{
  static constexpr std::int32_t HEIGHT_VOXELS = 48;
  static constexpr std::int32_t SCENE_EDGE_VOXELS = 128;
  static constexpr std::int32_t CASTLE_GROUND_VOXELS = 15;

  /// The world's voxel that a scene voxel is. A point in the scene's meters goes the same way: its x
  /// is WorldXVoxels(0) plus its own, and its z is WorldZVoxels(0) + 1 less its own, which the spike
  /// works out in float on its own side, as the simulation holds none (ADR-013).
  [[nodiscard]] std::int32_t WorldXVoxels(std::int32_t _sceneXVoxels) const noexcept;
  [[nodiscard]] std::int32_t WorldZVoxels(std::int32_t _sceneZVoxels) const noexcept;
  /// The world's quarter turns, clockwise seen from above, for a turn the scene states: the mirror
  /// takes a front facing the scene's +z, south, to the world's -z, and reverses the sense of a turn.
  [[nodiscard]] static constexpr std::uint32_t WorldQuarterTurns(std::uint32_t _sceneQuarterTurns) noexcept
  {
    return (6 - _sceneQuarterTurns % 4) % 4;
  }

  std::int32_t edgeVoxels;
  std::int32_t sceneXVoxels; // where the v5 scene's square starts
  std::int32_t sceneZVoxels;
  std::vector<std::int32_t> surfaceVoxels; // the generated surface at each column, before templates and digs
  std::uint64_t editedVoxels;              // voxels that differ from the generated ground: what templates and digs changed
  /// The scene's breach, which stands from the start: the wall it brought down and the rubble ramp
  /// it left, from which the client scatters its debris (the render-fix package, item 4).
  CollapseRecord breach;
  /// The breach's id, from the seed: what its debris is seeded by, as a collapse's is.
  std::uint64_t breachId;

  /// The generated surface at a column, clamped to the world's edge: what the support search's
  /// anchor depth is counted from.
  [[nodiscard]] std::int32_t SurfaceAt(std::int32_t _x, std::int32_t _z) const;

  /// The middle of the castle, where the spike's camera looks: the world's voxel there.
  [[nodiscard]] std::int32_t CastleXVoxels() const noexcept;
  [[nodiscard]] std::int32_t CastleZVoxels() const noexcept;
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
/// at 10 s into a heap of rubble at its foot. The quarry is worked by growing its terraced pit a
/// meter at a time, through the stone body, then south into the earth, then towards the castle, so
/// it reads as a quarry at every frame (the render-fix package, item 5).
class TestWorldEdits
{
public:
  static constexpr std::uint32_t FRAMES_PER_SECOND = 60;
  /// 94,000 voxel edits a second at 10 cm (the design review, F-51) are 94 at 1 m.
  static constexpr std::uint32_t QUARRY_VOXELS_PER_SECOND = 94;
  static constexpr std::uint32_t COLLAPSE_FRAME = 10 * FRAMES_PER_SECOND;
  static constexpr std::uint32_t LAST_FRAME = 60 * FRAMES_PER_SECOND;

  /// Reads the quarry's stone from _store, which BuildTestWorld has filled as _world, and orders
  /// what it digs.
  TestWorldEdits(const ChunkStore& _store, const TestWorld& _world, std::uint64_t _seed);

  /// Makes frame _frame's edits in _store, and returns the voxels it changed.
  std::vector<Protocol::VoxelPosition> Apply(ChunkStore& _store, std::uint32_t _frame);

  /// The collapse alone, as the bench's breach: the section's voxels that fall and the rubble they
  /// become, all as changed voxels. What it did goes into _record when it is given.
  [[nodiscard]] std::vector<Protocol::VoxelPosition> Collapse(ChunkStore& _store, CollapseRecord* _record = nullptr) const;

  /// What the collapse brought down, once Apply has made it, and empty before: the stand-in for the
  /// collapse event the simulation will send (ADR-004), which the client's debris is scattered from.
  [[nodiscard]] const std::vector<FallenVoxel>& Fallen() const noexcept;
  /// The rubble the collapse heaped, once Apply has made it: where its debris comes to rest.
  [[nodiscard]] const std::vector<Protocol::VoxelPosition>& Rubble() const noexcept;

  /// The collapse's id, from the edits' seed: what its debris is seeded by, so that every client
  /// shows the same (RendererLook.md §1).
  [[nodiscard]] std::uint64_t CollapseId() const noexcept;

private:
  void Quarry(ChunkStore& _store, std::uint32_t _voxels, std::vector<Protocol::VoxelPosition>& _changed);

  std::int32_t m_sceneXVoxels;
  std::int32_t m_sceneZVoxels;
  std::vector<Protocol::VoxelPosition> m_digs; // the quarry's voxels, in the order they are dug
  std::size_t m_nextDig = 0;
  CollapseRecord m_collapse;
  std::uint64_t m_collapseId;
};

} // namespace Simulation
