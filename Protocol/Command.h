#pragma once

#include "ByteReader.h"
#include "ByteWriter.h"
#include "ChunkCoordinate.h"
#include "EntityId.h"

#include <cstdint>
#include <variant>
#include <vector>

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

// M4's commands (MvpPlan-M4.md, Contracts; ADR-023 and ADR-024), each with its own tag, in the variant's order.

/// Orders a squad to attack: a squad, an engine, or, for a ram, a structure (ADR-023, decision 4). Tag 3.
struct AttackTarget
{
  EntityId squad;
  EntityId target;

  friend constexpr bool operator==(const AttackTarget&, const AttackTarget&) = default;
};

/// Orders a squad to a goal, engaging what it meets on the way (ADR-023, decision 4). Tag 4.
struct StormTo
{
  EntityId squad;
  VoxelPosition goalVoxels;

  friend constexpr bool operator==(const StormTo&, const StormTo&) = default;
};

/// Tag 5.
struct StopSquad
{
  EntityId squad;

  friend constexpr bool operator==(const StopSquad&, const StopSquad&) = default;
};

/// Strike from where it stands, and never leave (ADR-023, decision 4). Tag 6.
struct HoldSquad
{
  EntityId squad;

  friend constexpr bool operator==(const HoldSquad&, const HoldSquad&) = default;
};

/// The highest-id figure leaves as a squad of one (Hud.md §7). Tag 7.
struct SplitSquad
{
  EntityId squad;

  friend constexpr bool operator==(const SplitSquad&, const SplitSquad&) = default;
};

/// Squads of one kind and owner combined up to full size (Hud.md §7). On the wire a u8 count, then the ids. Tag 8.
struct MergeSquads
{
  std::vector<EntityId> squads;

  friend bool operator==(const MergeSquads&, const MergeSquads&) = default;
};

/// Tag 9.
struct GarrisonSquad
{
  EntityId squad;
  EntityId structure;

  friend constexpr bool operator==(const GarrisonSquad&, const GarrisonSquad&) = default;
};

/// Tag 10.
struct EjectGarrison
{
  EntityId structure;

  friend constexpr bool operator==(const EjectGarrison&, const EjectGarrison&) = default;
};

/// A laborer, a squad of one, set to repair a structure (ADR-024, decision 3). Tag 11.
struct RepairStructure
{
  EntityId laborer;
  EntityId structure;

  friend constexpr bool operator==(const RepairStructure&, const RepairStructure&) = default;
};

/// A box, both corners included, dug from the top (MvpPlan.md, M4, input 12). Tag 12.
struct DigTrench
{
  EntityId squad;
  VoxelPosition minVoxels;
  VoxelPosition maxVoxels;

  friend constexpr bool operator==(const DigTrench&, const DigTrench&) = default;
};

/// Tag 13.
struct PackEngine
{
  EntityId engine;

  friend constexpr bool operator==(const PackEngine&, const PackEngine&) = default;
};

/// Tag 14.
struct DeployEngine
{
  EntityId engine;
  std::uint32_t quarterTurns;

  friend constexpr bool operator==(const DeployEngine&, const DeployEngine&) = default;
};

/// Opens or shuts a gate (ADR-024, decision 6). On the wire open is a u8, 0 or 1. Tag 15.
struct SetGate
{
  EntityId structure;
  bool open;

  friend constexpr bool operator==(const SetGate&, const SetGate&) = default;
};

/// Every command, in tag order: a command's tag is its index here plus 1. An engine moves as its body's squad, by MoveSquad.
using Command = std::variant<MoveSquad, FireEngine, AttackTarget, StormTo, StopSquad, HoldSquad, SplitSquad, MergeSquads, GarrisonSquad,
                             EjectGarrison, RepairStructure, DigTrench, PackEngine, DeployEngine, SetGate>;

/// A command's tag, as its bytes begin with it and a refusal names it (ADR-023, decision 10).
[[nodiscard]] std::uint16_t CommandTag(const Command& _command) noexcept;

/// The one record version each command is written at. A new field is a new version.
inline constexpr std::uint16_t COMMAND_RECORD_VERSION = 1;

/// Writes _command's tag, version and fields. Throws std::invalid_argument for a MergeSquads of more than 255 squads.
void Encode(const Command& _command, Core::ByteWriter& _writer);

/// Reads one command. Throws std::invalid_argument for a tag it does not know or a version other
/// than COMMAND_RECORD_VERSION, and std::out_of_range when the bytes end first.
[[nodiscard]] Command DecodeCommand(Core::ByteReader& _reader);

} // namespace Protocol
