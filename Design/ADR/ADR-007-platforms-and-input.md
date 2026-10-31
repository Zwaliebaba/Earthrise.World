# ADR-007 — Windows on x64 and ARM64, with two input schemes over one command layer

**Status:** Accepted, 2026-10-31, by the owner. Like ADR-002 to ADR-006, it is written ahead of the code it governs. The first project implements decisions 1 to 3, the renderer spike implements decision 6, and the interface built in M3 and M4 implements decision 4.

## Context

The owner wants the game on three kinds of device:
- a laptop or desktop played with mouse and keyboard;
- a Surface Pro played by touch;
- Android, later.

These facts bear on that:
- **The owner's target Surface Pro runs a Snapdragon X, which is ARM64.** Until now AGENTS.md allowed x64 only and set `/arch:AVX2` for every project. Windows runs x64 code on ARM64 by emulation, at a cost in CPU time.
- **Windows 11 requires Armv8.1 atomics (LSE) on ARM64** (Windows 11 minimum hardware requirements, processor section). Snapdragon X parts are on Windows 11 24H2's list of supported processors. MSVC targets ARM64 extensions with `/arch:armv8.x`, and features such as LSE with a `+lse` suffix (Microsoft Learn, `/arch` (ARM64)).
- **Floating point is not bit-identical across the two architectures.** The CRT's math functions and DirectXMath's SIMD paths differ between x64 and ARM64. MSVC does not contract `a*b+c` into an FMA under `/fp:precise` unless `/fp:contract` is given, which has been the default since Visual Studio 2022 (Microsoft Learn, `/fp`). So the remaining differences come from libraries, not from contraction.
- **The GPU pass mark is an RTX 3060-class GPU at 1920 × 1080** (GameDesign.md, *Renderer spike*). The Surface's GPU is integrated and shares memory bandwidth with the CPU, and its panel has more pixels than 1080p. How far it falls short has not been measured.
- **ADR-001's method is untested on mobile GPUs.** It shades every voxel quad with a ray-box test, discards the misses and writes depth. Mobile GPU designs bin geometry into tiles and remove hidden surfaces before shading; discarding and writing depth defeat that removal. ADR-001's cost on such a GPU is unknown until it is measured.
- **Touch has no hover, and a fingertip is large.** At the default zoom a fingertip covers several units (RendererLook.md).
- **Android has neither Direct3D 12 nor MSVC.** R12 makes graphics Direct3D 12, and R14 admits only the Windows SDK and the MSVC standard library.

## Decision

1. **Two architectures, both native: Windows x64 and Windows ARM64.**
   - Every project has x64 and ARM64 platforms, each in Debug and Release, from the first project. There is no x86 and no ARM64EC.
   - **Each architecture's instruction-set floor is stated in every project:**
     - `/arch:AVX2` on x64, as before;
     - `/arch:armv8.0+lse` on ARM64, which is the floor Windows 11 itself requires, so it excludes no device that runs Windows 11 on Arm.
   - **The two architectures are aligned as Debug and Release are** (AGENTS.md §3). Every setting that is not about the architecture reads identically on both. Only the platform name, `/arch`, and the output directories MSBuild names after the platform may differ.
   - **Intrinsics:** code that uses one architecture's intrinsics directly (`<immintrin.h>`, `<arm64_neon.h>`) has a path for the other in the same commit. DirectXMath already has both.
2. **A replay is promised within one build, as before ([ADR-002](ADR-002-authoritative-simulation.md)), and an x64 build and an ARM64 build are two builds.** Anything that must agree across architectures is computed in integers or fixed point (R16). The MVP plan already proposes integer voxel coordinates and fixed-point positions for the simulation (MvpPlan.md).
3. **Windows and Direct3D stay inside `Engine` and the executables.**
   - `Core`, `Protocol`, `Content` and `Simulation` build against the C++ standard library alone: no `<windows.h>`, no Direct3D, no COM.
   - No abstract graphics interface is written while Direct3D 12 is the only backend (AGENTS.md R2). The layering is what makes a second backend the replacement of one library rather than a rewrite.
4. **Two input schemes feed one command layer.**
   - **Both schemes are first-class:** mouse and keyboard, and touch. Both produce the same commands, and the simulation never knows which device issued one (ADR-002). Orders address squads (GameDesign.md, 2026-10-31).
   - **Input comes through Windows pointer messages** (`WM_POINTER`, with `EnableMouseInPointer`), which deliver mouse, touch and pen in one stream from the SDK. The active scheme follows the device used last, not a setting, so a Surface with its keyboard cover attached can switch mid-game.
   - **Hover may speed things up, but never be the only way to see something.** Every readout shown on hover, such as the attack cursor's reading or the placement readout, can also be reached without hover. This binds from the first interface (M3), although the touch scheme itself comes later.
   - **Sizes follow the display.** Hit targets and interface sizes are set in logical pixels and scaled in steps by the display's DPI, so they keep roughly the same physical size ([Hud.md](../Hud.md) §2).
5. **The order of delivery:**
   - **The MVP** is played with mouse and keyboard, on both architectures.
   - **The touch scheme** follows a positive MVP verdict.
   - **Android** has no date and is not a target. R12 and R14 stand. An Android port needs its own ADR reopening them, for a Vulkan backend, a toolchain other than MSVC, and HLSL compiled to SPIR-V.
6. **The Surface Pro is measured from M1,** once one is available: the owner has none yet (GameDesign.md, 2026-10-31).
   - The renderer spike runs natively on a Snapdragon X Surface Pro beside the RTX 3060-class machine, and records the voxel pass's cost on both.
   - The Surface is measured at a reduced internal resolution, so the render resolution is separate from the display's from the start.
   - The owner sets the Surface's pass mark from those figures.

## Consequences for the code

- **AGENTS.md changes:** two platforms (§3), the ARM64 floor and the corrected note on contraction (R16), what CI builds (§6), and the checklist (§7).
- **CI builds Debug|x64 only** (amended 2026-10-31, by the owner; CI first cross-compiled Debug|ARM64 beside it). The test suites run on x64 in CI. Debug|ARM64 is built by hand before anyone claims it builds, and the suites are run on the Surface by hand before anyone claims something about ARM64 behaviour. `Build/CheckProjectFiles.py` still holds the two platforms to the same settings in CI, but code for one architecture only, which this ADR forecloses, is caught only by an ARM64 build.
- **`Build/CheckProjectFiles.py`** checks that x64 and ARM64 agree on every setting outside decision 1's list, as it checks Debug against Release.
- **The spike needs two machines:** a Snapdragon X Surface Pro as well as the RTX 3060-class one.
- **The renderer has a render resolution of its own** and scales to the display. AGENTS.md R12 leaves resolution and scaling to be decided when the renderer is built; this settles that the two resolutions are separate, not their values or the scaling method.

## What this forecloses

- x86, ARM64EC, and code for one architecture only, such as an `_mm_` intrinsic with no ARM64 path.
- `/arch:AVX2` as a setting for every project. It is the x64 setting.
- Information that only hover reveals.
- Windows or Direct3D types in `Core`, `Protocol`, `Content` or `Simulation`.
- Android as an MVP target, and Android work before an ADR reopens R12 and R14.

## Left open

- **The Surface's pass mark,** set from M1's figures.
- **The touch scheme's gestures and layout,** designed after the verdict. The HUD package's layout B and the second asset package's landscape phone layout are the starting points (Hud.md §4, RendererLook.md).
- **Whether ADR-001's method holds on mobile GPUs.** The Surface's Adreno gives a first reading; Android would need its own.
- **Pen input,** which can hover: treated as touch, or as a mouse.
