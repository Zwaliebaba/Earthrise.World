#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ChunkCoordinate.h"

namespace Protocol
{

/// One chunk of the world grid: 32 × 32 × 32 voxels of 1 m, each holding its palette index
/// (ADR-005, decision 5), with 0 for empty. Voxels are stored x fastest, then z, then y, the order
/// of the layer files' layers[y][z][x]. It is the payload of a ChunkSnapshot, so the client reads
/// it as the simulation wrote it.
class Chunk
{
public:
  static constexpr std::int32_t EDGE_VOXELS = CHUNK_EDGE_VOXELS;
  static constexpr std::size_t VOXEL_COUNT = static_cast<std::size_t>(EDGE_VOXELS) * EDGE_VOXELS * EDGE_VOXELS;
  static constexpr std::uint8_t EMPTY = 0;

  /// The palette index at a voxel inside the chunk. Throws std::out_of_range outside it.
  [[nodiscard]] std::uint8_t At(std::int32_t _xVoxels, std::int32_t _yVoxels, std::int32_t _zVoxels) const;

  /// Sets a voxel's palette index. Throws std::out_of_range outside the chunk.
  void Set(std::int32_t _xVoxels, std::int32_t _yVoxels, std::int32_t _zVoxels, std::uint8_t _paletteIndex);

  /// How many voxels are not empty.
  [[nodiscard]] std::size_t SolidCount() const noexcept;

  /// Every voxel, x fastest, then z, then y, as Engine::VoxelGridView and the layer files lay them out.
  [[nodiscard]] std::span<const std::uint8_t> Voxels() const noexcept
  {
    return m_voxels;
  }

private:
  [[nodiscard]] static std::size_t IndexOf(std::int32_t _xVoxels, std::int32_t _yVoxels, std::int32_t _zVoxels);

  std::array<std::uint8_t, VOXEL_COUNT> m_voxels{};
  std::size_t m_solidCount = 0;
};

} // namespace Protocol
