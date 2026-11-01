# Task: import MVP assets v3

Read `README.md` first; it explains every change and the reasons for them. Work on a branch, one commit per step, and get CI green on each.

1. **Models.**
   - Copy `GameData/Client/Models/*.json` into the repo's `GameData/Client/Models/`.
   - Add `"pose": "released"` to the existing `earthrise-s3-trebuchet-05m.json`.
   - Merge `GameData/Manifest-additions.json` into `GameData/Manifest.json`, using the existing entries' field style and recounting the voxel counts.
   - Run `Tools/CheckAssets.py`.
2. **Line models.**
   - Add `GameData/Client/Lines/` with `earthrise-x2-arrow.json`.
   - Teach `Tools/BakeContent.py` to bake line models into `Content`: parts, colours, widths and drawn length.
   - Teach `Tools/CheckAssets.py` to validate them:
     - the parts are contiguous from 0 to `drawnLengthMetres`;
     - every colour is a hex or the faction index;
     - every width is at least 1 px.
3. **Line pass in `Engine`.** After the voxel pass, add an instanced pass that draws screen-aligned quads:
   - one instance per arrow (position, unit direction, faction);
   - the width in pixels;
   - the same reversed-Z depth;
   - no lighting or shadows.

   Add a WARP render test (ADR-012) that draws one arrow across a voxel wall and checks:
   - it is hidden where the wall is in front;
   - it is about 2 px wide whatever the zoom.
4. **Trebuchet poses** in the client:
   - map the trebuchet entity's pose (packed, cocked or released) to S3p, S3c or S3;
   - S3p goes at the README's (+4, 0, +0.5) m offset.

   In the spike scene, show one trebuchet of each pose.
5. **Projectiles.**
   - Draw X1 at each trebuchet shot's interpolated position.
   - Draw X2 for each arrow, rotated freely along its velocity, as a 2 px dot below 6 px long.
6. **Gates.**
   - Add the `open` flag to template instances.
   - Navigation: the owner's units may pass the gate cells while it is `open`.
   - Client: while `open`, hide the gate voxels and draw G1 or G2 at the README's offsets.
7. **Debris.** Replace the spike's debris with D1–D6, using the README's rubble table and scatter rule.
8. **Docs.** Make the updates listed in the README under *Docs to update*. Copy `Reference/` to `Design/Reference/MvpAssetsV3/`, unchanged.

**Done when:**
- CI is green;
- the spike shows all three trebuchet poses, a shot and an arrow volley in flight, an open gate and debris over the breach;
- a screenshot of each is attached to the PR for the owner's review.
