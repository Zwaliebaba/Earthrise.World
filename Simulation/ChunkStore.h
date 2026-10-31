#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "Chunk.h"
#include "ChunkCoordinate.h"
#include "ChunkSnapshot.h"

namespace Simulation
{

/// The world's voxels, chunk by chunk, and the snapshots the client draws from (ADR-002,
/// decision 3).
///
/// The simulation reads and sets voxels here. At the end of a tick it calls Publish(), which hands
/// the client every chunk changed since the last call as a new immutable snapshot. A chunk is copied
/// only by its first change after a publication, so between publications a changed chunk has two
/// copies, the published one and the one being changed, and every other chunk has one, shared with
/// every snapshot of it. A chunk that has never been set reads as empty and has no snapshot.
///
/// Not thread-safe: the simulation's thread calls every member. The snapshots it returns may be read
/// from any thread.
class ChunkStore
{
public:
  /// The palette index at a voxel, as the simulation last set it, whether published or not.
  [[nodiscard]] std::uint8_t At(Protocol::VoxelPosition _position) const;

  /// Sets a voxel; the client sees it after the next Publish(). Setting a voxel to what it already
  /// holds changes nothing. Throws std::out_of_range for a voxel in a chunk the packed key cannot
  /// hold (Protocol::FitsChunkKey).
  void Set(Protocol::VoxelPosition _position, std::uint8_t _paletteIndex);

  /// Publishes every chunk changed since the last call and returns their new snapshots, in key
  /// order. When nothing changed it returns none, and the version does not move.
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> Publish();

  /// The latest published snapshot of every chunk that has one, in key order: what a client that
  /// joins now starts from. Changes not yet published are not in it.
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> Snapshots() const;

  /// The version of the latest publication that changed something, or 0 before the first.
  [[nodiscard]] std::uint64_t Version() const noexcept;

private:
  struct Entry
  {
    std::shared_ptr<const Protocol::Chunk> published; // null until the chunk is first published
    std::uint64_t publishedVersion = 0;
    std::shared_ptr<Protocol::Chunk> pending; // the copy being changed; null when unchanged since published
  };

  // Ordered by packed key, so that Publish() and Snapshots() return chunks in an order that does not
  // depend on the history of the map (R16).
  std::map<std::uint64_t, Entry> m_chunks;
  std::vector<std::uint64_t> m_changedKeys;
  std::uint64_t m_version = 0;
};

} // namespace Simulation
