#include "pch.h"

#include "ChunkStore.h"

namespace Simulation
{

std::uint8_t ChunkStore::At(Protocol::VoxelPosition _position) const
{
  const Protocol::ChunkCoordinate coordinate = Protocol::ChunkContaining(_position);
  if (!Protocol::FitsChunkKey(coordinate))
  {
    return Protocol::Chunk::EMPTY;
  }
  const auto found = m_chunks.find(Protocol::PackChunkKey(coordinate));
  if (found == m_chunks.end())
  {
    return Protocol::Chunk::EMPTY;
  }
  const Entry& entry = found->second;
  const Protocol::Chunk* current = entry.pending ? entry.pending.get() : entry.published.get();
  const Protocol::VoxelPosition inside = Protocol::PositionInChunk(_position);
  return current->At(inside.xVoxels, inside.yVoxels, inside.zVoxels);
}

void ChunkStore::Set(Protocol::VoxelPosition _position, std::uint8_t _paletteIndex)
{
  const Protocol::ChunkCoordinate coordinate = Protocol::ChunkContaining(_position);
  if (!Protocol::FitsChunkKey(coordinate))
  {
    throw std::out_of_range("voxel in a chunk the packed chunk key cannot hold");
  }
  const std::uint64_t key = Protocol::PackChunkKey(coordinate);
  const Protocol::VoxelPosition inside = Protocol::PositionInChunk(_position);

  auto found = m_chunks.find(key);
  if (found == m_chunks.end())
  {
    if (_paletteIndex == Protocol::Chunk::EMPTY)
    {
      return;
    }
    found = m_chunks.emplace(key, Entry{}).first;
  }
  Entry& entry = found->second;

  const Protocol::Chunk* current = entry.pending ? entry.pending.get() : entry.published.get();
  if (current != nullptr && current->At(inside.xVoxels, inside.yVoxels, inside.zVoxels) == _paletteIndex)
  {
    return;
  }
  if (!entry.pending)
  {
    // The first change since the chunk was published: copy it, so no snapshot ever changes.
    entry.pending = entry.published ? std::make_shared<Protocol::Chunk>(*entry.published) : std::make_shared<Protocol::Chunk>();
    m_changedKeys.push_back(key);
  }
  entry.pending->Set(inside.xVoxels, inside.yVoxels, inside.zVoxels, _paletteIndex);
}

std::vector<Protocol::ChunkSnapshot> ChunkStore::Publish()
{
  std::vector<Protocol::ChunkSnapshot> snapshots;
  if (m_changedKeys.empty())
  {
    return snapshots;
  }
  ++m_version;
  std::ranges::sort(m_changedKeys);
  snapshots.reserve(m_changedKeys.size());
  for (const std::uint64_t key : m_changedKeys)
  {
    Entry& entry = m_chunks.at(key);
    entry.published = std::move(entry.pending);
    entry.publishedVersion = m_version;
    snapshots.push_back({.coordinate = Protocol::UnpackChunkKey(key), .version = m_version, .voxels = entry.published});
  }
  m_changedKeys.clear();
  return snapshots;
}

std::vector<Protocol::ChunkSnapshot> ChunkStore::Snapshots() const
{
  std::vector<Protocol::ChunkSnapshot> snapshots;
  for (const auto& [key, entry] : m_chunks)
  {
    if (entry.published)
    {
      snapshots.push_back({.coordinate = Protocol::UnpackChunkKey(key), .version = entry.publishedVersion, .voxels = entry.published});
    }
  }
  return snapshots;
}

std::uint64_t ChunkStore::Version() const noexcept
{
  return m_version;
}

} // namespace Simulation
