# Earthrise.World — M3 work orders

**Status:** Proposed, 2026-11-01, with [MvpPlan.md](MvpPlan.md)'s M3 revision, which is accepted when the owner merges its pull request, #45 (GameDesign.md, *Closing M2*, item 6). The owner answered MvpPlan.md, M3, inputs 1–15 the same day, taking every proposal, and inputs 16 and 17 after main's v3 asset package merged (order M3-A1; GameDesign.md, *M3's values*). No order from phase B on starts before that pull request merges; phase A is paper. "Input N" means MvpPlan.md, M3, input N. These orders carry out M3 as MvpPlan.md, M3, defines it, under [ADR-019](ADR/ADR-019-text-and-the-interface-pass.md) to [ADR-021](ADR/ADR-021-world-overlays.md). Where an order and those differ, they win, and the order is corrected.

**An order is one commit.** Phase A went in the plan's pull request, #45, and phases B to G go in one draft pull request, by the owner's choice of 2026-11-01, as in M2, in place of AGENTS.md §6's one pull request a phase. There are 24 orders in seven phases:

| Phase | Orders | What lands | Waits on |
|---|---|---|---|
| A | A1 | The owner's decisions, written down | The owner's answers to MvpPlan.md, M3, inputs 1–17 |
| B | B1–B8 | The simulation's side: structures, state updates, queries, the three readings, the lab's items, the font's and the debris table's bake | A |
| C | C1–C3 | `Client` and `Earthrise`; the world view moves; the breach run on screen | B1–B3 |
| D | D1–D2 | `Engine`'s interface pass and overlay pipeline, with their render tests | A |
| E | E1–E4 | Text and the HUD's first pieces; picking, selection and FIRE | B8, C, D |
| F | F1–F3 | The collapse preview, the integrity view, the passability view | B4–B6, E |
| G | G1–G3 | The lab, its scoring, the session and the report | B7, F |

D needs only A, so its two orders can land anywhere before E; everything from E on is serial. **Owner runs** fall where an order draws: after C2, C3, E2, E4, F3 and G1, and the six-person session after G1's dry run. That is the pace M3 keeps (MvpPlan.md, M3, *What M3 inherits from M2*).

## How a session works an order

1. **Read** AGENTS.md, this section, *Contracts*, the order, and the order's *Read first* list. Nothing else should be needed. If something is, the order is incomplete: say so in the report.
2. **Check *Waits on*.** An order starts only when every order it lists is committed, in its own phase, or merged, in an earlier one, and every input it lists is decided.
3. **Keep the contracts.** The signatures under *Contracts* are what other orders build on. A session that cannot keep one stops and reports. The contract is then changed in this file, in a commit of its own, before any code departs from it. A session never deviates silently (AGENTS.md).
4. **Verify through CI.** The cloud container has no MSVC, no vstest and no GPU, so CI is the build, and WARP is the GPU ([ADR-012](ADR/ADR-012-render-tests-on-warp.md)).
   - **Before pushing,** run what runs here: `Build/CheckProjectFiles.py`; `Build/CheckFormat.py` with clang-format 18 where it is installed; `Build/TestCheckers.py` when a checker changed; and `Tools/CheckAssets.py`, `Tools/TestCheckAssets.py` and `Tools/TestBakeContent.py` when `GameData/` or `Tools/` changed.
   - **Then push the order's commit** to the branch of the one draft pull request for B to G. CI must be green before the next order starts.
   - **Count the CI runs** the order took, failures included. The commit's last body line says `CI runs: N`, and a fix that CI forced is a commit of its own, as M2 did.
5. **Name the commit for the order.** Its subject starts with the order's id, as in `M3-B3: Answer queries within the tick's budget`.
6. **Register every file** in its project's `.vcxproj` and `.filters`, under the functional filter the order names. Folders stay flat (AGENTS.md §2). A new project goes into `Earthrise.slnx`, `.clang-tidy`'s `HeaderFilterRegex` and the checker's layering table in the commit that creates it.
7. **Write the tests the order names.** They go in `<Project>Tests/<Type>Tests.cpp`, in `TEST_CLASS(<Type>Tests)`, with methods named for the behaviour they check. A render test creates its device with `GpuDevice::Desc::useWarp` and asserts coverage, colour and depth by number, never a golden image (ADR-012).
8. **Save what you add.** An order that adds an entity kind adds it to the save and to the state update, and extends `SaveTests` and `StateUpdateTests`, in the same commit (ADR-014, decision 4; ADR-020, decision 3).
9. **Accept the ADR you implement.** The commit that first implements an ADR marks it Accepted, with the date and any amendment the code forced (AGENTS.md §6). Each order says which ADR, if any.
10. **Hand the screen to the owner.** An order that draws or takes input lists what the owner should see under *Owner runs*, and the owner launches it before the pull request merges, and as soon after the order as the owner can, so that the orders after it do not rest on a picture nobody has seen (AGENTS.md §3). A Claude session cannot see a frame; what the owner reports comes back as a fix commit, or as a new order in the same phase, as M2-D3 provided for.
11. **Leave out-of-scope work out.** What a session notices beyond its order goes in the PR description, not the commit.
12. **Log nothing about time** (GameDesign.md, *No time logging*). The pull request and its commits, one per order, are the record.

## Contracts

The seams more than one order builds on. Parameters, members and constants follow AGENTS.md §1. Each contract's comments state its rules. `Client` and `Engine` work in `float` and convert `Core::Fixed` at the boundary, the one place a simulation number becomes a float (ADR-013, *Consequences*).

### `Protocol`: what crosses the boundary (B2, B3)

```cpp
// Protocol/FixedPosition.h — moved from Simulation, unchanged: Core::Fixed x, y and z in meters.
struct FixedPosition;

// Protocol/EntityKinds.h — the enumerations the views carry. Simulation's NavigationClass, EngineKind
// and the unit's new kind move here; Simulation keeps their names through using-declarations.
enum class NavigationClass : std::uint8_t { Foot, Cavalry, Machines };
enum class EngineKind : std::uint8_t { Trebuchet };
enum class UnitKind : std::uint8_t { Laborer, Spearman, Archer, Outrider, ManAtArms, Sapper }; // which model the client draws (MvpAssets.md, U1–U5, S2)

// Protocol/StateUpdate.h — ADR-020, decision 3: the whole visible state, a value, once a tick. Every
// vector is in id order. No byte encoding until online work; M3 copies the struct.
struct StructureView { EntityId id; std::uint8_t owner; std::string templateId; VoxelPosition originVoxels; std::uint32_t quarterTurns; std::string name; };
struct EngineView { EntityId id; std::uint8_t owner; EngineKind kind; VoxelPosition cellVoxels; std::uint32_t quarterTurns; std::uint32_t reloadTicks; };
struct SquadView { EntityId id; std::uint8_t owner; std::vector<EntityId> units; bool hasGoal; VoxelPosition goalVoxels; bool shortOfGoal; };
struct UnitView { EntityId id; EntityId squad; std::uint8_t owner; UnitKind kind; FixedPosition position; VoxelPosition cellVoxels; };
struct ShotView { EntityId id; EntityId engine; FixedPosition position; };
struct PlacedVoxel { VoxelPosition position; std::uint8_t paletteIndex; }; // Simulation's PlacedVoxel (CollapsePlan.h) and FallenVoxel (TestWorld.h), the same two fields, moved here as one
struct CollapseView
{
  EntityId id;
  EntityId structure;              // whose piece it is, or NO_ENTITY
  std::uint32_t warningTicksLeft;  // 0 once it falls
  std::uint32_t fallTicksLeft;
  std::vector<VoxelPosition> piece;
  std::vector<VoxelPosition> strip; // CollapsePlan::strip: where units are hit
  std::vector<PlacedVoxel> landing; // CollapsePlan::landing from the first warning tick, nearest the foot first: where its debris rests (input 16)
  std::uint32_t landedVoxels;       // how many of landing have landed: 0 until the fall's first tick
  std::vector<PlacedVoxel> fallen;  // the piece's voxels as they were, empty until the fall's first tick
};
struct ShotHitView { EntityId shot; VoxelPosition voxel; };
struct StateUpdate
{
  std::uint32_t tick;
  std::vector<StructureView> structures;
  std::vector<EngineView> engines;
  std::vector<SquadView> squads;
  std::vector<UnitView> units;
  std::vector<ShotView> shots;
  std::vector<CollapseView> collapses;
  std::vector<ShotHitView> hits; // the tick's
};

// Protocol/Query.h — ADR-009, ADR-020 decisions 5 and 6. On the wire: u32 query id, u16 tag, u16 record
// version (1), then the fields in declaration order. An answer: u32 query id, u32 tick, u16 tag, u16
// version, the reading's fields. Tags: 1 aim check, 2 section integrity, 3 tile reach.
struct AimCheck { EntityId engine; VoxelPosition targetVoxels; };
struct SectionIntegrity { EntityId structure; };
struct TileReach { NavigationClass navigationClass; VoxelPosition startCellVoxels; std::int32_t tileX; std::int32_t tileZ; };
using Query = std::variant<AimCheck, SectionIntegrity, TileReach>;
struct QueryRequest { std::uint32_t id; Query query; };
inline constexpr std::uint16_t QUERY_RECORD_VERSION = 1;
void Encode(const QueryRequest& _request, Core::ByteWriter& _writer);
[[nodiscard]] QueryRequest DecodeQueryRequest(Core::ByteReader& _reader); // throws as DecodeCommand does

enum class AimVerdict : std::uint8_t { InRange, Blocked, TooClose, OutOfRange }; // Hud.md §8
struct AimReading
{
  AimVerdict verdict;
  std::int32_t distanceMeters;     // across the ground, floored
  bool strikes;                    // the arc meets a solid voxel before leaving the world
  VoxelPosition strikeVoxels;      // the first solid voxel on the shot's own arc
};
enum class GroundKind : std::uint8_t { Earth, Rock, Bedrock };
enum class OpeningFloor : std::uint8_t { Ground, Rubble, Stone };
struct IntegrityReading // the paper test's support readout (Review/PaperTests/BreachPrediction.md, *The readouts*), per section
{
  EntityId structure;
  GroundKind ground;                // earth if any column under the lowest course is earth, else rock, else bedrock
  bool canBeUndermined;             // the resolver's: a column stands on earth, with no deep foundation (M6)
  std::uint8_t holeWidthVoxels;     // the widest opening through the whole thickness below the battlements; 0 for none
  std::uint8_t holeHeightVoxels;
  std::uint8_t holeFallWidthVoxels; // the width at which the resolver drops the stone above it (B5); 0 when none does within the section
  bool hasOpening;
  std::uint8_t openingFloorVoxels;  // the lowest opening's floor above the section's ground
  OpeningFloor openingFloor;
  std::uint8_t openingClearVoxels;
  bool routeNow;                    // navigation lets foot stand on the opening's floor
  std::uint8_t gapRunVoxels;        // the longest run of columns with nothing under the lowest course across the whole thickness
  std::uint8_t gapFallRunVoxels;    // the run at which the resolver drops the stone above
  bool falling;                     // a collapse of this structure is in warning or falling
  VoxelPosition weakMinVoxels;      // the weak point's box, the reading nearest its mark, both corners included
  VoxelPosition weakMaxVoxels;
};
struct TileReachReading
{
  std::int32_t tileX;
  std::int32_t tileZ;
  std::array<std::uint8_t, 128> usable;  // bit z * 32 + x: the class can enter the cell
  std::array<std::uint8_t, 128> reached; // and can reach it from the start
};
using Reading = std::variant<AimReading, IntegrityReading, TileReachReading>;
struct Answer { std::uint32_t queryId; std::uint32_t tick; Reading reading; };
void Encode(const Answer& _answer, Core::ByteWriter& _writer);
[[nodiscard]] Answer DecodeAnswer(Core::ByteReader& _reader);
```

### `Simulation` (B1–B7)

```cpp
// Simulation/World.h gains:
struct Structure // ADR-005, decision 10, without the built mask, which M4's construction adds as record version 2
{
  Protocol::EntityId id;
  std::uint8_t owner;
  std::string templateId;
  Protocol::VoxelPosition originVoxels; // the footprint's minimum corner after the turn
  std::uint32_t quarterTurns;           // clockwise seen from above, as VoxelPass::Batch counts them
  std::string name;                     // the scenario's, such as W2 (Hud.md §10)
};
// SiegeEngine gains std::uint32_t quarterTurns, which AddSiegeEngine takes; Unit gains Protocol::UnitKind kind, which AddSquad takes;
// Collapse gains Protocol::EntityId structure: the structure whose footprint holds the piece's first voxel, or NO_ENTITY.
class World
{
public:
  /// Stamps _templateId from the server data at _originVoxels, turned, by edits between ticks, as the
  /// breach run did by hand, and records the structure. Throws std::invalid_argument for a template
  /// the data lacks. Structures are saved (record version 1) and hashed.
  Protocol::EntityId AddStructure(std::uint8_t _owner, std::string_view _templateId, Protocol::VoxelPosition _originVoxels,
                                  std::uint32_t _quarterTurns, std::string_view _name);
  [[nodiscard]] const std::map<Protocol::EntityId, Structure>& Structures() const noexcept;
  /// What step 11 published at the end of the last tick, filtered for the one player M3 has.
  [[nodiscard]] const Protocol::StateUpdate& State() const noexcept;
  /// Queues a query's bytes from _player for step 12 (ADR-009; ADR-020, decision 6). Past
  /// MAX_PENDING_QUERIES the oldest pending query is dropped.
  void Ask(std::uint8_t _player, std::span<const std::byte> _query);
  /// The answers produced since the last call, encoded, in the order answered.
  [[nodiscard]] std::vector<std::vector<std::byte>> TakeAnswers();
  /// Nothing in flight: no shot, no collapse, no slump or search pending, no edit unsearched, and
  /// every unit standing still. What the lab's fast-forward stops on (M3-B7).
  [[nodiscard]] bool Quiet() const noexcept;
  static constexpr std::size_t MAX_PENDING_QUERIES = 64;
};

// Simulation/Rules.h gains, from RULE (input 10):
std::int32_t queryWorkPerTick; // units of work step 12 spends: an aim check 1, a tile's reach 1, a section's integrity 8

// Simulation/Shot.h gains:
/// One tick of a shot's flight: the first solid voxel the step from _ticksFlown crosses, if any, and,
/// missing, whether it has left a world _widthVoxels by _depthVoxels, below the ground or past an edge.
struct ShotStep { std::optional<Protocol::VoxelPosition> hit; bool left; };
[[nodiscard]] ShotStep StepShot(const ShotArc& _arc, std::uint32_t _ticksFlown, const VoxelWorld& _world, std::int32_t _widthVoxels,
                                std::int32_t _depthVoxels);
/// The first solid voxel a shot on _arc meets, stepping as the shot flies, or nothing when it leaves
/// the world. The aim check and the shot's own flight share StepShot, so they cannot disagree.
[[nodiscard]] std::optional<Protocol::VoxelPosition> TraceShot(const ShotArc& _arc, const VoxelWorld& _world, std::int32_t _widthVoxels,
                                                               std::int32_t _depthVoxels);

// Simulation/Integrity.h — B5. Pure: reads the world, changes nothing, same inputs same reading.
[[nodiscard]] Protocol::IntegrityReading ReadIntegrity(const World& _world, Protocol::EntityId _structure); // throws std::out_of_range for an unknown structure

// Simulation/NavigationGrid.h gains:
struct TileReach { std::bitset<TILE_EDGE_CELLS * TILE_EDGE_CELLS> usable; std::bitset<TILE_EDGE_CELLS * TILE_EDGE_CELLS> reached; };
/// Which of a tile's cells a class can enter, and which it can reach from _start, from the per-tile
/// components (M2-F4). Current after UpdateReach; throws std::logic_error otherwise.
[[nodiscard]] TileReach ReachInTile(std::int64_t _start, std::int32_t _tileX, std::int32_t _tileZ, NavigationClass _class) const;

// Simulation/BreachLab.h — B7 (input 2). Built in C++ as the breach run is, so the tests, the bench and
// Earthrise run the same thing.
struct LabItem
{
  std::string id;                                      // P0 the practice item, then L01 to L16
  std::vector<Protocol::VoxelPosition> priorAimVoxels; // shots before the question, each fired on the first tick the trebuchet is loaded
  Protocol::VoxelPosition markVoxels;                  // where the asked shots are aimed
  std::uint32_t shotsAsked;                            // N, 1 to 5
};
struct LabSetup { Protocol::EntityId engine; Protocol::EntityId squad; Protocol::EntityId target; Protocol::VoxelPosition squadStartVoxels; Protocol::VoxelPosition goalVoxels; };
struct LabOutcome { bool open; std::uint32_t ticks; std::uint64_t stateHash; bool timedOut; };
inline constexpr std::uint32_t LAB_MAX_TICKS_PER_PHASE = 4000;
inline constexpr std::uint32_t LAB_ORDER_TICK = 1; // the squad's order, once tick 0's rebuild has put the enclosure into navigation
[[nodiscard]] std::span<const LabItem> LabItems();                              // P0 first, then L01 to L16
[[nodiscard]] std::uint64_t LabItemsHash();                                     // Fnv1a64 over every item's fields: names the item set in the logs
[[nodiscard]] World::Desc LabDesc(const ServerData& _data, std::uint64_t _buildId); // the breach run's map and seeds
/// The breach run's site on a world not yet ticked: the footprint levelled, the enclosure as structures
/// W1 to W10 anticlockwise seen from above from the south-west corner, south side first: W1 to W3 the south side
/// west to east, W4 and W5 the east side south to north, W6 to W8 the north side east to west, and W9 and W10 the
/// west side north to south, with W2, the middle south section, the target; the trebuchet 40 m south of W2 facing it, and six men-at-arms 10 m south, ordered into the courtyard
/// on tick 1.
[[nodiscard]] LabSetup BuildLabSite(World& _world);
/// Fires shots from the site's trebuchet, one on the first tick it is loaded, then waits for the world
/// to go quiet with the trebuchet loaded. Driven a tick at a time, so that Earthrise draws between
/// ticks and the headless run reaches the same ticks. Whichever LabShots ticks the world through
/// LAB_ORDER_TICK also gives the site's squad its order, as the breach run's script does.
class LabShots
{
public:
  LabShots(const LabSetup& _setup, std::span<const Protocol::VoxelPosition> _aimVoxels);
  /// Submits what this tick needs and ticks _world once. Returns Done().
  bool Advance(World& _world);
  [[nodiscard]] bool Done() const noexcept;     // every shot fired, and the world quiet with the trebuchet loaded, or LAB_MAX_TICKS_PER_PHASE passed
  [[nodiscard]] bool TimedOut() const noexcept;
};
/// Navigation's answer: foot can reach the goal from the squad's start (ADR-015, *Consequences*).
[[nodiscard]] bool CourtyardOpen(const World& _world, const LabSetup& _setup);
/// The headless run of an item on a built site: the prior shots, then the asked shots, then the verdict and the state hash.
[[nodiscard]] LabOutcome ResolveLabItem(World& _world, const LabSetup& _setup, const LabItem& _item);
```

### `Content` (B8)

```cpp
// Content/Font.h — ADR-019, decision 1. Baked from GameData/Client/Fonts/EarthriseMason-glyphs.json.
struct Glyph
{
  char32_t codePoint;
  std::uint8_t widthPixels;  // font pixels
  std::uint8_t heightPixels; // the rows it has
  std::uint8_t topRow;       // its first row in the FONT_CELL_ROWS cell
  std::uint8_t advancePixels;
  bool icon;
  std::span<const std::uint16_t> rows; // bit x set for a pixel at column x
};
inline constexpr std::uint32_t FONT_CELL_ROWS = 10;
inline constexpr std::uint32_t FONT_BASELINE_ROW = 7;
inline constexpr std::uint32_t FONT_GLYPH_COUNT = 127;

// Content/DebrisChoice.h — input 16. Baked from GameData/Manifest.json's debrisByRubble (MvpAssets.md, *Poses, projectiles,
// gates and debris*).
struct DebrisChoice
{
  std::uint8_t rubbleIndex;                 // the palette index of the rubble it lies on
  std::span<const std::string_view> models; // the models scattered over it, each in Models()
  std::uint8_t tintIndex;                   // every voxel of a piece drawn in this index, or 0 to keep the model's own
};
[[nodiscard]] std::span<const DebrisChoice> DebrisChoices() noexcept; // ascending rubble index
[[nodiscard]] const DebrisChoice* FindDebrisChoice(std::uint8_t _rubbleIndex) noexcept;
[[nodiscard]] std::span<const Glyph> Glyphs() noexcept; // ascending code point
[[nodiscard]] const Glyph* FindGlyph(char32_t _codePoint) noexcept;
```

### `Engine` (D1, D2, E3)

```cpp
// Engine/InterfacePass.h — ADR-019, decision 3. Shaders InterfaceVS.hlsl and InterfacePS.hlsl.
struct InterfaceQuad
{
  float xPixels, yPixels, widthPixels, heightPixels; // top-left origin, in the target's pixels
  float u0, v0, u1, v1;                              // the atlas rectangle, in texels; unused when solid
  DirectX::XMFLOAT4 color;                           // linear RGBA
  std::uint32_t textured;                            // 1: alpha is the atlas's red channel times color.a; 0: solid
  std::array<std::uint32_t, 3> reserved;
};
static_assert(sizeof(InterfaceQuad) == 64, "the shaders read a quad as 64 bytes");
class InterfacePass
{
public:
  struct Desc { DXGI_FORMAT targetFormat; };
  InterfacePass(ID3D12Device* _device, const Desc& _desc);
  /// Reads _atlas, R8_UNORM, point-sampled, from now on. The caller first makes sure the GPU no longer reads the old one.
  void SetAtlas(ID3D12Resource* _atlas, std::uint32_t _widthTexels, std::uint32_t _heightTexels);
  /// Draws _quadCount quads at _quads in order, blended over the target, no depth. The caller has set
  /// the target, viewport and scissor to the whole back buffer.
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _quads, std::uint32_t _quadCount,
            std::uint32_t _targetWidthPixels, std::uint32_t _targetHeightPixels) const;
};

