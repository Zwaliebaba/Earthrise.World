# ADR-006 — The world and the simulation use 1 m voxels; models use 0.5 m

**Status:** Proposed, 2026-10-31. The owner adopted it as the design baseline the same day ([GameDesign.md](../GameDesign.md)). This ADR is accepted when the M1 renderer spike fills in the measurements below and they meet the acceptance condition. It comes from the owner's second asset package, whose draft is kept unchanged in [Reference/SiegePrototypeV2/](../Reference/SiegePrototypeV2/). It supersedes the earlier choice of 0.5 m for everything ([RendererLook.md](../RendererLook.md), first version).

## Context

GameDescription.md wants voxels small enough that a crater, a ditch or a breach shows what made it. A visual prototype compared three options from the strategy camera:

1. a 0.5 m world;
2. a 1 m world;
3. a 1 m world, with 0.5 m models for units, engines and debris.

What it found:

- **A 0.5 m world reads well up close, but its terrain looks restless.** It holds eight times the voxels of a 1 m world. The MVP map, at about 64 m deep, is about 134 million voxels at 0.5 m against about 17 million at 1 m. The support search, slump, navigation rebuilds and stored edits all scale with that count ([ADR-003](ADR-003-world-as-seed-plus-edits.md), [ADR-004](ADR-004-rule-based-destruction.md)).
- **A 1 m world gives calm, legible terrain and structures,** but units drawn in it lose their cues: spearmen and sappers become identical.
- **Units are already entities, not voxels in the world** ([ADR-002](ADR-002-authoritative-simulation.md)), so their resolution need not follow the world's.

The owner also plans the Surface Pro, whose integrated GPU the spike measures, and later Android ([ADR-007](ADR-007-platforms-and-input.md)). Both favour the cheaper world.

## Decision

1. **The world voxel is the simulation voxel, and it is 1 m.** That covers terrain, every template, earthworks, quarries, craters, rubble and trees. Support, collapse, slump, navigation, edits and cost all run on it.
2. **Models use 0.5 m voxels.** That covers units, siege engines, mantlets, carts, banners and the client's debris.
   - A model is drawn through ADR-001's pipeline at its entity's position.
   - It is rotated in 90° steps, so its voxels stay axis-aligned.
   - It is never written into the world grid.
   - Moving parts, such as the trebuchet's arm, switch between baked poses.
3. **Walls are 2 m thick, and thick walls 3 m.** A building still costs exactly its volume, so a standard section of 8 × 2 × 6 m costs 96 m³ and a thick one costs half as much again.
4. **Models see the world, but the world ignores models.** A model's corner shading counts a neighbour as solid if either the model voxel or the world voxel containing it is set. That contact shading keeps units from floating. The world's shading never samples models.
5. **Collapse debris is cosmetic.** The simulation's result is 1 m rubble. The client scatters 0.5 m model debris over it, deterministic from the collapse's id, and that debris never blocks a path or a shot ([ADR-004](ADR-004-rule-based-destruction.md), decision 7).
6. **Units switch to impostors when they get too small.** When a model voxel drops below about 3 px on screen, a unit is drawn as a 1 × 1 × 2 world-scale impostor: faction colour, plus a helmet or a hood.

## Consequences for the code

- **`Engine` draws two voxel sources in one pass:** world chunks of 1 m voxels, and model instances of 0.5 m voxels. Both use the same ray-box test and conservative depth. `Engine` knows voxels and palette indices, never units (R9).
- **Chunks of 32³ voxels are 32 m across.**
- **Model contact shading needs the world's occupancy around each model.** The spike chooses between baking it per instance on the CPU and sampling a small occupancy volume in the shader.
- **The asset format is unchanged** ([ADR-005](ADR-005-content-formats.md)). Each file states its own `voxelSize`.
- **About eight times fewer simulated voxels.** That retires most of the risk that the support search or navigation is too slow.
- **No stochastic pruning.** A world voxel is about 13 px at the default zoom and about 4 px fully zoomed out, so the world needs no level of detail.
- **Craters are coarser.** A weapon now reads from a crater's size and depth more than its shape. Crater radii and the balance targets need tuning in 1 m voxels.

## What this forecloses

- Simulated detail finer than 1 m, such as a 0.5 m step in the terrain or a 1.5 m wall.
- Models written into the world grid, and units drawn in it.
- Debris that affects play.

## To be measured in M1

| Measure | 1 m world + 0.5 m models | 0.5 m world |
|---|---|---|
| Voxel pass, default zoom, 1080p, RTX 3060-class (ms) | | |
| Edit-to-screen latency (frames) | | |
| Support search over a breach region (ms) | | |
| Navigation tile rebuild (ms) | | |
| Memory for the MVP map plus a season of edits (MB) | | |
| Breach-prediction accuracy in M3 (median) | | |

**Accept if** the 1 m baseline meets the pass mark and the M3 prediction accuracy reaches a median of 0.65 or more. **Otherwise** revisit the decision with the 0.5 m figures.

## Left open

- Redrawing the world templates natively at 1 m. Today's are automatic conversions from 0.5 m, to be redrawn before the M4 Siege castle is final.
- Crater radius and depth for each weapon and material, in 1 m voxels.
- The method for model contact shading.
- Platforms beyond the Windows PC.
