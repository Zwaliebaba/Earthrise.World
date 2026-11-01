# ADR-026 — Scenarios and the Siege trial: a baked source, the trial's rules, its log and its replay

**Status:** Proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). [ADR-014](ADR-014-edits-saves-and-replays.md), decision 7, says a scenario is a baked source in `GameData/Server/Scenarios/`, covered by the server-data hash, holding seeds, structures, earthworks and entities at rest; [ADR-016](ADR-016-server-data-format.md), decision 3, says scenarios join `ServerData` as a section. This fixes the source's format, the section, how a scenario loads, and the Siege trial's rules, log and replay. The castle and the armies are the owner's inputs (MvpPlan.md, M4, inputs 3 to 5 and 15 to 17). The owner gave it the go-ahead on 2026-11-01, as written (MvpPlan.md, M4, input 1; GameDesign.md, *M4's values*, item 1). Order M4-B4, which first implements it, marks it Accepted (AGENTS.md §6).

## Context

- **The trial** (GameDesign.md, *Stages*, 1): the player attacks a pre-built, garrisoned castle whose AI fights and repairs; they win by breaking the keep, which has stopped working while their soldiers hold its foot for two minutes without a repair; they lose when no attacking unit is left or a time limit runs out; the castle's nearest section is not its weakest.
- **The castle is the route table's** ([Review/PaperTests/SiegeRouteTable.md](../Review/PaperTests/SiegeRouteTable.md)): 62 × 62 m over its towers, 22 T3 sections, five T4 towers, the T5 gatehouse facing south, the keep and the hall in the bailey, a ditch and rampart with a causeway and a gap before E3, 1 m of soil over common rock except deep earth under E3, the attackers' camp 100 m south.
- **M3 builds sites in C++** (`BuildLabSite`), so tests, the bench and the executable share them; M4's castle has about thirty structures, earthworks and two armies, which is content, not code.
- **The playtests need records** (MvpPlan.md, M4): breach points, shots, the stone each side spends, and how many figures pass each breach a minute; [ADR-014](ADR-014-edits-saves-and-replays.md), decision 5, gives a replay as a start and the commands.
- **The control plays the same trial** with one rule changed ([ADR-028](ADR-028-the-control.md)).

## Decision

1. **A scenario is a JSON source in `GameData/Server/Scenarios/`,** one file each, `Siege.json` first, with:
   - `seeds`: world, resource and match;
   - `site`: the box the scenario owns, which it levels to a height and fills from bedrock up with the materials its `ground` boxes give, so the castle's ground is the route table's and not the generator's accident;
   - `structures`: each a template id, an origin, a turn, an owner, a name, and a `foundationDepthMeters` that extends the template's foundation course down, Proposed at 1 and at 4 under deep earth (GameDesign.md, *the paper tests' values*, item 16);
   - `earthworks`: boxes dug or raised, each with its material, so the ditch is a cut and the rampart a fill;
   - `players`: each an id, a side, a stockpile of stone and timber, a commander (`human`, `siegeDefender`, or, in tests, `siegeAttacker`), and whether the map starts seen ([ADR-025](ADR-025-fog-of-war.md), decision 5);
   - `squads` and `engines`: each a kind, an owner, cells, and for an engine its facing and state; and `garrisons`: a squad placed in a named building;
   - `trial`: the keep's name, the time limit, the hold time, the ring that is the keep's foot, the camp's box, and the names of the nearest and the weak sections, which the log records and nothing in the simulation reads;
   - or, in place of all of these but `seeds` and `players`, `mirrorOf`: another scenario, mirrored east to west about its site's centre line, with a `rename` table that names each structure for its mirrored place. The layout is mirrored and the templates are not: each structure stands on its mirrored footprint with the quarter turn whose facing mirrors its own, and keeps its template's own asymmetry. A mirror lets one castle test whether players follow its weak point or its geography (MvpPlan.md, M4, input 28).
2. **It is baked into a section, `SCEN`,** by `Tools/BakeContent.py --server`: every scenario, with names resolved to template indices, every number converted exactly, and every mirror resolved into a whole scenario, so the simulation never sees `mirrorOf`. `Tools/CheckAssets.py` checks it: every template exists, no two structures overlap, every squad's cells are walkable on the levelled site, every garrison names a building with room, the keep names a T2. Changing a scenario changes the server-data hash and nothing on the client.
3. **A scenario loads as tick 0 of a new world** (ADR-014, decision 7): the generated world from its seeds, then the site, the ground and the earthworks as edits, the structures through `World::AddStructure`, the players, the squads, the engines and the garrisons, in the file's order. Loading the same scenario twice gives the same state hash.
4. **The trial's rules run in step 10** from the scenario's `trial`:
   - **Won:** the keep does not work ([ADR-024](ADR-024-structures-at-work.md), decision 2), and the ring holds at least one of the attacker's soldiers and none of the defender's; the hold clock counts those ticks and is won at the hold time. It pauses while both sides stand in the ring, and resets when the ring holds no attacker's soldier or the keep is repaired, a voxel restored or, in the control, a hit point.
   - **Lost to the clock:** the time limit passes.
   - **Lost by the army:** the attacker has no unit left, engines included, as GameDesign.md, *Stages*, 1, decides; an attacker left with engines alone cannot win and plays out the clock.
   - The outcome stops the clock and is published in the state update; the world goes on ticking for the client's end screen and its replay.
5. **The trial's events are the simulation's,** each tick, beside the shot hits: a section, a structure without a critical part, **opens** when a column of its footprint is first walkable for foot, reachable from the camp, and linked to a cell on the section's inner side, the side of its centre line nearer the keep, so a gatehouse's passage and an open gate never count; a figure **passes** a section when it steps from outside its footprint into it and on through, counted per side and per game minute; a shot is fired and lands; stone is spent, per player; a structure stops or starts working. `Earthrise`, which holds the world and the client (ADR-020, decision 2), writes them to the trial's log; the client never reads them.
6. **Every run writes its replay** beside its log, as ADR-014, decision 5, defines one, starting from the scenario and holding the transport's commands, with a header naming each seat's commander. The commanders' commands are recomputed when it replays, not recorded ([ADR-027](ADR-027-commanders.md), decision 6).

## Consequences for the code

- **`ServerData`** gains `SCEN` and `FindScenario`; **`Simulation`** gains `LoadScenario`, the trial's rules in step 10, and the events; **`Tools/`** gains the scenario's check and bake, and the log's reader.
- **`BuildLabSite`** stays C++: the lab's items are built per item and keyed by the simulation, and moving them is no gain.
- **The trial's tests** load `Siege.json` from the baked file, as `ServerDataTests` read the real file.

## What this forecloses

- A scenario as a save, or holding voxel diffs, a tick or work in flight.
- Castles built in C++ for the trials.
- The trial's rules on the client.

## Left open

- **Hold's scenarios and waves** (M6), which add a build window and scored waves to the same format.
- **A scenario editor.** The JSON is written by hand or by a tool under `Tools/`.
