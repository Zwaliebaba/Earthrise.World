# Earthrise.World — MVP assets

**Status:** Decided by the owner, 2026-10-31. This is the catalogue of the MVP's voxel assets, which live in [`GameData/`](../GameData/), in the format of [ADR-005](ADR/ADR-005-content-formats.md). Templates and the palette are under `GameData/Server/`, models under `GameData/Client/`, and illustrations under `GameData/Illustrations/` ([ADR-008](ADR/ADR-008-server-and-client-game-data.md)). They come from the owner's second package, kept unchanged in [Reference/SiegePrototypeV2/](Reference/SiegePrototypeV2/). It replaced the first, which is still kept in [Reference/SiegePrototype/](Reference/SiegePrototype/). The runtime states of 2026-11-01, the trebuchet's other two poses, the projectiles, the open gates and the collapse debris, come from the owner's third package, kept unchanged in [Reference/MvpAssetsV3/](Reference/MvpAssetsV3/). [GameDesign.md](GameDesign.md) and [GameDescription.md](GameDescription.md) govern rules and behaviour; this document and [RendererLook.md](RendererLook.md) govern size, colour and shading.

Assets come in two voxel sizes ([ADR-006](ADR/ADR-006-voxel-sizes.md)):
- **World assets** use 1 m voxels, so one voxel is 1 m³. These are the templates, the earthworks and the material samples. Their file names end in `-1m`.
- **Models** use 0.5 m voxels, so one voxel is 0.125 m³. These are units, engines and squads. Their file names end in `-05m`.

Sizes are given as width × height × depth, with depth measured front to back. The world assets are automatic conversions from the first package's 0.5 m drawings, and are to be redrawn natively at 1 m (Known issues).

## Templates

Buildings, used by both the simulation and the client. The costs are counted under ADR-005, with decorations costed as the material they sit in. Stock and foundations are left out.

