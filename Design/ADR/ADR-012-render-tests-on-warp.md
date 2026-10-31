# ADR-012 — Render tests on WARP in CI

**Status:** Accepted, 2026-10-31, by the owner. It records how the renderer spike checks its GPU code before anyone looks at a frame. It extends AGENTS.md §3, "A green build says nothing about whether the game draws", without replacing it. It does not change the pass mark or the look's acceptance checks: [ADR-011](ADR-011-renderer-first-choices.md) and [RendererLook.md](../RendererLook.md) still own those.

## Context

- **Nobody watches the frames as they are written.** Claude sessions write the code and cannot see a frame. CI's Windows runners have no GPU, so every visual defect costs a round trip through the owner. [MvpPlan.md](../MvpPlan.md) names this the biggest risk to M1's estimate.
- **Windows ships a software Direct3D 12 adapter, WARP.** It is part of the operating system, so using it adds no dependency (R14). It offers feature level 12_0 and Shader Model 6.0, the floor ADR-011, decision 8 sets.
- **WARP works in CI.** The engine base's first run, [41239932255](https://github.com/Zwaliebaba/Earthrise.World/actions/runs/41239932255), created WARP devices inside vstest on the `windows-latest` runner. These tests passed:
  - a device and its direct queue;
  - both kinds of descriptor heap;
  - two frames in flight;
  - an upload that the GPU copied back byte for byte.
- **Much of a renderer's correctness is a number.** Examples are which pixels a voxel covers, the colour it shades to, the depth it writes, and what stays clear. Those can be checked without anyone looking.

## Decision

1. **The engine's GPU code is tested on WARP in CI.** The test suites create their devices with `GpuDevice::Desc::useWarp`. CI's existing test step runs them on every build, with no step of their own.
2. **Every render pass the spike adds comes with a render test.** Each test draws a small scene it builds itself into an offscreen target, then reads back colour and depth.
   - **The voxel pass (task 5)** comes with a single voxel and a pair that occlude each other.
   - **The look's passes (task 7)** each come with a test, where a property can be stated as a number.
   - **The upscale (task 8)** comes with a test.
3. **Expected values come from the scene, not from golden images.**
   - **What a test asserts:**
     - the pixels a box must cover and must not cover, with a margin of one pixel at its edges;
     - the colour at a pixel's centre, within a tolerance the test states;
     - the depth at a hit, against the depth computed for the camera;
     - the clear colour everywhere else.
   - **Why not golden images:** a golden image needs a reference someone has looked at and accepted, and none exists in M1.
4. **The debug layer validates every test where Windows has it.** In Debug builds the tests ask for it, and an error or corruption report fails the test (`GpuDevice::TakeDebugErrors`). It ships with Windows' Graphics Tools feature, which the owner's machine can install. Whether the CI runners have it is not yet known; where they don't, the tests run unvalidated.
5. **WARP never decides two things:**
   - **Speed.** Every timing, the pass mark and ADR-006's measurements come from the RTX 3060-class machine, as ADR-011, decision 10 says. A time measured on WARP is not reported.
   - **The look.** RendererLook.md's acceptance checks are judged on screen by the owner. A render test can pass while a frame looks wrong, so anything that touches presentation is still run and looked at (AGENTS.md §3).

## Consequences

- **EngineTests holds the render tests.** The voxel pass and every later pass is written so that a test can drive it into an offscreen target without a window.
- **The owner's round trips are kept for the look and the timings.** A defect that shows in coverage, colour or depth should reach CI before it reaches the owner. The estimate's owner hours stay as written until the time log shows the effect.
- **A test's tolerance is part of the test.** WARP rasterises by Direct3D's rules, so coverage at pixel centres matches hardware. Colour can differ in its last bits, so no test asserts exact colour bytes.

## What this forecloses

- Reporting a GPU time measured anywhere but the stated hardware.
- Treating a green render test as evidence that a frame looks right.

## Left open

- Golden images, once the owner has accepted a frame. They would catch regressions that numbers do not describe, and would need a decision on how to store them and how to review a change to one.
- Whether the CI runners have the debug layer, and, if they don't, whether CI should install Graphics Tools.
