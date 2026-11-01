# Earthrise.World — MVP implementation plan

**Status:** Accepted by the owner for M0 to M2, 2026-10-31; M3 to M7 stay Proposed until after M1 (GameDesign.md, *Starting the MVP*). M2's revision of 2026-11-01 (its estimate, its owner inputs, the trebuchet's shot moved in from M4, and ADR-013 to ADR-016) is Proposed until the owner accepts it. M0 and M1 were closed by the owner on 2026-11-01 (GameDesign.md, *Closing M0 and M1*); nothing from M2 on is implemented. [GameDesign.md](GameDesign.md) defines the MVP and governs where this plan differs from it. [AGENTS.md](../AGENTS.md) governs how the code is written. Every estimate is the design review's judgment estimate (F-52), in FTE-months for one experienced developer; none is a measurement. The renderer spike's actual time against its own written estimate rescales all of them.

**How the work is done.** Claude sessions write the code, tools and checkers and push them to branches. CI builds Debug|x64 and runs its tests; Debug|ARM64 is built by hand (ADR-007). The owner runs the builds on Windows, judges the look and the measurements, and approves each milestone. Because an AI writes the code, the estimates below describe the work rather than predict the calendar; the pace of M0 and M1 rescales them.

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

1. **The spike's pass mark:** an RTX 3060-class GPU at 1920×1080 and 60 frames a second. A frame is 16.7 ms, of which the voxel pass may take 8 ms at the default zoom. (The benchmark became an RTX 3070 Ti Laptop GPU on 2026-11-01: *Closing M0 and M1*.)
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
| `SimulationBench` | console executable | `SimulationBench` | Core, Protocol, Simulation; Content until M2's server bake (ADR-010, amended) | Headless measurements: spike question 6, the synthetic 2 km world, regression timing |
| `CoreTests`, `EngineTests`, `ProtocolTests`, `ContentTests`, `SimulationTests` | test projects | `<Project>Tests` | their library | Unit tests, run by vstest, each starting with `SuiteSmoke`. `EngineTests` holds the render tests on WARP (ADR-012) |

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
| Tick rate and number representation | Start of M2 | [ADR-013](ADR/ADR-013-ticks-numbers-and-randomness.md), Proposed: 20 ticks a second; no floating point in `Simulation` or `Protocol`; integer voxel coordinates; Q16.16 positions for units and shots; SplitMix64 in `Core`, with keyed draws |
| Chunk dimensions | Start of M1 | 32³ voxels, as ADR-006 assumes; the spike needs a chunk store |
| Edit encoding | M2 | [ADR-014](ADR/ADR-014-edits-saves-and-replays.md), Proposed: a chunk's edits saved as a list or a run-length copy, whichever is shorter; a save holds everything the next tick reads; a replay is a start and the commands, with a state hash every second; a scenario is a baked source, not a save |
| Navigation model | M2 | [ADR-015](ADR/ADR-015-navigation.md), Proposed: one surface per column with its headroom; classes by climb over a run, headroom, width and fords; clearance per cell; A\* budgeted in nodes; region labels; every touched tile rebuilt on the tick of its edit |
| `ServerData` format and bake target | M2 | [ADR-016](ADR/ADR-016-server-data-format.md), Proposed: one file, a hashed body of tagged sections; tuning values converted exactly from decimal sources; the bake target in `Simulation.vcxproj` |
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
5. Run the support search over the region the tick's edits so far can affect, and start a collapse for each unsupported piece, in entity-id order.
6. Advance the collapses in progress: warnings, falls, rubble, damage, cascades that start next tick.
7. Run slump and the rule for loose material, within the tick's work budget. The edits of steps 6 and 7 are searched in step 5 of the next tick, so a slump that undermines a wall starts its collapse one tick later.
8. Rebuild every navigation tile the tick's edits touched, none deferred, and update the region labels (ADR-015).
9. Drop, lift or move the units whose cells changed, in unit-id order (ADR-015, decision 8).
10. Check the win and loss rules: the storming clock, and the Siege trial's limits.
11. Filter state by what each player can see, and publish state updates and chunk snapshots.
12. Answer queries on the tick's state, filtered the same way, within what remains of the tick's budget (ADR-009).

