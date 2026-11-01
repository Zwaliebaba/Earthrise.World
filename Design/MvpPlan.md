# Earthrise.World — MVP implementation plan

**Status:** Accepted by the owner for M0 to M2, 2026-10-31; M3 to M7 stay Proposed until after M1 (GameDesign.md, *Starting the MVP*). M2's revision of 2026-11-01 was accepted when the owner merged it (pull request #32). M3's revision of 2026-11-01 was accepted when the owner merged it (pull request #45; GameDesign.md, *Closing M2*, item 6). It covers:
- M3's work orders and phases, in [MvpPlan-M3.md](MvpPlan-M3.md);
- the owner inputs, seventeen, each answered as proposed on 2026-11-01: fifteen with the plan, and two after the v3 asset package (pull request #44) (GameDesign.md, *M3's values*);
- ADR-019 to ADR-021, Proposed, with the owner's go-ahead.

**M4's revision of 2026-11-01 is Proposed,** and is accepted when the owner merges its pull request. It covers:
- M4's work orders and phases, in [MvpPlan-M4.md](MvpPlan-M4.md);
- the owner inputs, twenty-eight, each with the implementer's proposal and none yet answered;
- ADR-022 to ADR-028, Proposed, and the go-ahead for ADR-018, which has been Proposed since the v3 asset package.

M0 to M2 were closed by the owner on 2026-11-01 (GameDesign.md, *Closing M0 and M1* and *Closing M2*); nothing from M3 on is implemented. [GameDesign.md](GameDesign.md) defines the MVP and governs where this plan differs from it. [AGENTS.md](../AGENTS.md) governs how the code is written.

**How the work is done.** Claude sessions write the code, tools and checkers and push them to branches. CI builds Debug|x64 and runs its tests; Debug|ARM64 is built by hand (ADR-007). The owner runs the builds on Windows, judges the look and the measurements, and approves each milestone.

**What is forecast, and what is not.** The plan estimates no effort and neither forecasts nor logs time (owner, 2026-11-01: effort on planning M2, time on closing it; GameDesign.md, *No time logging*). Claude sessions write the code, so the work a solo developer would put in is not what sets the pace, and what does, the owner's attention, is not a number. A milestone is planned as orders and pull requests, and it closes on its done-when.

## Scope

### What the MVP proves

The MVP proves pillars 1 and 2. They are stated as H1 and H2 (GameDesign.md, *Hypotheses*), and each is tested against the health-bar control. It ends with that verdict. It proves neither persistence nor networking, but it keeps both possible: the simulation sits behind the server boundary of [ADR-002](ADR/ADR-002-authoritative-simulation.md), and the world is a seed plus edits ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)).

The MVP ends at the verdict on stages 1 and 2 (GameDesign.md, decided 2026-10-31). Skirmish, stage 3, gets its own plan once that verdict is in: its largest item, the Skirmish AI, is research-grade, and building it is only worth doing if the pillars hold.

### In

- **The look** of [RendererLook.md](RendererLook.md): 1 m world voxels and 0.5 m models ([ADR-006](ADR/ADR-006-voxel-sizes.md)), and its palette, jitter, corner shading, lighting and camera.
- **Platforms:** Windows on x64 and ARM64, played with mouse and keyboard ([ADR-007](ADR/ADR-007-platforms-and-input.md)). The interface keeps to ADR-007's rule that hover is never the only way to see something.
- **One map**, 512 m × 512 m, generated from its seeds by the integer-only generator and held in memory. The Siege castle and the Hold maps are scenarios: baked sources of seeds, structures, earthworks and entities, placed on the generated world ([ADR-014](ADR/ADR-014-edits-saves-and-replays.md), decision 7).
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
| `Client` | static library | `Client` | Core, Engine, Protocol, Content | Camera, the command layer and the mouse-and-keyboard scheme that feeds it, squad selection and orders, interface and overlays, presentation of state updates, cosmetic debris. Joins in M3 ([ADR-020](ADR/ADR-020-the-in-process-client.md)), with the world view the spike built |
| `Earthrise` | executable | (none) | all libraries | `wWinMain`, which joins the client and the simulation through an in-process transport: two queues of bytes and one state copy a tick (ADR-020). Joins in M3 with the breach lab; M4 adds the Siege trial |
| `RendererSpike` | executable | `RendererSpike` | Core, Engine, Content, Simulation, Protocol; Client from M3-C2, for the world view | The spike's scenes and measurements, kept afterwards as a rendering benchmark |
| `SimulationBench` | console executable | `SimulationBench` | Core, Protocol, Simulation; Content until M2's server bake, dropped by M2-B6 (ADR-010, amended) | Headless measurements: spike question 6, the synthetic 2 km world, regression timing |
| `CoreTests`, `EngineTests`, `ProtocolTests`, `ContentTests`, `SimulationTests`, `ClientTests` from M3 | test projects | `<Project>Tests` | their library | Unit tests, run by vstest, each starting with `SuiteSmoke`. `EngineTests` holds the render tests on WARP (ADR-012) |

The edges run one way:
- `Simulation` includes nothing from `Engine` or Direct3D, so it builds headless.
- `Client` includes nothing from `Simulation`. It sees only `Protocol` (ADR-002, decision 3).
- `Engine` knows voxels, not the game. Materials reach it as palette indices (R9).
- `Simulation` reads only `ServerData`, and `Client` only the compiled-in `Content`. The executable checks at startup that both were baked from the same shared sources (ADR-008, decision 5).
- Windows and Direct3D stay inside `Engine` and the executables. `Core`, `Protocol`, `Content` and `Simulation` include no Windows or Direct3D header (ADR-007).
- Every project has x64 and ARM64 platforms (ADR-007).
- Each project gets its own folder, flat, and is added to `.clang-tidy`'s `HeaderFilterRegex` in the commit that creates it.
- From M3 the edges are a table in `Build/CheckProjectFiles.py`, which fails a project that references or includes one it may not (ADR-020, decision 1).

### Decisions to record as ADRs, and when

