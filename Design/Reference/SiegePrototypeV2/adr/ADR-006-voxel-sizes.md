# ADR-006: Voxel sizes, 1 m world and simulation, 0.5 m models

- **Status:** Proposed (2026-10-31). Accepted or rejected by the M1 renderer spike's measurements.
- **Relates to:** ADR-001 (voxel renderer), ADR-002 (authoritative simulation), ADR-003 (world as seed plus edits), MvpPlan › Decisions to record (render and simulation voxel sizes).

## Context

The design asks for voxels "small enough that a crater … has the shape of whatever made it". A visual prototype compared three options at the strategy camera:

1. a 0.5 m world;
2. a 1 m world;
3. a 1 m world with 0.5 m models for units, engines and debris.

Findings:
- **A 0.5 m world** reads well up close, but terrain looks restless. It costs 8× the voxels of 1 m (MVP map at about 64 m deep: about 134 million against about 17 million), and the support search, slump, navigation rebuilds and stored edits all scale with that count.
- **A 1 m world** gives calm, legible terrain and structures. But units drawn in the world grid lose every silhouette cue: spearmen and sappers become identical.
- **Units are already separate entities** (MvpPlan: "units as small oriented voxel models"), so their resolution need not follow the world's.

## Decision (proposed)

- **World voxel = simulation voxel = 1 m.** This covers terrain, templates, earthworks, rubble and trees.
- **Model voxel = 0.5 m** for units, siege engines, mantlets, carts, banners and client debris. Models are drawn through the ADR-001 pipeline and never written into the world.
- **Standard stone wall 2 m thick; thick wall 3 m.** Cost still equals volume.
- **Model AO samples the world grid** (contact shading). World AO ignores models.
- **Collapse debris:** the simulation result is 1 m rubble. The client adds deterministic 0.5 m model debris, keyed by collapse id, that never affects play.

## Consequences

- **Good:** about 8× fewer simulated voxels, which retires most of the "support search or navigation too slow" risk. Terrain and walls read cleanly at strategy zoom. Units keep distinct silhouettes. A world voxel is about 13 px at the default zoom and about 4 px fully out, so the world needs no LOD.
- **Cost:** craters are coarser, so the weapon reads from crater size and depth more than shape. GameDescription wording and the balance targets need retuning, as do the trebuchet breach times. There are two voxel sources in one renderer, plus model-to-world AO sampling.
- **Neutral:** the template authoring format is unchanged, only its voxel size.

## To be measured in M1 (fill in)

| Measure | 1 m world + 0.5 m models | 0.5 m world |
|---|---|---|
| Voxel pass, default zoom, 1080p, RTX 3060 (ms) | | |
| Edit-to-screen latency (frames) | | |
| Support search over a breach region (ms) | | |
| Navigation tile rebuild (ms) | | |
| Memory for MVP map + a season of edits (MB) | | |
| Breach-prediction accuracy (M3, median) | | |

**Accept if** the baseline meets the pass mark and M3 prediction accuracy reaches at least 0.65. **Otherwise** revisit the decision with the 0.5 m figures.
