# Earthrise.World — Game Design

**Status:** Draft, 2026-10-30, revised the same day after a design review. Nothing here is implemented. Each section says what kind of statement it holds: **Decided** records the owner's answers, **Proposed** is a recommendation awaiting the owner, **Open** is what nobody has decided yet. A proposal becomes a decision only when the owner accepts it, and this document is updated when that happens.

This document says what the game is. [AGENTS.md](../AGENTS.md) says how its code is written, and [`Design/ADR/`](ADR/) records the engineering decisions that follow from it.

## The game in one paragraph

Earthrise.World is a real-time strategy game set in a destructible, medieval voxel world. It plays like a classic historical RTS: workers gather, a settlement grows, technology advances, and armies built around counters fight. Every structure and every hillside in it is made of voxels that can be dug, quarried, built and knocked down. The long-term goal is a shared, persistent online world. The first playable version is single-player against AI factions on one generated map.

## Decided by the owner (2026-10-30)

1. **Voxels exist for destruction.** Teardown is the reference for both the destruction and the look. How close the look can get from an RTS camera is what the renderer spike finds out (below).
2. **The game logic is the classic historical RTS:** a worker economy, technology progression and counter-based unit combat. Age of Empires II is the reference for how those mechanics feel. It is not a source of content: factions, units, resources and technology are this game's own.
3. **The real goal is a shared, persistent online world.** It is not part of the MVP.
4. **The MVP has one human player.** AI factions fill the other seats.
5. **Camera, render voxel size and simulation voxel size are settled by a renderer spike** that shows and measures them, not on paper. The two voxel sizes may differ by an integer ratio; the simulation voxel size is chosen on measured simulation cost as well as on the picture. The owner prefers an isometric view and is open to alternatives.
6. **The setting is medieval.**

## Pillars (Proposed)

A pillar is a test every feature has to pass. A feature that serves none of them is a candidate for cutting.

1. **Where a player digs decides how they can be attacked.** Resources are voxels in finite, located deposits: building stone in outcrops, ore in underground veins, timber in forests that stay cut for the life of a world. A quarry beside a wall is a ditch an attacker has to cross; a quarry in earth is a slope that can slump a foundation away.
2. **Siege is physical.** A wall is breached where it is hit, not when a health bar runs out. A breach is a hole an army can walk through. A collapse damages what it lands on, and undermining a foundation brings down what stands on it. A structure stands only where it is supported: connected down to bedrock, with no more than a maximum unsupported span for each material ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 2).
3. **Changes to the land last for the life of a world.** How long a world lives is the persistence model's question (Open, below). A world that ends is archived, not deleted: its seeds, generator version and edits stay viewable.

## Where RTS logic meets destruction (Proposed)

- **Unit-versus-unit combat stays stat-based**: attack, armor and counters. At RTS distance a hit location on a soldier is invisible, and counter-based combat is what keeps a battle legible.
- **Only siege weapons, workers on a designation and collapses remove voxels.** Arrows and melee never do. A shot travels through the voxel grid and stops at the first solid voxel it meets, so a wall stops an arrow. Vision is a radius and is not blocked by walls.
- **Structures and terrain are physical.** A hit removes a crater where it lands, with a radius that depends on the weapon and the material. Voxels carry no hit points.
- **Materials behave differently.** Wood, earth and stone each have their own strength: how large a crater a hit leaves, and how far the material spans without support. Earth slumps to a maximum slope, so digging beside a wall's foundation is a siege tactic. The generated world holds earth only at or below that slope; steeper ground is stone. Wood burns, but fire is not in the MVP.
- **Excavation is a designation.** The player places a quarry, ditch or trench with a footprint and a depth. Workers dig it in a pattern that always leaves a ramp out, so a quarry never traps its own workers. A moat is a ditch, and undermining is a trench beside a foundation.
- **A building is a template.** Construction fills in the template's voxels as workers build. Each template has a foundation that reaches the ground, and construction levels the footprint first. Repair puts missing voxels back and pays for the material. A building stops working when one of its critical parts, such as a gatehouse's gate or a tower's firing platform, falls below its threshold; it is then a ruin until repaired. There is no building outside templates in the MVP.
- **Units do not stand on structures in the MVP.** Towers and gatehouses garrison and fire.
- **A unit whose ground is removed drops to the next walkable surface in its column** and takes fall damage above a set height. A unit whose cell fills with rubble is placed on top and takes the collapse's damage. Navigation moves it; a collapse never does ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 6).
- **A breach is a path.** Navigation updates from edits once per tick, on a grid sized for units rather than voxels. A gap a unit fits through is a route. In the MVP each column has one walkable surface, because units do not stand on structures and nothing is tunnelled.
- **A piece that loses its support collapses** through a rule in the simulation ([ADR-004](ADR/ADR-004-rule-based-destruction.md)). The proposed rule: the piece falls towards the side that lost support, or straight down if it lost support evenly. It does not rotate in the simulation; it is laid down as rubble along the fall, over a length up to its height, and damages the voxels and units in that strip as it falls. A collapse that removes another structure's support starts a new collapse on the next tick. Visual debris is the client's business and never affects play.
- **Rubble is loose material.** It slumps like earth, can be walked over more slowly, and workers clear it for stone at a fraction of its volume.