// Engine/VoxelPass.h gains (ADR-021, decision 1):
/// Binds the overlay pipeline: records drawn flat in their palette colour at _alpha, blended over the
/// scene, tested against its depth with GREATER_EQUAL and writing none. Draw then draws batches as
/// after Begin.
void BeginOverlay(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants, D3D12_GPU_VIRTUAL_ADDRESS _palette,
                  float _alpha) const;

// Engine/VoxelPick.h — E3. CPU only; may disagree with the GPU at a box's edge (ADR-001, *Consequences*).
struct PickRay { DirectX::XMFLOAT3 originMeters; DirectX::XMFLOAT3 direction; }; // unit length
struct PickHit { std::array<std::int32_t, 3> cell; VoxelFace face; float distanceMeters; };
using SolidAt = std::function<bool(std::int32_t, std::int32_t, std::int32_t)>; // unnamed parameters, as SolidOutside's (R1)
/// The first solid cell the ray meets within _maxDistanceMeters, by the integer traversal of Amanatides and Woo in float, or nothing.
[[nodiscard]] std::optional<PickHit> PickVoxel(const PickRay& _ray, float _voxelEdgeMeters, float _maxDistanceMeters, const SolidAt& _solidAt);

// Engine/Window.h gains:
[[nodiscard]] std::uint32_t Dpi() const noexcept; // of the monitor the window is on, through GetDpiForWindow
```

### `Client` (C2, C3, E1, E2)

```cpp
// Client/Look.h — RendererLook.md's numbers, moved from the spike: the Golden preset, the shadow maps'
// sizes and biases, the camera's angles and zoom range, the model voxel's edge, and the palette entries
// above the materials': 240 attacker, 241 defender, 242 ford, and ADR-021's overlays, 243 good, 244 bad,
// 245 accent, 246 dim, 247 text, in Hud.md §2's colours; and the trebuchet's poses (input 16): S3c, cocked, placed at
// S3's placement moved -5 m along the model's own x before its turn, and S3, released, for RELEASED_POSE_TICKS = 20
// after a fire, both from MvpAssets.md, *Poses, projectiles, gates and debris*, as arithmetic on the files.

