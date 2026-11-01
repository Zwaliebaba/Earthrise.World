# Render parity with v5: what the render-fix package found, and what was done

**Status:** done in code, not yet seen on screen. The owner's visual review of the spike against the v5 scene (2026-11-01) came as eight fixes, kept in [Reference/RenderFixPackage/](Reference/RenderFixPackage/). This note says what became of each. The owner's screenshot after the fixes, and check 7's timing, are still to come.

## How the work was checked without a screen

Claude sessions cannot draw D3D12, so each fix was checked on an offline preview.

- **The preview.** The test world's voxels and model placements were dumped from the C++ itself, built on Linux. They were ray-cast with the voxel pass's lighting: the palette, the jitter, the corner shading, Lambert and hemisphere light, and shadows found by casting a ray to the sun.
- **What it leaves out:** soft shadow filtering (PCF), impostors, debris and fords.
- **How far it can be trusted.** Run on the code before the fixes, it reproduces the owner's screenshot, sawtooth included ([01](RenderParity/01-before-game-and-preview.png): the game above, the preview below). That is the evidence that it can be trusted for layout, materials and lighting.

**These images are previews, not screenshots of the game.** None replaces the owner's look at the spike.

## The cause behind most of it

**The spike drew v5 mirrored north to south.** v5 is a three.js page, right-handed, with +z south. The engine is left-handed, with +z north. M1 converted the camera's azimuth but copied v5's coordinates and the sun's direction unchanged.

That one mistake accounts for most of what the review saw:
- the quarry sat under the north wall;
- the fields were to the north;
- the sun stood in the north-west, so every south face the default camera sees was lit by the sky alone;
- the castle's shadow fell across the stepped slope east of the keep.

The fix converts the frame at one boundary and also mirrors the layer files in the bake. It is recorded as [ADR-017](ADR/ADR-017-the-prototypes-frame-and-the-worlds.md), in #38. After all the fixes, the preview lines up with v5 at the default framing ([02](RenderParity/02-after-v5-and-preview.png): v5 above, the preview below).

## The eight items

| # | Item | Outcome | Where |
|---|---|---|---|
| 1 | Stone material | **No change needed.** The templates already lay masonry (6), not building stone (4). The palette already gives masonry ±0.035 and building stone ±0.02 of lightness jitter, and masonry the structure corner shades. The walls looked like raw stone because half of their faces were unlit (the frame). | #38 |
| 2 | Openings | T1, T2, T4 and T5 come from the package, whose conversion keeps openings. The keep's and the towers' two bare faces carry the same slits as the other two (the owner's choice). Openings cost as their host (ADR-005), so the costs do not drop. | #39 |
| 3 | Keep shape | The keep's top is stone, carried on a 2 × 2 m pier so that it stands where stone spans 2 m (the owner's choice). The support search finds nothing unsupported. Without the pier, 16 roof voxels lose support. No stepped-back storey is drawn: neither v5 nor the package's T2 has one. | #39 |
| 4 | Breach | The breach stands from the first frame. Its notch is at least 4 m wide where the wall's first meter stands, ragged by a voxel. The ramp is stone rubble only; its 12% masonry is the renderer's colour pick. The breach's debris is scattered from its own id and is at rest from the start. | #40 |
| 5 | Quarry | Under the south wall once the frame is right. Its terraces are v5's, 2 m deep and 2 m wide, not the item's "1 m terraces". The floor is common rock, and grass lies only on earth. The scripted quarrying grows the terraced pit at 94 voxels a second for the full minute, instead of levelling the stone body ([04](RenderParity/04-quarry-at-30-and-60-seconds.png): 30 s and 60 s). | #38, #40 |
| 6 | Ditch | The undug stretch before the breach is 9 m, not 15. The ditch is 5 m wide and 2 m deep in 1 m steps, and the rampart 2 m high, as before. | #40 |
| 7 | Shadows | **No change made.** The sawtooth east of the keep shows in the preview, whose shadows come from a ray cast to the sun with no shadow map ([03](RenderParity/03-east-slope-game-and-exact-shadows.png): the game left, the preview right). It is the real shadow of the plateau's 1 m steps, not acne, and the frame fix moves the castle's shadow off that slope. The maps already fit the world (4096² static, 2048² dynamic) and filter 3 × 3. | #38 |
| 8 | Terrain noise | The generated surface is calmed by an opening and a closing over 3 × 3 columns. On the scene's 128 m square no column stands above or below both its neighbours along x or z, on ten seeds tried. | #40 |

## What the owner checks on screen

**Release, at the default framing,** against v5 and [RendererLook.md](RendererLook.md) §9. The package's spec numbers its checks differently; these are ours.
1. **Check 1:** open ground reads calm.
2. **Check 2:** masonry and the quarry's raw stone can be told apart.
3. **Check 3:** the breach reads as ragged, with its ramp and loose blocks through it, from the first frame.
4. **Check 11:** the shadows on the slopes are clean.
5. **Items 2 and 5:** the slits show on every face of the keep and the towers that the camera sees, and the quarry stays terraced through the minute.
6. **Check 10:** after `--measure`, the voxel pass stays within 8 ms. M1 measured 2.16 ms at the 95th percentile ([M1Report-2026-11-01.md](M1Report-2026-11-01.md)). Nothing here adds GPU work beyond the breach's debris, a few hundred oriented voxels.

**For item 7,** if a fault in the shadows remains on screen, the next step is to tune the normal offset and filter on the owner's machine, where they can be judged.
