# ADR-020 — The readability views are drawn as translucent voxel boxes through the voxel pass

**Status:** Proposed, 2026-11-01, with the M3 plan ([MvpPlan.md](../MvpPlan.md), M3; [MvpPlan-M3.md](../MvpPlan-M3.md)). It records how the world overlays of [GameDescription.md](../GameDescription.md), *Readability*, reach the screen: the integrity view, the collapse preview and the passability view in M3, and the fog's last-seen land and the silhouettes in M4 if they fit the same path. [RendererLook.md](../RendererLook.md) §8 says only that they are drawn separately and never baked into albedo; [Hud.md](../Hud.md) §1 says they are drawn in the world, not in the HUD, and that the HUD toggles them. The owner gave it the go-ahead on 2026-11-01, as written (MvpPlan.md, M3, input 1; GameDesign.md, *M3's values*, item 1). Order M3-D2 implements the engine's half and marks it Accepted; M3-F1 to M3-F3 draw the three views (AGENTS.md §6).

## Context

- **What the views show** (GameDescription.md, *Readability*; GameDesign.md, *Hypotheses*):
  - **The integrity view,** on by default: for each wall section, how close it is to opening, read from the collapse rule, as three readings: its widest hole against the width at which the stone above falls, its lowest opening against what foot soldiers can climb, and any gap under its foundation; the ground it stands on; its weak point; and the repair lock (M4).
  - **The collapse preview:** where a damaged piece will fall, before it falls, with an alert when it starts to go.
  - **The passability view:** where units can walk, climb and cross, and where a breach has opened a route. H2's "open" is this view's route for foot soldiers through the section.
- **Every reading comes from the simulation** ([ADR-004](ADR-004-rule-based-destruction.md), decision 5; [ADR-015](ADR-015-navigation.md), *Consequences*): the integrity readings and reach through queries, and a collapse's piece, strip and timing through the state update ([ADR-019](ADR-019-the-in-process-client.md)). The client computes none of it; it only draws what it was told.
- **The paper test fixed the integrity readings' form** ([Review/PaperTests/BreachPrediction.md](../Review/PaperTests/BreachPrediction.md), *The readouts*), with their keys and values in Hud.md §10's readout style, and found that the fall width must come from the resolver for each hole, not from a formula (its ambiguity 3). Hud.md §5's single OPEN percentage has no formula (its ambiguity 5).
- **The world is drawn by ray-cast voxel boxes** ([ADR-001](ADR-001-direct-voxel-ray-casting.md); [ADR-011](ADR-011-renderer-first-choices.md), decision 6): a record per visible voxel, bounded on screen by the vertex shader, hit exactly by the pixel shader, which writes depth. Models and debris go through the same pass.
- **What the camera hides** (RendererLook.md §7): at 34° the floors of ditches can be hidden, so a view painted onto the ground alone would vanish exactly where it matters. A view drawn as boxes in the world is seen wherever the world is.
- **The readings need text.** A section's three readings are words and numbers over the section; the interface pass ([ADR-018](ADR-018-text-and-the-interface-pass.md)) draws text on the screen.
- **Nothing may be baked into albedo** (RendererLook.md §8), and a chunk's records are rebuilt only on edits (ADR-011, decision 6), so a view that changed a record every frame would cost a rebuild a frame.

## Decision

