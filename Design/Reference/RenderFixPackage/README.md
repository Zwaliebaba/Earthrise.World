# Earthrise.World: render fix package (v5 parity)

**For Claude Code.** Read this file first, then follow `PROMPT.md` item by item.

## Why this package exists
The game renders the siege castle correctly in layout, but it differs from the design reference (`reference/Earthrise Siege Scene v5.dc.html`) in materials, openings, shadows, terrain noise and scenario set-up. `screenshots/current-render.png` is the render this package was reviewed against. Camera and zoom are **not** in scope; leave them as they are.

One gap was a design-package bug, not a renderer bug. The 0.5 m → 1 m template conversion used a majority vote, which deleted the slits and windows. The corrected templates are included here.

## Contents
| Path | What it is | What to do with it |
|---|---|---|
| `PROMPT.md` | The task: 8 ordered fixes with acceptance criteria | Execute in order, one commit per item |
| `assets/world/templates/*-1m.{json,vox}` | **Corrected** 1 m templates: T1 hall, T2 keep, T4 tower, T5 gatehouse. Openings are now present | Replace the game's copies (see below) |
| `manifest.json` | Full asset manifest with updated stone costs | Replace; costs feed build cost and rubble yield |
| `palette.json` | Material palette (unchanged) | Reference for material ids and colours |
| `specs/Renderer Look Spec v2.md` | Renderer spec: grids, palette, colour variation, corner shading, lighting, acceptance checks | The governing rules; `PROMPT.md` cites its sections |
| `reference/Earthrise Siege Scene v5.dc.html` | The look target. Open it in a browser (it needs `support.js` beside it) | Visual comparison after each item |
| `reference/MVP Asset Sheet.dc.html` | All MVP assets with the fixed conversion (`down()`, the openings rule) | Reference for the conversion logic and template shapes |
| `screenshots/current-render.png` | The game as reviewed | The "before" image |

Templates T3 and T6–T9 (wall, palisade, palisade gate, quarry yard, lumber camp) and all models are **unchanged**. Keep the ones already in the game.

## Asset changes
| Template | Opening voxels before → after | Stone (m³) before → after |
|---|---|---|
| T1 Hall | 3 → 5 | 0 → 0 (timber 185, unchanged) |
| T2 Keep | 4 → 54 | 1718 → 1668 |
| T4 Tower | 2 → 10 | 477 → 469 |
| T5 Gatehouse | 0 → 10 | 585 → 575 |

**Conversion rule (only if the game converts templates itself):** take the majority material of each 2×2×2 block of 0.5 m voxels, but if **any** voxel in the block is material 20 (opening), the 1 m cell becomes material 20.

**File formats** (unchanged from the v2 package):
- **JSON:** `layers[y][z][x]` are characters mapped through `legend` to material ids; `parts` is the same grid with part ids (1 = critical part, 2 = foundation, 3 = stock); y is up and +z is the front.
- **VOX:** MagicaVoxel 150. The colour index equals the palette material index. Coordinates are vox (x, y, z) = (x, depth − 1 − z, y).

## How to run it
1. **Import the assets.** Replace T1, T2, T4 and T5 and the manifest. Rebuild the test map, load it, and confirm that slits are visible on the keep and towers. This also resolves part of `PROMPT.md` item 2.
2. **Do `PROMPT.md` items 1–8 in order.** After each one:
   - take a screenshot at the default framing into `screenshots/parity/NN-<item>.png`;
   - compare it with the v5 reference;
   - commit.
3. **Check performance** once item 7 (shadows) is done: the voxel pass must stay at or under 8 ms at 1080p (spec §8, check 7). Record the result in ADR-006.

## Constraints
- **Keep both grids:** the world is 1 m and simulated; models are 0.5 m and display-only. Never write models into the world grid.
- **Stay deterministic:** colour variation is hashed per cell, and debris is seeded from the collapse id.
- **Faction colour** appears only on models and banners.
- **Leave the camera, zoom and HUD unchanged.**

## Done when
- Spec §8 checks 1, 2 and 3 pass by eye against v5, and check 7 still holds.
- Each `PROMPT.md` item has a before and after screenshot and its own commit.
