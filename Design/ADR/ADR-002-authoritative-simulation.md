# ADR-002 — The simulation is authoritative and sits behind a server boundary from the first line

**Status:** Proposed, 2026-10-30, revised the same day after a design review (decisions 3 and 6). Awaiting the owner's acceptance. Nothing implements it yet.

## Context

The long-term goal is a shared, persistent online world. The MVP is single-player against AI factions, with no networking ([GameDesign.md](../GameDesign.md)). Who owns the simulation has to be settled before the first line of gameplay code, because a network layer added later cannot change it. If the renderer reads world state directly, or the client decides an outcome, that code has to be rewritten; it cannot be wrapped.

Two models were considered.

**Deterministic lockstep** is the classic RTS model, and the one Age of Empires used (Bettner & Terrano, *1500 Archers on a 28.8: Network Programming in Age of Empires and Beyond*, GDC 2001). Every client runs the whole simulation from the same command stream, and only commands cross the network. It is cheap on bandwidth and suits a bounded match between a fixed set of players. It does not suit this game:

- Every client must simulate the entire world, which a shared persistent world rules out.
- A player cannot join without receiving the full state.
- The game runs at the pace of the slowest machine and connection.
- Every client holds the full world state, so fog of war is advisory and a map hack is trivial.
- Every gameplay system, physics included, must be bit-identical across machines and builds (R16).

**An authoritative simulation** owns the world. Clients send commands and receive what their player is allowed to see. It costs more bandwidth and server CPU, and it is what a shared world requires.

## Decision

1. **Game code is split into the simulation and the client.** The simulation owns the world state, the rules and the AI factions. The client owns input, presentation, rendering, audio and every effect that is only for show.
2. **Only messages cross the boundary.** Commands go from client to simulation, and state updates go from simulation to client. The client never holds a pointer or reference into simulation state, and the simulation never calls into the client.
3. **Commands cross the boundary as bytes from the first line; state updates do once online work starts.** In single-player the simulation runs in the same process and messages travel through an in-process transport. Commands are few and change rarely, and their encoding is the replay format decision 6 needs, so they are encoded and decoded exactly as they will be over a network from the start. State updates change with every iteration while the MVP looks for its fun, so in-process they cross as value copies, never as a pointer or reference into simulation state (decision 2). Giving them a byte encoding is the first task of online work. Voxel chunks cross as immutable, versioned snapshots. In-process the simulation and the client share them copy-on-write, so single-player holds one copy of the world; over a network a snapshot travels as ADR-003's per-chunk edits. The boundary stays honest by construction rather than by discipline: the client project references a library of message types and cannot include the simulation's headers (AGENTS.md R9 and §2).
4. **A client receives only what its player can see.** Fog of war is enforced by the simulation, not painted by the client over a world it fully knows. The same filter later limits a client to its own area of a large world.
5. **AI factions issue the same commands a player does**, through the same interface. An AI can therefore take over a human player's faction, on disconnect or while they are offline, without a second code path.
6. **The simulation advances in fixed ticks.** Wall-clock time maps to ticks at one seam. The simulation follows R16's rules on ordering, time and randomness, so within one build the same state and the same commands produce the same result, and a recorded command stream replays for tests and debugging. Bit-identical results across builds, compilers or machines are not required. The simulation's result is authoritative, so nobody else has to reproduce it. A replay records the build that made it and is reproduced in that build; a replay recorded in Release need not reproduce in Debug (R16). Work spread over several threads computes from the state after a tick's edits and merges its results in a fixed order. Work deferred to a later tick is budgeted in units of work, never in elapsed time, so how much is deferred never depends on the machine.

## Consequences for the code

- The client draws from its own copy of what it was told, never from simulation state. In-process, its copy of the voxel world is the shared chunk snapshots (decision 3). If rendering needs a fact the messages do not carry, the messages change.
- Every command has an encoding and a round-trip test from the first line. Every state update gains both when online work starts.
- A command takes effect on a later tick, not in the frame it was issued, even in single-player. The client acknowledges a command immediately and visibly. Client-side prediction is not part of this decision.
- Saving a single-player game saves the simulation's state, not the client's.

## What this forecloses

- Deterministic lockstep as the network model.
- Any gameplay outcome decided on a client.
- A client that holds world state its player cannot see.
- Peer-to-peer play in which every peer simulates.

## Left open

- The transport, the protocol over it, the encoding and compression.
- The tick rate.
- Client-side prediction and interpolation.
- Number representation inside the simulation. Floating point is not ruled out here, because cross-build identity is not required.
- How the simulation uses more than one core.
- Server topology for the shared world: one process per world, regions, sharding, and hosting.
