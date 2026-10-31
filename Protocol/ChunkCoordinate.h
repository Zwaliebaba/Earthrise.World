#pragma once

#include <cstdint>

namespace Protocol
{

// A chunk is a cube of 32 world voxels of 1 m on each edge (ADR-006, ADR-010).
inline constexpr std::int32_t CHUNK_EDGE_VOXELS = 32;

/// Where a chunk sits in the world, counted in chunks. Negative coordinates are valid.
struct ChunkCoordinate
{
  std::int32_t xChunks;
  std::int32_t yChunks;
  std::int32_t zChunks;

  friend constexpr bool operator==(const ChunkCoordinate&, const ChunkCoordinate&) = default;
};

// 21 bits per axis in a packed key: about a million chunks either way of the origin, far beyond
// the largest world (about 2 km, or 63 chunks, across).
inline constexpr std::int32_t CHUNK_KEY_AXIS_BITS = 21;

/// Packs a coordinate into one 64-bit key, for maps and for the wire. Unpacking returns it exactly.
[[nodiscard]] std::uint64_t PackChunkKey(ChunkCoordinate _coordinate) noexcept;
[[nodiscard]] ChunkCoordinate UnpackChunkKey(std::uint64_t _key) noexcept;

} // namespace Protocol
