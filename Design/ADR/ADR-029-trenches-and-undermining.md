# ADR-029 — Trenches and undermining: sappers dig open boxes, and earth slumps from under a structure only towards what a trench opened

**Status:** Accepted, 2026-11-02, with order M4-D6 ([MvpPlan-M4.md](../MvpPlan-M4.md)), which first implements it. The owner chose its third decision on 2026-11-02 (GameDesign.md, *M4's phase D*, item 1); the rest is how D6 builds MvpPlan.md's input 12.

## Context

- **The design's undermining** (GameDescription.md, *Undermining*; GameDesign.md, *Where RTS logic meets destruction*): undermining is open digging, never tunnelling. Sappers cut a trench beside a wall's foundation, the earth under the foundation slumps into it, and the wall loses support and falls. It works only where the wall stands on earth. A dig always leaves a ramp out.
- **Input 12** (MvpPlan.md, M4): sappers dig 0.2 m³ a second each; the spoil leaves play; a trench is dug from the top, each layer leaving a ramp at the end nearest the squad; the order is a box the player drags. The route table had six sappers bring E3 down in about 198 s on its 4 m foundation, digging 224 m³ under its slump model (Review/PaperTests/SiegeRouteTable.md, fix (c)).
- **M2's slump moves a column's top voxel alone** (MvpPlan-M2.md, M2-E6): earth under a structure never moves, so a trench beside a wall takes nothing from under it. Letting earth under every structure slump broke seven of the M2 and M3 suites' tests, among them the breach run, which fell five times where it fell once, and M3's breach lab, whose committed keys stopped reproducing (measured on M4-D6's tree, 2026-11-02, by a prototype not kept).
- **Support runs sideways through earth** ([ADR-004](ADR-004-rule-based-destruction.md), decision 2): with the earth under E3's foundation gone and the trench beside it dug, E3 still stood, held by the column of courtyard earth against its inner side; with that column gone too, it fell, 80 voxels (measured by a probe on the Siege castle, 2026-11-02, not kept).

## Decision

1. **`DigTrench` sets a squad of sappers digging a box,** both corners included. It is refused `Invalid` for a squad that is not sappers, a box with a corner past the other or outside the world, or a lead out of reach with no cell foot can use beside the box; `NotOwned` for another player's squad. The squad's order becomes `Trench`; it holds the box, the end its ramp is left at and its progress, saved at the squad's record version 6. The ramp's end is the end of the box's long axis nearer the squad's lead, x for a square box, the lower end on a tie. A lead within reach digs where it stands; one farther walks to the nearest cell foot can use on a ring about the box. While set to dig, a sapper takes no target, as a moving squad takes none, and strikes only what touches it.
2. **The dig runs in step 2, after repair.** Each sapper of the squad within 2 cells of the box across the ground, inside it included, the depth of a squad's two ranks, adds its kind's `digCubicMetersPerTick`; the squad gains the sum while the box has a voxel to dig, and at each whole cubic metre digs one as an edit to empty, its spoil leaving play. **A voxel to dig** is loose, earth, grass or rubble, with nothing on it; layer k of the box, counted from its top, is dug only in the columns k or more cells from the ramp's end along the long axis, so the floor there rises a voxel a cell, which foot climb. The highest goes first, then the farthest from the ramp's end, then the lowest column index. Rock, building stone, bedrock and structures are never dug: a trench stops at rock. What slumps into the box is dug again. **A dug cell within `repairLockReachVoxels` of a structure's footprint locks its repair** ([ADR-024](ADR-024-structures-at-work.md), decision 4).
3. **Earth slumps from under a structure only towards what a trench opened.** Each column a trench digs in is opened, and saved. In step 7, a column a structure tops gives the loose voxel it rests on, or the one below the gap under it, to the lowest opened neighbour more than a voxel below it, ties north, east, south, west, and is opened itself. A neighbour a structure tops offers the gap under it, at the highest solid voxel below the gap. And loose material beside an opened column that a structure tops sees it at that same height, so that the courtyard's earth drains into the gap under a foundation as the trench's side does. Every other slump is M2-E6's. Nothing changes where no trench has dug.

## Consequences

- **Measured** by `TrenchTests` (the suite built with clang on Linux, 2026-11-02):
  - six sappers dig a 2 × 8 × 4 m box in flat earth, with slump off, in 868 ticks: its 52 m³, the ramp leaving 1, 2 and 3 m in the three cells at its end, at six times 0.01 m³ a tick as Q16.16;
  - a foot soldier can climb out from every cell of it throughout;
  - one sapper of six killed, the dig gains five sappers' rate a tick.
- **E3 falls at 285 s, after 342 m³,** against the route table's 198 s and 224 m³. That is with an 8 × 8 m box against its outer face, x 368 to 375, its ramp at its east end, so the trench is 8 m deep the whole length of the wall; the castle with slump on, six of the defender's sappers. Boxes 6 or 7 m wide, whose ramps leave the trench short of the earth's bottom at the wall, did not bring it down in 20 minutes, by variants of that test not kept. A 2 m trench along the wall is deep only at its far end, since its ramp takes its length, and undermines nothing. So the voxel version's undermining is about 1.4 times slower than the route table's and needs a box reaching out from the wall; GameDesign.md's three minutes for undermining (*the paper tests' values*, item 15) is not met, and the owner tunes it in M5.
- **On rock nothing falls:** the same box against E2, on 1 m of soil over common rock, takes the soil and stops.
- **A crater or a ditch beside a wall on earth does not undermine it,** since neither is a trench. The Siege castle holds no such wall: E3 alone stands on earth, with nothing before it.
- **The save's format version is 3,** for the opened columns, after the columns waiting for slump.

## What this forecloses

- Tunnelling, and a trench that digs rock.
- Slump from under structures anywhere a trench has not opened.