| Id | Template | Size (m) | Cost | Critical part | Notes |
|---|---|---|---|---|---|
| T1 | Hall | 11 × 7 × 7 | 188 m³ timber | Hall frame (the whole building) | The drop site for everything. The hall is the keep until the keep is complete. The roof costs as timber. |
| T2 | Keep | 12 × 19 × 15 | 1,650 m³ stone, 120 m³ timber | Great tower | A 12 × 12 m great tower with a forebuilding in front. Hollow: 2 m stone walls with a timber floor every 5 m, and a stone roof carried by a 2 × 2 m stone pier up the middle, because stone spans 2 m (GameDesign.md, *2026-10-31: the paper tests' values*). A row of slits on each face, one every 3 m at each storey. |
| T3 | Stone wall, standard | 8 × 7 × 2 | 96 m³ stone | None: a section opens by the breach rule | 5 m of solid wall and 2 m of crenellation above it. That is the same 96 m³ as GameDescription.md's 8 × 2 × 6 m. |
| T4 | Tower | 7 × 12 × 7 | 479 m³ stone | Firing platform | Hollow, with 2 m walls. The platform is the top 3 m, merlons included. A slit on each face at each storey. |
| T5 | Gatehouse | 9 × 12 × 7 | 585 m³ stone + 12 m³ timber | Gate | The gate is 3 m wide and 4 m high. The garrison fires down into the passage. |
| T6 | Palisade | 8 × 4 × 2 | 35 m³ timber | None | |
| T7 | Palisade gate | 8 × 5 × 1 | 32 m³ timber | Gate | Its iron fittings cost as timber. The grid has 1 m of padding in front, which the baker trims. T2, T8 and T9 carry empty padding too, which it trims the same way. |
| T8 | Quarry yard | 7 × 3 × 6 | 41 m³ timber | Shed | The stone drop site. Its stone stack is stock. The shed stands on four corner posts. |
| T9 | Lumber camp | 7 × 3 × 6 | 38 m³ timber | Shed | The timber drop site. Its log stack is stock. The shed stands on four corner posts. |

## Models

Drawn by the client only. Models set no costs and no rules (ADR-005, decision 8). Each unit type is one figure: a body of 1 × 1 × 4 voxels, 0.5 × 0.5 × 2 m, with one cue beside it that tells it apart. Far away, units are drawn as impostors (RendererLook.md).

| Id | Model | Size (voxels) | Size (m) | Cue |
|---|---|---|---|---|
| U1 | Laborer | 2 × 5 × 1 | 1 × 2.5 × 0.5 | Tan body, bare head, pick on the shoulder |
| U2 | Spearman | 2 × 7 × 1 | 1 × 3.5 × 0.5 | Spear tip 1.5 m above the helmet |
| U3 | Archer | 2 × 5 × 1 | 1 × 2.5 × 0.5 | Leather hood, bow tip at the shoulder |
| U4 | Outrider | 1 × 5 × 5 | 0.5 × 2.5 × 2.5 | A horse, 2.5 m tall |
| U5 | Man-at-arms | 2 × 4 × 2 | 1 × 2 × 1 | All iron, with a faction shield in front |
| S2 | Sapper | 3 × 5 × 1 | 1.5 × 2.5 × 0.5 | Iron shoulders, raised pick |
| S1 | Battering ram | 6 × 7 × 15 | 3 × 3.5 × 7.5 | Hide roof, iron-headed log |
| S3 | Trebuchet, released | 23 × 26 × 7 | 11.5 × 13 × 3.5 | 12 m arm, counterweight down and arm up, as just after a throw. One of the three baked poses the MVP switches between (GameDesign.md, *Starting the MVP*, item 8); the other two are S3c and S3p. |
| S3c | Trebuchet, cocked | 26 × 18 × 7 | 13 × 9 × 3.5 | Arm hauled down behind the frame, sling on the ground, counterweight raised. Its frame is drawn on S3's voxels. |
| S3p | Trebuchet, packed | 16 × 7 × 7 | 8 × 3.5 × 3.5 | A four-wheeled wagon: 2 m spoked wheels, the counterweight box at the rear, the arm along the top, a hide hood in front, a draw pole facing +x, faction trim. Marching; it cannot fire. |
| X1 | Trebuchet shot | 2 × 2 × 2 | 1 × 1 × 1 | A cube of raw building stone, drawn at twice the stone's real size so that it reads at about 13 px a metre. A cube needs no turn. |
| X2 | Arrow | none: a line model | 1 long, drawn; 0.8 real | Three segments along its axis: faction fletching, a #c79a6b shaft, an iron head tapering to a point. See *Poses, projectiles, gates and debris*. |
| G1 | Gatehouse gate, raised | 6 × 1 × 1 | 3 × 0.5 × 0.5 | The iron-tipped bottom edge of T5's lifted gate, all that shows while the gate is open. |
| G2 | Palisade gate, open | 8 × 6 × 3 | 4 × 3 × 1.5 | T7's two leaves swung inward through 90°, beside the posts. |
| D1–D3 | Debris: stone block, pair, corner | 1 × 1 × 1; 2 × 1 × 1; 2 × 2 × 1 | up to 1 × 1 × 0.5 | Masonry and stone rubble, over stone and rock rubble; tinted with rock rubble's colour over rock. |
| D4–D5 | Debris: beam, splinters | 1 × 1 × 3; 2 × 2 × 2 | 0.5 × 0.5 × 1.5; 1 × 1 × 1 | Timber and timber rubble, over timber rubble. |
| D6 | Debris: clod | 2 × 1 × 1 | 1 × 0.5 × 0.5 | Earth and earth rubble, over earth rubble. |

The figures are the same as in the first package, which already drew them at 0.5 m. The sizes in voxels are the occupied boxes, which the bake trims to.

### Poses, projectiles, gates and debris

These come from the third package (2026-11-01). Its content, check and bake are in; the client code that uses them is not, and each paragraph says what it waits on.

**The trebuchet's poses** (GameDesign.md, *Starting the MVP*, item 8). The cycle is packed, marching and unable to fire; deploy; cocked; fire; released for about the second of the throw; and cocked again over the reload. The deploy and pack times are M4's. S3 and S3c draw their frame on the same layer-file voxels, so they swap with no offset there, and S3p has its own origin, placed by the package at (+4, 0, +0.5) m from S3's layer-file origin, which puts the wagon's centre on the frame's. The bake trims each grid to its voxels and mirrors x (ADR-005, decision 2; ADR-017). After both, with each model placed by the minimum corner of its footprint as the spike places models, S3p sits at S3's placement itself and S3c at S3's placement moved 5 m along -x; both figures are arithmetic on the files, recorded as `placement` in `Manifest.json`. Nothing switches the poses yet: the simulation's engine has no pose (M2's `SiegeEngine`), and the spike places S3 alone.

**The projectiles.** X1 is drawn at a shot's position, interpolated between ticks on the client; nothing draws it yet. X2, the arrow, is not voxels: a 0.5 m voxel is wider than an arrow is long, so one voxel reads as a block and two as a plank at any zoom, and an arrow tilts as it flies where a voxel model turns in quarter turns. It is the one exception to ADR-006, decision 2: a **line model** in [`GameData/Client/Lines/`](../GameData/Client/Lines/), three segments along its axis, the fletching in the faction colour at 3 px, the shaft #c79a6b at 2 px and the iron head at 3 px tapering to a point, drawn 1 m long against a real 0.8 m, with its width fixed in screen pixels and a 2 px dot once it is shorter than 6 px on screen. It is unlit, casts no shadow, and is depth-tested against the voxel pass. [ADR-018](ADR/ADR-018-line-models-and-the-line-pass.md), Proposed, records the format and the instanced line pass that draws it; the file, its check and its bake are in, and nothing draws it until the owner accepts the ADR. Arrows themselves are M4's ([MvpPlan.md](MvpPlan.md)).

**The open gates.** A gate's voxels stay in the world grid: they are T5's and T7's critical part, so cost, damage and craters are unchanged. The gate's state, open or closed, is a flag on the template instance (M4). While open, navigation lets the owner's units through the gate cells and shots still stop at them; the client skips the instance's gate voxels and draws G1 or G2 instead, G1 at T5-local (3, 3.5, 6) m and G2 at T7-local (2, 0, 0) m, both in the layer files' frames, which `Manifest.json`'s `attach` entries carry over to the trimmed grids. A destroyed gate loses its voxels by the normal rules and draws no open model. T5's gate moves to the inner end of its passage when T5 is redrawn before M4 (GameDesign.md, *the paper tests' values*, item 14), and G1's offset follows it. Nothing of this exists yet: the simulation has no template instances and no gate state.

**The collapse debris** replaces the spike's own pieces, which are single oriented voxels (RendererLook.md §1): over stone rubble (8) and rock rubble (28) D1–D3, tinted #7f7c77 over rock; over timber rubble (25) D4 and D5; over earth rubble (26) D6. The scatter is ADR-006, decision 5's: a PRNG seeded with the collapse's id, one piece per about 4 m² of rubble top on a rubble cell's top face, a quarter turn of yaw each, and an offset of 0, 0.25 or 0.5 m within the cell. Debris never blocks a path or a shot. `Manifest.json` carries the table as `debrisByRubble`; nothing bakes it, and the spike still scatters its own pieces.

**Placeholders** for the renderer spike's test scene (MvpPlan.md, M1 task 9), which `GameData/` lacked. They are drawn to the v5 scene's shapes, and the owner may replace them:

| Id | Model | Size (voxels) | Size (m) | Cue |
|---|---|---|---|---|
| P1 | Mantlet | 5 × 4 × 3 | 2.5 × 2 × 1.5 | A timber screen braced from behind, its front +z |
| P2 | Banner | 7 × 14 × 1 | 3.5 × 7 × 0.5 | A 7 m pole with a 3 m faction flag |

The scene's trees and cottages are not models. They are 1 m world voxels, as ADR-006 puts trees, drawn by the scene script. Its quarry stock is T8's, whose template holds it.
## Illustrations

These are for documentation and tests, and are never baked:
- **Squads** (0.5 m): U2–U5 and S2, in groups of six (outriders in threes). U1 shows six laborers together, though laborers are not in squads (GameDesign.md, 2026-10-31).
- **E1, ditch and rampart** (1 m): the standard ditch, with its spoil as a rampart 2 m high on the side towards the keep.
- **E2, deep foundation** (1 m): a 56 m³ wall on a 48 m³ foundation that reaches bedrock 3 m down, cut away at the front.
- **M1, ground strata** (1 m): grass, earth, common rock, a lens of building stone, and bedrock.
- **M2, rubble keeps its material** (1 m): heaps of stone, timber and earth rubble.

## Decided with the packages

The packages made choices the design documents had not fixed. The owner adopted them with the packages:

- **Proportions:** the hall, keep, gatehouse, tower and drop sites as in the table above.
- **Critical parts:** the hall's frame and each drop site's shed, as well as the gate, the firing platform and the great tower that GameDescription.md already names.
- **Hollow buildings:** towers and the keep are hollow, with 2 m walls and floors, so they cost less than their outline. The first package's walls were 1.5 m, which the 1 m grid cannot draw.
- **The standard ditch** is 5 m wide at the top and 2 m deep, with 1:1 sides. Those are 45°, the slope at which earth stands. On the 1 m grid the sides step: 4 m wide in the top metre and 2 m in the second, which is 6 m² of cross-section, the same as the 1:1 profile. The rampart is built from the spoil on the side towards the keep.
- **Unit cues:** one cue per unit type, as listed under Models.
- **Colours added for the look:** horse #5e4330 and leather (hides and hoods) #6e5a3e, which are render-only; timber rubble #5f4530, earth rubble #7d6044 and rock rubble #7f7c77, provisional, which the simulation now leaves when a collapse falls (GameDesign.md, *M2's values*, item 7).
- **The trebuchet's three poses are three models** (the third package, 2026-11-01): S3 released, S3c cocked and S3p packed, and the existing drawing is the released pose, not the cocked one that ADR-015 had called it.
- **The arrow is not voxels** (the third package): a line model, ADR-006's one exception, recorded in [ADR-018](ADR/ADR-018-line-models-and-the-line-pass.md), Proposed.
- **A gate opens as a flag on its instance,** with its voxels left in the world (the third package; GameDesign.md, *the MVP assets v3 package*).
- **Earth (2) and building stone (4) are marked `both`,** because the clod and the shot draw in them, as opening (20) was marked for S1's wheels.

## Known issues

- **The world assets are automatic conversions.** The package made each 1 m voxel from the majority of 2 × 2 × 2 voxels of the 0.5 m drawing. T1–T7 are to be redrawn natively at 1 m before the M4 Siege castle is final, and T8 and T9 before M6.
- **T8's and T9's sheds had no posts** (fixed 2026-10-31, the owner's decision). The conversion dropped the 0.5 m posts, so T9's roof touched nothing and T8's stood only on its stone stack, which is stock ([ADR-004](ADR/ADR-004-rule-based-destruction.md)). Each now stands on four timber corner posts 2 m tall, and T8's stray post, all that the conversion left of its lifting frame, is joined to the ground by one voxel. The asset check tests for this (ADR-005). Their previews in `Design/Previews/` predate the posts.
- **E1 does not hold its own spoil.** The rampart holds 48 m³ against 32 m³ dug, and the spoil and the rampart must be equal (GameDescription.md). The conversion flattened the ditch to 3 m wide and 1 m deep, with a 1 m slot 2 m deep. Redrawing the ditch at the decided profile digs exactly 48 m³ for its 8 m, which fixes both without touching the rampart.
- **E1's new rampart is topped with grass.** Only the generator places grass (RendererLook.md), so a fresh rampart is bare earth. E1 is an illustration, so this affects nothing that is baked.
- **T2's banner was removed.** The conversion dropped its pole and left two faction voxels floating. Banners are 0.5 m models (ADR-006). T2's preview still shows the banner. The render-fix package's T2 brought the two voxels back, and they were removed again.
- **The first conversion lost the openings** (fixed 2026-11-01, from the render-fix package in [Reference/RenderFixPackage/](Reference/RenderFixPackage/)). A majority vote over 2 × 2 × 2 blocks let a 0.5 m slit lose to the stone around it: the keep kept 4 openings of 54, and the gatehouse none. The package's conversion lets an opening win any block it touches, and T1, T2, T4 and T5 were taken from it. Its asset sheet draws slits on only two faces of the keep and the towers, so the other two faces were given the same pattern (the owner's choice). An opening costs as the stone it sits in (ADR-005), so the package's lower stone figures for T2, T4 and T5 are not used. The previews in `Design/Previews/` predate the openings.
- **T2's roof is stone on a pier** (2026-11-01, the owner's choice). The render-fix package made the keep's floors and roof stone again, but an 8 × 8 m stone roof cannot stand where stone spans 2 m: the support search finds 16 of its voxels unsupported. The floors stay timber, and a 2 × 2 m stone pier up the middle carries a stone roof, with every roof voxel within 2 m of support. The support search finds nothing unsupported in it. T2's preview shows a stone roof and no pier.
- **The package's costs left decorations out.** `Manifest.json` keeps each original figure as `packageCostM3`, next to the recount. T1, T2, T4 and T7 cost slightly more than the package said. T2 has changed more since: at stone's 2 m span its floors became timber and its roof stone on a pier, so it is 1,650 m³ of stone and 120 m³ of timber against the package's 1,718 m³ of stone, and the render-fix package's 1,668 (GameDesign.md, *the paper tests' values*, item 1).
- **The scene prompt builds its walls 1.5 m thick.** T3 is 2 m. Where they differ, the templates win ([MvpPlan.md](MvpPlan.md), M1).
- **The packed trebuchet is wider than its navigation class.** S3p is 3.5 m across its travel, 7 voxels, where the machines class is 3 cells wide, from the ram ([ADR-015](ADR/ADR-015-navigation.md), decision 2; GameDesign.md, *M2's values*, item 10); rounded up to whole cells it needs a clearance of 4. The owner decides whether the wagon narrows to 3 m, the width of the ram and of T5's passage, or the class widens; neither is changed.
- **The poses do not swap in place after the bake.** The third package says S3 and S3c swap with no offset, which holds in the layer files and not after the bake's trim and mirror: S3c's placement is S3's moved 5 m along -x, and S3p's is S3's own (*Poses, projectiles, gates and debris*).
- **The third package's models have no previews.** Nothing reads [Previews/](Previews/); `Tools/ExportVox.py` writes a `.vox` of any of them for MagicaVoxel, and the package's asset sheet, section 06, draws them in a browser.
- **The arrow's widths and dot rule are unseen in the engine.** 2 px, 3 px, 6 px and 2 px come from the package's own renderings; ADR-018 leaves them open until an arrow is on screen.

## Previews

The previews are the package's own renderings, kept in [Previews/](Previews/) rather than beside their sources in `GameData/`; nothing reads them. The third package's models, S3c, S3p, X1, G1, G2 and D1–D6, have none; its asset sheet in [Reference/MvpAssetsV3/](Reference/MvpAssetsV3/) renders them.

![T1 Hall](Previews/earthrise-t1-hall-1m.png)
![T2 Keep](Previews/earthrise-t2-keep-1m.png)
![T3 Stone wall](Previews/earthrise-t3-stone-wall-standard-1m.png)
![T4 Tower](Previews/earthrise-t4-tower-1m.png)
![T5 Gatehouse](Previews/earthrise-t5-gatehouse-1m.png)
![T6 Palisade](Previews/earthrise-t6-palisade-1m.png)
![T7 Palisade gate](Previews/earthrise-t7-palisade-gate-1m.png)
![T8 Quarry yard](Previews/earthrise-t8-quarry-yard-1m.png)
![T9 Lumber camp](Previews/earthrise-t9-lumber-camp-1m.png)
![S1 Battering ram](Previews/earthrise-s1-battering-ram-05m.png)
![S3 Trebuchet](Previews/earthrise-s3-trebuchet-05m.png)
![U1 Laborers](Previews/earthrise-u1-laborers-05m.png)
![U2 Spearmen](Previews/earthrise-u2-spearmen-05m.png)
![U3 Archers](Previews/earthrise-u3-archers-05m.png)
![U4 Outriders](Previews/earthrise-u4-outriders-05m.png)
![U5 Men-at-arms](Previews/earthrise-u5-men-at-arms-05m.png)
![S2 Sappers](Previews/earthrise-s2-sappers-05m.png)
![E1 Ditch and rampart](Previews/earthrise-e1-ditch-rampart-1m.png)
![E2 Deep foundation](Previews/earthrise-e2-deep-foundation-1m.png)
![M1 Ground strata](Previews/earthrise-m1-ground-strata-1m.png)
![M2 Rubble keeps its material](Previews/earthrise-m2-rubble-keeps-its-material-1m.png)
