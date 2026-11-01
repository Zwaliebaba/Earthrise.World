# ADR-017 — The prototype's frame and the world's

**Status:** Accepted, 2026-11-01, by the owner, after the render-fix package's review of the spike. It records how a position, a direction, a turn or a layer file drawn in the owner's prototypes reaches the world. It amends nothing in [ADR-005](ADR-005-content-formats.md)'s file format: the layer files keep their axes and are never edited to suit the engine.

## Context

- **The owner's prototypes are three.js pages made with Claude Design** ([Reference/](../Reference/)). three.js is right-handed. In the v5 scene, +x is east, +y is up and **+z is south**: its annotations put the quarry "under the south wall", at z 85 to 102, beyond the castle's z 45 to 78. Its camera's azimuth is measured from +z towards +x, so its default of −45° stands to the south-west.
- **The engine is left-handed.** DirectXMath's `XMMatrixLookToLH` and `XMMatrixOrthographicLH` draw the world with +x east, +y up and **+z north**, and the camera's azimuth is measured from +z, north, towards +x ([RendererLook.md](../RendererLook.md) §7; `Engine::OrthographicCamera`).
- **M1 converted only the camera.** The spike's azimuth became −135°, the south-west in the engine's frame. The v5 scene's coordinates and the sun's direction, (−90, 75, 55), were copied unchanged. The spike therefore drew v5 mirrored north to south:
  - the quarry under the north wall;
  - the fields to the north-east;
  - the sun in the north-west, leaving every south face that the default camera sees unlit.

  The owner's visual review against v5 (2026-11-01) found the symptoms. The cause was confirmed by rendering the test world's voxels offline with the voxel pass's lighting, which reproduced the owner's screenshot. The same render with the conversion below matches v5's layout.
- **The layer files have the same problem in miniature.** The asset sheet draws a template or model in the same right-handed frame, with its front towards +z and x to the viewer's right. Read unchanged into a left-handed world, every asymmetric shape is its own mirror image. A spear moves to the other hand, a gatehouse's door to the other side, and a trebuchet's counterweight to the other end.

## Decision

1. **The world's frame is left-handed: +x east, +y up, +z north.** Everything in the engine, the simulation and the protocol is in it.
2. **Anything stated in the prototype's frame is converted at one boundary, and only there.**
   - A point's z is negated. Within a square of edge E, a coordinate in meters becomes E − z, and a voxel's index becomes E − 1 − z.
   - A direction's z is negated. The Golden sun, (−90, 75, 55) in the prototype, is (−90, 75, −55) in the world.
   - An azimuth a, measured from the prototype's +z, becomes 180° − a, measured from the world's. The prototype's −45° is the world's −135°.
   - A quarter turn t, clockwise seen from above, becomes (2 − t) mod 4. The mirror reverses a turn's sense and takes a front facing the prototype's +z, south, to the world's −z, south.
3. **The test scene states its layout in the prototype's frame,** with v5's own numbers, so that it can be checked against v5 line by line. `Simulation::TestWorld` converts at its `Site`, which writes the scene into the world, and through `WorldXVoxels`, `WorldZVoxels`, `WorldXMeters`, `WorldZMeters` and `WorldQuarterTurns`. `Spike::SceneScript` converts every model it places through the same functions.
4. **The bake mirrors each layer file in x.** The baked grid keeps the file's +z as its front and its y up, so the shape seen from its front looks as drawn. The files, the `.vox` round trip and the asset check are unchanged, and `ContentTests` checks the mirror on the trebuchet. The server's view of a template is the client's plus its parts ([ADR-016](ADR-016-server-data-format.md)), so it is mirrored the same way, and the two agree voxel for voxel. A template stamped into the world is turned in the world's frame and is not mirrored again with the scene. Together these reproduce a prototype placement exactly: in the prototype the shape is turned t, in the world it is turned 2 − t.

## Consequences

- **The spike draws v5 as v5 draws it.** The quarry lies under the south wall, the fields to the south-east and the forest to the north-west. The sun lights the west and south faces that the default camera sees.
- **Directions in the design documents stay as written.** RendererLook.md's light directions and default azimuth are the prototype's, and they say so. GameDesign.md's "the west curtain just north of the breach" now names the section at the scene's lower z, which is the one that collapses.
- **The seeded world changes.** The test world is v5 mirrored into the world's frame. A mirror moves voxels without adding any, so the scene's edits still change 17,425 voxels (counted before and after with the same seed). The collapse is now the section north of the breach, so `SimulationBench`'s figures for the breach change with it.
- **A new prototype is converted the same way.** A future scene, preset or placement copied from a Claude Design page goes through these functions, or through new ones that sit beside them at the same boundary.

**Foreclosed:**
- editing layer files to suit the engine;
- a right-handed engine. That would mean rewriting the camera, the shadow projection, the turn convention and every render test to save one conversion.
