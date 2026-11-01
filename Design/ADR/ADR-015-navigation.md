# ADR-015 — Navigation: one surface per column, classes by climb and clearance, rebuilt every tick

**Status:** Proposed, 2026-11-01. It records the navigation model MvpPlan.md schedules for M2, on top of the tiles M1 built.
- **Before code rests on it,** the owner's go-ahead is needed ([MvpPlan.md](../MvpPlan.md), M2, input 1).
- **The commit that first implements it** marks it Accepted (AGENTS.md §6).

It fixes the model and leaves the search algorithm to the navigation orders, which choose it by measurement ([MvpPlan-M2.md](../MvpPlan-M2.md), phase F). Nothing implements the classes, the search or the movement yet.

## Context

- **GameDesign.md decides the rules** (*Where RTS logic meets destruction*):
  - A breach is a path. Navigation updates from edits once a tick, and a gap one voxel wide and two high is a route.
  - Each column has one walkable surface. No unit stands on a structure, nothing is tunnelled, and the space under an overhang is not walkable.
  - Foot soldiers climb as steep as settled earth; cavalry, carts, rams and engines climb less.
  - A unit whose ground goes drops to the next walkable surface, with crush damage past 3 m. A unit whose cell fills with rubble is placed on top.
  - Rubble is walked over more slowly.
