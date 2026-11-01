# Earthrise.World — The renderer's look

**Status:** Decided by the owner, 2026-10-31. A first version that day put everything at 0.5 m. The second, adopted later the same day, puts the world at 1 m and models at 0.5 m ([ADR-006](ADR/ADR-006-voxel-sizes.md), Proposed until the M1 spike and the M3 breach lab have measured it).

It comes from a visual prototype in three.js r160, tuned to read well from the strategy camera. Both versions of the prototype and their specifications are kept unchanged in [Reference/SiegePrototype/](Reference/SiegePrototype/) and [Reference/SiegePrototypeV2/](Reference/SiegePrototypeV2/). The owner then resolved the points where the prototype collided with the design, and those resolutions are written in here.

[GameDesign.md](GameDesign.md) and [GameDescription.md](GameDescription.md) govern where this document differs from them. [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md) governs how voxels reach the screen.

This is the look specification for the voxel renderer, the first step in GameDescription.md's *How the design is proven*. It defines voxel size, colour, shading, lighting and the camera. It describes no scene, layout or content.

## 1. Two grids

| Grid | Voxel | Holds | Simulated |
|---|---|---|---|
| **World** | **1 m** | Terrain, every template, earthworks, quarries, craters, rubble and trees | **Yes.** Support, collapse, slump, navigation, edits and cost all run on it. |
| **Models** | **0.5 m** | Units, rams, trebuchets, mantlets, carts, banners and debris | **No.** Units and engines are entities, and debris is spectacle. |

