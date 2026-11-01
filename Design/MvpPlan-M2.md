# Earthrise.World — M2 work orders

**Status:** Carried out, 2026-11-01, every order, and M2 closed the same day (GameDesign.md, *Closing M2*; the [M2 report](Review/M2-2026-11-01.md)). Proposed the same day with [MvpPlan.md](MvpPlan.md)'s M2 revision. No order from phase B on starts before M1 closes (GameDesign.md, *Planning M2*); phase A is paper. "Input N" means MvpPlan.md, M2, input N. These orders carry out M2 as MvpPlan.md, M2, defines it, under [ADR-013](ADR/ADR-013-ticks-numbers-and-randomness.md) to [ADR-016](ADR/ADR-016-server-data-format.md). Where an order and those differ, they win, and the order is corrected.

**An order is one commit, and a phase is one pull request** (AGENTS.md §6). There are 31 orders in seven phases:

| Phase | Orders | What lands | Waits on |
|---|---|---|---|
| A | A1 | The owner's decisions, written down | The owner's answers to MvpPlan.md, M2, inputs 1–15 |
| B | B1–B6 | `Core`'s numbers, randomness and bytes; the float check; `ServerData` | A, and M1 closed |
| C | C1–C5 | Commands, edits, the world and its tick, saves, replays | B |
| D | D1–D3 | The generator | C |
| E | E1–E7 | The shot, craters, the collapse, slump | C |
| F | F1–F6 | Navigation | C; F6 also phase E |
| G | G1–G3 | The done-when, the tick's timing, the report | D, E, F |

D, E and F each branch from `main` once C has merged, and can run side by side. Each fills its own steps of the tick, which C lands as empty functions.

## How a session works an order

1. **Read** AGENTS.md, this section, *Contracts*, the order, and the order's *Read first* list. Nothing else should be needed. If something is, the order is incomplete: say so in the report.
2. **Check *Waits on*.** An order starts only when every order it lists is committed, in its own phase, or merged, in an earlier one, and every input it lists is decided.
3. **Keep the contracts.** The signatures under *Contracts* are what other orders build on. A session that cannot keep one stops and reports. The contract is then changed in this file, in a commit of its own, before any code departs from it. A session never deviates silently (AGENTS.md).
4. **Verify through CI.** The cloud container has no MSVC and no vstest, so CI is the build.
   - **Before pushing,** run what runs here: `Build/CheckProjectFiles.py`; `Build/CheckFormat.py` with clang-format 18 where it is installed; `Build/TestCheckers.py` when a checker changed; and `Tools/CheckAssets.py` and the `Tools` tests when `GameData/` or `Tools/` changed.
   - **Then push the order's commit** to the phase's branch, whose PR is open as a draft. CI must be green before the next order starts.
   - **Count the CI runs** the order took, failures included. The commit's last body line says `CI runs: N`, and an amended commit updates it.
5. **Name the commit for the order.** Its subject starts with the order's id, as in `M2-B3: Add the byte writer and reader`.
6. **Register every file** in its project's `.vcxproj` and `.filters`, under the functional filter the order names. Folders stay flat (AGENTS.md §2).
7. **Write the tests the order names.** They go in `<Project>Tests/<Type>Tests.cpp`, in `TEST_CLASS(<Type>Tests)`, with methods named for the behaviour they check. M2 has no test that needs a window or a GPU.
8. **Save what you add.** An order that adds an entity kind or a queue of pending work adds it to the save, and extends `SaveTests`, in the same commit. ADR-014, decision 4 says a save holds everything the next tick reads.
9. **Accept the ADR you implement.** The commit that first implements an ADR marks it Accepted, with the date and any amendment the code forced (AGENTS.md §6), and notes the earlier ADRs' *Left open* items it closes. Each order says which ADR, if any.
10. **Hand the spike to the owner.** An order that changes `RendererSpike` lists it under *Owner runs*, and the owner launches the spike before the phase merges (AGENTS.md §3).
11. **Leave out-of-scope work out.** What a session notices beyond its order goes in the PR description, not the commit.
12. **Log the phase.** Dropped on closing M2 (GameDesign.md, *No time logging*): nothing about time is logged. The pull request and its commits, one per order, are the record. (Until then, a phase's last commit filled in its row in MvpPlan.md, M2, with dates, the PR, its commits, CI runs and the owner's hours.)

## Contracts

The seams more than one order builds on. Parameters, members and constants follow AGENTS.md §1. Each contract's comments state its rules.

### `Core` (B1–B3)

