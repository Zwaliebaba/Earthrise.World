#pragma once

#include <cstdint>
#include <memory>

#include "Chunk.h"
#include "ChunkCoordinate.h"

namespace Protocol
{

/// A handle to one published version of a chunk (ADR-002, decision 3).
///
/// The voxels it points to never change. The simulation's next edit of the chunk goes to a copy, so
/// a client may keep a snapshot for as long as it likes and read it from any thread. Until that
/// edit, the simulation and every client share one buffer, so single-player holds one copy of the
/// world.
struct ChunkSnapshot
{
  ChunkCoordinate coordinate;

  /// The publication that made this version. Versions rise with every publication that changes the
  /// chunk, and they are comparable across chunks: one publication gives all its chunks one version.
  std::uint64_t version;

  /// Never null.
  std::shared_ptr<const Chunk> voxels;
};

} // namespace Protocol
