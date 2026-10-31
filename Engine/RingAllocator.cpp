#include "pch.h"

#include "RingAllocator.h"

namespace Engine
{

namespace
{

constexpr bool IsPowerOfTwo(std::uint64_t _value) noexcept
{
  return _value != 0 && (_value & (_value - 1)) == 0;
}

constexpr std::uint64_t AlignUp(std::uint64_t _value, std::uint64_t _alignment) noexcept
{
  return (_value + _alignment - 1) & ~(_alignment - 1);
}

} // namespace

RingAllocator::RingAllocator(std::uint64_t _capacityBytes)
  : m_capacityBytes(_capacityBytes)
{
  if (_capacityBytes == 0)
  {
    throw std::invalid_argument("a ring needs a capacity");
  }
}

std::optional<std::uint64_t> RingAllocator::Allocate(std::uint64_t _sizeBytes, std::uint64_t _alignmentBytes)
{
  if (_sizeBytes == 0 || _sizeBytes > m_capacityBytes || !IsPowerOfTwo(_alignmentBytes))
  {
    throw std::invalid_argument("a span needs a size within the ring and a power-of-two alignment");
  }
  if (m_usedBytes == 0)
  {
    // Empty: start again at the front, so the whole ring is one contiguous span.
    m_headBytes = 0;
    m_tailBytes = 0;
  }

  const std::uint64_t alignedBytes = AlignUp(m_headBytes, _alignmentBytes);
  std::uint64_t offsetBytes = 0;
  std::uint64_t takenBytes = 0; // the span, plus its padding or the end a wrap skips
  if (m_usedBytes == 0 || m_headBytes > m_tailBytes)
  {
    // The free space is the end of the ring, then the front up to the tail.
    if (alignedBytes <= m_capacityBytes && _sizeBytes <= m_capacityBytes - alignedBytes)
    {
      offsetBytes = alignedBytes;
      takenBytes = alignedBytes - m_headBytes + _sizeBytes;
    }
    else if (_sizeBytes <= m_tailBytes)
    {
      offsetBytes = 0;
      takenBytes = m_capacityBytes - m_headBytes + _sizeBytes;
    }
    else
    {
      return std::nullopt;
    }
  }
  else if (m_headBytes < m_tailBytes && alignedBytes <= m_tailBytes && _sizeBytes <= m_tailBytes - alignedBytes)
  {
    // The free space lies between the head and the tail.
    offsetBytes = alignedBytes;
    takenBytes = alignedBytes - m_headBytes + _sizeBytes;
  }
  else
  {
    return std::nullopt;
  }

  m_headBytes = offsetBytes + _sizeBytes;
  m_usedBytes += takenBytes;
  m_unretiredBytes += takenBytes;
  return offsetBytes;
}

void RingAllocator::Retire(std::uint64_t _fenceValue)
{
  if (_fenceValue < m_lastFenceValue)
  {
    throw std::invalid_argument("fence values must not decrease");
  }
  m_lastFenceValue = _fenceValue;
  if (m_unretiredBytes == 0)
  {
    return;
  }
  m_retired.push_back({.fenceValue = _fenceValue, .endOffsetBytes = m_headBytes, .sizeBytes = m_unretiredBytes});
  m_unretiredBytes = 0;
}

void RingAllocator::Reclaim(std::uint64_t _completedFenceValue) noexcept
{
  while (!m_retired.empty() && m_retired.front().fenceValue <= _completedFenceValue)
  {
    m_tailBytes = m_retired.front().endOffsetBytes;
    m_usedBytes -= m_retired.front().sizeBytes;
    m_retired.pop_front();
  }
}

std::uint64_t RingAllocator::CapacityBytes() const noexcept
{
  return m_capacityBytes;
}

std::uint64_t RingAllocator::UsedBytes() const noexcept
{
  return m_usedBytes;
}

} // namespace Engine
