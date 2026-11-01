# Earthrise.World: MVP assets v3 (runtime states and effects)

**For Claude Code. Start here.** This package fills the gaps found when `Design/MvpAssets.md` was checked against `Design/MvpPlan.md` › *In*:
- **trebuchet poses:** cocked and packed;
- **projectiles:** a trebuchet shot and an arrow;
- **open gates:** for the gatehouse and the palisade gate;
- **collapse debris:** six pieces.

It also makes one **design decision: the arrow is not a voxel model.**

The files are laid out to mirror the repository, so `GameData/…` in this package lands at `GameData/…` in the repo.

## What's in the package
| Path | What it is |
|---|---|
| `README.md` | This file: what changed and why |
| `PROMPT.md` | The task for Claude Code, step by step |
| `GameData/Client/Models/*.json` | 11 new 0.5 m models in ADR-005 layer format, repo-ready (JSON only, no `.vox`) |
| `GameData/Client/Lines/earthrise-x2-arrow.json` | The arrow, as a **line model** (a new kind, see below) |
| `GameData/Manifest-additions.json` | Entries to merge into `GameData/Manifest.json`, plus the debris-by-rubble table |
| `Reference/MVP Asset Sheet.dc.html` | The design source and preview (section 06). Open it in a browser next to `support.js` |
| `Reference/runtime-assets-README.md` | The longer design notes these assets were drawn from |

## What changed

### 1. Trebuchet: three poses instead of one
- **The existing S3 drawing is the *released* pose**, not cocked: its counterweight is down and its arm up, as just after a throw.
  - **Keep** `GameData/Client/Models/earthrise-s3-trebuchet-05m.json` as it is.
  - **Add** `"pose": "released"` to its file and its manifest entry.
- **New S3c, cocked**, 34 × 20 × 9 voxels (13 × 9 × 3.5 m). The arm is hauled down so its long end and sling lie on the ground behind the frame, and the counterweight is raised.
  - The frame occupies the same voxels as in S3, so S3 and S3c swap with no offset.
- **New S3p, packed**, 16 × 7 × 7 voxels (8 × 3.5 × 3.5 m). The engine is broken down onto a **four-wheeled wagon**:
  - 2 m spoked wheels;
  - the counterweight box at the rear;
  - the arm laid along the top;
  - a hide hood over the front;
  - a draw pole facing +x;
  - faction trim.

  It has its own origin. Place it at **(+4, 0, +0.5) m** from the engine origin, which puts the wagon's centre on the frame's centre.
- **Pose cycle:**
  - packed (marching; cannot fire);
  - deploy;
  - cocked;
  - fire;
  - released for about 1 s;
  - cocked again over the reload.

  The deploy and pack times are M4 values.
- **ADR-015 is off by one label.** Its "trebuchet, cocked" row (11.5 × 13 × 3.5 m) gives the *released* pose's dimensions.
  - **Rename** the row to released.
  - **Add a cocked row:** 13 × 9 × 3.5 m.
  - **Add a packed row:** 8 × 3.5 × 3.5 m.
  - **Size the machine clearance** on the largest pose that moves. Only packed moves, so a marching trebuchet clears like the ram. A deployed trebuchet doesn't path.

### 2. Projectiles
- **X1, trebuchet shot:** a 2 × 2 × 2 voxel cube of raw building stone (4), so it's 1 m across.
  - The real stone is about 0.5 m. It's drawn at double size so it reads at about 13 px per metre.
  - A cube needs no rotation.
- **X2, arrow:** see the next section.

### 3. The arrow is not voxels (a design decision)
**Why:** the smallest voxel in the game is 0.5 m. A real arrow is about 0.8 m long and 1 cm thick.
- **One voxel:** a cube half a metre wide, which reads as a block.
- **Two voxels:** a 1 m brick, which reads as a plank.

No voxel arrow reads as an arrow at any zoom. Arrows also tilt as they fly, and voxel models only turn in 90° steps.

**Decision:** X2 is the **one exception** to ADR-006's "models use 0.5 m voxels". It's drawn as a real arrow, a thin line in three segments along its axis:

