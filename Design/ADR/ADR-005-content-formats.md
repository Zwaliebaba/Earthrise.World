# ADR-005 — Voxel content is JSON layer files, baked into the executable

**Status:** Accepted, 2026-10-31, by the owner. It closes the question of the object formats. It was amended the same day for [ADR-006](ADR-006-voxel-sizes.md)'s two voxel sizes. [ADR-008](ADR-008-server-and-client-game-data.md) supersedes decisions 1 and 3, and decision 9 for the server: sources now live in `GameData/`, the `.vox` is a working copy that is not committed, and the server reads its data from files. The rest stands. Nothing implements it yet. Like ADR-002 to ADR-004, it is written ahead of the code it governs, and the bake tool lands in M4 ([MvpPlan.md](../MvpPlan.md)).

## Context

The owner delivered the MVP's voxel assets as a package, kept unchanged in [`Design/Reference/SiegePrototype/`](../Reference/SiegePrototype/). It holds nine building templates, models of units and siege engines, and illustrations. Each asset comes in three files: a JSON layer file, a MagicaVoxel `.vox` and a `.png` preview.

These constraints bind the format:
- **Voxels come in two sizes** since [ADR-006](ADR-006-voxel-sizes.md): 1 m for the world and 0.5 m for models. Units are tokens with one cue each to tell them apart ([RendererLook.md](../RendererLook.md)). Each file states its own `voxelSize`.
- **A building costs exactly the volume it is made of.** Buildings also have critical parts with thresholds, and foundations (GameDescription.md).
- **The simulation and the client need different views of one template.** The simulation needs how each voxel behaves and what part it belongs to. The client needs each voxel's colour.
- **The client never includes simulation code, and one build replays its own commands** ([ADR-002](ADR-002-authoritative-simulation.md)).
- **Nothing third-party goes into the executable,** though a tool may use anything (R14). The MVP uses only content made for the project.
- **Project directories are flat,** and shaders are compiled into the executable (AGENTS.md §2).

## Decision

1. **Content sources live in `Assets/`** at the repository root, outside every project directory. *(Superseded by ADR-008, decision 1: they live in `GameData/Server/`, `GameData/Client/` and `GameData/Illustrations/`.)*
   - `Templates/`: buildings, used by both the simulation and the client;
   - `Models/`: units and engines, used by the client only;
   - `Illustrations/`: squads, earthworks and material samples, for documentation and tests, never baked;
   - `Fonts/`: the bitmap font's glyphs as text rows, with atlas previews ([Hud.md](../Hud.md) §9). They are baked like the rest, and the text ADR in M3 settles their baked form;
   - `Palette.json` and `Manifest.json`, beside them.
2. **The source of every voxel asset is its `.json` layer file.**
   - **Fields:** `voxelSize` (1 for world assets, 0.5 for models), `size`, and `legend`, which maps a character to a palette index. `layers[y][z]` holds strings over x, with `.` for empty. A template adds `parts[y][z]`: `1` for the critical part, `2` for foundation, `3` for stock (goods on site, not costed), and `.` for none.
   - **Axes:** y is up, and +z is the front.
   - **Origin:** a template's origin is the minimum corner of its occupied voxels; the baker trims empty padding.
   - The file is text, so it can be diffed and reviewed.
3. **The `.vox` file is a MagicaVoxel copy**, kept beside each JSON file for viewing and editing. *(Superseded by ADR-008, decision 7: `Tools/ExportVox.py` writes it on demand, it is not committed, and `Tools/ImportVox.py` writes an edit back.)*
   - **Encoding:** version 150; the colour index equals the palette index; a voxel (x, y, z) is stored at (x, depth − 1 − z, y).
   - **Editing:** a tool rewrites a JSON file's layers from an edited `.vox` and keeps its parts.
   - **Agreement:** the two must agree voxel for voxel, and the asset check fails when they do not.
   - **The `.png`** is a preview that nothing reads.
4. **`Palette.json` is the only palette.** Its indices are frozen, because the colour jitter is keyed on them (RendererLook.md §3). Each entry gives a colour, a jitter, a jitter cell, an AO set and a **behaviour**:

   | Behaviour | Meaning |
   |---|---|
   | earth | Earth |
   | common rock | Common rock |
   | building stone | Building stone |
   | bedrock | Bedrock |
   | timber | Timber |
   | leaves | Leaves |
   | water | Water |
   | stone rubble | Rubble that yields stone |
   | waste rubble | Rubble that yields nothing |
   | `inherit` | A decoration, which behaves as its host (decision 6) |
   | `modelOnly` | Appears only in models, never in the world grid |

