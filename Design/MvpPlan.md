# Earthrise.World — MVP implementation plan

**Status:** Proposed, 2026-10-31. Nothing here is implemented. [GameDesign.md](GameDesign.md) defines the MVP and governs where this plan differs from it. [AGENTS.md](../AGENTS.md) governs how the code is written. Every estimate is the design review's judgment estimate (F-52), in FTE-months for one experienced developer; none is a measurement. The renderer spike's actual time against its own written estimate rescales all of them.

## Scope

### What the MVP proves

The MVP proves pillars 1 and 2. They are stated as H1 and H2 (GameDesign.md, *Hypotheses*), and each is tested against the health-bar control. It ends with that verdict. It proves neither persistence nor networking, but it keeps both possible: the simulation sits behind the server boundary of [ADR-002](ADR/ADR-002-authoritative-simulation.md), and the world is a seed plus edits ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)).

GameDesign.md lists Skirmish as the MVP's third stage, built only once stages 1 and 2 have passed. This plan stops at the verdict on stages 1 and 2. Skirmish gets its own plan once that verdict is in, because its largest item, the Skirmish AI, is research-grade, and building it is only worth doing if the pillars hold.

### In

- **One map**, 512 m × 512 m, generated from its seeds by the integer-only generator and held in memory. The Siege castle and the Hold maps are scenarios, which means saves of seeds, edits and entities.
- **Materials:** earth, common rock, building stone, bedrock, timber, and rubble that keeps the material it fell from.
- **Economy, used by Hold only:** stone and timber, counted by volume. Six designations: quarry, pit, ditch, trench, felling and clearance. Two drop sites, the quarry yard and the lumber camp, with laborers hauling to them.
- **Templates:** hall and keep, palisade, standard stone wall, tower, gatehouse with its passage attack, quarry yard and lumber camp. Ditch and rampart as earthworks. Deep foundations, priced by their volume.
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

  Each trial gives the player a fixed army (GameDesign.md, MVP, Proposed). Combat is stat-based with physical shots. Arrows and bolts get elevation steps, towers take garrisons, climb limits differ by unit class, and units drop when their ground goes.
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
  - the repair lock.
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
- **Final art.** Placeholder art and sound are enough for the playtests.

### Inputs the owner gives before work starts

1. **The spike's pass mark:** target GPU, resolution and frame budget, which are Open in GameDesign.md.
   - Proposed: an RTX 3060-class GPU at 1920×1080, 16.7 ms a frame, of which the voxel pass may take 8 ms at the default zoom.
   - It is written down before anything is measured.
2. **The MVP roster and the fixed armies** (GameDesign.md, MVP, Proposed). Accepting or changing them fixes what M4 builds.
3. **Licensed content.** Any font or sound under a licence needs approval before it lands, with its licence text (R14).

## Architecture

### Solution layout (Proposed; ADR-005 records it with the spike)

| Project | Kind | Namespace | Depends on | Holds |
|---|---|---|---|---|
| `Core` | static library | `Core` | (none) | Integer and fixed-point math, the pinned PRNG, hashing, byte encoding, containers, logging, asserts |
| `Engine` | static library | `Engine` | Core | Win32 window, input and timing; the Direct3D 12 device, frames in flight and the upload ring; pipeline states from embedded shaders; the voxel renderer of ADR-001; text; XAudio2 |
| `Protocol` | static library | `Protocol` | Core | Command types and their byte encoding, state-update value types, handles to chunk snapshots |
| `Simulation` | static library | `Simulation` | Core, Protocol | The world, generator, edits, support search and resolver, slump, navigation, units, combat, designations and jobs, templates, repair, AI, the fog filter, saves, replays and the tick loop |
| `Client` | static library | `Client` | Core, Engine, Protocol | Camera, input mapping, selection and orders, interface and overlays, presentation of state updates, cosmetic debris |
| `Earthrise` | executable | (none) | all libraries | `wWinMain`, which joins the client and the simulation through an in-process transport |
| `RendererSpike` | executable | `RendererSpike` | Core, Engine, Simulation | The spike's scenes and measurements, kept afterwards as a rendering benchmark |
| `SimulationBench` | console executable | `SimulationBench` | Core, Protocol, Simulation | Headless measurements: spike question 6, the synthetic 2 km world, regression timing |
| `CoreTests`, `ProtocolTests`, `SimulationTests` | test projects | `<Project>Tests` | their library | Unit tests, run by vstest, each starting with `SuiteSmoke` |