## Milestones

| | Milestone | Ends with | FTE-months |
|---|---|---|---|
| M0 | Foundations and paper tests | Checkers in CI; the revised rules pass on paper; the spike's estimate written down | 0.5–1 |
| M1 | Renderer spike | ADR-006's measurements for the 1 m baseline and a 0.5 m world, on the benchmark machine, and on the Surface once one is available; the look's acceptance checks pass | 7.5–11.5 |
| M2 | Headless simulation | A scripted breach and units pathing through it, replaying identically | 9–16 |
| M3 | Breach lab | The first measured reading on H2's predictions | 1.5–2.5 |
| M4 | Siege trial | A Siege trial that can be played, with its control | 9–17 |
| M5 | Siege playtest | The verdict on H2 | 1–2 |
| M6 | Hold trial | A Hold trial that can be played, with its control | 5–9 |
| M7 | Hold playtest | The verdict on H1, and so the MVP's | 1–2 |
| | **Total** | | **34.5–61** |

The total is a little above the build budget's 32–56 for this stage (GameDesign.md, *Build budget*). The difference is the breach lab and the split into two playtests, which buy a verdict on H2 before the economy is built, bringing up ARM64 and measuring the Surface in M1, pulling the chunk store and the client's bake forward into M1, and the parts M2's table had left out (M2, 2026-11-01). M0 to M5 come to 28.5–50 FTE-months. At 20 hours a week, half of full time, that puts the verdict on H2 about 5–8 years in, and the MVP's verdict about 6–10 years in. That reading assumes a person writes the code. With Claude sessions writing it and the owner giving about 20 hours a week to running and judging it, the calendar comes from M1's measured pace instead (*The spike's own estimate*, under M1). The spike's ratio will move both.

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
- The three checkers and the property sheet have landed, with `Build/TestCheckers.py`: the good fixture and the repository pass, and 40 broken variants each fail for their own reason.
- CI runs them: the project checker in the Windows job, and the format check and the checker tests in the Linux job.
- The three paper tests have run; their reports are in `Design/Review/PaperTests/`, and their values are in GameDesign.md, *2026-10-31: the paper tests' values*:
  - **The Siege route table** passes all three criteria under the owner's decisions and its Proposed fixes.
  - **The Hold rule test** passes in 3 of 3 layouts under the owner's choices: three rule changes, and a quarry's undermining outweighing its ditch reason. Layout A passes exactly at the bar.
  - **The breach-prediction prototype** is built, and waits for its twelve people. Their median accuracy is M0's remaining gate.

**Closed (2026-11-01)** by the owner, with the breach-prediction gate waived (GameDesign.md, *Closing M0 and M1*). Every other gate passed: the checkers run in CI with their guards removed, the Hold rule test passes in 3 of 3 layouts, no route dominates the Siege table, and repair adds 1.50× with one engine at the 20 s reload.

**Done when:**
- the checkers run in CI;
- under the revised rules, the breach moves with where the player quarried in at least two of the three layouts;
- ~~the support readout reaches a median accuracy of 0.65 or better;~~ waived by the owner, 2026-11-01;
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
- the spike answers questions 1–6 with figures measured on the stated hardware: the benchmark machine, with its RTX 3070 Ti Laptop GPU (GameDesign.md, *Closing M0 and M1*), with the pass mark measured as GameDesign.md states it, and natively on a Snapdragon X Surface Pro once one is available;
- RendererLook.md's acceptance checks pass on screen;
- ADR-006's M1 measurements are filled in for the 1 m baseline and for a 0.5 m world, and an ADR records the solution layout. ADR-006 itself is accepted after M3.

