#pragma once

#include "ChunkCoordinate.h"
#include "ChunkSnapshot.h"
#include "CollapsePlan.h"
#include "Command.h"
#include "EntityId.h"
#include "EntityKinds.h"
#include "Fixed.h"
#include "FixedPosition.h"
#include "NavigationGrid.h"
#include "Arrow.h"
#include "PathSearch.h"
#include "Query.h"
#include "ServerData.h"
#include "Shot.h"
#include "StateUpdate.h"
#include "VoxelWorld.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <span>
#include <string>
#include <string_view>
#include <variant>
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
  /// The building it is garrisoned in, or NO_ENTITY (ADR-024, decision 5): its units then stand in no
  /// cell, move and settle not at all, and are left out of the state update.
  Protocol::EntityId garrisonedIn;
  Protocol::UnitKind kind; // its figures', and so its stats (ADR-023, decision 1)
  std::uint8_t facing;     // the direction of its lead's last step, 0 north and then clockwise in eighths (ADR-022)
  /// What it was last ordered to do (ADR-023, decision 4): Move and Storm turn Idle when it arrives, Attack when its
  /// target is gone. Garrisoned is read from garrisonedIn, never held here.
  Protocol::SquadOrder order = Protocol::SquadOrder::Idle;
  Protocol::EntityId target = Protocol::NO_ENTITY; // what an Attack names: a squad, or an engine
};

/// A unit's trail step when it is not on its squad's trail: in formation, or making for it (ADR-022).
inline constexpr std::uint32_t OFF_TRAIL = 0xffffffffU;
/// No cell: a unit standing still has reserved none.
inline constexpr std::int64_t NO_CELL = -1;

/// One figure of a squad.
struct Unit
{
  Protocol::EntityId id;
  std::uint8_t owner;
  NavigationClass navigationClass;
  Protocol::EntityId squad;
  FixedPosition position;             // the middle of its feet
  Protocol::VoxelPosition cellVoxels; // the cell it stands in
  std::uint32_t trailStep;            // the cell of its squad's trail it stands in, or steps toward, or OFF_TRAIL
  std::int64_t stepEffort;            // how far into its step, in Fixed's raw meters times the ground's factor; 0 when it stands
  Protocol::UnitKind kind;            // which figure it is, and so which model the client draws
  std::int32_t hitPoints;             // from its kind's stats; it dies at 0 (ADR-023, decision 2)
  // ADR-022: the cell it steps into, reserved from the step's start, or NO_CELL; the cell it steps from; and the ticks
  // in a row it has waited for a cell.
  std::int64_t reservedCell;
  std::int64_t stepFromCell;
  std::uint32_t waitTicks;
  std::uint32_t reloadTicks = 0;                   // until it can strike or loose again; 0 when it can (ADR-023)
  Protocol::EntityId target = Protocol::NO_ENTITY; // the enemy figure it engages, chosen at the start of each step 3
};

using Protocol::EngineKind;
using Protocol::EngineState;
using Protocol::FixedPosition;
using Protocol::UnitKind;

/// A siege engine placed in the world.
struct SiegeEngine
{
  Protocol::EntityId id;
  std::uint8_t owner;
  EngineKind kind;
  FixedPosition position;
  Protocol::VoxelPosition cellVoxels;
  std::uint32_t reloadTicks;  // until it can fire again; 0 when loaded
  std::uint32_t quarterTurns; // which way its model faces: quarter turns clockwise seen from above, as Engine::VoxelPass::Batch counts them
  EngineState state;          // a ram stays Packed (MvpPlan.md, M4, input 17)
  Protocol::EntityId squad;   // its body's squad of one, whose unit it moves as and whose hit points are its (ADR-023)
  std::uint32_t stateTicksLeft = 0; // until a deploy or a pack is done; 0 when it is neither
};

/// A player of the match: their side and their stockpiles, in hundredths of a cubic metre (MvpPlan-M4.md,
/// Contracts).
struct Player
{
  std::uint8_t id;
  Protocol::Side side;
  std::int32_t stoneHundredthsCubicMeters;
  std::int32_t timberHundredthsCubicMeters;
};

