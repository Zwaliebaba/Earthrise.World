# ADR-009 — Queries cross the server boundary, read-only and filtered

**Status:** Accepted, 2026-10-31, by the owner, from the MVP readiness review ([Review/MvpReadiness-2026-10-31.md](../Review/MvpReadiness-2026-10-31.md), B5). It amends [ADR-002](ADR-002-authoritative-simulation.md), decision 2. Like ADR-002 to ADR-008, it is written ahead of the code it governs; M3 builds the first queries. Amended 2026-11-01 by the owner (GameDesign.md, M2's values, item 15): decision 5's budget is counted in units of work, not time, as [ADR-013](ADR-013-ticks-numbers-and-randomness.md) requires of everything in the simulation.

## Context

- **ADR-002 lets only two kinds of message cross the boundary:** commands from the client to the simulation, and state updates back (decision 2).
- **The HUD shows readouts only the simulation can answer** ([Hud.md](../Hud.md) §10):
  - whether ground can be undermined;
  - the slump preview;
  - a trench's target;
  - a dig's yield.

  [ADR-004](ADR-004-rule-based-destruction.md), decision 5, requires those to come from the collapse resolver, never from a local guess.
- **Where resources lie stays on the server** ([ADR-003](ADR-003-world-as-seed-plus-edits.md)), and a dig's yield counts only the stone the player can see (GameDesign.md, the MVP's HUD).
- **The breach lab (M3) is the first milestone that needs these readouts.**

## Decision

1. **A third kind of message: a query from the client, and the simulation's answer.** Commands, state updates, queries and answers are the only messages that cross the boundary.
2. **Queries are read-only.**
   - Answering one never changes the simulation's state.
   - Queries are not part of the command stream, and replays do not record them.
   - A readout shown while a replay plays is asked again.
3. **An answer belongs to a tick.**
   - It is computed on the state at the end of a tick, and carries that tick and the query's id.
   - The client keeps only the newest answer to each query.
4. **An answer reveals nothing the player could not see.** It passes the same filter as state updates: the fog filter of ADR-002, and the hidden resources of ADR-003.
   - A yield counts only voxels the player has seen.
   - An answer about ground the player has not seen says so, rather than guessing.
5. **Queries are bounded.**
   - The simulation answers them after the tick's own work, within a budget of queries a tick, counted in units of work, never in time (amended 2026-11-01).
   - When too many arrive, it drops the oldest.
   - A readout asks again as the pointer moves, so a dropped query costs a frame of staleness, never a wrong answer.
6. **Queries and answers are bytes,** written with the same encoder as commands (ADR-002, decision 3), over the same transport. That transport is in-process in single-player.

## Consequences for the code

- `Protocol` gains query and answer types beside the commands and state updates.
- The tick gains a last step: answer queries on the tick's state, filtered, within the budget left ([MvpPlan.md](../MvpPlan.md), *One simulation tick*).
- A HUD readout comes only from an answer, or from snapshots for plain sums such as area and cost.

## What this forecloses

- The client calling into `Simulation`, or reading its state.
- A query that changes state. Anything that changes state is a command.
- An answer that leaks hidden state, such as stone the player has not seen.

## Left open

- Which queries exist beyond Hud.md's list.
- How many queries a tick answers, set in M3 when the first queries exist.
