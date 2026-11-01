# Earthrise.World: MVP runtime assets

**For Claude Code.** These are the MVP assets that `Design/MvpAssets.md` was missing: trebuchet poses, projectiles, open gates and collapse debris. All are **0.5 m display-only models** (ADR-006), except the arrow (X2), which is a line model; see *The arrow exception*. They set no costs and no rules, and they are never written into the world grid.

Source and preview: `reference/MVP Asset Sheet.dc.html`, section 06. Open it beside `support.js`; every card exports `.vox`, `.json` and `.png`. File formats are those of ADR-005 and the v2 package. Positions are in metres, y is up and +z is the front.

## Contents
| Id | Model | Grid (voxels) | Extent (m) | Use |
|---|---|---|---|---|
| S3 | Trebuchet, released | 26 × 28 × 9 | 11.5 × 13 × 3.5 | The existing S3 drawing, re-exported unchanged and renamed (see below) |
| S3c | Trebuchet, cocked | 34 × 20 × 9 | 13 × 9 × 3.5 | Ready to throw: arm down behind, counterweight up |
| S3p | Trebuchet, packed | 16 × 7 × 7 | 8 × 3.5 × 3.5 | Marching: broken down onto a four-wheeled wagon; cannot fire |
| X1 | Trebuchet shot | 2 × 2 × 2 | 1 × 1 × 1 | One per shot in flight |
| X2 | Arrow | none (line model) | 0.8 m, drawn 1 m | One per arrow in flight; **not voxels** |
| G1 | Gatehouse gate, raised | 6 × 1 × 1 | 3 × 0.5 × 0.5 | T5 open state |
| G2 | Palisade gate, open | 8 × 6 × 3 | 4 × 3 × 1.5 | T7 open state |
| D1–D3 | Stone debris: block, pair, corner | 1–3 voxels | ≤ 1 m | Over stone and rock rubble |
| D4–D5 | Timber debris: beam, splinters | 3 voxels | ≤ 1.5 m | Over timber rubble |
| D6 | Earth clod | 2 voxels | 1 m | Over earth rubble |

## Important: S3's existing pose is *released*
The S3 model in `GameData/Client/` has the counterweight down and the arm up. That is the pose just after a throw, not the cocked pose.
- **Rename it:** S3's file becomes S3 *released*. The other two poses are new.
- **ADR-015 mislabels it.** Its machine row gives "the trebuchet, cocked" as 11.5 × 13 × 3.5 m, which are this released pose's dimensions. The cocked pose is 13 × 9 × 3.5 m: it is longer and lower, because the arm lies behind the frame. Either correct the label, or size the clearance on the largest of the three poses (13 m long, 13 m high).
- **Pose cycle:**
  - packed (moving, cannot fire);
  - deploy;
  - cocked;
  - fire;
  - released for the ~1 s of the throw, then back to cocked over the reload.

  The deploy and pack timings are M4's.
- **Swapping poses:**
  - **Released and cocked** share the grid origin and frame voxels; swap them with no offset. The base frame lies at x 4–12 m, z 0.5–4 m.
  - **Packed is a wagon with its own origin.** Place it at (+4, 0, +0.5) m from the engine origin, which puts the wagon's centre on the frame's centre. It faces +x, so the draw pole is at +x.
  - **Clearance:** the packed wagon is 8 × 3.5 m, so navigation can use a smaller footprint while marching. Under ADR-015's machine class, it still needs width 3 and 4 m headroom.

## Projectiles
- **Placement:** draw both at the shot's Q16.16 position, interpolated between ticks on the client.
- **X1 (shot):** a voxel cube, so it needs no rotation. It is drawn 1 m across, about 13 px, where the real stone is about 0.5 m. It uses raw building stone (4), since shot is cut from the stockpile.

## The arrow exception
A 0.5 m voxel is wider than an arrow is long, so no voxel arrow reads as an arrow. **X2 is the one model that is not voxels.** `earthrise-x2-arrow.json` describes it as three line segments along the arrow's axis:

| Part | Along the axis (m) | Colour | Width |
|---|---|---|---|
| Fletching | 0–0.18 | Faction (palette 16) | 3 px |
| Shaft | 0.18–0.86 | #c79a6b | 2 px |
| Head | 0.86–1.0 | Iron #9aa0a6 | 3 px, tapering to a point |

