# Renderer input: voxel size, colouring and shading — Earthrise.World

Use this as the look specification for the voxel renderer: the prototype from step 1 of "How the design is proven", which ray-casts voxels directly without meshing. It defines **size, colour and shading only**. It does not describe any scene, layout or content. Where it conflicts with `Design/GameDescription.md` or `Design/GameDesign.md`, those govern.

It comes from a visual prototype that was tuned to read well from the strategy camera. Implement it so it matches that prototype, and flag anything that is expensive in a ray-caster before building it.

## 1. Voxel size

- **Base voxel: 0.5 m.** This applies to everything solid: structures, rubble, engines, timber, trees, units and terrain.
- **Terrain heights quantize to 1 m.** The generator produces surface heights in whole metres, so slopes are made of 1 m terraces built from 2 × 2 × 2 groups of voxels. Dug or damaged terrain (quarries, ditches, craters, slumps) follows edits at the full 0.5 m resolution.
- **Reference sizes at 0.5 m:**
  - standard wall 1.5 m thick, which is 3 voxels;
  - thick wall 2.5 m, which is 5 voxels;
  - a unit about 0.5 × 0.5 × 2 m, which is 1 × 1 × 4 voxels;
  - a merlon 0.5 m wide and 1 m tall.
- **Why 0.5 m:** at 1 m the breaches, craters and units become blobs. At 0.25 m, terrain gets too busy for the strategy view. 0.5 m with 1 m terrain terraces was the best trade-off.

## 2. Material palette

Colours are authored in sRGB and converted to linear before lighting. The output target is sRGB, with no tone mapping in the reference.

| Material | sRGB | Jitter | Jitter cell |
|---|---|---|---|
| Grass (top voxel of exposed earth) | #7d8b4f | 0.05 | 1 m |
| Earth | #8a6a48 | 0.04 | 1 m |
| Common rock | #8d8a84 | 0.04 | 1 m |
| Building stone (raw outcrop) | #ddd2b9 | 0.04 | 1 m |
| Bedrock | #4a4744 | 0.04 | 1 m |
| Dressed stone (built walls) | #c6bca7 | 0.07 | voxel |
| Rubble | #a69c8a | 0.07 | voxel |
| Timber | #7a5534 | 0.05 | voxel |
| Roof | #8c4a3a | 0.05 | voxel |
| Leaves A / B | #4f6a3a / #62803f | 0.10 | voxel |
| Water | #4f7f8c | 0.03 | voxel |
| Field A / B | #c9a95a / #a6a056 | 0.04 | 1 m |
| Road / track | #a88e6a | 0.04 | 1 m |
| Openings (slits, doors, windows) | #2e2a27 | 0.05 | voxel |
| Iron fittings / helmets | #9aa0a6 | 0.05 | voxel |
| Engine counterweight | #5b5650 | 0.05 | voxel |
| Skin | #d9a57f | 0.05 | voxel |
| Cloth (unit legs) | #3d3530 | 0.05 | voxel |
| Faction colours (example) | #b23a2e red, #2f5d93 blue | 0.05 | voxel |
| Laborer | #c79a6b | 0.05 | voxel |

**Rules**
- **Rubble mix:** rubble from dressed stone is about 12% dressed-stone voxels mixed into rubble. The material is still rubble; this is a colour pick only.
- **Built vs raw stone:** dressed stone (walls) is darker and greyer than raw building stone (outcrops). Players should be able to tell at a glance what was built from what was quarried.
- **Faction colour on units only:** in this palette, faction colour appears only on units and banners. Structures stay in their material colours.

## 3. Colour jitter

Jitter breaks up flat colour fields so the voxel grid reads as material, not plastic.

- **What it changes:** HSL lightness only, by ±jitter/2. Hue and saturation never change.
- **How it's chosen:** a deterministic hash of the jitter-cell coordinate and the material ID. It must be stable across frames and edits: the same voxel always gets the same shade.
- **Terrain materials** use a **1 m jitter cell**, keyed by floor(voxel / 2). This is what keeps the ground calm. Per-voxel jitter on terrain looked restless at 0.5 m and is not allowed.
- **Everything else** jitters per voxel.

