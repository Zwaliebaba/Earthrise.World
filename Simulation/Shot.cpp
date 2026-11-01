#include "pch.h"

#include "Shot.h"

#include "Chunk.h"
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

std::int32_t CraterSizeVoxels(Behavior _behavior, const Rules& _rules) noexcept
{
  switch (_behavior)
  {
  case Behavior::Earth:
    return _rules.earthCraterVoxels;
  case Behavior::CommonRock:
    return _rules.commonRockCraterVoxels;
  case Behavior::BuildingStone:
    return _rules.buildingStoneCraterVoxels;
  case Behavior::Bedrock:
    return _rules.bedrockCraterVoxels;
  case Behavior::Timber:
    return _rules.timberCraterVoxels;
  case Behavior::Leaves:
    return _rules.leavesCraterVoxels;
  case Behavior::Water:
    return _rules.waterCraterVoxels;
  case Behavior::StoneRubble:
    return _rules.stoneRubbleCraterVoxels;
  case Behavior::WasteRubble:
    return _rules.wasteRubbleCraterVoxels;
  case Behavior::None:
    break;
  }
  return 0;
}

std::vector<Protocol::VoxelPosition> CraterVoxels(Protocol::VoxelPosition _hit, std::int32_t _behindX, std::int32_t _behindZ,
                                                  const std::function<std::uint8_t(Protocol::VoxelPosition)>& _at,
                                                  const MaterialRules& _materials, const Rules& _rules)
{
  const MaterialRule& struck = _materials.at(_at(_hit));
  const std::int32_t size = CraterSizeVoxels(struck.behavior, _rules);
  const auto offset = [](Protocol::VoxelPosition _voxel, std::int32_t _dx, std::int32_t _dy, std::int32_t _dz)
  { return Protocol::VoxelPosition{.xVoxels = _voxel.xVoxels + _dx, .yVoxels = _voxel.yVoxels + _dy, .zVoxels = _voxel.zVoxels + _dz}; };
  std::vector<Protocol::VoxelPosition> shape;
  if (size >= 1)
  {
    shape.push_back(_hit);
  }
  if (size == 2 || size == 4)
  {
    shape.push_back(offset(_hit, _behindX, 0, _behindZ));
  }
  if (size == 4)
  {
    shape.push_back(offset(_hit, 0, -1, 0));
    shape.push_back(offset(_hit, _behindX, -1, _behindZ));
  }
  if (size >= 7)
  {
    constexpr std::array<std::array<std::int32_t, 3>, 6> FACES = {{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
    for (const auto& [dx, dy, dz] : FACES)
    {
      const Protocol::VoxelPosition neighbor = offset(_hit, dx, dy, dz);
      if (_materials.at(_at(neighbor)).behavior == struck.behavior)
      {
        shape.push_back(neighbor);
      }
    }
  }
  // No crater takes bedrock or water, nor what is already empty.
  std::erase_if(shape,
                [&](Protocol::VoxelPosition _voxel)
                {
                  const std::uint8_t index = _at(_voxel);
                  const MaterialRule& rule = _materials.at(index);
                  return index == Protocol::Chunk::EMPTY || rule.support == Support::Anchors || rule.liquid;
                });
  return shape;
}

ShotStep StepShot(const ShotArc& _arc, std::uint32_t _ticksFlown, const VoxelWorld& _world, std::int32_t _widthVoxels,
                  std::int32_t _depthVoxels)
{
  const FixedPosition from = ShotPositionAt(_arc, _ticksFlown);
  const FixedPosition to = ShotPositionAt(_arc, _ticksFlown + 1);
  const std::optional<Protocol::VoxelPosition> hit = TraverseVoxels(
    from, to, [&_world](Protocol::VoxelPosition _voxel) { return _voxel.yVoxels >= 0 && _world.At(_voxel) != Protocol::Chunk::EMPTY; });
  if (hit.has_value())
  {
    return {.hit = hit, .left = false};
  }
  const std::int32_t x = to.xMeters.FloorWhole();
  const std::int32_t z = to.zMeters.FloorWhole();
  return {.hit = std::nullopt, .left = to.yMeters.Raw() < 0 || x < 0 || z < 0 || x >= _widthVoxels || z >= _depthVoxels};
}

std::optional<Protocol::VoxelPosition> TraceShot(const ShotArc& _arc, const VoxelWorld& _world, std::int32_t _widthVoxels,
                                                 std::int32_t _depthVoxels)
{
  // A shot that left the world's top would come down again, so only the ground and the edges end it;
  // the arc reaches the ground long before this many ticks.
  constexpr std::uint32_t MOST_TICKS = 100000;
  for (std::uint32_t ticks = 0; ticks < MOST_TICKS; ++ticks)
  {
    const ShotStep step = StepShot(_arc, ticks, _world, _widthVoxels, _depthVoxels);
    if (step.hit.has_value() || step.left)
    {
      return step.hit;
    }
  }
  return std::nullopt;
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
