# ADR-013 — The simulation's tick, its numbers and its randomness

**Status:** Proposed, 2026-11-01. It settles what [ADR-002](ADR-002-authoritative-simulation.md) leaves open on the tick rate and the number representation, and it is the decision on a deterministic core that AGENTS.md R16 asks for.
- **The owner gave the go-ahead** on 2026-11-01 ([MvpPlan.md](../MvpPlan.md), M2, input 1; GameDesign.md, *M2's values*).
- **The commit that first implements it** marks it Accepted (AGENTS.md §6).

Nothing implements it yet.

## Context

- **ADR-002, decision 6:** the simulation advances in fixed ticks, wall-clock time maps to ticks at one seam, and within one build the same state and the same commands give the same result. Its *Left open* names the tick rate and the number representation, and says floating point is not ruled out, because nothing is promised across builds.
- **AGENTS.md R16** sets the rules inside a deterministic core: no `float` where an integer or fixed-point quantity will do, no unordered iteration that reaches the outcome, no wall-clock time, and a pinned PRNG with a recorded seed. It asks for the core to be named in an ADR.
- **The same build does not give the same float results everywhere.** Microsoft's documentation of the UCRT says so directly ([Math and floating-point support](https://learn.microsoft.com/cpp/c-runtime-library/floating-point-support); [`_set_FMA3_enable`](https://learn.microsoft.com/cpp/c-runtime-library/reference/get-fma3-enable-set-fma3-enable)):
  - The CRT picks among several implementations of a math function at run time, by the CPU's instruction sets, and the results "may vary across CPUs".
  - The UCRT is part of Windows, so the same function can give different results on different versions of the operating system.

  `Build/Earthrise.props` links the CRT as a DLL (`MultiThreadedDLL`, and `MultiThreadedDebugDLL` in Debug), so both apply to this build.
- **Replays leave the machine that made them.** The playtests (M5, M7) log what each player did. The owner replays those logs on another machine, which may run another Windows build, and a playtester may use the ARM64 Surface while the owner replays on x64.
- **M1's simulation code is already integer:** voxel coordinates, the chunk store, the support search and the navigation tiles. Floating point appears in `Simulation` twice: `TestWorld::CastleXMeters()` and `CastleZMeters()`, which tell the renderer spike where to point its camera, and `TestWorld`'s integer square root, which starts from a `double` guess and corrects it to the exact root. `Protocol` has none.
- **There is no pinned PRNG in `Core` yet.** SplitMix64 is written out three times: in `Simulation/TestWorld.cpp`, `RendererSpike/Debris.cpp` and `RendererSpike/SceneScript.cpp`. GameDesign.md's Proposed wave draw names it too (*the paper tests' values*, item 21).
- **The tick of [MvpPlan.md](../MvpPlan.md)** runs in a fixed order, and its deferred work is budgeted "in units of work, never in elapsed time".

## Decision

