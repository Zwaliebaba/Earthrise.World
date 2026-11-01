# AGENTS.md — Engineering Rules

Operating instructions for every agent (and human) writing code in this repository. **Read this before generating a single line.**

This repository is a greenfield C++23 game and a hobby project with one developer: **Earthrise.World, an open-world voxel game**, built on Direct3D 12 on Windows with MSVC. The world is drawn by direct voxel ray-casting after Majercik et al. 2018. Each voxel is rasterized as a screen-space bounding quad, and a ray-box test in the pixel shader finds the exact hit; nothing is meshed ([ADR-001](Design/ADR/ADR-001-direct-voxel-ray-casting.md)). This file is about **how code is written here** — naming, layout, build settings and the standing rules of the codebase. It is not the design: what the game *is* belongs in [`Design/GameDesign.md`](Design/GameDesign.md) and [`Design/GameDescription.md`](Design/GameDescription.md).

**The tree is young.** This repository holds this file, the root configuration files, `.gitignore`, `.github/`, `.claude/` (an agent command that reviews the design), `GameData/` (the MVP's content sources, split between server and client, [ADR-008](Design/ADR/ADR-008-server-and-client-game-data.md)), `Build/` (the property sheet every project imports, the checkers and their fixtures, [ADR-010](Design/ADR/ADR-010-solution-and-build-layout.md)), `Tools/` (Python scripts that bake content, check it, and move voxel files to and from MagicaVoxel, and the paper tests' scripts in `Tools/PaperTests/`), `Earthrise.slnx` with the twelve projects of ADR-010, each in its own folder at the root, and `Design/` (the game design, the renderer's look, the HUD, the MVP plan, its work orders and its assets, the ADRs, the design reviews and the paper tests' reports, and the papers and prototypes the decisions cite) . The C++ began with M1's scaffold. Nothing below is a target to migrate towards; it describes the code as it must be written from the first line. There is no legacy here and nothing is grandfathered, so a whole-tree run of any checker comes back clean, by conformance from the first line.

**Where these rules come from.** They are carried over from two sibling repositories: `Outpost.Warzone`, where the formatter and linter settings were measured against roughly 223,000 lines, and `Nomad-Commander`. That lineage is why `.clang-format` and `.clang-tidy` are what they are, and it is why code can move between the trees without a rename or a reflow pass. **What did not come across is the other trees' design, their decisions or their plan.** A decision taken there binds nothing here.

**What is authoritative, in order:**

1. **This file** — conformance: naming, style, build settings, and how to work here.
2. **`Design/ADR/`** — engineering decisions taken while building, one file per decision (§6). There are seventeen so far: `ADR-001` on how voxels are drawn, `ADR-002` on the authoritative simulation, `ADR-003` on the world as a seed plus its edits, `ADR-004` on rule-based destruction, `ADR-005` on the format of voxel content, `ADR-006` on voxel sizes (Proposed until after M3, by its own terms), `ADR-007` on platforms and input, `ADR-008` on server and client game data, `ADR-009` on queries across the server boundary, `ADR-010` on the solution and build layout, `ADR-011` on the renderer's first choices, `ADR-012` on render tests on WARP in CI, four still Proposed for M2: `ADR-013` on the tick, its numbers and its randomness, `ADR-014` on edits, saves and replays, `ADR-015` on navigation, and `ADR-016` on the `ServerData` file; and `ADR-017` on the prototype's frame and the world's. A Proposed ADR binds nothing until the owner accepts it, except where GameDesign.md has already decided what it records: ADR-006's 1 m world and 0.5 m models bind as the owner's Decided baseline. Numbering started at `ADR-001` in this repository and does not continue another's, so the next is `ADR-018`.
3. **The surrounding code** — for anything neither of the above covers, match the file you are editing.

The design sits alongside rather than above: [`Design/GameDesign.md`](Design/GameDesign.md) and [`Design/GameDescription.md`](Design/GameDescription.md) say what is built, and this file says how. Where the two differ, GameDesign.md governs, and it marks each item Decided, Proposed or Open. A task that needs a design answer they do not give, or that rests on something only Proposed or Open, asks the owner and gets the answer written down before the code is.

**`Design/Reference/` holds the papers and prototypes that decisions cite.** A reference is evidence, not authority: it binds only through the ADR that cites it, and only as far as that ADR says. Where the paper and an ADR differ, the ADR wins.

If a rule here conflicts with a habit from another codebase, this file wins. If you think a rule is wrong or your task cannot be done without deviating, **say so in your report — never deviate silently.**

---

