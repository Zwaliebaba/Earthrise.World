# Earthrise.World — MVP implementation plan

**Status:** Accepted by the owner for M0 to M2, 2026-10-31; M3 to M7 stay Proposed until after M1 (GameDesign.md, *Starting the MVP*). Nothing here is implemented. [GameDesign.md](GameDesign.md) defines the MVP and governs where this plan differs from it. [AGENTS.md](../AGENTS.md) governs how the code is written. Every estimate is the design review's judgment estimate (F-52), in FTE-months for one experienced developer; none is a measurement. The renderer spike's actual time against its own written estimate rescales all of them.

**How the work is done.** Claude sessions write the code, tools and checkers and push them to branches. CI compiles Debug|x64 and Debug|ARM64 and runs the x64 tests. The owner runs the builds on Windows, judges the look and the measurements, and approves each milestone. Because an AI writes the code, the estimates below describe the work rather than predict the calendar; the pace of M0 and M1 rescales them.

## Scope

### What the MVP proves

The MVP proves pillars 1 and 2. They are stated as H1 and H2 (GameDesign.md, *Hypotheses*), and each is tested against the health-bar control. It ends with that verdict. It proves neither persistence nor networking, but it keeps both possible: the simulation sits behind the server boundary of [ADR-002](ADR/ADR-002-authoritative-simulation.md), and the world is a seed plus edits ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)).

The MVP ends at the verdict on stages 1 and 2 (GameDesign.md, decided 2026-10-31). Skirmish, stage 3, gets its own plan once that verdict is in: its largest item, the Skirmish AI, is research-grade, and building it is only worth doing if the pillars hold.

### In

- **The look** of [RendererLook.md](RendererLook.md): 1 m world voxels and 0.5 m models ([ADR-006](ADR/ADR-006-voxel-sizes.md)), and its palette, jitter, corner shading, lighting and camera.
- **Platforms:** Windows on x64 and ARM64, played with mouse and keyboard ([ADR-007](ADR/ADR-007-platforms-and-input.md)). The interface keeps to ADR-007's rule that hover is never the only way to see something.
- **One map**, 512 m × 512 m, generated from its seeds by the integer-only generator and held in memory. The Siege castle and the Hold maps are scenarios, which means saves of seeds, edits and entities.
- **Materials:** earth, common rock, building stone, bedrock, timber, and rubble that keeps the material it fell from; also grass, which only the generator places, leaves and water (GameDesign.md, 2026-10-31).
- **Economy, used by Hold only:** stone and timber, counted by volume. Six designations: quarry, pit, ditch, trench, felling and clearance. Two drop sites, the quarry yard and the lumber camp, with laborers hauling to them.
- **Templates:** hall and keep, palisade and palisade gate, standard stone wall, tower, gatehouse with its passage attack, quarry yard and lumber camp. These are T1–T9 in `GameData/Server/Templates/`, catalogued in [MvpAssets.md](MvpAssets.md). Ditch and rampart as earthworks. Deep foundations, priced by their volume.
- **Construction and repair:** construction voxel by voxel, after the footprint is levelled. Repair at a fixed rate per section with the ten-second lock. Critical parts with thresholds.
- **Destruction:**
  - craters for the ram and the trebuchet in timber, earth and stone;
  - the support search;
  - the collapse resolver: the warning, the fall, rubble, cascades;
  - the slump of earth and rubble;
  - the rule that stops loose material resting near the top of a structure.
- **Units:**
  - laborers;
  - spearmen, archers, outriders and men-at-arms;
  - the ram, sappers and the trebuchet.

  Each trial gives the player a fixed army (GameDesign.md, MVP), commanded as squads. Combat is stat-based with physical shots. Arrows and bolts get elevation steps, towers take garrisons, climb limits differ by unit class, and units drop when their ground goes.