```cpp
// Core/Fixed.h — ADR-013, decision 3. Names carry the unit: xMeters, speedMetersPerTick.
class Fixed // Q16.16 in a signed 32-bit integer
{
public:
  static constexpr std::int32_t FRACTION_BITS = 16;
  static constexpr std::int32_t ONE_RAW = 1 << FRACTION_BITS;

  [[nodiscard]] static constexpr Fixed FromRaw(std::int32_t _raw) noexcept;
  [[nodiscard]] static Fixed FromWhole(std::int32_t _whole);         // throws std::overflow_error past ±32767
  [[nodiscard]] constexpr std::int32_t Raw() const noexcept;
  [[nodiscard]] constexpr std::int32_t FloorWhole() const noexcept;  // rounds toward negative infinity
  // + - * / go through 64-bit intermediates. * and / round toward negative infinity. A result outside
  // 32 bits throws std::overflow_error; dividing by zero throws std::domain_error. Comparisons are defaulted.
};

// Core/IntegerMath.h — portable 64-bit arithmetic only, no intrinsics, so ARM64 needs no second path.
[[nodiscard]] std::uint32_t IntegerSquareRoot(std::uint64_t _value) noexcept;                  // the floor
[[nodiscard]] std::int64_t FloorDivide(std::int64_t _numerator, std::int64_t _denominator);  // toward −∞; 0 throws std::domain_error

// Core/SplitMix64.h — ADR-013, decision 4.
inline constexpr std::uint64_t SPLITMIX64_GAMMA = 0x9e3779b97f4a7c15ULL;
/// SplitMix64's first output with _value as its seed: the gamma added, then the finalizer. Exactly the
/// local Mix in Simulation/TestWorld.cpp, RendererSpike/Debris.cpp and RendererSpike/SceneScript.cpp.
[[nodiscard]] constexpr std::uint64_t SplitMix64Mix(std::uint64_t _value) noexcept;
class SplitMix64
{
public:
  explicit constexpr SplitMix64(std::uint64_t _seed) noexcept;
  [[nodiscard]] constexpr std::uint64_t Next() noexcept; // SplitMix64Mix(state), then state += gamma
};
/// SplitMix64Mix(_seed ^ SplitMix64Mix(_salt ^ SplitMix64Mix(_key))), the form TestWorld's noise already uses.
[[nodiscard]] constexpr std::uint64_t KeyedDraw(std::uint64_t _seed, std::uint64_t _salt, std::uint64_t _key) noexcept;
/// ((_draw >> 32) * _bound) >> 32: [0, _bound), with a bias below _bound / 2^32.
[[nodiscard]] constexpr std::uint32_t DrawBelow(std::uint64_t _draw, std::uint32_t _bound) noexcept;

// Core/ByteWriter.h and Core/ByteReader.h — little-endian by shifts, unpadded; signed values in two's complement.
class ByteWriter
{
public:
  void WriteU8(std::uint8_t _value);   // and WriteU16, WriteU32, WriteU64, WriteI32, WriteI64
  void WriteBytes(std::span<const std::byte> _bytes);
  [[nodiscard]] std::span<const std::byte> Bytes() const noexcept;
  [[nodiscard]] std::vector<std::byte> TakeBytes() noexcept;
};
class ByteReader
{
public:
  explicit ByteReader(std::span<const std::byte> _bytes) noexcept;
  [[nodiscard]] std::uint8_t ReadU8();  // and ReadU16 … ReadI64; reading past the end throws std::out_of_range
  [[nodiscard]] std::span<const std::byte> ReadBytes(std::size_t _count);
  [[nodiscard]] std::size_t Remaining() const noexcept;
  [[nodiscard]] bool AtEnd() const noexcept;
};
```

### `ServerData` (B5–B6)

**The file** follows ADR-016. Three things are fixed here:
- **`MATL`** is 256 records of six `u8`s, one per palette index: whether the index exists, `Behavior`, `Support`, `spanVoxels`, flags (bit 0 for walkable ground, bit 1 for liquid) and `rubbleIndex`. The enums are numbered in their declared order.
- **`TMPL`** is a `u32` count, then each template in ascending id order. A template is written as:
  - its id, name and critical part's name, each as a `u32` length and then UTF-8;
  - `u32` width, height and depth, its voxel edge in millimetres, its stone in m³ and its timber in m³;
  - its palette indices, then its parts (0 none, 1 critical, 2 foundation, 3 stock), each w × h × d `u8` in layer-file order.
- **`RULE`** is a `u32` count, then each value as a `u32` key length, the key's UTF-8 and an `i64`, in ascending key order. A key names the converted unit, as in `warningBaseTicks`. The reader refuses a key it does not know and a key it needs that is missing. A new value is therefore a new key, not a new section version.

```cpp
// Simulation/MaterialRules.h gains:
enum class Behavior : std::uint8_t { None, Earth, CommonRock, BuildingStone, Bedrock, Timber, Leaves, Water, StoneRubble, WasteRubble };
// and MaterialRule gains: Behavior behavior; std::uint8_t rubbleIndex; // 0 when the material leaves no rubble

// Simulation/Rules.h — an aggregate: one field per RULE key, with the key's name and unit. B6 lands the
// fields of every key B5 bakes, and a later order that adds a key adds its field in the same commit.
struct Rules;

// Simulation/ServerData.h
class ServerData
{
public:
  static constexpr std::uint32_t FORMAT_VERSION = 1;
  [[nodiscard]] static ServerData Read(std::span<const std::byte> _file);  // throws std::runtime_error naming the failed check
  [[nodiscard]] static ServerData Load(const std::filesystem::path& _file);
  [[nodiscard]] std::uint64_t Hash() const noexcept;
  [[nodiscard]] std::uint64_t SharedHash() const noexcept;
  [[nodiscard]] const MaterialRules& Materials() const noexcept;
  [[nodiscard]] const Rules& Tuning() const noexcept;
  [[nodiscard]] TemplateLookup Templates() const; // refers to this object, which must outlive it
};
```

### Commands, voxels and the world (C1–C3)

