# Earthrise.World — M4.1 work orders

**Status:** Proposed, 2026-11-02, with [MvpPlan.md](MvpPlan.md)'s M4.1, which is accepted when the owner merges its pull request. The owner added M4.1 on 2026-11-02, while M4 was underway, so that the question below is settled after M4 rather than left until online work. The owner answered its two inputs the same day, taking each proposal (GameDesign.md, *M4.1's values*): the verdict's marks are 10% and 25%, and M4.1 runs after M4 closes, beside M5, unless M4-J4's measured run finds a tick of play over 25 ms. The owner also had A1 break down the simulation's load, as a measurement only, with no mark and no fix (GameDesign.md, *M4.1's values*, item 3). "Input N" means MvpPlan.md, M4.1, input N.

**M4.1 is off the critical path.** It changes `SimulationBench` alone and no simulation code, and nothing from M5 on waits on it.

**An order is one commit.** Phase A is one pull request: A1, then the owner's runs, then A2. Phase B exists only if A2's verdict calls for it, and its orders are written then. A session works an order as [MvpPlan-M4.md](MvpPlan-M4.md), *How a session works an order*, says, and its commit's subject begins with the order's id, as in `M4.1-A1: Time a voxel read in stores of every size`. Phase A adds no file to any project, no test, no saved state and no ADR, so that section's items 6 to 9 have nothing to do.

| Phase | Orders | What lands | Waits on |
|---|---|---|---|
| A | A1, A2 | The bench's lookup mode and the load's breakdown; the owner's runs; the report and its verdict | Inputs 1 and 2, and M4-B4 |
| B | Written then | A faster chunk lookup | A2's verdict calling for one |

## Why

M1 measured two costs that ought not to depend on the size of the world, and both grew with it ([Review/M1Report-2026-11-01.md](Review/M1Report-2026-11-01.md); the last row is arithmetic on the measured totals):

| 1 m world, Release x64, benchmark machine | MVP map, 258 chunks | 2 km season world, 4,224 chunks |
|---|---|---|
| Support search after the breach, median | 1.97 ms | 3.72 ms |
| One 32 m navigation tile, median | 1.32 ms | 3.30 ms |
| Every navigation tile, per tile | 427 ms / 256 = 1.67 ms | 16,818 ms / 4,096 = 4.11 ms |

Both are local by design. The support search covers only the region an edit can affect ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 2), and a tile rebuild reads one chunk column and the cells at its border. Two explanations fit, and they call for different things:

1. **The work differs.** The season world's breach may read more voxels than the MVP map's, and its tiles may hold more to label. The bench already prints the voxels each search reads (`found.voxelsSearched` in `Measure`), but the M1 report kept only the MVP map's, 46,342.
2. **Each read costs more in a bigger store.** Every voxel the simulation reads goes through `ChunkStore::At` (`Simulation/ChunkStore.cpp`). It packs the chunk key, finds it in a `std::map` of every chunk the store holds, follows the entry's `shared_ptr` to the chunk, and reads the voxel through `Protocol::Chunk::At`'s bounds check. A red-black tree of 258 chunks is about 8 levels deep and one of 4,224 about 12, each level a separate allocation. The support search reads every voxel through that path (`Simulation/SupportSearch.cpp`, its one `At`), and so does the navigation rebuild.

The MVP map holds at most 512 chunks, and M2's breach run fits its tick with room to spare, so neither explanation matters to the MVP. If the second holds, though, every local cost in the simulation grows with the chunk count, and any figure for a larger world extrapolated from M1's is low. Phase A finds out which explanation holds, and by how much. It fixes nothing.

**The load, while the bench is open.** A load regenerates every chunk and rebuilds every navigation tile, since a save holds no derived data ([ADR-014](ADR/ADR-014-edits-saves-and-replays.md), decision 4). On the 2 km season world the rebuild alone took 16.8 s. On the MVP map it took 427 ms on the benchmark machine, and the generated map's rebuild 209 ms in the Linux container with clang 18 at -O2 ([ADR-015](ADR/ADR-015-navigation.md)). The generator has never been timed, and neither has a scenario's load. A1 times each step on both worlds and on the Siege trial, so that the load has measured figures before anyone sizes it for a larger world. The figures carry no mark and call for no fix: the remedies, rebuilding tiles in parallel, building them lazily, or saving them, are Worlds' work, and the last needs ADR-014 amended. They also share a cause with the lookup's question, since a tile's rebuild took 1.67 ms on the MVP map and 4.11 ms at 2 km.

