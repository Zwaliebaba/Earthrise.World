# Earthrise.World — M4 work orders

**Status:** Proposed, 2026-11-01, with [MvpPlan.md](MvpPlan.md)'s M4 revision, which is accepted when the owner merges its pull request. The owner has not yet answered MvpPlan.md, M4, inputs 1–27; phase A writes the answers down. No order from phase B on starts before that pull request merges **and M3 closes** (input 2). "Input N" means MvpPlan.md, M4, input N. These orders carry out M4 as MvpPlan.md, M4, defines it, under [ADR-018](ADR/ADR-018-line-models-and-the-line-pass.md) and [ADR-022](ADR/ADR-022-formations-and-separation.md) to [ADR-028](ADR/ADR-028-the-control.md). Where an order and those differ, they win, and the order is corrected.

**This plan is written against M3's contracts as [MvpPlan-M3.md](MvpPlan-M3.md) fixes them,** before any M3 code exists. M3-G3 writes what M4 inherits; where M3's code changed a contract M4 builds on, this file is corrected in a commit of its own before phase B starts.

**An order is one commit.** Phase A goes in the plan's pull request. Phases B to J go in two draft pull requests (input 26): B to F, the simulation, headless, which CI confirms; and G to J, the client, which the owner runs. There are 42 orders in ten phases:

| Phase | Orders | What lands | Waits on |
|---|---|---|---|
| A | A1 | The owner's decisions, written down | The owner's answers to inputs 1–27 |
| B | B1–B5 | T1–T7 at 1 m; units', structures' and the trial's server data; the castle as a scenario; the font's four icons | A, and M3 closed |
| C | C1–C6 | Commands, players, hit points and damage; formations; melee and orders; arrows; the trebuchet against units and engines; the ram | B3 |
| D | D1–D6 | Working and ruin; gates; garrisons; the passage attack; repair and its lock; sappers' trenches | C1, B3 |
| E | E1 | Fog: visibility, the filtered state, chunks sent when seen | C1, D1 |
| F | F1–F5 | The trial's rules and events; the defender; the control; the scripted attacker and the trial in CI | D, E, B4 |
| G | G1–G3 | `Engine`: the line pass, silhouettes and the column shade, the image atlas | A |
| H | H1–H3 | The client draws units, engines, gates, arrows, fog and silhouettes | F5, G |
| I | I1–I7 | Layout A: the deck, selection, the command card, control groups, the minimap, alerts, and the hover and source rules | H1 |
| J | J1–J5 | `Earthrise --siege`, its log and replay, the control's client, the measured run, the report | F, I |

B1, B2 and B5 are content and can land beside B3. G needs only A, so its three orders can land anywhere before H. C, D, E and F are serial within the headless pull request, and H, I and J within the client's. **Owner runs** fall where an order draws: after B2, B4, H1, H2, H3, I2, I3, I5, I6, J1, J3 and J4, and the playthroughs in J5. That is the pace M4 keeps.

## How a session works an order

1. **Read** AGENTS.md, this section, *Contracts*, the order, and the order's *Read first* list. Nothing else should be needed. If something is, the order is incomplete: say so in the report.
2. **Check *Waits on*.** An order starts only when every order it lists is committed, in its own phase, or merged, in an earlier one, and every input it lists is decided.
3. **Keep the contracts.** The signatures under *Contracts*, and M3's that they extend, are what other orders build on. A session that cannot keep one stops and reports; the contract is then changed in this file, in a commit of its own, before any code departs from it. A session never deviates silently (AGENTS.md).
4. **Verify through CI.** The cloud container has no MSVC, no vstest and no GPU, so CI is the build, and WARP is the GPU ([ADR-012](ADR/ADR-012-render-tests-on-warp.md)).
   - **Before pushing,** run what runs here: `Build/CheckProjectFiles.py`; `Build/CheckFormat.py` with clang-format 18 where it is installed; `Build/TestCheckers.py` when a checker changed; and `Tools/CheckAssets.py`, `Tools/TestCheckAssets.py` and `Tools/TestBakeContent.py` when `GameData/` or `Tools/` changed.
   - **Then push the order's commit** to its phase's draft pull request. CI must be green before the next order starts.
   - **Count the CI runs** the order took, failures included. The commit's last body line says `CI runs: N`, and a fix that CI forced is a commit of its own, as M2 did.
5. **Name the commit for the order.** Its subject starts with the order's id, as in `M4-C2: Keep figures one to a cell, in ranks`.
6. **Register every file** in its project's `.vcxproj` and `.filters`, under the functional filter the order names. Folders stay flat (AGENTS.md §2).
7. **Write the tests the order names.** They go in `<Project>Tests/<Type>Tests.cpp`, in `TEST_CLASS(<Type>Tests)`, with methods named for the behaviour they check. A render test creates its device with `GpuDevice::Desc::useWarp` and asserts coverage, colour and depth by number, never a golden image (ADR-012).
8. **Save what you add.** An order that adds state to an entity or a kind of entity adds it to the save, the state hash and the state update, raising the record's version, and extends `SaveTests` and `StateUpdateTests`, in the same commit (ADR-014, decision 4; ADR-020, decision 3). A save is bound to its build (ADR-014, decision 6), so an older record version is refused, never migrated.
9. **Accept the ADR you implement.** The commit that first implements an ADR marks it Accepted, with the date and any amendment the code forced (AGENTS.md §6). Each order says which ADR, if any.
10. **Hand the screen to the owner.** An order that draws or takes input lists what the owner should see under *Owner runs*, and the owner launches it before the pull request merges, and as soon after the order as the owner can (AGENTS.md §3). What the owner reports comes back as a fix commit, or as a new order in the same phase.
11. **Numbers are the inputs'.** Every statistic and rule value lives in `GameData/Server/` and reaches the code through `ServerData`. A test that depends on a value reads it from the baked file, so a retune between playtests changes no test but those that pin the value on purpose, and those say so.
12. **Leave out-of-scope work out.** What a session notices beyond its order goes in the PR description, not the commit.
13. **Log nothing about time** (GameDesign.md, *No time logging*). The pull request and its commits, one per order, are the record.

## Contracts

The seams more than one order builds on. Parameters, members and constants follow AGENTS.md §1. Each contract's comments state its rules. M3's contracts stand, and these extend them; a field marked "M3's" is there already.

### `Protocol`: what crosses the boundary (C1, D1, E1, F1)

