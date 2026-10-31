# Earthrise.World — MVP voxel asset package

A handoff package for Claude Code. It holds every voxel item that `Design/MvpPlan.md › Scope › In` needs, built to `specs/Renderer Look Spec.md`. The look spec and this package are the source of truth for **size, colour and shading**. `Design/GameDescription.md` and `Design/GameDesign.md` govern rules and behaviour, and `AGENTS.md` governs how the code is written.

## What's inside

```
earthrise-mvp-assets/
  README.md                ← this file (start here)
  manifest.json            ← every asset: id, role, size, voxel count, cost, critical part, files
  palette.json             ← material index → name, sRGB hex, jitter, jitter cell, AO set
  specs/
    Renderer Look Spec.md  ← voxel size, colouring, jitter, AO, lighting, camera, acceptance checks
    Siege Scene Prompt.md  ← the key-art scene, for the renderer spike's test scene (M1)
  assets/
    templates/   T1–T9  hall, keep, stone wall (standard), tower, gatehouse, palisade, palisade gate, quarry yard, lumber camp
    earthworks/  E1–E2  ditch & rampart, deep foundation
    units/       U1–U5  laborers, spearmen, archers, outriders, men-at-arms (squads of 6 / 3)
    siege/       S1–S3  battering ram, sappers, trebuchet
    materials/   M1–M2  ground strata, rubble that keeps its material
    (each asset: .vox  .json  .png)
  reference/
    MVP Asset Sheet.dc.html          ← live sheet that generated these files; open in a browser (needs support.js beside it)
    Earthrise Siege Scene v4.dc.html ← key scene at the same look
    support.js
```

## Conventions

- **Voxel size:** 0.5 m. One voxel holds 0.125 m³ of material. Terrain heights quantize to 1 m.
- **Axes:** y is up and +z is the front of a template (the side facing the attacker or the camera). The origin is the template's min corner.
- **`.json` template layers:**
  - `layers[y][z]` is a string over x, one character per voxel. `legend` maps each character to `{material, name, hex}`, and `.` means empty.
  - `parts[y][z]` uses the same layout: `1` critical part, `2` foundation, `3` stock (goods on site, not costed), `.` none.
  - This matches the MVP plan's template format, "voxel layers with material and critical-part tags", so the Python tool under `Tools/` can read it directly.
- **`.vox`:** MagicaVoxel 150. Palette index = material index from `palette.json`. MagicaVoxel is z-up, so `vox = (x, depth-1-z, y)`.
- **`.png`:** an 880 × 660 render with a transparent background and the default golden light. Templates have a 2 m laborer beside them for scale.
- **Faction:** palette index 16 is the faction colour, exported red (#b23a2e). Swap it per faction at draw time (blue #2f5d93). Only units and banners use it.
- **Cost:** `costM3` is the exact counted volume, because buildings cost exactly the volume they are made of. Stock (part 3) is excluded. The deep foundation is reported separately and depends on the depth to bedrock at the site; the asset shows 3 m.

## Material indices (Engine sees palette indices only, per R9)

| idx | material | hex |  | idx | material | hex |
|---|---|---|---|---|---|---|
| 1 | grass | #7d8b4f | | 18 | iron | #9aa0a6 |
| 2 | earth | #8a6a48 | | 19 | laborer cloth | #c79a6b |
| 3 | common rock | #8d8a84 | | 20 | opening | #2e2a27 |
| 4 | building stone | #ddd2b9 | | 21 | counterweight | #5b5650 |
| 5 | bedrock | #4a4744 | | 22 | skin | #d9a57f |
| 6 | dressed stone | #c6bca7 | | 23 | cloth | #3d3530 |
| 7 | timber | #7a5534 | | 24 | horse | #5e4330 |
| 8 | stone rubble | #a69c8a | | 25 | timber rubble | #5f4530 |
| 12 | roof | #8c4a3a | | 26 | earth rubble | #7d6044 |
| 16 | faction | #b23a2e | | 27 | leather (hides, hoods) | #6e5a3e |

**Simulation vs render materials.** The simulation's MVP materials are earth, common rock, building stone, bedrock, timber and rubble (rubble records the material it fell from). Indices such as grass, dressed stone, roof, opening, horse, skin and leather are **render-only**: they are presentation of a simulation material, or unit cosmetics.
- Grass is the top face of earth.
- Dressed stone, opening and counterweight are stone; roof and leather are timber.

Keep that mapping in `Protocol` / `Client`, never in `Simulation` rules.

## How to use this in the design (suggested order)

1. **M1 renderer spike.** Implement `specs/Renderer Look Spec.md`: palette, 1 m terrain jitter cells, the two AO sets and the lighting presets. Use `Siege Scene Prompt.md` as the walled-town or ditched-wall test scene, and its acceptance checks as part of the spike's pass criteria.
2. **M4 templates.** Write the `Tools/` template tool so it reads these `.json` files. Load T1–T9, E1 and E2 through it, and check that the counted costs match `manifest.json`. T3 must come to exactly 72 m³.
3. **M4 units.** Draw U1–U5 and S1–S3 as small oriented voxel models through the ADR-001 pipeline, at the sizes given here.
4. **Consistency checks** for `SimulationTests` or the tool:
   - every template's critical part is non-empty where `criticalPart` is set;
   - no voxel in a template is a render-only index the simulation can't map;
   - costs are recomputed, not copied.

## Decisions made here that the design docs don't fix (confirm before treating as final)

- **Proportions** of the hall (11 × 7 m), keep (12 × 12 × 16 m with a forebuilding), gatehouse (9 × 7 × 11 m) and the drop-site sheds.
- **Critical parts** for the hall ("hall frame") and the drop sites ("shed"). The docs name only the gate, the firing platform and the great tower.
- **Hollow interiors.** Towers and the keep are hollow (1.5 m walls with floors), so they cost less than their outline.
- **Ditch profile.** It is 5 m wide at the top and 2 m deep with 1:1 sides. The rampart profile is solved so that its volume equals the spoil.
- **Units at 1 × 1 × 4 voxels.** Each unit type is told apart by one silhouette cue (see `silhouetteCue`). If they read too coarse up close, consider a finer voxel for units only, at an integer ratio such as 0.25 m.
- **Render-only colours** for horse, hides, timber rubble and earth rubble.

## Regenerating

Open `reference/MVP Asset Sheet.dc.html` in a browser, with `support.js` beside it and an internet connection to load three.js. It builds every model in code, renders it, and offers .vox, .json and .png per asset. The builders are in its logic class (`builders()`), so a change to proportions or materials belongs there or in the template tool, and everything is re-exported.