1. **The simulation runs at 20 ticks a second.**
   - A tick stands for 50 ms of game time. Durations inside the simulation are counted in ticks and named so: `warningTicks`, `reloadTicks` (R6).
   - A rate the design states per second converts once, through `TICKS_PER_SECOND`, into a per-tick quantity. Where it does not divide evenly, the per-tick quantity is a fixed-point value (decision 3), never a fraction recomputed each tick.
   - **Why 20:**
     - A command takes effect on the tick after it arrives (ADR-002), so its effect comes 50–100 ms after the click. The client acknowledges the click at once (ADR-002, *Consequences*), which covers that wait.
     - A shot at 50 m/s moves 2.5 m a tick. It is traced through every voxel on its way (MvpPlan.md, *One simulation tick*, step 4), so a faster tick buys no accuracy.
     - A 50 ms tick leaves room for a support search and a tile rebuild on the same tick as the edit that caused them. M1 measures their cost (ADR-006's table), and M2 checks the whole tick against the budget in decision 6.
2. **Wall time meets ticks at one seam, outside the simulation.**
   - The executable, or the headless runner in a test, holds a fixed-step accumulator and calls the tick. The simulation reads no clock.
   - A tick is never skipped, merged or shortened. When a machine cannot keep up, game time slows down; the simulation never thins its work to keep pace.
3. **The simulation holds no floating point.** `Simulation` and `Protocol` use no `float` or `double`: not in state, rules, commands, state updates or saves.
   - **Voxels:** voxel coordinates are 32-bit integers counted in world voxels of 1 m, as M1 has them.
   - **Positions and velocities** of units and shots are `Core::Fixed`: a 32-bit signed count of 1/65536 m (Q16.16). Its range is ±32,768 m, sixteen times the largest world, about 2 km across. Its step is about 0.015 mm. A unit at 1.4 m/s moves 0.07 m a tick, which Q16.16 holds to about 0.01 %.
   - **Products** go through 64-bit intermediates, and a squared distance is a 64-bit integer. `Core` provides `Fixed`'s arithmetic, an integer square root, and a rounding rule stated once: a fixed-point product or quotient rounds toward negative infinity.
   - **Directions** are integer vectors. The simulation uses no angles and no trigonometry. Facings are 90° steps for structures (ADR-005, decision 10) and one of eight directions for units.
   - **Names keep their units:** `xMeters` of type `Core::Fixed`, `speedMetersPerTick` (R6). The type says how the number is stored, and the name says what it counts.
   - `SimulationBench` is a separate project that times the simulation, and it may use `double` for its timings.
4. **One pinned PRNG, in `Core`: SplitMix64**, with the constants of its reference implementation (Steele, Lea and Flood, *Fast splittable pseudorandom number generators*, OOPSLA 2014).
   - **Draws are keyed, not sequential.** A draw mixes the seed with a salt naming its purpose and whatever identifies it: an entity id, a tick, a chunk coordinate. Adding a draw in one system therefore never shifts another system's draws, and the order in which systems run cannot reach a result through the PRNG.
   - **Three seeds:**
     - the world seed and the resource seed of [ADR-003](ADR-003-world-as-seed-plus-edits.md);
     - a match seed for the simulation's own draws, such as the Hold wave's draw.

     All three are recorded in saves and replays.
   - **One mixer for the whole tree.** The three local copies in `TestWorld.cpp`, `Debris.cpp` and `SceneScript.cpp` are replaced by the `Core` one. `CoreTests` pins it to outputs computed from the reference implementation.
   - `std::random_device`, `<random>`'s engines and distributions, and any hash of an address stay out of `Simulation`, as R16 says.
5. **Order is fixed.**
   - Entity ids are given out in rising order from 1 within a world, are never reused, and are saved.
   - Every pass over entities in a tick goes in id order, unless a step's own rule states another order: collapses that start on the same tick go in entity-id order (ADR-004), and slump in the order its own step states, which decision 6 requires of all deferred work.
   - Containers whose iteration reaches a result are ordered, such as `std::map` or a sorted `std::vector`, as `ChunkStore` already does.
6. **Deferred work is counted in units of work. A wall-time target is measured against them, never enforced by them.**
   - **Each kind of deferred work** has a budget per tick in its own unit, for example voxels slumped or path nodes expanded. Work over the budget waits for the next tick, in a stated order.
   - **The budgets are server data** ([ADR-016](ADR-016-server-data-format.md)). They change outcomes, how fast a slope settles for example, so the server-data hash covers them (ADR-008, decision 5).
   - **The wall-time target** is how long a tick may take at the 95th percentile on the reference machine. The owner sets it, and `SimulationBench` measures against it. It is never read inside a tick. The owner set it at 25 ms on 2026-11-01 (GameDesign.md, M2's values, item 2). Queries are budgeted the same way, in units of work ([ADR-009](ADR-009-queries-across-the-server-boundary.md), amended).
7. **M2 runs the tick on one thread.** ADR-002 allows work spread over threads, merged in a fixed order. It is added when a measurement calls for it, not before.

## Consequences for the code

- **`Core` gains** `Fixed`, the integer square root, SplitMix64 with its keyed draws, and the byte writer and reader that commands, saves and `ServerData` share (ADR-014, ADR-016). Each comes with tests.
- **`Build/CheckProjectFiles.py` gains a check** that fails on the tokens `float` and `double` in `Simulation/` and `Protocol/`, outside comments and strings, with a broken fixture that shows it firing (AGENTS.md §1, *Enforcement*). R16's rule then binds by check instead of by review. On acceptance, R16's example of a fixed-point quantity, integer hundredths named in the identifier, gains `Core::Fixed` beside it, whose type carries the representation while the name carries the unit.
- **`TestWorld`'s camera accessors** return voxels or `Core::Fixed`, and the renderer spike converts them to `float` on its own side.
- **The client converts.** `Client` and `Engine` work in `float`, and they convert `Core::Fixed` at the boundary, the one place a simulation number becomes a float. A float never travels back into a command.
- **Integer results are identical on x64 and ARM64,** because they leave nothing to the instruction set or the CRT. A replay recorded on one architecture therefore has a good chance of reproducing on the other. ADR-002 still promises reproduction only within one build, and nothing here widens that promise. If the owner wants it widened, a test that replays an ARM64 log on x64 has to come first.

## What this forecloses

- `float` and `double` anywhere in `Simulation` or `Protocol`, and with them CRT math functions in the simulation.
- Clocks inside the simulation, and work cut short because time ran out.
- Skipping or merging ticks to catch up.
- More than one PRNG, sequential draws whose order varies, and `<random>` in the simulation.
- Entity ids that are reused or that depend on allocation order.

## Left open

- A different tick rate for Worlds, where a server may run many worlds at once.
- The client's interpolation between ticks ([ADR-002](ADR-002-authoritative-simulation.md), *Left open*).
- How the simulation uses more than one core.
- A replay promise that crosses architectures, as above.