```cpp
// Protocol/EntityKinds.h gains:
enum class UnitKind : std::uint8_t { Laborer, Spearman, Archer, Outrider, ManAtArms, Sapper, Ram, Trebuchet }; // M3's six, and the engines' bodies (ADR-023)
inline constexpr std::size_t UNIT_KIND_COUNT = 8;
enum class EngineKind : std::uint8_t { Trebuchet, Ram };
enum class EngineState : std::uint8_t { Packed, Deploying, Deployed, Packing }; // a ram stays Packed: it moves and never deploys
enum class DamageType : std::uint8_t { Melee, Pierce, Crush };
enum class Side : std::uint8_t { Attacker, Defender };
enum class SquadOrder : std::uint8_t { Idle, Move, Attack, Storm, Hold, Trench, Repair, Garrisoned };
enum class ShotKind : std::uint8_t { Stone, Arrow };
enum class TrialVersion : std::uint8_t { Voxel, Control };
enum class TrialOutcome : std::uint8_t { Running, Won, LostToClock, LostArmy };
enum class Refusal : std::uint8_t { NotOwned, NotEnoughStone, NotEnoughTimber, Reloading, NotDeployed, NotPacked, Full, NotWorking, Invalid };

// Protocol/Command.h — each command keeps COMMAND_RECORD_VERSION 1 and its own tag; on the wire u16 tag, u16 version,
// then the fields in declaration order. M2's two keep their bytes; DecodeCommand reads the fields by tag (C1).
struct AttackTarget { EntityId squad; EntityId target; };                              // 3: a squad, an engine, or for a ram a structure
struct StormTo { EntityId squad; VoxelPosition goalVoxels; };                          // 4: move, engaging what it meets
struct StopSquad { EntityId squad; };                                                  // 5
struct HoldSquad { EntityId squad; };                                                  // 6: strike from where it stands, never leave
struct SplitSquad { EntityId squad; };                                                 // 7: the highest-id figure leaves as a squad of one
struct MergeSquads { std::vector<EntityId> squads; };                                  // 8: u8 count, then the ids; one kind and owner, up to full size
struct GarrisonSquad { EntityId squad; EntityId structure; };                          // 9
struct EjectGarrison { EntityId structure; };                                          // 10
struct RepairStructure { EntityId laborer; EntityId structure; };                      // 11: a laborer is a squad of one
struct DigTrench { EntityId squad; VoxelPosition minVoxels; VoxelPosition maxVoxels; }; // 12: a box, both corners included, dug from the top
struct PackEngine { EntityId engine; };                                                // 13
struct DeployEngine { EntityId engine; std::uint32_t quarterTurns; };                  // 14
struct SetGate { EntityId structure; bool open; };                                     // 15: u8 0 or 1
using Command = std::variant<MoveSquad, FireEngine, AttackTarget, StormTo, StopSquad, HoldSquad, SplitSquad, MergeSquads,
                             GarrisonSquad, EjectGarrison, RepairStructure, DigTrench, PackEngine, DeployEngine, SetGate>;
// An engine moves as its body's squad: MoveSquad names EngineView::squad.

// Protocol/StateUpdate.h — the views gain (M3's fields stand):
struct PlayerView { std::uint8_t player; Side side; std::int32_t stoneHundredthsCubicMeters; std::int32_t timberHundredthsCubicMeters; }; // stockpiles 0 for other players
struct StructureView // + M3's id, owner, templateId, originVoxels, quarterTurns, name
{
  bool working;                    // ADR-024, decision 2; true for a structure without a critical part
  std::uint8_t criticalPercent;    // the part's standing share, 100 without a part
  std::uint32_t repairLockTicks;
  bool gateOpen;
  std::uint8_t garrisonCount;      // the owner's update only; 0 in others'
  std::uint8_t garrisonCapacity;
  std::int32_t hitPoints;          // the control only (ADR-028); 0 in the voxel version
  std::int32_t maxHitPoints;
  bool opened;                     // a foot route from the camp crosses its footprint (ADR-026, decision 5)
  bool lastSeen;                   // not visible now: as it stood when last visible (ADR-025, decision 2)
};
struct EngineView { /* + M3's id, owner, kind, cellVoxels, quarterTurns, reloadTicks */ EntityId squad; EngineState state; std::uint32_t stateTicksLeft; FixedPosition position; std::int32_t hitPoints; std::int32_t maxHitPoints; };
struct SquadView { /* + M3's id, owner, units, hasGoal, goalVoxels, shortOfGoal */ UnitKind kind; SquadOrder order; std::uint8_t fullFigures; EntityId target; EntityId garrisonedIn; };
struct UnitView { /* + M3's id, squad, owner, kind, position, cellVoxels */ std::int32_t hitPoints; std::int32_t maxHitPoints; };
struct ShotView { /* + M3's id, engine, position */ ShotKind kind; std::uint8_t owner; };   // engine: the shooter's id, a squad's for an arrow
struct RefusalView { std::uint16_t commandTag; Refusal reason; };                     // the receiving player's commands refused this tick
struct TrialView
{
  TrialVersion version;
  TrialOutcome outcome;
  std::uint32_t ticksLeft;         // of the time limit
  EntityId keep;
  bool keepWorking;
  std::uint32_t holdTicks;         // the storming clock (ADR-026, decision 4)
  std::uint32_t holdTicksNeeded;
  bool holdContested;
};
struct StateUpdate // + M3's tick, structures, engines, squads, units, shots, collapses, hits
{
  std::uint8_t player;                    // whose filter made it
  std::vector<PlayerView> players;
  std::vector<RefusalView> refusals;
  TrialView trial;                        // version Voxel and outcome Running outside a trial
  std::int32_t widthCells;
  std::int32_t depthCells;
  std::vector<std::uint64_t> visibleCells; // bit z * widthCells + x (ADR-025, decision 2)
};

// Protocol/Query.h gains (query and answer record version 2):
struct SquadAim { EntityId squad; VoxelPosition targetVoxels; };  // tag 4: the attack cursor for a ranged squad, answered by an AimReading from its lead's loosing point
// Answer gains bool notVisible (ADR-025, decision 6); IntegrityReading gains std::uint32_t repairLockTicks.
```

### `Simulation` (B3, B4, C1–C6, D1–D6, E1, F1–F5)

```cpp
// Simulation/UnitStats.h — B3, from UNIT (ADR-023, decision 1).
struct UnitStats
{
  Protocol::UnitKind kind;
  NavigationClass navigationClass;
  std::uint8_t squadFigures;                          // 6 foot, 3 mounted, 1 laborers and engines
  std::int32_t hitPoints;
  std::array<std::int32_t, 3> armor;                  // by Protocol::DamageType
  Protocol::DamageType attackType;
  std::int32_t attack;                                // 0 for kinds with no attack on units
  std::uint32_t reloadTicks;
  std::int32_t rangeVoxels;                           // 0: melee, the eight neighbouring cells
  std::array<std::int32_t, Protocol::UNIT_KIND_COUNT> bonus;
  std::int32_t visionVoxels;
  Core::Fixed speedMetersPerTick;
  Core::Fixed digCubicMetersPerTick;                  // sappers and laborers; 0 otherwise
};
/// ADR-023, decision 2: max(1, floor((attack + bonus) * (100 + 15 * steps) / 100) - armor), steps for pierce only.
[[nodiscard]] std::int32_t Damage(const UnitStats& _attacker, const UnitStats& _target, std::int32_t _elevationSteps);

// Simulation/StructureRules.h — B3, from STRC (ADR-024, decision 7).
struct StructureRules { std::string templateId; std::uint8_t criticalThresholdPercent; std::uint8_t garrisonCapacity; std::int32_t standHeightVoxels; std::int32_t visionVoxels; std::int32_t controlHitPoints; };

// ServerData gains: [[nodiscard]] const UnitStats& Stats(Protocol::UnitKind) const; [[nodiscard]] const StructureRules* FindStructureRules(std::string_view) const;
// [[nodiscard]] const Scenario* FindScenario(std::string_view) const. Rules gains B3's RULE keys.

// Simulation/World.h gains:
struct Player { std::uint8_t id; Protocol::Side side; std::int32_t stoneHundredthsCubicMeters; std::int32_t timberHundredthsCubicMeters; };
// Unit gains hitPoints, reloadTicks, target, reservedCell, waitTicks, garrisonedIn. Squad gains kind, order, target, trench box.
// SiegeEngine gains squad (its body's), state, stateTicksLeft. Structure gains, at record version 2: builtMask, criticalPercent,
// working, repairLockTicks, repairProgress (Core::Fixed), gateOpen, garrison, hitPoints. Shot gains kind, shooter, target,
// and std::variant<ShotArc, ArrowLine> for its flight, ArrowLine being start, end and flight ticks in Q16.16.
struct TrialRules
{
  Protocol::TrialVersion version;
  std::uint8_t attacker;
  std::uint8_t defender;
  Protocol::EntityId keep;
  std::uint32_t timeLimitTicks;
  std::uint32_t holdTicks;
  std::int32_t ringCells;                      // the keep's foot: cells within this many of its footprint, outside it
  Protocol::VoxelPosition campMinVoxels;       // where "reachable from the camp" starts (ADR-026, decision 5)
  Protocol::VoxelPosition campMaxVoxels;
};
enum class TrialEventKind : std::uint8_t { ShotFired, ShotLanded, SectionOpened, FigurePassed, StoneSpent, TimberSpent, StructureStopped, StructureWorks, UnitDied, Outcome };
struct TrialEvent { TrialEventKind kind; std::uint32_t tick; std::uint8_t player; Protocol::EntityId subject; Protocol::EntityId other; std::int32_t amount; Protocol::VoxelPosition voxel; };
class World
{
public:
  void AddPlayer(const Player& _player);                                   // before tick 0; saved
  [[nodiscard]] const std::map<std::uint8_t, Player>& Players() const noexcept;
  /// M3's AddSquad, its class and its figures' hit points from the kind's stats.
  Protocol::EntityId AddSquad(std::uint8_t _owner, Protocol::UnitKind _kind, std::span<const Protocol::VoxelPosition> _cellsVoxels);
  /// M3's AddSiegeEngine with its state; makes its body's squad of one.
  Protocol::EntityId AddSiegeEngine(std::uint8_t _owner, Protocol::EngineKind _kind, Protocol::VoxelPosition _cellVoxels, std::uint32_t _quarterTurns,
                                    Protocol::EngineState _state);
  void PlaceInGarrison(Protocol::EntityId _squad, Protocol::EntityId _structure); // scenarios and tests, before tick 0
  /// M3's AddStructure, its foundation course repeated down to _foundationDepthVoxels below the template's base.
  Protocol::EntityId AddStructure(std::uint8_t _owner, std::string_view _templateId, Protocol::VoxelPosition _originVoxels, std::uint32_t _quarterTurns,
                                  std::string_view _name, std::int32_t _foundationDepthVoxels);
  /// What step 11 published for _player (ADR-025); M3's State() is State(1).
  [[nodiscard]] const Protocol::StateUpdate& State(std::uint8_t _player) const;
  /// The chunks _player is to be sent this tick (ADR-025, decision 3); M3's Publish() is Publish(1).
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> Publish(std::uint8_t _player);
  void SetTrial(const TrialRules& _rules);                                 // before tick 0; saved
  [[nodiscard]] std::span<const TrialEvent> LastTickEvents() const noexcept; // unfiltered: for Earthrise's log and the tests, never the client
  void SetCommander(std::uint8_t _player, std::unique_ptr<Commander> _commander); // before tick 0; saved (ADR-027)
};

// Simulation/Formation.h — C2 (ADR-022, decision 4). Pure.
/// The slots of a squad of _figures in _ranks ranks behind _leadVoxels, facing _direction (0 north, then clockwise in eighths), 1 m apart; slot 0 is the lead.
[[nodiscard]] std::vector<Protocol::VoxelPosition> FormationSlots(Protocol::VoxelPosition _leadVoxels, std::uint8_t _direction, std::uint8_t _figures, std::uint8_t _ranks);

// Simulation/Visibility.h — E1 (ADR-025, decision 1).
class Visibility
{
public:
  Visibility(std::int32_t _widthCells, std::int32_t _depthCells);
  void Clear() noexcept;
  void Stamp(std::int32_t _xCells, std::int32_t _zCells, std::int32_t _radiusCells) noexcept;                  // a disc, by squared distance
  void StampBox(Protocol::VoxelPosition _minVoxels, Protocol::VoxelPosition _maxVoxels, std::int32_t _radiusCells) noexcept; // a footprint grown by a radius
  [[nodiscard]] bool Visible(std::int32_t _xCells, std::int32_t _zCells) const noexcept;
  [[nodiscard]] std::span<const std::uint64_t> Bits() const noexcept;
};

// Simulation/Scenario.h — B4 (ADR-026).
struct Scenario; // the baked SCEN record: seeds, site, ground, structures, earthworks, players, squads, engines, garrisons, trial
[[nodiscard]] World::Desc ScenarioDesc(const ServerData& _data, std::string_view _scenarioId, std::uint64_t _buildId);
/// Builds the scenario on a world made from ScenarioDesc and not yet ticked, in the source's order (ADR-026, decision 3). Throws std::invalid_argument for an unknown id.
void LoadScenario(World& _world, std::string_view _scenarioId);

// Simulation/StructureRule.h — F4 (ADR-028, decision 1). D1–D6 write the voxel behaviour; F4 moves it behind this seam unchanged.
class StructureRule
{
public:
  virtual ~StructureRule() = default;
  virtual void ShotStrikes(World& _world, const ShotHit& _hit) = 0;
  virtual void RamStrikes(World& _world, Protocol::EntityId _structure, Protocol::VoxelPosition _voxel) = 0;
  virtual void TrenchDug(World& _world, Protocol::VoxelPosition _voxel) = 0;
  virtual void Repair(World& _world, Structure& _structure) = 0;          // one tick of repair, the lock already checked
  virtual void UpdateWorking(World& _world, Structure& _structure) = 0;   // step 10
};

// Simulation/Commander.h — F2 (ADR-027).
class CommanderView
{
public:
  [[nodiscard]] const Protocol::StateUpdate& State() const noexcept;                                  // its player's, from step 11
  [[nodiscard]] std::optional<Protocol::IntegrityReading> Integrity(Protocol::EntityId _structure);   // costs the query's units; empty past the budget or out of sight
  [[nodiscard]] bool Reachable(Protocol::NavigationClass _class, Protocol::VoxelPosition _fromVoxels, Protocol::VoxelPosition _toVoxels); // costs 1
  [[nodiscard]] std::int32_t WorkLeft() const noexcept;
  [[nodiscard]] std::uint64_t Draw(std::uint64_t _salt) const noexcept;                               // KeyedDraw on the match seed, the player and the tick
};
class Commander
{
public:
  virtual ~Commander() = default;
  virtual void Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands) = 0; // step 13, on its cadence
  virtual void Write(Core::ByteWriter& _writer) const = 0;                                  // its state, for the save and the hash
};
// SiegeDefender and SiegeAttacker implement it; MakeCommander(std::string_view _kind, Core::ByteReader* _saved) builds one from a scenario's name or a save.
```