## 1. Naming convention (normative — no exceptions)

| Kind | Convention | Example |
|---|---|---|
| Type (class, struct, enum, concept, alias) | `PascalCase` | `SwapChainTarget` |
| Function, method | `PascalCase` | `PresentFrame()` |
| Member variable | `m_camelCase` | `m_deviceRemoved` |
| Static member (mutable) | `sm_camelCase` | `sm_activeDevice` |
| Global | `g_camelCase` | `g_instance`, `g_frameCount` |
| Parameter | `_camelCase` | `_fileName`, `_entityId` |
| Local | `camelCase` | `shadedColor` |
| Compile-time constant | `UPPER_CASE` | `WIDTH_PIXELS`, `CELL_PIXELS` |
| Enumerator | `PascalCase` | `DeviceLost`, `OutOfVideoMemory` |
| Macro | `UPPER_CASE` | `ENGINE_ASSERT` |
| Namespace | `PascalCase` | `Engine` |
| File | `PascalCase.cpp` / `.h` | `SwapChainTarget.cpp` |

**Note the split that catches people out: a `constexpr` is `UPPER_CASE`, an enumerator is `PascalCase`.** They are both compile-time and they are spelled differently on purpose — an enumerator is a *value of a type* and reads as one at the use site (`PageFault::OutOfVideoMemory`), while a constant is a number with a name and is meant to look like one. [`.clang-tidy`](.clang-tidy) enforces both, and it is the single source of truth for the option values; this document states the rules in prose and does not repeat the settings, so there is nothing to drift.

### The rules behind the table

**R1 — The leading underscore on parameters is deliberate.** It is legal C++: the reserved forms are `_Uppercase`, anything containing `__`, and `_lowercase` **at global scope**. A parameter is never at global scope, so `_fileName` is safe. Never introduce a reserved form — no `_Impl`, no `__helper`, no file-scope `_cache` (use `g_cache` in an anonymous namespace).

**R2 — A type name carries no prefix or affix, and that includes abstract ones.** An interface is `Transport`, not `ITransport`. A base class is not `BaseTransport` or `AbstractTransport`. PascalCase means the name and nothing else. This bans `CFoo`, `SFoo`, `EFoo`, `IFoo`, `FooBase`, `FooAbstract`, `FooImpl` and `_t` suffixes. Name the concept and let the concrete types say what they are:

```
Transport             ← the concept
├── UdpTransport      ← a socket-backed one
└── LoopbackTransport ← in-process, for tests
```

That tree is an illustration of the rule, not a description of anything. A base class for one derived class is ceremony: name the concept, and add the layer when a second thing needs it.

clang-tidy can require an *absent* prefix but cannot see a *present* suffix, so the repository checker carries the other half (§6).

**R3 — Compile-time constants are `UPPER_CASE`.** `constexpr`, `inline constexpr` and `static constexpr` members: `WIDTH_PIXELS`, `TICKS_PER_SECOND`, `CELL_PIXELS`. `sm_` is reserved for *mutable* statics, which are rare and must document their thread-safety.

**R4 — Acronyms capitalize as words**: `HlslSource`, `DxgiFactory`, `UdpTransport` — never `HLSLSource`. Identifiers from an external SDK keep that SDK's spelling (`ID3D12Device`, `DXGI_FORMAT`, `HRESULT`, `IDXGISwapChain4`) and are never renamed to fit.

**R5 — Template parameters are PascalCase**: `T`, `Fn`, `BlockBytes`, `Ts...`.

**R6 — Units belong in names; types do not.** `durationTicks`, `speedUnitsPerSecond`, `radiusMeters`, `volumePercent` are encouraged — a game measured in ticks, seconds, pixels and distances makes unit ambiguity a real defect class, and it is one the compiler cannot catch for you. A voxel world adds its own units: a voxel and a world-space distance are different units, and a position counted in voxels is not a position in meters. Name which one it is: `positionVoxels`, `edgeMeters`, `projectedAreaPixels`. Never encode the type: no `iCount`, `pEntity`, `strName`.

**R7 — A file is named for its primary type**, PascalCase, `.h` / `.cpp` only. `.hpp`, `.cc` and `.inl` are not used; template implementations live in the header. Exceptions, because MSBuild and the Visual Studio wizards spell them this way: `pch.h`, `pch.cpp`, `framework.h`, `targetver.h`, `Resource.h`.

**R8 — `m_` marks encapsulated state, not every field.** A `class` with invariants prefixes private members `m_`. A public aggregate — a `Desc` config struct, a wire record, a POD handed to the renderer — uses plain `camelCase` fields so brace initialization reads naturally.