```cpp
// Protocol/EntityId.h
using EntityId = std::uint64_t;          // a world gives ids from 1, rising, never reused (ADR-013, decision 5)
inline constexpr EntityId NO_ENTITY = 0;

// Protocol/Command.h — on the wire: u16 tag, u16 record version (1), then the fields in declaration order.
struct MoveSquad { EntityId squad; VoxelPosition goalVoxels; };     // tag 1
struct FireEngine { EntityId engine; VoxelPosition targetVoxels; }; // tag 2
using Command = std::variant<MoveSquad, FireEngine>;
void Encode(const Command& _command, Core::ByteWriter& _writer);
[[nodiscard]] Command DecodeCommand(Core::ByteReader& _reader); // an unknown tag or version throws std::invalid_argument

// Simulation/VoxelWorld.h — ADR-014, decisions 1 and 2.
class VoxelWorld
{
public:
  [[nodiscard]] ChunkStore& Base();  // for the generator only; throws std::logic_error once Edit has been called
  [[nodiscard]] const ChunkStore& Store() const noexcept;
  [[nodiscard]] std::uint8_t At(Protocol::VoxelPosition _position) const;
  void Edit(Protocol::VoxelPosition _position, std::uint8_t _paletteIndex);         // no-op when unchanged
  [[nodiscard]] std::span<const Protocol::VoxelPosition> TickEdits() const noexcept; // each voxel once, first-edit order
  void EndTick();                                                                    // clears TickEdits
  [[nodiscard]] const std::set<std::uint64_t>& EditedChunks() const noexcept;       // packed keys since generation or load; a load rebuilds it from the chunks the save holds
  [[nodiscard]] std::uint64_t VoxelHash() const; // Fnv1a64 over (key, chunk hash) in key order; chunk hashes cached
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> Publish();
};

// Simulation/World.h
using GenerateChunk = std::function<void(Protocol::ChunkCoordinate, Protocol::Chunk&)>;
using SurfaceHeight = std::function<std::int32_t(std::int32_t, std::int32_t)>; // x and z, in voxels
struct SubmittedCommand { std::uint8_t player; std::vector<std::byte> bytes; };
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
    std::int32_t widthChunks;     // chunks from 0 on each axis
    std::int32_t heightChunks;
    std::int32_t depthChunks;
    GenerateChunk generate;       // D1's generator, or a test's
    SurfaceHeight surfaceVoxels;  // the generated surface, which the anchor depth counts from
  };
  explicit World(const Desc& _desc); // generates every chunk through VoxelWorld::Base()
  void Submit(std::uint8_t _player, std::span<const std::byte> _command); // decoded in the next Tick's step 1
  void Tick();                                                           // MvpPlan.md's twelve steps, in order
  [[nodiscard]] std::uint32_t TickCount() const noexcept;
  [[nodiscard]] std::span<const SubmittedCommand> LastTickCommands() const noexcept; // what the last Tick applied
  [[nodiscard]] Protocol::EntityId NewEntityId() noexcept;
  [[nodiscard]] const VoxelWorld& Voxels() const noexcept;
  [[nodiscard]] VoxelWorld& Voxels() noexcept; // for the breach run, scenarios and tests, between ticks: their edits are searched in the next step 5
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> Publish(); // chunks changed since the last call
};
```

**The tick's steps** are private member functions of `World`, called in this order. Their names come from MvpPlan.md's tick:

1. `ApplyCommands`
2. `AdvanceJobs`
3. `MoveUnits`
4. `AdvanceShots`
5. `SearchSupport`
6. `AdvanceCollapses`
7. `SettleLoose`
8. `RebuildNavigation`
9. `SettleUnits`
10. `CheckRules`
11. `PublishState`
12. `AnswerQueries`

**Entities** are held in one `std::map<Protocol::EntityId, Kind>` per kind, which keeps every pass in id order. The order that adds a kind also adds a function that places one, for scenarios and tests, such as `AddSquad` and `AddSiegeEngine`.

### Saves and the collapse plan (C4, E3)

```cpp
// Simulation/ChunkEdits.h — ADR-014, decision 3: a u8 form (0 list, 1 run-length), then the form's bytes.
[[nodiscard]] std::optional<std::vector<std::byte>> EncodeChunkEdits(const Protocol::Chunk& _base, const Protocol::Chunk& _edited); // none when equal
void ApplyChunkEdits(Core::ByteReader& _reader, Protocol::Chunk& _chunk); // _chunk holds its base

// Simulation/Save.h
inline constexpr std::uint32_t SAVE_FORMAT_VERSION = 1;
[[nodiscard]] std::vector<std::byte> WriteSave(const World& _world);
[[nodiscard]] World LoadSave(std::span<const std::byte> _save, const World::Desc& _desc); // seeds and tick from the save; throws naming the failed check
[[nodiscard]] std::uint64_t StateHash(const World& _world); // ADR-014, decision 5

// Simulation/CollapsePlan.h — ADR-004, decision 5: what every view, preview and AI asks, never a copy of it.
enum class FallDirection : std::uint8_t { Down, North, East, South, West }; // +z north, +x east
struct PlacedVoxel { Protocol::VoxelPosition position; std::uint8_t paletteIndex; };
struct CollapsePlan
{
  FallDirection direction;
  std::uint32_t warningTicks;
  std::uint32_t fallTicks;
  std::vector<PlacedVoxel> landing;            // rubble and crushed structure, nearest the foot first
  std::vector<Protocol::VoxelPosition> strip;  // the columns the fall covers, at their ground: where units are hit
};
[[nodiscard]] CollapsePlan PlanCollapse(const Piece& _piece, std::span<const Protocol::VoxelPosition> _cause, const VoxelWorld& _world,
                                        const ServerData& _data); // pure: same inputs, same plan, no change to the world
```

## Phase A — Decisions

### M2-A1 — Write down the owner's decisions
- **Waits on:** the owner's answers to MvpPlan.md, M2, inputs 1–15. This order needs no M1 close, because it is paper.
- **Read first:** MvpPlan.md, M2, *The owner's inputs*; GameDesign.md, *Planning M2*.
- **Changes:**
  - **GameDesign.md** gains `### <date>: M2's values`, with each input as Decided in the owner's words, or as the owner changed it. Input 13 stays Proposed, because G2 measures it.
  - **MvpPlan.md:** each input's row says Decided, and input 1 records the go-ahead for each ADR.
  - **The ADRs** are edited where the owner changed them. They stay Proposed (AGENTS.md §6).
  - **The scenario line:** if the owner takes ADR-014, decision 7, GameDesign.md's line "A scenario is a save" is rewritten as that ADR's consequences give it.
- **Tests:** none. The checkers run, and every relative link resolves.

## Phase B — `Core` and server data