1. **A view is a set of translucent axis-aligned boxes, drawn through the voxel pass's own pipeline with a blend state.** `Engine::VoxelPass` gains an overlay pipeline, `BeginOverlay`: ordinary `VoxelRecord` batches, each voxel drawn flat in its palette colour at an alpha the caller gives, blended over the scene, tested against the scene's depth with `GREATER_EQUAL` under reversed Z and writing none. A box placed on a world voxel therefore draws exactly over its visible faces, because the same shader computes the same hit and the same depth for the same box, and a box behind the world is hidden by it. No lighting, no corner shade, no shadow. The overlay pass runs after the opaque scene and the debris, at the scene's render scale, before the upscale.
2. **Overlay colours are palette entries of their own,** in the entries above the materials', beside the faction and ford entries the spike already uses there: Hud.md §2's tokens good, bad, accent, dim and text, converted to linear as every palette colour is. `Client/Look.h` names them. The palette's first 240 entries stay the materials' and are never used for a view.
3. **The client builds a view's records from what it was told, and rebuilds them only when that changes:** a state update that moves a collapse, an answer that changes a reading, a snapshot that changes the ground, a key that toggles the view, or the camera leaving a tile. A view's records are uploaded through the ring each frame, as the models' are, because they are few: a section's voxels, a strip's cells, the cells of the tiles on screen.
4. **What each view draws in M3:**
   - **The integrity view (F1, on by default):** for each structure the client knows, a label in the world through the interface pass, anchored above the section's middle and projected each frame: the section's name, then the paper test's three readings in Hud.md §10's `KEY value` form, HOLE, OPENING and GAP, each with its verdict, and GROUND. The weak point, the reading nearest its mark, has its voxels tinted accent. A section whose collapse is in warning or falling has its label marked FALLS. Hud.md §5's OPEN percentage is not drawn in M3 (MvpPlan.md, M3, input 5).
   - **The collapse preview (with F1):** a piece in warning has its voxels tinted bad, pulsing over the warning so that it reads as about to go, and its strip's cells, where units will be hit, tinted bad on the ground. The alerts W5 WILL FALL IN n S and W5 HAS FALLEN (Hud.md §10) are the HUD's, and they show whether or not the view is on; the debris of the fall is drawn as M1 does.
   - **The passability view (F2, off by default):** for the tiles on screen, every cell foot soldiers can reach from the player's squad tinted good, every cell they could stand in but cannot reach tinted dim, both as a box in the cell they stand in, so that a route shows as a path of boxes through the gap. The alert BREACH OPEN AT W5 (Hud.md §10) fires when reach first crosses into the courtyard. Other classes come with M4's selection.
   - **The attack cursor's strike mark (in FIRE mode):** the voxel the aim check says the shot would strike, tinted accent when IN RANGE and bad when BLOCKED, so that the player sees where a 45° shot lands before firing.
5. **The views are toggled by the HUD** (Hud.md §7): F1 and F2, each a key and nothing else in M3; the HUD model records which views are on, and the lab logs it with each answer.
6. **The silhouettes and the fog's last-seen land are M4's** and are not decided here. If a silhouette is a unit's model drawn where the depth test fails, it is this pipeline with another depth function; if the fog is a tint of the chunks' records, it is not, and gets its own amendment.

## Consequences for the code

- **`Engine::VoxelPass`** gains one pipeline state and `BeginOverlay`, with a render test on WARP ([ADR-012](ADR-012-render-tests-on-warp.md)): an overlay box on an opaque voxel blends to the expected colour at the pixels the voxel covers; a box behind an opaque voxel changes nothing; a box in the open blends over the clear colour.
- **`Client`** gains `Overlays`, which turns the HUD model's toggles, the state update, the answers and the snapshots into overlay batches and the labels' quads, and `Look.h`'s overlay entries.
- **The frame's cost grows by a few hundred boxes,** which the spike's measurements put far under a millisecond; `Earthrise` keeps the spike's GPU timer so that the owner can read the voxel pass's time in its title.

## What this forecloses

- Baking a view into a chunk's records or its albedo.
- A screen-space post-process for a view, in M3.
- A view computed on the client: every reading comes from the simulation through the boundary.
- Overlay colours taken from the materials' palette entries.

## Left open

- The silhouettes and the fog's last-seen land (M4).
- Colour-blind palettes, which the owner deferred (Hud.md §2); every state here is also carried by its label.
- Hud.md §5's single OPEN reading, which needs a formula before M4's selection panel shows one.
- Whether the labels in the world crowd a castle of thirty sections in M4; M3 has ten.
