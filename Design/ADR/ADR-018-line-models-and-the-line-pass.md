# ADR-018 — Line models, and the instanced line pass that draws them

**Status:** Proposed, 2026-11-01. It records the one decision the owner's MVP assets v3 package makes ([Reference/MvpAssetsV3/](../Reference/MvpAssetsV3/)): the arrow is not a voxel model. The owner gave it the go-ahead on 2026-11-01, as written, with M4's plan (MvpPlan.md, M4, input 1; GameDesign.md, *M4's values*, item 1); order M4-G1, the commit that first implements the pass, marks it Accepted (AGENTS.md §6). What is already in the tree ahead of it is content and tooling that nothing draws yet: the arrow's file, its check, its bake and `Content::LineModelView` (decisions 1 and 2). It amends [ADR-006](ADR-006-voxel-sizes.md), decision 2, with its one exception, and adds a pass to the renderer, which AGENTS.md R12 makes an ADR.

## Context

- **ADR-006, decision 2: models use 0.5 m voxels,** turned in 90° steps. The smallest thing the client draws is half a metre on a side.
- **An arrow is about 0.8 m long and 1 cm thick.** One voxel is a half-metre cube, which reads as a block; two are a metre-long brick, which reads as a plank. No voxel arrow reads as an arrow at any zoom, and an arrow tilts as it flies, where a voxel model turns only in quarter turns.
- **Arrows matter to the look.** M4 brings physical shots for arrows and bolts ([MvpPlan.md](../MvpPlan.md), M4), and a volley has to read from the strategy camera, where a metre is about 13 px at the default zoom and about 4 px fully zoomed out (ADR-006).
- **[ADR-001](ADR-001-direct-voxel-ray-casting.md)'s pipeline draws only voxels:** a screen-space quad per voxel and a ray-box test in the pixel shader. A line is not a box, so it needs a pass of its own.
- **The package decides the arrow's look:** three segments along its axis, fletching, shaft and head, with widths in screen pixels; drawn 1 m long against a real 0.8 m; a dot when it is too short on screen; unlit, no shadow, depth-tested against the voxel pass; and an optional tracer if a volley still does not read.
- **The simulation is unchanged.** An arrow is still a physical shot through the voxel grid that stops at the first solid thing it meets (GameDescription.md). Only how the client draws it changes.

## Decision

1. **A line model is the one exception to ADR-006, decision 2.** It is a thing too thin for a 0.5 m voxel, drawn as coloured segments along one axis, with its width fixed in screen pixels, turned freely along its velocity, pitch and yaw included. The arrow, X2, is the first and, for the MVP, the only one. Every voxel model still turns in 90° steps.
2. **Its source is a `.json` in `GameData/Client/Lines/`,** the client's side of [ADR-008](ADR-008-server-and-client-game-data.md): an id, a name, `drawnLengthMetres`, the dot rule, `dotBelowPixels` and `dotWidthPixels`, and `segments` from the tail, each with `fromMetres`, `toMetres`, a `color` that is `#rrggbb` or `faction`, a `widthPixels` of at least 1, and whether it `tapers` to a point. The segments are contiguous from 0 to the drawn length. `Tools/CheckAssets.py` checks it (check 12), `Tools/BakeContent.py` bakes it into `Content` as `LineModelView` and `LineSegmentView` with its lengths in whole millimetres, and `Manifest.json` lists it as a `line model`. Like a model it sets no cost and no rule, and nothing of it reaches the server.
3. **The client draws line models with an instanced line pass, after the voxel pass,** into the same scene target and depth buffer:
   - **One instance per line:** its position in metres, its unit direction, its side's faction colour and its line model. The engine knows lines, colours and pixels, never arrows (R9).
   - **The vertex shader expands each segment to a screen-aligned quad** of the segment's width, in the scene target's pixels, whatever the zoom; a tapering segment narrows to a point at its end. At a render scale below 1 the width scales to the display with the rest of the scene ([ADR-011](ADR-011-renderer-first-choices.md), decision 3).
   - **Shorter on screen than its model's `dotBelowPixels`,** the whole line is drawn as a dot of `dotWidthPixels`, in the shaft's colour.
   - **Depth:** reversed Z, `GREATER_EQUAL`, against the depth the voxel pass wrote (ADR-011, decision 4), so a wall or rubble in front hides it. It writes depth too, so that lines occlude each other.
   - **Unlit and unshadowed:** the colours are drawn as they are, with no Lambert term, corner shade or shadow read, and lines are drawn into neither shadow map.
   - **Root signature in code, shaders at Shader Model 6.0 through `FXCompile`** (ADR-011, decision 7): `Engine/Shader/LineVS.hlsl` and `LinePS.hlsl`.
4. **A render test on WARP comes with it** ([ADR-012](ADR-012-render-tests-on-warp.md)): one arrow drawn across a voxel wall, which checks that its pixels are hidden where the wall is in front and drawn where it is not, and that its shaft is about 2 px wide at two view heights.
5. **A tracer is not drawn until the owner asks for one.** The package offers a 1 px trail, `#f3ede2`, fading from 50% to 0 over the last 0.2 s, as the first thing to try if a volley still does not read. The pass can carry it, and rain and ropes later, without a new decision on how lines are drawn.

## Consequences for the code

- **`Engine` gains `LinePass`,** shaped as `VoxelPass` is: a `Begin` that binds the frame's constants, and a `Draw` of N instances from a GPU address, so that a test can drive it into an offscreen target (ADR-012).
- **`Content` serves line models** beside models and templates, done ahead of this ADR: `Content::LineModels()` and `FindLineModel`.
- **The client, the spike until M3's thin client, builds one instance per arrow in flight** from the simulation's shots, interpolated between ticks as the trebuchet's shot is.
- **Timing:** the pass is timed with the voxel pass's timestamps (ADR-011, decision 10). It does not count toward the pass mark, which is the voxel pass's, but a volley that costs more than noise is reported.

## What this forecloses

- A voxel arrow, and a 0.5 m voxel model that turns freely: free rotation stays the line models' and the debris's (ADR-006, decision 2; ADR-011, amended).
- A second exception to ADR-006, decision 2, without an amendment here naming it.
- Lines loaded or compiled at runtime: a line model is baked content, as a model is.

## Left open

- **Whether the pass writes depth.** It does here, so lines occlude each other; if that shows as sparkle where lines cross, it stops writing and sorts instead.
- **The dot rule's values,** 6 px and 2 px, and the widths, 2 and 3 px, which come from the package's own renderings and which nobody has seen in the engine. They are settled when an arrow is on screen (RendererLook.md's acceptance checks).
- **Anti-aliasing for 1 and 2 px lines.** ADR-011 has no MSAA; if lines crawl, a coverage falloff in the line pixel shader is the first thing to try.
- **Where on the arrow the shot's position sits:** the tail, the head or the middle. The simulation gives a point; the package does not say which.
- **The tracer** (decision 5).
