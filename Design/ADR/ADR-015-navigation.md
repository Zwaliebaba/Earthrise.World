# ADR-015 — Navigation: one surface per column, classes by climb and clearance, rebuilt every tick

**Status:** Accepted, 2026-11-01, by order M2-F1 ([MvpPlan-M2.md](../MvpPlan-M2.md)), which implements decisions 1 and 2's climb, headroom and fords in `Simulation::NavigationGrid`. Proposed the same day. It records the navigation model MvpPlan.md schedules for M2, on top of the tiles M1 built. **Amended 2026-11-01** with the owner's MVP assets v3 package: the trebuchet's rows below, where the row called cocked gave the released pose's size, and its packed width, which the machines class does not fit.
- **The owner gave the go-ahead** on 2026-11-01 ([MvpPlan.md](../MvpPlan.md), M2, input 1; GameDesign.md, *M2's values*).
- **What the code fixed that the decision left unsaid** (M2-F1):
  - **A run of R cells is R steps:** a link is open for a class when every window of R + 1 consecutive cells along the link's line that holds both of its ends rises or falls by at most 1 m over its walkable ground. For R = 1 that is M1's rule. Cells off the grid or with no surface are left out of a window.
  - **A ford is the one voxel of water a surface may stand in,** as M1's surface rule already allows; a class wades it when its ford depth is at least 1, which every class's is.
  - **Ground is rubble** when the voxel under the surface is rubble of either kind.
  - **The margin** an edit's tiles widen by is the longest climb run plus the widest class's clearance less one: a cell's clearance reads the square it is the corner of, and a link reads the run beyond the cells whose clearance changed. That is 5 cells for the MVP's classes (M2-F2; F1 had it as the larger of the two, before clearance existed).
  - **The search** (M2-F3, `Simulation/PathSearch.h`) is A\* over cells. A step costs 10 straight and 14 diagonally, times the factor of the ground it lands on for the class; the heuristic is the octile distance in the same integers, which never overestimates since no factor is below 1; ties go by f, then h, then cell index. A goal out of reach gives the reachable cell nearest it by octile distance, ties to the lower index. Searches run in step 3, before units move, served in the order of the tick they were asked for in, then squad id, within `pathNodesPerTick` nodes a tick (Proposed at 4,096 until M2-G2 measures it); one the budget cuts short goes on next tick and is saved with its open and closed cells. A `MoveSquad` asks for its squad's search from where its first unit stands.
  - **Its figures** on the generated MVP map (seed `0x5eed2026`), measured on 2026-11-01 in the Linux container with clang 18 at -O2, by the same code `SimulationBench` runs; the benchmark machine's figures are the owner's run of the bench (M2-G2):

    | What | Nodes | Time |
    |---|---|---|
    | Every tile, with its classes, clearance and links | | 209 ms |
    | One tile | | 1.3 ms |
    | Foot, corner to corner, (8, 8) to (504, 504) | 33,077 | 39 ms |
    | Foot, the other diagonal | 32,815 | 34 ms |
    | Cavalry, corner to corner | 85,646, short of the goal | 106 ms |
    | Machines, corner to corner | 838, short of the goal | 0.6 ms |

    A cross-map search takes about 8 ticks at the Proposed budget. Cavalry and machines cannot cross the generated map: the river's valley sides are steeper than 1 m in 2 cells, so they cannot reach its fords, and machines' 3 m clearance and 1-in-3 climb leave them little of the relief. That is a question for the generator's maps (M2-D3), which the owner judges.

    **GENERATOR_VERSION 2** grades each ford's banks 1 m in 3 from its water (the owner, after M2's report), and the same searches, measured the same way, give: foot 30,375 and 25,941 nodes; **cavalry now crosses,** 32,774 and 36,146 nodes; machines are still short of the goal from both corners, 838 and 1 nodes, hemmed in by the relief about their start rather than by the river. `GeneratorTests` checks that every class reaches the far bank along a ford.
  - **Clearance** (M2-F2) is built square by square, adding a row and a column each step, up to the widest class's width, 3; a class enters a cell whose clearance is at least its width, and its links open only between such cells.
  - **Reach** (M2-F4) is known from **per-tile components**, a tile-level graph kept for reach alone: each class's cells in each tile are labelled by the sets its links join without leaving the tile, and components in neighbouring tiles are joined, at each query, by the links that cross between them. A rebuild marks the tiles in which any cell's links or clearance changed, and the next search labels only those again. A query walks the components from the start's; if it reaches the goal's, the search goes for the goal, and otherwise for the reached cell nearest the goal by octile distance, ties to the lower index, found by scanning the reached tiles nearest the goal first. The search itself still runs over cells. The alternative, region labels over the whole map, was measured beside it in `SimulationBench` on the generated MVP map, in the Linux container with clang 18 at -O2, on 2026-11-01:

    | What, every class | Region labels | Per-tile components |
    |---|---|---|
    | Labelling the whole map | 11.2 ms, 4,338 regions | 35.7 ms, 5,502 components, once when the world is made or loaded |
    | After an edit: a 4 m wall across one tile, raised or taken down (3 tiles rebuilt) | 11.2 ms, the whole map again | 0.46 ms |
    | A query from (8, 8) toward (504, 504) | a lookup | foot 0.13 ms, reaches the goal; cavalry 0.69 ms, reaches (216, 510); machines 0.01 ms, reaches (27, 36) |

    A breach rebuilds tiles on most of its ticks, and every squad short of its goal searches again after each, so the cost that matters is the one after an edit, and per-tile components are the cheaper, by about 24 times. An unreachable goal no longer floods its class's region: the search goes straight for the nearest reachable cell (`ReachTests`).
  - **Searching again** follows decision 5, with one addition the code forced: a search not yet done when tiles are rebuilt starts again, asked for in the tick of the rebuild, since the cells it has closed may have changed under it. A search that runs past a tick therefore finishes only once the ground holds still for as many ticks as it needs, which G2's timing of the breach run shows.
  - **Movement** (M2-F5) follows decision 6 in single file. A squad's figures walk one trail: the cells of the figures behind the lead, the rearmost first, then the path, which starts at the lead's cell; a new path replaces what lay ahead of the lead and keeps what lies behind. Each figure stops as many cells short of the trail's end as it stands behind the lead. A step costs its length, 1 m or floor(√2 × 65,536) raw for a diagonal, times the factor of the ground it lands on, and a figure spends its class's speed on steps each tick, carrying what is left into the next. It lies between the two cells' middles in proportion to what it has spent, stands in the cell it left until halfway, and stands at that cell's height. A figure starts a step only along a link open to its class at that moment, and while its squad has a search not done it finishes the step it is in and starts no other, so the search's start, the cell the lead was stepping into, is still the lead's when the path arrives. 14 m of open ground takes 201 ticks at 4,587 raw a tick, foot's 0.07 m floored, and 401 on rubble (arithmetic on the format; `MovementTests` checks both). Each class moves at its speed in `RULE` (GameDesign.md, *M2's values*, item 10): foot 1.4 m/s, and, as the owner decided after M2's report, cavalry 2.8 m/s and machines 0.7 m/s, 9,175 and 2,293 raw a tick, so 14 m takes them 101 and 401 ticks.
  - **The drop step** (M2-F6) is step 9, `SettleUnits`, and runs decision 7 in unit-id order on the rebuilt grid. A unit whose column's surface fell drops to it, and a drop of more than 3 voxels, 3 m at 1 m, raises a crush event carrying it; one whose surface rose is lifted. A unit whose cell its class can no longer use goes to the nearest cell it can, by octile distance with ties to the lower index, and its squad's file forms again from where its figures stand, with a new search if it has a goal. A collapse hits each unit once: the units in its strip when the piece leaves, and any unit whose cell its landing reaches later, in the tick it lands, which is the tick step 9 lifts it. Each collapse keeps the units it has hit, and saves carry them.

