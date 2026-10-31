# Earthrise.World — Look v2 package (1 m world · 0.5 m models)

This package proposes a **design change** and holds everything needed to adopt it:

- **The world, and the simulation, run on a 1 m voxel grid:** terrain, every template, earthworks, rubble and trees.
- **Units, siege engines, mantlets and debris are separate 0.5 m voxel models.** They are drawn over the world and never written into it.

Start with **`INTEGRATION.md`**, the step-by-step guide for Claude Code. `specs/Renderer Look Spec v2.md` is the source of truth for size, colour and shading.

## Contents

```
README.md                 ← this file
INTEGRATION.md            ← how to fold this into the repo, in order, with done-checks
commands/integrate-look-v2.md ← drop into .claude/commands/ to run the integration as /integrate-look-v2
adr/ADR-006-voxel-sizes.md ← proposed ADR (Status: Proposed), ready to copy into Design/ADR/
design-changes.md         ← exact text changes for GameDescription.md and GameDesign.md
manifest.json             ← all 21 assets: grid, voxel size, size, cost, critical part, files
palette.json              ← index → name, hex, jitter, AO set, grid, simulation material
specs/Renderer Look Spec v2.md
assets/
  world/templates/   T1–T9  1 m grid  (.vox .json .png)
  world/earthworks/  E1–E2  1 m grid
  world/materials/   M1–M2  1 m grid
  models/units/      U1–U5, S2 (sappers)   0.5 m models
  models/siege/      S1 ram, S3 trebuchet  0.5 m models
reference/
  Earthrise Siege Scene v5.dc.html         ← the look, in a browser
  Earthrise Siege Scene v4 Mobile.dc.html  ← landscape phone layout (v4 detail)
  MVP Asset Sheet.dc.html                  ← generator for every asset; re-export from here
  support.js                               ← needed by the .dc.html files
```

The reference pages need an internet connection, because they load three.js from a CDN. Open them directly in a browser.

## Status of the assets

- **World templates (T, E, M)** were converted automatically from 0.5 m authoring by majority vote over 2 × 2 × 2 blocks. They are correct in size and cost on the 1 m grid, and fine for M1 and M2. **Redraw them natively at 1 m** before the M4 Siege castle is final. `manifest.json › authoring` marks each one.
- **Models (U, S)** are native 0.5 m.
- **Decisions made in this package** that the design docs don't fix are listed at the end of `INTEGRATION.md`.
