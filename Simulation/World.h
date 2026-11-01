#pragma once

#include "ChunkCoordinate.h"
#include "ChunkSnapshot.h"
#include "CollapsePlan.h"
#include "Command.h"
#include "EntityId.h"
#include "Fixed.h"
#include "FixedPosition.h"
#include "NavigationGrid.h"
#include "PathSearch.h"
#include "ServerData.h"
#include "Shot.h"
#include "VoxelWorld.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
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

/// A squad: units that take orders together.
struct Squad
{
  Protocol::EntityId id;
  std::uint8_t owner;
  NavigationClass navigationClass;
  std::vector<Protocol::EntityId> units; // in id order
  bool hasGoal;
  Protocol::VoxelPosition goalVoxels;      // where it was last ordered, when hasGoal
  std::vector<PathSearch::CellIndex> path; // its last search's path, from where it stood to the goal
  bool shortOfGoal;                        // the goal was out of its class's reach
  /// The cells its figures walk in single file (M2-F5): from where its rearmost figure stands, through
  /// the cells of the figures ahead of it, to the end of its path. Each figure stops as many cells
  /// short of the end as it stands behind the first.
  std::vector<PathSearch::CellIndex> trail;
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
  std::uint32_t trailStep;            // the cell of its squad's trail it stands in, or steps toward
  std::int64_t stepEffort;            // how far into that step, in Fixed's raw meters times the ground's factor; 0 when it stands
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
  std::uint32_t reloadTicks; // until it can fire again; 0 when loaded
};

/// A trebuchet's shot in flight (M2-E1), on its arc.
struct Shot
{
  Protocol::EntityId id;
  Protocol::EntityId engine;
  std::uint8_t owner;
  ShotArc arc;
  std::uint32_t ticksFlown;
};

/// A piece that lost support, warning and then falling (ADR-004, decisions 3 and 4).
struct Collapse
{
  Protocol::EntityId id;
  Piece piece;                // its voxels as the support search found them, in y, then z, then x order
  CollapsePlan plan;          // fixed when it starts
  std::uint32_t ticksElapsed; // step 6s since it started: the warning, then the fall
  /// The units it has hit, in id order: each takes its damage once, whether it stood in the strip
  /// when the piece left or its cell was filled later (M2-F6; ADR-015, decision 7).
  std::vector<Protocol::EntityId> struck;
};

/// A unit hit by a collapse or a fall: what M4 turns into damage.
struct CrushEvent
{
  Protocol::EntityId unit;
  Protocol::EntityId collapse; // the collapse that hit it, or NO_ENTITY for a fall
  std::int32_t dropVoxels;     // how far it fell, for a fall (M2-F6), or 0
};

/// Where a shot struck: the first solid voxel on its way, and the way it was going along its larger
/// horizontal axis, ties to x, which is what "behind" means for its crater (M2-E2).
struct ShotHit
{
  Protocol::EntityId shot;
  Protocol::VoxelPosition voxel;
  std::int32_t behindX; // -1, 0 or 1; one of behindX and behindZ is 0
  std::int32_t behindZ;
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
  ~World() = default;
  World(const World&) = default;
  World& operator=(const World&) = default;
  // A move may allocate under the debug library's checked containers, whose std::map allocates in
  // its move constructor, so it is not noexcept (bugprone-exception-escape).
  World(World&&) noexcept(false) = default;
  World& operator=(World&&) noexcept(false) = default;

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
  [[nodiscard]] const std::map<Protocol::EntityId, Shot>& Shots() const noexcept;
  [[nodiscard]] const std::map<Protocol::EntityId, Collapse>& Collapses() const noexcept;

  /// Where the last tick's shots struck, in shot-id order.
  [[nodiscard]] std::span<const ShotHit> LastTickHits() const noexcept;

  /// Where units can walk, rebuilt in step 8 from the tick's edits (ADR-015).
  [[nodiscard]] const NavigationGrid& Navigation() const noexcept;

  /// The searches still running, in the order they are served: the tick they were asked for in,
  /// then squad id (M2-F3).
  [[nodiscard]] const std::map<std::pair<std::uint32_t, Protocol::EntityId>, PathSearch>& Searches() const noexcept;

  /// The units the last tick's collapses and falls hit, in the order it hit them.
  [[nodiscard]] std::span<const CrushEvent> LastTickCrushes() const noexcept;