### `Content` (B5)

```cpp
// Content/Font.h: FONT_GLYPH_COUNT becomes 131. Icons U+E019 split, U+E01A merge, U+E01B deploy and U+E01C wall join M3's 25,
// each 9 x 9 (input 21).
```

### `Engine` (G1–G3)

```cpp
// Engine/LinePass.h — ADR-018, decision 3. Shaders LineVS.hlsl and LinePS.hlsl.
struct LineSegmentRecord { float fromMeters, toMeters; DirectX::XMFLOAT4 color; float widthPixels; std::uint32_t factionColor; std::uint32_t tapers; float reserved; }; // 40 bytes
struct LineModelRecord { std::uint32_t firstSegment; std::uint32_t segmentCount; float drawnLengthMeters; float dotBelowPixels; float dotWidthPixels; std::array<std::uint32_t, 3> reserved; }; // 32 bytes
struct LineInstance { DirectX::XMFLOAT3 positionMeters; std::uint32_t lineModel; DirectX::XMFLOAT3 direction; float reserved; DirectX::XMFLOAT4 factionColor; }; // 48 bytes; direction unit length
class LinePass
{
public:
  struct Desc { DXGI_FORMAT colorFormat; DXGI_FORMAT depthFormat; };
  LinePass(ID3D12Device* _device, const Desc& _desc);
  /// Binds the frame's constants and the model and segment tables. Depth tested GREATER_EQUAL and written (ADR-018, decision 3).
  void Begin(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants, D3D12_GPU_VIRTUAL_ADDRESS _models,
             D3D12_GPU_VIRTUAL_ADDRESS _segments) const;
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _instances, std::uint32_t _instanceCount) const;
};

// Engine/VoxelPass.h gains (ADR-021, decision 6, amended; ADR-025, decision 8):
/// Records drawn flat in their palette colour at _alpha where the depth test fails (LESS under reversed Z), blended, writing no depth.
void BeginSilhouette(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants, D3D12_GPU_VIRTUAL_ADDRESS _palette,
                     float _alpha) const;
/// From now on world voxels are multiplied by _shade where _mask's texel for their column is 0. R8_UNORM, one texel a cell; nullptr turns it off.
void SetColumnShade(ID3D12Resource* _mask, std::int32_t _widthCells, std::int32_t _depthCells, float _shade);

// Engine/InterfacePass.h: InterfaceQuad::textured gains the value 2, which samples an RGBA8 image atlas, point-sampled, times color (ADR-019, amended).
void SetImageAtlas(ID3D12Resource* _atlas, std::uint32_t _widthTexels, std::uint32_t _heightTexels);
```

### `Client` (H1–H3, I1–I7)

```cpp
// Client/Interpolation.h — H1 (input 20).
/// Where an entity is drawn: between its positions in the previous and the last update at the clock's fraction, a tick behind,
/// or at its last when it had none before or moved more than 1.5 m in the tick (a drop, a lift, a step 9 move).
[[nodiscard]] DirectX::XMFLOAT3 DrawnPosition(const Protocol::FixedPosition* _previous, const Protocol::FixedPosition& _last, double _tickFraction);

// Client/Selection.h — I2 (Hud.md §5, §10).
struct Selection
{
  std::vector<Protocol::EntityId> squads;        // engines' bodies' squads included; laborers one each
  Protocol::EntityId structure;                  // NO_ENTITY, or one structure, selected alone
  std::optional<Protocol::UnitKind> activeKind;  // drives the card; Tab moves it
};

// Client/HudModel.h gains Hud.md §10's rest:
struct HudCommandSlot { std::string command; std::string hotkey; std::string label; char32_t icon; bool enabled; bool active; };
struct HudCommandCard { std::string kind; std::uint8_t page; std::array<HudCommandSlot, 12> slots; };
struct HudDot { DirectX::XMFLOAT2 positionPixels; std::uint8_t team; bool structure; bool selected; };
struct HudPing { DirectX::XMFLOAT2 positionPixels; bool urgent; double expiresSeconds; };
struct HudMinimap { bool baseDirty; std::vector<HudDot> dots; std::array<DirectX::XMFLOAT2, 4> viewQuad; std::vector<HudPing> pings; };
// HudModel gains: Selection selection; std::array<std::vector<Protocol::EntityId>, 9> controlGroups; HudCommandCard commandCard;
// HudMinimap minimap; std::optional<std::string> tooltip; HudObjective's progress (0 to 1, the storming clock); the trial's outcome.
// selectedEngine (M3's) goes: an engine is selected through its squad.
```

