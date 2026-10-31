# Earthrise.World — The renderer's look

**Status:** Decided by the owner, 2026-10-31. It comes from a visual prototype in three.js r160, tuned to read well from the strategy camera. The owner then resolved the points where it collided with the design, and those resolutions are written in here. Nothing implements it yet. [GameDesign.md](GameDesign.md) and [GameDescription.md](GameDescription.md) govern where this document differs from them, and [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md) governs how voxels reach the screen.

This is the look specification for the voxel renderer, the first step in GameDescription.md's *How the design is proven*. It defines voxel size, colour, shading, lighting and the camera. It describes no scene, layout or content.

## 1. Voxel size

- **Every solid voxel is 0.5 m:** structures, rubble, engines, timber, trees, units and terrain. The render voxel and the simulation voxel are the same.
- **Terrain heights come in whole metres.** The generator builds slopes from 1 m terraces made of 2 × 2 × 2 groups of voxels. Earth's slope is measured over 1 m, and earth stands at up to 45°, so a 1 m step with at least 1 m of tread is stable (GameDesign.md). Terrain that is dug or damaged follows the edits at the full 0.5 m resolution.
- **Reference sizes:**

| Thing | Size | Voxels |
|---|---|---|
| Standard wall | 1.5 m thick | 3 |
| Thick wall | 2.5 m thick | 5 |
| Unit | 0.5 × 0.5 × 2 m | 1 × 1 × 4 |
| Merlon | 0.5 m wide, 1 m tall | 1 × 2 |

- **Units are tokens.** A unit is four voxels stacked one above another. From the bottom up they read as legs, body and helmet, banded in colour: cloth legs, a faction-coloured body, and a head of skin or iron. Units move as a whole, and may bob, but no limbs animate. A gap one voxel wide and four voxels high is a route.
- **Why 0.5 m.** At 1 m, breaches, craters and units become blobs. At 0.25 m, terrain gets too busy for the strategy view.

## 2. Material palette

Colours are authored in sRGB and converted to linear before lighting. The output is sRGB, with no tone mapping.

| Material | sRGB | Jitter | Jitter cell | Behaves as |
|---|---|---|---|---|
| Grass | #7d8b4f | 0.05 | 1 m | Earth. Only the generator makes it, on top of undisturbed earth, so digging leaves bare earth. |
| Earth | #8a6a48 | 0.04 | 1 m | Earth |
| Common rock | #8d8a84 | 0.04 | 1 m | Common rock |
| Building stone (raw outcrop) | #ddd2b9 | 0.04 | 1 m | Building stone |
| Bedrock | #4a4744 | 0.04 | 1 m | Bedrock |
| Masonry (built walls) | #c6bca7 | 0.07 | voxel | Building stone, laid by a template |
| Rubble of stone | #a69c8a | 0.07 | voxel | Rubble that yields stone |
| Rubble of rock, earth or timber | Its source colour, at lower lightness | 0.07 | voxel | Waste rubble, which yields nothing |
| Timber | #7a5534 | 0.05 | voxel | Timber |
| Roof | #8c4a3a | 0.05 | voxel | Timber |
| Leaves A / B | #4f6a3a / #62803f | 0.10 | voxel | Leaves: a span as wide as a canopy, no timber yield, and they burn |
| Water | #4f7f8c | 0.03 | voxel | Water. Shallow water is lighter, so fords show. |
| Field A / B | #c9a95a / #a6a056 | 0.04 | 1 m | Earth |
| Road / track | #a88e6a | 0.04 | 1 m | Earth |
| Openings (slits, doors, windows) | #2e2a27 | 0.05 | voxel | The structure's own material. Cosmetic: nothing passes through. |

**The Masons' dressed stone** is a material of its own (GameDescription.md, *Craters*). It gets a colour of its own when the Masons are built, after the MVP.

Unit and engine colours belong to their models, not to world materials:

| Model colour | sRGB | Jitter |
|---|---|---|
| Iron fittings, helmets | #9aa0a6 | 0.05 |
| Engine counterweight | #5b5650 | 0.05 |
| Skin | #d9a57f | 0.05 |
| Cloth (unit legs) | #3d3530 | 0.05 |
| Faction colours (example) | #b23a2e red, #2f5d93 blue | 0.05 |
| Laborer | #c79a6b | 0.05 |

Every model colour jitters per voxel.

**Rules:**
- **Rubble of masonry** shows about 12% of its voxels in the masonry colour, mixed into the stone-rubble colour. This only picks colours: every voxel of it is still rubble of stone.
- **Built and raw stone differ at a glance.** Masonry is darker and greyer than raw building stone, so a player can always tell what was built from what was quarried.
- **Rubble shows what it is worth.** Rubble of stone, which yields stone, must look different from waste rubble.
- **Faction colour appears only on units and banners.** Structures stay in their material colours.

## 3. Colour jitter

Jitter breaks up flat fields of colour so the grid reads as material, not plastic.

- **It changes HSL lightness only,** by ±jitter/2. Hue and saturation never change.
- **It is chosen** by a deterministic hash of the jitter cell's coordinate and the material ID, so the same voxel always gets the same shade, across frames and across edits.
- **Terrain materials use a 1 m jitter cell,** keyed by floor(voxel / 2). That is what keeps the ground calm. Per-voxel jitter on terrain looked restless at 0.5 m and is not allowed.
- **Everything else jitters per voxel.**

The hash, in 32-bit integer arithmetic, with its result in [0, 1]:

