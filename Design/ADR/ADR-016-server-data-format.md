# ADR-016 — The `ServerData` file and where it is baked

**Status:** Proposed, 2026-11-01. It settles two items left open by earlier ADRs: the `ServerData` format in detail ([ADR-008](ADR-008-server-and-client-game-data.md), *Left open*), and where its bake target lives ([ADR-010](ADR-010-solution-and-build-layout.md), *Left open*). It amends ADR-008, decision 4: the reader also recomputes the server-data hash and checks each section's version (decision 6).
- **The owner gave the go-ahead** on 2026-11-01 ([MvpPlan.md](../MvpPlan.md), M2, input 1; GameDesign.md, *M2's values*).
- **The commit that first implements it** marks it Accepted (AGENTS.md §6).

Nothing implements it yet.

## Context

- **ADR-008 decides the shape.** The server reads baked files in `<outputdir>\ServerData` at startup. The format is our own binary: little-endian and versioned. Every check runs at bake time. The server parses no JSON and refuses a file that is missing, corrupt or of another version. The tool can run on its own, so server-only data is tuned without recompiling. Two hashes keep the halves honest:
  - the shared hash, in both halves;
  - the server-data hash, which saves, replays and playtest logs record.
- **ADR-008 also says what is not server data:** generator parameters are compiled into the generator and count as its version. [ADR-004](ADR-004-rule-based-destruction.md) makes the anchor depth "a depth the generator fixes", so it is the generator's as well.
- **ADR-010:**
  - Every project writes to one output directory, `$(SolutionDir)$(Platform)\$(Configuration)\`, which is ADR-008's `<outputdir>`.
  - The client half of the bake is a target in `Content.vcxproj` that runs `Tools/BakeContent.py --client` before compiling.
  - The server half comes in M2 with its own target, at a place left open.
- **M1 left stand-ins for M2 to replace:**
  - `Simulation::StandInMaterialRules` holds each palette index's support and walking rules, with the owner's spans in code.
  - `SimulationBench` and `RendererSpike` hand `Simulation` the templates from `Content` through `TemplateLookup`, which is why `SimulationBench` depends on `Content` until M2 (ADR-010, amended).
- **The shared hash already exists on the client's side.** `Content/SharedHash.h` and the bake state its encoding, and `ContentTests` recomputes it from the compiled tables.
- **M2 adds tuning values the simulation decides by:**
  - the work budgets of [ADR-013](ADR-013-ticks-numbers-and-randomness.md);
  - the navigation classes of [ADR-015](ADR-015-navigation.md);
  - the crater table, the collapse's timings and the rule for loose material.

  The design writes these in metres and seconds; the simulation counts in voxels, ticks and `Core::Fixed`.

## Decision

1. **One file: `<outputdir>\ServerData\ServerData.bin`.** One file gives one hash, one check on load, and a replacement that is never half done. A later section joins this file rather than becoming a file of its own.
2. **It is a header and a body,** little-endian and unpadded throughout. The reader copies fields out with `Core`'s byte reader, so it never casts the file to a struct and has no alignment to respect.
   - **The header:**
     - the four bytes `ERSD`;
     - a `u32` format version;
     - a `u64` server-data hash: `Core::Fnv1a64` over every byte of the body.
   - **The body:**
     - the `u64` shared hash, in the encoding `Content/SharedHash.h` states;
     - a `u32` section count;
     - a directory with one entry per section: a four-character tag, a `u32` section version, a `u32` offset and a `u32` length;
     - the sections, in the directory's order.
3. **M2's sections:**
   - **`MATL`:** each of the 256 palette indices, resolved to one rule. A rule says:
     - whether the index exists, and its behaviour;
     - its span in voxels;
     - whether it is walkable ground or liquid;
     - the palette index its rubble takes, with 0 when it leaves none.

     It replaces `StandInMaterialRules`. A decoration reaches the world as a palette index for its host, with the decoration's look, so each index has exactly one rule (GameDesign.md, M2's values, item 7). Spans come from each behaviour's value in `RULE`, and "walkable ground" from Palette.json's server-side `ground` flag (item 14).
   - **`TMPL`:** the server view of each template. That is the client view (ADR-008, decision 2) plus each voxel's part and the names of its critical parts. It feeds `TemplateLookup`, and `SimulationBench` drops `Content`.
   - **`RULE`:** the tuning values, already in the simulation's units:
     - the work budgets;
     - the navigation classes and their ground factors;
     - the crater table;
     - the collapse's timings;
     - the loose material's set height.

   Later sections join when their milestone needs them: unit statistics, waves and scenarios ([ADR-014](ADR-014-edits-saves-and-replays.md), decision 7). Each joining section raises nothing but its own section version. The format version changes only when the header or the directory does.
4. **Sources say what the design says, and the bake converts them exactly.**
   - **Where they live:** the tuning values are in `GameData/Server/Rules.json`, each named with its unit, as R6 asks of code: `warningBaseSeconds`, `stoneSpanMeters`, `speedMetersPerSecond`.
   - **How they convert:** the bake reads each number as a decimal string into Python's `fractions.Fraction`, converts it to voxels, ticks or Q16.16 exactly, and rounds once, toward negative infinity, as `Core::Fixed` does (ADR-013, decision 3).
   - **Why:** no binary floating point touches a value on its way into the simulation, so the same sources give the same file on any machine. The asset check fails on a value that does not convert to a whole number of the unit it needs, such as a span that is not a whole number of voxels.
5. **The bake target lives in `Simulation.vcxproj`.**
   - **What it runs:** `python Tools/BakeContent.py --server "$(OutDir)ServerData"` before compiling, with `GameData/Server/**` and the bake's scripts as inputs and the file as its output, so it is incremental. It writes the file only when its bytes change.
   - **Why there:** every consumer of the server's data references `Simulation`, and every project writes to the same output directory. The file is therefore baked before anything that reads it and sits beside every executable and test DLL: `SimulationTests`, `SimulationBench`, `RendererSpike`, and `Earthrise` when it joins.
   - **Why not elsewhere:** a target in each executable would bake the same file several times. A project of its own would be a thirteenth project that compiles nothing, which ADR-010's layout and checker do not provide for.
   - **Running it by hand:** the command above, with the output folder named, is also how the owner retunes between playtests without a C++ build (ADR-008, decision 4).
6. **`Simulation` reads the file from a path it is given.**
   - **Why a path:** finding the executable's own folder needs Windows, and `Simulation` includes no Windows header ([ADR-007](ADR-007-platforms-and-input.md)). So the executable, or the test, passes the path.
   - **What the reader checks:**
     - the magic number and the format version;
     - the server-data hash, recomputed;
     - each section's version.

     It refuses anything else with a message naming what failed. Recomputing the hash is how "corrupt" is detected, which ADR-008 asks for.
   - **What it does not check:** none of the content checks run again. They all ran at bake time.
7. **The shared hash is compared wherever one process holds both halves.** That is `RendererSpike` from M2, which draws `Content`'s templates over a world built from `ServerData`'s, and `Earthrise` from M4 (ADR-008, decision 5). A mismatch refuses to start, naming both hashes. The likely cause is one half rebuilt without the other.

## Consequences for the code

- **`Tools/BakeContent.py`** gains `--server`, the `RULE` source and the conversions. Its server half reuses the reading and checks of the client half and of `Tools/CheckAssets.py`, so the two halves cannot disagree on a template.
- **`Tools/CheckAssets.py`** gains the check ADR-008 already names, that nothing under `GameData/Client/` reaches the server's bake, and the whole-unit check of decision 4.
- **`Simulation`** gains the reader. `StandInMaterialRules` and the `Content` edge of `SimulationBench` go (ADR-010, amended again on acceptance).
- **Tests:**
  - A test of the bake checks the encoding against a hand-written source.
  - `SimulationTests` reads the real baked file and checks its counts against `GameData/Manifest.json`'s, among them T3's 96 m³.
  - A test feeds the reader a corrupted copy and expects a refusal.
- **CI already builds before it tests,** so the file exists when the simulation tests run, as the readiness review asked (B3).
- **`.gitignore` needs nothing new:** `ServerData\` is inside the ignored output tree.

## What this forecloses

- More than one `ServerData` file, and a format with alignment or padding.
- Binary floating point in the bake's conversions, and tuning values in the C++ source once their section exists.
- Generator parameters and the anchor depth in `ServerData`.
- Baking the server's data from more than one project.
- `Simulation` finding its own data by itself.

## Left open

- Unit statistics, waves and scenarios, which join as sections in M4 and M6.
- Whether a Worlds server takes new server data during a season (ADR-008, *Left open*).
- Compressing the file. At the MVP's size there is no reason to.
