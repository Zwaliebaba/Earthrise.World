# Renderer input v2: voxel size, colouring and shading — Earthrise.World

**Supersedes v1.** The change: **the world is a 1 m voxel grid. Units, siege engines, mantlets and debris are separate 0.5 m voxel models drawn over it.** This spec defines size, colour and shading only. `Design/GameDescription.md` and `Design/GameDesign.md` govern rules; the design changes this implies are listed in section 9.

Reference implementations: `Earthrise Siege Scene v5.dc.html` (scene) and `MVP Asset Sheet.dc.html`, default mode "1 m world + 0.5 m models".

## 1. Two grids

| Grid | Voxel | Holds | Simulated? |
|---|---|---|---|
| **World** | **1 m** | terrain, all templates (walls, towers, keep, hall, palisades, sheds), earthworks, quarries, craters, rubble, trees | **Yes.** It is the simulation's voxel: support search, collapse, slump, navigation, edits and cost all run on it |
| **Models** | **0.5 m** | units, rams, trebuchets, mantlets, carts; debris and loose blocks; banners | **No.** Units and engines are entities with fixed-point positions; debris is spectacle |

- **World voxel = simulation voxel = 1 m.** This is the "coarser simulation voxel" response in MvpPlan.md › Risks, taken as the baseline. Cost is 8× less than a 0.5 m world: the 512 m MVP map at about 64 m deep is roughly 17 million voxels rather than about 134 million.
- **Models are oriented voxel models** drawn through the ADR-001 pipeline. They sit on the world surface at entity positions, rotate in 90° steps, and are never written into the world grid.
- **Debris:** the voxel-scale "tumbling blocks and dust" of a collapse are models. The simulation result is world rubble at 1 m. Client-side model debris is scattered over that rubble for texture: 0.5–1 m loose blocks and 0.5 m pebbles, deterministic from a hash of the collapse id. It never blocks paths or shots.

## 2. Sizes

- **Standard stone wall: 2 m thick** (it was 1.5 m). A section of 8 × 2 × 6 m costs 96 m³ of stone.
- **Thick wall: 3 m thick**, which costs 1.5× the standard. The design said "about two thirds more"; see section 9.
- **Battlements:** merlons are 1 m wide and 1 m tall with 1 m gaps. Arrow slits and windows are 1 × 2 m dark voxels.
- **Towers:** 7 × 7 m and 11 m high, hollow with 2 m walls. **Keep:** 12 × 12 × 16 m. **Hall:** about 11 × 7 m in timber.
- **Terrain:** heights in whole metres. Ditch: 4–5 m wide and 2 m deep with stepped 1 m sides. Rampart: 2 m high.
- **Unit model:** 1 × 1 × 4 voxels (0.5 × 0.5 × 2 m). Spears, bows, shields and tools are 0.5 m voxels beside the body, which gives each unit a silhouette cue.
- **Trebuchet model:** about 12 m long and 12.5 m tall, with 0.5 m beams. **Ram model:** about 3 × 7 m.

## 3. Material palette (sRGB, converted to linear before lighting; no tone mapping)