### M2-B1 — Fixed point and integer math
- **Waits on:** A1.
- **Read first:** ADR-013, decision 3; *Contracts*, `Core`.
- **Changes:** `Core/Fixed.h`, `Core/Fixed.cpp`, `Core/IntegerMath.h` and `Core/IntegerMath.cpp`, in the filter `Numbers`.
- **Tests:**
  - **`FixedTests`:**
    - Whole numbers round-trip, and −1.5 floors to −2.
    - The product of −1 raw and 0.5 floors to −1 raw.
    - −1 / 3 floors below −1/3.
    - `+` and `*` throw past 32 bits, and division by zero throws.
    - 1.4 m/s at 20 ticks a second is `FromRaw(4587)`, which is `FloorDivide(14 × 65536, 10 × 20)`.
  - **`IntegerMathTests`:**
    - `IntegerSquareRoot` of 0, 1 and 2 is 0, 1 and 1.
    - Of 2^64 − 1 it is 4294967295.
    - For n in {2, 65535, 4294967295}, n² gives n and n² − 1 gives n − 1.
    - `FloorDivide(−7, 2)` is −4, `(7, −2)` is −4 and `(−7, −2)` is 3, and a zero denominator throws.
- **ADR:** marks ADR-013 Accepted.

### M2-B2 — One pinned PRNG
- **Waits on:** B1.
- **Read first:** ADR-013, decision 4; *Contracts*, `Core`.
- **Changes:**
  - `Core/SplitMix64.h`, in the filter `Randomness`.
  - The local `Mix` in `Simulation/TestWorld.cpp`, `RendererSpike/Debris.cpp` and `RendererSpike/SceneScript.cpp` becomes `Core::SplitMix64Mix`, with each call pattern kept, so every output is bit-identical. `TestWorld`'s hash becomes a `KeyedDraw` call.
- **Tests:**
  - **`SplitMix64Tests`:** these values were computed on 2026-11-01 with Python's integers from the reference algorithm.
    - From seed 0, the first three outputs are `0xe220a8397b1dcdaf`, `0x6e789e6aa1b965f4` and `0x06c45d188009454f`.
    - From seed 1234567, they are `0x599ed017fb08fc85`, `0x2c73f08458540fa5` and `0x883ebce5a3f27c77`.
    - `SplitMix64Mix(0)` is the first output from seed 0.
    - `KeyedDraw` equals its nested form.
    - `DrawBelow(x, 1)` is 0, `DrawBelow(0, b)` is 0 and `DrawBelow(~0ULL, b)` is b − 1.
  - **The existing `TestWorldTests`** pass unchanged.
- **Owner runs:** the spike, to confirm the debris and the walkers are as before.

### M2-B3 — Bytes
- **Waits on:** B1.
- **Changes:** `Core/ByteWriter.h`, `Core/ByteWriter.cpp`, `Core/ByteReader.h` and `Core/ByteReader.cpp`, in the filter `Encoding`.
- **Tests:** **`ByteTests`:**
  - `WriteU64(0x0102030405060708)` writes `08 07 06 05 04 03 02 01`.
  - `WriteI32(−2)` writes `fe ff ff ff`.
  - Every width round-trips at its minimum and maximum.
  - A read past the end throws `std::out_of_range`.
  - `AtEnd` and `Remaining` agree.

### M2-B4 — Keep floating point out of the simulation
- **Waits on:** B1.
- **Read first:** ADR-013, *Consequences*; `Build/CheckProjectFiles.py`, `CheckSources`; `Build/TestCheckers.py`'s variant "Windows in a Windows-free project".
- **Changes:**
  - **`CheckSources`** reports `float` or `double` as a whole word in the code of `Simulation` and `Protocol`, after `StripCommentsAndStrings`. Its message is "{project} uses {word}; the simulation holds no floating point (ADR-013)".
  - **`TestCheckers.py`** gains the broken variant "floating point in the simulation", built as the Windows-free variant is.
  - **`TestWorld`:** `CastleXMeters()` and `CastleZMeters()` become `CastleXVoxels()` and `CastleZVoxels()`, returning `std::int32_t`. Its `IntegerSqrt`, which starts from a `double` guess, becomes B1's `Core::IntegerSquareRoot`; both return the exact floor, so its world is bit-identical.
  - **The callers:** `RendererSpike/SceneScript.cpp` casts to `float` on its own side, and `SimulationTests/TestWorldTests.cpp` drops its casts.
- **Tests:** `Build/TestCheckers.py`, which passes the repository and fails the new variant for its reason. It runs here and in the Linux job.
- **Owner runs:** the spike, to confirm its camera still opens on the castle.

### M2-B5 — The server half of the bake
- **Waits on:** A1, for ADR-016's go-ahead, input 7, and the values `Rules.json` starts with.
- **Read first:** ADR-016; ADR-008, decisions 2, 4 and 5; `Tools/BakeContent.py`; `Tools/CheckAssets.py`; *Contracts*, `ServerData`.
- **Changes:**
  - **`Tools/BakeContent.py --server OUT_DIR`** writes `OUT_DIR/ServerData.bin` as ADR-016 and the contract fix it, and only when its bytes change.
  - **Decorations** (input 7): the bake gives each decoration voxel a palette index for its host. Palette.json gains, at the next free indices, one entry for each decoration and host behaviour the templates hold (an opening in timber, an opening in stone, iron in timber), each with the decoration's look and its host's behaviour. Both halves of the bake write these indices, so a template's world voxels and the client's preview agree; the shared hash covers them.
  - **Ground and spans** (input 14): Palette.json gains a server-side `ground` flag for each index, which the client half ignores. `Rules.json` holds each behaviour's span: stone 2 m, timber 5 m, leaves 3 m, common rock 2 m.
  - **`GameData/Server/Rules.json`** holds every value A1 decided, among them the spans, E1's launch height of 10 m and gravity of 9.81 m/s², and E2's craters. Each value is a decimal string under a key named with its unit, and converts exactly through `fractions.Fraction`, floored once unless the decision says otherwise (the fall rounds up).
  - **`Tools/CheckAssets.py`** gains two checks: `Rules.json` converts to whole units, and nothing under `GameData/Client/` reaches the server's bake.
  - **`Tools/TestBakeContent.py`** is new, with a step in the workflow's Linux job, `python3 Tools/TestBakeContent.py -v`, guarded like the asset check's tests.
