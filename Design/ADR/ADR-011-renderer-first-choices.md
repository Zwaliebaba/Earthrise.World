# ADR-011 — The renderer's first choices

**Status:** Accepted, 2026-10-31, by the owner. It settles the choices that AGENTS.md R12, [ADR-001](ADR-001-direct-voxel-ray-casting.md) and [ADR-007](ADR-007-platforms-and-input.md) leave to the time the renderer is built, so that M1 can start. Each choice is a starting point that the M1 spike measures. A change the spike calls for is an amendment here, not a quiet edit in code. The project layout is [ADR-010](ADR-010-solution-and-build-layout.md). Under the orthographic camera, decisions 4 and 6 replace ADR-001's sphere and edge-clipping bounds and its sphere-front depth with the box's eight projected corners and its nearest corner. Amended 2026-11-01: collapse debris is drawn by a second, oriented pass (`Engine/OrientedVoxel.h`, `Engine/Shader/OrientedVoxelVS.hlsl` and `OrientedVoxelPS.hlsl`), whose voxels turn freely (GameDesign.md, M1's open values, item 7).

## Context

- **ADR-001 fixes the method:** a screen-space quad per voxel, a ray-box test in the pixel shader, and depth written conservatively. It leaves these open:
  - how quads are issued;
  - the shadow maps;
  - the depth convention;
  - MSAA.
- **R12 leaves open:** passes, render targets, resolution, scaling, the window style, multisampling and text.
- **ADR-007 makes the render resolution separate from the display's.** Interface sizes follow the display's DPI.
- **R14 forbids** the Agility SDK, `d3dx12.h` and vendor libraries. So the features the Direct3D 12 runtime shipped in Windows offers are the features there are.
- **The pass mark** is the 95th percentile of the voxel pass's GPU time, shadow passes included, on the benchmark GPU at 1080p: a desktop RTX 3060 when this was written, the owner's RTX 3070 Ti Laptop GPU from 2026-11-01 (GameDesign.md, *Renderer spike* and *Closing M0 and M1*).
- **The camera is orthographic,** so depth is linear and far less precision-starved than under perspective.

## Decision

1. **Window and display:**
   - a Win32 window, resizable, and borderless at full screen; no exclusive full screen;
   - per-monitor DPI awareness, version 2, declared in the executable's manifest.
2. **Swap chain:**
   - flip model with `DXGI_SWAP_EFFECT_FLIP_DISCARD`, three back buffers, and two frames in flight with fences;
   - a frame-latency waitable object, with the maximum latency set to two;
   - `R8G8B8A8_UNORM` buffers drawn through `R8G8B8A8_UNORM_SRGB` render-target views, because a flip-model swap chain cannot itself be sRGB;
   - tearing allowed when the system offers it and vsync is off, for variable refresh rate.
3. **Resolution:**
   - The scene draws into its own target at a render scale: 1.0 on the benchmark GPU, and lower where measured necessary, such as on the Surface.
   - The scene is then scaled to the back buffer with a bilinear filter.
   - The interface draws afterwards at the display's resolution, so text stays on whole pixels ([Hud.md](../Hud.md) §2).
4. **Depth:**
   - `D32_FLOAT` with reversed Z: cleared to 0 with a `GREATER_EQUAL` test.
   - The voxel pixel shader writes `SV_DepthLessEqual`. Its quad sits at the depth of the box's nearest corner, and the ray's hit can only lie at or behind it, so early depth rejection stays valid.
   - Reversed Z buys little under an orthographic camera. It is chosen so that the convention needs no change if a perspective close-up is ever added.
5. **No MSAA in M1.** A pixel shader that writes depth makes MSAA run per sample, and the look's checks judge edges without it. If the look calls for anti-aliasing, a post pass is the next step, decided by amendment.
6. **How quads are issued:**
   - **Records:** each chunk keeps a structured buffer of its visible voxels, those with at least one exposed face. Each record carries the voxel's coordinate in the chunk, its palette index, its baked corner shading and its jitter.
   - **Drawing:** one instanced draw per visible chunk, four vertices as a strip. The vertex shader fetches the record by `SV_InstanceID` and builds the screen-space rectangle that bounds the box's eight projected corners.
   - **Models** use the same path, with 0.5 m records and an instance transform rotated in 90° steps.
   - **Culling:** in M1 chunks are culled on the CPU against the view.
   - **Alternatives:** mesh shaders (Shader Model 6.5) and indirect draws are measured only if this path is the bottleneck.
7. **Shaders:**
   - HLSL compiled by dxc through `FXCompile` at Shader Model 6.0, into headers (AGENTS.md §2).
   - Debug builds compile without optimisation and with debug information; Release builds compile optimised.
   - Root signatures are written in code.
   - A higher shader model needs an amendment that names the feature it buys.
8. **Device:**
   - feature level 12_0 at minimum, and Shader Model 6.0 checked at start-up, with a clear message when either is missing;
   - one direct queue in M1, with uploads through the upload ring on that queue; a copy queue comes only if measurement asks for one;
   - the debug layer in Debug builds.
9. **Shadows** follow RendererLook.md:
   - **The static map** is a 4096² orthographic sun map, updated on edits.
   - **The dynamic map** is a 2048² map for moving things, drawn each frame.
   - Both use the same voxel path with a depth-only pixel shader. The sizes are first values, which the spike measures against check 11.
10. **Measurement is built in:**
    - timestamp queries around the voxel pass and each shadow pass, read back every frame;
    - a scripted camera path at the default zoom through all four rotations;
    - a CSV the spike reduces to the 95th percentile.

    That is how the pass mark is measured, and it is the input to ADR-006's table.

    **Amended 2026-11-01** with the owner's three details (GameDesign.md, *Starting the MVP*, item 3), as the spike's `--measure` run builds them:
    - **The path** is `Engine::CameraPath`: a 48 m square about the scene, walked clockwise seen from above with a quarter turn clockwise at each corner, four legs of 900 frames, stepped at 1/60 s. The first 60 frames are left out.
    - **The run presents without waiting for the vertical blank,** with tearing where DXGI offers it. Its scene draws at 1920 × 1080 on any display, and the upscale scales it to the display. A run whose scene draws at any other size, such as with `--render-scale`, says so and is not a pass-mark run.
    - **The spike starts borderless over the whole monitor,** as decision 1 provides, and Alt+Enter switches between that and a window. `--windowed` starts it in a window.
    - **Timestamps** come from `Engine::GpuTimer`: a pair of queries per span and per frame slot, read back once the slot's fence has passed. The 95th percentile is the nearest rank (`Core::NearestRankPercentile`), so it is always a measured frame.
    - **Edit-to-present latency:** an edit counts as shown once DXGI's `PresentCount` reaches the present that first carried it. It is reported two ways: in display refreshes, from the vertical blank before the publication (`SyncRefreshCount`) to the one that showed it (`PresentRefreshCount`), and in milliseconds to when the spike saw it, which is within one frame. Until task 9's quarrying, the run digs out and puts back one ground voxel each second, so that there is an edit to time.

## Consequences

- **M1 builds exactly these;** the spike's measurements decide what changes.
- **The engine's interfaces carry no game concept** (R9). Records hold palette indices, coordinates and shading, never units or templates.
- **Text** is drawn by the interface pass at display resolution. Its baked form is settled with the text ADR in M3.

## What this forecloses, until an amendment

- Exclusive full screen, MSAA, and a shader model above 6.0.
- Shaders compiled at runtime.
- Any Direct3D 12 feature beyond what Windows 11's built-in runtime offers.
- A depth convention other than reversed Z.

## Left open

- The render scale on the Surface, set from its measurement once one is available.
- How the shadow maps are filtered, and their bias, tuned against check 11.
- An anti-aliasing pass, if the look needs one.
- Streaming and GPU-driven culling, beyond the MVP's map (ADR-001).
