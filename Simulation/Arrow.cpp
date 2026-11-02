#include "pch.h"

#include "Arrow.h"

#include "Chunk.h"
#include "IntegerMath.h"
#include "Shot.h"

namespace Simulation
{

namespace
{

constexpr std::int64_t ONE = Core::Fixed::ONE_RAW;

Core::Fixed Narrow(std::int64_t _raw)
{
  if (_raw < INT32_MIN || _raw > INT32_MAX)
  {
    throw std::overflow_error("an arrow's line does not fit in Core::Fixed");
  }
  return Core::Fixed::FromRaw(static_cast<std::int32_t>(_raw));
}

/// The middle of a cell's floor, raised by _raiseRaw.
FixedPosition Raised(Protocol::VoxelPosition _cellVoxels, std::int64_t _raiseRaw)
{
  return {.xMeters = Narrow((_cellVoxels.xVoxels * ONE) + (ONE / 2)),
          .yMeters = Narrow((_cellVoxels.yVoxels * ONE) + _raiseRaw),
          .zMeters = Narrow((_cellVoxels.zVoxels * ONE) + (ONE / 2))};
}

/// _ticks as Core::Fixed, or nothing past what it holds.
std::optional<Core::Fixed> TicksAsFixed(std::uint32_t _ticks)
{
  if (_ticks > static_cast<std::uint32_t>(Core::Fixed::MAX_WHOLE))
  {
    return std::nullopt;
  }
  return Core::Fixed::FromWhole(static_cast<std::int32_t>(_ticks));
}

} // namespace

ArrowLine AimArrow(Protocol::VoxelPosition _fromCellVoxels, Protocol::VoxelPosition _toCellVoxels, const Rules& _rules)
{
  // The launch height is in centimetres, and 1.5 m is exact in Q16.16; any other value floors once.
  const FixedPosition start = Raised(_fromCellVoxels, (std::int64_t{_rules.arrowLaunchHeightCentimeters} * ONE) / 100);
  const FixedPosition end = Raised(_toCellVoxels, std::int64_t{_rules.arrowAimHeightVoxels} * ONE);
  const std::int64_t dx = std::int64_t{end.xMeters.Raw()} - start.xMeters.Raw();
  const std::int64_t dy = std::int64_t{end.yMeters.Raw()} - start.yMeters.Raw();
  const std::int64_t dz = std::int64_t{end.zMeters.Raw()} - start.zMeters.Raw();
  const auto length = static_cast<std::int64_t>(Core::IntegerSquareRoot(static_cast<std::uint64_t>((dx * dx) + (dy * dy) + (dz * dz))));
  const Core::Fixed speed = _rules.arrowSpeedMetersPerTick;
  return {.start = start, .end = end, .flightTicks = speed.Raw() > 0 ? Narrow(length) / speed : Core::Fixed::FromRaw(0)};
}

FixedPosition ArrowPositionAt(const ArrowLine& _line, std::uint32_t _ticks)
{
  const std::optional<Core::Fixed> ticks = TicksAsFixed(_ticks);
  if (!ticks.has_value() || *ticks >= _line.flightTicks)
  {
    return _line.end;
  }
  const Core::Fixed share = *ticks / _line.flightTicks;
  return {.xMeters = _line.start.xMeters + ((_line.end.xMeters - _line.start.xMeters) * share),
          .yMeters = _line.start.yMeters + ((_line.end.yMeters - _line.start.yMeters) * share),
          .zMeters = _line.start.zMeters + ((_line.end.zMeters - _line.start.zMeters) * share)};
}

bool ArrowArrives(const ArrowLine& _line, std::uint32_t _ticksFlown)
{
  const std::optional<Core::Fixed> next = TicksAsFixed(_ticksFlown + 1);
  return !next.has_value() || *next >= _line.flightTicks;
}

std::optional<Protocol::VoxelPosition> FirstSolid(const FixedPosition& _from, const FixedPosition& _to, const VoxelWorld& _world)
{
  return TraverseVoxels(_from, _to, [&_world](Protocol::VoxelPosition _voxel)
                        { return _voxel.yVoxels >= 0 && _world.At(_voxel) != Protocol::Chunk::EMPTY; });
}

std::int32_t ElevationSteps(std::int32_t _fromYVoxels, std::int32_t _toYVoxels, const Rules& _rules) noexcept
{
  if (_rules.elevationStepVoxels <= 0)
  {
    return 0;
  }
  const std::int32_t steps = (_fromYVoxels - _toYVoxels) / _rules.elevationStepVoxels;
  return std::clamp(steps, -_rules.elevationMaxSteps, _rules.elevationMaxSteps);
}

std::int32_t ArrowRangeVoxels(std::int32_t _rangeVoxels, std::int32_t _steps, const Rules& _rules) noexcept
{
  return std::max(0, _rangeVoxels + (_rules.elevationRangeBandVoxels * _steps));
}

} // namespace Simulation