// Client/TickClock.h — ADR-020, decision 4.
class TickClock
{
public:
  static constexpr std::uint32_t TICKS_PER_SECOND = 20;
  static constexpr std::uint32_t MAX_TICKS_PER_FRAME = 8;
  /// Adds _seconds times _speed to the accumulator and returns the ticks due, at most
  /// MAX_TICKS_PER_FRAME; the rest waits. A paused clock takes no time and gives no ticks.
  [[nodiscard]] std::uint32_t Advance(double _seconds, double _speed);
  void SetPaused(bool _paused) noexcept;
  [[nodiscard]] bool Paused() const noexcept;
  /// The accumulator's part of the next tick, 0 to 1: where the shot is drawn between its last two positions (input 17).
  [[nodiscard]] double TickFraction() const noexcept;
};

// Client/Session.h — ADR-020, decision 2: the client's copy of what it was told, and what it asks.
class Session
{
public:
  /// Takes a tick's state update, the chunks published and the answers, by copy.
  void Receive(const Protocol::StateUpdate& _state, std::span<const Protocol::ChunkSnapshot> _snapshots,
               std::span<const std::vector<std::byte>> _answers);
  [[nodiscard]] const Protocol::StateUpdate& State() const noexcept;
  /// The update before the last, for the shot's interpolation (input 17); empty before the second.
  [[nodiscard]] const Protocol::StateUpdate& PreviousState() const noexcept;
  /// The chunks received since the last call, oldest first, for the world view to take.
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> TakeSnapshots();
  /// The commands and queries the client has queued since the last call, as bytes, for the executable to hand the simulation.
  [[nodiscard]] std::vector<std::vector<std::byte>> TakeCommands();
  [[nodiscard]] std::vector<std::vector<std::byte>> TakeQueries();
  /// Queues one. A query is keyed so that the newest answer replaces the last (ADR-009, decision 3).
  void Submit(const Protocol::Command& _command, std::string_view _source); // _source is "key" or "click", for the log (Hud.md §11)
  void Ask(const Protocol::Query& _query);
  [[nodiscard]] const Protocol::AimReading* Aim(Protocol::EntityId _engine, Protocol::VoxelPosition _target) const noexcept;
  [[nodiscard]] const Protocol::IntegrityReading* Integrity(Protocol::EntityId _structure) const noexcept;
  [[nodiscard]] const Protocol::TileReachReading* Reach(std::int32_t _tileX, std::int32_t _tileZ) const noexcept;
  /// The sources of the commands TakeCommands last gave out, in the same order, for the lab's log (G1).
  [[nodiscard]] const std::vector<std::string>& TakenSources() const noexcept;
};

