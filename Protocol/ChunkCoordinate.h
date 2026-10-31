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
inline constexpr std::int32_t CHUNK_KEY_AXIS_MIN_CHUNKS = -(1 << (CHUNK_KEY_AXIS_BITS - 1));
inline constexpr std::int32_t CHUNK_KEY_AXIS_MAX_CHUNKS = (1 << (CHUNK_KEY_AXIS_BITS - 1)) - 1;

/// Packs a coordinate into one 64-bit key, for maps and for the wire. Unpacking returns it exactly
/// when FitsChunkKey holds; outside that range two coordinates share a key.
[[nodiscard]] std::uint64_t PackChunkKey(ChunkCoordinate _coordinate) noexcept;
[[nodiscard]] ChunkCoordinate UnpackChunkKey(std::uint64_t _key) noexcept;

/// Whether every axis lies in [CHUNK_KEY_AXIS_MIN_CHUNKS, CHUNK_KEY_AXIS_MAX_CHUNKS].
[[nodiscard]] bool FitsChunkKey(ChunkCoordinate _coordinate) noexcept;

/// Where a voxel sits in the world, counted in 1 m voxels. Negative coordinates are valid.
struct VoxelPosition
{
  std::int32_t xVoxels;
  std::int32_t yVoxels;
  std::int32_t zVoxels;

  friend constexpr bool operator==(const VoxelPosition&, const VoxelPosition&) = default;
};

/// The chunk that holds a voxel. It rounds down, so voxel -1 is in chunk -1, not chunk 0.
[[nodiscard]] ChunkCoordinate ChunkContaining(VoxelPosition _position) noexcept;

/// A voxel's position inside the chunk that holds it, each axis in [0, CHUNK_EDGE_VOXELS).
[[nodiscard]] VoxelPosition PositionInChunk(VoxelPosition _position) noexcept;

} // namespace Protocol
