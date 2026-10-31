# Assets

The game's voxel content. Nothing here is compiled or loaded directly: `Tools/BakeContent.py` bakes it into the `Content` library at build time. [ADR-005](../Design/ADR/ADR-005-content-formats.md) is the format, [ADR-006](../Design/ADR/ADR-006-voxel-sizes.md) sets the two voxel sizes, and [Design/MvpAssets.md](../Design/MvpAssets.md) is the catalogue.

| Path | Holds | Voxel | Read by |
|---|---|---|---|
| `Templates/` | T1–T9, the buildings | 1 m | The simulation and the client |
| `Models/` | One figure for each unit type (U1–U5, S2), the ram (S1) and the trebuchet (S3) | 0.5 m | The client |
| `Illustrations/` | Squads (U1–U5, S2) at 0.5 m; earthworks (E1, E2) and material samples (M1, M2) at 1 m | Both | Nobody: documentation and tests |
| `Fonts/` | Earthrise Mason: `EarthriseMason-glyphs.json`, the source, with atlas previews at 1×, 2× and 3× | | The font tool ([Design/Hud.md](../Design/Hud.md) §9) |
| `Palette.json` | Material index → colour, jitter, jitter cell, AO set, grid, behaviour | | The baker |
| `Manifest.json` | Every asset: id, kind, grid, files, size, voxel count, counted cost, critical part, known issues | | The asset check |

## Working with assets

- **The `.json` layer file is the source.** Edit it by hand, or edit the `.vox` in MagicaVoxel and convert it back with the tool. The two must agree voxel for voxel.
- **A file's name says its voxel size:** `-1m` for the world grid, `-05m` for models. Its `voxelSize` field must agree.
- **A palette index belongs to a grid.** `Palette.json` marks each index `world`, `models` or `both`, and a world asset uses only `world` and `both` indices.
- **The `.png`** is a preview only.
- **Costs are counted, not typed.** If a template changes, its expected cost in `Manifest.json` changes in the same commit.

## Where these came from

These files come from the owner's second package, kept unchanged in [`Design/Reference/SiegePrototypeV2/`](../Design/Reference/SiegePrototypeV2/). It replaced the first package, kept in [`Design/Reference/SiegePrototype/`](../Design/Reference/SiegePrototype/). The second package's world assets are its automatic conversions of the first package's 0.5 m drawings; its models are the same 0.5 m drawings. These things were changed on the way in:

- **Sorted** into templates, models and illustrations, and **re-pathed** in `Manifest.json`.
- **One figure for each unit type** (`Models/earthrise-u*-05m.json` and `earthrise-s2-sapper-05m.json`), cut from each squad. Each figure is the squad's first connected figure. Every squad's figures were checked to be identical first.
- **Template costs recounted** under ADR-005, with decorations costed as the material they sit in. The package left openings and fittings out, so T1, T2, T4 and T7 cost slightly more than the package said. Each of those entries keeps the package's figure as `packageCostM3`.
- **T2's two banner voxels removed.** The conversion dropped the banner's pole and left them floating. The `.vox` was rewritten to match; the `.png` still shows the banner.
- **The palette:**
  - It gained a `behavior` for every index, and seven entries from [RendererLook.md](../Design/RendererLook.md): water (9), leaves A and B (10, 11), fields A and B (13, 14), road (15), and rock rubble (28). The colour for rock rubble is provisional.
  - **Three indices moved** to match the package's look spec: water is now 9, and leaves A and B are 10 and 11. The first import had leaves at 9 and 10 and water at 11. Nothing had been baked, so nothing depended on the old numbers; from here on the indices are frozen (ADR-005).
  - Index 6 is renamed from "dressed stone" to "masonry".
  - **Six indices are marked `both`** where the package said `world` or `models`: masonry, timber and the three rubbles (6, 7, 8, 25, 26), because engines and debris are drawn in them, and iron (18), because templates carry iron fittings. That follows the package's own look spec over its `palette.json`.
- **Known issues** are recorded on the entries they concern, in `knownIssues`:
  - every world asset is an automatic conversion, to be redrawn natively at 1 m;
  - T8's and T9's shed roofs have no posts under them;
  - E1's rampart holds 48 m³ against 32 m³ dug, and is topped with grass;
  - T7's grid has 1 m of empty padding in front, which the baker trims.
- **The legend names inside the asset files are left unchanged.** For example, they still say "dressed stone". The palette's name is the one that counts.
- **The package's integration command** (`commands/integrate-look-v2.md`) was not installed into `.claude/commands/`. It is kept with the package as a record of what was asked.

**The font** comes from the owner's HUD package, kept unchanged in [`Design/Reference/HudPrototype/`](../Design/Reference/HudPrototype/). Only `EarthriseMason-glyphs.json` and its three atlases were copied, unchanged. The glyph rows in the JSON are the source; the package's `earthrise-font.js` and `.otf` stay with the package as its own editing tools. On import, the JSON and the JavaScript agreed glyph for glyph, and each atlas matched the rows pixel for pixel. The package's `integrate-hud.md` command was not installed either.