/// What the simulation records happening in a tick for the trial's log and the tests, never the client (ADR-026,
/// decision 5). M4-C1 raises UnitDied; the rest come with the orders that make them.
enum class TrialEventKind : std::uint8_t
{
  ShotFired,
  ShotLanded,
  SectionOpened,
  FigurePassed,
  StoneSpent,
  TimberSpent,
  StructureStopped,
  StructureWorks,
  UnitDied,
  Outcome
};

/// One event: UnitDied names its player, the unit, its squad, its kind as the amount and its cell; StoneSpent and
/// TimberSpent the player, what it was spent on, the engine that fired or the structure repaired, the hundredths of a
/// cubic metre as the amount, and the engine's cell or the voxel put back.
struct TrialEvent
{
  TrialEventKind kind;
  std::uint32_t tick;
  std::uint8_t player;
  Protocol::EntityId subject;
  Protocol::EntityId other;
  std::int32_t amount;
  Protocol::VoxelPosition voxel;
};

/// The Siege trial's rules, from a scenario (ADR-026, decision 4).
struct TrialRules
{
  Protocol::TrialVersion version;
  std::uint8_t attacker;
  std::uint8_t defender;
  Protocol::EntityId keep;
  std::uint32_t timeLimitTicks;
  std::uint32_t holdTicks;
  std::int32_t ringCells;                // the keep's foot: cells within this many of its footprint, outside it
  Protocol::VoxelPosition campMinVoxels; // where "reachable from the camp" starts (ADR-026, decision 5)
  Protocol::VoxelPosition campMaxVoxels;
};

/// A template placed in the world (ADR-005, decision 10, without the built mask, which M4's
/// construction adds as the record's version 2).
struct Structure
{
  Protocol::EntityId id;
  std::uint8_t owner;
  std::string templateId;
  Protocol::VoxelPosition originVoxels; // the minimum corner of its footprint, after the turn
  std::uint32_t quarterTurns;           // clockwise seen from above, as Engine::VoxelPass::Batch counts them: one turns +z towards +x
  std::string name;                     // the scenario's, such as W2 (Hud.md §10)
  // At record version 2 (ADR-024; MvpPlan-M4.md, Contracts):
  /// Which template voxels were built, one bit a voxel in the template's order, x fastest, then z, then y: bit i in
  /// byte i / 8, from its lowest bit. A scenario's structures start fully built (decision 1).
  std::vector<std::uint8_t> builtMask;
  std::uint8_t criticalPercent = 100; // the share of its critical part's built voxels that stand; 100 without a part
  bool working = true;                // while the share is at least its template's threshold (decision 2)
  std::uint32_t repairLockTicks = 0;  // until repair may resume (decision 4; M4-D5)
  Core::Fixed repairProgress{};       // towards the next voxel repair puts back (decision 3; M4-D5)
  bool gateOpen = false;              // a gatehouse's or a gate's (decision 6; M4-D2)
  std::int32_t hitPoints = 0;         // the control's pool (ADR-028; M4-F4); 0 in the voxel version
};

/// A trebuchet's shot in flight (M2-E1), on its arc.
struct Shot
{
  Protocol::EntityId id;
  Protocol::EntityId engine; // its shooter: the engine, or for an arrow the squad of the figure that loosed it
  std::uint8_t owner;
  std::variant<ShotArc, ArrowLine> flight; // a stone's arc, or an arrow's line (ADR-023, decision 5)
  std::uint32_t ticksFlown;
  Protocol::ShotKind kind = Protocol::ShotKind::Stone;
  // An arrow's: the figure that loosed it, the figure it was loosed at, and the damage it deals that figure, fixed when it
  // was loosed by the shooter's stats and the elevation then.
  Protocol::EntityId shooter = Protocol::NO_ENTITY;
  Protocol::EntityId target = Protocol::NO_ENTITY;
  std::int32_t damage = 0;
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
  /// The structure whose box holds the piece's first voxel when the collapse starts, or NO_ENTITY.
  Protocol::EntityId structure;
  /// The piece's voxels as they were when it left the grid, on the fall's first tick: what the
  /// client scatters its debris from. Empty until then.
  std::vector<PlacedVoxel> fallen;
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

  /// At most this many queries wait; one more drops the oldest (ADR-009, decision 5).
  static constexpr std::size_t MAX_PENDING_QUERIES = 64;

