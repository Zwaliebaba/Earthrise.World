# Proposed design-document changes (Look v2)

Apply these as **Proposed**. They become final when ADR-006 is accepted after M1. Quote the current text exactly when editing, and keep each document's voice.

## Design/GameDescription.md

| Section | Current | Proposed |
|---|---|---|
| The land › Materials (intro) | "Everything solid is made of voxels small enough that a crater, a ditch or a breach has the shape of whatever made it." | "Everything solid is made of 1 m voxels. A crater's size and depth show which weapon made it, and a ditch or breach has the outline of the work that cut it. Dust and loose blocks are drawn by the client and never change what happened." |
| The economy › Resources | "A stone wall section of 8 m × 1.5 m × 6 m costs 72 m³ of stone." | "A stone wall section of 8 m × 2 m × 6 m costs 96 m³ of stone." |
| Building › Fortifications | "Stone wall, standard (1.5 m thick)" | "Stone wall, standard (2 m thick)" |
| Building › Fortifications | "Stone wall, thick (2.5 m thick) … costs two thirds more stone" | "Stone wall, thick (3 m thick) … costs half as much again in stone" |
| How a siege plays › Balance targets | the trebuchet breach times of about 3 and 6 minutes | Keep the targets, and add: "tuned for 2 m and 3 m walls on the 1 m grid" |
| How it is built | (add a bullet) | "**Two voxel sizes.** The world and the simulation use 1 m voxels. Units, engines and debris are 0.5 m voxel models drawn over the world and never written into it." |
| Readability › camera | (add) | "At the default zoom a world voxel is about 13 px on a 1080p screen." |

## Design/GameDesign.md

- **Decisions:** add "Voxel sizes: 1 m world and simulation; 0.5 m models (ADR-006, Proposed)".
  - If *Decided 5* covers a coarser simulation voxel at an integer ratio to the render voxel, note that the ratio is now 1:1 for the world, and that models are not simulated.
- **Hypotheses (H2):** no change to the hypothesis. Add to its test notes that the integrity view reads 1 m sections.
- **Open questions:** add "Do 1 m craters let players tell a ram, a mangonel and a trebuchet apart?", tested in the M3 breach lab.

## Design/MvpPlan.md

See `INTEGRATION.md › Phase 1, step 3`.
