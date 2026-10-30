# ADR-001 — The world is drawn by direct voxel ray-casting, not by meshing

**Status:** Accepted, 2026-10-30. Decided by the owner before the design phase. Nothing implements it yet.

**Source:** A. Majercik, C. Crassin, P. Shirley, M. McGuire, *A Ray-Box Intersection Algorithm and Efficient Dynamic Voxel Rendering*, Journal of Computer Graphics Techniques 7(3), 66–81, 2018 — [`Design/Reference/Majercik2018Voxel.pdf`](../Reference/Majercik2018Voxel.pdf), <http://jcgt.org/published/0007/03/04/>. Section and listing numbers below refer to it.

## Context

Earthrise.World is an open-world voxel game on Direct3D 12. Most voxel games draw the world by turning it into triangles first: hidden faces are culled, coplanar faces merged, and a group of voxels is meshed again whenever one of them changes. That makes a static world cheap to draw and a changing one expensive. Every edit also has to wait for a re-mesh before it shows up on screen.

The paper draws voxels directly and needs no precomputation or spatial data structure. The rasterizer only works out which pixels *might* see a voxel, and a small ray tracer in the pixel shader decides exactly what each one sees (§3). Because nothing is precomputed, moving every voxel every frame costs nothing extra to render (§6.3).

The authors' own measurements, at 1920×1080 on a GeForce 1080, through OpenGL and Unreal Engine 4 (§6.3, Table 4):

| Scene | Rasterizing the equivalent static mesh | Direct voxel rendering |
|---|---|---|
| 53 Mvoxel world (Fig. 1), best / worst viewpoint | 75.1 / 80.8 ms | 8.3 / 13.9 ms |
| Science-fiction city (Fig. 9), best / worst viewpoint | 15.2 / 38.5 ms | 2.5 / 6.1 ms |

Over the full depth range they report about 2× near the camera, 20× for distant voxels and 7× overall. Stochastic pruning contributes a further 1.2× of that. **These figures are the paper's, measured on 2018 hardware. They have not been measured in this codebase,** and AGENTS.md §6 forbids quoting them as if they had.

## Decision

The world's voxels are drawn by the paper's method, written in HLSL for Direct3D 12:

1. **Host.** Voxels are grouped into objects with an aggregate bounding box. Objects are frustum-culled on the CPU, and the survivors are submitted as ordinary draws (Listing 2).
2. **Vertex shader: one voxel becomes one screen-space quad.** Direct3D has no point sprites, so the vertex shader moves each of a quad's four vertices to one corner of the voxel's screen-space bounds, chosen from the vertex index (§3). The bounds are computed in one of two ways (§4):
   - A voxel a few pixels across uses the quadric projection of its bounding sphere (Sigg et al. 2006, Listing 4).
   - A large voxel (the paper's threshold is 20×20 pixels) has its twelve box edges clipped against the five frustum planes other than the far plane, and the clipped segments are bounded.

   Voxels behind the near plane are culled conservatively.
3. **Subpixel voxels are pruned stochastically.** A stable hash of the voxel index is compared against the voxel's projected area. Voxels that survive are enlarged in proportion, so coverage is preserved (Cook et al. 2007, Listing 3).
4. **Pixel shader.** It builds the ray through the pixel (or the MSAA sample) and runs the paper's ray-box test (§5, Listing 5). The test is branchless, handles both oriented and axis-aligned boxes, and reads the normal and hit distance straight out of a three-face bitmask. A miss discards the pixel. A hit is shaded, and depth is written at the hit point.
5. **Shadow maps use the same path** (§3).
6. **Voxels go through the depth buffer as an ordinary draw.** Anything drawn as a mesh shares that depth buffer and composites with the voxels without special handling (§6.3).

## Consequences for the code

- **The ray-box intersection is written once in HLSL** and lives in a shared include, `Shader/*.hlsli` (AGENTS.md §2). Every pass that draws voxels includes it.
- **A CPU copy of the ray-box test** (for picking, collision or line of sight, §1 of the paper) is a separate C++ implementation. It will not agree with the GPU to the bit: they use different compilers, and `/arch:AVX2` can contract arithmetic into an FMA (R16). Gameplay must not depend on the CPU and GPU agreeing at a box edge.
- **Depth is written conservatively, not with plain `SV_Depth`.** Plain `SV_Depth` turns off early depth rejection for every pixel the quad covers. The shader uses `SV_DepthGreaterEqual` instead, or `SV_DepthLessEqual` under reversed-Z. That is valid only if the quad is rasterized at or in front of the voxel's nearest possible hit, which is the front of its bounding sphere and not its center.
- **The order of voxels in a render buffer has no meaning.** Stochastic pruning assumes voxels are in random order (Listing 3). Code must not rely on that order, and must not sort the buffer spatially without re-randomizing it.
- **The per-voxel record the vertex shader reads must match its HLSL declaration exactly.** The C++ side `static_assert`s its size and field offsets, which is why `.clang-tidy` leaves the padding check off.
- **None of the paper's code is used.** The paper is licensed CC BY-ND 3.0, which permits redistributing it unchanged (that is why the PDF can sit in `Design/Reference/`) but grants nothing for adapting its listings. The paper ships GLSL listings, a supplement and an Unreal Engine 4 modification. The HLSL here is written from the algorithm as the paper describes it, not transliterated from those listings. Bringing in any supplement code is a licence question for the owner (R14).
- **NVIDIA's `QUAD_FILLMODE` is excluded.** The paper notes it would save a vertex per quad (§3), but it comes through NVAPI, which is a vendor SDK and is excluded by R14.

## What this forecloses

- **Meshing the world.** Face culling and greedy meshing are not how voxels reach the screen. Geometry that is not made of voxels may still be a mesh; it shares the depth buffer (Decision 6).
- **A hierarchy-traversing voxel ray marcher** (sparse voxel octree or brick map, for example GigaVoxels) as the primary renderer.
- **Hardware ray tracing (DXR) as the primary visibility path.** A later ADR can use DXR for secondary effects without reopening this one.

Changing any of these is a new ADR that supersedes this one.

## Left open

These are design questions or later ADRs, not part of this decision:

- **The world's extent, the size of a voxel in world units, and how voxels are grouped into objects** for culling and for editing.
- **Worlds larger than GPU memory: streaming and level of detail.** The paper does not address them. Stochastic pruning thins distant voxels, but they still have to be resident. Cost is linear in both pixel count and voxel count (Fig. 8), so in an open world the view distance is the budget. This is the largest gap between the paper and this game.
- **Axis-aligned or oriented voxels.** The paper supports both, and the axis-aligned path is cheaper. A related question is whether voxels animate one by one.
- **How quads are issued.** The options are an instanced quad, vertices derived from `SV_VertexID`, or mesh shaders (Shader Model 6.5 via `dxc`).
- **Orthographic shadow maps.** A sun is a directional light, and the paper describes the pinhole-perspective case.
- **Depth convention (reversed-Z or not), MSAA, and the shading and material model.** The paper shades one color per voxel.