**R9 — One namespace per layer, and the engine does not know the game.** Reusable engine code gets its own namespace; game code gets another. The split is a rule rather than a filing preference: if an engine type has to know a game concept by name in order to do its job, it is in the wrong layer. Test suites use `namespace <Project>Tests`.

**R10 — No `using namespace` at file scope in a header.** It leaks into every translation unit that includes it, and the failure it causes appears somewhere else. In a `.cpp` it is allowed for the unit-test framework and nothing else; otherwise qualify the name or write a local alias.

**R11 — One spelling per family, and it is the SDK's.** `color`, `initialize`, `serialize`, `normalize`, `quantize`, `synchronize`, `behavior`, `neighbor`, `center`, `gray`, `canceled`. Neither spelling is wrong English; the defect is a tree where a reader has to know which half they are in and a grep for one finds half the uses. `D3D12_CLEAR_VALUE::Color` settles which half wins. Prose is not checked — a design document may spell `flavour` and `harbour`; an identifier spells `flavor` and `harbor`.

### Worked example — this is the target style

```cpp
// Engine/SceneTarget.h
#pragma once

#include <cstdint>

namespace Engine
{

// R3: constant → UPPER_CASE. R6: the unit is in the name.
inline constexpr std::uint32_t SCREEN_WIDTH_PIXELS = 1920;
inline constexpr std::uint32_t SCREEN_HEIGHT_PIXELS = 1080;

// Enumerator → PascalCase, unlike the constants above.
enum class TargetFault : std::uint8_t
{
  DeviceRemoved,
  BadFormat,
  OutOfVideoMemory
};

/// The colour framebuffer the game draws into, and the depth buffer that goes with it.
/// R2: no prefix on the type. R8: private state carries m_.
class SceneTarget
{
public:
  struct Desc // R8: aggregate → plain fields
  {
    std::uint32_t widthPixels; // R6: unit in the name
    std::uint32_t heightPixels;
    DXGI_FORMAT colorFormat; // R4: SDK spelling kept as-is
  };

  /// Throws winrt::hresult_error when the device refuses, so it is not noexcept (R12).
  [[nodiscard]] static SceneTarget Create(const winrt::com_ptr<ID3D12Device>& _device, // R1: _ on parameters
                                          const Desc& _desc);

  [[nodiscard]] std::uint32_t WidthPixels() const noexcept
  {
    return m_widthPixels;
  }

private:
  winrt::com_ptr<ID3D12Resource> m_depthTarget; // R12: RAII, never a raw AddRef/Release
  std::uint32_t m_widthPixels = 0;
  bool m_deviceRemoved = false;
};

} // namespace Engine
```

```cpp
// Engine/SceneTarget.cpp
#include "pch.h"

#include "SceneTarget.h"

namespace Engine
{

SceneTarget SceneTarget::Create(const winrt::com_ptr<ID3D12Device>& _device, const Desc& _desc)
{
  // Written by hand: no d3dx12.h helpers (R14).
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC depthDesc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                      .Width = _desc.widthPixels,
                                      .Height = _desc.heightPixels,
                                      .DepthOrArraySize = 1,
                                      .MipLevels = 1,
                                      .Format = DXGI_FORMAT_D32_FLOAT,
                                      .SampleDesc = {.Count = 1},
                                      .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL};

  SceneTarget target;

  // A failed HRESULT throws; put() hands the COM out-parameter to the smart pointer. The example
  // passes no optimized clear value, to stay short.
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                        nullptr, IID_PPV_ARGS(target.m_depthTarget.put())));

  target.m_widthPixels = _desc.widthPixels;
  return target;
}

} // namespace Engine
```

### Enforcement

| Rule | Enforced by |
|---|---|
| The naming table, R1, R3, R5, R8 | [`.clang-tidy`](.clang-tidy), gated in CI over the whole tree |
| R2 affixes, R7 file names and project registration, R9's root namespace (each project's `RootNamespace` is its name), R11 spellings, §2 flat directories, shader names and functional filters | `Build/CheckProjectFiles.py`, gated in CI |
| R4, R6, the rest of R9, R10 | Review. Check your own diff against the table before handing it back. |

**Both checkers exist** (M0). `.clang-tidy` gates through `Build/RunClangTidy.py` the moment there is a translation unit to run it over, and `Build/CheckProjectFiles.py` gates the rules in its row today. `Build/TestCheckers.py` proves they are not vacuous: a correct fixture in `Build/Fixtures/Good` passes, and each of its broken variants fails for its own reason (ADR-010). For `.clang-tidy` that test needs clang-tidy and the MSVC headers, so it runs in CI's Windows job: the good fixture passes the repository's configuration, and a parameter without its `_` fails it. A rule nobody can run is a rule that rots, so a new rule in this file comes with its check and a broken fixture that shows the check fires.

