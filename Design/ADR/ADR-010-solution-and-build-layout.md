# ADR-010 — The solution and build layout

**Status:** Accepted, 2026-10-31, by the owner. It is the solution-layout ADR that [MvpPlan.md](../MvpPlan.md) schedules for M1's first commit, and that AGENTS.md §2 requires when the first project is created. It settles the engineering items the MVP readiness review lists under C ([Review/MvpReadiness-2026-10-31.md](../Review/MvpReadiness-2026-10-31.md)). The renderer's own first choices are in [ADR-011](ADR-011-renderer-first-choices.md). Decision 2 is amended by [ADR-019](ADR-019-the-in-process-client.md), to which the owner gave the go-ahead on 2026-11-01: `Client` and `Earthrise` both join in M3, `RendererSpike` references `Client`, and `Build/CheckProjectFiles.py` holds the table of which project may reference which. Order M3-C1 writes it into decision 2 when it marks ADR-019 Accepted.

## Context

- **AGENTS.md fixes the rules but not the layout:**
  - flat project folders;
  - x64 and ARM64, each in Debug and Release, aligned;
  - v145, `/std:c++latest`, `/permissive-`, `/W4` with `/WX`, `/fp:precise`, and each architecture's `/arch`;
  - one header owning the Windows macros;
  - `pch.h` allowed as a name;
  - CppUnitTest for tests, run by vstest;
  - Python for build-time tools.
- **MvpPlan.md proposes the projects and the edges between them.** M1 also needs the chunk store, the client half of the bake, the asset check and `SimulationBench`.
- **CI already assumes some of the layout:**
  - one `.slnx` or `.sln` at the root;
  - test DLLs at `x64\<Configuration>\<Project>Tests.dll`;
  - `.clang-tidy`'s header filter is a registry of project names.
- **Two defects the review found:**
  - The filter is not anchored, so a header such as `…\Intermediate\Content\Palette.h` would match "Content".
  - The hand-run command in `.clang-tidy` passes `/DWIN32_LEAN_AND_MEAN /DNOMINMAX`, which collides with the one owning header (C4005, made fatal by `WarningsAsErrors`).

## Decision

