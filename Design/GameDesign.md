# Earthrise.World — Game Design

**Status:** Draft, 2026-10-30. Nothing here is implemented. Each section says what kind of statement it holds: **Decided** records the owner's answers, **Proposed** is a recommendation awaiting the owner, **Open** is what nobody has decided yet. A proposal becomes a decision only when the owner accepts it, and this document is updated when that happens.

This document says what the game is. [AGENTS.md](../AGENTS.md) says how its code is written, and [`Design/ADR/`](ADR/) records the engineering decisions that follow from it.

## The game in one paragraph

Earthrise.World is a real-time strategy game set in a destructible voxel world. It plays like a classic historical RTS: workers gather, a settlement grows, technology advances, and armies built around counters fight. Every structure and every hillside in it is made of voxels that can be dug, quarried, built and knocked down. The long-term goal is a shared, persistent online world. The first playable version is single-player against AI factions on one generated map.

## Decided by the owner (2026-10-30)

1. **Voxels exist for destruction.** Teardown is the reference for both the destruction and the look. How close the look can get from an RTS camera is what the renderer spike finds out (below).
2. **The game logic is the classic historical RTS:** a worker economy, technology progression and counter-based unit combat. Age of Empires II is the reference for how those mechanics feel. It is not a source of content: factions, units, resources and technology are this game's own.
3. **The real goal is a shared, persistent online world.** It is not part of the MVP.
4. **The MVP has one human player.** AI factions fill the other seats.
5. **Camera and voxel size are settled by a renderer spike** that shows and measures them, not on paper. The owner prefers an isometric view and is open to alternatives.

## Pillars (Proposed)

A pillar is a test every feature has to pass. A feature that serves none of them is a candidate for cutting.

1. **The economy is excavation.** Resources are voxels in the world. Stone is quarried out of a hillside, ore is dug out of a mountain, and timber is cut from a forest that stays cut. Gathering changes the map, and the land a player ends up with is a record of how they played.
2. **Siege is physical.** A wall is breached where it is hit, not when a health bar runs out. A breach is a hole an army can walk through. A collapse damages what it lands on, and undermining a foundation brings down what stands on it.
3. **The world remembers.** Quarries, ruins and breaches are still there next session. How long the world remembers is the persistence model's question (Open, below).

## Where RTS logic meets destruction (Proposed)

- **Unit-versus-unit combat stays stat-based**: attack, armor and counters. At RTS distance a hit location on a soldier is invisible, and counter-based combat is what keeps a battle legible.
- **Structures and terrain are physical.** Damage removes voxels where it lands.
- **A building is a template.** Construction fills in the template's voxels as workers build. Repair puts missing voxels back and pays for the material. A building keeps working until it has lost enough of itself; below that it is a ruin until repaired. The threshold is Open.
- **A breach is a path.** Navigation updates from every edit. A gap a unit fits through is a route.
- **A piece that loses its support collapses** through a rule in the simulation ([ADR-004](ADR/ADR-004-rule-based-destruction.md)). The proposed rule: the piece topples towards the side that lost support, settles as rubble and damages the voxels and units it lands on. Visual debris is the client's business and never affects play.
- **Materials behave differently.** Wood, earth and stone each have their own strength. Earth slumps to a maximum slope, so digging under a wall's foundation is a siege tactic. Wood burns, but fire is not in the MVP.

## Persistence and the shared world (Open)

A classic RTS match ends; this world is meant not to, so the match arc has to be replaced. None of the questions below blocks the MVP. All of them block the design of the online world.

- **Offline protection.** What happens to a player's settlement while they are logged off, in a world where it can be flattened? An AI taking over their faction is one tool ([ADR-002](ADR/ADR-002-authoritative-simulation.md), decision 5). It is not an answer by itself.
- **Snowballing.** What stops the oldest, largest player from owning everything?
- **Newcomers.** Where does a player who joins in month three start, and against whom?
- **Ending.** Does a world run forever, or in seasons that end and reset? A reset has to be reconciled with pillar 3.
- **Population.** A shared world needs enough players online at once. AI factions fill the gaps, but how many humans does a world need to feel alive?
- **Operations.** A shared world is a live service: hosting, accounts, backups, moderation and cheating. For a one-person project this is the largest non-technical commitment in the plan, and it should be costed before the game depends on it.

## MVP (Proposed)

The MVP exists to prove that pillars 1 and 2 are fun. It does not try to prove persistence or networking, only to keep both possible. The simulation sits behind a server boundary ([ADR-002](ADR/ADR-002-authoritative-simulation.md)), and the world is a seed plus edits ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)).

**In:**

- One map generated from a seed, bounded in size and held entirely in memory.
- One human player and a small number of AI factions.
- One faction type shared by everyone. Its roster is workers, a small military roster built around counters, and at least one siege weapon.
- Gathering by excavation, construction and repair from templates, and rule-based collapse.
- Save and load as a seed plus edits.
- The simulation running in-process behind the server boundary.

**Out.** Each item here is a later decision, not a never:

- Online play of any kind, including the shared world.
- More than one faction type.
- Debris simulated by physics that affects gameplay.
- Fire.
- Worlds larger than memory, and streaming.
- Technology progression beyond what the MVP roster needs.

## Renderer spike (Decided: it happens; Proposed: what it answers)

The spike is the first code in the repository. It conforms to AGENTS.md like any other code, which also means it settles the solution layout (AGENTS.md §2). It draws a generated test scene with [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md)'s method: terrain, a dense forest, and a town with walls, roughly what a late-game settlement looks like. It answers four questions:

1. **Voxel size.** The candidates are 10, 25 and 50 cm, each shown at the default zoom, the closest and the farthest. Teardown uses 10 cm, but the player sees it from a few metres away. As a rough estimate only, if the default view spans about 150 m of world at 1920 px, a 10 cm voxel is about 1.3 px, a 25 cm one about 3 px and a 50 cm one about 6 px. The spike replaces this estimate with what is actually on screen.
2. **Camera.** Orthographic isometric, or perspective at a steep fixed pitch with a narrow field of view. It also settles whether the camera rotates (in 90° steps or freely) and the zoom range. Destructible walls and rubble hide units, and that weighs in favor of rotation.
3. **Cost.** GPU time at the default and the farthest zoom, measured on stated hardware.
4. **Look.** How far shadow maps and per-voxel shading get towards the Teardown reference. If they fall short, a lighting ADR follows: traced shadows and occlusion through a voxel occupancy volume, or DXR for secondary rays, which ADR-001 leaves room for.

Its output is an ADR on voxel size and camera, with measured figures. If question 4 calls for one, a lighting ADR follows. The spike needs Windows and a Direct3D 12 GPU to build and run.

## Open questions

- The persistence model (above).
- Setting and era. The Age of Empires II reference implies medieval, but that has not been decided.
- The shape of technology progression.
- The number of AI factions and the map size for the MVP.
- Whether support is connectivity only, or also load.
- Rubble: does it block, slow, or get cleared by workers, and is it a resource when it is cleared?
- The threshold at which a damaged building stops working.