---

## 2. Repository shape

The concrete layout — the solution, the projects and the edges between them — is settled by [ADR-010](Design/ADR/ADR-010-solution-and-build-layout.md): `Earthrise.slnx` at the root, one flat folder per project, output in `$(SolutionDir)$(Platform)\$(Configuration)\`, and every mandated setting in `Build/Earthrise.props`, which every project imports and none may override. These are the standing constraints that layout satisfies.

**Project directories are flat, with exactly two sanctioned subdirectories.** C++ source — headers and `.cpp` alike — lives directly in its project's folder. **There is no `src/`, no `include/`**, and no other split of a project by file kind. This is not taste: `.clang-tidy`'s `HeaderFilterRegex` matches headers exactly one level in, so **a header in a subdirectory is silently unchecked** — no findings, no warning, and nobody notices for months. The two exceptions are the shader pipeline:

- **`<Lib>/Shader/`** holds the HLSL, hand-written, named for the shader and its stage: `<Shader>VS.hlsl` for a vertex shader and `<Shader>PS.hlsl` for a pixel shader. HLSL shared between shaders goes in `<Name>.hlsli` in the same folder. The ray-box intersection every voxel pass uses (ADR-001) is the first example. A `.hlsli` is included, never compiled on its own. It is registered in the `.vcxproj` as a `None` item, not an `FXCompile` one, so no `CompiledShader/` header is generated for it.
- **`<Lib>/CompiledShader/`** holds what the compiler wrote: one header per `.hlsl`, `<Shader>VS.h` and `<Shader>PS.h`, each declaring a byte array `g_<Shader>VS` / `g_<Shader>PS`. It is **build output** — produced by an `FXCompile` item in the `.vcxproj` on every build, listed in `.gitignore`, skipped by every checker, and never edited or committed. The `.cpp` that binds the pipeline state includes it and nothing else does.

**Shaders are compiled into the executable.** The bytecode reaches the GPU from those generated byte arrays, and nothing else: no `.cso` beside the `.exe`, no shader loaded from disk at runtime, no `D3DCompile` and no `d3dcompiler_47.dll` dependency. A shader change is a rebuild.

**The edges run one way, and a layer never reaches sideways.** Engine code is built on by game code and never the reverse (R9), and two libraries at the same level share what is below them rather than each other. An edge that only exists "for now" is an edge, and it is the one that will be impossible to remove later.

**The project files are part of the source.** Adding, removing or moving a file means editing the owning `.vcxproj` **and** its `.filters`. A file that compiles locally but is missing from the project fails only in CI — or worse, links a stale object nobody notices.

**Filters are functional.** A `.filters` file groups a project by what the code *does* — `Rendering`, `Audio`, `Input`, `Shader` — never by what kind of file it is. The Visual Studio defaults `Source Files`, `Header Files` and `Resource Files` are deleted when a project is created and never come back, and a `.h` sits in the same filter as its `.cpp`.

**There are no vendored SDKs and no package manager.** The build depends on the Windows SDK, the MSVC toolset and standard library, and Python for the build-time tools under `Build/` and `Tools/`, and on nothing else. See R14.

**`Design/` holds no code.** Its top level holds the game design, the renderer's look, the HUD, and the MVP plan, its work orders and its assets, `Design/Previews/` holds the `.png` renderings of the voxel assets and the font, which nothing reads, `Design/ADR/` holds the decisions (§6), `Design/Review/` holds design reviews, and `Design/Reference/` holds the papers and prototypes the decisions cite, each committed unchanged with any licence notice intact.

**`GameData/` holds the game's content sources, outside every project directory** ([ADR-005](Design/ADR/ADR-005-content-formats.md), [ADR-008](Design/ADR/ADR-008-server-and-client-game-data.md)). `GameData/Server/` holds what the simulation decides by, the templates and the palette; `GameData/Client/` holds what only the client reads, the models and the font; `GameData/Illustrations/` holds what neither reads. Voxel sources are `.json` layer files, the only committed copy, and their `.png` previews live in `Design/Previews/`, not here; `Tools/ExportVox.py` writes a `.vox` for MagicaVoxel, which is never committed, and `Tools/ImportVox.py` writes an edit back. **The client has everything compiled in**: the bake writes its tables into the `Content` library, as shaders are compiled in. **The server reads its data from files** the bake writes to `<outputdir>\ServerData`, never from the sources. `<outputdir>\GameData` is kept for client content too large to compile in, audio first. The format, project-file and clang-tidy checkers skip `GameData/`.

**Build and IDE output is never committed** — `x64/`, `ARM64/`, `.vs/`, `*.user`, and anything a build step generates.

---

## 3. Build and verify

**x64 and ARM64 are the platforms, both native** ([ADR-007](Design/ADR/ADR-007-platforms-and-input.md)). Every project has both, from the first project. No Win32/x86 and no ARM64EC configuration in any project or solution; do not add one, and do not write code that only works at 32 bits. Code that uses one architecture's intrinsics directly has a path for the other in the same commit.

**The compiler settings are the settings.** Toolset `v145` (Visual Studio 2026), `/std:c++latest`, `/permissive-`, `/W4` with **warnings as errors**, `/fp:precise`, and `/arch:AVX2` on x64 or `/arch:armv8.0+lse` on ARM64 (R16). There is no CMake. If a build error tempts you to change the toolset, lower the language standard, turn off `/permissive-` or silence a warning — **stop and report instead.**

**Debug and Release are aligned by rule, not by luck.** Every setting that is not *about* optimisation reads identically in both configurations: language standard, conformance, warning level, include directories, precompiled header, floating-point model, instruction set. The two differ in exactly four things — `Optimization`, `_DEBUG` vs `NDEBUG`, `FunctionLevelLinking`/`IntrinsicFunctions`, and the linker's folding and LTCG switches. (MSBuild spells those four through a few more properties — `UseDebugLibraries`, `RuntimeLibrary` as the debug or release CRT, `LinkIncremental`, `WholeProgramOptimization`, `EnableCOMDATFolding`, `OptimizeReferences` — and that list is the whole of what may differ. Shaders follow `Optimization`: `FXCompile`'s `DisableOptimizations` and `EnableDebuggingInformation` differ with it, and Debug's `-Qembed_debug`, set in `Build/Earthrise.props`, which keeps that debug information inside the shader, [ADR-011](Design/ADR/ADR-011-renderer-first-choices.md).)

That alignment matters more than it looks, because **CI builds Debug only** (§6). Release is compiled by whoever ships, and a Release that quietly lost an include directory or sat on an older language standard would not be discovered until then. A static check of the two configurations is what stands in for the build nobody runs.

**x64 and ARM64 are aligned the same way.** Every setting that is not about the architecture reads identically on both platforms. They differ in the platform name, `/arch`, and the output directories MSBuild names after the platform, and nothing else. CI builds and tests only x64 (§6), so before you claim that ARM64 builds, build it, and before you claim anything about ARM64 behaviour, run the suites on an ARM64 machine, and say so.

**Build through the solution, never a `.vcxproj` directly.** Output paths and cross-project include directories are anchored on `$(SolutionDir)`, and MSBuild defines `SolutionDir` only for a solution build. Building a project file directly resolves every one of those paths against the *project* folder instead of the repository root. **It does not fail — that is the problem.** Output lands in the wrong folder, so the next solution build links against whichever copy is staler, and every cross-project include path becomes a directory that does not exist. The breakage is latent: it bites the first time a file reaches across projects, which may be weeks after someone got into the habit. To build one project, use `/t:<ProjectName>` on the solution.

```powershell
# Everything, from the repository root, naming the solution.
msbuild <Solution>.slnx /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo

