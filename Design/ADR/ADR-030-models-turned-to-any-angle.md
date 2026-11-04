# ADR-030 — Models turned to any angle

**Status:** Proposed, 2026-11-04, with the owner's go-ahead the same day (GameDesign.md, *2026-11-04: the first run of M5's games*, item 1). The orders that carry it out are [MvpPlan-M5.md](../MvpPlan-M5.md), M5-G2 and M5-G3. The commit that first implements it marks it Accepted, with any amendment the code forced (AGENTS.md §6).

## Context

- **Models turn in 90° steps.** [ADR-011](ADR-011-renderer-first-choices.md), decision 6, draws a model through the voxel pass with 0.5 m records and an instance transform turned in quarter turns. [RendererLook.md](../RendererLook.md) §1 says so, and GameDesign.md, *the MVP assets v3 package*, item 2, repeats it: "every voxel model still turns in 90° steps".
- **The owner's first run of the playtest's games showed what that costs** (2026-11-04).
  - **The ram crabs along a diagonal.** Ordered "up" the screen from the default south-west camera, which is north-east in the world, it moves along the diagonal with its length on one axis or the other.
  - **The trebuchet fires sideways.** Deployed, it faces along the camera's view snapped to an axis (`Client::Orders::TurnsAlong`), east from the default camera, and fires at the castle to the north without turning, since the simulation never asks it to.
- **The voxel pass already draws voxels at any angle.** Collapse debris is drawn as `Engine::OrientedVoxel`s, each a box with a quaternion, through oriented pipelines that are lit and shadowed. They have no corner shading, and no silhouette pipeline (RendererLook.md §1; M1's open values, item 7).
- **The simulation needs no change.** A unit's cell, a machine's square footprint, navigation, a ram's blow and a trebuchet's aim are all on the grid, and none reads how a model is drawn (ADR-015; ADR-023).

## Decision

1. **Every unit and engine model is drawn turned to any angle** about the vertical: the figures, the ram and the trebuchet in each of its poses. Structures, gates, the trebuchet's shot and debris stay as they are: structures are world voxels, gates turn with their structure, the shot is a single voxel, and debris has its own orientation already.
2. **They are drawn through the oriented pipelines.** Each voxel of a turned model is an `OrientedVoxel` whose quaternion is the model's yaw, built on the CPU when the model moves or turns, as its records are now (`Client::WorldView`). A model at a whole quarter turn, and every impostor and open gate, keeps the axis-aligned path, which item 8's test shows draws the same.
3. **An oriented voxel carries its baked shading.** `OrientedVoxel`'s two reserved words carry what a `VoxelRecord` carries: the corner levels of its five drawn faces, which hold the contact shading where a model meets the world, sampled at each turned voxel's position, and the corner-shade table's bit, with a bit that says it carries them. The oriented pixel shader shades corners as the axis-aligned one does, in the voxel's own frame, and shades a column not seen now as it does (ADR-025, decision 8), so a model turned to 0° draws as it does today. Debris, which carries no levels, draws as it does today, but for that column shade, which it lacked.
4. **An oriented silhouette pipeline** draws a turned model's silhouette, as `VoxelPass::BeginSilhouette` draws an axis-aligned batch's (ADR-021, decision 6, amended).
5. **Where a model faces.**
   - **A unit or a moving engine** faces its direction of travel over about the last metre it moved, not over its last tick, so that a path of straight and diagonal steps between cells (ADR-015) does not make it wobble by 45°. It turns towards that heading the shorter way at a turn rate. The rates are Proposed, for the owner to judge on screen: 360° a second for a figure, and 90° a second for a machine.
   - **A model standing still** keeps its last heading.
   - **A deployed trebuchet** faces its aim: the bearing of its latest stone from the engine's cell, where the simulation starts the stone's arc in the vertical plane through the engine and its target, from the stone's first sighting in flight. Until its first stone it keeps its deploy facing. It turns to a new aim at once, in the frame its pose changes to released, since by then the stone has left. It is drawn only: the simulation fires in any direction, as it does now (GameDesign.md, *the first run of M5's games*, item 2). Read from the stones, it holds for every trebuchet: the player's, the scripted attacker's under `--watch` and a replay's, for which this client sent no FIRE command.
6. **Impostors stay axis-aligned.** Below 3 px a facing does not show (RendererLook.md §7).
7. **Picking uses the box that bounds the turned model,** axis-aligned, so a click on a turned engine still lands on it (M3-H3).
8. **Tested and measured.**
   - **Render tests on WARP** (ADR-012): an oriented voxel at 0° with a model's levels covers the pixels, at the colour and depth, that the axis-aligned record does; one at 45° covers the pixels its turned box projects to; its silhouette draws where the world hides it.
   - **On the benchmark machine:** M4-J4's measured run keeps the voxel pass within 8 ms at the 95th percentile with every model turned (GameDesign.md, *Renderer spike*).

## Consequences for the code

- **`Engine`:** `OrientedVoxel`'s reserved words are named and filled. The oriented pixel shaders shade corners. An oriented silhouette pipeline joins the pass, and the render tests above join `EngineTests`.
- **`Client`:**
  - `ModelPlacement` carries a yaw in radians in place of its quarter turns;
  - `WorldModels` turns each model towards its heading at its rate, and a deployed trebuchet to the bearing of its latest stone;
  - `WorldView` builds turned models' oriented voxels with their levels;
  - `WorldPicker` bounds the turned box.
- **`Simulation` and `Protocol`:** nothing. The state update already carries each unit's, engine's and stone's position, and each stone's engine, from which the headings and the aim come.

## What this forecloses

- **Models whose voxels are axis-aligned.** RendererLook.md §1's rule that models turn in 90° steps goes, and so does ADR-011, decision 6, for models; world chunks keep it.
- **A facing the simulation decides,** for now. A trebuchet that must turn before it fires, or a figure's facing that bears on combat, would be a rule and a new decision.

## Left open

- **The turn rates,** item 5's, judged on screen, and whether a deployed trebuchet's turn at once to a new aim reads as part of its throw or as a jump.
- **Shimmer.** Edges at angles other than the axes may shimmer as a model turns, which RendererLook.md's acceptance checks look for on silhouettes. The owner judges it on screen; if it shows, the turn may hold each facing a little longer, or snap the last few degrees.
- **Whether the trebuchet must turn in the simulation,** which the owner declined for M5 (item 2): it would change the balance and the scripted trial's pinned results.
