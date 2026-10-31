# GameData

The game's content sources, split by who reads them ([ADR-008](../Design/ADR/ADR-008-server-and-client-game-data.md)). Nothing here is loaded directly. The bake, `Tools/BakeContent.py`, which is not written yet, turns the client's content into the `Content` library, compiled in, and the server's into files in `<outputdir>\ServerData`, which the simulation reads at startup. [ADR-005](../Design/ADR/ADR-005-content-formats.md) is the format, [ADR-006](../Design/ADR/ADR-006-voxel-sizes.md) sets the two voxel sizes, and [Design/MvpAssets.md](../Design/MvpAssets.md) is the catalogue.

| Path | Holds | Voxel | Read by |
|---|---|---|---|
| `Server/Templates/` | T1–T9, the buildings | 1 m | The simulation; the client gets their shape, colours and cost from the bake |
| `Server/Palette.json` | Material index → colour, jitter, jitter cell, AO set, grid, behaviour | | The simulation (behaviours); the client gets the colours from the bake |
| `Client/Models/` | One figure for each unit type (U1–U5, S2), the ram (S1) and the trebuchet (S3) | 0.5 m | The client |
| `Client/Fonts/` | Earthrise Mason: `EarthriseMason-glyphs.json`, the source | | The client ([Design/Hud.md](../Design/Hud.md) §9) |
| `Illustrations/` | Squads (U2–U5, S2) and six laborers at 0.5 m; earthworks (E1, E2) and material samples (M1, M2) at 1 m | Both | Nobody: documentation and tests |
| `Manifest.json` | Every asset: id, kind, grid, files, size, voxel count, counted cost, critical part, known issues. Paths are relative to this folder | | The asset check |

## Working with assets

- **The `.json` layer file is the source, and the only copy committed.**
- **To edit in MagicaVoxel:**
  1. Run `python Tools/ExportVox.py`, or give it a file or folder, to write a `.vox` beside each layer file. `.gitignore` keeps those copies out of git.
  2. Edit the `.vox` and save it.
  3. Run `python Tools/ImportVox.py <file.vox>` to write the change back into the `.json`. It keeps each remaining voxel's part, reports added voxels that have none, and adds legend entries for new colours. Set an added voxel's part in the `.json` by hand if it belongs to a critical part, the foundation or stock.
- **A model or template is one MagicaVoxel object.** Merge objects before saving, or the import refuses.
- **A file's name says its voxel size:** `-1m` for the world grid, `-05m` for models. Its `voxelSize` field must agree.
- **A palette index belongs to a grid.** `Server/Palette.json` marks each index `world`, `models` or `both`, and a world asset uses only `world` and `both` indices.
- **Previews are not kept here.** The `.png` renderings live in [`Design/Previews/`](../Design/Previews/); nothing reads them, and they may lag an edit.
- **Costs are counted, not typed.** Layer files carry no costs. `Manifest.json` holds the expected counts, and if a template changes, its expected cost there changes in the same commit.
- **Shared content lives in `Server/`.** A template or the palette is read by both sides, and has its one source on the server's side (ADR-008).

## Where these came from

These files come from the owner's second package, kept unchanged in [`Design/Reference/SiegePrototypeV2/`](../Design/Reference/SiegePrototypeV2/). It replaced the first package, kept in [`Design/Reference/SiegePrototype/`](../Design/Reference/SiegePrototype/). The second package's world assets are its automatic conversions of the first package's 0.5 m drawings; its models are the same 0.5 m drawings. These things were changed on the way in:

- **Sorted** into templates, models and illustrations, and **re-pathed** in `Manifest.json`. They moved from `Assets/` to this folder's server and client trees on 2026-10-31 (ADR-008).
- **One figure for each unit type** (`Client/Models/earthrise-u*-05m.json` and `earthrise-s2-sapper-05m.json`), cut from each squad. Each figure is the squad's first connected figure. Every squad's figures were checked to be identical first.
- **Template costs recounted** under ADR-005, with decorations costed as the material they sit in. The package left openings and fittings out, so T1, T2, T4 and T7 cost slightly more than the package said. Each of those entries keeps the package's figure as `packageCostM3`.
- **T2's two banner voxels removed.** The conversion dropped the banner's pole and left them floating. Its preview still shows the banner.
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
- **The package's `.vox` copies were dropped** when the `.json` became the only committed source (ADR-008). `Tools/ExportVox.py` reproduces their voxels; the package keeps the originals.
- **The legend names inside the asset files are left unchanged.** For example, they still say "dressed stone". The palette's name is the one that counts.
- **The package's integration command** (`commands/integrate-look-v2.md`) was not installed into `.claude/commands/`. It is kept with the package as a record of what was asked.

**The font** comes from the owner's HUD package, kept unchanged in [`Design/Reference/HudPrototype/`](../Design/Reference/HudPrototype/). Only `EarthriseMason-glyphs.json` and its three atlases were copied, unchanged; the atlases are previews and now live in `Design/Previews/`. The glyph rows in the JSON are the source; the package's `earthrise-font.js` and `.otf` stay with the package as its own editing tools. On import, the JSON and the JavaScript agreed glyph for glyph, and each atlas matched the rows pixel for pixel. The package's `integrate-hud.md` command was not installed either.