5. **A world voxel stores its palette index.** That lets the client draw masonry apart from an outcrop, and rubble by its source. Simulation rules read only the behaviour, never the index. Only the generator places grass (index 1).
6. **Decorations behave as their host.** Openings and iron fittings are decorations. Banners and cloth are models ([ADR-006](ADR-006-voxel-sizes.md)), so their colours never appear in a template.
   - **Its host:** a decoration takes the most common behaviour among its six face neighbours that are not decorations and are of its own kind. Structure looks at structure, and stock at stock.
   - **Ties** go to the lower palette index.
   - **Decorations touching only decorations** resolve in repeated passes, so a fitting set into an opening still finds its host.
   - **Errors:** a decoration that never resolves is an error, and so is a `modelOnly` index in a template.
7. **A template's cost is counted, never copied.**
   - **Stone** is the volume of its building-stone voxels, and **timber** the volume of its timber voxels.
   - **Decorations** count as their host.
   - **Stock and foundation** are left out. The foundation is costed at the site, from the depth to bedrock.
   - `Manifest.json` holds the expected counts, and the asset check compares against them.
8. **Models set no costs and no rules.**
   - The simulation knows a unit's footprint and statistics from its stat table, and the model is only how the client draws it.
   - **A unit model is one figure;** a squad is an illustration.
   - Models are drawn as axis-aligned volumes at their entity's position, rotated in 90° steps ([ADR-006](ADR-006-voxel-sizes.md)). Engine poses are baked; debris is oriented voxels.
9. **Baking.** *(For the client this stands. For the server, ADR-008 replaces it: the bake writes the server's data to files in `<outputdir>\ServerData`, which the simulation reads at startup, and `Simulation` no longer depends on `Content`.)*
   - `Tools/BakeContent.py` reads the content sources and writes generated C++ headers into the build's intermediate directory. They are build output and are never committed.
   - **What it writes:** a simulation view of each template (behaviour and part for each voxel, plus the counted cost), a render view of each template and model (palette index for each voxel), and the palette.
   - **Where it goes:** the headers compile into a `Content` static library. It depends only on `Core`, and both `Simulation` and `Client` depend on it. `Content` holds data and accessors, and no rules.
   - **Only path into the executable:** like shaders, assets reach the executable only this way. Nothing is loaded from disk at runtime, and changing an asset means a rebuild, so a replay's build always carries its own templates (ADR-002, decision 6).
   - **Python** runs at build time only. It never ships and never links (R14).
10. **Saves and replays store a structure** as its template id, its origin, its rotation (0, 90, 180 or 270°) and a mask of which of its voxels have been built, plus the name the scenario gives it, if any, such as a wall section's W5 ([Hud.md](../Hud.md) §10). The mask is what lets repair restore only voxels that were once built. Entity state is a versioned binary, written with the same encoder as the commands (ADR-002, decision 3). This closes [ADR-003](ADR-003-world-as-seed-plus-edits.md)'s open item on the format of entity state.

## Consequences for the code

- **`Tools/CheckAssets.py`** runs in CI, guarded on the script existing (AGENTS.md §6). It checks that:
  - every index is in the palette;
  - every decoration resolves, and no template holds a `modelOnly` index;
  - the counted costs match `Manifest.json`, and T3 comes to exactly 96 m³ (ADR-006);
  - every named critical part has voxels;
  - every structure voxel of a template connects to its bottom layer through structure voxels, so nothing floats and nothing stands on stock (added for [ADR-006](ADR-006-voxel-sizes.md)'s converted templates, two of which failed it);
  - each figure is identical to the figures in its squad.
- **Templates are authored facing +z.** Rotations in 90° steps are made by the baker or at placement.
- **The format, project-file and clang-tidy checkers skip `GameData/`.**
- **`Content` joins the solution layout** in MvpPlan.md.

## What this forecloses

- Asset sources that are binary-only or editor-only.
- Loading content from disk at runtime, and with it mods made of files, until a later ADR. ADR-008 allows it for the server's data, and for client audio in `<outputdir>\GameData`.
- Costs taken from metadata, or from the volume of a model.
- Render-only indices in simulation rules.
- Meshes for units and engines, unless a later ADR allows them (ADR-001 allows meshes in general).

## Left open

- Rigid parts and animation for engine models, such as the trebuchet's arm. MVP engines are static models.
- Models with voxels finer than 0.5 m. The format carries `voxelSize`, so a later integer ratio fits it.
- Compressing the baked tables.
- Palette indices for materials outside the MVP: iron ore, Earthstone and the Masons' dressed stone.