If the 1 m baseline misses the pass mark, ADR-001 is revisited before any gameplay code, which is the kill criterion. The spike's estimate is then compared with its actual time, and this plan is rescaled.

**Closed (2026-11-01)** by the owner ([Review/M1Report-2026-11-01.md](Review/M1Report-2026-11-01.md)). The 1 m baseline's voxel pass takes 2.16 ms at the 95th percentile on the benchmark, against 8 ms, and the owner judged RendererLook.md's acceptance checks passed on screen. ADR-006's M1 rows are filled in, and ADR-010 records the solution layout. The Surface waits for a Surface. The farthest zoom's GPU time, which question 3 also asks for, was not measured: the scripted path runs at the default zoom.

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
| 1 | 2026-10-31 | 2026-10-31 | #8 | | The scaffold, with each project's first piece |
| 2 | 2026-10-31 | 2026-10-31 | #10 | | The engine base, and ADR-012 on render tests on WARP |
| 3 | 2026-10-31 | 2026-10-31 | #9 | | The chunk store with copy-on-write snapshots |
| 4 | 2026-10-31 | 2026-10-31 | #12 | | The client half of the bake, and the asset check |
| 5 | 2026-10-31 | 2026-10-31 | #13, #15 | | ADR-001's pipeline: the voxel pass and its render tests, then the spike's first scene of chunks and models |
| 6 | 2026-10-31 | 2026-10-31 | #16 | | The strategy camera and its controls |
| 7 | 2026-11-01 | 2026-11-01 | #26, #27, #28, #29, #30 | | The look: jitter, corner shading, Lambert and hemisphere light, fords and rubble colours; the two shadow maps; contact shading; impostors; the collapse's debris |
| 8 | 2026-10-31 | 2026-11-01 | #17 | | The render scale and its bilinear upscale |
| 9 | 2026-11-01 | 2026-11-01 | #21, #22 | | The v5 test scene: the batch turn and palette swap, then the scene, its edits and its moving units |
| 10 | 2026-11-01 | 2026-11-01 | #19 | | Measurement: the GPU timer, the scripted path, the CSV and its 95th percentile, edit-to-present latency |
| 11 | 2026-11-01 | 2026-11-01 | #24, #25 | | The support search and navigation tiles, then the test world in Simulation and `SimulationBench` on the MVP map and the season world |
| 12 | 2026-11-01 | 2026-11-01 | #25 | | The 0.5 m column: the test world split eight ways, its memory, search and tiles |
| 13 | 2026-11-01 | 2026-11-01 | this PR | | The measured runs on the benchmark, ADR-006's table, the owner's sign-off and the M1 report |

**M1 took 2 calendar days** (2026-10-31 to 2026-11-01), in 19 pull requests, against a forecast of 4–8 weeks and 19 pull requests. The owner's hours were not logged, so the ratio that rescales M2 to M7 has its calendar half only, and the plan is not rescaled until the owner's hours are in.

### M2 — Headless simulation

**When it starts.** No M2 code is written until M1 closes: the pass mark is measured on the benchmark machine, ADR-006's table is filled in, and the owner signs M1 off (owner, 2026-11-01). The kill criterion says a missed 1 m baseline revisits ADR-001 before any gameplay code. Most of M2 is tied to the 1 m grid: the generator, the support search, slump and navigation. The decisions below are paper, and they are settled while M1 finishes.

