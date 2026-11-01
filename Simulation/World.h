#pragma once

#include "ChunkCoordinate.h"
#include "ChunkSnapshot.h"
#include "Command.h"
#include "EntityId.h"
#include "Fixed.h"
#include "ServerData.h"
#include "VoxelWorld.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <vector>

namespace Simulation
{

/// Fills one chunk of the generated world, which arrives empty: the generator's (ADR-003), or a
/// test's. It reads only the seeds and the coordinate, never another chunk.
using GenerateChunk = std::function<void(Protocol::ChunkCoordinate, Protocol::Chunk&)>;

/// The generated surface's height at a column, in voxels, from x and z: what the support search's
/// anchor depth counts from (ADR-004, decision 2).
using SurfaceHeight = std::function<std::int32_t(std::int32_t, std::int32_t)>;

/// A command as it arrived: the player who sent it and its bytes (ADR-002, decision 3).
struct SubmittedCommand
{
  std::uint8_t player;
  std::vector<std::byte> bytes;
};

/// How a unit moves: by its climb, headroom, width and fords (ADR-015, decision 2).
enum class NavigationClass : std::uint8_t
{
  Foot,
  Cavalry,
  Machines
};

/// A point in meters, held as Core::Fixed (ADR-013, decision 3). y is the height of the feet.
struct FixedPosition
{
  Core::Fixed xMeters;
  Core::Fixed yMeters;
  Core::Fixed zMeters;

  friend constexpr bool operator==(const FixedPosition&, const FixedPosition&) = default;
};

/// A squad: units that take orders together.
struct Squad
{
  Protocol::EntityId id;
  std::uint8_t owner;
  NavigationClass navigationClass;
  std::vector<Protocol::EntityId> units; // in id order
  bool hasGoal;
  Protocol::VoxelPosition goalVoxels; // where it was last ordered, when hasGoal
};

/// One figure of a squad.
struct Unit
{
  Protocol::EntityId id;
  std::uint8_t owner;
  NavigationClass navigationClass;
  Protocol::EntityId squad;
  FixedPosition position;             // the middle of its feet
  Protocol::VoxelPosition cellVoxels; // the cell it stands in
};

enum class EngineKind : std::uint8_t
{
  Trebuchet
};

/// A siege engine placed in the world.
struct SiegeEngine
{
  Protocol::EntityId id;
  std::uint8_t owner;
  EngineKind kind;
  FixedPosition position;
  Protocol::VoxelPosition cellVoxels;
};

/// The simulation: a generated world, its entities, and the tick that advances them (ADR-002,
/// ADR-013). One build given the same Desc and the same commands reaches the same state.
///
/// Every pass over entities goes in id order, unless a step states its own order (ADR-013, decision
/// 5), which is why each kind is held in a std::map by its id. Not thread-safe: one thread runs the
/// tick (ADR-013, decision 7).
class World
{
public:
  struct Desc
  {
    const ServerData* serverData; // never null, and outlives the world
    std::uint64_t buildId;        // ADR-014, decision 6
    std::uint64_t worldSeed;
    std::uint64_t resourceSeed;
    std::uint64_t matchSeed;
    std::uint32_t generatorVersion;
    std::int32_t widthChunks; // chunks from 0 on each axis
    std::int32_t heightChunks;
    std::int32_t depthChunks;
    GenerateChunk generate;      // D1's generator, or a test's
    SurfaceHeight surfaceVoxels; // the generated surface, which the anchor depth counts from
  };

  /// Generates every chunk through VoxelWorld::Base(). Throws std::invalid_argument for a Desc with
  /// no server data, no generator, no surface or no chunks.
  explicit World(const Desc& _desc);

  /// Queues a command's bytes from _player. They are decoded and applied in the next Tick's step 1,
  /// in the order they were submitted.
  void Submit(std::uint8_t _player, std::span<const std::byte> _command);

  /// Advances one tick: MvpPlan.md's steps, in their fixed order. A malformed command throws in step
  /// 1: std::invalid_argument for an unknown tag or version, or bytes left over, and
  /// std::out_of_range for bytes that end too soon.
  void Tick();

  /// How many ticks have run: the number of the next tick.
  [[nodiscard]] std::uint32_t TickCount() const noexcept;

  /// The commands the last Tick applied, as they arrived: what a replay records (ADR-014, decision 5).
  [[nodiscard]] std::span<const SubmittedCommand> LastTickCommands() const noexcept;

  /// A new entity id: from 1, rising, never reused (ADR-013, decision 5).
  [[nodiscard]] Protocol::EntityId NewEntityId() noexcept;

  [[nodiscard]] const VoxelWorld& Voxels() const noexcept;

  /// The voxels, for the breach run, scenarios and tests to edit between ticks. Their edits are
  /// searched in the next step 5, as the tick's own are.
  [[nodiscard]] VoxelWorld& Voxels() noexcept;

  /// The chunks changed since the last call (ADR-002, decision 3).
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> Publish();

  [[nodiscard]] const Desc& Description() const noexcept;
  [[nodiscard]] const ServerData& Data() const noexcept;

  [[nodiscard]] const std::map<Protocol::EntityId, Squad>& Squads() const noexcept;
  [[nodiscard]] const std::map<Protocol::EntityId, Unit>& Units() const noexcept;
  [[nodiscard]] const std::map<Protocol::EntityId, SiegeEngine>& SiegeEngines() const noexcept;

  /// Places a squad of _owner's, with one unit standing in each of _cellsVoxels, for scenarios and
  /// tests. The squad takes the first new id and its units the next, in the order given.
  Protocol::EntityId AddSquad(std::uint8_t _owner, NavigationClass _class, std::span<const Protocol::VoxelPosition> _cellsVoxels);

  /// Places a siege engine of _owner's standing in _cellVoxels, for scenarios and tests.
  Protocol::EntityId AddSiegeEngine(std::uint8_t _owner, EngineKind _kind, Protocol::VoxelPosition _cellVoxels);

  /// Removes a squad and its units. Their ids are never given out again.
  void RemoveSquad(Protocol::EntityId _squad);

private:
  // A save reads and writes everything the next tick reads (Save.h).
  friend std::vector<std::byte> WriteSave(const World& _world);
  friend World LoadSave(std::span<const std::byte> _save, const Desc& _desc);
  friend std::uint64_t StateHash(const World& _world);

  // The tick's steps, in MvpPlan.md's order (*One simulation tick*).
  void ApplyCommands();
  void AdvanceJobs();
  void MoveUnits();
  void AdvanceShots();
  void SearchSupport();
  void AdvanceCollapses();
  void SettleLoose();
  void RebuildNavigation();
  void SettleUnits();
  void CheckRules();
  void PublishState();
  void AnswerQueries();

  void Apply(std::uint8_t _player, const Protocol::Command& _command);

  Desc m_desc;
  VoxelWorld m_voxels;
  std::uint32_t m_tick = 0;
  Protocol::EntityId m_nextEntityId = 1;
  std::vector<SubmittedCommand> m_pendingCommands;
  std::vector<SubmittedCommand> m_lastTickCommands;
  std::map<Protocol::EntityId, Squad> m_squads;
  std::map<Protocol::EntityId, Unit> m_units;
  std::map<Protocol::EntityId, SiegeEngine> m_siegeEngines;
};

/// The point a unit stands at in a cell: the middle of the cell's floor.
[[nodiscard]] FixedPosition CellFloorMiddle(Protocol::VoxelPosition _cellVoxels);

} // namespace Simulation