- **Tests:** **`TestBakeContent.py`:**
  - **The file:** the header's hash covers exactly the body, and the body's shared hash equals the client half's for the same sources. A second bake of unchanged sources leaves the file untouched.
  - **`MATL`:** indices 4 and 6 give stone rubble (8), 7 and 12 give timber rubble (25), 2 gives earth rubble (26), 3 gives rock rubble (28), and 10 and 11 give 0. Index 4 is ground and 6 is not. Each new decoration index takes its host's rule.
  - **`TMPL`:** it holds T1–T9, with T3 at 96 m³ of stone.
  - **`RULE`:** 1.4 m/s converts to 4587 raw a tick, and a span that is not a whole number of voxels fails.
- **ADR:** marks ADR-016 Accepted.

### M2-B6 — Simulation reads its server data
- **Waits on:** B3 and B5.
- **Read first:** ADR-016, decisions 5 to 7; ADR-010, decision 7, and its amendment; `Content/Content.vcxproj`'s `BakeClientContent` target.
- **Changes:**
  - **New files:** `Simulation/ServerData.h` and `.cpp` and `Simulation/Rules.h`, in the filter `Data`. `MaterialRule` gains `behavior` and `rubbleIndex`. `StandInMaterialRules` is replaced by `ScaleSpans(const MaterialRules&, std::uint32_t _voxelsPerMeter)`, which `SimulationBench` uses for ADR-006's 0.5 m column, and the support and navigation test fixtures take the baked rules.
  - **`Simulation.vcxproj`** gains the target `BakeServerData`, modelled on `BakeClientContent`, with outputs in `$(OutDir)ServerData\ServerData.bin`.
  - **`SimulationBench`:**
    - It drops `Content`: its reference, its include directory and `ContentTemplates`.
    - It becomes `wmain` and loads `ServerData` from beside its own executable, found from `argv[0]`.
    - ADR-010's table and its amendment say so.
  - **`RendererSpike`** builds its test world from `ServerData::Templates()`. It refuses to start when `ServerData::SharedHash()` differs from `Content::SharedHash()`, naming both.
  - **`SimulationTests`** finds the file through a helper, `SimulationTests/ServerDataFile.h`. The helper builds the path from `__FILE__`'s folder, the platform (`_M_X64` or `_M_ARM64`) and the configuration (`_DEBUG`), under ADR-010's output layout. That way the tests include no Windows header.
- **Tests:**
  - **`ServerDataTests`:**
    - The real file reads, with its template count and T3's stone matching `GameData/Manifest.json`.
    - Index 6 spans 2 voxels and leaves rubble 8.
    - A copy with one body byte flipped is refused, naming the hash. A wrong magic number or format version is refused too.
    - Every index's support, span, ground and liquid flags equal what `StandInMaterialRules` gave. The decoration indices 18 and 20 no longer reach the world, and the new host indices take their host's rule. The expected table is written into the test from `MaterialRules.cpp` before that function goes.
    - `ScaleSpans(rules, 2)` doubles every span, which `SupportSearchTests`' half-metre test now checks.
  - **The existing support and navigation tests** pass on the baked rules.
- **Owner runs:** the spike, to confirm the town is unchanged and that a stale `Content` is refused.
- **ADR:** ADR-010, amended.

## Phase C — Commands, edits, the tick, saves and replays

### M2-C1 — Commands as bytes
- **Waits on:** B3.
- **Read first:** ADR-002, decision 3; *Contracts*, commands.
- **Changes:** `Protocol/EntityId.h` and `Protocol/Command.h` and `.cpp`, in the filter `Commands`.
- **Tests:** **`CommandTests`:**
  - Each command round-trips.
  - `MoveSquad{7, {1, −2, 3}}` encodes as exactly `01 00 01 00 07 00 00 00 00 00 00 00 01 00 00 00 fe ff ff ff 03 00 00 00`.
  - An unknown tag throws, and so does a known tag at version 2.
  - Truncated bytes throw `std::out_of_range`.

### M2-C2 — Edits through one call
- **Waits on:** B6.
- **Read first:** ADR-014, decisions 1 and 2; *Contracts*, `VoxelWorld`.
- **Changes:**
  - `Simulation/VoxelWorld.h` and `.cpp`, in the filter `World`.
  - `ChunkStore` gains `[[nodiscard]] const Protocol::Chunk* Find(std::uint64_t _key) const`, which returns the chunk's current voxels, pending or published.
  - `TestWorld` and `TestWorldEdits` stay as they are (ADR-014, *Consequences*).
- **Tests:** **`VoxelWorldTests`:**
  - An edit to the value a voxel already holds changes nothing.
  - `TickEdits` lists each voxel once, in first-edit order, and `EndTick` clears it.
  - `Base()` throws after the first `Edit`.
  - A chunk dug and refilled stays in `EditedChunks`.
  - A chunk's hash changes after an edit, and equals `Fnv1a64` over its voxels.
  - Two edit orders that reach the same voxels hash the same.
- **ADR:** marks ADR-014 Accepted.

### M2-C3 — The world and its tick
- **Waits on:** C1 and C2.
- **Read first:** ADR-013, decisions 1, 2 and 5; MvpPlan.md, *One simulation tick*; *Contracts*, `World`.
- **Changes:**
  - **New files,** in the filter `Tick`:
    - `Simulation/World.h` and `.cpp`;
    - `Simulation/Script.h` and `.cpp`, holding `struct ScriptedCommand { std::uint32_t tick; std::uint8_t player; Protocol::Command command; };` and `void RunScript(World&, std::span<const ScriptedCommand>, std::uint32_t _ticks)`.
  - **The entity kinds:** `Squad`, `Unit` and `SiegeEngine`, minimal records of id, owner, class and position (`Core::Fixed` and the cell). Each later order adds its own fields.
  - **`ApplyCommands`** records a `MoveSquad`'s goal on its squad. It decodes `FireEngine` and leaves it for E1.