# One project, still through the solution.
msbuild <Solution>.slnx /t:<ProjectName> /p:Configuration=Debug /p:Platform=x64 /m /nologo

# Release, before you claim anything about it.
msbuild <Solution>.slnx /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo

# ARM64, which CI does not build: build it yourself, and run it on an ARM64 machine (ADR-007).
msbuild <Solution>.slnx /p:Configuration=Debug /p:Platform=ARM64 /m /v:minimal /nologo
```

**A project does not put its own directory on the include path.** `cl.exe` already searches the directory of the including file first for a quoted include, so `#include "FileSys.h"` from a `.cpp` in the same folder resolves without help. Only the directories of *other* projects are listed, as `$(SolutionDir)<Project>`.

**Run the tests**, through `vstest.console.exe`, over every suite the build produced.

**vstest reports "no tests found" as a pass.** An empty suite is therefore worse than no suite: it is a green check mark over a library nobody exercised. Every test project ships a placeholder `SuiteSmoke` for exactly this reason; delete it when the first real test lands, never before.

**Run the checkers before you push.** They are seconds of Python and they are what CI runs:

```powershell
python Build\CheckFormat.py           # clang-format, whole tree. --fix rewrites the offenders
python Build\CheckProjectFiles.py     # build shape, project registration, R2/R7/R11
python Build\RunClangTidy.py          # needs a Developer PowerShell (INCLUDE must be set), after a build
python Build\TestCheckers.py          # the checkers' own tests, when you change a checker
```

