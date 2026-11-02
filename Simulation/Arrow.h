#pragma once

#include "ChunkCoordinate.h"
#include "Fixed.h"
#include "FixedPosition.h"
#include "Rules.h"
#include "VoxelWorld.h"

#include <cstdint>
#include <optional>

namespace Simulation
{

using Protocol::FixedPosition;

/// An arrow's flight (ADR-023, decision 5): a straight line from its loosing point, arrowLaunchHeightCentimeters above
/// the middle of its shooter's cell floor, to its aim point, arrowAimHeightVoxels above the middle of its target's,
/// flown at arrowSpeedMetersPerTick. flightTicks is the line's length over that speed. Every value is Core::Fixed, so the
/// flight is the same on every machine (ADR-013).
struct ArrowLine
{
  FixedPosition start;
  FixedPosition end;
  Core::Fixed flightTicks;
};

/// The line from a shooter standing in _fromCellVoxels to a target standing in _toCellVoxels.
[[nodiscard]] ArrowLine AimArrow(Protocol::VoxelPosition _fromCellVoxels, Protocol::VoxelPosition _toCellVoxels, const Rules& _rules);

/// Where the arrow is _ticks after loosing: along the line in proportion, in closed form so that nothing accumulates, and
/// at its end once flightTicks have passed.
[[nodiscard]] FixedPosition ArrowPositionAt(const ArrowLine& _line, std::uint32_t _ticks);

/// Whether the arrow reaches its end in the tick that starts _ticksFlown after loosing.
[[nodiscard]] bool ArrowArrives(const ArrowLine& _line, std::uint32_t _ticksFlown);

/// The first solid voxel on the segment from _from to _to, by TraverseVoxels: any that is not empty, as a stone's shot
/// counts it; or nothing.
[[nodiscard]] std::optional<Protocol::VoxelPosition> FirstSolid(const FixedPosition& _from, const FixedPosition& _to,
                                                                const VoxelWorld& _world);

/// The elevation steps from a shooter standing at _fromYVoxels to a target standing at _toYVoxels: the whole
/// elevationStepVoxels in the height between them, towards zero, positive when the shooter stands higher, and at most
/// elevationMaxSteps either way (ADR-023, decisions 2 and 5).
[[nodiscard]] std::int32_t ElevationSteps(std::int32_t _fromYVoxels, std::int32_t _toYVoxels, const Rules& _rules) noexcept;

/// The reach across the ground of a ranged attack of _rangeVoxels at _steps: elevationRangeBandVoxels more for each step
/// down to the target, less for each step up, never below 0.
[[nodiscard]] std::int32_t ArrowRangeVoxels(std::int32_t _rangeVoxels, std::int32_t _steps, const Rules& _rules) noexcept;

} // namespace Simulation
