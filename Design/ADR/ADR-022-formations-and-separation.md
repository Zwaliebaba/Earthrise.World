# ADR-022 — Formations and separation: one figure a cell, reserved in unit-id order

**Status:** Proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). It is the ADR that [ADR-015](ADR-015-navigation.md), decision 6 and *Left open*, schedules for M4: how a squad's figures arrange about its path and how they keep apart. It binds nothing until the owner gives it the go-ahead (MvpPlan.md, M4, input 1), and order M4-C2, which first implements it, marks it Accepted (AGENTS.md §6).

## Context

- **M2 moves a squad in single file** (ADR-015, M2-F5): one A\* path from the lead's cell, a trail of the figures' own cells behind it, each figure stopping as many cells short of the trail's end as it stands behind the lead. Figures may pass through each other, and two may stand in one cell.
- **M4's done-when asks for the opposite** (MvpPlan.md, M4): "a squad sent through a breach passes it with no two figures overlapping, at a rate the breach log records and that replays identically". The rate is what makes a narrow breach a choke point, which is pillar 2's tactics: a 1 m slot is not a 5 m collapse.
- **The cell is 1 m and a figure stands on 0.5 m** (GameDescription.md, *Combat model*): a foot soldier is 2 m tall on a 0.5 m square, and a gap one voxel wide and two high is a route for foot. Navigation knows cells, not sub-cells (ADR-015, decision 1); clearance and reach are per cell.
- **Melee reaches the next figure** (ADR-023): a figure fights an enemy in one of its eight neighbouring cells. Where figures stand therefore decides who fights whom.
- **Everything is in the deterministic tick** (ADR-013): fixed point, no float, order fixed by ids, work budgeted in units.
- **Nothing pushes a unit** (ADR-004, decision 6; ADR-015, *Left open*): separation has to be a rule about where a figure may step, not a force.

## Decision

1. **One figure a cell.** A figure of foot or cavalry occupies the cell it stands in; a machine occupies the square of cells its class's width covers, with its cell at the minimum corner, as clearance reads it (ADR-015, decision 2). No two units occupy one cell at the end of any tick. Garrisoned units occupy nothing.
2. **A step is reserved before it is taken.** A figure that starts a step into a cell reserves it, and keeps its own cell until it passes the halfway point, as M2's figures already stand in the cell they left until halfway. In step 3, figures choose their next step in unit-id order: a figure may reserve a cell no unit occupies or has reserved. A figure that cannot reserve waits where it is, spending no effort, and tries again the next tick.
3. **Two friendly figures that want each other's cells swap.** When a figure's wanted cell is occupied by a figure of the same owner whose wanted cell is the first figure's own, both start the step together, which is the one case two figures cross. Without it, two squads meeting head-on in a 1 m breach would stand forever. Enemy figures never swap: they fight (ADR-023).
4. **A squad keeps a formation about its lead.** The lead walks the squad's path as in M2. Every figure has a slot, the lead's the middle of the front rank: foot squads of six stand in two ranks of three, outriders in one rank of three, 1 m apart, facing the direction of the lead's last step, one of eight. A figure walks towards its slot's cell, by the step among its eight neighbours that most reduces the octile distance to it along a link open to its class (ties to the lower link index). A slot whose cell the class cannot use, or that is more than 3 cells off the lead's trail, is dropped, and its figure follows the trail as in M2. **So a squad walks in ranks on open ground and funnels into single file through a gap,** with no extra search.
5. **The slots resolve at the goal.** On arrival the squad's slots are laid about the goal cell, facing the last step; a slot that cannot be used takes the nearest usable cell, by octile distance with ties to the lower index, that no other slot holds.
6. **A squad blocked for long searches again.** A figure that has waited `waitTicksBeforeSearch` ticks in a row (Proposed: 40, two seconds) marks its squad to search again, from the lead's cell, as ADR-015, decision 5, does after a rebuild; an occupied cell costs the search nothing extra, so this is a retry, not an avoidance.
7. **The breach's rate falls out of the rule, and is measured, not set.** A 1 m gap passes one figure at a time. On rubble at half of foot's 1.4 m/s, one cell a figure, that is about one figure each 1.4 s, about 42 a minute (arithmetic on the rule, not a measurement); the done-when's log records the real rate.

## Consequences for the code

- **`Simulation`'s movement** (M2-F5's `FormTrail` and `Walk`) gains reservations, slots and the swap, in step 3, in unit-id order. A unit's record gains its reserved cell and its wait count, saved with it ([ADR-014](ADR-014-edits-saves-and-replays.md), decision 4; the unit's record version rises).
- **Step 9's drop rule** (ADR-015, decision 7) moves a unit whose cell its class can no longer use to the nearest cell no other unit occupies.
- **Step 9** lifts every unit whose cell a landing filled, as M2-F6 does; two units lifted into one cell are moved apart in the same step, the higher id moving to the nearest free cell.
- **`MovementTests`** keep M2's single-file checks for a squad alone in a 1 m corridor, and gain: no two units in one cell on any tick of the breach run with two squads; two squads meeting head-on in a 1 m corridor pass; the rate through a 1 m gap.

## What this forecloses

- Figures overlapping, and separation by forces, steering or pushing.
- Sub-cell positions for separation: a 1 m cell holds one figure, though a figure is drawn on 0.5 m.
- A formation that changes the path search: the search stays per squad, over cells (ADR-015).

## Left open

- **Formations a player chooses** (line, column, wedge). The MVP has one.
- **Flow fields** for many squads with one goal (ADR-015, *Left open*).
- **Whether 42 a minute through a 1 m slot is the right choke.** It is the rule's, and M5's playtest says whether storming reads.