**A green build says nothing about whether the game draws.** The render tests on WARP ([ADR-012](Design/ADR/ADR-012-render-tests-on-warp.md)) check what a number can check, such as coverage, colour and depth, and nothing about the look or the speed. For anything touching rendering, input, audio or presentation, launch the executable and look at it. For the voxel renderer, look where the paper itself says the method is fragile:

- voxels close enough to cross the near plane, which must clip cleanly and must not vanish or smear;
- distant voxels at the farthest zoom, about 4 px each, which must stay put while the camera moves; unstable quad bounds or colour jitter show up as shimmer on silhouettes against the sky. Stochastic pruning is not used ([ADR-006](Design/ADR/ADR-006-voxel-sizes.md)), and units switch to impostors below 3 px, which must not pop.

**Report what you actually did.** "Builds clean, not run" and "builds and runs" are different claims. Never imply the second when you only did the first, and say which configurations you built.

---

## 4. Layout and formatting

[`.clang-format`](.clang-format) is the authority for C++ layout: 2-space indent, 140 columns, Allman braces, pointer and reference bound left, includes never reordered. [`.editorconfig`](.editorconfig) covers everything clang-format does not — CRLF, UTF-8, final newline, trailing whitespace, and the non-C++ formats — and repeats the two numbers an editor needs before the first save.

**This tree is formatted, and CI keeps it that way.** A whole-tree format check here is a no-op. Format what you write; if the check fires, run `--fix` and commit the result rather than arguing with it.

- **Do not reformat what your task did not touch.** The check being green tree-wide means a drive-by reformat produces pure churn and buries your actual change.
- **Include order is load-bearing and grouped by hand**, which is why `SortIncludes` is `Never`: `pch.h`, then `<windows.h>` before any D3D12/DXGI/XAudio2 header, then the rest of the SDK, then project headers, then the standard library. A formatter reordering these behind a change's back is a correctness risk, not a style preference.
- **One header owns the Windows macro family, and nothing else defines any of it.** That header is `Engine/framework.h` ([ADR-010](Design/ADR/ADR-010-solution-and-build-layout.md)). `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `NOMCX`, `NOSERVICE`, `NOHELP` are set there, before `<windows.h>`, and the project files deliberately define none of them. Two owners of one macro is C4005, and `/WX` makes that fatal — `/D` spells a bare macro as `1` where a `#define` spells it as nothing, so the collision is guaranteed rather than possible. If you need `<windows.h>`, include that header; do not add the macros yourself.
- Do not silence a diagnostic with `#pragma warning(disable: ...)` to make a build pass. Fix the cause, or report it.

---

## 5. Rules for this codebase

**R12 — Graphics is Direct3D 12.** COM lifetimes are RAII from the first line — a raw `AddRef`/`Release` pair in new code is a defect, not a style. The COM smart pointer is `winrt::com_ptr`, and a failed `HRESULT` goes through `winrt::check_hresult`, both from `<winrt/base.h>`; `Microsoft::WRL::ComPtr` and `<wrl/client.h>` are not used. `check_hresult` throws `winrt::hresult_error`, so a function that calls it is not `noexcept`. **How voxels reach the screen is settled by [ADR-001](Design/ADR/ADR-001-direct-voxel-ray-casting.md).** Each voxel is a screen-space quad sized by its projected bounds. A ray-box test in the pixel shader finds the exact hit, and depth is written conservatively. The world is never converted to triangle meshes. Work that would change any of that is a new ADR, not a refactor. The rest of the renderer's shape is not settled here: passes, render targets, resolution, scaling, window style, multisampling, text, and everything ADR-001 lists as open. Each is a design and engineering decision taken when the renderer is built, and recorded as an ADR (§6).

**R14 — No third-party dependencies and no package manager.** The Windows SDK and the MSVC standard library, and nothing else. If you believe something is unavoidable, propose it in your report with what it buys and what it costs — do not add it. This is a closed list, not a high bar.

**Two exceptions are decided** (GameDesign.md, 2026-10-31): test projects use Visual Studio's native unit-test framework (`CppUnitTest.h`), which vstest runs and which never reaches the game; and build-time tools run on Python, which never ships.