  /// Queues a query's bytes from _player for step 12 (ADR-009; ADR-020, decision 6). Past
  /// MAX_PENDING_QUERIES the oldest pending query is dropped. Queries change nothing, are not saved
  /// and do not reach the state hash: a client asks again.
  void Ask(std::uint8_t _player, std::span<const std::byte> _query);

  /// The answers produced since the last call, encoded, in the order answered.
  [[nodiscard]] std::vector<std::vector<std::byte>> TakeAnswers();

  /// Step 12 alone, between ticks: the waiting queries answered within one tick's budget on the world as it stands,
  /// and nothing else changed, for a client whose clock is paused, such as the breach lab's question (M3-G1; ADR-020,
  /// decision 6, amended). The answers carry the last tick ticked.
  void AnswerBetweenTicks();

  /// Nothing in flight: no shot, no collapse, no slump or search pending, no edit unsearched, and every
  /// unit standing still. What the breach lab's fast-forward stops on (M3-B7).
  [[nodiscard]] bool Quiet() const;

  /// What step 11 published at the end of the last tick for _player: the whole state they see, as
  /// values (ADR-020, decision 3), with the stockpiles of their own alone and the commands of theirs
  /// the tick refused. Step 11 publishes for every player of the match, and for player 1 in a world
  /// that has none. Empty before the first tick, after a load until the next, and for a player it
  /// does not publish for.
  [[nodiscard]] const Protocol::StateUpdate& State(std::uint8_t _player) const;

  /// M3's: State(1).
  [[nodiscard]] const Protocol::StateUpdate& State() const;

  /// The last tick's events, unfiltered: for Earthrise's log and the tests, never the client (ADR-026,
  /// decision 5).
  [[nodiscard]] std::span<const TrialEvent> LastTickEvents() const noexcept;

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

  /// Places a squad of _owner's figures of _kind, with one unit standing in each of _cellsVoxels, for
  /// scenarios and tests, their class and hit points their kind's (ADR-023, decision 1). The squad
  /// takes the first new id and its units the next, in the order given.
  Protocol::EntityId AddSquad(std::uint8_t _owner, UnitKind _kind, std::span<const Protocol::VoxelPosition> _cellsVoxels);

  /// Places a siege engine of _owner's standing in _cellVoxels, facing _quarterTurns, in _state, for
  /// scenarios and tests, and its body, a squad of one of the kind Ram or Trebuchet in the same cell,
  /// which it moves as while packed (ADR-023). The engine takes the first new id, then its body's
  /// squad and unit. M2's and M3's engines are deployed.
  Protocol::EntityId AddSiegeEngine(std::uint8_t _owner, EngineKind _kind, Protocol::VoxelPosition _cellVoxels,
                                    std::uint32_t _quarterTurns = 0, EngineState _state = EngineState::Deployed);

  /// Stamps the server data's template _templateId with the minimum corner of its footprint at
  /// _originVoxels, turned _quarterTurns clockwise seen from above, by edits between ticks, as a
  /// scenario places one, and records it as a structure named _name. Its foundation course, the
  /// building stone of the template's bottom layer, is repeated down _foundationDepthVoxels below its
  /// base (ADR-026, decision 1). Its voxels are searched in the next step 5, as every edit is. Throws
  /// std::invalid_argument for a template the data lacks.
  Protocol::EntityId AddStructure(std::uint8_t _owner, std::string_view _templateId, Protocol::VoxelPosition _originVoxels,
                                  std::uint32_t _quarterTurns, std::string_view _name, std::int32_t _foundationDepthVoxels = 0);

  /// Adds a player of the match, for scenarios before tick 0. Saved.
  void AddPlayer(const Player& _player);
  [[nodiscard]] const std::map<std::uint8_t, Player>& Players() const noexcept;

  /// Puts a squad in a building, for scenarios and tests before tick 0 (ADR-024, decision 5): its units
  /// leave their cells for the building's origin, and move and settle no more. Throws
  /// std::invalid_argument for a squad or a structure the world does not hold.
  void PlaceInGarrison(Protocol::EntityId _squad, Protocol::EntityId _structure);

  /// The Siege trial's rules, for scenarios before tick 0 (ADR-026, decision 4). Saved.
  void SetTrial(const TrialRules& _rules);
  [[nodiscard]] const std::optional<TrialRules>& Trial() const noexcept;

