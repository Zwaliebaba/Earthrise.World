#pragma once

#include "ChunkCoordinate.h"
#include "ChunkSnapshot.h"
#include "ChunkStore.h"

#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <vector>

namespace Simulation
{

/// The world's voxels, edited through one call (ADR-014, decisions 1 and 2). The generator fills the
/// base through Base(); after that, every change goes through Edit, which records it twice: the
/// chunk joins the set of edited chunks, which a save writes, and the voxel joins the tick's edit
/// list, which the support search, slump and the navigation rebuild read. The chunks are whole in
/// memory, copied on write as ChunkStore does.
///
/// Not thread-safe: the simulation's thread calls every member.
class VoxelWorld
{
public:
  VoxelWorld() = default;
  ~VoxelWorld() = default;
  VoxelWorld(const VoxelWorld&) = default;
  VoxelWorld& operator=(const VoxelWorld&) = default;
  // A move may allocate under the debug library's checked containers, whose std::map allocates in
  // its move constructor, so it is not noexcept (bugprone-exception-escape).
  VoxelWorld(VoxelWorld&&) noexcept(false) = default;
  VoxelWorld& operator=(VoxelWorld&&) noexcept(false) = default;

  /// The store, for the generator to fill the base. Throws std::logic_error once Edit has been
  /// called: from then on every change is an edit.
  [[nodiscard]] ChunkStore& Base();

  [[nodiscard]] const ChunkStore& Store() const noexcept;

  /// The palette index at a voxel.
  [[nodiscard]] std::uint8_t At(Protocol::VoxelPosition _position) const;

  /// Sets a voxel and records the edit. An edit to the value the voxel already holds changes and
  /// records nothing. Throws std::out_of_range for a voxel the chunk key cannot hold.
  void Edit(Protocol::VoxelPosition _position, std::uint8_t _paletteIndex);

  /// The voxels edited since the last EndTick, each once, in the order of its first edit.
  [[nodiscard]] std::span<const Protocol::VoxelPosition> TickEdits() const noexcept;

  /// Clears TickEdits, at the end of a tick.
  void EndTick();

  /// Lists a voxel among the tick's edits without editing it: what a load does for the edits a save
  /// holds as pending work (ADR-014, decision 4).
  void RestoreTickEdit(Protocol::VoxelPosition _position);

  /// The packed keys of every chunk edited since generation or load, in key order. A chunk dug and
  /// refilled stays in it; the save leaves out a chunk that is back to its base (ADR-014, decision
  /// 3). A load rebuilds it from the chunks the save holds.
  [[nodiscard]] const std::set<std::uint64_t>& EditedChunks() const noexcept;

  /// Sets a whole chunk as a save holds it, and marks it edited, without listing its voxels among
  /// the tick's edits: what a load does for each chunk its save holds (ADR-014, decision 4).
  void RestoreChunk(Protocol::ChunkCoordinate _coordinate, const Protocol::Chunk& _voxels);

  /// Core::Fnv1a64 over its voxels, x fastest, then z, then y, as Protocol::Chunk lays them out; 0
  /// for a chunk the store does not hold. Cached, and recomputed only after an edit.
  [[nodiscard]] std::uint64_t ChunkHash(std::uint64_t _key) const;

  /// Core::Fnv1a64 over each chunk that holds a voxel, in key order: its packed key, then its
  /// ChunkHash, each as a little-endian u64. A chunk with nothing in it counts as absent, so a
  /// chunk dug empty hashes as one never made (ADR-014, decision 5).
  [[nodiscard]] std::uint64_t VoxelHash() const;

  /// Publishes every chunk changed since the last call (ADR-002, decision 3).
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> Publish();

private:
  using PositionKey = std::tuple<std::int32_t, std::int32_t, std::int32_t>;

  ChunkStore m_store;
  bool m_edited = false;
  std::vector<Protocol::VoxelPosition> m_tickEdits;
  std::set<PositionKey> m_tickEdited;
  std::set<std::uint64_t> m_editedChunks;
  mutable std::map<std::uint64_t, std::uint64_t> m_chunkHashes;
};

} // namespace Simulation
