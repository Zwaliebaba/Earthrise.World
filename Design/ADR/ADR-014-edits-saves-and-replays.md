# ADR-014 — Edits, saves and replays

**Status:** Proposed, 2026-11-01. It closes [ADR-003](ADR-003-world-as-seed-plus-edits.md)'s open item on the edit encoding, and fixes the layout of the save that ADR-003 and [ADR-005](ADR-005-content-formats.md), decision 10, describe.
- **The owner gave the go-ahead** on 2026-11-01 ([MvpPlan.md](../MvpPlan.md), M2, input 1; GameDesign.md, *M2's values*).
- **The commit that first implements it** marks it Accepted (AGENTS.md §6).

Nothing implements it yet.

## Context

- **ADR-003:** the unedited world is a pure function of two seeds, the generator version and the chunk coordinate. Every change is an edit stored per chunk against that base, and a chunk nobody has touched is not stored. Its *Left open* asks whether a chunk's edits are a list of changed voxels or a full copy past some threshold, and leaves compression open.
- **ADR-002:**
  - Commands are bytes from the first line, and their encoding is the replay format (decision 3).
  - A replay reproduces within one build (decision 6), and [ADR-008](ADR-008-server-and-client-game-data.md) adds "and one set of server data".
- **ADR-004, decision 4:** a collapse is an entity that lives across ticks, so saves carry collapses in progress.
- **ADR-005, decision 10:** a structure is saved as its template, origin, rotation and built mask, and entity state is a versioned binary written with the commands' encoder.
- **M1's `ChunkStore`** holds whole chunks of 32³ palette indices, copied on write. A whole chunk is 32 KiB, by the size of `Protocol::Chunk`. The MVP map of 512 m × 512 m with two chunks of height is at most 512 chunks, 16 MiB, so M2 keeps the whole map in memory, as GameDesign.md's MVP scope says.
- **The MVP's scenarios** are the Siege castle and the Hold maps. GameDesign.md says "A scenario is a save: the seeds, the edits and the entities" (MVP, *In*, a Proposed line), and MvpPlan.md says the same.
- **The playtests** log what happened, and the owner replays the logs (M5, M7).

## Decision

1. **The world is edited through one call.** Only the generator, filling the base, writes the chunk store directly. Everything else goes through one call that changes a voxel and records it, in two places:
   - The chunk joins the world's set of edited chunks. The set lives as long as the world and is saved.
   - The voxel joins the tick's edit list. That list feeds the support search, slump and the navigation rebuild, and the chunk store publishes the tick's changes as snapshots (ADR-002, decision 3).
2. **In memory the world is whole chunks.** The generator fills them on load, and edits change them in place, copied on write as M1 does. The edit encoding below is only for saves and, later, the network.
3. **A chunk's edits are written as whichever of two forms is shorter.** On save, each edited chunk is compared with its base, regenerated, and written in one of these forms, with ties going to the list. The rule needs no tuned threshold, so there is nothing to retune when voxel content changes.
   - **A list of changed voxels:** a `u16` count, then for each voxel its `u16` index in the chunk (x fastest, then z, then y, as `Protocol::Chunk` lays them out) and its `u8` palette index, in rising index order.
   - **The whole chunk, run-length encoded:** runs of a `u16` length and a `u8` palette index, covering the chunk in the same order.

   A chunk that is back to its base, dug and then refilled for instance, is not written. Nothing else is compressed: R14 rules out a compression library, and the MVP's saves are small.
4. **A save holds everything the next tick reads, and nothing it can rebuild.** The test of a save is this: run N ticks, save, load, and run M more. The world must then hash the same as a run of N + M ticks without the save. The save is little-endian, and each section's records are written with `Core`'s byte writer, the encoder commands use. It has four sections, in this order:
   1. **The header.** It holds:
      - a magic number and the save format's version;
      - the build id (decision 6), the server-data hash (ADR-008) and the generator version;
      - the world, resource and match seeds ([ADR-013](ADR-013-ticks-numbers-and-randomness.md), decision 4);
      - the tick, and the next entity id.
   2. **The edited chunks:** each as its packed chunk key and one form from decision 3, in rising key order.
   3. **The entities:** every entity in id order. Each is written as its type tag, its record's version, its length and the record. Structures, units, squads, shots and collapses in progress are all entities.
   4. **The pending work:** every queue that carries work into the next tick, each in its own stated order. That covers slump not yet done, path searches part way through, and commands that have arrived for a later tick.

   Navigation tiles, chunk hashes and anything else computed from the voxels are not saved. They are rebuilt on load, and the test above proves that the rebuild gives what the uninterrupted run had.
5. **A replay is a starting point and the commands.** It is written as follows:
   - **Its start:** a save, or a scenario (decision 7).
   - **Its commands:** each tick's commands as the transport carried them, keyed by tick.
   - **A state hash every 20 ticks,** once a game second. When a replay goes wrong, the hash shows where within a second, instead of only at the end.

   The state hash is FNV-1a 64 (`Core::Fnv1a64`) over the chunk keys and their contents in key order, then over the entities and pending work as a save writes them. Each chunk's hash is cached and recomputed only after an edit, so the hash costs little more than the entity bytes.
6. **A save or a replay refuses to run against anything but its own build, its own server data and its own generator version.**
   - **The build id** is FNV-1a 64 over the executable's own file. The executable computes it at startup and passes it to the simulation, so it needs no build step and no version control. It is stricter than the source: two builds of the same commit differ, and so do Debug and Release, as ADR-002 allows.
   - **A test** passes its own id, and records and replays inside one process.
   - **A mismatch is refused with a message saying which check failed.** It is never loaded on a best effort.
7. **A scenario is not a save.**
   - **Why:** a save is tied to its build by decision 6, so a scenario written as a save would be refused by the next build of the game.
   - **What a scenario is:** a source in `GameData/Server/Scenarios/`, baked into `ServerData` ([ADR-016](ADR-016-server-data-format.md)), and covered by the server-data hash and nothing else. It states:
     - the seeds;
     - the structures as ADR-005, decision 10, saves them;
     - the earthworks as volumes dug or raised;
     - the entities at rest.

     It holds no voxel diffs, no tick and no work in flight.
   - **How it loads:** on the generated world, as tick 0. A scenario therefore survives a new build, and while the generator may still change freely (ADR-003, decision 4), it also survives a change of generator version.

## Consequences for the code

- **`Simulation` gains** the editing call, the edited-chunk set and the tick's edit list. M1's `TestWorld` and `TestWorldEdits` build the spike's and the bench's stand-in world outside any simulation, so they keep writing their `ChunkStore` directly until the generator and the M2 scenario replace them.
- **CI runs three tests:**
  - the save test of decision 4 on a scripted scenario;
  - a replay test that records the M2 scenario and replays it inside the same test, comparing every state hash;
  - a round trip of each chunk form, with a chunk that comes back to its base.
- **Each executable that saves or replays computes its build id** at startup: `SimulationBench` in M2, `RendererSpike` if it ever does, and `Earthrise` when it joins in M4.
- **GameDesign.md's line "A scenario is a save" changed** on 2026-11-01, with the owner's go-ahead, because decision 7 departs from it. It now reads that a scenario holds the seeds, the structures and earthworks placed on the generated world, and the entities, and that a save holds the same and the work in flight. What a scenario holds is unchanged, but its form differs from a save's.
- **A replay's size grows with the commands, not the world.** A save's edited chunks are bounded above by 3 bytes a changed voxel (decision 3). GameDesign.md puts a whole season of a 2 km world at about a million edits, which bounds its edits at about 3 MB. That figure is arithmetic on the format, not a measurement.

## What this forecloses

- A save of the whole world, voxel by voxel.
- Derived data in a save, such as navigation tiles or render records.
- A save or replay that loads across builds, server data or generator versions.
- A tuned threshold between the list and the whole chunk.
- A compression library.
- Scenarios written as voxel diffs or tied to a build.

## Left open

- Edits over the network. ADR-003 sends them per chunk; whether the forms of decision 3 are also the wire forms is decided with online work.
- Compression, if a measured save ever needs it.
- Saves that survive a new build. The MVP refuses them; Worlds needs them, and decides how with its generator versioning (ADR-003, decision 4).
- How often the game saves on its own.
