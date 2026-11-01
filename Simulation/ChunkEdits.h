#pragma once

#include "ByteReader.h"
#include "Chunk.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Simulation
{

// A chunk's edits against its generated base, as a save writes them (ADR-014, decision 3): a u8 form,
// then the form's bytes, whichever form is shorter.
//   0, a list of changed voxels: a u16 count, then for each voxel, in rising index order, its u16
//      index in the chunk (x fastest, then z, then y, as Protocol::Chunk lays them out) and its u8
//      palette index: 2 + 3n bytes after the form.
//   1, the whole chunk, run-length encoded: runs of a u16 length and a u8 palette index, covering
//      the chunk in the same order: 3r bytes after the form.
// 2 + 3n is never a multiple of 3, so the two can never tie, and ADR-014's tie rule is moot.

inline constexpr std::uint8_t CHUNK_EDITS_LIST = 0;
inline constexpr std::uint8_t CHUNK_EDITS_RUNS = 1;

/// The shorter form of what _edited changed in _base, or nothing when the two are equal: a chunk
/// back to its base is not written.
[[nodiscard]] std::optional<std::vector<std::byte>> EncodeChunkEdits(const Protocol::Chunk& _base, const Protocol::Chunk& _edited);

/// Reads one chunk's edits and makes them in _chunk, which holds its base. Throws
/// std::invalid_argument for a form it does not know, an index outside the chunk, or runs that do
/// not cover it exactly, and std::out_of_range when the bytes end first.
void ApplyChunkEdits(Core::ByteReader& _reader, Protocol::Chunk& _chunk);

} // namespace Simulation
