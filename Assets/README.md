# Assets

The game's voxel content. Nothing here is compiled or loaded directly: `Tools/BakeContent.py` bakes it into the `Content` library at build time. [ADR-005](../Design/ADR/ADR-005-content-formats.md) is the format, and [Design/MvpAssets.md](../Design/MvpAssets.md) is the catalogue.

| Path | Holds | Read by |
|---|---|---|
| `Templates/` | T1–T9, the buildings | The simulation and the client |
| `Models/` | One figure for each unit type (U1–U5, S2), the ram (S1) and the trebuchet (S3) | The client |
| `Illustrations/` | Squads (U1–U5, S2), earthworks (E1, E2) and material samples (M1, M2) | Nobody: documentation and tests |
| `Palette.json` | Material index → colour, jitter, jitter cell, AO set, behaviour | The baker |
| `Manifest.json` | Every asset: id, kind, files, size, voxel count, counted cost, critical part | The asset check |

## Working with assets

- **The `.json` layer file is the source.** Edit it by hand, or edit the `.vox` in MagicaVoxel and convert it back with the tool. The two must agree voxel for voxel.
- **The `.png`** is a preview only.
- **Costs are counted, not typed.** If a template changes, its expected cost in `Manifest.json` changes in the same commit.

## Where these came from

These files come from the owner's package, which is kept unchanged in [`Design/Reference/SiegePrototype/`](../Design/Reference/SiegePrototype/). Six things were changed on the way in:

- **Sorted** into templates, models and illustrations, and **re-pathed** in `Manifest.json`.
- **One figure for each unit type** (`Models/earthrise-u*-*.json` and `earthrise-s2-sapper.json`), cut from each squad. Each figure is the squad's first connected figure. Every squad's figures were checked to be identical first.
- **Template costs recounted** under ADR-005, with decorations costed as the material they sit in. The package left openings, fittings and banners out, so T1, T2, T4, T5, T7 and T8 cost slightly more than the package said. Each entry keeps the package's figure as `packageCostM3`.
- **The palette gained a `behavior` for every index**, and seven entries from [RendererLook.md](../Design/RendererLook.md): leaves A and B, water, fields A and B, road, and rock rubble. Index 6 is renamed from "dressed stone" to "masonry". The colour for rock rubble is provisional.
- **Known issues** are recorded on two entries:
  - E1's rampart holds 44 m³ against 40 m³ dug, and is topped with grass.
  - T7's grid has 1 m of empty padding in front, which the baker trims.
- **The legend names inside the asset files are left unchanged.** For example, they still say "dressed stone". The palette's name is the one that counts.