### The trial's log (J2)

One CSV a run, `Logs\Siege-<participant>-<voxel|control>-<yyyymmdd-hhmmss>.csv` beside the executable, LF-terminated, UTF-8, and its replay beside it as `.errp` with the same name (ADR-026, decision 6). Comment lines first, each `# key: value`: participant, version, build id, server-data hash, scenario, display size, UI scale, start time. Then a header row and rows of these kinds, in tick order, each led by its kind and its tick:
- `shot,<tick>,<player>,<engine>,<target x y z>,<strike x y z>,<structure name or ->`;
- `opened,<tick>,<structure name>`: a section's first foot route from the camp (ADR-026, decision 5);
- `passed,<minute>,<structure name>,<attacker|defender>,<figures>`: figures through an opened section in that game minute, written when the minute ends;
- `stone,<tick>,<player>,<hundredths of m³>,<shot|repair>` and `timber,…` likewise;
- `stopped,<tick>,<structure name>` and `works,<tick>,<structure name>`;
- `died,<tick>,<player>,<kind>`;
- `command,<tick>,<seconds>,<command>,<source>`: every command the player issued, `key` or `click` (Hud.md §11);
- `outcome,<tick>,<won|clock|army>,<seconds>`.

## Phase A — Decisions

### M4-A1 — Write down the owner's decisions
- **Waits on:** the owner's answers to MvpPlan.md, M4, inputs 1–27. Paper only.
- **Read first:** MvpPlan.md, M4, *The owner's inputs*; GameDesign.md, *M3's values*, *the MVP assets v3 package* and *Open questions*.
- **Changes:**
  - **GameDesign.md** gains `### <date>: M4's values`, with each input as Decided in the owner's words, or as the owner changed it. Input 27 stays Proposed, because F5 measures it. The Open question *The HUD's sizes* is rewritten as the owner decides it (input 21), and the v3 package's open item on the packed wagon's width as input 17 decides it. The paper tests' Proposed values the inputs take up (items 11, 14, 15, 16 and 17) are marked Decided or changed.
  - **MvpPlan.md:** each input's row says Decided, and input 1 records the go-ahead for each ADR.
  - **The ADRs** are edited where the owner changed them, and stay Proposed (AGENTS.md §6).
  - **Hud.md:** its Proposed items the inputs decide (selecting by a squad's footprint, the deck's coverage, the minimap built at its drawn size) and the placeholders in §5 that the inputs replace are marked Decided or changed; §5's wall row follows input 22.
- **Tests:** none. The checkers run, and every relative link resolves.

## Phase B — Content and server data

### M4-B1 — T3, T4 and T5 redrawn natively at 1 m
- **Waits on:** A1, and inputs 3 and 4.
- **Read first:** MvpAssets.md, *Known issues* and the templates' table; ADR-005, decisions 1–4; GameDesign.md, *the paper tests' values*, item 14; Review/PaperTests/SiegeRouteTable.md, *T5's gate*; ADR-015, *Consequences*, on T5's passage; `Tools/CheckAssets.py`.
- **Changes:** the three templates' layer files in `GameData/Server/Templates/` redrawn at 1 m as input 3 says, each keeping its outline, its volume within 2 % (T3 96 m³ of stone exactly), its openings and its parts. **T5's gate moves to the inner end of the passage**, its 12 voxels at the passage's inner face, the passage 3 m wide and 4 m high clear in front of it, so a lobbed shot cannot reach the gate until the arch over the passage is cut back; G1's placement offset in `Manifest.json` follows the gate. Previews regenerated into `Design/Previews/`. MvpAssets.md's table and *Known issues* updated.
- **Tests:** `Tools/CheckAssets.py` and `Tools/TestBakeContent.py` pass; `ServerDataTests`' T5 check reads the gate at the inner face; `BreachRunTests` and the lab's `BreachLabTests` keep their keys, or the commit records the new ones and why (a changed T3 is a changed lab, as M3-B7 said).

### M4-B2 — T1, T2, T6 and T7 redrawn natively at 1 m
- **Waits on:** A1, and input 3.
- **Read first:** as B1, and MvpAssets.md on T2's pier and timber floors.
- **Changes:** the four templates redrawn as B1's are, T2 keeping 1,650 m³ of stone and 120 m³ of timber and its great tower as its critical part, T7 its gate. Previews and MvpAssets.md updated.
- **Tests:** as B1.
- **Owner runs:** the previews in `Design/Previews/`, and the renderer spike, whose v5 scene draws T1–T7 from `Content`: the seven templates natively at 1 m, with no majority-vote artefacts on edges and openings.

### M4-B3 — Units, structures and the trial's rules as server data
- **Waits on:** A1, and inputs 6–13, 17 and 19.
- **Read first:** ADR-016, decisions 3 and 4; ADR-023, decision 1; ADR-024, decision 7; `Tools/BakeContent.py`'s server half; `Simulation/ServerData.cpp`'s `RULE_FIELDS` and section checks; *Contracts*, `Simulation`.
- **Changes:**
  - **`GameData/Server/Units.json`** and **`Structures.json`**, with input 6's and input 10's values under keys named with their units, and the `UNIT` and `STRC` sections they bake into; `Rules.json` gains inputs 7–9, 11–13, 17 and 19's values, ADR-022's wait and ADR-027's cadence, and `RULE_FIELDS` their keys.
  - **`ServerData`** accepts the two sections at version 1, and gives `Stats` and `FindStructureRules`; `Simulation/UnitStats.h` and `StructureRules.h`, in the filter `Data`, with `Damage`.
  - **`Tools/CheckAssets.py`** gains check 15: every unit kind once, every bonus naming a kind, every structure naming a template, thresholds 1–100, every value converting exactly; `Tools/TestCheckAssets.py` and `TestBakeContent.py` gain a broken file of each and the round trip.
- **Tests:** **`UnitStatsTests`:** every kind reads back its input-6 values; `Damage` gives input 6's table of first hits for every pairing of the five soldier kinds, with steps −2 to 2 for the archer; it is never below 1. **`ServerDataTests`:** a file lacking `UNIT` or `STRC` is refused by name, as is a `RULE` lacking any new key.
- **ADR:** ADR-023's decision 1 and ADR-024's decision 7 are implemented; both stay Proposed until C1 and D1. ADR-016 is amended: `UNIT` and `STRC` join at section version 1.

