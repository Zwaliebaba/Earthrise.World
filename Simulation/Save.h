#pragma once

#include "World.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Simulation
{

// A save holds everything the next tick reads, and nothing it can rebuild (ADR-014, decision 4).
// Little-endian throughout, written with Core's byte writer, in four sections:
//   1. the header: the four bytes ERSV; u32 SAVE_FORMAT_VERSION; u64 the build id; u64 the
//      server-data hash; u32 the generator version; u64 the world, resource and match seeds; u32 the
//      tick; u64 the next entity id;
//   2. the edited chunks: u32 their count, then in rising key order each chunk that differs from its
//      generated base, as its u64 packed key and its edits in ChunkEdits.h's shorter form;
//   3. the entities: u32 their count, then every entity in id order, each as a u16 type tag, a u16
//      record version, a u32 length and the record (SaveEntityTag). Each record is at version 1 as
//      M3 leaves it: a save is tied to its build anyway (ADR-014, decision 6). A structure's record
//      is its id, u8 owner, template id and name each as a u32 length and UTF-8, origin and u8
//      quarter turns; M4's construction adds its built mask as version 2;
//   4. the pending work, each queue in its own stated order: u32 the commands that arrived for the
//      next tick, then each as its u8 player, u32 length and bytes, in arrival order; u32 the edits
//      the last tick made after its step 5, then each voxel, in edit order; and u32 the edits made
//      between ticks since, then each voxel, in first-edit order, which the next step 5 searches with
//      them; u32 the columns waiting for slump, then each column index, z * width + x, as an i64,
//      in rising order; and u32 the path searches still running, then each as the u32 tick it was
//      asked for in, its u64 squad and the search as PathSearch::Write gives it, in that order.
// Navigation tiles, chunk hashes and anything else computed from the voxels are not saved: a load
// rebuilds them.

inline constexpr std::uint32_t SAVE_FORMAT_VERSION = 1;

/// The type tag each kind of entity is saved under.
enum class SaveEntityTag : std::uint16_t
{
  Squad = 1,
  Unit = 2,
  SiegeEngine = 3,
  Shot = 4,
  Collapse = 5,
  Structure = 6
};

/// Writes _world, between ticks.
[[nodiscard]] std::vector<std::byte> WriteSave(const World& _world);

/// A world from a save: generated from _desc with the save's seeds, its edited chunks restored, and
/// its tick, entities and pending work as the save holds them. Throws std::runtime_error naming the
/// check that failed when the save is from another format version, build, server data or generator
/// version (ADR-014, decision 6), or does not read.
[[nodiscard]] World LoadSave(std::span<const std::byte> _save, const World::Desc& _desc);

/// The state hash (ADR-014, decision 5): Core::Fnv1a64, starting from the voxels' VoxelHash, over
/// the tick and the next entity id as the header writes them, then the entities and the pending work
/// as a save writes them. Two worlds with equal hashes run on identically.
[[nodiscard]] std::uint64_t StateHash(const World& _world);

} // namespace Simulation