**Build:**
- the tick in its fixed order, commands as bytes with a round-trip test for each, and entity storage (ADR-013);
- `Core`'s numbers: `Core::Fixed`, the integer square root, the pinned SplitMix64 with keyed draws, and the byte writer and reader, with the check that keeps `float` out of `Simulation` and `Protocol` (ADR-013);
- edits on M1's chunk store, through one call (ADR-014);
- the server half of the bake: the `ServerData` file, its reader and both hashes, with the bake target in `Simulation.vcxproj`, built before the simulation tests run in CI (ADR-016);
- the integer-only generator, with golden hashes and the slope invariant in CI;
- the materials from `ServerData`, and save and load (ADR-014);
- a replay test, and the save test of ADR-014, decision 4;
- the support search in the tick, the collapse resolver, slump and the rule for loose material;
- **the trebuchet's shot and its craters,** moved here from M4: the done-when below needs a trebuchet to breach a wall, and M3's click-to-fire needs the shot, while neither milestone had it in its parts. Arrows, bolts and hits on units stay in M4;
- navigation, with classes by climb, headroom and width, the search, units following their squad's path, and the drop rule (ADR-015).

| Part | FTE-months |
|---|---|
| The tick, commands, entities and `Core`'s numbers (missing from the table before 2026-11-01) | 1–1.5 |
| Generator | 1.5–3 |
| Edits, saves, replays (the chunk store exists from M1) | 1.5–2.5 |
| `ServerData`: format, bake and reader | 0.5 |
| Support search, resolver and slump (research-grade; M1 built a first search) | 2.5–4.5 |
| The trebuchet's shot and craters (moved from M4) | 0.5 |
| Navigation (research-grade; M1 built the foot tiles) | 1.5–3.5 |

### M2's own estimate (written 2026-11-01, before M2 starts)

Like the spike's, it is written before the work, so that M2's actual pace can be compared with it. It is a judgment estimate, in the same three units: work content in FTE-days, owner hours and pull requests. It credits what M1 already built: the chunk store, a first support search, the foot tiles, `MaterialRules` and the test world.

| # | Task | Needs first | Work (FTE-days) | Owner hours | PRs |
|---|---|---|---|---|---|
| 1 | The decisions: ADR-013 to ADR-016 accepted, and the owner's inputs below recorded in GameDesign.md | — | 2–4 | 3 | 2 |
| 2 | `Core`: `Fixed`, the integer square root, SplitMix64 and keyed draws in place of the three local copies, the byte writer and reader; the float check with its broken fixture; `TestWorld`'s camera accessors off `float` | ADR-013 | 4–8 | 0 | 1 |
| 3 | `ServerData`: the server half of the bake, `Rules.json` and its exact conversions, the reader, both hashes, the target in `Simulation.vcxproj`; `MATL` in place of `StandInMaterialRules`, `TMPL` in place of `Content`'s templates, `SimulationBench` off `Content`, the spike's shared-hash check | ADR-016; input 7 | 8–14 | 0.5 | 2 |
| 4 | The tick and commands: `Protocol`'s commands and their encoding with a round-trip test each, the in-process transport, entity storage and ids, the tick's twelve steps as stubs until their tasks land, a headless runner | ADR-013 | 10–18 | 0.5 | 2 |
| 5 | Edits: the one editing call, the edited-chunk set, the tick's edit list, chunk hashes; `TestWorld` and its edits through the call | ADR-014 | 3–6 | 0 | 1 |
| 6 | Saves and replays: the format, load, the save test, the replay test, the build id | ADR-014 | 10–18 | 0.5 | 2 |
| 7 | The generator: the world-scale layer, chunks as a pure function of seeds and coordinate, golden hashes and the slope invariant in CI, the spike drawing a generated map for the owner to judge | ADR-013; input 3 | 25–50 | 4 | 3 |
| 8 | The support search in the tick, and the collapse resolver: warning, fall, rubble with its material, damage in the strip, cascades a tick later in id order, collapses saved in progress | Tasks 3 and 5; inputs 4–8 | 25–45 | 2 | 3 |
| 9 | Slump and the rule for loose material, within the work budget | Task 8; input 9 | 12–25 | 1 | 2 |
| 10 | The trebuchet's shot through the voxel grid, its craters by material, and the scripted fire command | Tasks 3–5; input 12 | 8–15 | 1 | 1 |
| 11 | Navigation: headroom, ground kinds and classes, clearance, region labels, A\* in a node budget, squads and figures moving in fixed point, the drop step | ADR-015; tasks 3 and 4; inputs 10 and 11 | 35–65 | 2 | 4 |
| 12 | The M2 scenario in CI, the tick's timing on the benchmark machine, ADR-006 and these ADRs updated, the M2 report | All; inputs 2 and 13 | 8–15 | 3 | 2 |
| | **Total** | | **150–283, or 7–13 FTE-months** | **about 18** | **25** |