// Client/HudModel.h — Hud.md §10, the part M3 builds. Plain fields, built each frame from the Session.
struct HudResource { std::string key; std::string value; bool warn; };
struct HudObjective { std::string title; std::string detail; };
struct HudMode { std::string command; std::string banner; }; // empty command when none
struct HudAlert { std::string text; std::string sub; std::optional<Protocol::VoxelPosition> location; bool urgent; double expiresSeconds; };
struct HudModel
{
  std::vector<HudResource> resources;
  HudObjective objective;
  HudMode mode;
  std::vector<HudAlert> alerts; // newest first, at most 3
  Protocol::EntityId selectedEngine; // NO_ENTITY when none (M4 brings squads)
  bool integrityView;   // F1, on by default
  bool passabilityView; // F2
};
```

### The lab's log (G1)

One CSV a run, `Logs\BreachLab-<participant>-<yyyymmdd-hhmmss>.csv` beside the executable (input 13), LF-terminated, UTF-8. Comment lines first, each `# key: value`: participant, build id, server-data hash, item-set hash (`LabItemsHash`), display size, UI scale, start time. Then a header row and rows of two kinds:
- **An item row:** `item,<id>,<n>,<key>,<answer>,<correct>,<secondsToAnswer>,<integrityOn>,<passabilityOn>,<cameraTurns>,<zoomNotches>,<liveHash>,<headlessHash>,<matched>`, where the key and the answer are `open` or `shut`, the flags `0` or `1`, and the hashes 16 hexadecimal digits.
- **A command row:** `command,<tick>,<seconds>,<command>,<source>`: every command the player issued in free fire, with `key` or `click` (Hud.md §11).

## Phase A — Decisions

### M3-A1 — Write down the owner's decisions
- **Waits on:** the owner's answers to MvpPlan.md, M3, inputs 1–17. Paper only.
- **Read first:** MvpPlan.md, M3, *The owner's inputs*; GameDesign.md, *Closing M2* and *Open questions*.
- **Changes:**
  - **GameDesign.md** gains `### <date>: M3's values`, with each input as Decided in the owner's words, or as the owner changed it. Input 10 stays Proposed, because G3 measures it. The Open questions *Craters at 1 m* and *The HUD's sizes* are rewritten as the owner decides them (inputs 11 and 8).
  - **MvpPlan.md:** each input's row says Decided, and input 1 records the go-ahead for each ADR.
  - **The ADRs** are edited where the owner changed them, and stay Proposed (AGENTS.md §6). ADR-007 gains its amendment on mouse messages (input 15) and ADR-010 the note that its decision 2 is amended by ADR-020 on acceptance.
  - **Hud.md:** its Proposed items that the inputs decide, the DPI bands and the camera framing, are marked Decided or changed.
- **Tests:** none. The checkers run, and every relative link resolves.

## Phase B — The simulation's side

### M3-B1 — Structures as entities
- **Waits on:** A1.
- **Read first:** ADR-005, decision 10; Hud.md §10, *Section names*; `Simulation/BreachRun.cpp`'s `PlaceSection`; *Contracts*, `Simulation`.
- **Changes:**
  - `Structure`, `AddStructure` and `Structures` in `World`, in the filter `Tick`; `PlaceSection` becomes `World`'s stamping, turned clockwise as the engine turns a batch, and the breach run raises its enclosure through `AddStructure`, as W1 to W10 in the order B7's contract names them, W2 the target, with the same voxels as before.
  - `Collapse::structure`, set in step 5 from the piece's first voxel.
  - `AddSiegeEngine` takes the engine's facing; the breach run's trebuchet faces north, towards the wall.
  - Structures in the save and the state hash (record version 1), and in `SaveTests`.
- **Tests:** **`StructureTests`:**
  - A T3 placed at each of the four turns stamps the voxels `PlaceSection` stamped, turn for turn, with the convention's change accounted for.
  - An unknown template throws.
  - The breach run's collapse names W2.
  - A save with structures round-trips, and `BreachRunTests` pass unchanged: the same craters, the same slot, the same ticks.

### M3-B2 — State updates as values
- **Waits on:** B1.
- **Read first:** ADR-002, decision 3; ADR-020, decision 3; *Contracts*, `Protocol`.
- **Changes:**
  - **`Protocol`:** `FixedPosition.h` and `EntityKinds.h` moved in, and `PlacedVoxel` with them, `StateUpdate.h` new, in the filter `State`. `Simulation` and `RendererSpike` keep compiling through using-declarations and include changes only.
  - **`Unit::kind`,** which `AddSquad` takes; the breach run's squad is men-at-arms.
  - **`World::PublishState`** fills the update from the entities, through `PublishState(player)` with the identity filter M3 has; `World::State` returns it.
- **Tests:** **`StateUpdateTests`:**
  - On the breach run, every structure, engine, squad, unit, shot and collapse appears in its tick's update, in id order, and nothing that no longer exists does.
  - A collapse's view carries its piece, strip and landing from its first warning tick, the landing equal to its `CollapsePlan`'s; `fallen` empty and `landedVoxels` 0 until the fall's first tick, then `landedVoxels` growing to the landing's size on the fall's last tick; and `warningTicksLeft` counting down to 0.
  - The tick's hits match `LastTickHits`.
  - A unit's position in the update equals the unit's.
- **ADR:** ADR-020, decision 3, implemented; the ADR stays Proposed until C1.