- **Tests:** **`WorldTests`:**
  - A command submitted before tick n takes effect in tick n's step 1, not before.
  - Ids rise from 1 and are not reused after an entity goes.
  - Two worlds given the same `Desc` and script agree after 100 ticks, in `VoxelHash` and in every squad's goal.
  - A malformed command throws in step 1: `std::invalid_argument` for an unknown tag or version, `std::out_of_range` for truncated bytes.
  - Every chunk is generated.
  - `LastTickCommands` holds what the tick applied.
- **ADR:** ADR-013, amended if the tick forced anything.

### M2-C4 — Saves
- **Waits on:** C3.
- **Read first:** ADR-014, decisions 3, 4 and 6; *Contracts*, saves.
- **Changes:**
  - **New files:** `Simulation/ChunkEdits.h` and `.cpp` and `Simulation/Save.h` and `.cpp`, in the filter `Saves`.
  - **The build id:** `SimulationBench` computes it at startup by hashing its own file, found from `argv[0]`, and passes it in. The spike neither saves nor replays in M2, so it needs none yet.
- **Tests:**
  - **`ChunkEditsTests`:**
    - A few changes take the list form, and a chunk replaced throughout takes the run-length form.
    - A chunk back to its base gives nothing.
    - Both forms round-trip.
    - The two forms can never tie: a list is 2 + 3n bytes and a run-length copy 3r. The tie rule in ADR-014 is therefore moot, and this commit says so there.
  - **`SaveTests`:**
    - 37 ticks, a save, a load and 63 ticks match 100 ticks straight, by `StateHash`.
    - Saving a loaded save gives the same bytes.
    - A save from another build id, server-data hash, generator version or format version is refused, naming that check.

### M2-C5 — Replays
- **Waits on:** C4.
- **Read first:** ADR-014, decision 5.
- **Changes:** `Simulation/Replay.h` and `.cpp`, in the filter `Saves`. It holds `class ReplayRecorder`, which takes the start, each tick's `LastTickCommands` and a `StateHash` every 20 ticks, and `ReplayResult Replay(std::span<const std::byte>, const World::Desc&)`, which returns whether the replay matched and the first hashed tick that differed.
- **Tests:** **`ReplayTests`:**
  - 200 recorded ticks replay with every hash equal.
  - One flipped command byte is reported at the first hashed tick at or after it, within 20 ticks.
  - A replay from another build id is refused.

## Phase D — The generator

### M2-D1 — The world-scale layer and chunks
- **Waits on:** C3, and input 3.
- **Read first:** ADR-003; GameDescription.md, *Materials* and *Generated maps*; input 3; `Simulation/TestWorld.cpp`, for the height field M1 used.
- **Changes:** `Simulation/Generator.h` and `.cpp`, in the filter `Generation`:
  - **The constants:** `GENERATOR_VERSION = 1` and `ANCHOR_DEPTH_VOXELS = 8`.
  - **`Generator::Desc`** holds the two seeds and the map's size in voxels.
  - **The layer:** the constructor computes the world-scale layer from the seeds alone: relief, soil depth, bedrock depth, a river with one-voxel fords, outcrops of building stone from the resource seed, forest, and grass on undisturbed earth. Earth goes only where no four-neighbour is more than one voxel lower; steeper ground is rock.
  - **`GenerateChunk(coordinate, chunk)`** reads only the layer and the coordinate. `SurfaceAt(x, z)` gives the generated surface.
  - **The map** is 512 × 64 × 512 voxels.
- **Tests:** **`GeneratorTests`:**
  - The same chunk twice gives the same voxels.
  - Generating the chunks in two different orders gives the same world.
  - Every behaviour input 3 asks for appears on a fixed seed.
  - The surface stays within input 3's range.
- **Out of scope:** the Hold maps' constraints (M6), scenarios and Worlds.

### M2-D2 — Golden hashes and the slope invariant
- **Waits on:** D1.
- **Read first:** ADR-003, *Consequences*.
- **Changes:** none outside the tests.
- **Tests:** **`GeneratorTests`:**
  - **`GoldenHashes`:** `Fnv1a64` over every chunk of the map, in key order, for three fixed seed pairs, against committed values. The session cannot run the build, so the first push fails and prints the actual hashes. The session reads them from the CI log, checks that a second run prints the same, and amends this order's commit with them, its own branch's history only. The commit body says so.
  - **`SlopeInvariant`:** for the same seeds, no column whose top solid voxel is earth or grass stands more than one voxel above any of its four neighbours' tops. That is the condition under which E6 would slump it.
- **Note:** Release must give the same hashes (ADR-003). Whoever builds Release checks that.

### M2-D3 — The spike draws a generated map
- **Waits on:** D1.
- **Changes:** `RendererSpike --generated-map <seed>` draws the generated map with no units and no edits, the camera at its middle.
- **Owner runs:** the spike, and judges the map against input 3. Changes the owner asks for become orders D4, D5 and so on, one commit each, in this phase's PR.

## Phase E — Destruction

### M2-E1 — The trebuchet's shot
- **Waits on:** C4, and input 12.
- **Read first:** GameDescription.md, *Combat model*; GameDesign.md, *the paper tests' values*, items 12 and 13; input 12.
- **Changes:**
  - **New entity kind:** `Shot`. `SiegeEngine` gains a reload counter.
  - **When a `FireEngine` is refused:** when the target lies outside the range, or the engine is still reloading. A refused command does nothing.
  - **The arc:** the shot leaves 10 m above the engine's cell (input 12, a `RULE` value baked by B5), and descends at exactly 45° at the target's height. It is solved in `Core::Fixed` with `IntegerSquareRoot`, under gravity from `RULE`.
  - **Each tick** it moves along its arc and visits every voxel the step crosses, by an integer 3D DDA (Amanatides and Woo, 1987). It stops at the first solid voxel, which is the hit E2 takes.
- **Tests:** **`ShotTests`:**
  - A shot at open ground strikes the target's column, or the first solid voxel before it.
  - A wall in the way is struck on its face.
  - A shot whose step spans a one-voxel post hits the post.
  - Out-of-range and reloading commands are refused.
  - The same script gives the same hits.
  - A save taken with a shot in flight continues identically.

