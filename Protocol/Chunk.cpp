#include "pch.h"

#include "Chunk.h"

namespace Protocol
{

std::uint8_t Chunk::At(std::int32_t _xVoxels, std::int32_t _yVoxels, std::int32_t _zVoxels) const
{
  return m_voxels[IndexOf(_xVoxels, _yVoxels, _zVoxels)];
}

void Chunk::Set(std::int32_t _xVoxels, std::int32_t _yVoxels, std::int32_t _zVoxels, std::uint8_t _paletteIndex)
{
  std::uint8_t& voxel = m_voxels[IndexOf(_xVoxels, _yVoxels, _zVoxels)];
  if (voxel == EMPTY && _paletteIndex != EMPTY)
  {
    ++m_solidCount;
  }
  else if (voxel != EMPTY && _paletteIndex == EMPTY)
  {
    --m_solidCount;
  }
  voxel = _paletteIndex;
}

std::size_t Chunk::SolidCount() const noexcept
{
  return m_solidCount;
}

std::size_t Chunk::IndexOf(std::int32_t _xVoxels, std::int32_t _yVoxels, std::int32_t _zVoxels)
{
  const auto inside = [](std::int32_t _value) { return _value >= 0 && _value < EDGE_VOXELS; };
  if (!inside(_xVoxels) || !inside(_yVoxels) || !inside(_zVoxels))
  {
    throw std::out_of_range("voxel coordinate outside its chunk");
  }
  return (static_cast<std::size_t>(_yVoxels) * EDGE_VOXELS + static_cast<std::size_t>(_zVoxels)) * EDGE_VOXELS +
         static_cast<std::size_t>(_xVoxels);
}

} // namespace Protocol