### M4-B4 — The Siege castle as a scenario
- **Waits on:** B1, B2 and B3, and inputs 4, 5, 14, 15, 16 and 17.
- **Read first:** ADR-026; ADR-014, decision 7; Review/PaperTests/SiegeRouteTable.md, *The castle* and its per-section table; `Simulation/BreachLab.cpp`'s `BuildLabSite`, the levelling M3 wrote; *Contracts*, `Simulation`, `Scenario`.
- **Changes:**
  - **`GameData/Server/Scenarios/Siege.json`**: the route table's castle (input 4) at the site the order picks on the generated map: the flattest 200 × 200 m clear of the river, reported with its levelling volume. The site levelled; 1 m of soil over common rock, deep earth 8 m deep under E3 and its approach; the ditch and rampart, the causeway and E3's gap; the 22 sections S1–S4, E1–E6, N1–N6 and W1–W6, the towers SW, SE, NW, NE and ST, the gatehouse GH, the keep and the hall, each named; E3's foundation at 4 m; the camp 100 m south; input 5's armies and stockpiles, the defender's archers in the seven garrisons; the attacker's map seen at the start (input 14); the trial's rules (inputs 15 and 16).
  - **The `SCEN` section,** its check (check 16: ADR-026, decision 2's list) and bake; **`Simulation/Scenario.h` and `.cpp`,** in the filter `World`: `ScenarioDesc` and `LoadScenario`; `World::AddPlayer`, `PlaceInGarrison`, `SetTrial` stored and saved, with no behaviour yet; `AddStructure` with its foundation depth.
  - **`Earthrise --siege`** loads the scenario in place of the lab's site and runs it with M3's client, nothing ordered, so the owner sees the castle (J1 builds the trial on it).
- **Tests:** **`ScenarioTests`:** the castle has 22 sections, 5 towers, a gatehouse, a keep and a hall with their names; S3 is the section nearest the camp, stands on rock and has the ditch before it; E3 stands on earth with no ditch, farther from the camp than S3; GH's gate is at its passage's inner end; loading twice gives one state hash; a save at tick 0 loads to the same hash; every squad's cells are walkable and distinct.
- **ADR:** marks ADR-026 Accepted for decisions 1–3; decisions 4–6 land in F1 and J2. ADR-016 is amended: `SCEN` joins.
- **Owner runs:** `Earthrise --siege --windowed`: the castle on the generated map, its ditch, rampart and causeway, the camp, and M3's drawings of the armies standing.

### M4-B5 — Four icons for the font
- **Waits on:** A1, and input 21.
- **Read first:** Hud.md §7 and §9; ADR-019, *Left open*; `GameData/Client/Fonts/EarthriseMason-glyphs.json`; `Tools/CheckAssets.py`'s check 13.
- **Changes:** SPLIT, MERGE, DEPLOY and WALL as 9 × 9 icons at U+E019–U+E01C, drawn in the font's style; check 13's counts move to 131 glyphs and 29 icons; the atlases in `Design/Previews/` regenerated; `FONT_GLYPH_COUNT` 131.
- **Tests:** `Tools/TestCheckAssets.py`; **`FontTests`**: 131 glyphs; the four icons present and 9 wide.

## Phase C — Units and combat

### M4-C1 — Commands, players, hit points and damage
- **Waits on:** B3, and input 8.
- **Read first:** ADR-023, decisions 1, 2, 8 and 10; ADR-002, *Consequences*, on acknowledging commands; `Protocol/Command.cpp`'s `DecodeCommand`, which reads one shape for every tag; `Simulation/World.cpp`'s `Strike` and `SettleUnits`, which raise `CrushEvent`s; *Contracts*, `Protocol` and `Simulation`.
- **Changes:**
  - **`Protocol`:** the thirteen new commands, encoded and decoded by tag, and the enumerations; `StateUpdate`'s new fields, filled with what exists.
  - **`Simulation`:** players and stockpiles; units' hit points from their kind's stats; `CrushEvent`s turned into damage, the collapse's and the drop's (input 8); a unit at 0 hit points removed at the end of the step that killed it, a squad with none removed, an engine with its body; `UnitDied` events; refused commands recorded as `RefusalView`s for their player; engines move as their body's squad, by `MoveSquad`, when packed. Every command that is not yet built is refused `Invalid` until its order lands.
- **Tests:** **`CommandTests`:** every command round-trips; a known encoding byte for byte for `MergeSquads{{4, 9}}`; M2's two keep their bytes. **`DamageTests`:** a unit under a collapse's strip takes `collapseCrushDamage` once; a 5 m drop deals two metres' worth; a unit killed leaves its squad one figure short; a squad's last figure removes the squad; a ram's body killed removes the engine. **`SaveTests`** and **`StateUpdateTests`** gain players, hit points and refusals.
- **ADR:** marks ADR-023 Accepted.

### M4-C2 — Formations and separation
- **Waits on:** C1.
- **Read first:** ADR-022; ADR-015, M2-F5 and M2-F6; `Simulation/World.cpp`'s `FormTrail`, `Walk` and `SettleUnits`; *Contracts*, `Simulation`, `FormationSlots`.
- **Changes:** reservations, the swap, slots and the wait in step 3, and step 9's free-cell rule, in the filter `Navigation`; `Simulation/Formation.h` and `.cpp`.
- **Tests:** **`FormationTests`:** `FormationSlots` for six facing north is two ranks of three behind the lead, and for each of the eight directions the same shape turned; three outriders stand in one rank. **`MovementTests`** gain: on open ground a squad of six arrives in two ranks about its goal; through a 1 m gap it passes in single file and re-forms; no two units share a cell on any tick of the breach run with two squads ordered through the breach; two squads meeting head-on in a 1 m corridor both pass; a squad blocked for `waitTicksBeforeSearch` searches again; the rate through the breach run's slot is recorded and replays to the same ticks. M2's single-file checks for a squad alone in a 1 m corridor pass unchanged.
- **ADR:** marks ADR-022 Accepted; ADR-015's decision 6 and *Left open* point to it.

### M4-C3 — Melee and the squads' orders
- **Waits on:** C2.
- **Read first:** ADR-023, decisions 3 and 4; Hud.md §7's cards for soldiers; GameDescription.md, *Squads*.
- **Changes:** acquisition, melee strikes worked out from the step's starting state and applied together, `AttackTarget` on a squad or an engine, `StormTo`, `StopSquad`, `HoldSquad`, `SplitSquad` and `MergeSquads`, in the filter `Combat`.
- **Tests:** **`MeleeTests`:** for each counter pairing of the roster, six against six on open ground, the side the roster says is strong wins, and the ticks and survivors match the inputs' arithmetic; two figures that kill each other on the same tick both die; a holding squad strikes and does not move; a storming squad engages what it meets and goes on; split gives a squad of one with the highest-id figure; merge of a four and a three gives a six and a one; merge across kinds is refused.

### M4-C4 — Arrows
- **Waits on:** C3, and input 7.
- **Read first:** ADR-023, decisions 5 and 6; `Simulation/Shot.h`'s `TraverseVoxels`; Review/PaperTests/SiegeRouteTable.md, *Reach*.
- **Changes:** ranged figures loose arrows as `Shot`s of kind `Arrow` with an `ArrowLine`, traced each tick in step 4; elevation steps and bands; line of fire checked before loosing; a blocked squad walks towards its target; `SquadAim` answered (record version 2), in the filters `Combat` and `Queries`.
- **Tests:** **`ArrowTests`:** an archer on level ground reaches 30 m and not 31; one standing 6 m above its target reaches 40 m and deals the two-step damage; one 6 m below deals the uphill damage; a wall between shooter and target stops the arrow at its face, deals nothing, and the squad walks until it has a line; an arrow passes a unit in its way that is not its target; a target that leaves its aim point by more than a cell is missed; `SquadAim` reads IN RANGE, BLOCKED and OUT OF RANGE as the arrow would.

### M4-C5 — The trebuchet against units and engines, its stone, deploying and packing
- **Waits on:** C1, and inputs 8 and 17.
- **Read first:** ADR-023, decisions 7 and 10; MvpAssets.md, *The trebuchet's poses*; GameDesign.md, *the MVP assets v3 package*, items 1 and 6.
- **Changes:** a shot's crush on units and engines; the shot's stone from the stockpile, `NotEnoughStone` below it; `PackEngine` and `DeployEngine` with their times, a packed trebuchet moving as machines, firing only deployed; the packed wagon's width as input 17 decides, in the filter `Destruction`.
- **Tests:** **`EngineTests`** (`SimulationTests`): a shot landing beside a squad damages the figures within a cell and no others; four shots kill a trebuchet as input 6 says; a shot costs 1.5 m³ and the 81st of 120 m³ is refused; a packed trebuchet cannot fire; deploying takes its time and the engine fires the tick after; a packed trebuchet paths as machines from the camp to a stand. `ShotTests` and `BreachRunTests` pass unchanged.

### M4-C6 — The ram
- **Waits on:** C2 and C5, and input 9.
- **Read first:** ADR-023, decision 9; GameDescription.md, *Craters* and *Siege units*; Review/PaperTests/SiegeRouteTable.md, ambiguities 3 and 5.
- **Changes:** `AttackTarget` from a ram at a structure: it drives to touch it and strikes on its cadence, at the voxel the decision names, with input 9's craters, each blow an edit as a shot's crater is, in the filter `Destruction`.
- **Tests:** **`RamTests`:** blows land every `ramBlowSeconds`; on T5's gate each removes 4 timber voxels; the gate is open to foot after the blows the inputs' arithmetic gives, and to machines after all 12; on a T3's foot each blow removes one stone voxel within the bottom metre and none above; on earth one voxel; the ram's roof takes 1 from an arrow and full damage in melee.

## Phase D — Structures at work

### M4-D1 — The built mask, critical parts and working
- **Waits on:** B3 and C1, and input 10.
- **Read first:** ADR-024, decisions 1 and 2; ADR-005, decision 10; `Simulation/ServerData.h`'s `ServerTemplate::parts`; MvpPlan-M3.md, M3-B1.
- **Changes:** `Structure` at record version 2, with its mask, its part's share kept from the tick's edits, working, and `StructureStopped` and `StructureWorks` events; `StructureView`'s fields, in the filter `Structures`.
- **Tests:** **`StructureWorkTests`:** a T4 works at 100 % and stops when its firing platform falls below input 10's threshold; voxels in a collapse in warning do not stand; a T3 with no critical part always works; the share is unchanged on a tick with no edit inside the footprint; a save with masks round-trips.
- **ADR:** marks ADR-024 Accepted.

### M4-D2 — Gates
- **Waits on:** D1, and input 13.
- **Read first:** ADR-024, decision 6; GameDesign.md, *the MVP assets v3 package*, item 3; `Simulation/NavigationGrid.cpp`'s surface and headroom.
- **Changes:** `SetGate`; navigation reads an open gate's voxels as air and marks its tiles for step 8; a gate that stops working shuts.
- **Tests:** **`GateTests`:** a shut T5 blocks foot and machines; open, both path through it; a shot at an open gate's voxels strikes them; opening rebuilds the tiles on the next step 8 and the squads waiting search again; a broken gate cannot be opened; a ram reaches T5's gate down its passage (ADR-015's 3 by 4).

### M4-D3 — Garrisons
- **Waits on:** D1 and C4, and input 10.
- **Read first:** ADR-024, decision 5; Review/PaperTests/SiegeRouteTable.md, *Reach*.
- **Changes:** `GarrisonSquad` and `EjectGarrison`; garrisoned units out of the grid, healing and firing from the outer face at the stand height; put out when the building stops working, in the filter `Structures`.
- **Tests:** **`GarrisonTests`:** six archers garrison a T4 and a seventh is refused `Full`; garrisoned archers reach 40 m from the tower's face and not 41; they take no damage; they heal; eject puts them on free cells on the owner's side clear of the footprint; a tower that stops working puts them out and its fall strip stays clear.

### M4-D4 — The gatehouse's passage attack
- **Waits on:** D3 and C6, and input 13.
- **Read first:** ADR-024, decision 6; GameDesign.md, *the design review*, item 9, *Gatehouses*.
- **Changes:** the attack on its cadence, while the gatehouse works and holds a garrison.
- **Tests:** **`PassageAttackTests`:** a ram in T5's passage takes `passageAttackDamage` every `passageAttackSeconds`; so does a foot squad; it stops when the garrison leaves or the gate breaks; the ram's time to break the gate under the attack is reported against GameDescription.md's four minutes (MvpPlan.md, M4's risks).

### M4-D5 — Repair and its lock
- **Waits on:** D1, and input 11.
- **Read first:** ADR-024, decisions 3 and 4; Review/PaperTests/SiegeRouteTable.md, *Repair*; GameDesign.md, *the paper tests' values*, item 3.
- **Changes:** `RepairStructure` for laborers; repair in step 2 at the fixed rate, from the stockpile, lowest first; the lock from shots, ram blows and trench cells, counted down in step 2; `StoneSpent` and `TimberSpent` events; `IntegrityReading::repairLockTicks`, in the filter `Structures`.
- **Tests:** **`RepairTests`:** a T3 with a two-voxel crater and a laborer on its inner side is whole again after the ticks the rate gives, and the stockpile is 2 m³ lighter; two laborers repair no faster; a laborer on the outer side repairs nothing; a shot sets the lock and repair waits 200 ticks; with the trebuchet at 20 s and repair on, the breach run's section opens after the multiple of shots the route table found, 1.5× within a shot (GameDesign.md, *the paper tests' values*, item 3); a voxel that would stand unsupported waits.