**The bottom-up figure sits below the parts table's 9–16 FTE-months,** as M1's did. The parts table stands until M1's pace says otherwise.

**The calendar cannot be forecast from M1 yet.** M1's code landed in two days, in 18 pull requests (#8 to #30), but no M1 task has a finish date and no owner hours are logged. The ratio this plan rescales by therefore does not exist yet. **Logging M1's finish dates and owner hours is what turns M1 into a measurement.** Until then, the judgment is 3–6 calendar weeks from M1's close. The code is not the constraint: the owner's 13 inputs and about 18 hours are, and so are the rounds that the research-grade tasks 8 and 11 take to come right. Answering all 13 inputs in one sitting, before M2 starts, removes most of the waiting.

**Order.**
- **The chain:** tasks 2, 4 and 5 come first, in that order, and then 6. Task 3 can run beside 4.
- **The critical path:** tasks 8 and 9, then 12. Task 11 runs beside them.
- **The generator, task 7,** is needed only by task 12's scenario. It can run beside everything after task 2, and tasks 8 to 11 test on M1's test world until it lands.

**The owner's inputs.** Each is a design value no document gives. AGENTS.md requires it written down before the code that rests on it. The proposals are the implementer's; the owner confirms or changes them.

| # | Input | For task | Proposal |
|---|---|---|---|
| 1 | Accept ADR-013 to ADR-016, or change them | 2–6, 11 | As written |
| 2 | **The tick's wall-time target.** It replaces "the budget the spike measured" in the done-when, which is circular: the spike measures costs, it does not set a budget | 12 | 25 ms, half a tick, at the 95th percentile, on one core of the benchmark machine, with `SimulationBench` recording its CPU |
| 3 | **What the generator makes on the MVP map** | 7 | 512 m × 512 m and 64 m high, two chunks. The relief and soil depths of M1's test ground, carried over the map. Bedrock depth varying so that thin soil over rock and deep earth both occur. Sparse outcrops of building stone, forest, and one river with fords. The owner judges the first maps in the spike, by eye |
| 4 | **A collapse's warning** | 8 | 1 s, plus the time a foot soldier at 1.4 m/s takes to walk the piece's height, rounded up to whole ticks: 135 ticks, 6.75 s, for an 8 m piece |
| 5 | **The fall** | 8 | It lasts the free-fall time from the piece's height, in whole ticks: 26 ticks for 8 m. The piece leaves the grid on the fall's first tick, and its rubble and damage land in equal parts over the fall, nearest the foot first |
| 6 | **Which way it falls** | 8 | Towards the one of four sides where the voxels whose removal unsupported it lie, measured from the piece's centre. Straight down when that offset is less than a quarter of the piece's width along it |
| 7 | **What each material leaves** | 3, 8 | One rubble voxel for each voxel that falls. Building stone and masonry become stone rubble (8); common rock, rock rubble (28); timber and roof, timber rubble (25); earth, grass, field and road, earth rubble (26). Leaves leave nothing. Each decoration index takes the behaviour of its most common host across the templates |
| 8 | **What the fall damages** | 8 | Every structure voxel in the landing strip below the rubble's top is removed and becomes rubble of its own material. Units in the strip take the collapse's crush damage, whose amount is M4's |
| 9 | **Slump** | 9 | Over the four side neighbours. Diagonal neighbours may then differ by 2 m, and navigation closes those diagonals. Loose material rests at most 1 m below a structure's top, confirming the paper tests' item 8 |
| 10 | **The navigation classes,** from the models (ADR-015) | 11 | Foot: R = 1, 2 m headroom, width 1. Cavalry: R = 2, 3 m, width 1. Machines: R = 3, 4 m, width 3. Every class fords one voxel of water. Rubble and fords halve speed. Foot moves at 1.4 m/s, the spike's walkers' speed, until M4's statistics |
| 11 | **A consequence of input 10 to confirm:** settled earth and rubble lie at 45°, so a rubble ramp into a breach is open to foot alone | 11 | Keep it. "Men-at-arms storm up the rubble" (GameDescription.md), and rams go for gates |
| 12 | **The trebuchet's craters,** on the paper tests' ladder of 1, 2, 4 and 7 voxels ([Siege route table](Review/PaperTests/SiegeRouteTable.md)) | 10 | **Timber:** very large, the voxel struck and its six face neighbours, timber only. **Earth:** large, 4 voxels, which slump closed. **Stone:** medium, the voxel struck and the one behind it along the shot. Reload, range and descent stay the paper tests' Proposed 20 s, 12–50 m and 45° or steeper. The ram's contact craters are M4's |
| 13 | **M2's work budgets a tick:** voxels slumped and path nodes expanded | 8–12 | Set by the implementer from M2's measurements to fit input 2, and recorded as Proposed |