- **The world voxel is the simulation voxel.** The 512 m MVP map, about 64 m deep, is about 17 million voxels, against about 134 million at 0.5 m.
- **Models are drawn at their entity's position** through ADR-001's pipeline. They are rotated in 90° steps, so their voxels stay axis-aligned, and they are never written into the world grid. Debris is the exception: it turns as oriented voxels (below).
- **Debris.** A collapse's result is 1 m rubble in the world. The client scatters 0.5 m model debris over it: loose blocks of 0.5–1 m, and 0.5 m pebbles. The debris is deterministic from the collapse's id, so every client shows the same. It falls with the collapse for about 1.5 s, turning, as oriented voxels, and then lies on the rubble at its final angles for good (GameDesign.md, *M1's open values*, item 7). It never blocks a path or a shot.
- **Terrain heights come in whole metres,** which is one voxel. Earth stands at up to 45°, so no earth column stands more than one voxel above any of its four side neighbours (GameDesign.md).

## 2. Sizes

- **Walls.** A standard wall is 2 m thick, two voxels, and a section of 8 × 2 × 6 m costs 96 m³ of stone. A thick wall is 3 m, and costs half as much again.
- **Battlements.** Merlons are 1 m wide and 1 m tall, with 1 m gaps. Arrow slits and windows are 1 × 2 m openings.
- **Buildings.** A tower is 7 × 7 m and about 12 m high, hollow, with 2 m walls. The keep is 12 × 12 m, 16 m to its roof and 19 m to the top of its parapets, with a forebuilding. The hall is about 11 × 7 m, in timber.
- **Earthworks.** The standard ditch is 4–5 m wide and 2 m deep, with stepped 1 m sides. Its rampart is 2 m high.
- **Units are tokens.** A unit is 1 × 1 × 4 model voxels, 0.5 × 0.5 × 2 m.
  - A spear, a bow, a shield or a tool sits beside the body as 0.5 m voxels, which gives each unit type its cue.
  - Units move as a whole and may bob; no limbs animate.
  - A gap one world voxel wide and two high is a route for foot; cavalry and machines need more (GameDesign.md, *M2's values*, item 10).
- **Engines.** The trebuchet is about 12 m long and 13 m tall, built of 0.5 m beams; its arm switches between baked poses (cocked, released, packed). The ram is about 3 × 7 m.
- **Why two grids.** A 0.5 m world looked restless and costs eight times as much. A 1 m world is calm, but units drawn in it lose their cues. Units are entities anyway, so they keep the finer grid.

## 3. Material palette

Colours are authored in sRGB and converted to linear before lighting. The output is sRGB, with no tone mapping.

[`GameData/Server/Palette.json`](../GameData/Server/Palette.json) carries these colours with their frozen indices and behaviours ([ADR-005](ADR/ADR-005-content-formats.md)). The indices follow the second package's numbering.

| idx | Material | sRGB | Jitter | Grid | Behaves as |
|---|---|---|---|---|---|
| 1 | Grass | #7d8b4f | 0.05 | world | Earth. Only the generator places it, on undisturbed earth, so dug ground stays bare. |
| 2 | Earth | #8a6a48 | 0.04 | world | Earth |
| 3 | Common rock | #8d8a84 | 0.04 | world | Common rock |
| 4 | Building stone (raw outcrop) | #ddd2b9 | 0.04 | world | Building stone |
| 5 | Bedrock | #4a4744 | 0.04 | world | Bedrock |
| 6 | Masonry (built walls; "dressed stone" in the package) | #c6bca7 | 0.07 | both | Building stone, laid by a template |
| 7 | Timber | #7a5534 | 0.05 | both | Timber |
| 8 | Rubble of stone | #a69c8a | 0.07 | both | Rubble that yields stone |
| 9 | Water | #4f7f8c | 0.03 | world | Water. Water one voxel deep over a solid floor draws lighter, so fords show; deeper water draws at this colour (Proposed: 15% lighter, tuned in the spike). |
| 10 / 11 | Leaves A / B | #4f6a3a / #62803f | 0.10 | world | Leaves: a span as wide as a canopy, no timber yield, and they burn |
| 12 | Roof | #8c4a3a | 0.05 | world | Timber |
| 13 / 14 | Field A / B | #c9a95a / #a6a056 | 0.04 | world | Earth |
| 15 | Road | #a88e6a | 0.04 | world | Earth |
| 16 | Faction (red example; blue #2f5d93) | #b23a2e | 0.05 | models | Models only, banners included; never on structures |
| 18 | Iron | #9aa0a6 | 0.05 | both | A decoration in a template (gate fittings) |
| 19 | Laborer cloth | #c79a6b | 0.05 | models | Models only |
| 20 | Opening (slits, doors, windows) | #2e2a27 | 0.05 | both | A decoration: the material around it. Nothing passes through it. S1's wheels are drawn in it, so models use it too (owner, 2026-10-31). |
| 21 | Counterweight | #5b5650 | 0.05 | models | Models only |
| 22 | Skin | #d9a57f | 0.05 | models | Models only |
| 23 | Cloth (legs) | #3d3530 | 0.05 | models | Models only |
| 24 | Horse | #5e4330 | 0.05 | models | Models only |
| 25 | Rubble of timber | #5f4530 | 0.07 | both | Waste rubble, which yields nothing |
| 26 | Rubble of earth | #7d6044 | 0.05 | both | Waste rubble |
| 27 | Leather (hides, hoods) | #6e5a3e | 0.05 | models | Models only |
| 28 | Rubble of rock (provisional) | #7f7c77 | 0.07 | world | Waste rubble |

**The Masons' dressed stone** gets an index of its own when the Masons are built, after the MVP.

**Rules:**
- **Rubble of masonry** shows about 12% of its voxels in the masonry colour. This only picks colours: every voxel of it is still rubble of stone.
- **Built and raw stone differ at a glance.** Masonry is darker and greyer than raw building stone.
- **Rubble shows what it is worth.** Rubble of stone, which yields stone, looks different from waste rubble.
- **Faction colour appears only on models,** banners included, never on structures.

## 4. Colour jitter

- **It changes HSL lightness only,** by ±jitter/2. Hue and saturation never change.
- **The cell** is one 1 m world voxel for every world material, and one 0.5 m voxel in a model.
- **It is chosen** by a deterministic hash of the cell's coordinate and the material ID, so a voxel always gets the same shade, across frames and across edits.

The hash, in 32-bit integer arithmetic, with its result in [0, 1]:

```
h = x*374761393 + y*668265263 + material*982451653   // wrapping 32-bit multiply and add
h = (h ^ (h >> 13)) * 1274126177                      // logical shift on uint32, wrapping multiply
r = (h ^ (h >> 16)) / 4294967295                      // as uint32
```

Here (x, y) are (cellX·7 + cellY·131, cellZ·3), with y up, as in the prototype. That fold repeats one shade every 131 cells along a diagonal. The repeat is invisible in practice, and the fold is kept so that colours match the prototype exactly.

**Implementation:** the hash runs in C++ on `std::uint32_t` when a chunk's or a model's render records are built. Coordinates use floor division, never truncation. Material IDs are frozen, because colours depend on them. The HSL is that of linear RGB: three.js r160's `offsetHSL`, which the prototype calls, works in its working colour space, which is linear sRGB (checked in r160's `Color.js` and `ColorManagement.js`, 2026-11-01).

## 5. Corner shading (voxel ambient occlusion)

Each face gets an occlusion value at each corner. It comes from the three neighbours in front of the face at that corner: side 1, side 2 and the diagonal.

```
ao = (side1 && side2) ? 0 : 3 - (side1 + side2 + corner)
```

| ao level | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| Structures, rubble, trees, models | 0.52 | 0.68 | 0.84 | 1.00 |
| Terrain materials (indices 1–5 and 13–15) | 0.74 | 0.84 | 0.93 | 1.00 |

- **The four corner values blend bilinearly** across the face, at the hit point.
- **Terrain must use the reduced values.** This is not a tuning option: full AO on terraces makes the land read as noise.
- **Models see the world.** In a model, a neighbour counts as solid if the model voxel is set, or if the world voxel containing it is set. That gives contact shading where feet meet the ground and blocks meet rubble.
- **The world ignores models.** World shading never samples them.
- **At the edges of the world,** cells outside it count as empty, and cells below y = 0 count as solid.
- **Implementation:**
  - **World voxels:** the 24 corner values (6 faces × 4 corners, 2 bits each) are computed when a chunk's records are built, and stored in each voxel's record. An edit rebuilds the records around it, across chunk borders.
  - **Models:** a model's own corner values are baked once.
  - **Contact with the world:** this depends on where the model stands. It is baked per instance on the CPU (GameDesign.md, *M1's open values*, item 6; ADR-006).

## 6. Lighting

Lighting is diffuse only: Lambert, with no metalness and no specular highlights.

| Preset | Sun colour | Sun intensity | Sun direction (towards the light) | Sky / ground (hemisphere) | Hemisphere intensity | Clear colour |
|---|---|---|---|---|---|---|
| Golden (default) | #ffd8a6 | 2.7 | (−90, 75, 55), normalized | #d4dbe6 / #5a4a38 | 1.0 | #d6ccba |
| Morning | #fff3e2 | 2.4 | (60, 120, 80) | #dfe8f0 / #5d5446 | 1.15 | #c9d3d6 |
| Dusk | #ff9858 | 2.3 | (−130, 42, 25) | #5b6690 / #2a2018 | 0.7 | #2c2b34 |

- **The intensities are three.js r160 physical units.** Match them by eye, not by number. Three.js's Lambert term divides by π, so the engine is expected to scale these intensities by 1/π. That has to be checked against the prototype before it is relied on. (Checked 2026-11-01 against r160's source: `BRDF_Lambert` returns the albedo over π for the sun and the hemisphere alike, and with `useLegacyLights` false, r160's default, `WebGLLights` scales no light by π. The engine divides by π.)
- **Corner AO multiplies the albedo before lighting,** as in the prototype, so AO stays visible in shadow.
- **There is one directional shadow from the sun, soft-edged** (PCF or similar). It goes through ADR-001's voxel path (decision 5), not through rays traced towards the sun.
  - The world's shadow map is updated only when there is an edit or the sun changes.
  - Models cast and receive shadows every frame, and they receive the world's shadow.

## 7. Camera and level of detail

- **Orthographic and isometric,** 34° above the horizon (0.6 rad).
- **It rotates in 90° steps,** eased over about 0.3 s. The default azimuth is −45°, from the south-west.
- **It zooms** from about 12 m of view height (single stones) to about 260 m (a whole holding).
- **Pixel sizes at 1080p.**
  - At the default framing of about 84 m of view height, a world voxel is about 13 px and a model voxel about 6–7 px.
  - At the widest zoom, a world voxel is about 4 px, so the world needs no level of detail and ADR-001's stochastic pruning is not used.
- **Model level of detail.** When a model voxel falls below about 3 px (a view height above about 180 m), each unit is drawn as a 1 × 1 × 2 world-scale impostor: faction colour, plus a helmet or a hood. Engines stay at full detail, since there are few of them.
- **What this angle hides.** At 34°, the near wall of a ditch 2 m deep hides about 3 m of ground behind it. The floors of ditches and the near terraces of pits can therefore be hidden from every rotation. The owner accepted this. Units in them show as silhouettes, which stay visible behind walls, rubble and terrain.
- **A mobile layout (Open).** The second package sketches a landscape phone layout: the view height scales as 128 / aspect, clamped between 58 and 120 m; tap targets are at least 44 px; and the shadow map is 2048². It binds nothing yet: the touch scheme follows a positive MVP verdict, and its layout is designed then ([ADR-007](ADR/ADR-007-platforms-and-input.md)).

## 8. Effects that are not voxels

- **Dust is drawn by the client and never enters the voxel grid.** It is at least 0.8 m across. Debris is 0.5 m models (§1).
- **Water is flat and opaque.** Transparency or animation is optional, and must never hide fords.
- **The readability overlays,** meaning the integrity view, the collapse preview, silhouettes and the passability view, are drawn separately and are never baked into albedo.

## 9. Acceptance checks

1. Ground at the default framing reads calm, with no speckle. One jitter cell is one 1 m world voxel.
2. Masonry and raw building stone can be told apart at the default zoom.
3. A breach reads as ragged: a 1 m notch, with model debris over 1 m rubble. The craters' shapes come from the simulation, which stamps a fixed footprint for each weapon and material (GameDesign.md, *M2's values*, item 12); the debris over them makes the edge ragged.
4. Every MVP unit type can be identified at the default zoom by its cue. Sappers and spearmen must differ, and faction colour must be identifiable.
5. Units visibly stand on the ground. Contact shading is present, and no model floats above the 1 m surface or sinks into it.
6. A world edit changes the voxel's colour and AO, and its neighbours', on the next frame, with no seams. A collapse's model debris looks the same on every client, because it comes from the collapse's id.
7. The same seed and edits produce the same colours on every machine.
8. Fords can be told apart from deep water.
9. Rubble of stone can be told apart from waste rubble.
10. The voxel pass stays within 8 ms at 1080p at the default zoom on an RTX 3060-class GPU. This is the M1 pass mark, measured as GameDesign.md states it: the 95th percentile on a desktop RTX 3060, shadow passes included, over a scripted camera path through all four rotations, with units moving and edits running.
11. Shadows land where the sun puts them: no acne on lit faces, no gap between a wall, tower, tree or unit and its shadow at the default zoom, and an edit's shadow changes within a frame of the edit.
