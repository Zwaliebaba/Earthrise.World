#pragma once

#include "ChunkCoordinate.h"
#include "PlacedVoxel.h"
#include "ServerData.h"
#include "SupportSearch.h"
#include "VoxelWorld.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Simulation
{

/// Which way a piece falls: towards a side, or straight down. +z is north and +x east.
enum class FallDirection : std::uint8_t
{
  Down,
  North,
  East,
  South,
  West
};

/// A voxel a collapse writes: a rubble voxel landing, or a structure voxel crushed in place. Protocol's,
/// since a collapse's view carries its landing (ADR-020).
using Protocol::PlacedVoxel;

/// What a piece's collapse does (ADR-004, decisions 3 and 4; GameDesign.md, M2's values, items 4 to
/// 8): every view, preview and AI asks PlanCollapse, never a copy of its rule (ADR-004, decision 5).
struct CollapsePlan
{
  FallDirection direction;
  std::uint32_t warningTicks; // before any voxel moves
  std::uint32_t fallTicks;    // over which the landing arrives, in equal parts
  /// What the fall writes, nearest the foot first: one rubble voxel for each voxel that falls and
  /// leaves rubble, stacked on the ground of the column it lands in, and each structure voxel below
  /// the rubble's top crushed into rubble of its own material, or to empty when it leaves none.
  std::vector<PlacedVoxel> landing;
  /// The columns the fall covers, each at the cell above its ground before the fall: where units
  /// are hit.
  std::vector<Protocol::VoxelPosition> strip;
  std::uint32_t fallenVoxels; // how many of the landing's voxels are the piece's rubble

  friend bool operator==(const CollapsePlan&, const CollapsePlan&) = default;
};

/// Plans a piece's collapse, reading _world and changing nothing: the same inputs give the same plan.
///   - The warning: warningBaseTicks plus the ticks a foot soldier takes to walk the piece's height,
///     rounded up: 135 ticks for an 8 m piece (item 4).
///   - The fall: the free-fall time from the piece's height under gravity, rounded up: 26 ticks for
///     8 m (item 5).
///   - The direction: towards the side the voxels of _cause lie on, measured from the middle of the
///     piece's box along the axis where they lie farther from it, ties to x; straight down when that
///     offset is less than a quarter of the piece's width along that axis (item 6). _cause is the
///     edits whose removal unsupported the piece; those beyond the support search's reach of it,
///     or above it, are left out.
///   - The landing (items 7 and 8): falling towards a side, the piece is laid down along the fall,
///     each voxel landing as far beyond the piece's leading face as it stood above the piece's
///     foot, plus one, in its own column across the fall, so the fall covers at most the piece's
///     height. Falling straight down, each voxel lands in its own column. Each lands on the column's
///     ground, the highest terrain or rubble below the piece's top, or on the rubble already there.
[[nodiscard]] CollapsePlan PlanCollapse(const Piece& _piece, std::span<const Protocol::VoxelPosition> _cause, const VoxelWorld& _world,
                                        const ServerData& _data);

/// The ticks a collapse of a piece _heightVoxels tall warns for (item 4).
[[nodiscard]] std::uint32_t WarningTicks(std::int32_t _heightVoxels, const Rules& _rules);

/// The ticks a piece _heightVoxels tall takes to fall (item 5).
[[nodiscard]] std::uint32_t FallTicks(std::int32_t _heightVoxels, const Rules& _rules);

} // namespace Simulation