It fixes the model and leaves the search algorithm to the navigation orders, which choose it by measurement ([MvpPlan-M2.md](../MvpPlan-M2.md), phase F). M2-F1 implements the classes, M2-F3 the search, M2-F4 reach, M2-F5 movement and M2-F6 the drop step.

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
  | The trebuchet, released (S3) | 11.5 m | 13 m | 3.5 m |
  | The trebuchet, cocked (S3c) | 13 m | 9 m | 3.5 m |
  | The trebuchet, packed (S3p), the only pose that moves | 3.5 m | 3.5 m | 8 m |

  The trebuchet's three rows are measured from the occupied boxes of the layer files in `GameData/Client/Models/` (2026-11-01). Only the packed pose moves; a deployed trebuchet does not path, so the released and cocked rows size nothing in navigation. The packed wagon faces +x, so its 8 m lies along its travel and its width across it is 3.5 m, 7 voxels: decision 2's machines class is 3 cells wide, from the ram, so the wagon needs a clearance of 4 where the class has 3. Which gives way, the wagon's width or the class's, is the owner's ([MvpAssets.md](../MvpAssets.md), *Known issues*).

  For foot the design overrides the models: "a foot soldier stands 2 m tall on a 0.5 m square" (GameDescription.md, *Combat model*), so arms, tools and spears do not count. The design gives no such size for cavalry or engines. T5's passage is exactly 3 m wide and 4 m high, the ram's width and its height rounded up to whole voxels.
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

   Their values are server data ([ADR-016](ADR-016-server-data-format.md)). The owner decided them on 2026-11-01, from the models (GameDesign.md, M2's values, item 10).
   - **Climb:** a class climbs at most 1 m over a run of R cells. A link is open for it when the ground along the link's line rises or falls by at most 1 m over any R consecutive cells that include the link. Foot has R = 1, which is M1's rule. A class that "climbs less" has a larger R. A single limit per step could not say that on a 1 m grid, because the only limit below 1 m is zero.
   - **Corners:** a diagonal link is open only when the cells beside it are open for the class, as M1 does.
   - **Width:** each cell holds, for each class, its clearance. That is the width of the largest square of cells, with this cell at its minimum corner, on which the class can stand: every cell walkable for it, with its headroom, and within 1 m of this cell's height (Harabor and Botea, *Hierarchical path planning for multi-size agents in heterogeneous environments*, 2008). A unit enters only cells whose clearance is at least its width. The square takes the footprint's width and ignores its length, so a long machine is treated as square. The ram's 7.5 m can therefore clip a corner, and the MVP accepts that: the ram drives straight to a gate.
3. **What a unit walks is what its path costs.** Each ground kind has a factor for each class, and rubble and fords have factors above 1. The factor divides the unit's speed on the cell and multiplies the cell's cost to the search by the same amount. The factors are server data.
4. **Tiles are rebuilt on the tick of the edit, all of them.** In the tick's navigation step, every tile whose cells the tick's edits can change is rebuilt, in tile order. That includes the margin the clearance squares and the climb runs reach into. None is deferred: a deferred tile is a wrong answer about whether a breach is open, and that is what H2 tests.
5. **A squad goes as near its goal as it can, and searches again when the ground changes.**
   - **Out of reach:** a goal the class cannot reach gives a path to the reachable cell nearest it, and the squad is marked short of its goal.
   - **Searching again:** after each tick's rebuild, two kinds of squad are marked to search again, and they search in the next tick's step 3, before units move, in squad-id order (MvpPlan.md, *One simulation tick*):
     - every squad marked short of its goal;
     - every squad whose path crosses a rebuilt tile.

     A breach therefore draws a squad ordered inside through it on the tick after the breach opens, or within the budget's few ticks after that.
   - **Determinism and budget:** searches are deterministic, ties included. They are budgeted in units of work ([ADR-013](ADR-013-ticks-numbers-and-randomness.md), decision 6), and a search the budget interrupts goes on next tick and is saved with the pending work ([ADR-014](ADR-014-edits-saves-and-replays.md)).
   - **Left to the navigation orders:** the search algorithm, and how reach is known without searching the map. Phase F chooses them on M2's measurements, and the commit that implements them records them here.
6. **Units follow their squad's path, in fixed point.**
   - Figures and engines move between cell centres along links, at their class speed. Positions are `Core::Fixed` (ADR-013), and a figure stands at its cell's height.
   - How figures arrange about the path, and how they keep apart, comes with combat in M4, which schedules it as *formations and separation* ([MvpPlan.md](../MvpPlan.md), M4). Until then they may pass through each other.
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
- **A breach is open to cavalry and the ram only where it slopes gently enough for their R.** Settled earth and rubble lie at 45°, so a rubble ramp into a breach is open to foot alone, unless the owner sets cavalry's R to 1. The owner kept it so (M2's values, item 11).
- **T5's redraw before M4** (GameDesign.md, *the paper tests' values*, item 14) has to keep a passage at least 3 m wide and 4 m high, or the ram cannot reach its gate.