### M2-E2 — Craters
- **Waits on:** E1.
- **Read first:** input 12; the [Siege route table](Review/PaperTests/SiegeRouteTable.md)'s crater ladder, in its table of the values it proposed.
- **Changes:** a hit stamps the crater the struck voxel's behaviour sets in `RULE`, as edits to empty. "Behind" is along the shot's larger horizontal axis, ties to x.
  - **Timber:** the voxel and its six face neighbours, timber only.
  - **Earth:** the voxel, the one behind it, and the one below each.
  - **Stone:** the voxel and the one behind it.
  - **The rest** (input 12): common rock as stone; leaves as timber, leaves only; every rubble as earth; bedrock and water take nothing, and the shot stops.
- **Tests:** **`CraterTests`:**
  - Each material's count and shape.
  - A timber crater takes no stone.
  - Masonry (index 6) and common rock take stone's crater, rubble takes earth's, and bedrock takes none.
  - The crater's voxels are in `TickEdits`.

### M2-E3 — The support search in the tick, and the collapse plan
- **Waits on:** E2, and inputs 4–7.
- **Read first:** ADR-004, decisions 2 to 5; *Contracts*, collapse plan; MvpPlan.md, M2, *Risks this estimate sees*.
- **Changes:**
  - **The edits not yet searched:** `World` keeps them as a queue: every edit since the last step 5, those made between ticks included. Step 5 searches and clears it, and the queue is saved.
  - **New entity kind:** `Collapse`, one for each unsupported piece, given ids in piece order.
  - **`Simulation/CollapsePlan.h` and `.cpp`,** in the filter `Destruction`. They set the warning (input 4), the fall's ticks (input 5), its direction (input 6) and the landing, one rubble voxel per fallen voxel (input 7), along the fall over at most the piece's height.
  - **During the warning,** no voxel of the piece moves.
- **Tests:** **`CollapseTests`:**
  - **The hand calculation:**
    - A T3 section from `ServerData` on flat ground, undercut 5 wide at its base, drops the one-voxel-wide slot MvpPlan.md's risks work out by hand.
    - Undercut 4 wide, it drops nothing.
    - If the code disagrees with the hand calculation, this commit corrects the risk's text.
  - **The plan:**
    - An 8 m piece warns for 135 ticks.
    - An undercut on one side falls towards it, and an even one falls `Down`.
    - `PlanCollapse` changes nothing and returns the same plan twice.
- **ADR:** ADR-004, amended with the landing's layout.

### M2-E4 — The fall
- **Waits on:** E3, and input 8.
- **Changes:** step 6 runs the fall.
  - When the warning ends, the piece leaves the grid on the fall's first tick.
  - The landing arrives in equal parts over the fall's ticks, nearest the foot first.
  - Structure voxels in the strip below the rubble's top become rubble of their own material.
  - Each unit standing in the strip gets one crush event carrying the collapse's id. Its damage is M4's.
- **Tests:** **`FallTests`:**
  - As many voxels land as fell.
  - The materials follow input 7.
  - Nothing outside the strip changes.
  - The edits are spread over the fall's ticks.
  - A unit inside the strip gets one event, and a unit outside it gets none.

### M2-E5 — Cascades, and collapses in a save
- **Waits on:** E4.
- **Changes:** a fall's edits are searched in the next tick's step 5. Collapses started on the same tick advance in id order. Collapses and their state are saved.
- **Tests:**
  - **`CascadeTests`:**
    - A fall that undermines a second piece starts that collapse on the next tick, not the same one.
    - Two collapses that start together advance in id order.
  - **`SaveTests`:** saves mid-warning and mid-fall each continue identically.

### M2-E6 — Slump
- **Waits on:** E5, and input 9.
- **Changes:** step 7 settles loose material over four neighbours.
  - **What is loose:** earth, grass and every rubble. A grass voxel that moves lands as earth (index 2), because only the generator places grass.
  - **The rule:** the top voxel of a column more than one above a neighbour moves to its lowest such neighbour, with ties in north, east, south, west order.
  - **The budget:** columns wait in a queue, in column-index order, under `RULE`'s `slumpVoxelsPerTick`, which this order adds with a Proposed value. The queue is saved.
- **Tests:** **`SlumpTests`:**
  - A one-wide, four-deep pit in earth settles to slopes of one, and no voxel is lost.
  - With a budget of 1, one voxel moves a tick, in the stated order.
  - Slump under a wall's foundation starts its collapse on the next tick.
  - A trebuchet's earth crater slumps closed.
  - A save mid-slump continues identically.

### M2-E7 — Loose material near structures
- **Waits on:** E6.
- **Read first:** GameDescription.md, *Materials*, the paragraph on loose material; input 9.
- **Changes:** loose material never rests on top of a structure, nor higher than the set height below the top of a structure beside it, a `RULE` value. Such a voxel moves on, away from the structure.
- **Tests:** **`LooseMaterialTests`:**
  - Rubble heaped against a T3 section settles at least 1 m below its top.
  - Nothing comes to rest on the wall's top.
  - No heap ever stands within 1 m of an intact wall's top.

## Phase F — Navigation

### M2-F1 — Headroom, ground and classes
- **Waits on:** C3, and inputs 10 and 11.
- **Read first:** ADR-015, decisions 1 and 2; `Simulation/NavigationGrid.h` and `.cpp`.
- **Changes:**
  - Each cell gains its headroom and ground kind.
  - The classes come from `RULE`, and each class has its own link bits under ADR-015's climb rule.
  - `TilesTouchedBy` widens by the largest margin.
- **Tests:** **`NavigationClassTests`:**
  - **Climb:** on 1 m steps foot passes and cavalry does not. On a 1-in-2 ramp cavalry passes and machines do not.
  - **Headroom:** T5's passage, from `ServerData`, takes every class's headroom. A 2 m arch takes foot only.
  - **Fords:** one voxel of water passes every class, and two pass none.
  - **Corners:** no class cuts a corner.
  - **The existing `NavigationGridTests`** pass.
