#include "pch.h"

#include "Formation.h"

namespace Simulation
{

std::uint8_t FacingOf(std::int32_t _dx, std::int32_t _dz) noexcept
{
  for (std::size_t facing = 0; facing < FACINGS.size(); ++facing)
  {
    if (FACINGS.at(facing)[0] == _dx && FACINGS.at(facing)[1] == _dz)
    {
      return static_cast<std::uint8_t>(facing);
    }
  }
  return 0;
}

std::vector<Protocol::VoxelPosition> FormationSlots(Protocol::VoxelPosition _leadVoxels, std::uint8_t _direction, std::uint8_t _figures,
                                                    std::uint8_t _ranks, std::uint8_t _fullFigures)
{
  const std::array<std::int32_t, 2>& forward = FACINGS.at(_direction % FACINGS.size());
  // The right of a facing is a quarter turn clockwise from it, seen from above: north's is east.
  const std::array<std::int32_t, 2> right = {forward[1], -forward[0]};
  const std::int32_t ranks = std::max<std::int32_t>(_ranks, 1);
  const std::int32_t perRank = std::max<std::int32_t>(1, (std::max<std::int32_t>(_fullFigures, _figures) + ranks - 1) / ranks);
  std::vector<Protocol::VoxelPosition> slots;
  slots.reserve(_figures);
  for (std::int32_t slot = 0; slot < _figures; ++slot)
  {
    const std::int32_t rank = slot / perRank;
    const std::int32_t place = slot % perRank;
    // The middle, then left (-1) and right (+1) of it, alternately outwards.
    const std::int32_t lateral = place == 0 ? 0 : (place % 2 == 1 ? -((place + 1) / 2) : place / 2);
    slots.push_back({.xVoxels = _leadVoxels.xVoxels - (forward[0] * rank) + (right[0] * lateral),
                     .yVoxels = _leadVoxels.yVoxels,
                     .zVoxels = _leadVoxels.zVoxels - (forward[1] * rank) + (right[1] * lateral)});
  }
  return slots;
}

std::vector<Protocol::VoxelPosition> FormationSlots(Protocol::VoxelPosition _leadVoxels, std::uint8_t _direction, std::uint8_t _figures,
                                                    std::uint8_t _ranks)
{
  return FormationSlots(_leadVoxels, _direction, _figures, _ranks, _figures);
}

} // namespace Simulation