  [[nodiscard]] const std::map<Protocol::EntityId, Structure>& Structures() const noexcept;

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
  /// Step 12 on the world as it stands, the answers carrying _tick.
  void AnswerQueries(std::uint32_t _tick);

  void Apply(std::uint8_t _player, const Protocol::Command& _command);
  void Fire(std::uint8_t _player, const Protocol::FireEngine& _fire);
  /// A packed trebuchet stops and deploys facing _deploy's way, a deployed one packs (MvpPlan.md, M4, input 17).
  void Deploy(std::uint8_t _player, const Protocol::DeployEngine& _deploy);
  void Pack(std::uint8_t _player, const Protocol::PackEngine& _pack);
  /// A stone's crush where it struck: every unit within a cell of the struck voxel, and an engine whose footprint holds
  /// its column, each once (ADR-023, decision 7).
  void CrushAround(Protocol::VoxelPosition _struck);

  // ADR-023, decision 9: the ram at a structure, in step 4.
  /// A structure's box: its footprint's minimum corner at its base, and its extents.
  struct Box
  {
    Protocol::VoxelPosition min;
    std::int32_t widthVoxels;
    std::int32_t heightVoxels;
    std::int32_t depthVoxels;
  };
  /// Where a ram touches a structure from: the cell it stands in and the way it faces, one step across x or z.
  struct RamContact
  {
    Protocol::VoxelPosition cellVoxels;
    std::int32_t facingX;
    std::int32_t facingZ;
  };
  [[nodiscard]] std::optional<Box> BoxOf(const Structure& _structure) const;
  /// The three columns before a ram's front, standing in _cellVoxels facing (_facingX, _facingZ): the one before its
  /// centre, then the one either side, the lower first.
  [[nodiscard]] std::array<std::pair<std::int32_t, std::int32_t>, 3> FrontColumns(Protocol::VoxelPosition _cellVoxels,
                                                                                  std::int32_t _facingX, std::int32_t _facingZ) const;
  /// The lowest voxel of _box in column (_x, _z) at or above _floorVoxels that is not empty, or nothing.
  [[nodiscard]] std::optional<Protocol::VoxelPosition> LowestIn(const Box& _box, std::int32_t _x, std::int32_t _z,
                                                                std::int32_t _floorVoxels) const;
  /// The world voxels of _structure's critical part, at their turned places; none when its template names none.
  [[nodiscard]] std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> CriticalVoxels(const Structure& _structure) const;
  /// What a ram's next blow from _cellVoxels would strike: the way it faces, the voxel, how many voxels it takes and how
  /// many of those are the structure's critical part's.
  struct RamBlow
  {
    std::int32_t facingX;
    std::int32_t facingZ;
    Protocol::VoxelPosition voxel;
    std::size_t takes;
    std::size_t critical;
  };
  /// The blow of the ram's four facings that touch _box, a column before its front holding a voxel of it at the ram's
  /// floor, that takes most of _critical, then most; ties to the order south, north, west, east. Nothing when it touches
  /// nothing.
  [[nodiscard]] std::optional<RamBlow> BestBlow(Protocol::VoxelPosition _cellVoxels, const Box& _box,
                                                const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _critical) const;
  /// The cell machines can use and reach from _ram from which its blow takes most of _critical, then most, the nearest by
  /// octile distance among those, ties to the lower index; nothing when no blow from any takes a voxel.
  [[nodiscard]] std::optional<RamContact>
  RamContactFor(const Unit& _ram, const Box& _box, const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _critical) const;
  /// The voxels a blow at _voxel removes, on input 9's ladder.
  [[nodiscard]] std::vector<Protocol::VoxelPosition> RamCrater(Protocol::VoxelPosition _voxel, const Box& _box) const;
  /// Every ram ordered at a structure strikes it on its cadence when it stands touching it, and makes for the nearest
  /// place it can when it does not.
  void RamBlows();
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
  /// The state _player may see: the filter of ADR-002, decision 4, through which M4's fog goes. M3
  /// has one player and no fog, so it passes everything (ADR-020, decision 3).
  [[nodiscard]] Protocol::StateUpdate StateFor(std::uint8_t _player) const;
  /// The reading a query asks for, on the tick's state (ADR-009, decision 3): M3-B4 to B6.
  [[nodiscard]] Protocol::Reading Answer(std::uint8_t _player, const Protocol::Query& _query) const;
  /// The attack cursor's reading for an engine of _player's aimed at a voxel (M3-B4).
  [[nodiscard]] Protocol::AimReading AimReadingFor(std::uint8_t _player, const Protocol::AimCheck& _aim) const;
  /// A tile's reach for the passability view (M3-B6).
  [[nodiscard]] Protocol::TileReachReading TileReachReadingFor(const Protocol::TileReach& _reach) const;
  /// A collapse as a state update shows it.
  [[nodiscard]] Protocol::CollapseView ViewOf(const Collapse& _collapse) const;
  /// The structure whose box holds _voxel, the lowest id where boxes overlap, or NO_ENTITY.
  [[nodiscard]] Protocol::EntityId StructureHolding(Protocol::VoxelPosition _voxel) const;
  /// Whether _unit's squad is in a building.
  [[nodiscard]] bool Garrisoned(const Unit& _unit) const;
  /// Records a command refused for _player, which their next update names (ADR-023, decision 10).
  void Refuse(std::uint8_t _player, const Protocol::Command& _command, Protocol::Refusal _reason);
  /// The engine whose body _squad is, or null.
  [[nodiscard]] SiegeEngine* EngineOfBody(Protocol::EntityId _squad);
  /// Takes _amount of crush from _unit, less its armor against crush, at least 1 (ADR-023, decision 8).
  void Crush(Unit& _unit, std::int32_t _amount);
  /// The end of a step that may have killed: every unit at 0 hit points removed in unit-id order, a squad left with no
  /// figure with it, an engine with its body, each death an event (ADR-023, decision 2).
  void RemoveDead();
  /// Puts each engine where its body stands.
  void FollowBodies();

