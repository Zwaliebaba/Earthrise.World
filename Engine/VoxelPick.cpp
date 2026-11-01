#include "pch.h"

#include "VoxelPick.h"

#include <limits>

namespace Engine
{

namespace
{

constexpr std::size_t AXES = 3;

/// The face a step along _axis by _step enters a cell by: its negative face when the step is positive.
VoxelFace EnteredFace(std::size_t _axis, std::int32_t _step) noexcept
{
  constexpr std::size_t X = 0;
  constexpr std::size_t Y = 1;
  if (_axis == X)
  {
    return _step > 0 ? VoxelFace::NegativeX : VoxelFace::PositiveX;
  }
  if (_axis == Y)
  {
    return VoxelFace::PositiveY; // a step down: a rising ray is refused
  }
  return _step > 0 ? VoxelFace::NegativeZ : VoxelFace::PositiveZ;
}

} // namespace

std::optional<PickHit> PickVoxel(const PickRay& _ray, float _voxelEdgeMeters, float _maxDistanceMeters, const SolidAt& _solidAt)
{
  if (_ray.direction.y > 0.0F)
  {
    throw std::invalid_argument("a pick ray rises, and would enter a cell by its bottom face, which VoxelFace does not name");
  }
  const std::array<float, AXES> origin = {_ray.originMeters.x, _ray.originMeters.y, _ray.originMeters.z};
  const std::array<float, AXES> direction = {_ray.direction.x, _ray.direction.y, _ray.direction.z};
  std::array<std::int32_t, AXES> cell{};
  std::array<std::int32_t, AXES> step{};
  std::array<float, AXES> nextMeters{};  // along the ray, where it next crosses a cell boundary on each axis
  std::array<float, AXES> deltaMeters{}; // along the ray, between two boundaries on each axis
  std::size_t largest = 0;
  for (std::size_t axis = 0; axis < AXES; ++axis)
  {
    cell.at(axis) = static_cast<std::int32_t>(std::floor(origin.at(axis) / _voxelEdgeMeters));
    const float component = direction.at(axis);
    if (std::abs(component) > std::abs(direction.at(largest)))
    {
      largest = axis;
    }
    if (component == 0.0F)
    {
      step.at(axis) = 0;
      nextMeters.at(axis) = std::numeric_limits<float>::infinity();
      deltaMeters.at(axis) = std::numeric_limits<float>::infinity();
      continue;
    }
    step.at(axis) = component > 0.0F ? 1 : -1;
    const float boundary = static_cast<float>(cell.at(axis) + (component > 0.0F ? 1 : 0)) * _voxelEdgeMeters;
    nextMeters.at(axis) = (boundary - origin.at(axis)) / component;
    deltaMeters.at(axis) = _voxelEdgeMeters / std::abs(component);
  }
  if (_solidAt(cell[0], cell[1], cell[2]))
  {
    const std::int32_t back = direction.at(largest) > 0.0F ? 1 : -1;
    return PickHit{.cell = cell, .face = EnteredFace(largest, back), .distanceMeters = 0.0F};
  }
  while (true)
  {
    // Across the nearest boundary; on a tie, the lowest axis first.
    std::size_t axis = 0;
    for (std::size_t other = 1; other < AXES; ++other)
    {
      if (nextMeters.at(other) < nextMeters.at(axis))
      {
        axis = other;
      }
    }
    const float distance = nextMeters.at(axis);
    if (!(distance <= _maxDistanceMeters))
    {
      return std::nullopt; // also when the ray has no direction
    }
    cell.at(axis) += step.at(axis);
    nextMeters.at(axis) += deltaMeters.at(axis);
    if (_solidAt(cell[0], cell[1], cell[2]))
    {
      return PickHit{.cell = cell, .face = EnteredFace(axis, step.at(axis)), .distanceMeters = distance};
    }
  }
}

} // namespace Engine