| idx | Material | sRGB | Jitter | Grid |
|---|---|---|---|---|
| 1 | Grass (top of exposed earth) | #7d8b4f | 0.05 | world |
| 2 | Earth | #8a6a48 | 0.04 | world |
| 3 | Common rock | #8d8a84 | 0.04 | world |
| 4 | Building stone (raw) | #ddd2b9 | 0.04 | world |
| 5 | Bedrock | #4a4744 | 0.04 | world |
| 6 | Dressed stone (built) | #c6bca7 | 0.07 | both |
| 7 | Timber | #7a5534 | 0.05 | both |
| 8 | Rubble (stone) | #a69c8a | 0.07 | both |
| 9 | Water | #4f7f8c | 0.03 | world |
| 10 / 11 | Leaves A / B | #4f6a3a / #62803f | 0.10 | world |
| 12 | Roof | #8c4a3a | 0.05 | world |
| 13 / 14 | Field A / B | #c9a95a / #a6a056 | 0.04 | world |
| 15 | Road | #a88e6a | 0.04 | world |
| 16 | Faction (red example; blue #2f5d93) | #b23a2e | 0.05 | models, banners |
| 18 | Iron | #9aa0a6 | 0.05 | models |
| 19 | Laborer cloth | #c79a6b | 0.05 | models |
| 20 | Opening (slits, doors, windows) | #2e2a27 | 0.05 | world |
| 21 | Counterweight | #5b5650 | 0.05 | models |
| 22 | Skin | #d9a57f | 0.05 | models |
| 23 | Cloth (legs) | #3d3530 | 0.05 | models |
| 24 | Horse | #5e4330 | 0.05 | models |
| 25 | Timber rubble | #5f4530 | 0.07 | both |
| 26 | Earth rubble | #7d6044 | 0.05 | both |
| 27 | Leather (hides, hoods) | #6e5a3e | 0.05 | models |

**Rules**
- **Built vs raw stone:** dressed stone must read darker and greyer than raw building stone.
- **Faction colour:** only on models and banners, never on structures.
- **Rubble mix:** about 12% of world rubble voxels from stone are drawn in dressed stone.

## 4. Colour jitter

- **What it changes:** HSL lightness only, by ±jitter/2. Hue and saturation never change.
- **How it's chosen:** a deterministic hash of (cell, material), stable across frames and edits.
- **World grid:** the cell is the voxel (1 m) for every material.
- **Model grid:** the cell is the model voxel (0.5 m). Terrain materials never appear in models.

```
h = x*374761393 + y*668265263 + material*982451653   // wrapping int32
h = (h ^ (h >>> 13)) * 1274126177                     // wrapping int32 multiply
r = (h ^ (h >>> 16)) as uint32 / 4294967295           // with (x, y) = (cx*7 + cy*131, cz*3)
```

## 5. Corner shading (voxel AO)

These are per-face corner values from the three neighbours in front of the face (side 1, side 2, diagonal), blended bilinearly:

```
ao = (side1 && side2) ? 0 : 3 - (side1 + side2 + corner)
```

| ao level | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| Structures, rubble, trees, models | 0.52 | 0.68 | 0.84 | 1.00 |
| Terrain materials (idx 1–5, 13–15) | 0.74 | 0.84 | 0.93 | 1.00 |

- **Models see the world:** a model's AO samples both grids. A model neighbour is solid if the model voxel is set **or** the world voxel containing it (`floor(p / 2)`) is set. This gives contact shading where feet meet ground and blocks meet rubble.
- **World ignores models:** the world grid's AO does not sample models.
- **Edges:** outside the world counts as empty, and below y = 0 counts as solid.

## 6. Lighting

Diffuse-dominant: roughness about 0.95, no metalness, no specular highlights.

| Preset | Sun | Intensity | Direction (towards light) | Hemi sky / ground | Hemi | Clear |
|---|---|---|---|---|---|---|
| Golden (default) | #ffd8a6 | 2.7 | (−90, 75, 55) | #d4dbe6 / #5a4a38 | 1.0 | #d6ccba |
| Morning | #fff3e2 | 2.4 | (60, 120, 80) | #dfe8f0 / #5d5446 | 1.15 | #c9d3d6 |
| Dusk | #ff9858 | 2.3 | (−130, 42, 25) | #5b6690 / #2a2018 | 0.7 | #2c2b34 |

- **Intensities** are three.js r160 units; match them by eye.
- **Shadows:** one soft directional shadow over the visible holding. World shadows can update lazily, on edit or sun change. Models cast and receive shadows every frame, and receive the world's shadow.
- **Ordering:** apply AO to albedo before lighting.

## 7. Camera and level of detail

- **Projection:** orthographic isometric at about 34° elevation, rotating in 90° steps eased over about 0.3 s. Default azimuth −45°, from the south-west.
- **Pixel sizes at 1080p:** at the default framing of about 84 m view height, a world voxel is about 13 px and a model voxel about 6–7 px. At the widest zoom of about 260 m view height, a world voxel is about 4 px, which needs no world LOD.
- **Model LOD:** when a model voxel drops below about 3 px (view height over roughly 180 m), draw units as a 1 × 1 × 2 world-scale impostor in faction colour plus helmet or hood. Engines stay full detail, since there are few of them.
- **Mobile landscape** (reference `Earthrise Siege Scene v4 Mobile.dc.html`): the view height scales as 128 / aspect, clamped between 58 and 120 m. Keep tap targets at 44 px or more, and use a 2048² shadow map.

## 8. Acceptance checks

1. Ground at default framing reads calm, with no speckle. One jitter cell is one 1 m world voxel.
2. Dressed and raw stone are distinguishable.
3. A breach reads as ragged: a 1 m notch plus model debris scattered over 1 m rubble.
4. Every unit type in the MVP is identifiable at default zoom by silhouette cue. Sappers and spearmen must differ.
5. Units visibly stand on the ground. Contact AO is present, and no model floats above or sinks into the 1 m surface.
6. A world edit updates the voxel and its neighbours' colour and AO on the next frame, with no seams. Model debris for a collapse appears the same on every client, because it comes from the collapse id.
7. The voxel pass stays within 8 ms at 1080p at the default zoom on an RTX 3060-class GPU (the M1 pass mark).

## 9. Proposed design-document changes (for the owner)

| Where | Current text | Proposed |
|---|---|---|
| Fortifications | Standard wall 1.5 m; thick wall 2.5 m "costs two thirds more stone" | Standard **2 m**; thick **3 m**, costs **half as much again** (1.5×) |
| Building, example cost | "8 m × 1.5 m × 6 m costs 72 m³" | "8 m × 2 m × 6 m costs **96 m³**" |
| Materials, intro | "Voxels small enough that a crater … has the shape of whatever made it" | "**1 m** voxels: a crater's size and depth show which weapon made it; dust and loose blocks are drawn by the client" |
| How it is built | (none) | "Units, engines and debris are separate voxel models at 0.5 m, drawn over the 1 m world and never written into it" |
| GameDesign, Decided 5 / ADR to come | Render and simulation voxel sizes decided by the spike | **Baseline: world = simulation = 1 m; models 0.5 m.** The M1 spike measures it against a 0.5 m world, and the ADR records the result |
| Balance targets | Trebuchet breaches a standard section in about 3 min, a thick one in about 6 | Re-tune for 2 m and 3 m walls. Crater radii in 1 m voxels need retuning, so the times may not hold |
