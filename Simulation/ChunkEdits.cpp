#include "pch.h"

#include "ChunkEdits.h"

#include "ByteWriter.h"

namespace Simulation
{

namespace
{

constexpr std::int32_t EDGE = Protocol::Chunk::EDGE_VOXELS;

/// A voxel's place in the chunk's order, x fastest, then z, then y.
std::int32_t X(std::size_t _index) noexcept
{
  return static_cast<std::int32_t>(_index % EDGE);
}

std::int32_t Z(std::size_t _index) noexcept
{
  return static_cast<std::int32_t>((_index / EDGE) % EDGE);
}

std::int32_t Y(std::size_t _index) noexcept
{
  return static_cast<std::int32_t>(_index / (static_cast<std::size_t>(EDGE) * EDGE));
}

} // namespace

std::optional<std::vector<std::byte>> EncodeChunkEdits(const Protocol::Chunk& _base, const Protocol::Chunk& _edited)
{
  const std::span<const std::uint8_t> base = _base.Voxels();
  const std::span<const std::uint8_t> edited = _edited.Voxels();
  std::size_t changed = 0;
  std::size_t runs = 0;
  for (std::size_t index = 0; index < edited.size(); ++index)
  {
    if (base[index] != edited[index])
    {
      ++changed;
    }
    if (index == 0 || edited[index] != edited[index - 1])
    {
      ++runs;
    }
  }
  if (changed == 0)
  {
    return std::nullopt;
  }
  Core::ByteWriter writer;
  if (2 + 3 * changed < 3 * runs)
  {
    writer.WriteU8(CHUNK_EDITS_LIST);
    writer.WriteU16(static_cast<std::uint16_t>(changed));
    for (std::size_t index = 0; index < edited.size(); ++index)
    {
      if (base[index] != edited[index])
      {
        writer.WriteU16(static_cast<std::uint16_t>(index));
        writer.WriteU8(edited[index]);
      }
    }
  }
  else
  {
    writer.WriteU8(CHUNK_EDITS_RUNS);
    std::size_t start = 0;
    for (std::size_t index = 1; index <= edited.size(); ++index)
    {
      if (index == edited.size() || edited[index] != edited[start])
      {
        writer.WriteU16(static_cast<std::uint16_t>(index - start));
        writer.WriteU8(edited[start]);
        start = index;
      }
    }
  }
  return writer.TakeBytes();
}

void ApplyChunkEdits(Core::ByteReader& _reader, Protocol::Chunk& _chunk)
{
  const std::uint8_t form = _reader.ReadU8();
  if (form == CHUNK_EDITS_LIST)
  {
    const std::uint16_t count = _reader.ReadU16();
    for (std::uint16_t number = 0; number < count; ++number)
    {
      const std::uint16_t index = _reader.ReadU16();
      const std::uint8_t paletteIndex = _reader.ReadU8();
      if (index >= Protocol::Chunk::VOXEL_COUNT)
      {
        throw std::invalid_argument("a chunk's edit lies outside the chunk");
      }
      _chunk.Set(X(index), Y(index), Z(index), paletteIndex);
    }
    return;
  }
  if (form != CHUNK_EDITS_RUNS)
  {
    throw std::invalid_argument("a chunk's edits in a form this build does not read");
  }
  std::size_t index = 0;
  while (index < Protocol::Chunk::VOXEL_COUNT)
  {
    const std::uint16_t length = _reader.ReadU16();
    const std::uint8_t paletteIndex = _reader.ReadU8();
    if (length == 0 || index + length > Protocol::Chunk::VOXEL_COUNT)
    {
      throw std::invalid_argument("a chunk's runs do not cover it exactly");
    }
    for (std::size_t end = index + length; index < end; ++index)
    {
      _chunk.Set(X(index), Y(index), Z(index), paletteIndex);
    }
  }
}

} // namespace Simulation
