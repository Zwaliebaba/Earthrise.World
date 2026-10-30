# ADR-004 — Destruction is resolved by rules in the simulation; debris is cosmetic

**Status:** Proposed, 2026-10-30. Awaiting the owner's acceptance. Nothing implements it yet.

## Context

Destruction is the reason the world is made of voxels, and Teardown is the reference for it ([GameDesign.md](../GameDesign.md)). In Teardown, a piece that loses its connection to the rest falls as a rigid body, collides with what it meets and comes to rest wherever the physics puts it. Copying that into this game runs into four things:

- **R14.** There is no physics engine on the dependency list. Rigid-body physics for arbitrarily shaped voxel bodies would be written here, and it is specialist work.
- **[ADR-002](ADR-002-authoritative-simulation.md).** The simulation is authoritative, so physics that decides gameplay runs on the server. In a shared world that means every collapse, with the moving bodies replicated to every client that can see them.
- **[ADR-003](ADR-003-world-as-seed-plus-edits.md).** The world is a seed plus edits, and a loose body is not voxels in a chunk. In a world that runs for months, every piece of debris either ends up as voxels again or piles up forever.
- **The camera and the genre.** An RTS is seen from far above. From there the detail of how a piece tumbles is barely visible, while the outcome is what the player has to read: whether the wall is open, and what was crushed. A strategy game also needs outcomes the player can predict. A breach that depends on how a chunk happened to bounce is noise, not tactics.

## Decision

1. **Removing voxels is an edit** (ADR-003), and only the simulation makes it.
2. **After an edit, the simulation finds what lost support.** A piece of a structure that no longer connects, through solid voxels, to its anchor (the ground or its foundation) is unsupported. The search is bounded to the structure that was edited, never the whole world.
3. **An unsupported piece collapses through a rule, the collapse resolver.** The resolver takes the piece and the world around it. It returns where the piece ends up as rubble and which voxels and units it damages on the way, and nothing else. Its result is ordinary edits and ordinary damage. It runs in the simulation, so it is authoritative, and it is a rule, so its outcome is predictable and cheap. The rules themselves are design (GameDesign.md).
4. **The client shows a collapse but never computes one.** It animates the falling piece towards the resolver's result and adds debris, dust and smoke simulated locally. That debris never affects gameplay and never becomes world state: it settles or fades, and what remains is the rubble the simulation placed. Two clients may show different debris.
5. **No physics engine is added; R14 stands.** A physics-based resolver can later replace the rule, behind the same interface, on the server. Because the simulation is authoritative it would not need to be deterministic. That replacement is a new ADR, and reopening R14 for it is the owner's decision.

## Consequences for the code

- The collapse resolver is one interface with one implementation. Nothing else in the simulation knows how a collapse is computed.
- The support search covers structures. Terrain is not searched as one connected structure: what undercut terrain does is a material rule (GameDesign.md).
- Navigation updates from edits, rubble included.
- Cosmetic debris lives entirely in the client and is excluded from saves, replays and network traffic.

## What this forecloses

- Gameplay outcomes decided by rigid-body simulation, until a later ADR replaces the resolver.
- Persistent loose bodies. Everything that comes to rest is voxels in chunks.
- A third-party physics engine, unless a later ADR and the owner reopen R14.

## Left open

- The resolver's rules: the direction of a fall, how rubble settles, and what it damages.
- Whether support is connectivity only, or also considers load.
- Whether rubble blocks movement, slows it, or can be cleared and recycled.
- How cosmetic debris is simulated and drawn, including whether it uses ADR-001's oriented voxels.
