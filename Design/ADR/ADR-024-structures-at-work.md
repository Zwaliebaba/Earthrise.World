# ADR-024 — Structures at work: the built mask, critical parts, repair and its lock, gates and garrisons

**Status:** Proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). M3 makes structures entities with a template, an origin, a turn and a name (MvpPlan-M3.md, M3-B1). This records what a structure does in M4: whether it works, how it is repaired, how a gate opens and how a garrison fights. The numbers are the owner's inputs (MvpPlan.md, M4, inputs 10 to 13). It binds nothing until the owner gives it the go-ahead (input 1); order M4-D1, which first implements it, marks it Accepted (AGENTS.md §6).

## Context

- **The design's rules** (GameDesign.md, *Where RTS logic meets destruction*; GameDescription.md, *Templates* and *Fortifications*):
  - a building stops working when a critical part falls below its threshold, the share of the part's voxels still standing and supported, and is a ruin until repaired;
  - repair puts back voxels once built and later removed, pays for their material, works from the owner's side at a fixed rate per section however many laborers are on it, and not on a section hit in the last ten seconds by an engine shot, a ram blow or a trench;
  - towers and keeps garrison and fire; garrisoned ranged units fire from height, their shots start at the outer face of their tower, and they heal slowly; when a tower stops working its garrison is put out at its foot, on its owner's side and clear of where it would fall;
  - the gatehouse opens for its owner, its timber gate is its weak point, and its garrison fires down into the passage in front of the gate with crush damage a ram's roof does not stop.
- **[ADR-005](ADR-005-content-formats.md), decision 10:** a saved structure holds its template, origin, turn, name and a mask of which voxels have been built. M3's record is version 1 without the mask.
- **The templates mark their parts** (ADR-005; `ServerTemplate::parts`): critical, foundation and stock, and each critical part's name: T2's great tower, T4's firing platform, T5's and T7's gate. Nothing reads them yet.
- **The v3 asset package decided the gate's form** (GameDesign.md, *the MVP assets v3 package*, item 3): the gate's voxels stay in the world as T5's and T7's critical part, so cost, damage and craters are unchanged; open or shut is a flag on the instance; while open the owner's units pass and shots still stop; the client draws G1 or G2 in place of its voxels.
- **The route table's values** ([Review/PaperTests/SiegeRouteTable.md](../Review/PaperTests/SiegeRouteTable.md)), Proposed: repair at 0.08 m³ a second per section, a gate working while half its 12 voxels stand, six archers a garrison, reach 40 m from the outer face at two steps.

## Decision

1. **A structure records which of its voxels were built,** as ADR-005, decision 10, says: the record rises to version 2 with the mask, one bit per template voxel in the template's own order. A scenario's structures start fully built. A template voxel is **standing** when the world holds, at its turned position, the index the template puts there and no collapse in warning or falling holds it.
2. **Working is read from the critical part.** A structure with a critical part works while the share of the part's built voxels that stand is at least its threshold, in whole percent, from the `STRC` section (decision 7). The share is kept per structure and updated in step 10 from the tick's edits and from any collapse holding its voxels that started or ended in the tick, so it costs nothing on a quiet tick. A structure that stops working raises an event; its garrison is put out (decision 5), and a gate that stops working is shut for good until repaired above its threshold.
3. **Repair is a job of the owner's laborers, at a fixed rate per section.** A structure is repaired while at least one of its owner's laborers, ordered to repair it, stands within 3 cells of its footprint on its owner's side: the side of the footprint's centre line nearer the owner's keep. It gains `repairCubicMetersPerSecond` of progress a tick, as Q16.16 a tick; each whole cubic metre restores one removed built voxel, lowest first, then nearest the footprint's centre, then by template order, into an empty cell only, and takes one cubic metre of the voxel's material from the owner's stockpile. A voxel that would stand unsupported is skipped until one below it is back. Rubble in a voxel's cell stops that voxel; clearing rubble is a designation, which M4 does not build.
4. **The lock.** An engine shot whose strike or crater touches a structure, a ram blow on it, or a trench cell dug within 2 cells of its footprint sets its `repairLockTicks` to 10 s of ticks; repair waits while it is above 0, and it counts down in step 2.
5. **A garrison is units inside a building.** `Garrison` moves a squad into a building of its owner that works and has room, when its lead reaches the building's footprint; garrisoned units occupy no cell, are invisible to everyone but their owner, take no damage, heal one hit point every `garrisonHealSecondsPerHitPoint`, and fire if ranged, from the point of the building's outer face nearest the target, at the building's stand height, so their elevation steps count from there (ADR-023, decision 5). `Eject`, or the building stopping working, puts them out on the nearest cells on the owner's side clear of the building's footprint and its fall strip, in unit-id order.
6. **The gate is a flag, and the passage attack is the gatehouse's.**
   - **Open:** `SetGate` by its owner opens or shuts a working gate. While it is open, navigation reads the gate's voxels as absent, for every unit (MvpPlan.md, M4, input 13): a sally can be followed in, and the defender's AI shuts it when an enemy comes near. Shots and craters still treat the voxels as solid; the client draws G1 or G2 in their place.
   - **The passage attack:** while a gatehouse works and holds at least one garrisoned unit, every `passageAttackSeconds` it deals `passageAttackDamage` of crush to every unit, the ram included, in the passage's cells outside the gate. The ram's roof is pierce armor and does not stop crush.
7. **Per-template values are a server-data section, `STRC`,** baked from `GameData/Server/Structures.json`: each template's critical threshold, garrison capacity, stand height, its vision bare and garrisoned, and its hit points in the control ([ADR-028](ADR-028-the-control.md)). The templates' own files stay shared content; these values are the server's to tune (ADR-008).

## Consequences for the code

- **`Simulation::Structure`** gains the mask, the part's share, working, the lock, the gate's flag and the garrison, saved and hashed at record version 2; **`StructureView`** carries working, the share, the lock, the gate and the garrison's count.
- **Navigation** reads an open gate's cells as air when it rebuilds them, and `SetGate` marks those tiles for step 8, as an edit would.
- **Step 2** gains repair and the lock's countdown; **step 4** the garrisons' arrows and the passage attack; **step 10** the parts' shares and their events.
- **The integrity reading** (MvpPlan-M3.md, M3-B5) gains the lock's ticks, so the view shows "Locked · n s" (GameDescription.md, *Readability*).

## What this forecloses

- Repair that adds voxels never built, builds outside a template, or runs faster with more laborers.
- Garrisoned units that can be hit, or a garrison that fires from inside the footprint.
- A gate that changes the world grid when it opens.

## Left open

- **Construction,** M6's, which fills the mask from empty.
- **Clearing rubble,** M6's designation, which a breach's repair waits on.
- **Each template's threshold,** first values in MvpPlan.md, M4, input 10, for M5 to tune.
- **Whether only the owner's units should pass an open gate.** That needs navigation per owner, which ADR-015 does not have; input 13 records the choice.
