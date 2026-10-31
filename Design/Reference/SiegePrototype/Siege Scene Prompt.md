# Design session: "The Breach at the West Wall" — Earthrise.World key scene

You are helping build the key scene for the Earthrise.World game brief: a single isometric voxel diorama that shows the game's three pillars in one frame. Read `Design/GameDescription.md` first; this scene must stay consistent with it, and where they differ, `Design/GameDesign.md` governs.

The reference is `Earthrise Siege Scene v4.dc.html` (a three.js prototype). Reproduce what it shows, using the spec below as the source of truth.

## Goal

One frame, readable in five seconds, that shows:

1. **Walls fall where they are struck.** A breach in the west curtain wall with a rubble ramp, and no health bars.
2. **Where you dig decides how you can be attacked.** A terraced quarry under the south wall, which is also the castle's moat.
3. **Siege is fought in the economy.** Trebuchets whose shot costs quarried stone.
4. **The siege choice.** The ditch and rampart stop short of the breach, so the attacker struck the one unditched stretch, on deep earth.

Setting: Skirmish, minute 34, a Mason holding under Citadel-era siege.

## Scale and resolution

- **Area:** 128 × 128 m holding. +x is east and +z is south. All coordinates below are in metres.
- **Voxels:** 0.5 m for structures, rubble, engines, trees and units (2 voxels per metre). Height runs to 42 m (84 voxels).
- **Terrain is deliberately calmer.** Ground heights snap to 1 m terraces, colour varies per 1 m patch and not per voxel, and corner shading is lighter on terrain. This keeps the strategy view calm while siege details stay crisp.
- **Map edges are cut open** like a diorama, so the strata show: grass, earth, common rock, building stone and bedrock.

## Materials (sRGB)

| Material | Hex | Notes |
|---|---|---|
| Grass (top of earth) | #7d8b4f | ±2.5% lightness per 1 m patch |
| Earth | #8a6a48 | deep (3–6 m) on the west side, thin (≤1.5 m) on the east |
| Common rock | #8d8a84 | |
| Building stone | #ddd2b9 | outcrop under the quarry |
| Bedrock | #4a4744 | bottom 1–4 m |
| Dressed wall stone | #c6bca7 | ±3.5% per voxel |
| Rubble | #a69c8a | about 12% mixed with wall stone |
| Timber | #7a5534 | |
| Roof | #8c4a3a | |
| Leaves | #4f6a3a / #62803f | ±5% per voxel |
| Water | #4f7f8c | flat, does not flow |
| Fields | #c9a95a / #a6a056 | stripes 3 m wide |
| Road | #a88e6a | |
| Attacker | #b23a2e | |
| Defender | #2f5d93 | |
| Laborer | #c79a6b | |
| Steel | #9aa0a6 | helmets, axles, spear tips |
| Slits, doors, windows | #2e2a27 | |
| Counterweight | #5b5650 | |

## Terrain

- **Base height:** 6–12 m from low-frequency value noise (fbm, frequency 0.035).
- **Castle hill:** centred at (74, 61). The plateau sits at G = 15 m within a radius of 27 m and blends down to the base height by a radius of 42 m.
- **River:** its centreline is x = 12 + 0.12z + 5·sin(0.06z). The bed is 2 m deep and water is filled to 4 m within 4.5 m of the centreline.
- **Building-stone outcrop:** an ellipse around (74, 93) with radii of about 25 × 17 m, running from 3 m deep up to the surface.

## Castle (base at G = 15 m)

- **Curtain walls:** 1.5 m thick and 7 m high, on the rectangle x 58–91, z 45–78. Merlons are 0.5 m wide with 0.5 m gaps, 1 m tall, on the outer edge.
- **Corner towers:** 7 × 7 m and 11 m high, with merlons on all four edges. Arrow slits (0.5 × 1.5 m) at +3 m and +7.5 m on every face.
- **Keep:** x 68–80, z 55–67, 16 m high, with 2.5 m corner turrets up to 18.5 m. Three rows of windows, a door on the west face, and a red banner on a 7 m pole.
- **Gatehouse:** on the east wall at x 87–94, z 57–65, 9 m high, with a timber gate 3.5 m tall. The road runs east from it.
- **Cottages:** five timber cottages of 5 × 4 m in the bailey, with pitched roofs that overhang by one voxel.

## The breach (the hero detail)

- **Notch:** a V cut through the west wall, centred at z = 61. Its half-width is 1.4 m + 0.6 × (height above G), with a ragged edge from noise.
- **Rubble ramp:** an ellipse centred on the breach with radii of 8.5 m (x) and 5.6 m (z), and a peak of 3 m. It spills both outward and into the bailey.
- **Loose blocks:** about 40 dressed-stone blocks of 0.5–1 m scattered within 10 m.
- **Fresh craters:** a 1.4 m crater on the west wall at z 70.5, a 1.2 m crater on the NW tower, and a 1 m crater at z 51.5. These show hits that have not opened the wall yet.