**It binds what the executable is built from, not what a development tool needs.** Scripts under `Build/` and `Tools/` never ship and never link, so a baker that needs Pillow does not reopen this rule. **Third-party *content* is a different question and it is the owner's**: art, fonts and sound are allowed, and anything under a licence needs the owner's approval before it lands, with the licence text travelling with the bytes.

For Direct3D that list means what the Windows SDK installs: `d3d12.h`, `dxgi1_6.h`, `DirectXMath.h`, `winrt/base.h` (C++/WinRT's `winrt::com_ptr` and `winrt::check_hresult`, which R12 asks for; the header ships in the SDK's `cppwinrt` folder, so the `Microsoft.Windows.CppWinRT` NuGet package is not needed and not used) and the `fxc`/`dxc` compilers that `FXCompile` drives. It excludes what a D3D12 sample reaches for by reflex, because each is NuGet or GitHub content and not SDK content: the DirectX Agility SDK and its `d3dx12.h`, DirectX-Headers, DirectXTK12, DirectXTex, and the DirectX Shader Compiler as a redistributable. GPU vendor extension libraries are excluded too: NVAPI, which the paper behind ADR-001 suggests for `QUAD_FILLMODE`, and AMD AGS. Resource barriers and heap descriptions are written by hand.

**R15 — Memory is plain C++.** `new`/`delete` where it must be, RAII everywhere, standard containers by default. No pool, slab or free-list allocator without a decision recorded in `Design/ADR/`.

**R16 — The floating-point model and instruction set are stated, not inherited.** Every project compiles `/fp:precise`, and **`/arch:AVX2`** on x64 or **`/arch:armv8.0+lse`** on ARM64 ([ADR-007](Design/ADR/ADR-007-platforms-and-input.md)), stated explicitly in `Build/Earthrise.props`, the property sheet every project imports and none may override ([ADR-010](Design/ADR/ADR-010-solution-and-build-layout.md)), rather than inherited from an MSVC default — a default is not a decision, and the symptom of losing one is two builds of the same code disagreeing about the same sum with no line to blame. Each setting is identical in Debug and Release. `/fp:contract` is not used.

**What the floors cost is named rather than waved at.** `/arch:AVX2` sets an x64 floor of Intel Haswell (2013) and AMD Excavator (2015); an older CPU meets an illegal instruction, not a message. `/arch:armv8.0+lse` is the floor Windows 11 on Arm itself requires, so it excludes nothing that runs Windows 11. Under `/fp:precise` MSVC does not contract `a*b+c` into an FMA unless `/fp:contract` is given, which has been the default since Visual Studio 2022. Float results still differ between x64 and ARM64, because the CRT's math functions and DirectXMath's SIMD paths differ, so a result that must be bit-identical across builds is not computed in float.

**If the game needs a deterministic core** — a simulation that replays, lockstep networking, a result that must reproduce from a seed — that is a decision recorded as an ADR, and inside that core: no `float` where a fixed-point or integer quantity will do (hold a fraction as integer hundredths and say so in the name, R6), no iteration over an unordered container whose order reaches the outcome, no wall-clock time (a tick is the clock, and wall time maps to ticks at one seam), and randomness from a pinned PRNG with a recorded seed — never `std::random_device`, never a hash of an address.

**R17 — A string you do not write is `const`.** `/permissive-` turns on `/Zc:strictStrings`: a literal is `const char[N]` and will not bind to `char*`. The fix is `const` on the signature, never a cast at the call site — a `const_cast` here is a lie about a literal that lives in a read-only section, and writing through it is a real crash rather than a theoretical one.

**R18 and up are reserved.** A design document does not only say what to build; some of what it says constrains how the code is *shaped* — which state a decision routine may read, what an emitted event has to carry with it, where tuning values live. Those are conformance rules with a design source, and they are written here as R18 onward, each citing the passage of the design it comes from, without renumbering anything above. The design now exists, but no rule has been drawn from it yet. Do not invent one, and do not import one from another tree: a rule with no source behind it is a rule nobody can settle an argument with.

---

## 6. Working rules

**Stay in scope.** Do what the task asks. Adjacent code that offends you is not part of the task — note it in your report and move on. Unrequested "while I was in there" changes are the main way a young tree acquires regressions it cannot bisect.

**Record decisions as ADRs.** An engineering decision — a file format, a wire protocol, a subsystem's shape, an exception to a rule here — goes in `Design/ADR/` as one file per decision, numbered in order from `ADR-001-<slug>.md`, stating the context, the decision and what it forecloses. Figures in an ADR are measured, not estimated — if you quote one, say how you measured it, or say that it is arithmetic on the format and not a measurement. A decision nobody wrote down gets re-litigated every few months by whoever forgot it.