The edges run one way:
- `Simulation` includes nothing from `Engine` or Direct3D, so it builds headless.
- `Client` includes nothing from `Simulation`. It sees only `Protocol` (ADR-002, decision 3).
- `Engine` knows voxels, not the game. Materials reach it as palette indices (R9).
- Each project gets its own folder, flat, and is added to `.clang-tidy`'s `HeaderFilterRegex` in the commit that creates it.

### Decisions to record as ADRs, and when

| Decision | Taken in | Proposal |
|---|---|---|
| Solution layout | M1, first commit | The table above |
| Render and simulation voxel sizes, camera projection | End of M1 | Measured by the spike |
| Lighting | End of M1, only if spike question 4 falls short | Traced shadows through an occupancy volume, or DXR |
| Tick rate and number representation | Start of M2 | 20 ticks a second; integer voxel coordinates; fixed-point positions for units and shots |
| Chunk dimensions and edit encoding | M2 | 32³ voxels; a sparse list of edits, replaced by a full copy past a threshold |
| Navigation model | M2 | A height field of 2.5D tiles, a clearance grid for each class (foot, cavalry, machines), and a dirty tile rebuilt at most once a tick |
| Template format and how templates are authored | M4 | Voxel layers with material and critical-part tags, written by a Python tool under `Tools/` |
| Text rendering and how units are drawn | M4 | A bitmap font atlas baked by a tool; units as small oriented voxel models drawn through the ADR-001 pipeline |

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

## Milestones

| | Milestone | Ends with | FTE-months |
|---|---|---|---|
| M0 | Foundations and paper tests | Checkers in CI; the revised rules pass on paper; the pass mark and the spike's estimate written down | 0.5–1 |
| M1 | Renderer spike | An ADR on voxel sizes and projection, with measured figures | 6–9 |
| M2 | Headless simulation | A scripted breach and units pathing through it, replaying identically | 8.5–15 |
| M3 | Breach lab | The first measured reading on H2's predictions | 1–2 |
| M4 | Siege trial | A Siege trial that can be played, with its control | 11–20 |
| M5 | Siege playtest | The verdict on H2 | 1–2 |
| M6 | Hold trial | A Hold trial that can be played, with its control | 5–9 |
| M7 | Hold playtest | The verdict on H1, and so the MVP's | 1–2 |
| | **Total** | | **34–60** |

The total is a little above the build budget's 32–56 for this stage (GameDesign.md, *Build budget*). The difference is the breach lab and the split into two playtests, which buy a verdict on H2 before the economy is built. M0 to M5 come to 28–49 FTE-months. At 20 hours a week, half of full time, that puts the verdict on H2 about 4.5–8 years in, and the MVP's verdict about 5.5–10 years in. The spike's ratio will move both.

**The critical path** runs M1 → M2 → M4 → M5. The paper tests in M0, the template tool and the placeholder art can run alongside it.

### M0 — Foundations and paper tests

**Build:**
- `Build/CheckFormat.py`.
- `Build/CheckProjectFiles.py`. It checks R2 affixes, R7 names and project registration, R11 spellings, flat folders, shader names, functional filters, and that Debug and Release agree (AGENTS.md §3).
- `Build/RunClangTidy.py`.

Each CI guard is removed once its input is there.

**Run on paper, before any gameplay code** (review, *Cheapest tests*):
- the Hold rule test with three quarry layouts (F-45);
- the breach-prediction prototype comparing a thickness readout with a support readout (F-04, F-47);
- the Siege castle's route table and range circles (F-01, F-02);
- the repair spreadsheet (F-03).

**Write down** the spike's pass mark and the spike's own estimate.

**Done when:**
- the checkers run in CI;
- under the revised rules, the breach moves with where the player quarried in at least two of the three layouts;
- the support readout reaches a median accuracy of 0.65 or better;
- no route dominates in the table.

### M1 — Renderer spike