## What this forecloses

- Navigation meshes, and more than one walkable surface in a column: wall walks, tunnels and bridges.
- Cutting corners on diagonals.
- Deferring a tile rebuild to a later tick.
- A search whose result depends on anything but the grid, the goal and the order of requests.
- Units moved by anything but navigation, and float in any of it.
- A footprint longer than it is wide, until a later ADR.

## Left open

- ~~**The search algorithm** and **how reach is answered** without searching the map~~: closed by M2-F3 (A\* over cells) and M2-F4 (per-tile components), above.
- **A tile-level graph for the search** (Botea, Müller and Schaeffer, *Near optimal hierarchical path-finding*, 2004), if cross-map searches run too far past the budget. M2-F4's components are the first half of one: they answer reach, and the search does not yet use them.
- ~~**Cavalry's and machines' speeds**~~: 2.8 m/s and 0.7 m/s, the owner's, after M2's report.
- **Flow fields for many units** with one goal.
- **Formations and separation,** scheduled in M4 with its own done-when ([MvpPlan.md](../MvpPlan.md), M4). An ADR records how figures keep apart when it is built. Like the rest of movement, it stays inside navigation: nothing pushes a unit (ADR-004, decision 6).
- ~~**The trebuchet's packed width,** once its packed model is drawn.~~ Drawn 2026-11-01: 3.5 m across its travel, which the machines class's width of 3 does not fit (the table above). The owner decides which changes.