  /// The voxels the last tick edited, edits made before it between ticks included: its TickEdits as
  /// they stood at its end, each once, in first-edit order.
  [[nodiscard]] std::span<const Protocol::VoxelPosition> LastTickEdits() const noexcept;

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
  void Fire(std::uint8_t _player, const Protocol::FireEngine& _fire);
  /// Runs one tick of a collapse's fall; returns whether it has ended.
  [[nodiscard]] bool Fall(Collapse& _collapse);
  [[nodiscard]] std::int32_t WidthVoxels() const noexcept;
  [[nodiscard]] std::int32_t DepthVoxels() const noexcept;
  [[nodiscard]] std::int32_t HeightVoxels() const noexcept;
  /// The height of a column's highest voxel that is neither empty nor water, or -1.
  [[nodiscard]] std::int32_t TopOf(std::int32_t _x, std::int32_t _z) const;
  /// Queues a column and its four neighbours for slump.
  void QueueSlump(std::int32_t _x, std::int32_t _z);
  /// Moves a column's top voxel, if it is loose, to its lowest neighbour more than a voxel below it;
  /// or, resting on a structure or too near the top of one beside it, on and away (M2-E6, E7).
  /// Returns the neighbour it went to, or nothing.
  std::optional<std::pair<std::int32_t, std::int32_t>> Slump(std::int32_t _x, std::int32_t _z);
  /// Whether a shot that reached _point has left the world.
  [[nodiscard]] bool OutsideWorld(const FixedPosition& _point) const noexcept;

  /// Asks for a squad's search from where its first unit stands, or the cell it is stepping into, to
  /// its goal, replacing any it has.
  void RequestSearch(Squad& _squad);
  /// Hits a unit with a collapse, unless that collapse has hit it already.
  void Strike(Collapse& _collapse, Protocol::EntityId _unit);
  /// Lays a squad's trail along its new path (M2-F5).
  void FormTrail(Squad& _squad);
  /// Walks a figure along its squad's trail for one tick, at its class's speed (ADR-015, decision 6).
  void Walk(Unit& _unit, const Squad& _squad, std::size_t _place, bool _holding);
  /// Sets a figure's position and cell from where it is on its squad's trail.
  void Place(Unit& _unit, const Squad& _squad) const;
  /// The effort a step between two trail cells takes a class: its length in Fixed's raw meters, times
  /// the factor of the ground it lands on.
  [[nodiscard]] std::int64_t StepEffort(PathSearch::CellIndex _from, PathSearch::CellIndex _to, NavigationClass _class) const;
  /// Whether a class may step between two trail cells now: the same cell, or a neighbour along a link.
  [[nodiscard]] bool Linked(PathSearch::CellIndex _from, PathSearch::CellIndex _to, NavigationClass _class) const;

  Desc m_desc;
  VoxelWorld m_voxels;
  NavigationGrid m_navigation;
  std::map<std::pair<std::uint32_t, Protocol::EntityId>, PathSearch> m_searches;
  std::uint32_t m_tick = 0;
  Protocol::EntityId m_nextEntityId = 1;
  std::vector<SubmittedCommand> m_pendingCommands;
  std::vector<SubmittedCommand> m_lastTickCommands;
  std::map<Protocol::EntityId, Squad> m_squads;
  std::map<Protocol::EntityId, Unit> m_units;
  std::map<Protocol::EntityId, SiegeEngine> m_siegeEngines;
  std::map<Protocol::EntityId, Shot> m_shots;
  std::map<Protocol::EntityId, Collapse> m_collapses;
  std::vector<ShotHit> m_tickHits;
  std::vector<CrushEvent> m_tickCrushes;
  // The edits made after the last step 5 in the tick before: its steps 6 and 7. The next step 5
  // searches them with its own tick's edits so far (MvpPlan.md, *One simulation tick*).
  std::vector<Protocol::VoxelPosition> m_unsearchedEdits;
  std::size_t m_searchedThrough = 0; // how many of this tick's edits step 5 searched
  // The columns waiting for slump, by column index, z * width + x (M2-E6).
  std::set<std::int64_t> m_slumpQueue;
  std::vector<Protocol::VoxelPosition> m_lastTickEdits;
};

/// The point a unit stands at in a cell: the middle of the cell's floor.
[[nodiscard]] FixedPosition CellFloorMiddle(Protocol::VoxelPosition _cellVoxels);

} // namespace Simulation
