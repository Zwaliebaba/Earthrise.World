#pragma once

#include "World.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Simulation
{

// A save holds everything the next tick reads, and nothing it can rebuild (ADR-014, decision 4).
// Little-endian throughout, written with Core's byte writer, in six sections:
//   1. the header: the four bytes ERSV; u32 SAVE_FORMAT_VERSION; u64 the build id; u64 the
//      server-data hash; u32 the generator version; u64 the world, resource and match seeds; u32 the
//      tick; u64 the next entity id;
//   1a. the match (M4-B4): u32 the players, then each as u8 its id and its side (Protocol::Side) and
//      i32 its stone and its timber in hundredths of a cubic metre, and u8 whether its map is revealed (M4-E1), in player
//      order; then u8 1 and the
//      trial's rules, or u8 0 for none: u8 its version, its attacker and its defender, u64 its keep,
//      u32 its time limit and its hold in ticks, i32 its ring in cells, and the camp's two corners;
//   1b. the trial's progress (M4-F1): u8 its outcome, u32 the tick of it and u32 the storming clock; u32 the sections
//      opened, then each u64 in id order; u32 the figures in a section's footprint, then each as its u64 id, the u64
//      section and u8 1 when it came in from the inner side, in figure order;
//   1c. the commanders (M4-F2; ADR-027, decision 5): u32 their count, then each as its u8 player, its kind as a u32 length
//      and UTF-8, and its state as a u32 length and the bytes its Write gives, in player order; then the commands they
//      gave in the last step 13, u32 their count and each as its u8 player, u32 length and bytes, in order;
//   2. the edited chunks: u32 their count, then in rising key order each chunk that differs from its
//      generated base, as its u64 packed key and its edits in ChunkEdits.h's shorter form;
//   3. the entities: u32 their count, then every entity in id order, each as a u16 type tag, a u16
//      record version, a u32 length and the record (SaveEntityTag). Each kind has its own record
//      version (RecordVersion), and an order that adds a field raises its kind's; a save is tied to
//      its build anyway (ADR-014, decision 6), so an older version is refused, never migrated. A
//      structure's record is its id, u8 owner, template id and name each as a u32 length and UTF-8,
//      origin and u8 quarter turns; M4-D1's version 2 adds its built mask, as a u32 byte count and the
//      bytes, its u8 share, u8 working, u32 lock, Fixed repair progress, u8 gate and i32 hit points, and M4-F2's version 3
//      its u8 standing share. A squad's version 6 adds
//      its trench's two corners, its u8 ramp's end and its Fixed dig progress (M4-D6). A squad's version 2 adds
//      the u64 building it is garrisoned in, and an engine's version 2 its u8 state (M4-B4); a
//      squad's version 3 its u8 kind, an engine's version 3 its u64 body's squad, and a unit's
//      version 2 its i32 hit points (M4-C1); a squad's version 4 its u8 facing, and a unit's version
//      3 its i64 reserved cell, its i64 step's first cell and its u32 wait (M4-C2); a squad's
//      version 5 its u8 order and u64 target, and a unit's version 4 its u32 reload and u64 target
//      (M4-C3); a shot's version 2 puts its u8 kind after its owner, holds an arrow's line, its start
//      and end and its Fixed flight ticks, where a stone's arc stands, and ends with its u64 shooter,
//      u64 target and i32 damage (M4-C4); an engine's version 4 its u32 ticks left of a
//      deploy or a pack (M4-C5);
//   4. the pending work, each queue in its own stated order: u32 the commands that arrived for the
//      next tick, then each as its u8 player, u32 length and bytes, in arrival order; u32 the edits
//      the last tick made after its step 5, then each voxel, in edit order; and u32 the edits made
//      between ticks since, then each voxel, in first-edit order, which the next step 5 searches with
//      them; u32 the columns waiting for slump, then each column index, z * width + x, as an i64,
//      in rising order; u32 the columns trenches opened to slump from under a structure, then each
//      column index as an i64, in rising order (M4-D6; ADR-029); and u32 the path searches still running, then each as the u32 tick it was
//      asked for in, its u64 squad and the search as PathSearch::Write gives it, in that order;
//   5. fog's (M4-E1; ADR-025): u32 the players with structures last seen, then each as its u8 id, u32 their count and
//      each structure's view, in player then id order, which ends with u32 its open gate's voxels and each voxel (M4-H1); then u32 the players sent chunks, then each as its u8 id, u32 their
//      count and each chunk's u64 packed key and u64 version last sent, in key order. The state hash holds the first and
//      not the second, which the client's calls to Publish set.
// Navigation tiles, chunk hashes, visibility and anything else computed from the voxels and the entities are not saved: a
// load rebuilds them.

/// 2 from M4-B4, which gave the save its match; 3 from M4-D6, which saves the columns trenches opened; 4 from M4-E1, which
/// saves fog's last-seen structures and chunks sent; 5 from M4-F1, which saves the trial's progress; 6 from M4-F2, which
/// saves the commanders; 7 from M4-H1, which saves an open gate's voxels with each structure last seen.
inline constexpr std::uint32_t SAVE_FORMAT_VERSION = 7;

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

/// The record version each kind of entity is written at.
[[nodiscard]] std::uint16_t RecordVersion(SaveEntityTag _tag) noexcept;

/// Writes _world, between ticks.
[[nodiscard]] std::vector<std::byte> WriteSave(const World& _world);

/// A world from a save: generated from _desc with the save's seeds, its edited chunks restored, and
/// its tick, entities and pending work as the save holds them. Throws std::runtime_error naming the
/// check that failed when the save is from another format version, build, server data or generator
/// version (ADR-014, decision 6), or does not read.
[[nodiscard]] World LoadSave(std::span<const std::byte> _save, const World::Desc& _desc);

/// The state hash (ADR-014, decision 5): Core::Fnv1a64, starting from the voxels' VoxelHash, over
/// the tick and the next entity id as the header writes them, then the match, the entities and the
/// pending work as a save writes them. Two worlds with equal hashes run on identically.
[[nodiscard]] std::uint64_t StateHash(const World& _world);

} // namespace Simulation