### M3-B3 — Queries within the tick's budget
- **Waits on:** B2, and input 10.
- **Read first:** ADR-009; ADR-020, decisions 5 and 6; ADR-013, decision 6; `Protocol/Command.cpp`; `Simulation/ServerData.cpp`'s `RULE_FIELDS`; `Tools/CheckAssets.py`, check 10; *Contracts*, `Protocol` and `Simulation`.
- **Changes:**
  - **`Protocol/Query.h` and `.cpp`,** in the filter `Queries`: the three queries, the three readings, `Encode` and `Decode` for requests and answers.
  - **`World::Ask`, `TakeAnswers`, `AnswerQueries`** (step 12) and `MAX_PENDING_QUERIES`; the three kinds dispatch to B4, B5 and B6, which this order lands as stubs that return empty readings, so that the budget and the plumbing are tested here and each reading in its own order. (Written first as stubs that throw `std::logic_error`, which the budget tests could not drive; corrected by B3.)
  - **`queryWorkPerTick`** in `GameData/Server/Rules.json` at 16, `Rules.h`, `RULE_FIELDS`, and `Tools/TestBakeContent.py`'s RULE test, which reads the key back.
  - **`World::Quiet`.**
- **Tests:**
  - **`QueryTests`** (`ProtocolTests`): each query and each answer round-trips; a known encoding byte for byte for `AimCheck{3, {1, 2, 3}}` with id 7; an unknown tag or version throws `std::invalid_argument`, truncated bytes `std::out_of_range`.
  - **`QueryBudgetTests`** (`SimulationTests`), on a `FlatWorld` with a stub reading of cost 1 and one of cost 8:
    - A query asked before tick n is answered at the end of tick n, carrying n and its id.
    - With a budget of 16, 17 unit queries asked at once take two ticks, oldest first, and their answers come in asking order.
    - A query whose cost exceeds what is left waits for the next tick and is not dropped.
    - The 65th pending query drops the oldest.
    - A malformed query throws in step 12.
    - `Quiet` is false during a shot, a warning, a fall, a slump and a walk on the breach run, and true once it is over.
  - **`ServerDataTests`:** the real file reads with the new key, and a file lacking it is refused by name.

### M3-B4 — The aim check
- **Waits on:** B3.
- **Read first:** Hud.md §8, *The attack cursor*; ADR-020, decision 5; `Simulation/Shot.cpp`.
- **Changes:** `TraceShot`, factored from the shot's flight so that both use it, in the filter `Destruction`; the aim check answered: TOO CLOSE and OUT OF RANGE from `AimShot`'s range test, otherwise the arc traced to its strike, IN RANGE when the strike is the target or one of its six face neighbours and BLOCKED otherwise; the distance across the ground, floored.
- **Tests:** **`AimCheckTests`:**
  - On the breach run before any shot, the five scripted aim points read IN RANGE, each with a strike one voxel above its aim point, as the M2 report describes.
  - An aim point 10 m from the trebuchet reads TOO CLOSE, 60 m OUT OF RANGE, with their distances.
  - An aim point behind the wall reads BLOCKED, with the wall's face as the strike.
  - For every one of the five, the strike equals the voxel `ShotTests`' shot hits when fired.
  - `ShotTests` pass unchanged.

### M3-B5 — The integrity reading
- **Waits on:** B3, and input 5.
- **Read first:** GameDescription.md, *Readability*; Review/PaperTests/BreachPrediction.md, *The readouts*, *The 16 states* and *Ambiguities*, items 1, 3 and 5; `Tools/PaperTests/BreachPrediction.py`; ADR-004, decisions 2 and 5; ADR-021, decision 4; *Contracts*, `Simulation`.
- **Changes:** `Simulation/Integrity.h` and `.cpp`, in the filter `Destruction`, and the section-integrity query answered from it.
  - **The three readings** follow the paper test's definitions, over the structure's own footprint box: the widest opening through the whole thickness below the battlements, the lowest opening with its floor and clear height, and the longest gap under the lowest course across the whole thickness.
  - **The fall width and the fall run come from the resolver** (input 5): on a scratch copy of the chunks the section touches, the hole is widened one column at a time, alternately each side, and `FindUnsupported` is run after each, until it finds a piece or the section's width is reached; the gap under the foundation is lengthened the same way. The scratch copy is dropped; the world is unchanged.
  - **The ground** reads the voxel under each column of the lowest course; **routeNow** asks navigation whether foot can enter the opening's floor cells; **the weak point** is the reading nearest its mark, as the paper test defines it; **falling** reads the collapses.
  - **`SimulationBench --integrity`** times the reading on the breach run's W2 before and after each shot, and reports the support searches it ran.
- **Tests:** **`IntegrityTests`,** on a `FlatWorld` with a T3 structure and one column of each neighbour, as the paper test's model stands:
  - **The 16 states** W01 to W16 of the paper test, built by edits from its table: each gives the paper's hole width and verdict, opening and gap, and the resolver drops exactly the voxels its *Fell* column counts, so the paper's key holds in 16 of 16. If the code disagrees with the paper on a state, the report says which and why, and the paper test's report is corrected in the same commit.
  - The fall width of a 4 m hole bridged at both ends is 5; with one bridging pillar cut away (the paper's ambiguity 3), it is 4.
  - A section on bedrock reads `Bedrock` and cannot be undermined; on earth it can.
  - `ReadIntegrity` changes nothing and returns the same reading twice.
- **ADR:** ADR-004, amended: the integrity reading is a caller of the resolver on a scratch copy, which decision 5 allows and did not name.

### M3-B6 — Reach in a tile
- **Waits on:** B3, and input 7.
- **Read first:** ADR-015, M2-F4; `Simulation/NavigationGrid.cpp`'s components; *Contracts*, `Simulation`.
- **Changes:** `NavigationGrid::ReachInTile` from the components, in the filter `Navigation`, and the tile-reach query answered from it with the class's start cell.
- **Tests:** **`TileReachTests`:**
  - Before the breach, the courtyard's cells read usable and not reached from the squad's start; after it, the courtyard reads reached and the route of boxes crosses W2's footprint.
  - For every cell of three tiles on the breach run, `reached` agrees with `NearestReachable(start, cell)` returning the cell.
  - A tile off the grid throws `std::out_of_range`.

### M3-B7 — The lab's items
- **Waits on:** B1, B3 and B6, and inputs 2 and 4.
- **Read first:** MvpPlan.md, M3, *Done when* and *Risks*; GameDesign.md, *Hypotheses*, H2; the M2 report, *What a breach looks like*; `Simulation/BreachRun.cpp`; *Contracts*, `Simulation`.
- **Changes:** `Simulation/BreachLab.h` and `.cpp`, in the filter `World`: `LabItems`, `LabDesc`, `BuildLabSite`, `LabShots`, `CourtyardOpen` and `ResolveLabItem`. `BuildBreachRun` keeps its site through the same code, so M2's tests still pass.
  - **The items** are the implementer's: the practice item P0, an intact wall and two shots at the ground row, which stays shut (they are aimed at the earth at the wall's foot, since a shot strikes the face a voxel above its aim, and two aimed a row higher at one mark open a doorway 1 m wide and 2 m high); then L01 to L16, varying the aim row, the columns, the prior damage and N, so that the set splits eight open and eight shut and no reading of the set is trivial (a shot at the crenellation alone, or five adjacent shots at the ground row alone). Each item's key is the simulation's, never typed by hand.
  - **`SimulationBench --lab-items`** runs every item headless and prints its id, key, ticks, state hash and the slowest tick.
- **Tests:** **`BreachLabTests`:**
  - The 16 items split 8 and 8. Their keys are committed as a table and the test fails on any change, with the new keys printed, as the generator's golden hashes do; a changed rule is a changed item set.
  - For every item, the squad reaches the goal if and only if the key is open.
  - Every item ends quiet within `LAB_MAX_TICKS_PER_PHASE` of each phase, and none times out.
  - An item resolved twice gives the same hash, and resolved from a save taken between its two halves gives the same key and hash.