| Decision | Taken in | Proposal |
|---|---|---|
| Solution layout | M1, first commit | [ADR-010](ADR/ADR-010-solution-and-build-layout.md), Accepted: `Earthrise.slnx`, the M1 projects, one property sheet, `Engine/framework.h`, output in `$(Platform)\$(Configuration)\` |
| The renderer's first choices | M1, first commit | [ADR-011](ADR/ADR-011-renderer-first-choices.md), Accepted: window, swap chain, render scale, reversed-Z depth, no MSAA, instanced quads, dxc at Shader Model 6.0, shadow-map sizes, built-in timing |
| What the voxel sizes and the camera cost | After M3 | Decided by the owner as the baseline: a 1 m world and simulation, 0.5 m models, orthographic at 34°. [ADR-006](ADR/ADR-006-voxel-sizes.md) is accepted once the M1 spike has filled in its measurements and M3 has read breach predictions on the 1 m world. |
| Lighting | End of M1, only if spike question 4 falls short | Traced shadows through an occupancy volume, or DXR |
| Tick rate and number representation | Start of M2 | [ADR-013](ADR/ADR-013-ticks-numbers-and-randomness.md), Accepted by M2-B1: 20 ticks a second; no floating point in `Simulation` or `Protocol`; integer voxel coordinates; Q16.16 positions for units and shots; SplitMix64 in `Core`, with keyed draws |
| Chunk dimensions | Start of M1 | 32³ voxels, as ADR-006 assumes; the spike needs a chunk store |
| Edit encoding | M2 | [ADR-014](ADR/ADR-014-edits-saves-and-replays.md), Accepted by M2-C2: a chunk's edits saved as a list or a run-length copy, whichever is shorter; a save holds everything the next tick reads; a replay is a start and the commands, with a state hash every second; a scenario is a baked source, not a save |
| Navigation model | M2 | [ADR-015](ADR/ADR-015-navigation.md), Accepted by M2-F1: one surface per column with its headroom; classes by climb over a run, headroom, width and fords; clearance per cell; every touched tile rebuilt on the tick of its edit; the search chosen by measurement in M2 |
| `ServerData` format and bake target | M2 | [ADR-016](ADR/ADR-016-server-data-format.md), Accepted by M2-B5: one file, a hashed body of tagged sections; tuning values converted exactly from decimal sources; the bake target in `Simulation.vcxproj` |
| Queries across the boundary | Decided | [ADR-009](ADR/ADR-009-queries-across-the-server-boundary.md): read-only queries and answers, filtered like state updates. M3's three queries and their budget: [ADR-020](ADR/ADR-020-the-in-process-client.md), Proposed |
| Content formats | Decided | [ADR-005](ADR/ADR-005-content-formats.md): JSON layer files, baked. [ADR-008](ADR/ADR-008-server-and-client-game-data.md): client content compiled into `Content`, server data baked into files in `<outputdir>\ServerData`, `.vox` written on demand for MagicaVoxel |
| Platforms and input | Decided | [ADR-007](ADR/ADR-007-platforms-and-input.md): Windows on x64 and ARM64; two input schemes over one command layer, touch after the verdict |
| Render tests | M1 | [ADR-012](ADR/ADR-012-render-tests-on-warp.md), Accepted: each pass draws a known scene on WARP in CI and checks its coverage, colour and depth |
| Text rendering | M3 | [ADR-019](ADR/ADR-019-text-and-the-interface-pass.md), Proposed: Earthrise Mason's bitmaps baked into `Content`, an atlas built at startup, one interface pass drawing quads into the back buffer at display resolution, text only at whole multiples of a font pixel |
| The in-process client | M3 | [ADR-020](ADR/ADR-020-the-in-process-client.md), Proposed: `Client` and `Earthrise` join; two queues of bytes and one copy of the whole visible state a tick; a fixed-step clock with the lab's fast-forward and no interpolation; three queries budgeted in units of work in `RULE` |
| The readability views | M3 | [ADR-021](ADR/ADR-021-world-overlays.md), Proposed: translucent voxel boxes through the voxel pass's overlay pipeline, depth-tested against the scene, with labels through the interface pass; every reading from the simulation |
| How units are drawn | M1 | Units are 0.5 m models ([MvpAssets.md](MvpAssets.md)), axis-aligned and rotated in 90° steps. Engine poses are baked; debris is oriented voxels. Arrows are line models, turned freely along their velocity ([ADR-018](ADR/ADR-018-line-models-and-the-line-pass.md), Proposed). |
| Formations and separation | M4 | [ADR-022](ADR/ADR-022-formations-and-separation.md), Proposed: one figure a cell, steps reserved in unit-id order, friendly figures swapping head-on; ranks of three on open ground, single file through a gap |
| Units and combat | M4 | [ADR-023](ADR/ADR-023-units-and-combat.md), Proposed: statistics in a `UNIT` section; one integer damage formula; melee to the eight neighbours, worked out together; arrows as straight physical shots that hit only their target; engines as squads of one body; shots cost stone |
| Structures at work | M4 | [ADR-024](ADR/ADR-024-structures-at-work.md), Proposed: the built mask; working read from the critical part's standing share; repair by the owner's laborers at a fixed rate with its lock; garrisons firing from the outer face; the gate as a flag; per-template values in `STRC` |
| Fog of war | M4 | [ADR-025](ADR/ADR-025-fog-of-war.md), Proposed: a visibility bit per player per cell each tick; the state update filtered per player; chunks sent when seen; last-seen land kept by the client; a column shade in the voxel pass |
| Scenarios and the Siege trial | M4 | [ADR-026](ADR/ADR-026-scenarios-and-the-siege-trial.md), Proposed: JSON sources baked into `SCEN`; loaded as tick 0; the trial's rules in step 10; its events, log and replay |
| The AI | M4 | [ADR-027](ADR/ADR-027-commanders.md), Proposed: commanders inside the simulation, one a player, reading only their player's view, run as step 13, saved, recomputed on replay |
| The control | M4 | [ADR-028](ADR/ADR-028-the-control.md), Proposed: health-bar structures behind one seam, `StructureRule`, chosen by the scenario; hit points calibrated on the voxel version |

### One simulation tick, in order

The order is fixed, so that one build replays the same commands to the same result (ADR-002, decision 6). Deferred work is budgeted in units of work, never in elapsed time.

1. Decode and apply the commands that arrived for this tick.
2. Advance designations and jobs: digging, hauling, construction, repair at its fixed rate per section and its lock, and engines deploying or packing.
3. Search for the squads that need a path, those marked by step 8 of the last tick and those still searching, within the node budget (ADR-015, decision 5); then move units along their paths, one to a cell (ADR-022), and resolve melee, every strike worked out before any lands (ADR-023).
4. Loose arrows and fire garrisons and the passage attack; advance shots through the voxel grid; land the ram's blows; apply hits to units, and stamp craters as edits.
5. Run the support search over the region the tick's edits so far can affect, and start a collapse for each unsupported piece, in entity-id order.
6. Advance the collapses in progress: warnings, falls, rubble, damage, cascades that start next tick.
7. Run slump and the rule for loose material, within the tick's work budget. The edits of steps 6 and 7 are searched in step 5 of the next tick, so a slump that undermines a wall starts its collapse one tick later.
8. Rebuild every navigation tile the tick's edits touched, none deferred, and what navigation keeps to answer reach (ADR-015).
9. Drop, lift or move the units whose cells changed, in unit-id order (ADR-015, decision 7).
10. Update which structures work, then check the win and loss rules: the storming clock, and the Siege trial's limits.
11. Filter state by what each player can see, and publish state updates and chunk snapshots, each chunk to a player who sees it (ADR-025).
12. Answer queries on the tick's state, filtered the same way, within the tick's query budget, a number of queries in units of work (ADR-009, amended 2026-11-01).
13. Let each commander read its player's view and queue its commands for the next tick's step 1, within its work budget (ADR-027; added by M4's revision, Proposed).

## Milestones

| | Milestone | Ends with |
|---|---|---|
| M0 | Foundations and paper tests | Checkers in CI; the revised rules pass on paper; the spike's estimate written down |
| M1 | Renderer spike | ADR-006's measurements for the 1 m baseline and a 0.5 m world, on the benchmark machine, and on the Surface once one is available; the look's acceptance checks pass |
| M2 | Headless simulation | A scripted breach and units pathing through it, replaying identically |
| M3 | Breach lab | The first measured reading on H2's predictions |
| M4 | Siege trial | A Siege trial that can be played, with its control |
| M5 | Siege playtest | The verdict on H2 |
| M6 | Hold trial | A Hold trial that can be played, with its control |
| M7 | Hold playtest | The verdict on H1, and so the MVP's |

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
- the repair spreadsheet meets the pass mark: with one engine, repair adds 1.3–2× to the time to open a section (the owner's, GameDesign.md, *the paper tests' values*, item 3).

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

If the 1 m baseline misses the pass mark, ADR-001 is revisited before any gameplay code, which is the kill criterion. (The plan was then to be rescaled from the spike's actual time; the owner dropped that on closing M2, GameDesign.md, *No time logging*.)

**Closed (2026-11-01)** by the owner ([Review/M1Report-2026-11-01.md](Review/M1Report-2026-11-01.md)). The 1 m baseline's voxel pass takes 2.16 ms at the 95th percentile on the benchmark, against 8 ms, and the owner judged RendererLook.md's acceptance checks passed on screen. ADR-006's M1 rows are filled in, and ADR-010 records the solution layout. The Surface waits for a Surface. The farthest zoom's GPU time, which question 3 also asks for, was not measured: the scripted path runs at the default zoom.

### The spike's own estimate (written 2026-10-31, before M1 starts)

The review asked for this in writing before the spike started, so that its actual time could rescale the plan (F-52). The owner dropped that rescaling on closing M2, with every forecast and log of time (GameDesign.md, *No time logging*); the work-content column in FTE-days had gone on 2026-11-01 with the plan's other effort estimates, and the owner-hours column and the calendar forecast went with the rescaling. What stays is the task list and its pull requests, the record of what M1 was planned as.
- **Pull requests:** the number of changes, each green in CI before it merges.

| # | Task | PRs |
|---|---|---|
| 1 | The scaffold of ADR-010: the solution, ten projects, the sheet, `framework.h`, the precompiled headers, `SuiteSmoke`, built in CI on both architectures (CI has built x64 only since ADR-007's amendment) | 1 |
| 2 | The engine base: the DPI-aware window, pointer input, the timer, the device and debug layer, the flip-model swap chain, frames in flight, descriptor heaps, the upload ring and hand-written barriers (ADR-011) | 2 |
| 3 | A minimal chunk store with copy-on-write snapshots in `Simulation`, its `Protocol` handles and tests | 1 |
| 4 | The client half of the bake and the asset check (ADR-005, ADR-008), with the CI step | 1 |
| 5 | ADR-001's pipeline: per-chunk records, instanced quads for the orthographic camera, the ray-box test, reversed-Z with `SV_DepthLessEqual`, world chunks and 0.5 m models in one pass | 2 |
| 6 | The camera: orthographic at 34°, rotation in 90° steps, zoom from about 12 m to about 260 m | 1 |
| 7 | The look: jitter, baked corner shading, contact shading, Lambert and hemisphere light, the two shadow maps, impostors, debris keyed on the collapse id, the shallow-water rule | 3 |
| 8 | The render scale and its bilinear upscale | 1 |
| 9 | The test scene: a seeded scene script with the v5 layout, T1–T7, placeholder models, about 95 units, the quarrying and the wall collapse | 2 |
| 10 | Measurement: timestamp queries, the scripted camera path, the CSV reduced to the 95th percentile, edit-to-screen latency | 1 |
| 11 | In `SimulationBench`: a first support search and a navigation tile rebuild, on the MVP map and on the synthetic 2 km world | 2 |
| 12 | The 0.5 m world for ADR-006's comparison column | 1 |
| 13 | The acceptance checks on screen, ADR-006's table filled in, and the M1 report | 1 |
| | **Total** | **19** |

There are 19 pull requests, and about half of them need one to three rounds of the owner looking at the screen.

**The biggest risk to the estimate is debugging the renderer without eyes on the screen.** A Claude session cannot see a frame, so every visual defect costs a round trip through the owner. To keep that cheap, the spike writes its GPU timings to a CSV, which the owner can attach to an issue or a session. (A screenshot on demand was planned here and not built; the owner dropped it on 2026-11-01.) CI also renders each pass on WARP and checks the result by number (ADR-012), so a defect in coverage, colour or depth should reach CI before it reaches the owner. That leaves the owner's round trips for the look and the timings.

**The record,** each task's pull requests:

| # | PRs | Notes |
|---|---|---|
| 1 | #8 | The scaffold, with each project's first piece |
| 2 | #10 | The engine base, and ADR-012 on render tests on WARP |
| 3 | #9 | The chunk store with copy-on-write snapshots |
| 4 | #12 | The client half of the bake, and the asset check |
| 5 | #13, #15 | ADR-001's pipeline: the voxel pass and its render tests, then the spike's first scene of chunks and models |
| 6 | #16 | The strategy camera and its controls |
| 7 | #26, #27, #28, #29, #30 | The look: jitter, corner shading, Lambert and hemisphere light, fords and rubble colours; the two shadow maps; contact shading; impostors; the collapse's debris |
| 8 | #17 | The render scale and its bilinear upscale |
| 9 | #21, #22 | The v5 test scene: the batch turn and palette swap, then the scene, its edits and its moving units |
| 10 | #19, #20 | Measurement: the GPU timer, the scripted path, the CSV and its 95th percentile, edit-to-present latency; then the borderless full screen and the measured run at 1920 × 1080 |
| 11 | #24, #25 | The support search and navigation tiles, then the test world in Simulation and `SimulationBench` on the MVP map and the season world |
| 12 | #25 | The 0.5 m column: the test world split eight ways, its memory, search and tiles |
| 13 | this PR | The measured runs on the benchmark, ADR-006's table, the owner's sign-off and the M1 report |

**M1 was carried out in 19 pull requests,** as planned.

### M2 — Headless simulation

**When it starts.** No M2 code is written until M1 closes: the pass mark is measured on the benchmark machine, ADR-006's table is filled in, and the owner signs M1 off (owner, 2026-11-01). M1 closed the same day (GameDesign.md, *Closing M0 and M1*). The kill criterion says a missed 1 m baseline revisits ADR-001 before any gameplay code. Most of M2 is tied to the 1 m grid: the generator, the support search, slump and navigation. The decisions below are paper, and they are settled while M1 finishes.

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

### M2's phases (written 2026-11-01, before M2 starts)

M2 is carried out as 31 work orders in seven phases, written out in [MvpPlan-M2.md](MvpPlan-M2.md). Each order is one commit and each phase one pull request (GameDesign.md, *Planning M2*; AGENTS.md §6). An order is a self-contained brief:
- what it changes;
- what to read first;
- the signatures it must keep;
- the tests that must pass in CI;
- what it waits on.

The plan credits what M1 already built: the chunk store, a first support search, the foot tiles, `MaterialRules` and the test world.

| Phase | Orders | What lands | Needs first |
|---|---|---|---|
| A | 1 | The owner's decisions, written down | Inputs 1–15 below |
| B | 6 | `Core`'s numbers, randomness and bytes; the float check; `ServerData` | A, and M1 closed |
| C | 5 | Commands, edits, the world and its tick, saves, replays | B |
| D | 3 | The generator | C; input 3 |
| E | 7 | The trebuchet's shot, craters, the collapse, slump | C; inputs 4–9 and 12 |
| F | 6 | Navigation | C; inputs 10 and 11; its last order also phase E |
| G | 3 | The done-when in CI, the tick's timing, the report | D, E and F; inputs 2 and 13 |
| | **31 orders, 7 pull requests** | | |

The phases' calendar forecast and owner hours, written here, were dropped on closing M2 (GameDesign.md, *No time logging*).
- **The owner answered all 15 inputs on 2026-11-01,** before M2 starts, which removes most of the waiting.
- **Phases D, E and F can run side by side** once C has merged, each in its own pull request.

**The owner's inputs.** Each is a design value no document gives, and AGENTS.md requires it written down before the code that rests on it. The implementer proposed them, and the owner took every proposal on 2026-11-01. A check of the documents the same day changed input 7 and added inputs 14 and 15. GameDesign.md, *M2's values*, records them all, and input 13 stays the implementer's to measure.

| # | Input | For orders | Decided, 2026-11-01 |
|---|---|---|---|
| 1 | Give ADR-013 to ADR-016 the go-ahead, or change them. Each is marked Accepted by the commit that first implements it (AGENTS.md §6) | B1, B5, C2, F1 | Go-ahead for all four, as written |
| 2 | **The tick's wall-time target.** It replaces "the budget the spike measured" in the done-when, which is circular: the spike measures costs, it does not set a budget | G2 | 25 ms, half a tick, at the 95th percentile, on one core of the benchmark machine, with `SimulationBench` recording its CPU. Changed after the M2 report: the breach run's slowest tick of play, all but tick 0, against 25 ms |
| 3 | **What the generator makes on the MVP map** | D1, D3 | 512 m × 512 m and 64 m high, two chunks. The relief and soil depths of M1's test ground, carried over the map. Bedrock depth varying so that thin soil over rock and deep earth both occur. Sparse outcrops of building stone, forest, and one river with fords. The owner judges the first maps in the spike, by eye |
| 4 | **A collapse's warning** | B5, E3 | 1 s, plus the time a foot soldier at 1.4 m/s takes to walk the piece's height, rounded up to whole ticks: 135 ticks, 6.75 s, for an 8 m piece |
| 5 | **The fall** | B5, E3, E4 | It lasts the free-fall time from the piece's height, under gravity of 9.81 m/s², rounded up to whole ticks: 26 ticks for 8 m. The piece leaves the grid on the fall's first tick, and its rubble and damage land in equal parts over the fall, nearest the foot first |
| 6 | **Which way it falls** | E3 | Towards the one of four sides where the voxels whose removal unsupported it lie, measured from the piece's centre. Straight down when that offset is less than a quarter of the piece's width along it |
| 7 | **What each material leaves** | B5, E3 | One rubble voxel for each voxel that falls. Building stone and masonry become stone rubble (8); common rock, rock rubble (28); timber and roof, timber rubble (25); earth, grass, field and road, earth rubble (26). Leaves leave nothing. A decoration takes its host's behaviour voxel by voxel: the bake writes it into the world as a palette index for its host, with the decoration's look (changed the same day from "the most common host across the templates", which made T1's openings in timber behave as stone) |
| 8 | **What the fall damages** | E4 | Every structure voxel in the landing strip below the rubble's top is removed and becomes rubble of its own material. Units in the strip take the collapse's crush damage, whose amount is M4's |
| 9 | **Slump** | B5, E6, E7 | Over the four side neighbours. Diagonal neighbours may then differ by 2 m, and navigation closes those diagonals. Loose material rests at least 1 m below a structure's top, confirming the paper tests' item 8 |
| 10 | **The navigation classes,** from the models (ADR-015) | B5, F1, F5 | Foot: R = 1, 2 m headroom, width 1. Cavalry: R = 2, 3 m, width 1. Machines: R = 3, 4 m, width 3. Every class fords one voxel of water. Rubble and fords halve speed. Foot moves at 1.4 m/s, the spike's walkers' speed, until M4's statistics |
| 11 | **A consequence of input 10 to confirm:** settled earth and rubble lie at 45°, so a rubble ramp into a breach is open to foot alone | F1 | Keep it. "Men-at-arms storm up the rubble" (GameDescription.md), and rams go for gates |
| 12 | **The trebuchet's craters,** on the paper tests' ladder of 1, 2, 4 and 7 voxels ([Siege route table](Review/PaperTests/SiegeRouteTable.md)) | B5, E1, E2 | **Timber:** very large, the voxel struck and its six face neighbours, timber only. **Earth:** large, 4 voxels, which slump closed. **Stone:** medium, the voxel struck and the one behind it along the shot. Common rock as stone; bedrock and water none; leaves as timber, leaves only; every rubble as earth. For M2 the shot reloads in 20 s, reaches 12–50 m and descends at 45° or steeper, leaving from 10 m above the engine's cell under gravity of 9.81 m/s². The ram's contact craters are M4's |
| 13 | **M2's work budgets a tick:** voxels slumped and path nodes expanded | E6, F3, G2 | Set by the implementer from M2's measurements to fit input 2, and recorded as Proposed |
| 14 | **Ground, and the spans not yet decided** | B5 | Palette.json gives each index a server-side `ground` flag: terrain, fields, roads and rubble are ground; masonry, timber, roof, leaves and decorations are not. Leaves span 3 m and common rock 2 m |
| 15 | **How queries are budgeted** (ADR-009 against ADR-013) | — | In units of work, a number answered a tick, oldest dropped first; the simulation reads no clock. M3 sets the number |

**Risks this estimate sees** (judgments, beside the plan's risk table):
- **What a breach looks like on the 1 m grid may not be a collapse.** This is worked by hand from ADR-004, decision 2, not measured:
  - At stone's 2 m span, a hole w metres wide through the base of a T3 section leaves everything above it supported until w reaches 5. Then only a slot w − 4 wide falls.
  - With input 12's medium craters, two shots one above the other open a route for foot, one voxel by two, long before anything falls.

  So the done-when's "the wall collapses with its warning" depends on where the scripted shots land. Order E3 tests the hand calculation, G3 reports what the rules do to a T3 section, and the owner judges whether that reads as a breach before M3 asks players to predict one. **The owner judged that it does** (2026-11-01, after the [M2 report](Review/M2-2026-11-01.md)): five shots open a hole a voxel above the wall's base, the slot above its middle falls, and its rubble makes the way in; the rules stay.
- **Clearance treats the ram as a 3 m square,** so its 7.5 m length can clip corners (ADR-015). T5's passage fits it with no margin, and T5's redraw has to keep 3 m by 4 m.
- **A scenario is not a save** (ADR-014, decision 7). The owner accepted the change on 2026-11-01, and GameDesign.md's line now says so.

**Done when** all of the following hold:
- **Headless, in CI** (`SimulationTests`, with no window open):
  - a trebuchet placed by the M2 breach run, built in `Simulation` so the tests and the bench share it, on a generated map fires on scripted commands at a T3 section, and its shots fly through the voxel grid and leave craters by material;
  - the section, or the part of it the rules drop, loses support, warns for its warning ticks before any of its voxels move, and falls towards the side that lost support, or straight down if it lost support evenly;
  - its rubble keeps its material;
  - a squad of six foot, ordered inside before the breach, goes as near as it can. Once the tiles rebuild, it searches again and paths through the breach, and no unit stands on a structure on any tick;
  - the whole run replays with every state hash equal, and a save taken at any tick continues identically;
  - the generator's golden hashes and slope invariant pass.
- **On the benchmark machine:** every tick of play in the breach run, all but its set-up tick 0, fits input 2's target, measured in `SimulationBench` (the owner, after the [M2 report](Review/M2-2026-11-01.md), in place of the 95th percentile over every tick).
- **Throughout:** the AGENTS.md checklist passes.

**The record.** Each phase's pull request, its commits, one per order, and what CI found. (Each phase's last commit filled this in with dates, CI runs and the owner's hours until the owner dropped time from the plan on closing M2, GameDesign.md, *No time logging*.)

| Phase | PR | Commits | Notes |
|---|---|---|---|
| Plan | #32 | 4 | This plan, the work orders and ADR-013 to ADR-016, Proposed. |
| A | #32 | 1 | The owner's 13 answers, in the plan's PR because they edit the files it adds. |
| B | #42 | 6 | B1–B6. One draft PR for B to G by the owner's choice. Owner runs: the spike after B2, B4 and B6. |
| C | #42 | 6 | C1–C5; C4 took two commits and two runs, its fix a commit of its own (clang-tidy 22, `bugprone-exception-escape`). |
| D | #42 | 4 | D1–D3; D3 took two commits and two runs, its fix a commit of its own (clang-tidy 22, `bugprone-integer-division`). Owner runs: the spike with `--generated-map`. |
| E | #42 | 9 | E1–E7; E2 and E4 took a second commit each, for clang-tidy 22 on `std::ranges::find` over positions. E2 onwards were pushed together at the owner's request, so CI ran on the newest commit only: five runs covered E2 to the end of G (the [M2 report](Review/M2-2026-11-01.md)). |
| F | #42 | 8 | F1–F6; F4 took a second commit for MSVC's C4244 in `std::ranges::fill`, and F5 one for C4702 after `Assert::Fail`. Cavalry's and machines' speeds and the graded fords followed the report (row G). |
| G | #42 | 9 | G1–G3, G1 with a second commit for clang-tidy 22; the report is [Review/M2-2026-11-01.md](Review/M2-2026-11-01.md). After G3, five commits carry the owner's answers to it and their record: cavalry's and machines' speeds, the fords graded for every class (GENERATOR_VERSION 2), the slowest tick of play as the timing rule, the breach judged a breach, and this log. Owner runs: `SimulationBench --breach-run` on the benchmark machine, and the spike with `--generated-map`, both taken 2026-11-01 (the report, *Owner runs*). |

**M2 was carried out in two pull requests,** #32 for the plan and phase A and #42 for phases B to G, one draft pull request for B to G by the owner's choice in place of one a phase. The benchmark machine's run on 2026-11-01 met the done-when's timing line: the slowest tick of play took 4.856 ms, 4.033 ms and 5.920 ms over three Release runs, against 25 ms ([M2 report](Review/M2-2026-11-01.md), *The budgets*). **The owner closed M2 the same day** (GameDesign.md, *Closing M2*).

### M3 — Breach lab

**When it starts.** The owner answered inputs 1–17 below on 2026-11-01, taking every proposal (phase A, order M3-A1). No M3 code is written until this revision is accepted by merging pull request #45 (GameDesign.md, *Closing M2*, item 6). M2 is closed, so nothing else holds it back. M3 is the first milestone that draws and takes input, so each order that does is run by the owner before its phase merges (AGENTS.md §3).

**Build** a thin client, which is the first game executable:
- `Client` and `Earthrise` join the solution ([ADR-020](ADR/ADR-020-the-in-process-client.md); ADR-010 amended), and the world view the spike built moves into `Client`;
- the simulation's side of the boundary: structures as entities, the state update as a value, queries as bytes with their budget, the three readings the lab needs, an aim check, a section's integrity and reach in a tile, and `Quiet`, for the lab's fast-forward to stop on;
- the camera, with Hud.md's keys and its framing below the top bar;
- the trebuchet drawn cocked and released, its shot drawn between ticks, and collapse debris from the v3 package's models (inputs 16 and 17);
- click-to-fire a trebuchet at a wall, with the attack cursor's four readings and a mark where the shot will strike;
- the integrity view read from the resolver;
- the collapse preview and the passability view, with F1 toggling the integrity view and F2 the passability view ([ADR-021](ADR/ADR-021-world-overlays.md));
- the text renderer and the font tool, which M4 used to hold ([ADR-019](ADR/ADR-019-text-and-the-interface-pass.md));
- the first of the HUD: the top bar, the mode banner, the camera buttons, the collapse alert and the attack cursor for FIRE ([Hud.md](Hud.md) §11);
- the lab: prediction items built in `Simulation`, whose keys the simulation gives, prompts in the client, logged, and a scoring tool.

Run it with six people (F-48).

### M3's phases (written 2026-11-01, before M3 starts)

M3 is carried out as 24 work orders in seven phases, written out in [MvpPlan-M3.md](MvpPlan-M3.md). Each order is one commit and each phase one pull request (GameDesign.md, *Planning M2*; AGENTS.md §6), with phases B to G in one draft pull request, by the owner's choice of 2026-11-01, as in M2. An order is a self-contained brief, as M2's were.

| Phase | Orders | What lands | Needs first |
|---|---|---|---|
| A | 1 | The owner's decisions, written down | Inputs 1–17 below |
| B | 8 | The simulation's side: structures, the state update, queries and their budget, the aim, integrity and reach readings, the lab's items, the font's and the debris table's bake | A |
| C | 3 | `Client`, `ClientTests` and `Earthrise`; the world view moves; the breach run on screen | B1–B3 |
| D | 2 | `Engine`'s interface pass and overlay pipeline, with render tests on WARP | A |
| E | 4 | Text, the top bar, the banner, the buttons and the alerts; picking, selection and FIRE | B8, C, D |
| F | 3 | The collapse preview, the integrity view, the passability view | B4–B6, E |
| G | 3 | The lab's flow and log, the scoring tool and the facilitator's guide, the session and the report | B7, F |
| | **24 orders, 7 pull requests** | | |

- **D needs only A,** so its two orders can land anywhere before E.
- **The owner runs** fall after C2, C3, E2, E4, F3 and G1, and the six-person session after G1's dry run. That is what sets M3's pace (*What M3 inherits from M2*, below).

**The owner's inputs.** Each is a design value no document gives, or a choice the documents leave open for M3, and AGENTS.md requires it written down before the code that rests on it. The implementer proposed each, and the owner took every proposal on 2026-11-01. GameDesign.md, *M3's values*, records them, and input 10 stays the implementer's to measure.

| # | Input | For orders | Decided, 2026-11-01 |
|---|---|---|---|
| 1 | Give [ADR-019](ADR/ADR-019-text-and-the-interface-pass.md), [ADR-020](ADR/ADR-020-the-in-process-client.md) and [ADR-021](ADR/ADR-021-world-overlays.md) the go-ahead, or change them, with ADR-010's amendment: `Client` and `Earthrise` join in M3, and `RendererSpike` references `Client`. Each is marked Accepted by the commit that first implements it (AGENTS.md §6) | B2, B3, C1, D1, D2 | Go-ahead for all three, as written, and for ADR-010's amendment |
| 2 | **The lab's items and its question** | B7, G1 | 16 items, L01 to L16, each a fresh world at the breach run's site, with prior damage by scripted shots, a marked aim voxel and N fixed per item from 1 to 5. The question: WILL W2 BE OPEN TO FOOT AFTER n SHOTS AT THE MARK?, answered OPEN or SHUT by O or S, or the banner's buttons, with no time limit. The lab fires the asked shots at the mark, so the key is the simulation's and the set splits 8 open and 8 shut. Before the items, two minutes of free fire on the intact site and one practice item. The integrity view is on by default, as GameDesign.md's *Hypotheses* says, and F1 and F2 are free |
| 3 | **Participants and scoring** | G2, G3 | Six people who play RTS games, each taking all 16 items once, in an order shuffled by a per-participant seed. Reasons are spoken and written down by the facilitator against the item's id; the client takes no typed text. Pass at a median accuracy of 0.65 or better, 11 of 16, as GameDesign.md states. The thickness readout is not run beside it: the paper test compared readouts, and the lab has one view |
| 4 | **Fast-forward and the pause** | B7, C3, G1 | An item's automatic parts run at ten times, at most eight ticks a frame, and the clock pauses while a question is up. The simulation reads no clock (ADR-013), so the live run reaches the ticks and the state hash of the headless key, which the log checks |
| 5 | **The integrity view's readings on the 1 m grid** | B5, F2 | The paper test's three readings per section, HOLE, OPENING and GAP, each with its verdict, plus GROUND and the weak point, as a label in the world above the section. The fall width comes from the resolver, by widening the hole on a scratch copy until a piece falls, not from 2 × span + 1: the paper test's ambiguity 3 shows the formula misreports when a pillar goes. No OPEN percentage in M3; Hud.md §5's row waits for a formula in M4 |
| 6 | **The collapse preview** | F1 | Drawn with the integrity view, F1: the piece pulsing in bad, its strip in bad on the ground. The alerts WILL FALL IN n S and HAS FALLEN show whatever is toggled |
| 7 | **The passability view** | B6, F3 | Foot only in M3, from the player's squad's start cell; reached cells in good and usable but unreached cells in dim, as boxes in the cell a unit stands in; BREACH OPEN AT W2 when the courtyard's goal cell is first reached |
| 8 | **The HUD's sizes** (GameDesign.md, *Open questions*) | E2 | The camera buttons draw 36 × 36 inside the 44 bar with 44 × 44 hit targets, so Hud.md's two numbers both hold. The deck's width at 1280 px at 1.5× is M4's question, when the deck is built |
| 9 | **The UI scale and the text sizes** | E1 | Hud.md §2's bands: 100–124 % gives 1, 125–174 % gives 1.5, 175 % and above 2, with `--ui-scale` to override; labels at 2, 3 and 4 screen px a font pixel at the three steps, names at 3, 4 and 6 |
| 10 | **The query budget** (ADR-009, *Left open*) | B3, G3 | `queryWorkPerTick` 16 units in `RULE`: an aim check 1, a tile's reach 1, a section's integrity 8; at most 64 pending, the oldest dropped. Proposed until G3 measures it on the benchmark machine |
| 11 | **Craters at 1 m: can players tell the weapons apart?** (GameDesign.md, *Open questions*, "settle before M3") | A1 | Not testable in M3, which has the trebuchet alone. It is tested in M5's playtest, once M4 adds the ram's craters, and the Open question moves there |
| 12 | **Interpolation between ticks** (ADR-002 and ADR-013, *Left open*) | C3 | None in M3: an entity is drawn where its last update put it, about a pixel a tick for a walking figure at the default zoom. Judged on screen; M4 decides if units stutter |
| 13 | **The lab's log** | G1, G3 | One CSV a run in `Logs\` beside the executable, with the columns MvpPlan-M3.md fixes and each item's live and headless state hash. After the session the logs and the facilitator's reasons are committed under `Design/Review/BreachLab/` |
| 14 | **The camera in the lab** | C3, E2 | It starts at the default azimuth, the south-west, at 84 m of view height on W2, with Hud.md §3's framing offset for the top bar, and the player moves it freely; the log counts turns and zoom notches during a question |
| 15 | **Mouse messages** (ADR-007, decision 4) | E3 | M1's `PointerInput` reads mouse messages, not `WM_POINTER`. M3 keeps that, and the switch comes with the touch scheme after the verdict. ADR-007 is amended to say so |
| 16 | **The v3 package's models in M3** (GameDesign.md, *the MVP assets v3 package*; asked after pull request #44 merged) | B2, B8, C2, C3 | The trebuchet as S3c, cocked, and S3, released, for 20 ticks after it fires; its shot as X1; collapse debris as D1–D6 by rubble, baked from `Manifest.json`'s `debrisByRubble` and scattered as MvpAssets.md says, falling with the collapse for 1.5 s and resting at a quarter turn of yaw. The state update carries each collapse's landing, so that the debris knows where it rests. S3p and the open gates wait for M4 |
| 17 | **The shot between ticks** (against input 12; asked with input 16) | C3 | X1 is drawn between its last two positions at the clock's fraction of a tick, a tick behind, since it moves 10 px or more a tick at the default zoom. Nothing else is interpolated |

**Risks this plan sees** (judgments, beside the plan's risk table):
- **The items may not split 8 and 8 with trebuchet shots alone.** On the 1 m grid a shot takes two voxels and a hole opens the slot above it at 5 wide, so most shot patterns either open the wall at the fifth adjacent shot or never do. Prior damage and shots that leave pillars give the shut side its items; order B7 reports the set, and if fewer than eight shut items are more than trivial, the set shrinks or input 2 changes.
- **Labels in the world may crowd the screen.** The lab's enclosure has ten sections; M4's castle has about thirty (ADR-021, *Left open*). The owner's run after F3 judges ten.
- **The CPU pick and the GPU disagree at a voxel's edge** (ADR-001, *Consequences*), so a click on an edge may pick the neighbour. The lab fires its shots at the mark, so no key depends on a click.
- **Fast-forward meets the tick's cost.** The benchmark's slowest tick of play is 6 ms, and ten times is 200 ticks a second; the clock's cap of eight ticks a frame lets game time slow rather than skip (ADR-013, decision 2).
- **Six owner runs and a session of six people.** Each run may add fix orders, as M2-D3 provided for, and gathering six people is the owner's calendar.

**Done when** all of the following hold:
- **Headless, in CI:**
  - the paper test's 16 states give the paper's readings and keys from the real resolver, in 16 of 16, or the paper's report is corrected to what the resolver does;
  - the lab's 16 items split 8 open and 8 shut by the simulation's own key, each key stable, and the squad reaches the courtyard if and only if the key is open;
  - every query round-trips as bytes, is answered on its tick within the budget, oldest first, and the 65th pending query drops the oldest;
  - the interface pass and the overlay pipeline pass their render tests on WARP;
  - every suite passes, `ClientTests` among them.
- **On the benchmark machine:**
  - `Earthrise --lab` runs free fire, the practice item and all 16 items, and every item's live state hash matches its headless hash;
  - the top bar, the banner, the buttons and the alerts render with no overlap and no clipped text at 1280 × 720 and 1920 × 1080, at each UI scale step;
  - the slowest tick of play across the items, the query budget included, fits M2's 25 ms (GameDesign.md, *M2's values*, item 2).
- **With six people:** the median prediction accuracy on items split evenly between open and shut reaches 0.65 or better. If it falls short, legibility is reworked before M4, and that rework counts toward the one round the kill criterion allows (GameDesign.md, *Kill criteria*).
- **Throughout:** the AGENTS.md checklist passes, and every order that draws or takes input was run by the owner.

**The record.** Each phase's pull request, its commits, one per order, and what CI found.

| Phase | PR | Commits | Notes |
|---|---|---|---|
| Plan | #45 | 3 | This revision, the work orders and ADR-019 to ADR-021, Proposed: first numbered 018 to 020, renumbered when main took ADR-018 for line models (#44), and main merged in. |
| A | #45 | 2 | The owner's 15 answers, every proposal taken, then inputs 16 and 17 after main's v3 package; in the plan's PR because they edit the files it adds. |

**What M3 inherits from M2** (written 2026-11-01, when M2's orders were done; its calendar and owner-hours forecast was dropped on closing M2, GameDesign.md, *No time logging*):
- **M2's shape.** Its 30 orders from B1 to G3 landed in 32 commits; two orders needed a second CI run, both for a lint the container could not reach.
- **What does not carry over.** M2 was headless: every order could be built and tested in a container and confirmed by CI. M3 is a client, and AGENTS.md requires anything that draws or takes input to be run and looked at, which only the owner's Windows machine can do. Each order that draws needs an owner run, and the done-when needs six people (F-48).
- **What sets M3's length** is the owner's runs, about one an order that draws, and gathering six people for the test. The breach M2 found, one slot and a rubble ramp after five shots and 88 s (the [M2 report](Review/M2-2026-11-01.md)), is what M3's players will be asked to predict; the owner judged that it reads as a breach (M2's risks, above), so the rules stand.

### M4 — Siege trial

**When it starts.** M4's revision of 2026-11-01 is Proposed: it is accepted when the owner merges its pull request, and its 28 inputs below wait for the owner's answers (phase A, order M4-A1). It was written before any M3 code, against M3's contracts as [MvpPlan-M3.md](MvpPlan-M3.md) fixes them. **No M4 code is written until M3 closes** (input 2): M3's six-person reading may send the views back for the one round of legibility work the kill criterion allows (GameDesign.md, *Kill criteria*), and M3-G3's account of what M4 inherits corrects this plan's contracts before phase B. Like M3, M4 draws and takes input, so each order that does is run by the owner before its pull request merges (AGENTS.md §3).

**Build** the trial the MVP's second hypothesis is tested in:
- **T1–T7 redrawn natively at 1 m,** T5 with its gate at the inner end of its passage (GameDesign.md, *the paper tests' values*, item 14), and the **Siege castle** as a scenario, a baked source of seeds, structures, earthworks, players and armies ([ADR-026](ADR/ADR-026-scenarios-and-the-siege-trial.md); ADR-014, decision 7), and its mirror, with the weak section on the other flank, for M5's test of H2's clustering clause (input 28);
- **units' statistics, structures' values and the trial's rules as server data,** tuned between playtests without a client build ([ADR-023](ADR/ADR-023-units-and-combat.md), [ADR-024](ADR/ADR-024-structures-at-work.md));
- **formations and separation:** one figure a cell, ranks on open ground and single file through a gap, so that a breach passes figures at a limited rate ([ADR-022](ADR/ADR-022-formations-and-separation.md); ADR-015, decision 6);
- **combat:** hit points and the three damage types, melee, arrows as physical shots with elevation steps, the trebuchet's shot against units and engines, its stone, deploying and packing, and the ram's blows and craters (ADR-023); arrows drawn as line models ([ADR-018](ADR/ADR-018-line-models-and-the-line-pass.md));
- **structures at work:** the built mask, critical parts and working, gates, garrisons, the gatehouse's passage attack, repair at a fixed rate per section with its lock, and sappers' trenches (ADR-024);
- **fog:** visibility by radius, the state filtered per player, chunks sent when seen, last-seen land ([ADR-025](ADR/ADR-025-fog-of-war.md));
- **the trial's rules:** the storming win, the clock and the army rule, and the events its log records (ADR-026);
- **the Siege defender,** a commander inside the simulation that sees through its own fog, fights, repairs, sallies to a leash and shoots sappers, and a scripted attacker for the tests ([ADR-027](ADR/ADR-027-commanders.md));
- **the control:** health-bar structures behind one seam, chosen by the scenario ([ADR-028](ADR/ADR-028-the-control.md));
- **drawing the war:** units, engines, gates, arrows, interpolation, silhouettes and fog;
- **the rest of layout A** ([Hud.md](Hud.md)): the deck, selection, the command card and its modes, control groups, the minimap, the alerts, the repair lock, and every hover reading also reachable without hover (ADR-007);
- **`Earthrise --siege [--control]`,** its log and replay, and a reader for the logs: breach points by role (the nearest section, the weak one, the gate or another), shots, the stone each side spends, the figures through each breach a minute, and whether each breach and each warning began under the deck or off screen and how long the player took to frame it (Hud.md §3). Predictions are M3's lab's (input 24).

### M4's phases (written 2026-11-01, before M3's code)

M4 is carried out as 43 work orders in ten phases, written out in [MvpPlan-M4.md](MvpPlan-M4.md). Each order is one commit; phase A goes in the plan's pull request, and B to J in two draft pull requests, the simulation's and the client's (input 26). An order is a self-contained brief, as M2's and M3's were.

| Phase | Orders | What lands | Needs first |
|---|---|---|---|
| A | 1 | The owner's decisions, written down | Inputs 1–28 below |
| B | 6 | T1–T7 at 1 m; units', structures' and the trial's server data; the castle as a scenario, and its mirror; four icons | A, and M3 closed |
| C | 6 | Commands, players, hit points and damage; formations; melee and orders; arrows; the trebuchet against units and engines; the ram | B3 |
| D | 6 | Working and ruin; gates; garrisons; the passage attack; repair and its lock; trenches | C1, B1, B3 |
| E | 1 | Fog | C1, D |
| F | 5 | The trial's rules; the defender; the control; the scripted attacker and the trial in CI | D, E, B4 |
| G | 3 | `Engine`: the line pass, silhouettes and the column shade, the image atlas | A |
| H | 3 | Units, engines, gates, arrows, fog and silhouettes drawn | F5, G |
| I | 7 | Layout A | H1 |
| J | 5 | `Earthrise --siege`, its log and replay, the control's client, the measured run, the report | F, I |
| | **43 orders, 3 pull requests** | | |

- **B1, B2 and B5 are content** and can land beside B3; **G needs only A.**
- **The headless pull request, B to F, ends with a trial that plays itself** in CI and in `SimulationBench --siege`, both versions, before any HUD exists. That is the checkpoint at which the owner can stop and retune values in data before the client's half begins.
- **The owner runs** fall after B2, B4, B6, H1, H2, H3, I2, I3, I5, I6, J1, J3 and J4, with the playthroughs in J5. That is what sets M4's pace.

**The owner's inputs.** Each is a design value no document gives, or a choice the documents leave to M4, and AGENTS.md requires it written down before the code that rests on it. The implementer proposes each; the owner answers before phase B. Where a proposal takes up a value the paper tests left Proposed, it says so.

| # | Input | For orders | Proposed by the implementer, 2026-11-01 |
|---|---|---|---|
| 1 | Give [ADR-018](ADR/ADR-018-line-models-and-the-line-pass.md) and [ADR-022](ADR/ADR-022-formations-and-separation.md) to [ADR-028](ADR/ADR-028-the-control.md) the go-ahead, or change them, with the amendments they bring: ADR-004 (a second structure rule, for the control), ADR-014 (the replay's header names each seat's commander), ADR-015 (decision 6 carried out by ADR-022), ADR-016 (`UNIT`, `STRC` and `SCEN`), ADR-019 (the image atlas, and a ninth row of glyph cells for 131 glyphs), ADR-020 (the state update per player, interpolation) and ADR-021 (silhouettes, the fog's shade). Each is marked Accepted by the commit that first implements it (AGENTS.md §6) | B3, B4, C1, C2, D1, E1, F2, F4, G1–G3 | Go-ahead for all eight, as written |
| 2 | **When M4 starts** | All from B | After M3 closes, with its verdict in and any round of legibility work done; M3-G3's inheritance corrects this plan's contracts first |
| 3 | **Who redraws T1–T7 natively at 1 m** (MvpAssets.md, *Known issues*) | B1, B2 | The implementer, as layer files under the asset check: each template's outline, parts and openings kept, its volume within 2 % (T3's 96 m³ and T2's 1,650 and 120 m³ exactly), previews for the owner to judge. T5's gate moves to the inner end of its passage, the passage 3 m wide and 4 m high clear before it (the paper tests' item 14, taken up). The owner may instead supply the seven from Claude Design, and B1 and B2 then import them |
| 4 | **The Siege castle** | B1, B4 | The route table's ([Review/PaperTests/SiegeRouteTable.md](Review/PaperTests/SiegeRouteTable.md)): 62 × 62 m over its towers; 22 T3 sections S1–S4, E1–E6, N1–N6, W1–W6; towers SW, SE, NW, NE and ST; GH facing south; the keep and the hall in the bailey; a ditch 4–9 m out and a rampart 1–4 m out, with a causeway before the gate and a gap before E3; 1 m of soil over common rock, deep earth under E3, whose foundation reaches 4 m (item 16, taken up); the camp 100 m south. S3 is nearest and on rock behind the ditch; E3 is the weak section. At the decided 50 m range no stand outside every garrison's reach covers the great tower, with 5.5 m to spare (item 11, taken up as measured on the plan). Placed on the generated MVP map at the flattest 200 × 200 m site clear of the river, levelled |
| 5 | **The armies and their stockpiles** (GameDesign.md, *MVP*: a fixed army; the route table's ambiguity 18: one trebuchet) | B4, F5 | **Attacker:** one trebuchet, packed in the camp, one ram, one squad of sappers, four of men-at-arms, three of spearmen, three of archers and two of outriders, 72 figures; 120 m³ of stone, 80 shots; no timber. **Defender:** six archers in each of the seven garrisons (SW, SE, NW, NE, ST, GH, the keep), 42, as the route table has them; one squad of men-at-arms and one of outriders as the sally and the reserve; six laborers; 200 m³ of stone and 20 m³ of timber. **A shot costs 1.5 m³** (the paper tests' item 17, taken up), so a breach costs the attacker about half the defender's rebuild |
| 6 | **Unit statistics,** vision included (Hud.md §5's placeholders; GameDesign.md, *Open questions*) | B3, C1–C6, E1, I2 | The table below. Counters by arithmetic on it, not measured: six spearmen kill three outriders in about 8 s and lose one or two; three outriders kill six archers in about 12 s; an archer's arrow does 1 to a man-at-arms, so men-at-arms storm; six archers kill five of six spearmen closing 30 m |
| 7 | **Arrows and elevation** (GameDescription.md, *Combat model*) | B3, C4 | An arrow flies straight at 40 m/s from 1.5 m above its shooter's cell, or from a garrison's outer face at its stand height, to 1 m above its target's cell, and hits if the target stands within a cell of that point when it arrives. A step is 3 m of height, at most two, each +15 % damage and a 5 m band of range, the same lost uphill. No bolts: crossbowmen are out of the Trials |
| 8 | **Damage from engines, collapses and falls** (GameDesign.md, *M2's values*, item 8; ADR-015, decision 7) | B3, C1, C5 | A trebuchet's shot deals 40 crush to every unit within a cell of the struck voxel and to an engine whose footprint holds it. A collapse deals 60 crush, once to each unit it strikes. A drop deals 10 crush for each metre beyond 3 |
| 9 | **The ram** (the paper tests' item 16; ADR-006, *Left open*) | B3, C6 | A blow every 15 s (item 16, taken up); craters on GameDescription.md's contact column, on the ladder: timber 4 voxels, earth 1, stone 1 within the bottom metre of the structure and none above (the route table's ambiguity 5). 300 hit points; its roof is 20 pierce armor, its melee armor 0; machines' 0.7 m/s |
| 10 | **Critical parts and garrisons** | B3, D1, D3 | Working while standing: T2's great tower 90 %, T4's firing platform 75 %, T5's gate 50 % (the route table's), T7's gate 50 %, T1's frame 75 %. Garrisons of six in T2, T4 and T5; stand heights 16 m in T2 and 11 m in T4 and T5, so every garrison has two steps (the route table's); garrisoned units heal a hit point every 5 s. Vision 10 m from a structure's footprint, and 80 m from a garrisoned building's, so the defender sees the stands within its sally's leash and not the camp 100 m out |
| 11 | **Repair and the lock** (the route table's repair; GameDesign.md, *the paper tests' values*, item 3) | B3, D5 | 0.08 m³ a second per section, the route table's, which gave 1.50× at the 20 s reload on its own model; on the voxel version's 2 m³ crater it is about 1.7× by arithmetic, inside the Decided 1.3–2×, and D5 measures it. One of the owner's laborers within 3 m on the owner's side; one cubic metre of the voxel's material from the stockpile for each voxel; lowest first, into empty cells only. The lock: 10 s after a shot, a blow or a trench cell within 2 m |
| 12 | **Sappers and trenches** | B3, D6 | 0.2 m³ a second each, twice a laborer's 0.1 (the route table's); spoil leaves play; dug from the top, each layer leaving a ramp at the end nearest the squad; the TRENCH order a box the player drags. At those rates six sappers bring E3 down in about 3.3 minutes on a 4 m foundation (the route table's 198 s), meeting the paper tests' item 15, taken up |
| 13 | **Gates and the passage attack** (GameDesign.md, *the MVP assets v3 package*, item 3; *the design review*, item 9) | B3, D2, D4 | **An open gate is passable to every unit,** not only its owner's: owner-only passage needs navigation per owner, which ADR-015 does not have, and a sally that can be followed in is a real risk the defender takes. The defender shuts it when an enemy soldier comes within 10 m with no sally figure as near. **The passage attack:** 10 crush every 4 s to every unit in the passage outside the gate, while the gatehouse works and holds a garrison |
| 14 | **Fog** (ADR-025) | B3, B4, E1, H3 | Radii from input 6, and structures' from input 10. **The map is revealed to the attacker for the first 2 s,** chunks, structures and readings, never units or garrisons: the castle's plan and every section's integrity reading, kept as last seen, so the choice of breach can be read from the first minute (H2). The shade 0.55, judged on screen |
| 15 | **Storming** (GameDesign.md, *Stages*, 1) | B4, F1 | The keep's foot is the ring of cells within 3 m of its footprint. The clock counts while the keep does not work and the ring holds one of the attacker's soldiers and none of the defender's; it pauses while both stand there, and resets when the ring holds no attacker's soldier or the keep is repaired, a voxel restored or, in the control, a hit point. Two minutes |
| 16 | **The time limit** | B4, F1, J1 | 40 minutes of game time. The army rule stays as GameDesign.md, *Stages*, 1, decides it: lost when no attacking unit is left, engines included, so an attacker left with engines alone plays out the clock |
| 17 | **The trebuchet's deploy and pack, and its packed width** (GameDesign.md, *the MVP assets v3 package*, items 1 and 6) | B3, C5 | 20 s each, one reload. The packed wagon moves as machines, width 3, accepting its 0.25 m overhang either side as ADR-015 accepts the ram's length; the owner may instead have S3p narrowed to 3 m, which changes no code |
| 18 | **The Siege defender** (MvpPlan.md, *In*; ADR-027) | F2, F3 | Every 0.5 s. Garrisons shoot sappers first, then the ram, the trebuchet, then the nearest. Laborers repair the most damaged unlocked structures, one each, nearest first. Archers put out go to the nearest working building with room. The sally goes when an attacker's engine is seen within 70 m of the curtain's outer faces (the route table's leash) and the sally force is at half strength or more, and returns when the engine is dead, the force below half, or any of it past the leash. An opened section draws the reserve to its inner mouth |
| 19 | **The control** (ADR-028) | B3, F4, F5, J3 | One pool of hit points a structure, first values 900 for a T3, the route table's nine shots at 100 a shot, then calibrated in F5: each template's unrepaired shots to open or stop on the voxel version, times 100. A ram's blow 100, a fourth of it on stone; a sapper's trench 4 a cubic metre dug within 2 m of a section. A section at 0 falls whole with its warning; a building at 0 stops and stands; a gatehouse or palisade gate at 0 loses its gate. Repair restores 50 hit points a cubic metre of stone at the voxel version's rate, so a repair window restores the same share of a shot in both versions. The control's HUD shows HEALTH in place of the critical part's row on towers, the gatehouse and the keep, which amends Hud.md §1 and §5, whose list names the towers alone |
| 20 | **Interpolation** (ADR-020, *Left open*; GameDesign.md, *M3's values*, item 12) | H1, H2 | Every unit, engine and shot drawn between its last two positions at the clock's fraction, a tick behind, as M3 draws the shot; a unit that moves more than 1.5 m in a tick snaps, and a shot never does. Cavalry at 2.8 m/s moves about 2 px a tick at the default zoom of about 13 px a metre, and an arrow about 26, so M3's "decide if units stutter" is decided for smoothness |
| 21 | **The HUD's sizes** (GameDesign.md, *Open questions*) | B5, G3, I1, I2, I4 | **The UI scale caps** at the largest step that leaves the deck its 972 logical px: 1 at 1280 px, 1.5 at 1920 px, whatever the DPI; the done-when's "each scale step" becomes "each step the width allows". **Control groups:** Hud.md §3's strip of five slots and SET, 120 × 44 and 64 × 44, so the 44 px hit target holds; groups 6–9 by key alone. **SPLIT, MERGE, DEPLOY and WALL** drawn by the implementer as 9 × 9 icons at U+E019–U+E01C; the owner may redraw them. **Portraits** rendered from each kind's model once at startup into an RGBA atlas; structures as their icon glyphs. The glyph atlas gains a ninth row of cells for the four icons |
| 22 | **The wall section's OPEN row** (Hud.md §5; ADR-021, *Left open*) | I6 | No percentage: there is no formula for one but the Hold rule test's 1 − T ÷ T₀, a trench search M6 builds for the waves. The row reads `OPEN yes` once the section is opened, and otherwise its weak reading against its fall, as `WEAK HOLE 3/5 M` or `WEAK GAP 2/5 M`, within 17 characters |
| 23 | **The views across the castle** (ADR-021, *Left open*: labels for about thirty structures) | G2, H3, I6 | Full integrity labels for the eight sections on screen nearest its centre, name and weak line for the rest, names alone above 150 m of view height; the selected structure always full. Silhouettes for the player's units and visible enemies, in their faction colour at 0.5 alpha |
| 24 | **Predictions for M5** (MvpPlan.md, M5; M4's build list had "predictions" logged) | J2 | M5 asks its prediction items through M3's lab, unchanged, after each participant's voxel trial: the instrument M3 validated, keyed by the simulation, comparable with M3's reading. Prompts inside the trial would need keys resolved on scratch copies of a world with an AI and the player's other units in it, which is a different and murkier question, and three more orders |
| 25 | **The trial's flow and records** | H1, J1, J2 | **Where breaches happen on screen** (Hud.md §3, "M4 measures it"): when a section opens or a collapse starts its warning, the log records whether the structure's centre was in the uncovered area, under the deck, under the top bar or off screen, with the share of its footprint the deck covered, and then the seconds of game time until the player first framed it in the uncovered area; the reader summarises both, so M5 can say whether players lose breaches under the deck. A log and a replay per run in `Logs\`, committed under `Design/Review/SiegeTrial/` after playtests; no save or load offered to players; pause on Pause and F10, keys on no card; `--watch` runs the scripted attacker for the player and needs no participant |
| 26 | **The pull requests** | All from B | Two draft pull requests: B to F, headless and confirmed by CI, and G to J, the client and the owner's runs. M2 and M3 used one for B to G by the owner's choice; at 42 orders after A, one would be hard to review and hides the checkpoint above |
| 27 | **M4's work budgets:** the commanders' work a tick, and any budget combat needs | B3, F5, J4 | Set by the implementer from F5's measurements to fit M2's 25 ms, and recorded as Proposed until J4 measures them on the benchmark. Starting at 32 units a tick for the commanders |
| 28 | **A mirrored castle for H2's clustering clause** (GameDesign.md, *Hypotheses*: breach points that cluster at the nearest section or the gate "whatever the wall and the ground are like") | B6, F5, J1, J2 | With one castle whose weak section is always E3, eight to ten players cannot tell "chose the weak point for stated reasons" from "everyone goes east". **`SiegeMirror`**, the castle mirrored east to west about the site's centre line, puts the weak section on the west flank, W3, with everything else the same: the nearest section, the distances, the armies, the site, which is levelled and authored symmetric about that line. The templates are not mirrored, only placed (ADR-026, decision 1). In the mirror the client starts the camera at the south-east and mirrors the sun's azimuth, so neither layout's weak flank is the lit or near one by default. The log names both roles, so the reader reports breaches as nearest, weak, gate or other in either layout. How participants are assigned is M5's; the implementer's suggestion is the voxel version on one layout and the control on the other, counterbalanced |

**Input 6's table** (Proposed; every number is a first value for M5 to tune):

| Kind | Squad | Hit points | Attack | Reload | Range | Armor, melee / pierce / crush | Speed | Vision | Bonuses |
|---|---|---|---|---|---|---|---|---|---|
| Laborer | 1 | 25 | 2 melee | 2 s | touch | 0 / 0 / 0 | 1.4 m/s | 8 m | digs 0.1 m³/s |
| Spearmen | 6 | 45 | 4 melee | 2 s | touch | 0 / 0 / 0 | 1.4 m/s | 10 m | +12 against outriders |
| Archers | 6 | 30 | 4 pierce | 2 s | 30 m | 0 / 0 / 0 | 1.4 m/s | 32 m | none: their range beats spearmen |
| Outriders | 3 | 60 | 6 melee | 2 s | touch | 0 / 2 / 0 | 2.8 m/s | 16 m | +6 against archers, +20 against the ram and the trebuchet, +4 against laborers |
| Men-at-arms | 6 | 70 | 8 melee | 2 s | touch | 2 / 4 / 0 | 1.4 m/s | 10 m | +4 against spearmen |
| Sappers | 6 | 40 | 3 melee | 2 s | touch | 0 / 4 / 0 | 1.4 m/s | 10 m | dig 0.2 m³/s |
| Ram | 1 | 300 | blows, input 9 | 15 s | contact | 0 / 20 / 0 | 0.7 m/s | 8 m | |
| Trebuchet | 1 | 150 | shot, input 8 | 20 s | 12–50 m | 0 / 4 / 0 | 0.7 m/s packed | 50 m | |

**Risks this plan sees** (judgments, beside the plan's risk table):
- **The balance is on paper, and thin.** The route table modelled no combat (its *Not modelled*), so input 6 is a first guess. At its values an arrow does 1 to a man-at-arms, and the 12 to 18 archers covering each side of the keep's foot take about 15 of 24 men-at-arms over the two-minute hold (arithmetic: 18 arrows every 2 s at 1 each, over 120 s, against 70 hit points). Storming therefore wants the covering towers stopped first, which is the intended play, but F5's scripted attacker may not win with first values. F5 reports, and the fix is a retune in `GameData/Server/`, not code.
- **No first value gives the gate its four minutes** (the route table's ambiguity 3). The ram opens T5's 12 voxels in three blows, 30 s from the first, and the passage attack stops when the gate breaks. D4 measures it; the owner chooses among a thicker gate in T5's redraw, a smaller contact crater in timber, or a passage attack that outlives the gate.
- **The plan is written before M3's code.** Its contracts extend M3's as planned; M3 may change them, and M4 then starts with a correcting commit (input 2).
- **Separation can still jam.** The swap clears two figures head-on; three squads meeting in a 1 m gap can wait on each other until ADR-022's retry search moves one. C2's tests try it, and the breach log shows it in play.
- **Thirty structures' labels may crowd the screen,** and ten were all M3 tried (input 23; I6's owner run).
- **The control's hit points rest on `SiegeAttacker`'s shot patterns.** A player who aims better on the voxel version breaches faster than the calibration; that difference is the hypothesis, not a defect.
- **Thirteen owner runs and the playthroughs,** the most of any milestone, on 43 orders. The headless pull request's end is the place to stop and retune before the client's half.

**Done when** all of the following hold:
- **Headless, in CI:**
  - in both versions, `SiegeAttacker` wins by storming against `SiegeDefender` inside the time limit, and in the voxel version on the mirrored castle too; with no commands the attacker loses to the clock; with every unit spent it loses by the army; each run replays with every state hash equal, and saves at three ticks each continue identically for 1,000 ticks;
  - two squads sent through a 1 m breach pass it with no two units in one cell on any tick, at a rate the trial's events record, and the run replays identically;
  - every command and query round-trips as bytes;
  - the line pass, the silhouettes, the column shade and the image atlas pass their render tests on WARP;
  - the HUD model differs between the versions in the health bars and the critical part's rows alone (input 19);
  - every suite passes.
- **On the benchmark machine:**
  - the owner plays `Earthrise --siege` to a win and to a loss in both versions, and their logs and replays are committed;
  - layout A renders with no overlap and no clipped text at 1280 × 720 and 1920 × 1080 at each UI scale step the width allows (input 21), and on the Surface once one is available (Hud.md §11);
  - over J4's measured run the voxel pass stays within 8 ms at the 95th percentile and the slowest tick of play within M2's 25 ms.
- **Throughout:** the AGENTS.md checklist passes, and every order that draws or takes input was run by the owner.

**The record.** Each phase's pull request, its commits, one per order, and what CI found.

| Phase | PR | Commits | Notes |
|---|---|---|---|
| Plan | | 1 | This revision, the work orders and ADR-022 to ADR-028, Proposed. |

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

**Done when** the trial can be played through to its win rule in both versions, and on a paper re-run of M0's layouts, where the player quarried visibly changes where the waves breach.

### M7 — Hold playtest: the verdict on H1, and on the MVP

**Run** the same protocol on the Hold trial.

**Done when** H1 passes, so Skirmish is planned next, or its kill criterion fires. If it fires, the branch GameDesign.md describes applies, and the owner decides whether Worlds still goes ahead.

## Risks

| Risk | Where it shows | Response |
|---|---|---|
| The renderer misses the pass mark | M1 | Kill criterion: revisit ADR-001 before any gameplay code |
| 1 m craters do not show which weapon made them | M5 (moved from M3 by input 11: M3 has the trebuchet alone) | Tune each weapon's crater radius and depth for each material, the client's debris, and the integrity view |
| Players cannot read breaches | M3, M5 | The single round of legibility work, then the kill criterion |
| The lab's items cannot be split evenly between open and shut | M3 | Prior damage and pillars give the shut side its items; B7 reports the set, and the owner changes input 2 if it falls short |
| The Siege defender is too weak or too strong to test anything | M4 | Scripted behaviors and tuning; it plays one authored castle, not arbitrary ones |
| Combat's first values make the Siege trial unwinnable, or trivial | M4 | Statistics are server data; `SiegeAttacker`'s trial in CI and `SimulationBench --siege` report it before the client's half; a retune is a data change |
| M4 is planned before M3's code | M4 | No M4 code until M3 closes; M3-G3's inheritance corrects the plan's contracts in a commit of its own first |
| The plan overruns | Every milestone | The plan neither forecasts nor logs time (GameDesign.md, *No time logging*); what to cut is the owner's call (*Planning M2*) |
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