**Risks this estimate sees** (judgments, beside the plan's risk table):
- **What a breach looks like on the 1 m grid may not be a collapse.** This is worked by hand from ADR-004, decision 2, not measured:
  - At stone's 2 m span, a hole w metres wide through the base of a T3 section leaves everything above it supported until w reaches 5. Then only a slot w − 4 wide falls.
  - With input 12's medium craters, two shots one above the other open a route for foot, one voxel by two, long before anything falls.

  So the done-when's "the wall collapses with its warning" depends on where the scripted shots land. Task 12 reports what the rules do to a T3 section, and the owner judges whether that reads as a breach before M3 asks players to predict one.
- **Clearance treats the ram as a 3 m square,** so its 7.5 m length can clip corners (ADR-015). T5's passage fits it with no margin, and T5's redraw has to keep 3 m by 4 m.
- **A scenario is not a save** (ADR-014, decision 7). GameDesign.md says it is, in a Proposed line, so the owner either accepts the change or the scenarios have to be rebuilt with every build.

**Done when** all of the following hold:
- **Headless, in CI** (`SimulationTests`, with no window open):
  - a trebuchet placed by the M2 scenario on a generated map fires on scripted commands at a T3 section, and its shots fly through the voxel grid and leave craters by material;
  - the section, or the part of it the rules drop, loses support, warns for its warning ticks before any of its voxels move, and falls towards the side that lost support;
  - its rubble keeps its material;
  - a squad of six foot, ordered inside before the breach, goes as near as it can. Once the tiles rebuild, it searches again and paths through the breach, and no unit stands on a structure on any tick;
  - the whole run replays with every state hash equal, and a save taken at any tick continues identically;
  - the generator's golden hashes and slope invariant pass.
- **On the benchmark machine:** every tick of the scenario fits input 2's target at the 95th percentile, measured in `SimulationBench`.
- **Throughout:** the AGENTS.md checklist passes.

**How the actual pace is recorded:** each task's start and finish dates, its pull requests, and the owner's hours, logged below as the work proceeds.

| # | Started | Finished | PRs | Owner hours | Notes |
|---|---|---|---|---|---|
| 1 | 2026-11-01 | | | | This plan and ADR-013 to ADR-016, Proposed |

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
- units, stats and combat, physical shots for arrows and bolts (the trebuchet's shot and craters come from M2), hits on units, elevation, garrisons and the gatehouse's passage attack;
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
| Combat (the trebuchet's shot moved to M2) | 2–3.5 |
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