**Build:**
- **The engine base:** the window, input, a timer, the device and DXGI factory, a flip-model swap chain, two or three frames in flight with fences, descriptor heaps, an upload ring, and barriers written by hand.
- **ADR-001's pipeline:** quad bounds in the vertex shader, the ray-box test in the pixel shader, conservative depth, stochastic pruning on a key that survives edits, and shadow maps.
- **The test scene:** terrain, a dense forest, a walled town, a ditched wall and a terraced pit, with stand-ins for units at their simulated size.
- **Scripted edits:** quarrying at the peak rate back-solved from the tempo, and one wall collapse.
- **In `SimulationBench`:** a first support search and a navigation tile rebuild, run on the MVP map and on a synthetic 2 km world carrying a season's edits.

**Done when** the spike answers questions 1–6 with figures measured on the stated hardware, and two ADRs record the choices: one for the solution layout, one for the voxel sizes and projection. If no candidate size meets the pass mark, ADR-001 is revisited before any gameplay code, which is the kill criterion. The spike's estimate is then compared with its actual time, and this plan is rescaled.

### M2 — Headless simulation

**Build:**
- the tick loop, commands as bytes with a round-trip test for each, and entity storage;
- the chunk store, edits and copy-on-write snapshots;
- the integer-only generator, with golden hashes and the slope invariant in CI;
- the materials, and save and load;
- a replay test;
- the support search, the collapse resolver, slump and the rule for loose material;
- navigation, with climb limits for each class and the drop rule.

| Part | FTE-months |
|---|---|
| Generator | 1.5–3 |
| Chunks, edits, saves, replays | 2–3 |
| Support search and resolver (research-grade) | 3–5 |
| Navigation (research-grade) | 2–4 |

**Done when,** with no window open, a scripted trebuchet breaches a test wall, the wall collapses with its warning, its rubble keeps its material, units are ordered inside and path through the breach, and the whole run replays tick for tick. The support search and the tile rebuilds must fit the budget the spike measured.

### M3 — Breach lab

**Build** a thin client:
- the camera;
- click-to-fire a trebuchet at a wall;
- the integrity view read from the resolver;
- the collapse preview and the passability view;
- prediction prompts, logged.

Run it with six people (F-48).

**Done when** the median prediction accuracy on items split evenly between open and shut reaches 0.65 or better. If it falls short, legibility is reworked before M4. That rework counts toward the one round the kill criterion allows (GameDesign.md, *Kill criteria*).

### M4 — Siege trial

**Build:**
- units, stats and combat, physical shots, elevation, garrisons and the gatehouse's passage attack;
- drawing units and buildings;
- fog with the visibility filter and last-seen land;
- the template format, its authoring tool and the Siege castle;
- the subset of jobs the trial needs: repair and trenches;
- the rest of the interface: selection, control groups, the command card, the minimap, the overlays, alerts, the attack cursor and the repair lock;
- the Siege defender AI;
- the storming win and the loss rule;
- the control version of the trial;
- logging of breach points, shots, the stone spent by each side, and predictions.

| Part | FTE-months |
|---|---|
| Combat | 2.5–4 |
| Units and buildings drawn | 1.5–3 |
| Fog | 1–1.5 |
| Templates and the tool | 1.5–3 |
| Repair and trench jobs | 1–2 |
| Interface | 2–3.5 |
| Siege AI | 1–2 |
| Control | 0.5–1 |

**Done when** the trial can be won by storming and lost by the clock or by running out of attackers, the control plays the same in everything but the wall rule, and the AGENTS.md checklist passes, including running the executable.

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
- the generator constraints for Hold maps: at least two outcrops, and mixed ground;
- the scored waves and the sapper wave;
- the control's node resources;
- the Hold content.

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
| The support search or navigation is too slow at the chosen simulation voxel size | M1, M2 | A coarser simulation voxel at an integer ratio to the render voxel (GameDesign.md, Decided 5) |
| Players cannot read breaches | M3, M5 | The single round of legibility work, then the kill criterion |
| The Siege defender is too weak or too strong to test anything | M4 | Scripted behaviors and tuning; it plays one authored castle, not arbitrary ones |
| The plan overruns | Every milestone | The spike's ratio rescales everything; the cut order in GameDesign.md applies past Skirmish, and within the MVP the trebuchet, the gatehouse attack and the breach lab are the last things to cut |
| Content takes longer than code | M4, M6 | The template tool is written first; one castle and two Hold maps only |

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