## Persistence and the shared world (Open, except where marked Proposed)

A classic RTS match ends; this world is meant not to, so the match arc has to be replaced. None of the questions below blocks the MVP. All of them block the design of the online world.

- **Offline protection.** What happens to a player's settlement while they are logged off, in a world where it can be flattened? An AI taking over their faction is one tool ([ADR-002](ADR/ADR-002-authoritative-simulation.md), decision 5). It is not an answer by itself.
- **Snowballing.** What stops the oldest, largest player from owning everything?
- **Newcomers.** Where does a player who joins in month three start, and against whom?
- **Ending.** Does a world run forever, or in seasons that end and reset? Pillar 3 binds a change to the life of its world, so a reset does not break it.
- **Population.** A shared world needs enough players online at once. AI factions fill the gaps, but how many humans does a world need to feel alive?
- **Operations.** A shared world is a live service: hosting, accounts, backups, moderation and cheating. For a one-person project this is the largest non-technical commitment in the plan, and it should be costed before the game depends on it.

**Proposed:**

- **The first online form is a dedicated server that players host.** A world this project runs itself is a later decision, costed on its own. A hosted server is the authoritative simulation of [ADR-002](ADR/ADR-002-authoritative-simulation.md), so this changes nothing in the architecture.
- **Online work starts only when** the MVP's hypotheses have passed; the simulation builds headless as its own project, with no dependency on the client; [ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)'s generator versioning is in force and a save survives a change of generator version; an operations budget is written down, in hours per week and money per month; and it is decided whether identity and distribution go through a platform such as Steam, whose SDK R14 excludes.

## MVP (Proposed)

The MVP exists to prove that pillars 1 and 2 are fun. It does not try to prove persistence or networking, only to keep both possible. The simulation sits behind a server boundary ([ADR-002](ADR/ADR-002-authoritative-simulation.md)), and the world is a seed plus edits ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)). It is built in stages (below), so the test of the pillars does not wait for an AI that plays the whole game.

**In:**

- One map, 512 m × 512 m, generated from a seed and held entirely in memory.
- One human player. Stages 1 and 2 have one AI faction; the skirmish has one or two.
- One faction type shared by everyone. Its roster is workers, a small military roster built around counters, and at least one siege weapon. The period is the high medieval, before gunpowder.
- Gathering by excavation, construction and repair from templates, and rule-based collapse.
- Fog of war as a vision radius, enforced by the simulation ([ADR-002](ADR/ADR-002-authoritative-simulation.md), decision 4). A fogged area shows what the player last saw.
- A win condition for each stage.
- Save and load as a seed plus edits. A scenario is a save: the seeds, the edits and the entities.
- The simulation running in-process behind the server boundary.

**Out.** Each item here is a later decision, not a never:

- Online play of any kind, including the shared world.
- More than one faction type.
- Debris simulated by physics that affects gameplay.
- Fire.
- Worlds larger than memory, and streaming.
- Technology progression beyond what the MVP roster needs.
- Gunpowder.
- Flowing water. Water is static terrain from the generator, and a dug voxel never fills with it.
- Tunnels. All digging is open to the sky.
- Units on walls.
- AI factions that undermine. They repair against undermining but do not do it.
- Building outside templates.

### Hypotheses

Each pillar is stated as a claim a playtest can refute.

