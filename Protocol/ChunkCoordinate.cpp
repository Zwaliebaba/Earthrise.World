#include "pch.h"

#include "ChunkCoordinate.h"

namespace Protocol
{

namespace
{

constexpr std::uint64_t AXIS_MASK = (std::uint64_t{1} << CHUNK_KEY_AXIS_BITS) - 1;
constexpr std::int64_t AXIS_SIGN = std::int64_t{1} << (CHUNK_KEY_AXIS_BITS - 1);

constexpr std::uint64_t Encode(std::int32_t _value) noexcept
{
  return static_cast<std::uint64_t>(static_cast<std::uint32_t>(_value)) & AXIS_MASK;
}

constexpr std::int32_t Decode(std::uint64_t _bits) noexcept
{
  // Sign-extends the axis's low CHUNK_KEY_AXIS_BITS bits.
  const auto value = static_cast<std::int64_t>(_bits & AXIS_MASK);
  return static_cast<std::int32_t>((value ^ AXIS_SIGN) - AXIS_SIGN);
}

} // namespace

std::uint64_t PackChunkKey(ChunkCoordinate _coordinate) noexcept
{
  return Encode(_coordinate.xChunks) | (Encode(_coordinate.yChunks) << CHUNK_KEY_AXIS_BITS) |
         (Encode(_coordinate.zChunks) << (2 * CHUNK_KEY_AXIS_BITS));
}

ChunkCoordinate UnpackChunkKey(std::uint64_t _key) noexcept
{
  return {.xChunks = Decode(_key), .yChunks = Decode(_key >> CHUNK_KEY_AXIS_BITS), .zChunks = Decode(_key >> (2 * CHUNK_KEY_AXIS_BITS))};
}

} // namespace Protocol