- **Win rules:** the keep falls only to storming, and the Siege trial has a loss rule.
- **Readability:**
  - the camera;
  - the integrity view, read from the resolver;
  - the collapse preview and its alert;
  - silhouettes;
  - the passability view;
  - fog with last-seen land;
  - the readout at placement: soil depth and where a dig will slump;
  - the attack cursor;
  - the repair lock;
  - the HUD of [Hud.md](Hud.md): layout A, played with mouse and keyboard.
- **AI:**
  - the Siege defender, which fights, repairs, sallies out to a leash and shoots sappers;
  - the Hold waves, scored through the resolver with a seeded draw, plus one sapper wave.
- **Control:** walls built of segments with health bars, and resources gathered from nodes that run out. Both versions keep the same cosmetics.
- **Infrastructure:**
  - the simulation in-process behind the server boundary;
  - commands encoded as bytes from the first line;
  - fixed ticks;
  - replay within one build;
  - save and load;
  - instrumentation for the playtests.

### Out

- **Skirmish and its AI**, which come after the verdict.
- **Worlds**, with all their rules and online play.
- **Second and third cultures**, fire, eras and technology, provisions, iron and unit training.
- **The mangonel, the mantlet, crossbowmen, knights and thick walls.**
- **AI that plans undermining.** The scripted sapper wave is the one exception.
- **Streaming, and worlds larger than memory.**
- **The touch scheme, and Android** (ADR-007). Touch follows a positive verdict; Android has no date.
- **Final art, and any licensed content.** Placeholder art and sound are enough for the playtests. Every asset in the MVP is made for the project or is in the public domain.

### Inputs the owner has given (2026-10-31)

1. **The spike's pass mark:** an RTX 3060-class GPU at 1920×1080 and 60 frames a second. A frame is 16.7 ms, of which the voxel pass may take 8 ms at the default zoom.
2. **The roster and the fixed armies** as listed above, which fix what M4 builds.
3. **Own content only.** Every font, sound and piece of art is made for the project or is in the public domain. Text therefore needs a bitmap font drawn for the project, or one in the public domain.
4. **Platforms and squads** (ADR-007, GameDesign.md): x64 and ARM64 from the first project, mouse and keyboard in the MVP, the Surface measured from M1, and squads as what the player commands.
5. **The HUD** ([Hud.md](Hud.md)), from the owner's HUD package, with the bitmap font Earthrise Mason in `GameData/Client/Fonts/`.
6. **Game data** ([ADR-008](ADR/ADR-008-server-and-client-game-data.md)): sources split into `GameData/Server/` and `GameData/Client/`; the client has everything compiled in, and the server reads baked files from `<outputdir>\ServerData`.
7. **Starting the MVP** (GameDesign.md, 2026-10-31): the plan accepted through M2; how the pass mark is measured; the RTX 3060 machine is available, a Surface is not yet; the v5 layout for the test scene; CppUnitTest and Python allowed; the paper tests' values picked by the implementer as Proposed; the trebuchet's baked poses.

## Architecture

### Solution layout (the M1 projects are settled by [ADR-010](ADR/ADR-010-solution-and-build-layout.md); the rest stays Proposed until the milestone that adds it)

