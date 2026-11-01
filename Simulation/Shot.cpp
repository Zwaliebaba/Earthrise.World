#include "pch.h"

#include "Shot.h"

#include "IntegerMath.h"

namespace Simulation
{

namespace
{

constexpr std::int64_t ONE = Core::Fixed::ONE_RAW;

Core::Fixed Narrow(std::int64_t _raw)
{
  if (_raw < INT32_MIN || _raw > INT32_MAX)
  {
    throw std::overflow_error("a shot's position does not fit in Core::Fixed");
  }
  return Core::Fixed::FromRaw(static_cast<std::int32_t>(_raw));
}

} // namespace

std::optional<ShotArc> AimShot(Protocol::VoxelPosition _engineCellVoxels, Protocol::VoxelPosition _targetVoxels, const Rules& _rules)
{
  // Across the ground, from the middle of the engine's cell to the middle of the target voxel.
  const std::int64_t acrossX = std::int64_t{_targetVoxels.xVoxels} - _engineCellVoxels.xVoxels;
  const std::int64_t acrossZ = std::int64_t{_targetVoxels.zVoxels} - _engineCellVoxels.zVoxels;
  const std::int64_t acrossSquared = acrossX * acrossX + acrossZ * acrossZ;
  const std::int64_t nearest = _rules.shotMinRangeVoxels;
  const std::int64_t farthest = _rules.shotMaxRangeVoxels;
  if (acrossSquared < nearest * nearest || acrossSquared > farthest * farthest || _rules.gravityMetersPerTickSquared.Raw() <= 0)
  {
    return std::nullopt;
  }
  const Core::Fixed distance =
    Core::Fixed::FromRaw(static_cast<std::int32_t>(Core::IntegerSquareRoot(static_cast<std::uint64_t>(acrossSquared) << 32)));
  const Core::Fixed launchY = Core::Fixed::FromWhole(_engineCellVoxels.yVoxels + _rules.shotLaunchHeightVoxels);
  const Core::Fixed aimY = Core::Fixed::FromWhole(_targetVoxels.yVoxels + 1); // the middle of its top face
  const Core::Fixed drop = launchY - aimY;
  if (distance <= drop)
  {
    return std::nullopt; // no 45-degree descent reaches it
  }
  const Core::Fixed gravity = _rules.gravityMetersPerTickSquared;
  const Core::Fixed flightSquared = Core::Fixed::FromWhole(2) * (distance - drop) / gravity;
  const Core::Fixed flight =
    Core::Fixed::FromRaw(static_cast<std::int32_t>(Core::IntegerSquareRoot(static_cast<std::uint64_t>(flightSquared.Raw()) << 16)));
  if (flight.Raw() <= 0)
  {
    return std::nullopt;
  }
  const Core::Fixed half = Core::Fixed::FromRaw(Core::Fixed::ONE_RAW / 2);
  return ShotArc{.start = {.xMeters = Core::Fixed::FromWhole(_engineCellVoxels.xVoxels) + half,
                           .yMeters = launchY,
                           .zMeters = Core::Fixed::FromWhole(_engineCellVoxels.zVoxels) + half},
                 .acrossXMeters = Core::Fixed::FromWhole(static_cast<std::int32_t>(acrossX)),
                 .acrossZMeters = Core::Fixed::FromWhole(static_cast<std::int32_t>(acrossZ)),
                 .flightTicks = flight,
                 .riseMetersPerTick = gravity * flight - distance / flight,
                 .gravityMetersPerTickSquared = gravity};
}

FixedPosition ShotPositionAt(const ShotArc& _arc, std::uint32_t _ticks)
{
  const std::int64_t ticks = _ticks;
  const std::int64_t flight = _arc.flightTicks.Raw();
  // Across the ground in proportion to the ticks flown; up and down as the parabola goes.
  const std::int64_t x = _arc.start.xMeters.Raw() + Core::FloorDivide(std::int64_t{_arc.acrossXMeters.Raw()} * ticks * ONE, flight);
  const std::int64_t z = _arc.start.zMeters.Raw() + Core::FloorDivide(std::int64_t{_arc.acrossZMeters.Raw()} * ticks * ONE, flight);
  const std::int64_t y = _arc.start.yMeters.Raw() + std::int64_t{_arc.riseMetersPerTick.Raw()} * ticks -
                         Core::FloorDivide(std::int64_t{_arc.gravityMetersPerTickSquared.Raw()} * ticks * ticks, 2);
  return {.xMeters = Narrow(x), .yMeters = Narrow(y), .zMeters = Narrow(z)};
}

Protocol::VoxelPosition VoxelHolding(const FixedPosition& _point) noexcept
{
  return {.xVoxels = _point.xMeters.FloorWhole(), .yVoxels = _point.yMeters.FloorWhole(), .zVoxels = _point.zMeters.FloorWhole()};
}

std::optional<Protocol::VoxelPosition> TraverseVoxels(const FixedPosition& _from, const FixedPosition& _to,
                                                      const std::function<bool(Protocol::VoxelPosition)>& _visit)
{
  const std::array<std::int64_t, 3> from = {_from.xMeters.Raw(), _from.yMeters.Raw(), _from.zMeters.Raw()};
  const std::array<std::int64_t, 3> to = {_to.xMeters.Raw(), _to.yMeters.Raw(), _to.zMeters.Raw()};
  const Protocol::VoxelPosition first = VoxelHolding(_from);
  std::array<std::int32_t, 3> voxel = {first.xVoxels, first.yVoxels, first.zVoxels};
  // On each axis: the step, the segment's length along it, and the distance from the start to the
  // next boundary it crosses. The boundary is crossed at t = distance / length of the segment, and
  // two axes' crossings compare by cross-multiplying, so nothing divides.
  std::array<std::int32_t, 3> step{};
  std::array<std::int64_t, 3> length{};
  std::array<std::int64_t, 3> distance{};
  for (std::size_t axis = 0; axis < 3; ++axis)
  {
    const std::int64_t along = to.at(axis) - from.at(axis);
    if (along == 0)
    {
      continue;
    }
    step.at(axis) = along > 0 ? 1 : -1;
    length.at(axis) = along > 0 ? along : -along;
    const std::int64_t corner = std::int64_t{voxel.at(axis)} * ONE;
    distance.at(axis) = along > 0 ? corner + ONE - from.at(axis) : from.at(axis) - corner;
  }
  const auto position = [&voxel] { return Protocol::VoxelPosition{.xVoxels = voxel[0], .yVoxels = voxel[1], .zVoxels = voxel[2]}; };
  if (_visit(position()))
  {
    return position();
  }
  while (true)
  {
    std::optional<std::size_t> next;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
      if (length.at(axis) != 0 && (!next.has_value() || distance.at(axis) * length.at(*next) < distance.at(*next) * length.at(axis)))
      {
        next = axis;
      }
    }
    if (!next.has_value() || distance.at(*next) > length.at(*next))
    {
      return std::nullopt; // the next crossing lies past the segment's end
    }
    voxel.at(*next) += step.at(*next);
    distance.at(*next) += ONE;
    if (_visit(position()))
    {
      return position();
    }
  }
}

} // namespace Simulation