### M3-B8 — The font's and the debris table's bake
- **Waits on:** A1, for ADR-019's go-ahead and input 16.
- **Read first:** ADR-019, decision 1; Hud.md §9; MvpAssets.md, *Poses, projectiles, gates and debris*; `GameData/Manifest.json`'s `debrisByRubble`; `Tools/BakeContent.py`'s client half; `Content/Palette.cpp`, which reads a generated table; *Contracts*, `Content`.
- **Changes:**
  - **`Tools/BakeContent.py --client`** writes `FontTable.h` from the glyph JSON, and `DebrisTable.h` from `Manifest.json`'s `debrisByRubble`, each tint as the palette index of that colour; `Content.vcxproj`'s bake inputs and outputs name both sources and both headers.
  - **`Content/Font.h` and `.cpp`,** in the filter `Fonts`; **`Content/DebrisChoice.h` and `.cpp`,** in the filter `Catalog`, beside the models, named for their type so that `Client`'s `Debris.h` (C2) does not hide them on the include path.
  - **`Tools/CheckAssets.py`** gains check 13, after main's check 12 for line models: the glyph file holds 127 glyphs, each row as wide as its glyph says, no code point twice, every row made of `.` and `#`, and the 15 symbols and 25 icons of Hud.md §9 present. It gains check 14 too: each `debrisByRubble` row names a palette index whose behaviour is a rubble, models that are in `GameData/Client/Models/`, and a tint, if any, that is exactly one palette entry's colour.
  - **`Tools/TestCheckAssets.py`** and **`Tools/TestBakeContent.py`** gain the font and the table: a broken glyph fails, a row naming a model that does not exist fails, the baked rows of `1` are its bitmap, and rock rubble's row bakes D1–D3 with tint index 28.
- **Tests:** **`DebrisChoiceTests`** (`ContentTests`): four choices in ascending rubble index, 8, 25, 26 and 28; 25 gives D4 and D5 with no tint; every model named is in `Models()`; `FindDebrisChoice(6)` is null. **`FontTests`** (`ContentTests`): 127 glyphs in ascending code point; `FindGlyph(U'1')`'s rows are the seven known masks, with `topRow` 1 and `advancePixels` 6; `FindGlyph(U'$')` is null, since the font lacks it; every icon code point U+E000 to U+E018 is present and 9 wide.

## Phase C — The client and the executable join

### M3-C1 — `Client`, `ClientTests` and `Earthrise`
- **Waits on:** A1, for ADR-020's go-ahead.
- **Read first:** ADR-010, decisions 2 and 4; ADR-020, decision 1; `RendererSpike/RendererSpike.vcxproj` and its manifest; `Build/CheckProjectFiles.py`; `Build/TestCheckers.py`; AGENTS.md §2 and §6.
- **Changes:**
  - **Three projects,** each flat, with `pch.h`, in `Earthrise.slnx` and `.clang-tidy`'s registry: `Client`, a static library on `Core`, `Engine`, `Protocol` and `Content`, with its include directories; `ClientTests`, with `SuiteSmoke`; `Earthrise`, a Windows executable on every library, with a manifest for per-monitor DPI, whose `wWinMain` opens the window full screen, makes the device and the swap chain, presents a cleared frame a second until closed, and shows an error box as the spike does. `--windowed` as the spike.
  - **The layering check:** `Build/CheckProjectFiles.py` gains the table of which project may reference which, from MvpPlan.md's *Solution layout*, and checks every `ProjectReference` and `AdditionalIncludeDirectories` against it; a library never references a test suite. `Build/TestCheckers.py` gains the broken variant "a library referencing a test suite".
  - **ADR-010** is amended: `Client` and `Earthrise` join in M3, `RendererSpike` references `Client` (C2), and the checker holds the layering table. **ADR-020** is marked Accepted. **AGENTS.md** §2's list of what the tree holds counts fifteen projects.
- **Tests:** `SuiteSmoke` in `ClientTests`; `Build/TestCheckers.py` passes the repository and fails the new variant for its reason. CI builds and runs every suite.

### M3-C2 — The world view moves into `Client`, and the debris becomes models
- **Waits on:** C1, B2 and B8, and input 16.
- **Read first:** `RendererSpike/WorldView.h`, `Debris.h`, `SceneScript.h`'s `ModelPlacement`, and the constants at the top of `RendererSpike.cpp`; ADR-020, decision 1; MvpAssets.md, *Poses, projectiles, gates and debris*; GameDesign.md, *M1's open values*, item 7.
- **Changes:**
  - `WorldView`, `Debris` and `ModelPlacement` move to `Client`, in the filter `World`, unchanged but for their namespace and for `Debris`, below. `Client/Look.h` takes the spike's look constants and palette entries, in the filter `Look`.
  - **`Debris` scatters the v3 package's models** (input 16): `Scatter` takes the collapse's id, its fallen voxels and its landing as `Protocol::PlacedVoxel`s. Over each landing column's top voxel, `Content::FindDebrisChoice` gives the models; one piece is placed per about 4 m² of that rubble's top, by keyed draws on the collapse's id (`Core::KeyedDraw`), with its model, a quarter turn of yaw and an offset of 0, 0.25 or 0.5 m within its cell, as MvpAssets.md says. Each piece starts at a fallen voxel chosen by the same draws, falls and turns for `FALL_SECONDS`, and rests on its cell's top at its yaw. It is drawn as its model's voxels, each an `Engine::OrientedVoxel` sharing the piece's turn, through the oriented pass, as M1's single voxels were; over rock rubble every voxel takes the choice's tint index.
  - `RendererSpike` references `Client`, includes these from it, and deletes its copies. It builds its collapse's landing from the rubble cells its scene script records and the store's indices there. Its loop, its measurement and its scene script otherwise stay as they are.
- **Tests:** the existing `EngineTests` and the spike's build. `ClientTests` gains `DebrisTests`, which need no GPU: the same id gives the same pieces; a landing of 40 stone-rubble columns gives about 10 pieces, each one of D1–D3, each resting on a landing column at a whole quarter turn; over rock rubble every voxel is index 28; over earth rubble every piece is D6; nothing rests outside the landing.
- **Owner runs:** the spike with `--measure --windowed`: the same picture but for the debris, which is now D1–D6 models tumbling and coming to rest on the breach's and the collapse's rubble, and the voxel pass's 95th percentile within the run-to-run spread of the M1 and M2 figures.