Reference hash (32-bit integer arithmetic, result in [0,1]):

```
h = x*374761393 + y*668265263 + material*982451653   // wrapping int32
h = (h ^ (h >>> 13)) * 1274126177                     // wrapping int32 multiply
r = (h ^ (h >>> 16)) as uint32 / 4294967295
```

In the prototype, (x, y) are (cellX*7 + cellY*131, cellZ*3).

## 4. Corner shading (voxel ambient occlusion)

This is per-face, per-corner occlusion from the three neighbours in front of the face at that corner: side 1, side 2 and the diagonal.

```
ao = (side1 && side2) ? 0 : 3 - (side1 + side2 + corner)
```

| ao level | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| Structures, units, trees, rubble | 0.52 | 0.68 | 0.84 | 1.00 |
| Terrain materials (section 2, 1 m cell) | 0.74 | 0.84 | 0.93 | 1.00 |

- **Blending:** interpolate bilinearly across the face. In a ray-caster, evaluate the four corner values for the hit face and blend them at the hit point's UV.
- **Reduced terrain AO is required,** not a tuning option. Full AO on 1 m terraces makes the land read as noise.
- **Edges of the world:** cells outside the world count as empty for AO, and cells below y = 0 count as solid.

## 5. Lighting

Use a Lambert / diffuse-dominant model: roughness about 0.95, no metalness, no specular highlights on voxels.

| Preset | Sun colour | Sun intensity | Sun direction (towards light) | Sky / ground (hemisphere) | Hemi intensity | Clear colour |
|---|---|---|---|---|---|---|
| Golden (default) | #ffd8a6 | 2.7 | (−90, 75, 55), normalized | #d4dbe6 / #5a4a38 | 1.0 | #d6ccba |
| Morning | #fff3e2 | 2.4 | (60, 120, 80) | #dfe8f0 / #5d5446 | 1.15 | #c9d3d6 |
| Dusk | #ff9858 | 2.3 | (−130, 42, 25) | #5b6690 / #2a2018 | 0.7 | #2c2b34 |

Intensities are three.js r160 physical units. Match them by eye in the engine, not by number.

- **Shadows:** one directional shadow from the sun, soft-edged (PCF or similar), covering the whole visible holding.
  - Terrain and structures only change on edits, so shadow updates can be lazy: on edit and on sun change, not every frame.
  - In a ray-caster, the shadow can be a second ray towards the sun.
- **Ordering:** apply AO to the albedo before lighting, as in the prototype. That keeps AO visible in shadow.

## 6. Camera (affects how sizes read)

- **Projection:** orthographic isometric.
- **Elevation:** about 34° (0.6 rad).
- **Rotation:** azimuth in 90° steps, eased over about 0.3 s. Default azimuth is −45°, from the south-west.
- **Zoom range:** from about 12 m view height (single stones) to about 260 m (a whole holding).
- **Calibration:** at the default framing of about 84 m view height on a 1080p frame, one 0.5 m voxel is about 6–7 px. Below about 3 px per voxel, consider a level-of-detail step to 1 m voxels for structures. Terrain is already uniform at 1 m.

## 7. Non-voxel effects

- **Debris and dust:** drawn by the client, never part of the voxel grid. Dust is at least 0.8 m.
- **Water:** flat and opaque in the reference. Transparency or animation is optional and must not hide fords.
- **Readability overlays** are separate from these rules: integrity view, collapse preview, silhouettes and passability. Never bake them into albedo.

## 8. Acceptance checks

1. A 128 × 128 m holding at default framing has calm ground: no visible per-voxel speckle on grass or earth.
2. Dressed stone walls and raw building stone are distinguishable at default zoom.
3. A breach notch and its rubble ramp read as ragged at default zoom, not as a clean rectangle.
4. Units read as figures (legs, body, helmet) at default zoom, and faction colour is identifiable.
5. Editing a voxel changes its colour and AO, and its neighbours' AO, on the next frame, with no seams.
6. The same seed and edits produce the same colours on every machine (the hash is deterministic).