```
h = x*374761393 + y*668265263 + material*982451653   // wrapping 32-bit multiply and add
h = (h ^ (h >> 13)) * 1274126177                      // logical shift on uint32, wrapping multiply
r = (h ^ (h >> 16)) / 4294967295                      // as uint32
```

Here (x, y) are (cellX·7 + cellY·131, cellZ·3), with y up, as in the prototype. That fold repeats one shade every 131 cells along a diagonal. The repeat is invisible in practice, and the fold is kept so that colours match the prototype exactly.

**Implementation:**
- The hash runs in C++ on `std::uint32_t` when a chunk's render records are built.
- Coordinates use floor division, never truncation.
- Material IDs are frozen, because colours depend on them.

## 4. Corner shading (voxel ambient occlusion)

Each face gets an occlusion value at each corner. It comes from the three neighbours in front of the face at that corner: side 1, side 2 and the diagonal.

```
ao = (side1 && side2) ? 0 : 3 - (side1 + side2 + corner)
```

| ao level | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| Structures, units, trees, rubble | 0.52 | 0.68 | 0.84 | 1.00 |
| Terrain materials (those with a 1 m jitter cell) | 0.74 | 0.84 | 0.93 | 1.00 |

- **The four corner values blend bilinearly** across the face, at the hit point's coordinates on that face.
- **Terrain must use the reduced values.** This is not a tuning option: full AO on 1 m terraces makes the land read as noise.
- **At the edges of the world,** cells outside it count as empty, and cells below y = 0 count as solid.
- **Implementation:** the 24 corner values (6 faces × 4 corners, 2 bits each) are computed when a chunk's records are built and stored in each voxel's record. The pixel shader only blends them.
  - An edit rebuilds the records of the voxels around it, across chunk borders, so the voxel's own AO and its neighbours' change on the next frame.
  - A model, such as a unit or an engine, computes its AO within itself.

## 5. Lighting

Lighting is diffuse only: Lambert, with no metalness and no specular highlights on voxels.

| Preset | Sun colour | Sun intensity | Sun direction (towards the light) | Sky / ground (hemisphere) | Hemisphere intensity | Clear colour |
|---|---|---|---|---|---|---|
| Golden (default) | #ffd8a6 | 2.7 | (−90, 75, 55), normalized | #d4dbe6 / #5a4a38 | 1.0 | #d6ccba |
| Morning | #fff3e2 | 2.4 | (60, 120, 80) | #dfe8f0 / #5d5446 | 1.15 | #c9d3d6 |
| Dusk | #ff9858 | 2.3 | (−130, 42, 25) | #5b6690 / #2a2018 | 0.7 | #2c2b34 |

- **The intensities are three.js r160 physical units.** Match them by eye, not by number. Three.js's Lambert term divides by π, so the engine is expected to scale these intensities by 1/π. That has to be checked against the prototype before it is relied on.
- **Corner AO multiplies the albedo before lighting,** as in the prototype, so AO stays visible in shadow.
- **There is one directional shadow from the sun, soft-edged** (PCF or similar), covering everything visible. It goes through ADR-001's voxel path (decision 5), not through rays traced towards the sun, which this renderer cannot trace without a lighting ADR.
  - A static shadow map covers terrain and structures. It is updated only when there is an edit or the sun changes.
  - A small dynamic shadow map is redrawn every frame for units, engines and debris.
  - Both are sampled together.

## 6. Camera

- **Orthographic and isometric,** 34° above the horizon (0.6 rad).
- **It rotates in 90° steps,** eased over about 0.3 s. The default azimuth is −45°, from the south-west.
- **It zooms** from about 12 m of view height (single stones) to about 260 m (a whole holding).
- **Calibration:** at the default framing, about 84 m of view height on a 1080p frame, one voxel is about 6.4 px. At the widest zoom it is still about 2 px, so no voxel falls below a pixel and ADR-001's stochastic pruning is not used.
- **Level of detail.** A step to 1 m voxels for structures below about 3 px per voxel is deferred until the spike shows a need for it.
- **What this angle hides.** At 34°, each wall of a 3 m-deep ditch hides about 4.4 m of ground behind it. The floors of deep ditches and the near terraces of pits can therefore be hidden from every rotation. The owner accepted this. Units in them show as silhouettes, which stay visible behind walls, rubble and terrain.

## 7. Effects that are not voxels

- **Debris and dust** are drawn by the client and are never part of the voxel grid (ADR-004, decision 7). Dust is at least 0.8 m across.
- **Water is flat and opaque.** Transparency or animation is optional, and must never hide fords.
- **The readability overlays,** meaning the integrity view, the collapse preview, silhouettes and the passability view, are drawn separately and are never baked into albedo.

## 8. Acceptance checks

1. A 128 × 128 m area of a holding at the default framing has calm ground, with no visible per-voxel speckle on grass or earth.
2. Masonry walls and raw building stone can be told apart at the default zoom.
3. A breach notch and its rubble ramp read as ragged at the default zoom, not as a clean rectangle. The craters' shapes come from the simulation, which stamps them irregularly but deterministically.
4. Units read as figures at the default zoom, with legs, body and helmet as bands of colour, and their faction colour can be identified.
5. Editing a voxel changes its colour and AO, and its neighbours' AO, on the next frame, with no seams.
6. The same seed and edits produce the same colours on every machine.
7. Fords can be told apart from deep water.
8. Rubble of stone can be told apart from waste rubble.
