#pragma once

#include "ByteReader.h"
#include "ByteWriter.h"
#include "ChunkCoordinate.h"
#include "EntityId.h"

#include <cstdint>
#include <variant>

namespace Protocol
{

// What a player asks the simulation to do. Commands are bytes from the first line (ADR-002, decision
// 3): each is written as a u16 tag, a u16 record version and then its fields in declaration order,
// with Core's byte writer, and those bytes are what the transport carries and a replay records
// (ADR-014, decision 5).

/// Orders a squad towards a goal, which it reaches as near as its class can (ADR-015, decision 5).
/// Tag 1.
struct MoveSquad
{
  EntityId squad;
  VoxelPosition goalVoxels;

  friend constexpr bool operator==(const MoveSquad&, const MoveSquad&) = default;
};

/// Orders a siege engine to shoot at a voxel. Tag 2.
struct FireEngine
{
  EntityId engine;
  VoxelPosition targetVoxels;

  friend constexpr bool operator==(const FireEngine&, const FireEngine&) = default;
};

using Command = std::variant<MoveSquad, FireEngine>;

/// The one record version each command is written at. A new field is a new version.
inline constexpr std::uint16_t COMMAND_RECORD_VERSION = 1;

/// Writes _command's tag, version and fields.
void Encode(const Command& _command, Core::ByteWriter& _writer);

/// Reads one command. Throws std::invalid_argument for a tag it does not know or a version other
/// than COMMAND_RECORD_VERSION, and std::out_of_range when the bytes end first.
[[nodiscard]] Command DecodeCommand(Core::ByteReader& _reader);

} // namespace Protocol