**An ADR may be written ahead of its code** (the owner, 2026-11-01), so that the owner decides before code rests on it:
- It is **Proposed**, and binds nothing.
- **Before code rests on it,** the owner gives it the go-ahead, or changes it, and the plan records that where it tracks the owner's inputs. For M2, that is MvpPlan.md, M2, input 1.
- **The commit that first implements it** marks it **Accepted**, with the date and any amendment the code forced. The owner's merge of that pull request is the acceptance.

An ADR written with its code is Accepted in that same commit. ADR-001 to ADR-012 were written before this rule and stand as they are: all are Accepted except ADR-006, which by its own terms is accepted after M3.

**The checkers come first.** `Build/CheckFormat.py`, `Build/CheckProjectFiles.py` and `Build/RunClangTidy.py` are what §1, §2 and §3 lean on, and they landed in M0, before any C++. `Build/TestCheckers.py` runs in CI beside them. A rule they do not cover is review's problem until a check covers it.

**What CI runs.** [`.github/workflows/build.yml`](.github/workflows/build.yml) has two jobs: a Windows job that checks the build shape, builds **Debug|x64**, runs the test suites, then clang-tidy and clang-tidy's own test on the fixtures; and a Linux job that checks formatting on clang-format 18, runs the checkers' own tests (`Build/TestCheckers.py`, `Tools/TestCheckAssets.py`) and runs the asset check (`Tools/CheckAssets.py`) over `GameData/`. **Every step blocks.** While the tree was empty, each gate was guarded on the file it needed and skipped without it; those inputs are now permanently there, and the guards were removed when M0 closed, so a missing solution or test suite fails the build. Nothing is `continue-on-error`, and a script that fails fails the build. Never add a guard to get past a red build.

**CI does not build Release.** The Windows build is the slow half of the pipeline and a second configuration roughly doubles it for a tree where the two differ only in optimisation. What stands in for it is the static alignment check on the two configurations (§3) — and, before a release, an actual `Configuration=Release` build by whoever is shipping. If you change something that could plausibly break only under optimisation, build Release yourself and say so.

**CI does not build ARM64 either** (the owner's decision, [ADR-007](Design/ADR/ADR-007-platforms-and-input.md), amended). The build-shape check still fails when x64 and ARM64 drift apart on a setting, but code that only compiles on one architecture, such as an intrinsic with no path for the other, is caught only by an ARM64 build. If you touch intrinsics, `/arch`, or anything conditioned on the platform, build Debug|ARM64 yourself and say so.

**Commits and PRs.** Branch off `main`; small, focused commits with an imperative subject describing the change, not the process. CI must be green. Never commit build output, `.vs/` or `.user` files.

- **Work planned as work orders** (M2's are in [`Design/MvpPlan-M2.md`](Design/MvpPlan-M2.md)) is **one commit per order and one PR per phase** (the owner, 2026-11-01). The commit's subject begins with the order's id, as in `M2-B3: Add the byte writer and reader`.
- **Every commit builds and passes the suites on its own,** so the history bisects. CI runs on the PR's head, so push after each order's commit, with the phase's PR open as a draft. A red run is then always the newest commit's.
- **Work outside a plan** stays one change per PR.

---

## 7. Before you hand work back

- [ ] Naming conforms to §1 — `_` on parameters, `m_` on class state, `UPPER_CASE` constants, `PascalCase` enumerators, no `I`/`C`/`Base` affixes.
- [ ] Only the lines the task required were changed; no reformatting, no drive-by fixes.
- [ ] New, removed or moved files are in the `.vcxproj` **and** the `.filters` of every project involved.
- [ ] No project's `ConformanceMode`, `LanguageStandard`, `WarningLevel` or `TreatWarningAsError` was changed, and no warning was silenced with a pragma.
- [ ] Debug and Release, and x64 and ARM64, still agree on everything §3 says they must.
- [ ] No new third-party dependency (R14).
- [ ] The checkers pass — or, for one not yet written, the report says which and why.
- [ ] It builds Debug|x64, and every test suite runs and passes on x64. Debug|ARM64 too, if it touches intrinsics, `/arch` or anything conditioned on the platform.
- [ ] If it touches rendering, input, audio or presentation: it was **run**, not just built.
- [ ] `Design/ADR/` has a new file if the change *was* a decision.
- [ ] Your report states plainly what you verified, what you assumed, and any rule here you had to bend.
