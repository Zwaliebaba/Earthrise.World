#include "pch.h"

#include "Visibility.h"

namespace Simulation
{

namespace
{

constexpr std::int32_t WORD_BITS = 64;

/// The largest whole number whose square is at most _value, for _value of 0 or more, by Newton's method in integers: the
/// simulation holds no floating point (ADR-013).
std::int32_t SquareRootFloor(std::int64_t _value) noexcept
{
  if (_value < 2)
  {
    return static_cast<std::int32_t>(std::max<std::int64_t>(0, _value));
  }
  std::int64_t root = _value;
  std::int64_t next = (root + 1) / 2;
  while (next < root)
  {
    root = next;
    next = (root + (_value / root)) / 2;
  }
  return static_cast<std::int32_t>(root);
}

/// How far _at lies outside _low to _high, both included: 0 inside.
std::int32_t Outside(std::int32_t _at, std::int32_t _low, std::int32_t _high) noexcept
{
  return std::max({_low - _at, 0, _at - _high});
}

} // namespace

Visibility::Visibility(std::int32_t _widthCells, std::int32_t _depthCells)
  : m_widthCells(std::max(0, _widthCells)),
    m_depthCells(std::max(0, _depthCells)),
    m_bits(((static_cast<std::size_t>(m_widthCells) * static_cast<std::size_t>(m_depthCells)) + WORD_BITS - 1) / WORD_BITS, 0)
{
}

void Visibility::Clear() noexcept
{
  std::ranges::fill(m_bits, std::uint64_t{0});
}

void Visibility::Stamp(std::int32_t _xCells, std::int32_t _zCells, std::int32_t _radiusCells) noexcept
{
  StampBox({.xVoxels = _xCells, .yVoxels = 0, .zVoxels = _zCells}, {.xVoxels = _xCells, .yVoxels = 0, .zVoxels = _zCells}, _radiusCells);
}

void Visibility::StampBox(Protocol::VoxelPosition _minVoxels, Protocol::VoxelPosition _maxVoxels, std::int32_t _radiusCells) noexcept
{
  const std::int32_t radius = std::max(0, _radiusCells);
  const std::int64_t radiusSquared = std::int64_t{radius} * radius;
  for (std::int32_t z = std::max(0, _minVoxels.zVoxels - radius); z <= std::min(m_depthCells - 1, _maxVoxels.zVoxels + radius); ++z)
  {
    const std::int64_t across = Outside(z, _minVoxels.zVoxels, _maxVoxels.zVoxels);
    if (across * across > radiusSquared)
    {
      continue;
    }
    const std::int32_t reach = SquareRootFloor(radiusSquared - (across * across));
    SetRun(z, _minVoxels.xVoxels - reach, _maxVoxels.xVoxels + reach);
  }
}

bool Visibility::Visible(std::int32_t _xCells, std::int32_t _zCells) const noexcept
{
  if (_xCells < 0 || _zCells < 0 || _xCells >= m_widthCells || _zCells >= m_depthCells)
  {
    return false;
  }
  const std::size_t bit = (static_cast<std::size_t>(_zCells) * static_cast<std::size_t>(m_widthCells)) + static_cast<std::size_t>(_xCells);
  return ((m_bits[bit / WORD_BITS] >> (bit % WORD_BITS)) & 1U) != 0;
}

bool Visibility::AnyVisible(std::int32_t _x0, std::int32_t _z0, std::int32_t _x1, std::int32_t _z1) const noexcept
{
  const std::int32_t x0 = std::max(0, _x0);
  const std::int32_t x1 = std::min(m_widthCells - 1, _x1);
  for (std::int32_t z = std::max(0, _z0); z <= std::min(m_depthCells - 1, _z1) && x0 <= x1; ++z)
  {
    const std::size_t first = (static_cast<std::size_t>(z) * static_cast<std::size_t>(m_widthCells)) + static_cast<std::size_t>(x0);
    const std::size_t last = (static_cast<std::size_t>(z) * static_cast<std::size_t>(m_widthCells)) + static_cast<std::size_t>(x1);
    for (std::size_t word = first / WORD_BITS; word <= last / WORD_BITS; ++word)
    {
      const std::size_t low = std::max(first, word * WORD_BITS) - (word * WORD_BITS);
      const std::size_t high = std::min(last, (word * WORD_BITS) + WORD_BITS - 1) - (word * WORD_BITS);
      const std::uint64_t mask = (high - low + 1 == WORD_BITS) ? ~std::uint64_t{0} : (((std::uint64_t{1} << (high - low + 1)) - 1) << low);
      if ((m_bits[word] & mask) != 0)
      {
        return true;
      }
    }
  }
  return false;
}

std::span<const std::uint64_t> Visibility::Bits() const noexcept
{
  return m_bits;
}

void Visibility::SetRun(std::int32_t _z, std::int32_t _x0, std::int32_t _x1) noexcept
{
  const std::int32_t x0 = std::max(0, _x0);
  const std::int32_t x1 = std::min(m_widthCells - 1, _x1);
  if (_z < 0 || _z >= m_depthCells || x0 > x1)
  {
    return;
  }
  const std::size_t first = (static_cast<std::size_t>(_z) * static_cast<std::size_t>(m_widthCells)) + static_cast<std::size_t>(x0);
  const std::size_t last = (static_cast<std::size_t>(_z) * static_cast<std::size_t>(m_widthCells)) + static_cast<std::size_t>(x1);
  for (std::size_t word = first / WORD_BITS; word <= last / WORD_BITS; ++word)
  {
    const std::size_t low = std::max(first, word * WORD_BITS) - (word * WORD_BITS);
    const std::size_t high = std::min(last, (word * WORD_BITS) + WORD_BITS - 1) - (word * WORD_BITS);
    m_bits[word] |= (high - low + 1 == WORD_BITS) ? ~std::uint64_t{0} : (((std::uint64_t{1} << (high - low + 1)) - 1) << low);
  }
}

} // namespace Simulation