| Project | Kind | Namespace | Depends on | Holds |
|---|---|---|---|---|
| `Core` | static library | `Core` | (none) | Integer and fixed-point math, the pinned PRNG, hashing, byte encoding, containers, logging, asserts |
| `Engine` | static library | `Engine` | Core | Win32 window, pointer input (mouse, touch and pen) and timing; the Direct3D 12 device, frames in flight and the upload ring; pipeline states from embedded shaders; the voxel renderer of ADR-001; text; XAudio2 |
| `Protocol` | static library | `Protocol` | Core | Command types and their byte encoding, state-update value types, handles to chunk snapshots |
| `Content` | static library | `Content` | Core | The client's content, compiled in: palette colours, the client view of the templates, the models and the font, baked from `GameData/` (ADR-005, ADR-008). Data and accessors, no rules |
| `Simulation` | static library | `Simulation` | Core, Protocol | Reading `<outputdir>\ServerData` at startup (ADR-008); the world, generator, edits, support search and resolver, slump, navigation, units, combat, designations and jobs, templates, repair, AI, the fog filter, saves, replays and the tick loop |
| `Client` | static library | `Client` | Core, Engine, Protocol, Content | Camera, the command layer and the mouse-and-keyboard scheme that feeds it, squad selection and orders, interface and overlays, presentation of state updates, cosmetic debris |
| `Earthrise` | executable | (none) | all libraries | `wWinMain`, which joins the client and the simulation through an in-process transport |
| `RendererSpike` | executable | `RendererSpike` | Core, Engine, Content, Simulation, Protocol | The spike's scenes and measurements, kept afterwards as a rendering benchmark |
| `SimulationBench` | console executable | `SimulationBench` | Core, Protocol, Simulation | Headless measurements: spike question 6, the synthetic 2 km world, regression timing |
| `CoreTests`, `ProtocolTests`, `SimulationTests` | test projects | `<Project>Tests` | their library | Unit tests, run by vstest, each starting with `SuiteSmoke` |

The edges run one way:
- `Simulation` includes nothing from `Engine` or Direct3D, so it builds headless.
- `Client` includes nothing from `Simulation`. It sees only `Protocol` (ADR-002, decision 3).
- `Engine` knows voxels, not the game. Materials reach it as palette indices (R9).
- `Simulation` reads only `ServerData`, and `Client` only the compiled-in `Content`. The executable checks at startup that both were baked from the same shared sources (ADR-008, decision 5).
- Windows and Direct3D stay inside `Engine` and the executables. `Core`, `Protocol`, `Content` and `Simulation` include no Windows or Direct3D header (ADR-007).
- Every project has x64 and ARM64 platforms (ADR-007).
- Each project gets its own folder, flat, and is added to `.clang-tidy`'s `HeaderFilterRegex` in the commit that creates it.

### Decisions to record as ADRs, and when