## Earthworks

- **Ditch:** 4 m wide at the top and 2 m deep with 1:1 slopes, centred at x = 50.5.
- **Rampart:** 1.5 m high, at x = 54.5, on the keep side of the ditch.
- **Extent:** both run from z 34 to 89 and taper out around a gap at z 54–68, in front of the breach.

## Quarry

- **Pit:** x 61–89, z 85–103, terraced in 2 m steps, 2.2 m wide, down to 5 m.
- **Ramp out:** along the north rim.
- **Stone stacks:** east of the pit.
- **Laborers:** about 14, on the terraces.

## Siege lines (west)

- **Trebuchets:** two, at (32, 49) and (34, 69.5).
  - Base frame of 8 × 3.5 m, made of 0.5 m beams.
  - A-frames up to an axle at 6.5 m.
  - A 12 m arm in its post-release pose, with the tip high to the west and a 2 m counterweight box hanging at the short end.
  - A sling hanging from the tip.
- **Shot stack:** stone shot near the engines.
- **Mantlets:** four timber screens, 2.5 m wide and 2 m tall, leaning west on props. They stand at x 44–45, z 53–71.
- **Forest:** in the NW and SW. Trunks are 2.5–4.5 m with crowns of 1.4–2.5 m radius and dithered edges. Stumps stand closest to the engines, because the timber went into them.

## Units

- **Figure:** 0.5 × 0.5 m footprint and 2 m tall (4 voxels). Legs in cloth, two voxels of team colour, and a steel helmet, or a skin-coloured head for laborers. Some carry a 2.5 m spear with a steel tip.
- **Attackers (about 65):** storming the rubble, a few at the top of the breach, groups behind the mantlets, a reserve column in the ditch gap, and crews at the engines.
- **Defenders (about 30):** inside the breach and around the keep.

## Camera and light

- **Camera:** orthographic isometric, elevation about 34°, with the default view from the south-west at azimuth −45°. It rotates in 90° steps with eased motion, as in the game.
- **Framing:** a view height of about 84 m, slightly cropping the map corners, with the frame centred on the castle.
- **Light presets:**
  - Golden, the default: sun #ffd8a6 at 2.7 from the low west. Hemisphere light with sky #d4dbe6 and ground #5a4a38. Background #d6ccba.
  - Morning: sun #fff3e2 high from the south-east, background #c9d3d6.
  - Dusk: sun #ff9858 very low from the west, background #2c2b34.
- **Shadows:** soft shadows cover the whole holding. Bake them once, since the scene is static.
- **Voxel corner shading:** 4 levels at 0.52, 0.68, 0.84 and 1.0 for structures, and 0.74, 0.84, 0.93 and 1.0 for terrain.

## Motion

- **Shot:** one stone shot per trebuchet loops every 3.6 s on a parabolic arc (about 24 m apex) to the breach.
- **Dust:** a burst at impact that rises and fades over about 1.2 s.
- **Static voxels:** nothing in the voxel scene changes. In line with the design, debris is spectacle and never changes the outcome.

## Overlay (brief frame, 1920 × 1080)

The overlay uses EB Garamond for headings, IBM Plex Sans for body text and IBM Plex Mono for labels. Panels are rgba(21,19,16,.9) with #f3ede2 text and #e8b86a accents.

- **Title (top left):** "Earthrise.World — Key Scene" / "The Breach at the West Wall" / a one-line setup.
- **Markers 1–4:** pinned to the breach, the quarry, the north trebuchet and the end of the ditch, with notes in a panel at top right:
  1. **The breach:** no health bars; shot cut a crater, the section lost support and fell, leaving a rubble ramp.
  2. **The quarry is the moat:** the castle was cut from this outcrop, and the pit is a ditch engines can't cross.
  3. **Trebuchets:** every shot costs the attacker quarried stone, and mantlets screen the crews.
  4. **Where the ditch ran out:** ditch and rampart protect the west wall except one stretch on deep earth, where the attackers struck.
- **Legend (bottom left):** the material swatches plus "1 voxel = 0.5 m · terrain in 1 m terraces · 128 × 128 m holding".
- **Controls (bottom right):** rotate ↺ / ↻.

## Rules to respect (from the design)

- No unit stands on a wall. No ladders, no siege towers.
- Rubble never forms a ramp over an intact wall, only into a breach.
- All digging is open to the sky, with no tunnels.
- No gunpowder. The trebuchet is the strongest weapon.

## Deliverable for this session

Recreate the scene in the target you're working in (three.js prototype or the engine's renderer prototype), keeping the scale, layout and palette above. Then propose up to three changes that would make the pillars read faster, and explain why each one helps before you make it.