- **H1, pillar 1: where a player quarries changes how their settlement is attacked.** It fails if, in stage 2, where players quarried has no visible relation to where the waves breached or how the defense went, and players cannot name a trade-off they made in choosing where to dig.
- **H2, pillar 2: players choose where to breach for reasons they can state, and predict the result.** It fails if the breach points players choose cluster at the nearest point of the wall whatever the wall and the ground are like; if players asked "will this section be open after the next N shots?" are right less than half the time; or if players do not prefer it to the control.

The control is the same stages with walls built of segments that have health bars, and resources gathered from nodes that run out. It swaps the collapse resolver ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 3) and the gathering rule and leaves the rest of the game unchanged. The playtest is 6 to 10 people who play RTS games, each playing both stages in both versions.

### Stages

1. **Siege.** The player attacks a pre-built, garrisoned castle. Its AI faction fights and repairs, and does nothing else. The player wins when the keep stops working and is not repaired within a set time.
2. **Build and defend.** The player quarries and builds for a set time, then scripted waves attack. A wave chooses its breach point by one rule: the thinnest stretch of wall it can reach from where it arrives. The player wins if the keep still works when the last wave ends.
3. **Skirmish.** One or two AI factions that play the whole game. It is built only after stages 1 and 2 have passed. A faction loses when its keep stops working and is not repaired within a set time, or when all its units are dead.

### Kill criteria

- If H2 still fails after one round of work on legibility, destruction becomes cosmetic over walls with health bars, and this document is rewritten before more content is made.
- If H1 fails, resources become nodes that leave visible scars on the land.
- If the renderer spike misses its pass mark at every candidate voxel size, a new ADR revisits [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md) before any gameplay code is written.

## Renderer spike (Decided: it happens; Proposed: what it answers)

The spike is the first code in the repository. It conforms to AGENTS.md like any other code, which also means it settles the solution layout (AGENTS.md §2). It draws a generated test scene with [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md)'s method: terrain, a dense forest, and a town with walls, roughly what a late-game settlement looks like. The scene also changes while it is drawn (question 5). It answers six questions:

1. **Voxel size.** The candidates are 10, 25 and 50 cm, each shown at the default zoom, the closest and the farthest. Teardown uses 10 cm, but the player sees it from a few metres away. As a rough estimate only, if the default view spans about 150 m of world at 1920 px, a 10 cm voxel is about 1.3 px, a 25 cm one about 3 px and a 50 cm one about 6 px. The spike replaces this estimate with what is actually on screen. It settles two sizes: the render voxel and the simulation voxel. They are equal unless question 6 calls for a simulation voxel that is an integer multiple of the render voxel; the client then generates the finer detail from the simulation voxel's material and a position hash, and that detail is cosmetic.
2. **Camera.** Orthographic isometric, or perspective at a steep fixed pitch with a narrow field of view. It also settles whether the camera rotates (in 90° steps or freely) and the zoom range. Destructible walls and rubble hide units, and that weighs in favor of rotation.
3. **Cost.** GPU time at the default and the farthest zoom, measured on stated hardware.
4. **Look.** How far shadow maps and per-voxel shading get towards the Teardown reference. If they fall short, a lighting ADR follows: traced shadows and occlusion through a voxel occupancy volume, or DXR for secondary rays, which ADR-001 leaves room for.
5. **Edits.** The scene runs quarrying at a stated rate, and one wall collapse. The spike measures the cost of rebuilding an edited chunk's render buffer, and checks distant chunks for voxels that pop in or out. The stochastic pruning in ADR-001's source keys on a voxel's position in the render buffer (its Listing 3), which an edit reorders; the spike's ADR records a key that survives edits.
6. **Simulation cost** for each candidate size: memory for the MVP map, one support search on the test town after a breach, and one navigation tile rebuild.

The pass mark — target GPU, resolution and frame budget — is written down before the spike measures anything.

Its output is an ADR on the two voxel sizes and the camera, with measured figures. If question 4 calls for one, a lighting ADR follows. The spike needs Windows and a Direct3D 12 GPU to build and run.

## Open questions

- The persistence model (above).
- Whether later periods, and gunpowder with them, come with technology progression. Bombards would be the strongest destruction tool in the game and would change what a wall is worth.
- The shape of technology progression.
- The skirmish AI's capabilities.
- The target hardware and frame budget, which the spike's pass mark needs before it measures.
- The resource set, and whether it includes food.
- Whether units ever fight from walls.
- Whether players ever build outside templates.
- Water beyond static bodies.