  // ADR-024, decisions 1 and 2: what stands of a structure, and whether it works.
  /// Where a template voxel of _structure lies in the world, turned as AddStructure stamps it.
  [[nodiscard]] Protocol::VoxelPosition TemplateVoxel(const Structure& _structure, const ServerTemplate& _template, std::uint32_t _x,
                                                      std::uint32_t _y, std::uint32_t _z) const;
  /// The share of _structure's critical part's built voxels that stand, in whole percent, floored: a voxel stands when the
  /// world holds its template's index there and no collapse in _held holds it. 100 for a structure with no critical part.
  [[nodiscard]] std::uint8_t CriticalShare(const Structure& _structure,
                                           const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held) const;
  /// Every voxel a collapse still running holds, in warning or falling.
  [[nodiscard]] std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> HeldByCollapses() const;
  /// Sets _structure's share and working from scratch, raising StructureStopped or StructureWorks when working changes.
  void UpdateWorking(Structure& _structure, const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held);
  /// Step 10's part: the structures the tick touched, by an edit in their box or a collapse of theirs starting or ending.
  void UpdateStructures();

  // ADR-024, decision 6: gates.
  /// Whether _structure's template has a gate: its critical part is named Gate, as T5's and T7's are.
  [[nodiscard]] bool HasGate(const Structure& _structure) const;
  /// The world voxels of _structure's gate, its critical part's, turned.
  [[nodiscard]] std::vector<Protocol::VoxelPosition> GateVoxels(const Structure& _structure) const;
  /// Opens or shuts a gate of _player's (MvpPlan.md, M4, input 13).
  void SetGate(std::uint8_t _player, const Protocol::SetGate& _gate);
  /// Opens or shuts _structure's gate: navigation reads every open gate's voxels as air, and the gate's tiles are rebuilt
  /// in this tick's step 8, or at once when _now, for a gate step 10 shuts.
  void SwingGate(Structure& _structure, bool _open, bool _now);
  /// Rebuilds the navigation tiles _tiles, brings reach up to date, and has the squads they matter to search again
  /// (ADR-015, decisions 4 and 5).
  void RebuildTiles(const std::vector<std::pair<std::int32_t, std::int32_t>>& _tiles);
  /// After a load: navigation reads the open gates' voxels as air again, and their tiles are rebuilt.
  void RestoreGates();
  /// The world voxels of _structure's passage: its template's empty voxels joined, face to face, to its gate's, turned;
  /// with T5's gate at the passage's inner end, they are the passage outside the gate. Empty for a template with no gate.
  [[nodiscard]] std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> PassageVoxels(const Structure& _structure) const;
  /// Step 4: every passageAttackTicks, each gatehouse that works and holds a garrison deals passageAttackDamage of crush
  /// to every unit standing in its passage (MvpPlan.md, M4, input 13).
  void PassageAttacks();

