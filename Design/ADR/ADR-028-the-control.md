# ADR-028 — The control: health-bar structures behind one seam, chosen by the scenario

**Status:** Proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). It records how the Siege trial's control version is built. Its numbers are the owner's input (MvpPlan.md, M4, input 19). It binds nothing until the owner gives it the go-ahead (input 1); order M4-F4, which first implements it, marks it Accepted (AGENTS.md §6).

## Context

- **The control is the hypotheses' yardstick** (GameDesign.md, *Hypotheses*): the same stages with walls built of segments that have health bars, and resources from nodes that run out. It swaps the collapse resolver ([ADR-004](ADR-004-rule-based-destruction.md), decision 3) and the gathering rule and leaves the rest of the game unchanged. Both versions keep every cosmetic, and the integrity view is on by default, as the health bars are.
- **The HUD's differences** (Hud.md §1): health bars over wall sections, and HEALTH in place of PLATFORM on towers; everything else identical, and a test asserts it.
- **Siege has no gathering,** so the Siege control swaps the walls alone.
- **The balance targets bind both versions:** one trebuchet unopposed opens a standard section in about three minutes, nine shots at 20 s in the route table, and repair at most doubles it ([Review/PaperTests/SiegeRouteTable.md](../Review/PaperTests/SiegeRouteTable.md)).

## Decision

1. **Structures take hits through one seam,** `StructureRule`, with two implementations. `VoxelStructureRule` is M2's and M4's: craters, the support search, the resolver, and critical parts read from standing voxels ([ADR-024](ADR-024-structures-at-work.md)). `HealthStructureRule` is the control's. The scenario's trial names its version, and the world creates one rule for its life; nothing else in the simulation asks which version runs.
2. **In the control, a structure has hit points,** one pool a structure. `STRC` gives each template's (ADR-024, decision 7). An engine's shot that strikes a structure voxel deals the shot's structure damage and removes no voxel of it; a ram's blow deals the ram's, a fourth of it on stone; a sapper's trench deals its damage per cubic metre dug within 2 cells of a section's footprint, and the digging itself goes on, so the trench exists. Shots and blows on terrain crater as in the voxel version; the support search still runs, for terrain alone: structures stand while their hit points last.
3. **At 0 hit points:**
   - **a wall section falls whole,** through the resolver's own collapse, with its warning, its fall and its rubble, so the breach is the whole section's ramp, as a health-bar wall's breach is;
   - **a building stops working** and keeps standing, its garrison put out as in the voxel version;
   - **a gate's voxels are removed,** opening its passage: a gatehouse or palisade gate at 0 loses its gate and stops working, whichever of its voxels took the hits.
4. **Repair restores hit points** at the voxel version's rate per section times what a cubic metre is worth in hit points: the shot's structure damage over the shot's crater in the template's material, 50 a cubic metre for stone at first values (100 over 2 m³). A repair window therefore restores the same share of a shot in both versions, at the same lock and the same stone a cubic metre, so the economy of repair is the same.
5. **The structures' hit points are calibrated, not guessed.** Order M4-F5 measures on the voxel version, unrepaired, how many shots each template in the castle takes to open or to stop, firing from a stand at its weakest point by a fixed pattern for each template, and sets each template's hit points to that many times the shot's structure damage; the measurement is recorded in the commit and in `Structures.json`'s note. F4 starts from first values.
6. **The client draws the control from the same views.** `StructureView` carries hit points in the control and none in the voxel version; the HUD draws a health bar over each section with hit points, and HEALTH in place of the critical part's row on towers, the gatehouse and the keep (Hud.md §1 and §5, amended: Hud.md names the towers' row alone). A test in `ClientTests` builds the HUD model from the same state in both versions and asserts the two differ in those fields alone.

## Consequences for the code

- **`Simulation`** gains `StructureRule` and its two implementations; the shot's, the ram's and the trench's effects on structures call it. **`STRC`** gains hit points, and **`RULE`** each weapon's structure damage.
- **ADR-004's *Consequences*** say the resolver is "one interface with one implementation". The control is the second implementation the design asks for; ADR-004 is amended to say so when this is accepted. The resolver's collapse, which both rules use, stays one.
- **The trial's tests** run in both versions; the control's `SiegeAttacker` wins by storming as the voxel version's does.

## What this forecloses

- A control that differs in anything but how structures take damage and fall: units, AI, fog, the HUD and the cosmetics stay the same.
- A health-bar wall that falls in pieces: it falls whole, or stands.

## Left open

- **Hold's control,** with its resource nodes and Hud.md's GATHER (M6; GameDesign.md, *Open questions*).
- **Damage drawn on a control wall.** Both versions keep every cosmetic, and a control wall shows no crater, since none is cut; the owner judges whether a health-bar wall needs a cosmetic of its own.