| Decision | Taken in | Proposal |
|---|---|---|
| Solution layout | M1, first commit | [ADR-010](ADR/ADR-010-solution-and-build-layout.md), Accepted: `Earthrise.slnx`, the M1 projects, one property sheet, `Engine/framework.h`, output in `$(Platform)\$(Configuration)\` |
| The renderer's first choices | M1, first commit | [ADR-011](ADR/ADR-011-renderer-first-choices.md), Accepted: window, swap chain, render scale, reversed-Z depth, no MSAA, instanced quads, dxc at Shader Model 6.0, shadow-map sizes, built-in timing |
| What the voxel sizes and the camera cost | After M3 | Decided by the owner as the baseline: a 1 m world and simulation, 0.5 m models, orthographic at 34°. [ADR-006](ADR/ADR-006-voxel-sizes.md) is accepted once the M1 spike has filled in its measurements and M3 has read breach predictions on the 1 m world. |
| Lighting | End of M1, only if spike question 4 falls short | Traced shadows through an occupancy volume, or DXR |
| Tick rate and number representation | Start of M2 | 20 ticks a second; integer voxel coordinates; fixed-point positions for units and shots |
| Chunk dimensions | Start of M1 | 32³ voxels, as ADR-006 assumes; the spike needs a chunk store |
| Edit encoding | M2 | A sparse list of edits, replaced by a full copy past a threshold |
| Navigation model | M2 | A height field of 2.5D tiles, a clearance grid for each class (foot, cavalry, machines), and a dirty tile rebuilt at most once a tick |
| Queries across the boundary | Decided | [ADR-009](ADR/ADR-009-queries-across-the-server-boundary.md): read-only queries and answers, filtered like state updates |
| Content formats | Decided | [ADR-005](ADR/ADR-005-content-formats.md): JSON layer files, baked. [ADR-008](ADR/ADR-008-server-and-client-game-data.md): client content compiled into `Content`, server data baked into files in `<outputdir>\ServerData`, `.vox` written on demand for MagicaVoxel |
| Platforms and input | Decided | [ADR-007](ADR/ADR-007-platforms-and-input.md): Windows on x64 and ARM64; two input schemes over one command layer, touch after the verdict |
| Text rendering | M3 | Earthrise Mason ([Hud.md](Hud.md) §9): its glyph rows in `GameData/Client/Fonts/`, baked into `Content` by a tool, drawn only at whole multiples of a font pixel |
| How units are drawn | M1 | Units are 0.5 m models of 1 × 1 × 4 voxels, axis-aligned and rotated in 90° steps. Engine poses are baked; debris is oriented voxels. |

### One simulation tick, in order

The order is fixed, so that one build replays the same commands to the same result (ADR-002, decision 6). Deferred work is budgeted in units of work, never in elapsed time.

1. Decode and apply the commands that arrived for this tick.
2. Advance designations and jobs: digging, hauling, construction, repair at its fixed rate per section.
3. Move units along their paths, and resolve melee.
4. Advance shots through the voxel grid; apply hits to units, and stamp craters as edits.
5. Run the support search over the region each edit can affect, and start a collapse for each unsupported piece, in entity-id order.
6. Advance the collapses in progress: warnings, falls, rubble, damage, cascades that start next tick.
7. Run slump and the rule for loose material, within the tick's work budget.
8. Rebuild the navigation tiles the edits touched.
9. Check the win and loss rules: the storming clock, and the Siege trial's limits.
10. Filter state by what each player can see, and publish state updates and chunk snapshots.
11. Answer queries on the tick's state, filtered the same way, within what remains of the tick's budget (ADR-009).

## Milestones

| | Milestone | Ends with | FTE-months |
|---|---|---|---|
| M0 | Foundations and paper tests | Checkers in CI; the revised rules pass on paper; the spike's estimate written down | 0.5–1 |
| M1 | Renderer spike | ADR-006's measurements for the 1 m baseline and a 0.5 m world, on the RTX 3060-class machine, and on the Surface once one is available; the look's acceptance checks pass | 7.5–11.5 |
| M2 | Headless simulation | A scripted breach and units pathing through it, replaying identically | 8.5–15 |
| M3 | Breach lab | The first measured reading on H2's predictions | 1.5–2.5 |
| M4 | Siege trial | A Siege trial that can be played, with its control | 9.5–17.5 |
| M5 | Siege playtest | The verdict on H2 | 1–2 |
| M6 | Hold trial | A Hold trial that can be played, with its control | 5–9 |
| M7 | Hold playtest | The verdict on H1, and so the MVP's | 1–2 |
| | **Total** | | **34.5–60.5** |

The total is a little above the build budget's 32–56 for this stage (GameDesign.md, *Build budget*). The difference is the breach lab and the split into two playtests, which buy a verdict on H2 before the economy is built, bringing up ARM64 and measuring the Surface in M1, and pulling the chunk store and the client's bake forward into M1. M0 to M5 come to 28.5–49.5 FTE-months. At 20 hours a week, half of full time, that puts the verdict on H2 about 5–8 years in, and the MVP's verdict about 6–10 years in. That reading assumes a person writes the code. With Claude sessions writing it and the owner giving about 20 hours a week to running and judging it, the calendar comes from M1's measured pace instead (*The spike's own estimate*, under M1). The spike's ratio will move both.

**The critical path** runs M1 → M2 → M4 → M5. The paper tests in M0, the template tool and the placeholder art can run alongside it.

### M0 — Foundations and paper tests

**Build:**
- `Build/CheckFormat.py`.
- `Build/CheckProjectFiles.py`. It checks R2 affixes, R7 names and project registration, R11 spellings, flat folders, shader names, functional filters, and that Debug and Release agree, as do x64 and ARM64 (AGENTS.md §3).
- `Build/RunClangTidy.py`.

Each CI guard is removed once its input is there.

**Run on paper, before any gameplay code** (review, *Cheapest tests*). Values the design leaves open are picked by the implementer and recorded as Proposed in GameDesign.md, with their reasons:
- the Hold rule test with three quarry layouts (F-45);
- the breach-prediction prototype comparing a thickness readout with a support readout (F-04, F-47);
- the Siege castle's route table and range circles (F-01, F-02);
- the repair spreadsheet (F-03).

**Write down** the spike's own estimate. The pass mark is already decided (GameDesign.md, *Renderer spike*). Written 2026-10-31: see *The spike's own estimate*, under M1.

**Progress (2026-10-31):**
- The three checkers and the property sheet have landed, with `Build/TestCheckers.py`: the good fixture and the repository pass, and 37 broken variants each fail for their own reason.
- CI runs them: the project checker in the Windows job, and the format check and the checker tests in the Linux job.
- The paper tests are in progress; their reports go in `Design/Review/PaperTests/`.

**Done when:**
- the checkers run in CI;
- under the revised rules, the breach moves with where the player quarried in at least two of the three layouts;
- the support readout reaches a median accuracy of 0.65 or better;
- no route dominates in the table;
- the repair spreadsheet meets the pass mark the implementer proposes (the review suggested 1.3–2×, F-03).

### M1 — Renderer spike

**Build:**
- **The engine base,** built for x64 and ARM64: the window, pointer input, a timer, the device and DXGI factory, a flip-model swap chain, two or three frames in flight with fences, descriptor heaps, an upload ring, and barriers written by hand.
- **ADR-001's pipeline:** quad bounds for the orthographic camera in the vertex shader, the ray-box test in the pixel shader, and conservative depth.
  - **Two voxel sources in one pass:** world chunks of 1 m voxels from the simulation's snapshots, and model instances of 0.5 m voxels at entity positions, rotated in 90° steps.
  - **No stochastic pruning:** no world voxel falls below about 4 px, and units switch to impostors below 3 px.
- **A minimal chunk store,** in `Simulation`: 32³ chunks with copy-on-write snapshots, which the renderer draws from. Edits, saves and the generator stay in M2.
- **The client half of the bake and the asset check** (ADR-005, ADR-008): `Tools/BakeContent.py` writes the palette, the templates' client view and the models into `Content`, and `Tools/CheckAssets.py` runs in CI.
- **The look of RendererLook.md:**
  - colour jitter and corner shading, baked into each voxel's record when its chunk or model is built;
  - contact shading where models meet the world;
  - Lambert and hemisphere lighting;
  - a static shadow map updated on edits, and a per-frame shadow map for moving things;
  - units as 0.5 m models and impostors, and a collapse's model debris keyed on its id (RendererLook.md, checks 3 and 6);
  - the shallow-water rule and the shadow check (RendererLook.md, §3 and check 11).
- **The test scene:** the key scene, "The Breach at the West Wall".
  - **Its layout and its target look** come from the v5 scene in [Reference/SiegePrototypeV2/](Reference/SiegePrototypeV2/), which is already on the 1 m grid. The look is matched by eye, not pixel for pixel.
  - **What it holds:** a 128 m holding, forest, a walled castle with a breach and its rubble ramp, a ditch and rampart, a terraced quarry, trebuchets, and about 95 units as 0.5 m models.
  - **What it is built from:** a scene script in `RendererSpike`, with a seeded integer height field and the templates and models in `GameData/` placed by coordinate. The generator is M2 work, so the spike does not wait for it. The templates and models are used where they exist, and where the v5 scene's sizes differ from them, the templates win. What `GameData/` lacks (trees, cottages, mantlets, banners and stock stacks) the implementer draws as placeholder models, which the owner may replace.
  - **What is left out:** the reference scene's overlay of title, markers and legend, which uses licensed fonts, because the MVP uses only its own content.
- **Scripted edits:** quarrying at the peak rate back-solved from the tempo, and one wall collapse.
- **A render resolution of its own,** scaled to the display, so the Surface can be measured below its panel's resolution (ADR-007).
- **Render tests on WARP** ([ADR-012](ADR/ADR-012-render-tests-on-warp.md)). Each pass ships with a test that draws a known scene offscreen in CI and checks its coverage, colour and depth against values computed from the scene. Speed and the look are still measured and judged on the stated hardware.
- **In `SimulationBench`:** a first support search and a navigation tile rebuild, run on the MVP map and on a synthetic 2 km world carrying a season's edits.

**Done when:**
- the spike answers questions 1–6 with figures measured on the stated hardware: the RTX 3060-class machine, with the pass mark measured as GameDesign.md states it, and natively on a Snapdragon X Surface Pro once one is available;
- RendererLook.md's acceptance checks pass on screen;
- ADR-006's M1 measurements are filled in for the 1 m baseline and for a 0.5 m world, and an ADR records the solution layout. ADR-006 itself is accepted after M3.

If the 1 m baseline misses the pass mark, ADR-001 is revisited before any gameplay code, which is the kill criterion. The spike's estimate is then compared with its actual time, and this plan is rescaled.

### The spike's own estimate (written 2026-10-31, before M1 starts)

The review asks for this in writing before the spike starts, so that its actual time can rescale the plan (F-52; GameDesign.md, *Build budget*). It is a judgment estimate, not a measurement. It is given in three units:
- **Work content,** in FTE-days for one experienced developer, the plan's own unit, so it can be compared with M1's 7.5–11.5 FTE-months.
- **Owner hours:** the owner's time to run builds on Windows, judge the look and take the measurements. Claude sessions write the code (GameDesign.md, *Starting the MVP*).
- **Pull requests:** the number of changes, each green in CI before it merges.

| # | Task | Work (FTE-days) | Owner hours | PRs |
|---|---|---|---|---|
| 1 | The scaffold of ADR-010: the solution, ten projects, the sheet, `framework.h`, the precompiled headers, `SuiteSmoke`, built in CI on both architectures | 2–4 | 1 | 1 |
| 2 | The engine base: the DPI-aware window, pointer input, the timer, the device and debug layer, the flip-model swap chain, frames in flight, descriptor heaps, the upload ring and hand-written barriers (ADR-011) | 8–15 | 2 | 2 |
| 3 | A minimal chunk store with copy-on-write snapshots in `Simulation`, its `Protocol` handles and tests | 6–12 | 0 | 1 |
| 4 | The client half of the bake and the asset check (ADR-005, ADR-008), with the CI step | 5–8 | 0.5 | 1 |
| 5 | ADR-001's pipeline: per-chunk records, instanced quads for the orthographic camera, the ray-box test, reversed-Z with `SV_DepthLessEqual`, world chunks and 0.5 m models in one pass | 15–30 | 4 | 2 |
| 6 | The camera: orthographic at 34°, rotation in 90° steps, zoom from about 12 m to about 260 m | 2–5 | 1 | 1 |
| 7 | The look: jitter, baked corner shading, contact shading, Lambert and hemisphere light, the two shadow maps, impostors, debris keyed on the collapse id, the shallow-water rule | 25–45 | 8 | 3 |
| 8 | The render scale and its bilinear upscale | 2–4 | 1 | 1 |
| 9 | The test scene: a seeded scene script with the v5 layout, T1–T7, placeholder models, about 95 units, the quarrying and the wall collapse | 12–25 | 4 | 2 |
| 10 | Measurement: timestamp queries, the scripted camera path, the CSV reduced to the 95th percentile, edit-to-screen latency | 4–8 | 3 | 1 |
| 11 | In `SimulationBench`: a first support search and a navigation tile rebuild, on the MVP map and on the synthetic 2 km world | 20–40 | 1 | 2 |
| 12 | The 0.5 m world for ADR-006's comparison column | 4–8 | 2 | 1 |
| 13 | The acceptance checks on screen, ADR-006's table filled in, and the M1 report | 5–10 | 4 | 1 |
| | **Total** | **110–214, or 5–10 FTE-months** | **about 32** | **19** |

**The owner can give about 20 hours a week** to running builds and judging them (owner, 2026-10-31). The owner's 32 hours are therefore under two weeks of their time, and the calendar is set by round trips instead. There are 19 pull requests, and about half of them need one to three rounds of the owner looking at the screen. **Forecast: M1 takes 4–8 calendar weeks.** This is a judgment, and the time log below measures it.

**The bottom-up figure sits below the plan's 7.5–11.5 FTE-months** at its low end. The plan's figure stands until the spike's real pace says otherwise; the difference is itself a reading of how uncertain both are.

**The biggest risk to the estimate is debugging the renderer without eyes on the screen.** A Claude session cannot see a frame, so every visual defect costs a round trip through the owner. To keep that cheap, the spike writes a screenshot and its GPU timings to files on demand, which the owner can attach to an issue or a session. CI also renders each pass on WARP and checks the result by number (ADR-012), so a defect in coverage, colour or depth should reach CI before it reaches the owner. That leaves the owner's round trips for the look and the timings.

**How the actual pace is recorded:**
- each task's start and finish dates;
- its pull requests;
- the owner's hours, logged per task below as the work proceeds.

At the end of M1:
- the ratio of the actual owner hours and calendar weeks to this estimate rescales M2 to M7, as GameDesign.md's build budget asks;
- the work-content figures stay as the measure of scope.

| # | Started | Finished | PRs | Owner hours | Notes |
|---|---|---|---|---|---|
| 1 | 2026-10-31 | | #8 | | The scaffold, with each project's first piece |
| 3 | 2026-10-31 | | #9 | | The chunk store with copy-on-write snapshots |

### M2 — Headless simulation

**Build:**
- the tick loop, commands as bytes with a round-trip test for each, and entity storage;
- edits on M1's chunk store;
- the server half of the bake: the `ServerData` format, its reader and the server-data hash (ADR-008), built before the simulation tests run in CI;
- the integer-only generator, with golden hashes and the slope invariant in CI;
- the materials, and save and load;
- a replay test;
- the support search, the collapse resolver, slump and the rule for loose material;
- navigation, with climb limits for each class and the drop rule.

| Part | FTE-months |
|---|---|
| Generator | 1.5–3 |
| Edits, saves, replays (the chunk store exists from M1) | 1.5–2.5 |
| `ServerData`: format, bake and reader | 0.5 |
| Support search and resolver (research-grade) | 3–5 |
| Navigation (research-grade) | 2–4 |

**Done when,** with no window open, a scripted trebuchet breaches a test wall, the wall collapses with its warning, its rubble keeps its material, units are ordered inside and path through the breach, and the whole run replays tick for tick. The support search and the tile rebuilds must fit the budget the spike measured.

### M3 — Breach lab

**Build** a thin client:
- the camera;
- click-to-fire a trebuchet at a wall;
- the integrity view read from the resolver;
- the collapse preview and the passability view, with F1 toggling the integrity view and F2 the passability view;
- the text renderer and the font tool, which M4 used to hold;
- the first of the HUD: the top bar, the mode banner, the camera buttons, the collapse alert and the attack cursor for FIRE ([Hud.md](Hud.md) §11);
- prediction prompts, logged.

Run it with six people (F-48).

**Done when** the median prediction accuracy on items split evenly between open and shut reaches 0.65 or better. If it falls short, legibility is reworked before M4. That rework counts toward the one round the kill criterion allows (GameDesign.md, *Kill criteria*).

### M4 — Siege trial

**Build:**
- units, stats and combat, physical shots, elevation, garrisons and the gatehouse's passage attack;
- drawing units and buildings;
- fog with the visibility filter and last-seen land;
- the shared hash between the client's tables and `ServerData` (ADR-008, decision 5); the world templates T1–T7 redrawn natively at 1 m, and the Siege castle built from them;
- the subset of jobs the trial needs: repair and trenches;
- the rest of the interface: layout A of [Hud.md](Hud.md), with squad selection, control groups, the command card, the minimap, alerts and the repair lock, and every hover reading also reachable without hover (ADR-007);
- the Siege defender AI;
- the storming win and the loss rule;
- the control version of the trial;
- logging of breach points, shots, the stone spent by each side, and predictions.

| Part | FTE-months |
|---|---|
| Combat | 2.5–4 |
| Units and buildings drawn | 1.5–3 |
| Fog | 1–1.5 |
| The Siege castle, from the redrawn templates (the bake exists from M1 and M2) | 0.5–1 |
| Repair and trench jobs | 1–2 |
| Interface (text moved to M3) | 1.5–3 |
| Siege AI | 1–2 |
| Control | 0.5–1 |

**Done when** the trial can be won by storming and lost by the clock or by running out of attackers, the control plays the same in everything but the wall rule, the HUD renders without overlap or clipped text at the sizes Hud.md §11 names, and the AGENTS.md checklist passes, including running the executable.

### M5 — Siege playtest: the verdict on H2

**Run** the playtest that GameDesign.md proposes, on the Siege trial alone:
- 8 to 10 RTS players, with half playing the voxel version first;
- their reasons recorded before any rules are explained;
- prediction items split evenly between open and shut;
- preference passing at 7 of 8 or better.

**Done when** H2 passes, or its kill criterion fires.

### M6 — Hold trial

**Build:**
- the full set of designations, with laborer jobs, hauling, drop sites and construction;
- the readout at placement: soil depth and the slump preview;
- T8 and T9 redrawn natively at 1 m, with posts under their sheds;
- the HUD's laborer cards, its placement and dig readouts, with YIELD counting only stone the player can see, and the Hold control's GATHER ([Hud.md](Hud.md) §10, §11);
- the generator constraints for Hold maps: at least two outcrops, and mixed ground;
- the scored waves and the sapper wave;
- the control's node resources;
- the Hold content, whose maps start with the hall and a finished keep.

| Part | FTE-months |
|---|---|
| The rest of the economy | 2–3 |
| Placement readout | 0.5–1 |
| Hold maps | 0.5–1 |
| Waves | 0.5–1 |
| Node control | 0.5–1 |
| Content | 1–2 |

**Done when** the trial can be played through to its win rule in both versions, and on a paper re-run of M0's layouts, where the player quarried visibly changes where the waves breach.

### M7 — Hold playtest: the verdict on H1, and on the MVP

**Run** the same protocol on the Hold trial.

**Done when** H1 passes, so Skirmish is planned next, or its kill criterion fires. If it fires, the branch GameDesign.md describes applies, and the owner decides whether Worlds still goes ahead.

## Risks

| Risk | Where it shows | Response |
|---|---|---|
| The renderer misses the pass mark | M1 | Kill criterion: revisit ADR-001 before any gameplay code |
| 1 m craters do not show which weapon made them | M3 | Tune each weapon's crater radius and depth for each material, the client's debris, and the integrity view |
| Players cannot read breaches | M3, M5 | The single round of legibility work, then the kill criterion |
| The Siege defender is too weak or too strong to test anything | M4 | Scripted behaviors and tuning; it plays one authored castle, not arbitrary ones |
| The plan overruns | Every milestone | The spike's ratio rescales everything; the cut order in GameDesign.md applies past Skirmish, and within the MVP the trebuchet, the gatehouse attack and the breach lab are the last things to cut |
| Content takes longer than code | M4, M6 | The templates and models already exist in `GameData/`; one castle and two Hold maps only |

## Deferred to the Skirmish plan

Not part of this plan; they come in the Skirmish plan if H1 and H2 pass:
- provisions and fields, iron and the mine head, and unit training;
- eras and upgrades;
- the mangonel, the mantlet, crossbowmen, knights and thick walls;
- fire;
- the Skirmish AI with its castle building;
- every house playing for itself;
- matched start positions;
- maps of 512 to 768 m.
