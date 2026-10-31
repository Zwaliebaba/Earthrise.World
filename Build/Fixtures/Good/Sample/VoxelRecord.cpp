#include "pch.h"

#include "VoxelRecord.h"

namespace Sample
{

std::uint32_t PackPosition(std::uint32_t _xVoxels, std::uint32_t _yVoxels, std::uint32_t _zVoxels) noexcept
{
  return _xVoxels | (_yVoxels << 8) | (_zVoxels << 16);
}

} // namespace Sample