### M3-C3 — The breach run on screen
- **Waits on:** C2 and B3, and inputs 4, 12, 14, 16 and 17.
- **Read first:** ADR-020, decisions 2 and 4; ADR-016, decision 7; MvpAssets.md, *Poses, projectiles, gates and debris*; ADR-014, decision 6; `RendererSpike/RendererSpike.cpp`'s `Run`, `Steer` and `MakeFrame`; `SimulationBench/SimulationBench.cpp`'s `BuildIdOf`; RendererLook.md §7; Hud.md §3, *The camera frames the uncovered area*, and §7's global keys; *Contracts*, `Client`.
- **Changes:**
  - **`Client::TickClock`, `Client::Session`, `Client::Camera`** (Hud.md's keys, moved from the spike's `Steer`, with the look-at point offset below the top bar's 44 logical px) and **`Client::WorldRenderer`** (the spike's frame: the two shadow maps, the voxel pass, the debris, the upscale, and the GPU timer, so that the title shows the voxel pass's time), in the filters `Time`, `Session`, `Camera` and `Rendering`.
  - **ADR-016,** decision 7, is amended: `Earthrise` compares the two shared hashes from M3, not M4 (ADR-020, decision 7).
  - **`Earthrise`** loads `ServerData` from beside itself, computes its build id, refuses a shared-hash mismatch, builds the lab's site through `BuildLabSite` (B7) or, until B7 lands, the breach run, and runs it live: each frame the clock's ticks, each tick the session's command and query bytes handed over, the world ticked, and its state, snapshots and answers handed back. The breach run's script fires as scripted; the trebuchet draws at its cell facing the wall as S3c, cocked, and as S3, released, for `RELEASED_POSE_TICKS` after its `reloadTicks` rises, which is when it fired, each placed as `Client/Look.h` says (input 16); the squad as six U5; a shot as X1, centred on its position, drawn between its positions in the session's previous and last updates at the clock's `TickFraction`, or at its last when it has none before (input 17); the collapse with its debris scattered on the fall's first tick from the collapse view's fallen voxels and landing. The camera starts at the default azimuth and view height on W2. `--speed 10` runs the clock at ten times, for the owner to see the whole run quickly.
- **Tests:** **`TickClockTests`:** 1/60 s a frame gives a tick every third frame on average and 20 a second; a 1 s frame gives 8 and keeps 12 for later; paused gives none; speed 10 gives ten times the ticks; `TickFraction` is 0.5 halfway to the next tick, and stays below 1. **`SessionTests`:** a received update replaces the last and becomes `PreviousState` when the next arrives; commands and queries come out as the bytes `Encode` writes; the newest answer per key replaces the last.
- **Owner runs:** `Earthrise --windowed`, and with `--speed 10`: the enclosure, the trebuchet cocked and released at each throw without a jump in its frame, and the men-at-arms; five shots that fly smoothly as X1 and leave craters where the M2 report says; the slot's warning and fall with debris; the squad walking through. The voxel pass's time in the title.

## Phase D — `Engine`: text and overlays

### M3-D1 — The interface pass
- **Waits on:** A1, for ADR-019's go-ahead.
- **Read first:** ADR-019, decision 3; ADR-011, decisions 2 and 3; ADR-012; `Engine/UpscalePass.h` and its shaders; `EngineTests/UpscalePassTests.cpp`; *Contracts*, `Engine`.
- **Changes:** `Engine/InterfacePass.h` and `.cpp`, `Engine/Shader/InterfaceVS.hlsl` and `InterfacePS.hlsl`, in the filter `Interface`; `Engine::Window::Dpi`.
- **Tests:** **`InterfacePassTests`** on WARP, into a 64 × 64 target cleared to a known colour, with a 4 × 4 atlas of two set and two clear texels:
  - A textured quad 8 × 8 over the atlas covers exactly the pixels of its set texels at scale 2, in its colour, and leaves the clear texels' pixels at the clear colour.
  - A solid quad at 0.92 alpha blends to the expected linear colour within 2 of 255.
  - Two overlapping quads draw in order: the later one on top.
  - Nothing outside any quad changes.
- **ADR:** marks ADR-019 Accepted.

### M3-D2 — The overlay pipeline
- **Waits on:** A1, for ADR-021's go-ahead.
- **Read first:** ADR-021, decision 1; `Engine/VoxelPass.cpp`'s pipeline states; `EngineTests/VoxelPassTests.cpp`; *Contracts*, `Engine`.
- **Changes:** `VoxelPass::BeginOverlay` and its pipeline state, with a blend state, `GREATER_EQUAL` and no depth write, through the existing shaders with a root constant for the alpha and the flat colour, in the filter `Rendering`.
- **Tests:** **`VoxelPassTests`** gains:
  - An overlay box on an opaque voxel blends the overlay's colour over the voxel's at every pixel the voxel covers, and nowhere else.
  - An overlay box behind an opaque voxel changes nothing.
  - An overlay box in the open blends over the clear colour, and the depth buffer is unchanged.
- **ADR:** marks ADR-021 Accepted.

## Phase E — The HUD's first pieces, and FIRE

### M3-E1 — Text on screen
- **Waits on:** B8, C3 and D1, and input 9.
- **Read first:** ADR-019, decisions 2, 4 and 5; Hud.md §2 and §9; *Contracts*, `Client`.
- **Changes:** `Client::FontAtlas` (the 160 × 80 texture from `Content::Glyphs`, uploaded once), `Client::TextLayout` (measure and lay out at a whole-pixel scale), `Client::UiScale` (the DPI bands, and `--ui-scale`), `Client::Strings` (every string the HUD can draw, as `inline constexpr std::string_view`s), in the filter `Text`; `WorldRenderer` gains the interface pass after the upscale; `Earthrise` draws the title's figures as text in the top-left corner, as a first use.
- **Tests:** **`TextLayoutTests`:** `W2` at scale 2 is 22 px wide (5 + 1 + 5 at 2 px, less the last advance's gap as the layout defines it); a string's quads land on whole pixels; the UI scale at 96, 120, 144 and 192 DPI is 1, 1, 1.5 and 2; **`StringsTests`:** every code point of every string in `Client/Strings.h` has a glyph.
- **Owner runs:** none of its own; E2's covers it.

### M3-E2 — The top bar, the mode banner, the camera buttons and the alerts
- **Waits on:** E1, and inputs 8 and 14.
- **Read first:** Hud.md §2, §3, §7 and §10; ADR-007, decision 4; *Contracts*, `Client`, `HudModel`.
- **Changes:** `Client::Hud` builds quads from the `HudModel` in the filter `Hud`: the top bar at 44 logical px with resources on the left (`TIME m:ss` of game time and `SHOTS n`), the objective in the centre, and the camera buttons ↺ ↻ − + on the right, drawn 36 × 36 with 44 × 44 hit targets (input 8), which turn and zoom the camera; the mode banner at top 56, centred, with CANCEL · ESC; the alerts at left 8, top 110, newest first, up to three, each expiring after its time. Clicks on the HUD never reach the world. The model is built each frame from the session.
- **Tests:** **`HudTests`:** the camera buttons' hit rectangles are 44 × 44 at scale 1 and 88 × 88 at 2; a click inside one is consumed and names its button; three alerts show and a fourth pushes the oldest out; every label the HUD draws fits its element at every scale, measured with the font's metrics.
- **Owner runs:** `Earthrise --windowed` at 1280 × 720 and at 1920 × 1080, and with `--ui-scale 1.5` and `2`: no overlap, no clipped text, names larger than labels at every step, the buttons working.

### M3-E3 — Picking and selection
- **Waits on:** C3, and input 15.
- **Read first:** ADR-001, *Consequences*, on the CPU copy of the ray cast; Hud.md §5, first paragraph, and §8; `Engine/OrthographicCamera.h`; *Contracts*, `Engine`, `VoxelPick`.
- **Changes:** `Engine::VoxelPick`, in the filter `Camera`; `Client::WorldPicker`, which builds the pixel's ray from the camera and picks over the session's snapshots, and tests the engines' footprints on the ground; a left click on the trebuchet's footprint selects it, Esc deselects, and the HUD's objective line names the selection.
- **Tests:** **`VoxelPickTests`** (`EngineTests`, no GPU): a ray straight down onto a 4 × 4 × 4 grid hits the top face of the expected cell at the expected distance; a ray at 34° through the grid hits the first solid cell on its way, as a hand-stepped traversal says; a ray through nothing returns nothing; a ray that starts inside a solid cell hits it. **`WorldPickerTests`:** a pixel over the trebuchet's footprint selects it, one over the wall picks the wall's voxel and face.

### M3-E4 — FIRE, the attack cursor and click-to-fire
- **Waits on:** E2, E3 and B4, and D2 for the strike mark.
- **Read first:** Hud.md §7, the trebuchet's card, §8, *Targeting modes* and *The attack cursor*; ADR-020, decision 5; ADR-021, decision 4; `Protocol/Command.h`.
- **Changes:** with a trebuchet selected, F enters FIRE: the banner `FIRE · RIGHT-CLICK A TARGET · CANCEL · ESC`, the crosshair, and the attack cursor, a ring with its label following the pointer, read from the aim check the session asks for the voxel under the pointer each frame it moves: `IN RANGE · d m`, `BLOCKED · d m`, `TOO CLOSE · d m` or `OUT OF RANGE · d m`, in good or bad, with the strike voxel marked through the overlay pipeline. A right click fires a `FireEngine` at the picked voxel, as bytes, and leaves the mode unless Shift is held; Esc cancels. A right click with the trebuchet selected and no mode fires too (Hud.md §8, *Order*). FIRE is disabled, in off text, while the trebuchet reloads. Every command is recorded with its source and time for G1's log.
- **Tests:** **`FireModeTests`:** F with a trebuchet selected enters the mode and sets the banner; a right click submits `FireEngine{engine, pickedVoxel}` and leaves the mode; with Shift it stays; Esc leaves it; without a selection F does nothing; the cursor's label follows the reading's verdict and distance.
- **Owner runs:** `Earthrise --windowed`: select the trebuchet, enter FIRE, move the cursor over the wall and read the four verdicts and the strike mark one voxel above the aim at the foot of the wall, fire five shots along W2's foot and see the slot fall, as the script did.

## Phase F — The views

### M3-F1 — The collapse preview and its alerts
- **Waits on:** E4, and input 6.
- **Read first:** ADR-021, decision 4; Hud.md §10, *The MVP's alerts*; GameDescription.md, *Support and collapse*, item 1.
- **Changes:** `Client::Overlays`, in the filter `Views`, builds overlay batches from the session: for each collapse in warning, its piece in bad, pulsing with a period of a second, and its strip's cells in bad on the ground; the alerts `W2 WILL FALL IN n S`, counting down, urgent, and `W2 HAS FALLEN` when it falls, from the collapse view's structure name. The preview draws while the integrity view is on (F1); the alerts always.
- **Tests:** **`OverlaysTests`:** a collapse view with a 9-voxel piece and a 10-cell strip gives 19 overlay records in the right entries; the alert text counts down in whole seconds, rounding up; a collapse with no structure says `A WALL WILL FALL`.

### M3-F2 — The integrity view
- **Waits on:** F1 and B5, and input 5.
- **Read first:** ADR-021, decision 4; Review/PaperTests/BreachPrediction.md, *The readouts*; Hud.md §10's readout rows.
- **Changes:** with F1 on (the default), the session asks each structure's integrity whenever a snapshot under it changes or a collapse touches it, and `Overlays` draws for each a label anchored above the section's middle, projected each frame and kept on whole pixels: the name at the names' size, then rows at the labels' size: `HOLE w m · falls at F m` or `HOLE none`, `OPENING ground 1 m · too low` as the paper test words its verdicts, `GAP none` or `GAP r m · falls at R m`, `GROUND earth · can be undermined` in bad or `GROUND rock · cannot be undermined` in good, and `FALLS` in bad while a collapse of it runs. The weak point's box is tinted accent. F1 toggles the view and the model records it.
- **Tests:** **`OverlaysTests`** gains: a reading becomes the expected rows word for word; the weak point's box gives its voxels as records; F1 toggles both the label and the tint; a label never leaves the screen's bounds when its anchor is near an edge.

### M3-F3 — The passability view
- **Waits on:** F2 and B6, and input 7.
- **Read first:** ADR-021, decision 4; ADR-015, *Consequences*; GameDesign.md, *Hypotheses*, H2's "open".
- **Changes:** with F2 on, the session asks reach for every tile the camera sees, from the player's squad's start cell, and asks again when a snapshot in or beside the tile changes or the camera moves to new tiles; `Overlays` draws each reached cell in good and each usable unreached cell in dim, as a box in the cell the unit stands in; the alert `BREACH OPEN AT W2` when the courtyard's goal cell first reads reached. F2 toggles the view.
- **Tests:** **`OverlaysTests`** gains: a tile reading of n reached and m usable cells gives n good and m − n dim records at the cells' walkable heights; the alert fires once, on the first update that reaches the goal; tiles off screen are not asked for.
- **Owner runs:** `Earthrise --windowed`, and with `--speed 10`: with F1 on, W2's label and the weak point's tint before the shots, the readings changing shot by shot, the piece pulsing in its warning with its strip on the ground, and the alerts; with F2 on, the courtyard dim until the fall and good after it, with the ramp's cells the route in; both views at 1280 × 720 and 1920 × 1080. What the owner asks changed becomes F4, F5 and so on, one commit each, in this phase's PR.

## Phase G — The lab

### M3-G1 — The lab's flow and its log
- **Waits on:** B7 and F3, and inputs 2, 3, 4 and 13.
- **Read first:** MvpPlan.md, M3, *Done when*; GameDesign.md, *Hypotheses*, H2, and *The playtest*; Review/PaperTests/BreachPrediction.md, *Running a session*; *Contracts*, `Simulation`, `BreachLab`, and *The lab's log*.
- **Changes:** `Earthrise --lab <participant> [--seed n]` in `Earthrise/Lab.h` and `.cpp`, in the filter `Lab`:
  - **Free fire first:** two minutes on the intact site with FIRE free, the integrity view on, and a banner `FREE FIRE · 1:59 LEFT`; its commands go to the log.
  - **Then the items,** P0 first and L01 to L16 in an order shuffled by `KeyedDraw` from the participant's seed, each on a fresh world: the site built, the camera put on W2 at the default framing, the prior shots run through `LabShots` at the clock's speed of 10 with the banner `WATCH`, and when they are done the clock paused and the question up: the objective `ITEM 3 OF 16`, the banner `WILL W2 BE OPEN TO FOOT AFTER 3 SHOTS AT THE MARK? · OPEN · O · SHUT · S`, the mark drawn through the overlay pipeline in accent at the aim voxel with its strike voxel beside it. The views, the camera and the HUD work; FIRE does not. The answer, by key or by the banner's buttons, unpauses the clock, runs the asked shots through `LabShots` at 10, and shows the result: `W2 IS OPEN` or `W2 IS SHUT`, with the passability view forced on, then `SPACE FOR THE NEXT`.
  - **The log** as *Contracts* fixes it: each item's row with the time from the question to the answer, which views were on at the answer, the camera's turns and zoom notches during the question, the live state hash, and the headless hash from `ResolveLabItem` on a fresh world of the same item, run when the item ends, with `matched`.
  - **`--lab` without a participant** refuses with a message; the practice item is logged with `P0` and not scored.
- **Tests:** **`LabTests`** (`ClientTests`, over a `LabModel` the executable's loop drives, with no window): the item order for seed 1 is a fixed permutation and differs from seed 2's; an answer before the question is up is ignored; the banner and the objective read as above for item 3 of 16 with N = 3; the log row for an answered item has every column, and `correct` agrees with the key. **`BreachLabTests`** gains: the live sequence, built from `LabShots` as `Earthrise` calls it with ticks between, gives each item the same hash as `ResolveLabItem`.
- **Owner runs:** `Earthrise --lab P00 --windowed`, through free fire, the practice item and all 16 items: every item matches its headless hash; the questions read; the fast-forward is watchable; then the log's columns in the CSV. What the owner asks changed becomes G1's fix commits.

### M3-G2 — Scoring, and the facilitator's guide
- **Waits on:** G1, and input 3.
- **Read first:** Review/PaperTests/BreachPrediction.md, *Scoring* and *Running a session*; `Tools/PaperTests/BreachPrediction.py`'s scoring arithmetic.
- **Changes:**
  - **`Tools/BreachLab/ScoreBreachLab.py <folder>`:** reads every log in a folder, checks that every item row matched its headless hash and that the keys across the logs split 8 and 8, and prints each participant's accuracy over L01 to L16, the median over participants, and PASS or FAIL against 0.65 (GameDesign.md, *Hypotheses*), with the paper test's note that at 16 items 0.65 needs 11 right. `--json` writes the same.
  - **`Tools/TestScoreBreachLab.py`,** with a step in the workflow's Linux job, over hand-written logs: six logs with 11, 11, 10, 12, 9 and 13 right give a median of 0.6875 and PASS; 10, 10, 11, 9, 12, 8 give 0.625 and FAIL; a log whose hash did not match fails the check by name.
  - **`Design/BreachLab.md`:** the facilitator's guide, Decided by the inputs: who takes part, what the facilitator says and does not say (the rules are not explained; the views are), how reasons are written down against item ids, the command line, and where the logs go.
- **Tests:** `Tools/TestScoreBreachLab.py`, run here and in CI.

### M3-G3 — The session and the report
- **Waits on:** G2, and the six-person session, which the owner runs on the benchmark machine.
- **Read first:** MvpPlan.md, M3, *Done when*; GameDesign.md, *Kill criteria*; ADR-006, *To be measured in M1 and M3*; the M2 report's shape.
- **Changes:**
  - **The logs** of the session go to `Design/Review/BreachLab/`, one CSV a participant, with the facilitator's reasons as `Reasons.md` beside them.
  - **`Design/Review/M3-<date>.md`** reports: each participant's accuracy, the median and the verdict; which items were missed most, with the reasons given, and whether a miss reads as a misread view or a misread rule; the query budget measured on the benchmark (`SimulationBench --integrity` and the lab's slowest ticks against 25 ms); the live and headless hashes matched; the CI runs per order; each ADR amendment; and what M4 inherits. If the median falls short, it proposes the one round of legibility work the kill criterion allows (GameDesign.md, *Kill criteria*), as changes to the views, never to the rules, for the owner to decide before M4.
  - **ADR-006**'s M3 row takes the median; the owner accepts ADR-006 on a pass, by its own terms.
  - **GameDesign.md,** *M3's values*, item 10 becomes Decided at the measured budget.
  - **MvpPlan.md:** the M3 record is complete, and what M4 inherits from M3 is written.