- **ADR:** marks ADR-015 Accepted.

### M2-F2 — Clearance
- **Waits on:** F1.
- **Changes:** each cell holds, for each class, its clearance, under ADR-015, decision 2. It is capped at the widest class and rebuilt with its tile, margins included.
- **Tests:** **`ClearanceTests`:**
  - Open ground gives the cap.
  - T5's passage gives 3, so the ram fits it.
  - A 2-wide gap stops the ram and lets foot through.
  - After seeded random edits, rebuilding only the touched tiles matches a full rebuild.

### M2-F3 — The search
- **Waits on:** F2.
- **Read first:** ADR-015, decision 5; ADR-013, decision 6.
- **Changes:**
  - **The algorithm** to start from is A\* over cells. Steps cost 10 straight and 14 diagonally, times the ground's factor. The heuristic is octile distance in the same integers, and ties go by f, then h, then cell index.
  - **Where:** searches run in step 3, `MoveUnits`, before units move, for the squads step 8 marked and those still searching (MvpPlan.md, *One simulation tick*).
  - **The budget:** `RULE`'s `pathNodesPerTick`, added with a Proposed value. Searches are served in request order, then by squad id. A search the budget interrupts goes on next tick and is saved.
  - **An unreachable goal** gives the nearest reachable cell by octile distance, with ties to the lower index.
  - **`SimulationBench`** reports nodes and milliseconds for cross-map searches on the MVP map.
  - **ADR-015** records the algorithm and the figures, as an amendment.
- **Tests:** **`SearchTests`:**
  - On open ground the path is the least cost.
  - The path detours round rubble when the detour is cheaper.
  - Equal paths always come out the same.
  - A search split over ticks by the budget gives the unsplit search's path.
  - An unreachable goal gives the nearest reachable cell.
  - A save taken mid-search continues identically.

### M2-F4 — Reach, and searching again
- **Waits on:** F3.
- **Changes:**
  - **How reach is known without searching the map:** by region labels per class, or by a tile-level graph. The session measures both on the MVP map in `SimulationBench`, keeps the cheaper, and records the choice and its figures in ADR-015.
  - **Searching again:** under ADR-015, decision 5.
- **Tests:** **`ReachTests`:**
  - **Before the breach:** a squad ordered past an intact wall is short of its goal and stands at the nearest reachable cell.
  - **After it:** once edits remove the section, the squad searches again after the rebuild, and its path crosses the gap.
  - **On a rebuilt tile:** a squad whose path crosses one searches again.
  - **Unreachable goals:** one costs fewer nodes than a full-map search.

### M2-F5 — Squads moving
- **Waits on:** F4.
- **Changes:**
  - A `MoveSquad` starts its squad's search, and the figures follow the path in single file in `Core::Fixed`.
  - They move at their class speed from `RULE`, slowed by the ground's factor, and stand at their cell's height.
  - Positions are saved.
- **Tests:** **`MovementTests`:**
  - A foot squad crosses 14 m of open ground in 200 ticks, give or take one.
  - The same distance on rubble takes twice as long.
  - No figure stands on a structure's cell on any tick.
  - A save taken mid-walk continues identically.

### M2-F6 — The drop step
- **Waits on:** F5, and phase E merged.
- **Changes:** step 9, `SettleUnits`, runs as ADR-015, decision 7, sets out.
- **Tests:** **`DropTests`:**
  - **Ground dug away:** the unit drops. A drop over 3 m raises a crush event carrying the drop, and a drop of 3 m or less raises none.
  - **Rubble filling the cell:** the unit is lifted, with that collapse's id.
  - **Lost headroom:** the unit moves to the nearest usable cell, with ties to the lower index.
  - **Order:** units are handled in id order.

## Phase G — Done

### M2-G1 — The M2 breach run, in CI
- **Waits on:** D2, E7 and F6.
- **Read first:** MvpPlan.md, M2, *Done when*. It is not a scenario in ADR-014's sense; it is built in C++, as M1's test world is, so the tests and the bench share it.
- **Changes:** `Simulation/BreachRun.h` and `.cpp`, in the filter `World`, build it, and `SimulationTests/BreachRunTests.cpp` asserts it. It is set up as follows:
  - **The ground:** the generated map with fixed seeds.
  - **The wall:** a T3 section placed by edits on a levelled footprint.
  - **The forces:** a trebuchet 40 m off, and a squad of six foot outside the wall, ordered inside before the breach.
  - **The shots:** scripted `FireEngine` commands at aim points chosen from E3's findings and written into the test.
- **Tests:**
  - Each line of the done-when is an assertion.
  - Replays match, and saves taken at three ticks (before the breach, mid-fall and mid-walk) continue identically.

### M2-G2 — The tick's timing
- **Waits on:** G1, and input 2.
- **Changes:**
  - **`SimulationBench --breach-run`** times every tick of the breach run. It writes a CSV holding the per-tick times, their 95th percentile against input 2's target, the build id, and the CPU from `PROCESSOR_IDENTIFIER` and `NUMBER_OF_PROCESSORS`, read with `std::getenv`, so no Windows header is needed.
  - **The work budgets** are tuned to fit input 2's target and recorded as Proposed in GameDesign.md, *M2's values* (input 13).
- **Owner runs:** the bench on the benchmark machine.

### M2-G3 — The report
- **Waits on:** G2.
- **Changes:**
  - **`Design/Review/M2-<date>.md`** reports:
    - what the done-when showed;
    - what a breach looks like under the rules on the 1 m grid;
    - the budgets measured;
    - the CI runs per order (the owner's hours per phase were asked for too, until the owner dropped time from the plan, GameDesign.md, *No time logging*);
    - each ADR amendment.
  - **MvpPlan.md:** its M2 record is complete, and what M3 inherits from M2 is written.