1. **One solution, `Earthrise.slnx`, at the repository root.** CI finds it by its extension, and MSBuild builds `.slnx` directly.
2. **The projects in M1** are the MvpPlan layout's projects that M1 uses. Each sits in its own flat folder at the root, named for the project, with a namespace of the same name (R9):

   | Project | Kind | Depends on | In M1 for |
   |---|---|---|---|
   | `Core` | static library | (none) | The pinned hash and the percentile in M1; integer and fixed-point math, the pinned PRNG and byte encoding follow in M2 ([ADR-013](ADR-013-ticks-numbers-and-randomness.md)) |
   | `Engine` | static library | Core | The window, pointer input, the Direct3D 12 device, the voxel renderer ([ADR-011](ADR-011-renderer-first-choices.md)) |
   | `Protocol` | static library | Core | Handles to chunk snapshots |
   | `Content` | static library | Core | The client's baked tables (ADR-008) |
   | `Simulation` | static library | Core, Protocol | The minimal chunk store with copy-on-write snapshots; in M1 also the first support search, the navigation tiles and the test world; the rest in M2 |
   | `RendererSpike` | Windows executable | Core, Engine, Protocol, Content, Simulation | The spike's scene script, camera path and measurements |
   | `SimulationBench` | console executable | Core, Protocol, Simulation | Spike question 6 |
   | `CoreTests`, `EngineTests`, `ProtocolTests`, `ContentTests`, `SimulationTests` | CppUnitTest DLLs | their library | Unit tests, each starting with `SuiteSmoke`. `EngineTests` holds the render tests on WARP ([ADR-012](ADR-012-render-tests-on-warp.md)) |

   `SimulationBench` gained Content during M1 (amended 2026-11-01, by the owner, GameDesign.md, *M1's open values*, item 9). The test world it measures is built in Simulation from templates handed in through `Simulation::TemplateLookup`, and until the server bake wrote the templates the bench handed in Content's. Simulation itself never depends on Content. **The edge went in M2** (amended again 2026-11-01, by order M2-B6): the bench is a `wmain` that reads `ServerData\ServerData.bin` from beside its own executable, found from its first argument, and hands in `ServerData::Templates()` ([ADR-016](ADR-016-server-data-format.md)).

   `EngineTests` and `ContentTests` joined during M1, with the engine base and the client bake (MvpPlan.md, M1 tasks 2 and 4), once their libraries held code to test. `Client` and `Earthrise` join when M3 and M4 need them. A project joins the solution when the milestone that first uses it starts, never earlier.
3. **Output goes to `$(SolutionDir)$(Platform)\$(Configuration)\`** for every project: libraries, executables and test DLLs.
   - This is CI's test path, and it is the `<outputdir>` of [ADR-008](ADR-008-server-and-client-game-data.md). `ServerData\` and `GameData\` sit inside it.
   - Intermediate files go to `$(SolutionDir)$(Platform)\$(Configuration)\Intermediate\$(ProjectName)\`.
   - Both trees are build output and never committed (`x64/` and `ARM64/` are ignored).
4. **The mandated settings live in one property sheet, `Build/Earthrise.props`.** Every `.vcxproj` imports it explicitly, by a path relative to the project file (`$(MSBuildThisFileDirectory)..\Build\Earthrise.props`), so it also works in a build the solution did not start.
   - **What the sheet sets:**
     - the toolset;
     - the language standard;
     - conformance;
     - warnings as errors at level 4;
     - `/fp:precise`;
     - `/arch:AVX2` for x64 and `/arch:armv8.0+lse` for ARM64, as `AdditionalOptions`, so both architectures use one mechanism;
     - `CharacterSet=Unicode`;
     - the output and intermediate directories;
     - the four settings that may differ between Debug and Release.
   - **Projects may not override them.** A project file adds only what is its own: its kind, its files, its include directories (other projects' folders, never its own, AGENTS.md §3) and its references.
   - **This bends R16's wording.** R16 asks for these settings "in the project file". Here they are stated once in a sheet every project imports, which is still stated rather than inherited from an MSVC default. On acceptance AGENTS.md R16 and §3 say so.
5. **Each C++ project has a precompiled header,** `pch.h` built from `pch.cpp`. Every `.cpp` includes `"pch.h"` first.
   - A `pch.h` includes only standard and SDK headers the project uses.
   - `Core`, `Protocol`, `Content` and `Simulation` include no Windows header ([ADR-007](ADR-007-platforms-and-input.md), decision 3).
6. **`Engine/framework.h` owns the Windows macros.**
   - It defines `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `NOMCX`, `NOSERVICE` and `NOHELP`, then includes `targetver.h` and `<windows.h>`. `framework.h` and `targetver.h` are the names R7 already allows.
   - Engine's `pch.h` and each Windows executable's `pch.h` include it before any Direct3D, DXGI or XAudio2 header, and nothing else defines those macros.
   - **The minimum OS is Windows 11.** `targetver.h` states the `NTDDI_VERSION` of Windows 11's first release, named as the SDK names it, and `<sdkddkver.h>` derives `_WIN32_WINNT` from it as `0x0A00`, which Windows 10 and 11 share.
7. **The bake is a build step of `Content`.**
   - A target runs `python Tools/BakeContent.py` before compiling. It takes `GameData/` as inputs and writes headers to `$(IntDir)Generated\`, so the build is incremental.
   - Python comes from `PATH`, version 3.12 or later as CI pins it. When Python is missing, the build fails with a message saying so.
   - The server half of the bake (ADR-008) is a target of its own, `BakeServerData`, in `Simulation.vcxproj` (M2-B6, [ADR-016](ADR-016-server-data-format.md), decision 5). It runs `python Tools/BakeContent.py --server` before compiling, with the server's sources as inputs, and writes `$(OutDir)ServerData\ServerData.bin`. `Build/CheckProjectFiles.py` lets `Content` and `Simulation` bake, and no other project.
8. **The clang-tidy setup is corrected:**
   - **The header filter becomes `(^|[/\\])(Core|Engine|Protocol|Content|Simulation|RendererSpike|SimulationBench|[A-Za-z0-9]+Tests)[/\\][A-Za-z0-9]+\.h$`.** A project name then matches only as a whole path segment directly above the header. Generated headers sit one folder deeper, in `Generated\`, and never match.
   - **`Build/RunClangTidy.py` takes each translation unit's flags from the project files and the property sheet:** include directories, defines and the precompiled header. They are never written by hand.
   - **The sample command in `.clang-tidy` drops `/DWIN32_LEAN_AND_MEAN /DNOMINMAX`.**
9. **`Build/CheckProjectFiles.py` checks values, not only agreement.** It checks that:
   - every project imports `Build/Earthrise.props`;
   - the sheet holds the mandated values;
   - no project overrides any of them;
   - Debug and Release, and x64 and ARM64, differ only as AGENTS.md §3 allows;
   - the `.clang-tidy` registry names exactly the solution's projects;
   - every file is registered in the `.vcxproj` and its `.filters`;
   - R2, R7 and R11 hold, folders are flat, and shaders are named as §2 says.

   `Build/Fixtures/` holds a small correct project, `Build/TestCheckers.py` makes each broken variant from it in code, and the checker's own test is that it passes the first and fails each of the others for the right reason. Run on an empty tree, the checker would prove nothing.

## Consequences

- **The first commit of M1 is the scaffold:** the solution, `Build/Earthrise.props`, the ten projects with their `pch` and `SuiteSmoke`, and `framework.h`. It also corrects `.clang-tidy`. CI then builds both architectures and runs the three suites (since amended: CI builds x64 only, ADR-007, and runs every suite, five by the end of M1) for real.
- **M0's checkers are written against this layout,** with their fixtures, before the scaffold, so the scaffold is the first thing they gate.
- **Graphics Tools:** a developer machine needs the Windows optional feature "Graphics Tools" for the Direct3D 12 debug layer in Debug builds. CI's render tests create a WARP device ([ADR-012](ADR-012-render-tests-on-warp.md)); whether the runners have the debug layer is ADR-012's open question.
- **On acceptance,** as done the same day:
  - AGENTS.md R16, §3 and §4 name the property sheet and `framework.h`;
  - MvpPlan.md's layout table points here;
  - the readiness review's item C closes.

## What this forecloses

- Settings repeated in each project, or overridden per project.
- `Directory.Build.props`, or any other implicit import: the sheet is imported by name.
- More than one solution, and projects added before the milestone that needs them.
- A Windows header in `Core`, `Protocol`, `Content` or `Simulation`, or a second owner of the Windows macros.
- Hand-written clang-tidy flags.

## Left open

- The `.filters` groups of each project, which are chosen as each project is created (AGENTS.md §2).
- Where `ServerData`'s bake target lives, decided in M2. *(Closed by [ADR-016](ADR-016-server-data-format.md), decision 5: `Simulation.vcxproj`, implemented by M2-B6.)*
- A Release build in CI, which stays out (AGENTS.md §6).
