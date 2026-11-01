#pragma once

#include "ByteReader.h"
#include "ByteWriter.h"
#include "ChunkCoordinate.h"
#include "EntityId.h"
#include "EntityKinds.h"

#include <array>
#include <cstdint>
#include <variant>

namespace Protocol
{

// What a client asks the simulation, and the simulation's answer (ADR-009; ADR-020, decisions 5 and
// 6). Both are bytes, written with Core's byte writer as commands are. A request is a u32 query id,
// a u16 tag, a u16 record version and the query's fields in declaration order; an answer is the u32
// query id, the u32 tick it was computed on, a u16 tag, a u16 record version and the reading's
// fields. Tags: 1 aim check, 2 section integrity, 3 tile reach. Queries change nothing, and replays
// do not record them (ADR-009, decision 2).

/// Can an engine's shot reach a voxel, and what would it strike (Hud.md §8, the attack cursor)?
struct AimCheck
{
  EntityId engine;
  VoxelPosition targetVoxels;

  friend constexpr bool operator==(const AimCheck&, const AimCheck&) = default;
};

/// How close a section is to opening, read from the resolver (ADR-021, decision 4).
struct SectionIntegrity
{
  EntityId structure;

  friend constexpr bool operator==(const SectionIntegrity&, const SectionIntegrity&) = default;
};

/// Which of a navigation tile's cells a class can enter and reach from a start (ADR-015).
struct TileReach
{
  NavigationClass navigationClass;
  VoxelPosition startCellVoxels;
  std::int32_t tileX;
  std::int32_t tileZ;

  friend constexpr bool operator==(const TileReach&, const TileReach&) = default;
};

using Query = std::variant<AimCheck, SectionIntegrity, TileReach>;

struct QueryRequest
{
  std::uint32_t id; // the client's, so that it keeps the newest answer to each (ADR-009, decision 3)
  Query query;

  friend constexpr bool operator==(const QueryRequest&, const QueryRequest&) = default;
};

/// The one record version each query and reading is written at. A new field is a new version.
inline constexpr std::uint16_t QUERY_RECORD_VERSION = 1;

/// Hud.md §8's four readings of the attack cursor.
enum class AimVerdict : std::uint8_t
{
  InRange,
  Blocked,
  TooClose,
  OutOfRange
};

struct AimReading
{
  AimVerdict verdict;
  std::int32_t distanceMeters; // across the ground, floored
  bool strikes;                // the shot's own arc meets a solid voxel before it leaves the world
  VoxelPosition strikeVoxels;  // the first solid voxel on it, when it strikes

  friend constexpr bool operator==(const AimReading&, const AimReading&) = default;
};

enum class GroundKind : std::uint8_t
{
  Earth,
  Rock,
  Bedrock
};

enum class OpeningFloor : std::uint8_t
{
  Ground,
  Rubble,
  Stone
};

/// The paper test's support readout (Design/Review/PaperTests/BreachPrediction.md, *The readouts*),
/// for one section: its widest hole, its lowest opening and its gap under the foundation, each with
/// what the resolver makes of it, its ground and its weak point.
struct IntegrityReading
{
  EntityId structure;
  GroundKind ground;            // earth if any column under the lowest course is earth, else rock, else bedrock
  bool canBeUndermined;         // a column stands on earth
  std::uint8_t holeWidthVoxels; // the widest opening through the whole thickness below the battlements; 0 for none
  std::uint8_t holeHeightVoxels;
  std::uint8_t holeFallWidthVoxels; // the width at which the resolver drops the stone above it; 0 when none does within the section
  bool hasOpening;
  std::uint8_t openingFloorVoxels; // the lowest opening's floor above the section's ground
  OpeningFloor openingFloor;
  std::uint8_t openingClearVoxels;
  bool routeNow;                 // foot can stand on the opening's floor
  std::uint8_t gapRunVoxels;     // the longest run of columns with nothing under the lowest course across the whole thickness
  std::uint8_t gapFallRunVoxels; // the run at which the resolver drops the stone above; 0 when none does within the section
  bool falling;                  // a collapse of this structure is in warning or falling
  VoxelPosition weakMinVoxels;   // the weak point's box, the reading nearest its mark, both corners included
  VoxelPosition weakMaxVoxels;

  friend constexpr bool operator==(const IntegrityReading&, const IntegrityReading&) = default;
};

inline constexpr std::size_t TILE_REACH_BYTES = 128; // 32 x 32 cells, a bit each

struct TileReachReading
{
  std::int32_t tileX;
  std::int32_t tileZ;
  std::array<std::uint8_t, TILE_REACH_BYTES> usable;  // bit z * 32 + x: the class can enter the cell
  std::array<std::uint8_t, TILE_REACH_BYTES> reached; // and can reach it from the start

  friend constexpr bool operator==(const TileReachReading&, const TileReachReading&) = default;
};

using Reading = std::variant<AimReading, IntegrityReading, TileReachReading>;

struct Answer
{
  std::uint32_t queryId;
  std::uint32_t tick; // the tick whose end state it was computed on
  Reading reading;

  friend constexpr bool operator==(const Answer&, const Answer&) = default;
};

void Encode(const QueryRequest& _request, Core::ByteWriter& _writer);

/// Reads one request. Throws std::invalid_argument for a tag it does not know, a version other than
/// QUERY_RECORD_VERSION or a field out of its range, and std::out_of_range when the bytes end first.
[[nodiscard]] QueryRequest DecodeQueryRequest(Core::ByteReader& _reader);

void Encode(const Answer& _answer, Core::ByteWriter& _writer);

/// Reads one answer, refusing as DecodeQueryRequest does.
[[nodiscard]] Answer DecodeAnswer(Core::ByteReader& _reader);

} // namespace Protocol