### M4-D6 — Sappers' trenches
- **Waits on:** D5 and C1, and input 12.
- **Read first:** GameDescription.md, *Undermining*; GameDesign.md, *Where RTS logic meets destruction*, on designations leaving a ramp; Review/PaperTests/SiegeRouteTable.md, *E3 by sappers*.
- **Changes:** `DigTrench` for sappers: the box dug layer by layer from the top, each layer leaving a ramp at the end nearest the squad, at the squad's rate summed over the sappers beside the box, each whole cubic metre an edit whose spoil leaves play; trench cells within 2 cells of a footprint lock it, in the filter `Jobs`.
- **Tests:** **`TrenchTests`:** six sappers dig a 2 × 8 × 4 m trench in the time input 12's rate gives; the trench always has a ramp a foot soldier can climb out by; under E3's footprint on deep earth with a 4 m foundation the section falls after the route table's volume, within a layer; on rock nothing falls; a sapper killed slows the dig.

## Phase E — Fog

### M4-E1 — Visibility, the filtered state, and chunks sent when seen
- **Waits on:** C1 and D1, and inputs 6 and 14.
- **Read first:** ADR-025; ADR-020, decisions 2 and 3; ADR-009, decision 4; `Simulation/World.cpp`'s `Publish`; *Contracts*, `Simulation`, `Visibility`.
- **Changes:** `Simulation/Visibility.h` and `.cpp`, in the filter `Fog`; step 11 stamps each player's visibility and fills `State(player)` through the filter; `Publish(player)` with the versions sent per player; structures last seen; answers filtered; the scenario's map seen at the start; the saved state ADR-025 names.
- **Tests:** **`FogTests`:** an enemy squad 33 m from the attacker's archers is absent from the attacker's update and present at 31; a garrison's count is the owner's alone; a section cratered out of sight keeps its last-seen view and its chunks are not sent, then both arrive the tick an attacker sees it; the attacker's first update after a scenario that reveals the map sends every chunk once; an integrity query on a section out of sight answers `notVisible`; a save and load filters the same. **`SimulationBench --siege`** reports visibility's cost a tick.
- **ADR:** marks ADR-025 Accepted; ADR-020, decisions 2 and 3, amended: the update and the chunks are per player.

## Phase F — The trial

### M4-F1 — The trial's rules and its events
- **Waits on:** E1, D1 and B4, and inputs 15 and 16.
- **Read first:** ADR-026, decisions 4 and 5; GameDesign.md, *Stages*, 1; *Contracts*, `Simulation`, `TrialRules` and `TrialEvent`.
- **Changes:** step 10's trial: the storming clock, its pause and reset, the time limit, the army rule, the outcome; `TrialView` in every update and `StructureView::opened`; the events of ADR-026, decision 5, `SectionOpened` from the camp's reach after each rebuild and `FigurePassed` from step 3's steps, in the filter `Trial`.
- **Tests:** **`TrialRuleTests`**, on the castle with units placed by hand: soldiers in the ring of a working keep start no clock; with the keep stopped the clock counts, pauses while a defender stands in the ring, resets when the attackers leave and when a keep voxel is repaired, and wins at 2,400 ticks; the limit loses; the last attacker soldier's death loses with engines still standing; S3 opened by scripted shots raises one `SectionOpened`; six figures through it raise six `FigurePassed`.
- **ADR:** ADR-026, decisions 4 and 5, implemented.

### M4-F2 — Commanders, and the defender's garrison, fire and repair
- **Waits on:** F1, D3 and D5, and input 18.
- **Read first:** ADR-027; *Contracts*, `Simulation`, `Commander`.
- **Changes:** `Commander`, `CommanderView`, step 13, commanders saved; `SiegeDefender`'s first half: garrisons fire at sappers first, then rams, then the trebuchet, then the nearest; laborers repair the most damaged unlocked structures, one each, nearest first; archers put out go to the nearest working building with room, in the filter `Commanders`.
- **Tests:** **`SiegeDefenderTests`:** sappers at E3's foot draw the SE and NE garrisons' arrows before spearmen beside them; a cratered tower is repaired once its lock ends; a stopped tower's archers garrison the keep; a run of 2,000 ticks with the defender replays equal and continues equal from a save at tick 1,000; the defender never issues a command for a unit it cannot see an enemy near.
- **ADR:** marks ADR-027 Accepted.

### M4-F3 — The defender's sally, gate and breaches
- **Waits on:** F2 and D2, and input 18.
- **Read first:** ADR-024, decision 6; MvpPlan.md, *In*, on the Siege defender.
- **Changes:** `SiegeDefender`'s second half: the sally on an engine within the leash, opening the gate and shutting it, the return; the reserve to an opened section's inner mouth.
- **Tests:** **`SiegeDefenderTests`** gain: a trebuchet deployed 60 m from the curtain draws a sally that kills it and returns, and the gate shuts behind it; one at 80 m draws none; an attacker soldier near the open gate shuts it with the sally still out; an opened section draws the reserve to its mouth.

