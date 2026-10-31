#pragma once

#include <cstdint>
#include <deque>
#include <optional>

namespace Engine
{

/// The arithmetic of a ring whose spans the GPU finishes in submission order (ADR-011, decision 8).
/// Offsets are handed out front to back and wrap to the start, and each span is held until the
/// fence value it was retired with has completed. It owns no memory: UploadRing puts a buffer behind
/// it, and its tests run without a GPU.
class RingAllocator
{
public:
  /// Throws std::invalid_argument for a capacity of zero.
  explicit RingAllocator(std::uint64_t _capacityBytes);

  /// The offset of a span of _sizeBytes aligned to _alignmentBytes, or nothing when the ring has no
  /// room until the GPU finishes older spans. Throws std::invalid_argument for a size of zero, a size
  /// above the capacity, or an alignment that is not a power of two.
  [[nodiscard]] std::optional<std::uint64_t> Allocate(std::uint64_t _sizeBytes, std::uint64_t _alignmentBytes);

  /// Holds every span allocated since the last call until _fenceValue completes. Fence values must
  /// not decrease; throws std::invalid_argument when one does.
  void Retire(std::uint64_t _fenceValue);

  /// Frees every retired span whose fence value is at or below _completedFenceValue.
  void Reclaim(std::uint64_t _completedFenceValue) noexcept;

  [[nodiscard]] std::uint64_t CapacityBytes() const noexcept;

  /// Bytes held, counting alignment padding and the end a wrap skipped.
  [[nodiscard]] std::uint64_t UsedBytes() const noexcept;

private:
  struct RetiredSpan
  {
    std::uint64_t fenceValue;
    std::uint64_t endOffsetBytes;
    std::uint64_t sizeBytes;
  };

  std::uint64_t m_capacityBytes;
  std::uint64_t m_headBytes = 0; // where the next span starts
  std::uint64_t m_tailBytes = 0; // where the oldest held span starts
  std::uint64_t m_usedBytes = 0;
  std::uint64_t m_unretiredBytes = 0;
  std::uint64_t m_lastFenceValue = 0;
  std::deque<RetiredSpan> m_retired;
};

} // namespace Engine