- **Size:** it is drawn 1 m long, about 13 px at the default zoom, against a real arrow's 0.8 m. Its width is in **screen pixels**, so it stays readable at every zoom. Below about 6 px long it is drawn as a 2 px dot.
- **Orientation:** it rotates freely along the velocity, pitch and yaw included. This breaks MvpPlan's "rotated in 90° steps" rule, which still holds for every voxel model.
- **How it renders:**
  - it is unlit and casts no shadow;
  - it is depth-tested against the voxel pass, so walls hide it;
  - an optional tracer, 1 px, #f3ede2 fading from 50% to 0 over the last 0.2 s, is the first thing to try if a volley still doesn't read.
- **Engine:** ADR-001's pipeline draws only voxels, so this needs a small instanced-line pass after it:
  - one instance per arrow (position, direction, faction);
  - expanded to screen-aligned quads in the vertex shader;
  - drawn in the same reversed-Z depth.
  
  The same pass can later carry tracers and rain.
- **Docs to amend:**
  - **ADR-006**, decision 2: "models use 0.5 m voxels" gets the arrow as its one exception;
  - **MvpAssets.md:** list X2 under Models as a line model.

## Gates: how the open state works
- **The gate voxels stay in the world grid.** They are T5's and T7's critical part, so cost, damage and craters all still apply.
- **The gate state is a flag on the template instance**, `open` or `closed`, set by the owner's command or automatically.
- **Simulation:** while `open`, navigation treats the gate cells as passable for the owner's units only. Shots still stop at them, since a raised gate is still masonry overhead and open leaves are still timber.
- **Client:** while `open`, skip drawing the instance's gate voxels (part 1, timber and iron at the gate cells) and draw the open model instead:
  - **G1 (T5):** place it at T5-local (3, 3.5, 6) m. Only the iron-tipped bottom edge of the raised gate shows, 0.5 m under the passage head. The passage stays 3 × 3.5 m clear, enough for the 3 × 3.5 m ram.
  - **G2 (T7):** place it at T7-local (2, 0, 0) m. Both leaves swing inward (−z) through 90° and lie beside the posts, so the full 3 m stays clear.
- **When a gate is destroyed**, its voxels are removed by the normal rules and the open model is no longer drawn.

## Debris
- **Choosing pieces:** by the rubble's material.
  - Stone rubble (8) and rock rubble (28) use D1–D3. For rock, tint the pieces with rock rubble #7f7c77.
  - Timber rubble (25) uses D4–D5.
  - Earth rubble (26) uses D6.
- **Scatter (ADR-006, decision 5):**
  - seed a PRNG with the collapse id;
  - for every ~4 m² of rubble top surface, place one piece on the top face of a rubble cell;
  - give each piece a 90° yaw;
  - offset each piece by 0, 0.25 or 0.5 m within the cell.
- **Never in the world:** debris never blocks a path or a shot, and is never written into the world.
- **Motion:** in-flight debris during the fall may tumble freely, since it's client-only. It then settles to the scatter above. Pieces are "oriented voxels" (MvpPlan), so any rotation is allowed while they're moving.

## Integration
1. **Copy the files:**
   - `assets/models/**` → `GameData/Client/Models/`, in the matching subfolders;
   - rename the existing S3 file to `earthrise-s3-trebuchet-released-05m`;
   - merge `manifest.json`'s `assets` into the client manifest.
2. **Bake:** run `Tools/BakeContent.py` and `Tools/CheckAssets.py`. The new models have no `parts` grid, since models carry no rules.
3. **M2:** add the trebuchet's `pose` to its entity state; it's a byte. X1 is drawn for every shot in flight.
4. **M4:** add arrows (X2), the gate-state flag with its passability rule and the gate models (G1, G2), and pose timings.
5. **M1 debris:** the spike's own debris is replaced by D1–D6 with the scatter rule above.
6. **Docs:**
   - add the 13 models to `Design/MvpAssets.md` under Models;
   - remove the missing-poses known issue;
   - correct ADR-015's cocked trebuchet row.