### M4-F4 — The control
- **Waits on:** D1, D5, D6, C5 and C6, and input 19.
- **Read first:** ADR-028; GameDesign.md, *Hypotheses*; Hud.md §1.
- **Changes:** `StructureRule`, `VoxelStructureRule` (D's behaviour, moved unchanged) and `HealthStructureRule`, chosen from the trial's version, in the filter `Structures`; the calibration of ADR-028, decision 5, run in `SimulationBench --calibrate-control` and written into `Structures.json`; ADR-004 amended.
- **Tests:** **`ControlTests`:** in the control a shot on a T3 removes no voxel and takes its damage; at 0 a T3 falls whole with its warning and leaves its rubble; a T4 at 0 stops and stands; a gate at 0 is removed; repair restores hit points at the rate and the price; terrain craters as in the voxel version; every other test of phases C–E passes in the control unchanged where it does not touch structures. The voxel version's tests pass unchanged after the move behind the seam.
- **ADR:** marks ADR-028 Accepted.

### M4-F5 — The scripted attacker, and the trial in CI
- **Waits on:** F3 and F4, and input 27.
- **Read first:** MvpPlan.md, M4, *Done when*; ADR-027, decision 1; the M2 report's shape for the bench.
- **Changes:** `SiegeAttacker`, a commander that deploys the trebuchet at a stand outside the towers' reach, stops the towers covering the keep's foot from the south and east, opens a section, storms through it, and holds the foot; `SimulationBench --siege [--control]` runs it against the defender and reports the slowest tick of play, tick 0, the 95th percentile, the outcome, the shots, each side's stone and every opened section's figures a minute; input 27's budgets set from it.
- **Tests:** **`SiegeTrialTests`:** in both versions the attacker wins by storming inside the limit; with no commands the attacker loses to the clock; with its soldiers sent into the towers' reach until dead it loses by the army; each run replays with every state hash equal; a save every 1,000 ticks continues equal; outcomes, ticks and shots are committed as a table and printed when they change, as M3's lab keys are, since a retune changes them.
- **Report in the commit:** the trial's times against the balance targets (one trebuchet opens a section in about three minutes, repair at most doubles it, a gate falls to a ram in about four) and the slowest tick in the container.

## Phase G — `Engine`

### M4-G1 — The line pass
- **Waits on:** A1, for ADR-018's go-ahead.
- **Read first:** ADR-018; `Engine/VoxelPass.h` and its tests; `Content/Catalog.h`'s `LineModelView`; *Contracts*, `Engine`, `LinePass`.
- **Changes:** `Engine/LinePass.h` and `.cpp`, `Engine/Shader/LineVS.hlsl` and `LinePS.hlsl`, in the filter `Rendering`.
- **Tests:** **`LinePassTests`** on WARP: an arrow across a voxel wall is hidden where the wall is in front and drawn where it is not; its shaft is 2 px wide at two view heights; shorter than 6 px on screen it is a 2 px dot; two lines crossing occlude by depth.
- **ADR:** marks ADR-018 Accepted.

### M4-G2 — Silhouettes and the column shade
- **Waits on:** A1, for ADR-021's amendment and ADR-025's go-ahead, and input 23 for the silhouettes' alpha.
- **Read first:** ADR-021, decisions 1 and 6; ADR-025, decision 8; `Engine/VoxelPass.cpp` and `Shader/VoxelPS.hlsl`.
- **Changes:** `VoxelPass::BeginSilhouette` and its pipeline state; `SetColumnShade` and the shader's read of the mask; ADR-021 amended with the silhouettes' decision.
- **Tests:** **`VoxelPassTests`** gain: a model voxel behind a wall draws its silhouette colour at the alpha over the wall's pixels and nowhere it is visible; a world voxel in a shaded column is multiplied by the shade within 2 of 255, one in an unshaded column unchanged; with no mask nothing changes.

### M4-G3 — The image atlas and the portraits' render
- **Waits on:** A1, and input 21.
- **Read first:** ADR-019, decision 3; `Engine/InterfacePass.h`; Hud.md §5, the portrait.
- **Changes:** `InterfacePass::SetImageAtlas` and `textured` 2; ADR-019 amended.
- **Tests:** **`InterfacePassTests`** gain: an image quad samples a 2 × 2 RGBA atlas point by point at scale 4; colour multiplies it; a glyph quad and an image quad in one draw both draw.

## Phase H — The client draws the war

### M4-H1 — Units, engines and gates, interpolated
- **Waits on:** F5, for the scripted attacker, and input 20.
- **Read first:** ADR-020, decision 4, amended by input 20; MvpAssets.md, the models' table and *Poses, projectiles, gates and debris*; `Client/WorldRenderer`, `Client/Look.h`; *Contracts*, `Client`, `DrawnPosition`.
- **Changes:** every unit drawn as its kind's model in its owner's faction entry, at its interpolated position, facing its last step; garrisoned units not drawn; the ram as S1; the trebuchet as S3p packed and moving, S3c deployed, S3 released after a throw; gates open drawn as G1 or G2 with the gate's voxels hidden from the world's batch; the impostors below 3 px as M1 has them, in the filter `World`. `Earthrise --siege --watch` runs `SiegeAttacker` for the player, so that the owner can watch a trial before the HUD can order one; J1 keeps the flag. ADR-020, decision 4, is amended with input 20.
- **Tests:** **`InterpolationTests`:** halfway between ticks a figure is drawn halfway; a figure that dropped 4 m is drawn at its new place at once; an entity new in the last update is drawn there. **`WorldModelsTests`:** an open gate's instance hides exactly its gate voxels; each kind gives its model.
- **Owner runs:** `Earthrise --siege --watch --windowed`: the armies moving in ranks and filing through the gate and the breach, smoothly, the trebuchet packing, moving and deploying, the ram at the gate, the gate opening for the sally.

### M4-H2 — Arrows
- **Waits on:** H1 and G1.
- **Read first:** ADR-018, decision 3 and *Left open*; *Contracts*, `Engine`, `LinePass`.
- **Changes:** one `LineInstance` per arrow in flight, at its interpolated position with its direction from its last two, in its shooter's faction colour, drawn after the voxel pass, timed with its timestamps, in the filter `Rendering`.
- **Tests:** **`ArrowDrawTests`:** an arrow's instance lies on its line between ticks and points along its flight.
- **Owner runs:** volleys from the towers and from the attacker's archers at the default zoom and fully zoomed out: arrows read as arrows, and a volley's cost in the title's timings.

### M4-H3 — Fog and silhouettes drawn
- **Waits on:** H1, G2 and E1, and inputs 14 and 23.
- **Read first:** ADR-025, decisions 4 and 8; ADR-021, amended; RendererLook.md §7 on what the angle hides.
- **Changes:** `Session` keeps the newest snapshot of each chunk; the visibility uploaded as the column shade each tick it changes; the player's units and visible enemies drawn as silhouettes behind walls, rubble and terrain, in the filter `Views`.
- **Tests:** **`SessionTests`** gain: a chunk not sent keeps its last snapshot; **`FogDrawTests`:** the mask's texels follow the update's bits.
- **Owner runs:** the castle in fog from the camp: last-seen land, the garrison unseen, a breach seen only when watched; units in the ditch and behind the curtain as silhouettes.

## Phase I — Layout A

### M4-I1 — The deck, the HUD model and the scale's cap
- **Waits on:** H1, and input 21.
- **Read first:** Hud.md §2, §3 and §10; ADR-019, decision 5; *Contracts*, `Client`, `HudModel`.
- **Changes:** the deck at the bottom, 204 logical px, with the minimap's well, the selection's panel and the command card's grid empty; the control groups' strip; the camera framing the area between the top bar and the deck; the UI scale capped at the largest step that leaves the deck its 972 logical px (input 21), in the filter `Hud`.
- **Tests:** **`HudLayoutTests`:** at 1920 × 1080 the deck's columns are 188, the rest and 4 × 112 + 3 × 4 wide; at 1280 px the scale caps at 1 and at 1920 px at 1.5, whatever the DPI; the camera's look-at point sits in the middle of the uncovered area; nothing overlaps at 1280 × 720 and 1920 × 1080 at every scale the width allows.

### M4-I2 — Selection and the selection panel
- **Waits on:** I1 and G3, and input 21.
- **Read first:** Hud.md §5 and §8; MvpPlan-M3.md, M3-E3; *Contracts*, `Client`, `Selection`.
- **Changes:** a click selects a squad by its footprint on the ground, an engine by its footprint, a laborer alone, a structure by its voxels; Shift and Ctrl add and remove; a drag box-selects; a double-click selects every squad of the type on screen; Tab cycles the active kind; Esc deselects. The panel: nothing, one, and several with tiles; the portraits rendered once at startup through the voxel pass into the image atlas, structures as their icon glyphs; hit points, figures and the stat rows of Hud.md §5 with input 6's values; enemy things read ENEMY, and their garrisons as `?` unless seen, in the filters `Selection` and `Hud`.
- **Tests:** **`SelectionTests`:** a click inside a squad's footprint selects it, outside does not; a box over two squads selects both; a double-click picks only the type; Tab moves the active kind in Hud.md's order; **`SelectionPanelTests`:** every row fits its 220 px at every scale; the several-squads title and tiles read as Hud.md says.
- **Owner runs:** selecting the army by click, box, double-click and Tab; the panel's portraits and rows at both sizes and every scale.

### M4-I3 — The command card, its modes and its keys
- **Waits on:** I2.
- **Read first:** Hud.md §7 and §8; MvpPlan-M3.md, M3-E4; ADR-007, decision 4.
- **Changes:** the card for each kind M4 has: soldiers, sappers, the ram and the trebuchet, Hud.md §7's slots, labels, hotkeys and disabled states; targeting modes MOVE, ATTACK, STORM and TRENCH with their banners, as FIRE is; the attack cursor for every ranged attack, asking `SquadAim` for archers; a right click's default order by what lies under the pointer; Shift keeps a mode; the tooltip on hover, and its text also in the banner when a mode is entered by key or click, in the filter `Hud`.
- **Tests:** **`CommandCardTests`:** each kind's slots are Hud.md's in order; every label fits 90 logical px; every hotkey is unique within its card and none is W, A, S or D; FIRE is disabled while reloading, packed or below a shot's stone; PACK while packing; SPLIT with one figure; MERGE below two squads; **`ModeTests`:** each mode's key enters it and sets its banner; a right click submits its command and leaves the mode unless Shift is held.
- **Owner runs:** ordering the army through every card by key and by click; STORM up to the keep; TRENCH beside E3; the trebuchet packed, moved and deployed.

### M4-I4 — Control groups
- **Waits on:** I3.
- **Read first:** Hud.md §3, §7 and §8.
- **Changes:** Ctrl + 1–9 and Ctrl-click on a slot assign; 1–9 selects, twice centres the camera on the group in the uncovered area; the strip shows each group's count and kind, 44 high (input 21); groups kept by the client and cleared of the dead.
- **Tests:** **`ControlGroupTests`:** assigning, selecting and centring; a dead squad leaves its group; a group of nothing shows empty; the strip's slots are 120 × 44 hit targets.

### M4-I5 — The minimap
- **Waits on:** I1 and H3.
- **Read first:** Hud.md §6; ADR-025, decision 4.
- **Changes:** the base built at its drawn size from the session's last-seen chunks, by reducing columns, rebuilt when a chunk changes; squads' and structures' dots in team colours, selected in #ffe2a8, only where the update put them; the camera's view quad; alert pings; press or drag moves the camera, right-click orders the selection, in the filter `Minimap`.
- **Tests:** **`MinimapTests`:** 512 m reduces to 188 px as Hud.md says; a dot appears only for a squad in the update; the view quad's corners are the screen's corners on the ground; a click at the centre moves the camera's look-at to the map's centre.
- **Owner runs:** the minimap through a trial's opening: the castle as last seen, the army's dots, the view quad turning with the camera, a ping on a collapse alert.

### M4-I6 — Alerts, the repair lock, and the integrity view across the castle
- **Waits on:** I3, and inputs 22 and 23.
- **Read first:** Hud.md §5 and §10's alerts; ADR-021, *Left open*; MvpPlan-M3.md, M3-F1 and M3-F2.
- **Changes:** the alerts M4 has: WILL FALL and HAS FALLEN (M3's), SAPPERS AT … when enemy sappers are first seen, BREACH OPEN AT … when a section's view first reads `opened`, HIT W5 · REPAIR LOCKED 10 S when the player's shot lands on one, NOT ENOUGH STONE on a refusal, each with JUMP where it has a place and Space for the newest; the lock as `Locked · n s` on the integrity labels and the selection's rows; the wall section's row as input 22 says; the labels across thirty structures as input 23 says, in the filters `Hud` and `Views`.
- **Tests:** **`AlertTests`:** each alert's trigger and text, its JUMP target, and Space; a refusal gives one alert, not one a tick; **`OverlaysTests`** gain: with 30 structures on screen the labels follow input 23 and none leaves the screen.
- **Owner runs:** a trial to its first breach: the alerts as they fire, the lock on the labels after each shot, and whether thirty labels read or crowd.

### M4-I7 — Hover, and where each command came from
- **Waits on:** I4, I5 and I6.
- **Read first:** ADR-007, decision 4; Hud.md §8, *Hover and ADR-007*, and §11.
- **Changes:** a table in `Client/Strings.h` of every reading the HUD can show and how it is reached without hover; every command the player issues recorded with its source, `key` or `click`, and its time, for J2's log.
- **Tests:** **`HoverTests`:** every reading in the table is reachable by a key or a click alone, driven through the HUD model with no pointer movement; **`CommandSourceTests`:** a command by hotkey records `key`, by card or right click `click`.

## Phase J — The trial

### M4-J1 — `Earthrise --siege`
- **Waits on:** F5 and I7, and inputs 16 and 25.
- **Read first:** ADR-026; MvpPlan-M3.md, M3-C3 and M3-G1, for `Earthrise`'s loop; Hud.md §10's objective.
- **Changes:** `Earthrise --siege <participant> [--control] [--seed n]`, in `Earthrise/Siege.h` and `.cpp`, in the filter `Siege`: the scenario loaded, the defender's commander on, the camera on the camp; the objective `TAKE THE KEEP`, then `HOLD THE KEEP'S FOOT · 0:42 / 2:00` with its progress; the resources `STONE m³`, `ARMY n`, `TIME m:ss` of the limit left; pause on input 25's key; the end banner `THE KEEP HAS FALLEN`, `TIME HAS RUN OUT` or `YOUR ARMY IS GONE`, with the time; `--watch` runs `SiegeAttacker` for the player, for the owner's checks and J4. Without a participant it refuses with a message.
- **Tests:** **`SiegeFlowTests`** (`ClientTests`, over the model the loop drives): the objective and resources read as above; the end banner follows the outcome; pause stops the clock and nothing else.
- **Owner runs:** `Earthrise --siege P00 --windowed`, played to a win and to a loss: the whole trial, the HUD, the defender sallying and repairing, the storming clock.

### M4-J2 — The trial's log, its replay, and the reader
- **Waits on:** J1, and input 25.
- **Read first:** ADR-026, decisions 5 and 6; *The trial's log*; MvpPlan-M3.md, M3-G2's scoring tool.
- **Changes:** `Earthrise` writes the log from `LastTickEvents` and the client's command records, and the replay beside it; `Earthrise --replay <file>` plays one back on screen and reports whether every state hash matched; `Tools/SiegeTrial/ReadSiegeLog.py <folder>` prints each run's outcome, time, breach points, shots, each side's stone and the figures a minute through each breach, `--json` the same; `Tools/TestReadSiegeLog.py` with a step in the workflow's Linux job.
- **Tests:** `Tools/TestReadSiegeLog.py` over hand-written logs; **`SiegeLogTests`** (`ClientTests`): a run's rows have every column; the replay of a short scripted run matches.

### M4-J3 — The control's client
- **Waits on:** J1, and input 19.
- **Read first:** ADR-028, decision 6; Hud.md §1 and §5.
- **Changes:** health bars over sections with hit points; HEALTH in place of PLATFORM on towers; nothing else.
- **Tests:** **`ControlParityTests`:** the HUD model built from one state in both versions differs in the health bars and the towers' row alone, compared field by field.
- **Owner runs:** `Earthrise --siege P00 --control --windowed`, played to a win: the same trial with health-bar walls, and nothing else different.

### M4-J4 — The measured run
- **Waits on:** J1.
- **Read first:** GameDesign.md, *Starting the MVP*, item 3; `RendererSpike`'s measurement, which M3 moved into `Client`.
- **Changes:** `Earthrise --siege --watch --measure` runs the trial live with the scripted attacker for its first ten minutes of game time under M1's camera path about the castle, and writes the voxel pass's, the line pass's and the tick's times to a CSV with the 95th percentiles and the slowest tick of play.
- **Owner runs:** on the benchmark machine, Release x64: the voxel pass at the 95th percentile against 8 ms, the slowest tick against 25 ms, and the line pass's cost.

### M4-J5 — The playthroughs and the report
- **Waits on:** J2, J3 and J4, and the owner's playthroughs.
- **Read first:** MvpPlan.md, M4, *Done when*; the M2 report's shape; MvpPlan-M3.md, M3-G3.
- **Changes:**
  - **`Design/Review/M4-<date>.md`** reports: the done-when line by line; the scripted trial's times against the balance targets; the owner's playthroughs, their logs under `Design/Review/SiegeTrial/`, wins and losses in both versions; the measured run; the CI runs per order; each ADR amendment; the values the owner asked changed; and what M5 inherits.
  - **GameDesign.md,** *M4's values*, input 27 becomes Decided at the measured budgets.
  - **MvpPlan.md:** the M4 record is complete.