## The orders

### M4.1-A1 — The bench's lookup mode

- **Waits on:** input 2, and M4-B4, whose `LoadScenario` part 4 times.
- **Read first:**
  - `Simulation/ChunkStore.h` and `.cpp`: `At`; `SetChunk`, which keeps a chunk absent when it holds nothing; `Find` and `Keys`.
  - `Simulation/SupportSearch.cpp`, `FindUnsupported`, and `Simulation/NavigationGrid.h`, `RebuildTile`, `RebuildAll` and `UpdateReach`.
  - `Simulation/Generator.h`; `Simulation/Scenario.h`, `ScenarioDesc` and `LoadScenario`; and `Simulation/World.h`, what its constructor and its first tick do.
  - `SimulationBench/SimulationBench.cpp`: `Measure`, `Run`, and how `--breach-run` takes its CSV.
  - [Review/M1Report-2026-11-01.md](Review/M1Report-2026-11-01.md), its table, and [ADR-006](ADR/ADR-006-voxel-sizes.md)'s measurements.
- **Changes:** `SimulationBench.cpp` gains a mode, `SimulationBench --lookup [file.csv] [--chunks N]`, beside `--breach-run`, `--integrity` and `--lab-items`, and named in the comment above them. It times with the wall clock, as the bench's other modes do, and its figures mean something only from a Release build. It measures four things.
  1. **The work, on both worlds.** It builds the MVP test world and the 2 km season world as `Run` does, and runs the M1 breach's support search on each, 20 times. For each world it prints the voxels read, the median and slowest times, and the median divided by the reads, in nanoseconds a read. It also prints, for each world, the one-tile rebuild of the tile `Measure` takes, and how many solid voxels that tile's chunk column holds. This alone may settle the question: if the season world's search reads about twice the voxels, explanation 1 holds.
  2. **The same work in a bigger store.** On the MVP test world, it adds filler chunks until the store holds N chunks, for N of 258 (no filler), 1,024, 4,224 (the 2 km world's count), 16,384 (about 4 km) and 65,536 (about 8 km, one chunk of height). At each N it times the same breach's support search and the same tile's rebuild again, 20 times each. The filler:
     - is solid, of one material, so that `SetChunk` keeps it;
     - lies at chunk x of 4,096 and beyond, far outside any region the search or the rebuild reads, and inside `Protocol::FitsChunkKey`;
     - is added in an order the bench fixes, so two runs build the same tree.

     **The reads must not change with N.** At every N the bench checks that `voxelsSearched` and the pieces found are what they were at 258, and that the rebuilt tile's cells are the same, by whatever `NavigationGrid` exposes. It exits non-zero if any differ. Whatever changes in the time is then the store's size and nothing else. At 65,536 chunks the store holds about 2 GiB (65,536 × 32 KiB, arithmetic on the chunk's size), so the bench prints each step as it starts, and a run that runs out of memory shows where it stopped.
  3. **The read path itself.** At each N, the bench reads every voxel of the breach search's box, x fastest, then z, then y, in three ways, and prints nanoseconds a read for each. The box is the collapse's bounds widened by `reach`, as `Measure` computes it. The three ways:
     - through `ChunkStore::At`, as the simulation reads;
     - through a table the bench builds for the box alone, holding the `const Protocol::Chunk*` that `Find` returns for each chunk coordinate in the box, then `Chunk::At`, which is roughly what a flat chunk index would cost;
     - through the same table, reading the chunk's `Voxels()` span by index without a bounds check, which is the floor.

     The bench prints the sum of the palette indices read, so the compiler cannot drop the loops. The gap between the first way and the second is the lookup; the gap between the second and the third is the bounds check and the index arithmetic.
  4. **The load, step by step.** Each step runs five times, each time on fresh objects, and the bench prints its median and slowest:
     - **The generator alone:** every chunk of the generated MVP map into a fresh `ChunkStore`, as `GenerateMvpMap` does, and the same generator over 2,048 × 64 × 2,048 m. The land need not look right at that size, since only its time matters. For each, the total, the chunks generated, the chunks the store kept, and the time per chunk generated.
     - **Navigation from nothing:** `NavigationGrid::RebuildAll` and then `UpdateReach`, each timed on its own, on the generated MVP map with the classes and rules `Run` gives its grid, and the same on the 2 km season world. For each, the total and the time per tile.
     - **The Siege trial to its first tick:** a `World` made from `ScenarioDesc` for `Siege`, then `LoadScenario`, then the first tick, each timed on its own. The output names what each step covers as the code does it, such as whether the construction or the first tick rebuilds navigation, so that no step is counted twice. Their sum is the simulation's share of the trial's load; the client's share, the voxel records and the first frame, is not measured, since the bench has no client.

  **`--chunks N`** runs parts 2 and 3 at that one N only, repeated 200 times, for a profiler. **The CSV**, when a file is named, has one row per world or N and per measure: the world or N, the measure, the median, the slowest, and the reads, empty for part 4's rows.

  No `Simulation` file changes. If a measurement needs an accessor `ChunkStore` lacks, the session stops and reports rather than adding one; `Find` and `Keys` should be enough.
- **Tests:** none new: the mode is a measurement. The commit builds in CI (Debug|x64), every suite passes unchanged, and `Build/CheckProjectFiles.py`, `Build/CheckFormat.py` and `Build/RunClangTidy.py` pass. CI does not run the bench.
- **Owner runs:** on the benchmark machine, from a Release|x64 build of the order's commit, on mains power, three runs one after another, as M2's breach run was taken:

  ```powershell
  x64\Release\SimulationBench.exe --lookup lookup.csv
  ```

  Hand back the printed summary lines and the CSVs, and the machine's free memory if the 65,536 step did not finish. A run takes the 2 km season world's full rebuild five times, about 16.8 s each by M1's figure, so it lasts a few minutes. **Only if part 2's support search is slower at 4,224 chunks than at 258 by input 1's upper mark,** one more run under Windows Performance Recorder, which ships with Windows and is a tool, not a dependency (R14):

  ```powershell
  wpr -start CPU
  x64\Release\SimulationBench.exe --lookup --chunks 4224
  wpr -stop lookup.etl
  ```

  Opened in Windows Performance Analyzer, it gives the share of `FindUnsupported`'s samples spent in `std::map`'s find, in the rest of `ChunkStore::At`, in `Chunk::At`, and in the search's own code.

### M4.1-A2 — The report and its verdict

- **Waits on:** A1's owner runs, and input 1.
- **Changes:**
  - `Design/Review/ChunkLookup-<date>.md`: the three runs' figures, both worlds' reads, the profile if one was taken, the CPU and the build id, as M1's report gives them, and the verdict under input 1, stated as one of its three outcomes.
  - In the same report, the load's breakdown from part 4 for both worlds and the Siege trial, with no verdict, and how much of a tile's rebuild part 2 shows growing with the store's size.
  - MvpPlan.md, M4.1: its record, and, if the verdict calls for phase B, the phase's line in the table above, its orders written in this file under the constraints below.
  - ADR-006's table is M1's and does not change.
- **Tests:** none; the commit changes no code.

## Phase B, if the verdict calls for it (its constraints, not its orders)

- `Publish`, `Snapshots`, `Keys`, the state hash and the save all run in key order (R16; [ADR-014](ADR/ADR-014-edits-saves-and-replays.md), decisions 4 and 5), and their bytes must not change. The save test, the replay test and the generator's golden hashes pass unchanged. The ordered map therefore stays as the owner of the entries, and any faster lookup sits beside it.
- `std::map` never moves a node on insert or on erasing another, so an index of entry pointers stays valid until its own chunk is removed, and is updated when a chunk is added or removed.
- An index holds the entry, never the chunk: a `Set` after a publication swaps in a pending copy, so a cached chunk pointer would read stale voxels.
- The candidates, smallest first:
  - a reader for the hot loops that keeps the last entry it found, since a run of 32 reads along x stays in one chunk, so that `At` itself keeps no hidden state;
  - a flat index over the world's bounded box, which for an 8 km world 64 m high is 256 × 2 × 256 = 131,072 slots, 1 MiB of pointers (arithmetic);
  - both. An `unordered_map` would also do, for lookup alone, because R16 forbids its order reaching an outcome, not the container; but a bounded world makes the flat index simpler.
- **Done when** A1's part 2, run again, shows the support search at 4,224 chunks within input 1's lower mark of its time at 258.