| Part | Along the axis (m) | Colour | Width on screen |
|---|---|---|---|
| Fletching | 0–0.18 | Faction (palette 16) | 3 px |
| Shaft | 0.18–0.86 | #c79a6b | 2 px |
| Head | 0.86–1.0 | Iron #9aa0a6 | 3 px, tapering to a point |

- **Size:** it's drawn 1 m long, against a real 0.8 m, which is about 13 px at the default zoom.
  - The **width is fixed in screen pixels**, so it stays readable at every zoom.
  - Below about 6 px long it's drawn as a 2 px dot.
- **Orientation:** it rotates **freely along its velocity, pitch included**. The "rotate in 90° steps" rule still holds for every voxel model.
- **How it's drawn:**
  - unlit;
  - casts no shadow;
  - depth-tested against the voxel pass, so walls and rubble hide it.
- **Tracer (optional):** a 1 px tracer, #f3ede2, fading from 50% to 0 over the last 0.2 s. Add it only if a volley still doesn't read.
- **Simulation:** unchanged. Arrows are still physical shots through the voxel grid (GameDescription.md). Only how the client draws them changes.
- **Engine:** ADR-001's pipeline draws only voxels, so this needs a **small instanced-line pass** after the voxel pass:
  - one instance per arrow (position, direction, faction);
  - expanded to screen-aligned quads in the vertex shader;
  - the same reversed-Z depth.

  The same pass can later carry tracers, rain or ropes.
- **Where the file goes:** a line model isn't a layer file, so it can't sit in `Client/Models/`, where the asset check expects voxels. It gets a new folder, `GameData/Client/Lines/`.
  - The bake gains a small reader for it.
  - The asset check validates it against the fields in the file.
  - `GameData/README.md` gets a row for the new folder.

### 4. Open gates
- **The gate voxels stay in the world grid.** They're T5's and T7's critical part, so cost, damage and craters are unchanged.
- **Gate state** is an `open`/`closed` flag on the template instance.
- **Simulation:** while `open`, navigation lets the **owner's** units through the gate cells, and shots still stop at them.
- **Client:** while `open`, skip the instance's gate voxels (part 1 at the gate cells) and draw the open model instead:
  - **G1, gatehouse gate raised** (6 × 1 × 1 voxels), at T5-local (3, 3.5, 6) m. Only the iron-tipped bottom edge of the lifted gate shows, so the passage stays 3 × 3.5 m clear for the ram.
  - **G2, palisade gate open** (8 × 6 × 3 voxels), at T7-local (2, 0, 0) m. Both leaves are swung inward through 90° beside the posts, so the full 3 m stays clear.
- **When a gate is destroyed**, its voxels go by the normal rules and no open model is drawn.

### 5. Collapse debris
- **The pieces:**
  - D1–D3: stone block, pair and corner;
  - D4–D5: timber beam and splinters;
  - D6: earth clod.
- **Which pieces go on which rubble:**
  - stone rubble (8) and rock rubble (28) get D1–D3, tinted #7f7c77 for rock;
  - timber rubble (25) gets D4–D5;
  - earth rubble (26) gets D6.
- **Scatter (ADR-006, decision 5):**
  - seed a PRNG with the collapse id;
  - place one piece per ~4 m² of rubble top;
  - give each a 90° yaw;
  - offset each by 0, 0.25 or 0.5 m.

  Debris never blocks a path or a shot. These pieces **replace the spike's own debris.**

## Docs to update in the same change
- **`Design/MvpAssets.md`:**
  - add S3c, S3p, X1, X2, G1, G2 and D1–D6 under Models;
  - mark S3 as released;
  - remove the "other two poses still to draw" note;
  - add X2 as a line model, with the reason above.
- **`Design/ADR/ADR-006-voxel-sizes.md`:** decision 2 gets the arrow as its one exception.
- **`Design/ADR/ADR-015-navigation.md`:** the trebuchet rows, as in §1.
- **`GameData/README.md`:**
  - add a row for `Client/Lines/`;
  - extend `Client/Models/` to "poses, projectiles, open gates, debris".
- **`Design/MvpPlan.md`**, *How units are drawn*: add "arrows are line models, rotated freely".
