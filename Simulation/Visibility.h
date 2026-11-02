#pragma once

#include "ChunkCoordinate.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Simulation
{

/// What one player sees, a bit a cell of the map, bit z * width + x (ADR-025, decision 1). Stamped from scratch every
/// tick in step 11 by squared distance in whole cells, never through walls; derived, so never saved.
class Visibility
{
public:
  Visibility(std::int32_t _widthCells, std::int32_t _depthCells);

  /// Every cell unseen.
  void Clear() noexcept;
  /// Every cell within _radiusCells of (_xCells, _zCells), by squared distance, clipped to the map.
  void Stamp(std::int32_t _xCells, std::int32_t _zCells, std::int32_t _radiusCells) noexcept;
  /// Every cell within _radiusCells of the footprint _minVoxels to _maxVoxels, both included, by squared distance from
  /// its nearest cell; the footprint itself always.
  void StampBox(Protocol::VoxelPosition _minVoxels, Protocol::VoxelPosition _maxVoxels, std::int32_t _radiusCells) noexcept;
  /// Whether (_xCells, _zCells) is seen; false off the map.
  [[nodiscard]] bool Visible(std::int32_t _xCells, std::int32_t _zCells) const noexcept;
  /// Whether any cell of the box _x0 to _x1 by _z0 to _z1, both included, is seen.
  [[nodiscard]] bool AnyVisible(std::int32_t _x0, std::int32_t _z0, std::int32_t _x1, std::int32_t _z1) const noexcept;
  /// The bits, 64 cells a word, the lowest bit first.
  [[nodiscard]] std::span<const std::uint64_t> Bits() const noexcept;

private:
  /// Sets the cells _x0 to _x1 of row _z, clipped to the map.
  void SetRun(std::int32_t _z, std::int32_t _x0, std::int32_t _x1) noexcept;

  std::int32_t m_widthCells;
  std::int32_t m_depthCells;
  std::vector<std::uint64_t> m_bits;
};

} // namespace Simulation
