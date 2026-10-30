# ADR-003 — The world is a seed plus the edits made to it

**Status:** Proposed, 2026-10-30, revised the same day after a design review (a second seed for resources, and the slope invariant). Awaiting the owner's acceptance. Nothing implements it yet.

## Context

The world is generated procedurally. Every voxel in it can be removed or added, by gathering, building, siege and collapse ([GameDesign.md](../GameDesign.md), [ADR-004](ADR-004-rule-based-destruction.md)). The world has to be saved in single-player. Later a server shares it with clients that each see only part of it ([ADR-002](ADR-002-authoritative-simulation.md)). Storing or sending every voxel scales with the world's volume, while what players actually change is a small, local fraction of it.

## Decision

1. **The unedited world is a pure function of two seeds, the generator version and the chunk coordinate.** The world seed decides the shape of the terrain. The resource seed decides where resources lie, such as ore veins and deposits of building stone, and only the simulation and its saves hold it. A chunk is a fixed-size block of the world. Generating the same chunk twice, on any machine and with any build, gives the same voxels.
2. **The generator is integer-only.** It uses no `float` or `double` anywhere: noise, slopes, placement and every random choice use integer or fixed-point arithmetic (R16). Randomness comes from a pinned PRNG keyed on the seeds and the chunk coordinate, never on the order in which chunks are generated. With floating point, a Debug and a Release build, or a client and a server, could generate different terrain from the same seed, because `/arch:AVX2` lets the compiler contract arithmetic differently (R16). Nothing downstream could detect that except as a mismatch.
3. **Every change to the world is an edit, stored per chunk against the generated base.** A chunk nobody has touched is not stored. A save is the two seeds, the generator version, the edited chunks and the simulation's entity state. Terrain crosses the network the same way: a client generates the base from the world seed alone, with resource voxels filled in as the rock around them, and receives as edits both the changes to the chunks its player can see and the resource voxels in those chunks that are exposed to the air (ADR-002, decision 4).
4. **A world records its generator version, and that version's output never changes.** Changing what the generator produces makes a new version, and worlds made with an old version keep generating with it. The rule binds from the first world that has to survive an update. Before then, there is one version and it may change freely.

## Consequences for the code

- **Golden hashes in CI.** CI hashes generated chunks for fixed seeds and compares them against committed values. An unintended change to the generator's output fails the build. An intended one comes with a new generator version and new hashes.
- **Release must match Debug.** CI builds Debug only (AGENTS.md §6), so the golden hashes are a Debug result. A Release build must produce the same hashes, and whoever builds Release checks that. A mismatch is a bug in the generator, not a tolerance.
- **Earth lies at or below its slump slope.** The generator emits earth only where the ground is no steeper than the slope at which earth slumps ([GameDesign.md](../GameDesign.md)); steeper ground is stone. Otherwise the first edit near steep generated earth would start a slide. CI checks this as an invariant over the same fixed seeds as the golden hashes.
- **World-scale features come from the seeds alone.** Features larger than a chunk, such as rivers, mountain ranges and start positions, come from a world-scale layer computed from the world seed, and resource distribution from one computed from the resource seed. Chunk generation reads both. A chunk never depends on another chunk having been generated first.
- **Old generator versions stay in the code** for as long as a world made with them exists. That is a maintenance cost, and it is why the version changes deliberately, not with every tweak.
- **The shape of the terrain is not secret; where resources lie is.** A modified client can generate terrain its player has not explored, but it cannot find resources that way, because the resource seed never reaches a client. What players have built and changed stays secret too, because edits are sent only for chunks the player can see. Unexplored terrain is hidden by the client's presentation, not by the simulation, and that is accepted here.

## What this forecloses

- Storing or transmitting the whole world voxel by voxel.
- Floating point in the generator, float noise functions included.
- A generator change that silently alters existing worlds.
- Generation that depends on the order in which chunks are generated.
- A client that can compute where resources lie.

## Left open

- Chunk dimensions, which depend on the simulation voxel size the renderer spike settles.
- The edit encoding: a list of changed voxels, or a full copy of a chunk once it has been edited past some threshold. Also compression.
- The format of the entity state in a save.
- The world's extent, and whether it is bounded at all.
- Streaming chunks in and out of memory, which [ADR-001](ADR-001-direct-voxel-ray-casting.md) also leaves open.