  // ADR-024, decision 5: garrisons.
  /// Sends a squad of _player's to garrison a building of theirs; it goes in when its lead reaches the footprint.
  void Garrison(std::uint8_t _player, const Protocol::GarrisonSquad& _garrison);
  /// Puts a building's garrison out, at its owner's order.
  void Eject(std::uint8_t _player, const Protocol::EjectGarrison& _eject);
  /// The units garrisoned in _structure.
  [[nodiscard]] std::size_t GarrisonedCount(Protocol::EntityId _structure) const;
  /// Each squad walking to garrison a building goes in once its lead reaches the footprint, or stops when it no longer
  /// may, the building no longer working or full.
  void EnterGarrisons();
  /// Puts every squad garrisoned in _structure out, in squad then unit-id order, on the nearest free cells foot can use,
  /// on its owner's side first, clear of its footprint and of every falling piece's strip.
  void PutOut(const Structure& _structure);
  /// The centre of _owner's keep, doubled so that it is whole, when they have one: the trial's keep, or else their first T2.
  [[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>> KeepCenterTwice(std::uint8_t _owner) const;
  /// Where a garrison of _structure looses at a target in _targetCellVoxels from: the cell just outside the face of its box
  /// nearest the target, at the building's stand height (ADR-024, decision 5).
  [[nodiscard]] Protocol::VoxelPosition LoosingCell(const Box& _box, Protocol::VoxelPosition _targetCellVoxels,
                                                    std::int32_t _standVoxels) const;
  /// Step 4: every ranged garrisoned figure whose reload is due looses at the nearest enemy it can reach from its building.
  void LooseFromGarrisons();
  /// Step 2: garrisoned figures heal a hit point every garrisonHealTicksPerHitPoint.
  void HealGarrisons();
  /// Whether (_x, _z) is on the owner's side of _box: the half-plane through its footprint's centre that holds the
  /// owner's keep, given doubled as _keepTwice, or any side when there is none (ADR-024, decisions 3 and 5).
  [[nodiscard]] static bool OnOwnersSide(const Box& _box, const std::optional<std::pair<std::int64_t, std::int64_t>>& _keepTwice,
                                         std::int32_t _x, std::int32_t _z) noexcept;

  // ADR-024, decisions 3 and 4: repair and its lock.
  /// Sets a laborer of _player's walking to repair a structure of theirs, from the nearest cell it may repair from.
  void OrderRepair(std::uint8_t _player, const Protocol::RepairStructure& _repair);
  /// Whether _cell is one a laborer repairs _structure from: within repairReachVoxels of its footprint, outside it, on
  /// its owner's side.
  [[nodiscard]] bool AtRepairStand(const Structure& _structure, const Box& _box, Protocol::VoxelPosition _cell) const;
  /// The voxel repair would put back in _structure next and its palette index, or none: a built voxel whose cell is empty,
  /// that no unit stands in, and that would not stand on nothing; lowest first, then nearest the footprint's centre, then
  /// a column through the template's depth at a time along its x, each from its front.
  [[nodiscard]] std::optional<std::pair<Protocol::VoxelPosition, std::uint8_t>> NextRepair(const Structure& _structure) const;
  /// Step 2: each structure's lock counts down, and one unlocked with a laborer of its owner's at work gains repair's
  /// progress while it has a voxel to put back and its owner the material, putting one back for each cubic metre.
  void AdvanceRepairs();
  /// Locks repair on every structure whose box holds one of _voxels, for repairLockTicks.
  void LockRepairs(std::span<const Protocol::VoxelPosition> _voxels);
  /// Records _amount hundredths of a cubic metre of stone, or of timber, spent by _player on _subject.
  void NoteSpent(bool _timber, std::uint8_t _player, Protocol::EntityId _subject, std::int32_t _amount, Protocol::VoxelPosition _voxel);

  // ADR-023, decisions 3 and 4: targets and melee, in step 3.
  /// Each figure's target for the tick, and each squad's order as its target and its arrival leave it.
  void Acquire();
  /// The nearest enemy figure to _unit within _rangeVoxels, of _onlySquad unless it is NO_ENTITY, ties to the lower id, or
  /// NO_ENTITY. A ranged _unit also takes one beyond _rangeVoxels that its reach at the elevation to it covers.
  [[nodiscard]] Protocol::EntityId Nearest(const Unit& _unit, std::int64_t _rangeVoxels, Protocol::EntityId _onlySquad) const;
  /// Whether two units' footprints touch: one in a neighbouring cell of the other's.
  [[nodiscard]] bool Adjacent(const Unit& _first, const Unit& _second) const;
  /// The tick's melee: worked out from the state before any strike lands, then applied together.
  void Melee();
  /// Whether _unit, ranged, can loose at _foe now: in its reach at the elevation, with nothing solid on the line.
  [[nodiscard]] bool CanLoose(const Unit& _unit, const Unit& _foe) const;
  /// Every ranged figure whose reload is due looses an arrow at its target when it can (ADR-023, decision 5).
  void Loose();
  /// The attack cursor's reading for a ranged squad of _player's, as its lead's arrow would fly (M4-C4).
  [[nodiscard]] Protocol::AimReading SquadAimReadingFor(std::uint8_t _player, const Protocol::SquadAim& _aim) const;
  /// The squad an order names when _player may order it, or null after refusing the command.
  [[nodiscard]] Squad* OrderableSquad(std::uint8_t _player, const Protocol::Command& _command, Protocol::EntityId _squad);
  /// The squad an AttackTarget's target fights as: the squad, or an engine's body.
  [[nodiscard]] const Squad* TargetSquad(Protocol::EntityId _target) const;
  /// Ends a squad's walk where its figures stand, with _order.
  void Halt(Squad& _squad, Protocol::SquadOrder _order);
  void Split(std::uint8_t _player, const Protocol::SplitSquad& _split);
  void Merge(std::uint8_t _player, const Protocol::MergeSquads& _merge);

  /// Asks for a squad's search from where its first unit stands, or the cell it is stepping into, to
  /// its goal, replacing any it has.
  void RequestSearch(Squad& _squad);
  /// Hits a unit with a collapse, unless that collapse has hit it already.
  void Strike(Collapse& _collapse, Protocol::EntityId _unit);
  /// Lays a squad's trail along its new path (M2-F5).
  void FormTrail(Squad& _squad);

  // ADR-022's movement in step 3. Cells are indexed z * width + x.
  /// Which units hold and have reserved each cell.
  struct Occupancy
  {
    Occupancy() = default;
    ~Occupancy() = default;
    Occupancy(const Occupancy&) = default;
    Occupancy& operator=(const Occupancy&) = default;
    // A move may allocate under the debug library's checked containers, so it is not noexcept
    // (bugprone-exception-escape), as World's is not.
    Occupancy(Occupancy&&) noexcept(false) = default;
    Occupancy& operator=(Occupancy&&) noexcept(false) = default;

    std::map<PathSearch::CellIndex, Protocol::EntityId> occupied;
    std::map<PathSearch::CellIndex, Protocol::EntityId> reserved;
  };
  /// Where each figure of a squad makes for this tick: its slot's cell, or NO_CELL to follow the trail.
  struct SquadPlan
  {
    bool moving;  // it has a trail
    bool arrived; // its lead stands at the trail's end, and its search is done: the slots resolve about the goal
    std::vector<PathSearch::CellIndex> targets;
  };
  /// A figure's next step: the cell, and its index on the trail when the step follows it, or OFF_TRAIL.
  struct Step
  {
    PathSearch::CellIndex cell;
    std::uint32_t trailStep;
  };
  /// Every squad's plan for the tick, and each figure's place in its squad.
  struct Plans
  {
    Plans() = default;
    ~Plans() = default;
    Plans(const Plans&) = default;
    Plans& operator=(const Plans&) = default;
    // As Occupancy's.
    Plans(Plans&&) noexcept(false) = default;
    Plans& operator=(Plans&&) noexcept(false) = default;

    std::map<Protocol::EntityId, SquadPlan> squads;
    std::map<Protocol::EntityId, std::size_t> places;
  };
  static constexpr std::int32_t SLOT_TRAIL_REACH_CELLS = 3; // ADR-022, decision 4
  static constexpr std::int32_t TRAVEL_FLOOD_CELLS = 4;     // how far from the lead a slot it can reach may lie on the way
  static constexpr std::int32_t ARRIVAL_FLOOD_CELLS = 8;    // and at the goal
  static constexpr std::uint32_t TRAIL_BEHIND_CELLS = 16;   // the trail a squad keeps behind its lead
  static constexpr std::int32_t GREEDY_REACH_STEPS = 8;     // how far a figure's greedy steps are followed to see they arrive
  static constexpr std::int32_t ATTACK_FOLLOW_CELLS = 4;    // how far an attacked squad's lead strays from the goal before a new search

  [[nodiscard]] PathSearch::CellIndex CellOf(const Protocol::VoxelPosition& _cellVoxels) const noexcept;
  [[nodiscard]] std::int64_t SpeedOf(const Unit& _unit) const;
  [[nodiscard]] std::vector<PathSearch::CellIndex> Footprint(const Unit& _unit, PathSearch::CellIndex _cell) const;
  [[nodiscard]] Occupancy BuildOccupancy() const;
  [[nodiscard]] bool Free(const Occupancy& _occupancy, const Unit& _unit, PathSearch::CellIndex _cell) const;
  [[nodiscard]] std::set<PathSearch::CellIndex> Flood(PathSearch::CellIndex _start, NavigationClass _class,
                                                      std::int32_t _radiusCells) const;
  [[nodiscard]] SquadPlan PlanFor(const Squad& _squad) const;
  [[nodiscard]] std::optional<Step> NextStep(const Unit& _unit, const Squad& _squad, std::size_t _place, const SquadPlan& _plan) const;
  [[nodiscard]] std::optional<PathSearch::CellIndex> Towards(PathSearch::CellIndex _from, NavigationClass _class,
                                                             PathSearch::CellIndex _target) const;
  [[nodiscard]] bool GreedyReaches(PathSearch::CellIndex _from, NavigationClass _class, PathSearch::CellIndex _target,
                                   std::int32_t _steps = GREEDY_REACH_STEPS) const;
  [[nodiscard]] Plans PlanAll() const;
  /// The step each standing figure of a squad not holding for its search wants, for the swap.
  [[nodiscard]] std::map<Protocol::EntityId, PathSearch::CellIndex> Wanted(const Plans& _plans) const;
  [[nodiscard]] bool Holding(Protocol::EntityId _squad) const;
  [[nodiscard]] std::optional<Protocol::EntityId> SwapPartner(const Unit& _unit, PathSearch::CellIndex _cell, const Occupancy& _occupancy,
                                                              const std::map<Protocol::EntityId, PathSearch::CellIndex>& _wanted) const;
  void StartStep(Unit& _unit, Squad& _squad, std::size_t _place, const Step& _step, Occupancy& _occupancy);
  void AdvanceStep(Unit& _unit, const Squad& _squad, std::int64_t& _budget, Occupancy& _occupancy);
  /// Every figure's walk for the tick, in unit-id order.
  void WalkFigures();
  /// Drops the trail behind every figure on it, keeping as many cells behind the lead as the squad has figures.
  void TrimTrail(Squad& _squad);
  /// Moves a unit to the nearest cell its class can use that no other holds; false when there is none near.
  bool MoveToFreeCell(Unit& _unit, Occupancy& _occupancy);
  /// Sets a figure's position and cell from its step.
  void Place(Unit& _unit) const;
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
  std::map<Protocol::EntityId, Structure> m_structures;
  std::map<std::uint8_t, Player> m_players;
  /// The voxels of gates opened or shut in this tick's step 1, whose tiles step 8 rebuilds; empty between ticks.
  std::vector<Protocol::VoxelPosition> m_gateTouches;
  std::optional<TrialRules> m_trial;
  // The collapses whose fall ended this tick, as their last view: step 11 publishes them once more,
  // so the client sees the last of their landing land. Not saved: the next tick reads nothing of it.
  std::vector<Protocol::CollapseView> m_endedCollapses;
  std::map<std::uint8_t, Protocol::StateUpdate> m_states;                     // step 11's, by player
  std::vector<std::pair<std::uint8_t, Protocol::RefusalView>> m_tickRefusals; // the tick's step 1's, by player, in order
  std::vector<TrialEvent> m_tickEvents;
  std::deque<SubmittedCommand> m_pendingQueries; // a query is bytes from a player, as a command is
  std::vector<std::vector<std::byte>> m_answers;
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
