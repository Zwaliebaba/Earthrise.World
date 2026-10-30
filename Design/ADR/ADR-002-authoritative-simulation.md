# ADR-002 — The simulation is authoritative and sits behind a server boundary from the first line

**Status:** Proposed, 2026-10-30. Awaiting the owner's acceptance. Nothing implements it yet.

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
3. **The boundary is serialized from the first line, even in-process.** In single-player the simulation runs in the same process and the messages travel through an in-process transport. They are still encoded to bytes and decoded on the other side exactly as they will be over a network. A boundary that is only a function call stays honest by discipline; one made of bytes stays honest by construction. It also means the encoding is tested from the start, not written in a hurry when networking begins.
4. **A client receives only what its player can see.** Fog of war is enforced by the simulation, not painted by the client over a world it fully knows. The same filter later limits a client to its own area of a large world.
5. **AI factions issue the same commands a player does**, through the same interface. An AI can therefore take over a human player's faction, on disconnect or while they are offline, without a second code path.
6. **The simulation advances in fixed ticks.** Wall-clock time maps to ticks at one seam. The simulation follows R16's rules on ordering, time and randomness, so within one build the same state and the same commands produce the same result, and a recorded command stream replays for tests and debugging. Bit-identical results across builds, compilers or machines are not required. The simulation's result is authoritative, so nobody else has to reproduce it.

## Consequences for the code

- The client draws from its own copy of what it was told, never from simulation state. If rendering needs a fact the messages do not carry, the messages change.
- Every command and every state update has an encoding and a round-trip test.
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
