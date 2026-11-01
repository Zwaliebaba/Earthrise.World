# ADR-025 — Fog of war: visibility by radius, a filtered state, chunks sent when seen, and last-seen land

**Status:** Proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). It fills the seam [ADR-020](ADR-020-the-in-process-client.md), decision 3, leaves for M4, `World::PublishState(player)`, and decides how the client draws what it no longer sees, which [ADR-021](ADR-021-world-overlays.md), decision 6, leaves for M4. The radii are the owner's inputs (MvpPlan.md, M4, inputs 6 and 14). The owner gave it the go-ahead on 2026-11-01, as written (MvpPlan.md, M4, input 1; GameDesign.md, *M4's values*, item 1). Order M4-E1, which first implements it, marks it Accepted (AGENTS.md §6).

## Context

- **[ADR-002](ADR-002-authoritative-simulation.md), decision 4:** a client receives only what its player can see; fog is enforced by the simulation, not painted by the client. It forecloses a client that holds world state its player cannot see.
- **The design** (GameDesign.md, *Where RTS logic meets destruction* and *MVP*; GameDescription.md, *Readability*): vision is a radius and is not blocked by walls; an engine sees as far as it shoots; ground the player is not watching shows the land as they last saw it, so scouting reveals new quarries, ditches and breaches.
- **[ADR-009](ADR-009-queries-across-the-server-boundary.md), decision 4:** an answer passes the same filter, and an answer about ground the player has not seen says so.
- **Chunks cross as copy-on-write snapshots** (ADR-002, decision 3), published when they change (`World::Publish`); the client keeps its own copy of what it was told ([ADR-020](ADR-020-the-in-process-client.md), decision 2).
- **The map is 512 × 512 cells of 1 m,** in 16 × 16 chunk columns of 32 m, each 2 chunks high.
- **Hidden resources** ([ADR-003](ADR-003-world-as-seed-plus-edits.md)): where stone lies is kept on the server. That is Hold's question (M6's YIELD); the Siege trial digs only trenches.

## Decision

1. **Visibility is a bit per player per cell, recomputed every tick in step 11.** A cell is visible to a player when it lies within the vision radius of any of the player's units that is not garrisoned, measured from the unit's cell in whole cells by squared distance, or within a structure's vision of its footprint, its garrisoned vision while it holds a garrison (ADR-024, decision 7). Walls do not block it. The trebuchet's vision is its range, as GameDesign.md says of engines.
2. **The state update is filtered through it.** A player's update holds their own entities, and of the others: units, squads, engines and shots whose cell is visible; collapses whose piece or strip touches a visible cell; structures they have ever seen, each as it stood when last visible, marked `lastSeen` while not visible now. Garrisons are counted only for the owner. The update carries the player's visibility as a bitset over the map's cells, 32 KiB, copied a tick as the rest of the update is (ADR-020, decision 3).
3. **Chunks are sent when seen.** A player is sent a chunk's current snapshot on every tick on which any cell of its column is visible and its version differs from the last one sent to that player; the simulation keeps, per player, the version last sent of each chunk. A chunk changed while unseen therefore reaches the client the tick it is seen again, and not before.
4. **The client keeps what it was last sent.** `Client::Session` holds the newest snapshot of each chunk it has received, and draws those; that is last-seen land, with nothing reconstructed on the client.
5. **The scenario may reveal the map at the start.** For a player it reveals, every cell counts as visible for the first `revealTicks`, for chunks, structures and answers, never for units or garrisons: the player is sent every snapshot and every structure once, and the readings the client asks for in that window are answered, so the integrity view starts with every section's reading, kept as last seen. The Siege trial reveals the map to the attacker (MvpPlan.md, M4, input 14), who starts with the castle's plan and its readings and none of its garrison.
6. **Answers are filtered.** A query about a structure, a tile or a target not visible to the asker at the tick of its answer is answered with `notVisible` set and its reading empty; the client keeps the last reading it had and marks it as last seen.
7. **The AI sees through its own fog** ([ADR-027](ADR-027-commanders.md)): it reads only what its player's filter passes.
8. **Fog is drawn as a shade on the columns not visible now** ([ADR-021](ADR-021-world-overlays.md), amended): the client uploads the visibility as an `R8` texture of one texel per cell, and the voxel pass multiplies a world voxel's colour by a shade where its column's texel is 0. Models are drawn only where the update put them, so none stand in fog.

## Consequences for the code

- **`Simulation`** gains `Visibility`, per player, and `PublishState(player)` fills the filter; `Publish()` becomes `Publish(player)`, with the versions sent kept per player. Visibility is derived data: saves leave it out and loads rebuild it, except the versions sent and the structures last seen, which a load needs to filter the same way and which are saved as pending work ([ADR-014](ADR-014-edits-saves-and-replays.md), decision 4).
- **`Protocol::StateUpdate`** gains the visibility bitset, `StructureView::lastSeen`, and `notVisible` on answers.
- **`Engine::VoxelPass`** gains a column-shade texture, generic to the engine: a multiplier per world column, which the game uses for fog (R9). A render test on WARP checks a shaded and an unshaded column.
- **Cost:** about 130 units and 30 structures stamp their circles each tick, about 150,000 cell writes at the trial's radii (arithmetic on the inputs, not a measurement); M4-E1 measures it in `SimulationBench --siege`.

## What this forecloses

- A client that draws a unit or a change it was not sent.
- Fog painted by the client over a world it fully knows.
- Line-of-sight vision: walls never block sight in the MVP.

## Left open

- **Hiding voxels within a seen chunk.** A seen chunk is sent whole, so a client holds its buried stone; Hold's YIELD rule (Hud.md §10) needs voxels hidden below what was seen, which M6 decides.
- **Sending columns rather than chunks,** if a network ever makes 32 m too coarse.
- **The shade's value,** judged on screen.
