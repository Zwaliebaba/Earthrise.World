# ADR-004 — Destruction is resolved by rules in the simulation; debris is cosmetic

**Status:** Accepted, 2026-10-31. Proposed 2026-10-30 and revised the same day after a design review (decisions 2, 3, 4 and 6); accepted by the owner after the review of the game description, which added three consequences below. M1 implements a first support search for decision 2 (`Simulation/SupportSearch.h`), measured in `SimulationBench`; the resolver comes in M2.

## Context

Destruction is the reason the world is made of voxels, and Teardown is the reference for it ([GameDesign.md](../GameDesign.md)). In Teardown, a piece that loses its connection to the rest falls as a rigid body, collides with what it meets and comes to rest wherever the physics puts it. Copying that into this game runs into four things:

- **R14.** There is no physics engine on the dependency list. Rigid-body physics for arbitrarily shaped voxel bodies would be written here, and it is specialist work.
- **[ADR-002](ADR-002-authoritative-simulation.md).** The simulation is authoritative, so physics that decides gameplay runs on the server. In a shared world that means every collapse, with the moving bodies replicated to every client that can see them.
- **[ADR-003](ADR-003-world-as-seed-plus-edits.md).** The world is a seed plus edits, and a loose body is not voxels in a chunk. In a world that runs for months, every piece of debris either ends up as voxels again or piles up forever.
- **The camera and the genre.** An RTS is seen from far above. From there the detail of how a piece tumbles is barely visible, while the outcome is what the player has to read: whether the wall is open, and what was crushed. A strategy game also needs outcomes the player can predict. A breach that depends on how a chunk happened to bounce is noise, not tactics.

## Decision

1. **Removing voxels is an edit** (ADR-003), and only the simulation makes it.
2. **After an edit, the simulation finds what lost support.** A voxel's support distance is the fewest horizontal steps on any path from it to an anchor that moves only down or sideways through solid voxels. A voxel is supported if its support distance is no more than its material's span (GameDesign.md); a piece made of voxels that are not supported is unsupported. An anchor is a voxel of bedrock material, or any voxel below a depth the generator fixes. Ordinary terrain is not an anchor, because it can be dug away. Connectivity alone would not do: a wall dug out from underneath stays connected to the ground at both ends of the dig and would stand as a bridge, so undermining (GameDesign.md, pillar 2) would never work. The span also bounds the search. An edit can change support only for voxels at or above it and within the longest span of any material horizontally, and the search covers that region, never a whole structure or the world.
3. **An unsupported piece collapses through a rule, the collapse resolver.** The resolver takes the piece and the world around it. It decides where the piece ends up as rubble and which voxels and units it damages on the way, and nothing else. Its result is ordinary edits and ordinary damage. The piece never rotates in the simulation: the resolver lays it down as rubble, so nothing resamples a rotated body. If the damage leaves another piece unsupported, that piece's collapse starts on the next tick, and collapses that start on the same tick are processed in entity-id order, so a cascade is bounded per tick and replays the same way. It runs in the simulation, so it is authoritative, and it is a rule, so its outcome is predictable and cheap. The rules themselves are design (GameDesign.md).
4. **A collapse takes time.** It is a simulation entity: it starts on the tick the piece loses support, lasts a number of ticks, and applies its edits and damage as the fall progresses, not all at once. Under the rule the fall is scripted, but it is still state that lives across ticks. Saves (ADR-003) and state updates (ADR-002) therefore carry a collapse in progress from the start. A resolver that decided everything on the tick support is lost would be simpler today, and it is the one shape a physics resolver could not sit behind. The time a collapse takes is also a warning a player can act on: they see the fall begin and can pull units out of its path before the damage lands.
5. **Nothing outside the resolver predicts a collapse.** The AI planning a breach, and any interface that previews where something will fall, ask the resolver. Neither carries its own copy of the rule.
6. **Units are not physics bodies, under any resolver.** A collapse affects a unit through damage, not by moving it. Unit movement belongs to navigation; a resolver that could push units would turn a change of resolver into a change of the unit model. A unit whose ground is dug away or buried is moved by navigation, under a rule in the design (GameDesign.md), never by the resolver.
7. **The client shows a collapse but never computes one.** It draws the falling piece from the simulation's state updates and adds debris, dust and smoke simulated locally. That debris never affects gameplay and never becomes world state: it settles or fades, and what remains is the rubble the simulation placed. Two clients may show different debris.
8. **No physics engine is added; R14 stands.** A physics-based resolver can later replace the rule's implementation without changing decisions 1–7: its moving bodies become a collapse's in-flight state. It would not need to give the same result on every machine, because the simulation is authoritative (ADR-002). It would still have to meet ADR-002's rule that one build replays the same commands to the same result, and that narrows which engines qualify. The replacement is a new ADR, and reopening R14 for it is the owner's decision.

## Consequences for the code

- The collapse resolver is one interface with one implementation. Nothing else in the simulation knows how a collapse is computed.
- The support search covers structures and stone terrain, and only the region an edit can affect (decision 2). Nothing searches the world as one connected structure. Earth and rubble carry support but are not judged by span: they follow the slump rule, a material rule (GameDesign.md).
- Navigation updates from edits, rubble included.
- Three more callers ask the resolver under decision 5, all from the review of the game description: the integrity view, the preview of where a dig will slump, and the Hold trial's waves when they score the sections they can reach.
- Rubble keeps the material it fell from, so rubble is several materials, not one; only rubble of building stone yields stone when cleared ([GameDesign.md](../GameDesign.md)).
- The resolver's edits pass the same check for holding protection as every other edit, and a collapse that starts inside a holding's window runs to its end (GameDescription.md, *Holdings and siege windows*).
- Cosmetic debris lives entirely in the client and is excluded from saves, replays and network traffic.
- A later physics resolver has to put bodies that come to rest back into the voxel grid, because loose bodies do not persist (ADR-003). A body at rest is generally rotated, so that is a resampling step the rule never needs, and it changes the rubble's shape.
- The cost of switching to physics is mostly design, not code. Siege weapons, building templates and the AI's siege planning are tuned against the rule's outcomes, and a physics resolver changes those outcomes. The more content is tuned, the more the switch costs. If physics is wanted, the time to decide is before content grows beyond the MVP.

## What this forecloses

- Gameplay outcomes decided by rigid-body simulation, until a later ADR replaces the resolver.
- Persistent loose bodies. Everything that comes to rest is voxels in chunks.
- A collapse decided entirely on the tick support is lost.
- Units moved by physics.
- A third-party physics engine, unless a later ADR and the owner reopen R14.
- Support decided by connectivity alone.

## Left open

- The numbers in the resolver's rules, such as warning times and fall lengths. The rules themselves, covering the direction of a fall, how rubble settles and slows movement, and what a fall damages, are decided in GameDesign.md (*Where RTS logic meets destruction*). *(The warning and the fall's length decided 2026-11-01: GameDesign.md, M2's values, items 4 to 8.)*
- Each material's span. It is tuning, and it is counted in voxels, so it waits on the simulation voxel size. *(Decided: stone 2 m and timber 5 m, GameDesign.md, the paper tests' values, item 1, and M1's open values, item 1; leaves 3 m and common rock 2 m, M2's values, item 14.)*
- How cosmetic debris is simulated and drawn, including whether it uses ADR-001's oriented voxels. *(Decided: oriented voxels that tumble for about 1.5 s and then rest, seeded by the collapse's id, GameDesign.md, M1's open values, item 7.)*