- **The owner decided the tiles on 2026-11-01** (GameDesign.md, *M1's open values*, item 3): tiles of 32 × 32 cells of 1 m, one per chunk column, and an edit rebuilds the tiles its chunks touch.
- **[ADR-004](ADR-004-rule-based-destruction.md), decision 6:** units are not physics bodies, and navigation moves them, never a collapse.
- **M1's `NavigationGrid`** has foot's half of this. Each cell holds:
  - the highest cell with ground under it and two cells clear, of which only the lower may be water;
  - eight links, open where the heights differ by at most one voxel;
  - diagonals that never cut a corner.

  `SimulationBench` times a tile rebuild on the MVP map and the 2 km season world.
- **The models give the classes their sizes** (`GameData/Client/Models/`, 0.5 m voxels):

  | Model | Width | Height | Length |
  |---|---|---|---|
  | Foot figures, the sapper included | 1–1.5 m | 2–3.5 m | 0.5–1 m |
  | The outrider, horse and rider | 0.5 m | 2.5 m | 2.5 m |
  | The battering ram | 3 m | 3.5 m | 7.5 m |
  | The trebuchet, cocked | 13 m | 14 m | 4.5 m |

  For foot the design overrides the models: "a unit stands 2 m tall on a 0.5 m square" (GameDescription.md, *Units and the land*), so arms, tools and spears do not count. The design gives no such size for cavalry or engines. T5's passage is exactly 3 m wide and 4 m high, the ram's width and its height rounded up to whole voxels.
- **Two needs come from outside navigation:**
  - The passability view (M3) shows "a route for foot soldiers through the section", which H2's prediction items depend on (GameDesign.md, *Hypotheses*).
  - A unit ordered inside must take the breach as soon as it opens.

## Decision

1. **A cell holds one surface and what stands above it.**
   - **Its height:** the highest cell with walkable ground under it and two cells clear above it, as M1 has it. Two cells is foot's headroom, the least of any class.
   - **Its headroom:** the clear cells above that surface, in whole voxels, up to the tallest class's need.
   - **Its ground:** plain, ford (one voxel of water) or rubble.

   A taller class is stopped where the headroom is short. It never finds a second surface in the same column.
2. **A class is a climb, a headroom, a width and a ford rule.** The MVP has three classes:
   - **foot:** all foot soldiers, sappers and laborers;
   - **cavalry:** the outriders;
   - **machines:** the ram, and the trebuchet when it moves packed.

   Their values are server data ([ADR-016](ADR-016-server-data-format.md)). MvpPlan.md, M2, proposes them from the models for the owner to decide.
   - **Climb:** a class climbs at most 1 m over a run of R cells. A link is open for it when the ground along the link's line rises or falls by at most 1 m over any R consecutive cells that include the link. Foot has R = 1, which is M1's rule. A class that "climbs less" has a larger R. A single limit per step could not say that on a 1 m grid, because the only limit below 1 m is zero.
   - **Corners:** a diagonal link is open only when the cells beside it are open for the class, as M1 does.
   - **Width:** each cell holds, for each class, its clearance. That is the width of the largest square of cells, with this cell at its minimum corner, on which the class can stand: every cell walkable for it, with its headroom, and within 1 m of this cell's height (Harabor and Botea, *Hierarchical path planning for multi-size agents in heterogeneous environments*, 2008). A unit enters only cells whose clearance is at least its width. The square takes the footprint's width and ignores its length, so a long machine is treated as square. The ram's 7.5 m can therefore clip a corner, and the MVP accepts that: the ram drives straight to a gate.
3. **What a unit walks is what its path costs.** Each ground kind has a factor for each class, and rubble and fords have factors above 1. The factor divides the unit's speed on the cell and multiplies the cell's cost to the search by the same amount. The factors are server data.
4. **Tiles are rebuilt on the tick of the edit, all of them.** In the tick's navigation step, every tile whose cells the tick's edits can change is rebuilt, in tile order. That includes the margin the clearance squares and the climb runs reach into. None is deferred: a deferred tile is a wrong answer about whether a breach is open, and that is what H2 tests.
5. **A squad goes as near its goal as it can, and searches again when the ground changes.**
   - **Out of reach:** a goal the class cannot reach gives a path to the reachable cell nearest it, and the squad is marked short of its goal.
   - **Searching again:** after each tick's rebuild, two kinds of squad search again, in squad-id order:
     - every squad marked short of its goal;
     - every squad whose path crosses a rebuilt tile.

     A breach therefore draws a squad ordered inside through it on the tick after the breach opens, or within the budget's few ticks after that.
   - **Determinism and budget:** searches are deterministic, ties included. They are budgeted in units of work ([ADR-013](ADR-013-ticks-numbers-and-randomness.md), decision 6), and a search the budget interrupts goes on next tick and is saved with the pending work ([ADR-014](ADR-014-edits-saves-and-replays.md)).
   - **Left to the navigation orders:** the search algorithm, and how reach is known without searching the map. Phase F chooses them on M2's measurements, and the commit that implements them records them here.
6. **Units follow their squad's path, in fixed point.**
   - Figures and engines move between cell centres along links, at their class speed. Positions are `Core::Fixed` (ADR-013), and a figure stands at its cell's height.
   - How figures arrange about the path, and how they keep apart, comes with combat in M4. Until then they may pass through each other.
7. **Units whose cells change are moved in one step, after the rebuild.** The step runs in unit-id order:
   - **Ground gone:** a unit whose cell's height fell drops to it. A drop over 3 m raises a crush event that carries the drop. The damage it does is M4's statistics.
   - **Cell filled:** a unit whose cell's height rose is lifted to the new surface, with the collapse's damage when a collapse filled it.
   - **No longer walkable:** a unit whose cell its class can no longer use, for lack of headroom or clearance or for deep water, moves to the nearest cell it can use, with ties to the lower index.

   MvpPlan.md's tick gains this as a step after the navigation rebuild.

## Consequences for the code

- **`NavigationGrid` grows:** a headroom and a ground kind for each cell, and per class its links and its clearance. A tile rebuild reads its neighbours' cells as far as the clearance and climb margins reach, and M1's `TilesTouchedBy` widens by the same margin.
- **`Simulation` gains** the search, its budget and its pending queue, squad paths, figure movement, and the drop step.
- **Clearance, and whatever the search keeps to answer reach,** are derived data. Saves leave them out and loads rebuild them (ADR-014, decision 4).
- **The passability view (M3) reads reach from navigation,** the same answer the squads move by.
- **A breach is open to cavalry and the ram only where it slopes gently enough for their R.** Settled earth and rubble lie at 45°, so a rubble ramp into a breach is open to foot alone, unless the owner sets cavalry's R to 1. MvpPlan.md, M2, asks the owner.
- **T5's redraw before M4** (GameDesign.md, *the paper tests' values*, item 14) has to keep a passage at least 3 m wide and 4 m high, or the ram cannot reach its gate.

## What this forecloses

- Navigation meshes, and more than one walkable surface in a column: wall walks, tunnels and bridges.
- Cutting corners on diagonals.
- Deferring a tile rebuild to a later tick.
- A search whose result depends on anything but the grid, the goal and the order of requests.
- Units moved by anything but navigation, and float in any of it.
- A footprint longer than it is wide, until a later ADR.

## Left open

- **The search algorithm** and **how reach is answered** without searching the map, chosen by the navigation orders on M2's measurements.
- **A tile-level graph** (Botea, Müller and Schaeffer, *Near optimal hierarchical path-finding*, 2004), if cross-map searches run too far past the budget.
- **Flow fields for many units** with one goal.
- **Formations and separation,** in M4.
- **The classes' values,** and the trebuchet's packed width once its packed model is drawn.
