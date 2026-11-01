#include "pch.h"

#include "VoxelWorld.h"

#include "Hash.h"

namespace Simulation
{

namespace
{

void Fold(std::uint64_t& _hash, std::uint64_t _value) noexcept
{
  std::array<std::byte, 8> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index)
  {
    bytes.at(index) = static_cast<std::byte>((_value >> (8 * index)) & 0xffU);
  }
  _hash = Core::Fnv1a64(bytes, _hash);
}

} // namespace

ChunkStore& VoxelWorld::Base()
{
  if (m_edited)
  {
    throw std::logic_error("the world's base is the generator's, and it has been edited since");
  }
  m_chunkHashes.clear();
  return m_store;
}

const ChunkStore& VoxelWorld::Store() const noexcept
{
  return m_store;
}

std::uint8_t VoxelWorld::At(Protocol::VoxelPosition _position) const
{
  return m_store.At(_position);
}

void VoxelWorld::Edit(Protocol::VoxelPosition _position, std::uint8_t _paletteIndex)
{
  if (m_store.At(_position) == _paletteIndex)
  {
    return;
  }
  m_store.Set(_position, _paletteIndex);
  m_edited = true;
  const std::uint64_t key = Protocol::PackChunkKey(Protocol::ChunkContaining(_position));
  m_editedChunks.insert(key);
  m_chunkHashes.erase(key);
  if (m_tickEdited.insert({_position.xVoxels, _position.yVoxels, _position.zVoxels}).second)
  {
    m_tickEdits.push_back(_position);
  }
}

std::span<const Protocol::VoxelPosition> VoxelWorld::TickEdits() const noexcept
{
  return m_tickEdits;
}

void VoxelWorld::EndTick()
{
  m_tickEdits.clear();
  m_tickEdited.clear();
}

const std::set<std::uint64_t>& VoxelWorld::EditedChunks() const noexcept
{
  return m_editedChunks;
}

void VoxelWorld::RestoreChunk(Protocol::ChunkCoordinate _coordinate, const Protocol::Chunk& _voxels)
{
  m_store.SetChunk(_coordinate, _voxels);
  m_edited = true;
  const std::uint64_t key = Protocol::PackChunkKey(_coordinate);
  m_editedChunks.insert(key);
  m_chunkHashes.erase(key);
}

std::uint64_t VoxelWorld::ChunkHash(std::uint64_t _key) const
{
  const auto cached = m_chunkHashes.find(_key);
  if (cached != m_chunkHashes.end())
  {
    return cached->second;
  }
  const Protocol::Chunk* chunk = m_store.Find(_key);
  const std::uint64_t hash = chunk == nullptr ? 0 : Core::Fnv1a64(std::as_bytes(chunk->Voxels()));
  m_chunkHashes.emplace(_key, hash);
  return hash;
}

std::uint64_t VoxelWorld::VoxelHash() const
{
  std::uint64_t hash = Core::FNV_OFFSET_BASIS;
  for (const std::uint64_t key : m_store.Keys())
  {
    const Protocol::Chunk* chunk = m_store.Find(key);
    if (chunk == nullptr || chunk->SolidCount() == 0)
    {
      continue;
    }
    Fold(hash, key);
    Fold(hash, ChunkHash(key));
  }
  return hash;
}

std::vector<Protocol::ChunkSnapshot> VoxelWorld::Publish()
{
  return m_store.Publish();
}

} // namespace Simulation
