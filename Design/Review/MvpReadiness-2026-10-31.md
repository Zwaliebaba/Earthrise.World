# MVP readiness review, 2026-10-31

**Question:** is the design ready for MVP implementation to start, and what is still unclear or open?

**Method:** three independent read-only reviews, each with one lens:
1. contradictions and stale text across the design documents, the ADRs, AGENTS.md, `GameData/` and CI;
2. whether each milestone has what it needs to start, deepest for M0 and M1;
3. an inventory of every Open, Proposed, placeholder and deferred item.

The findings were then checked against the files; each section below cites its locations. The open-items inventory counted 68 items: 9 before M0/M1, 35 before a later milestone, 9 tuning values and 15 after the MVP.

**Verdict: not ready to start.** Nothing found reopens a pillar or a major decision. What stops M0 and M1 is narrower:
- **Approvals:** the plan itself is still Proposed.
- **Sequencing:** M1 leans on work the plan schedules for M2 and M4.
- **Specifications:** the pass mark, the test scene and the paper tests are not specified enough to execute or measure.
- **Stale text:** about twenty statements contradict later decisions.

## A. Owner decisions that block M0 and M1

1. **The plan is still Proposed.**
   - MvpPlan.md is "Status: Proposed".
   - So is the rest of what the spike answers (GameDesign.md, *Renderer spike*).
   - So are the kill criteria, including the spike's own (GameDesign.md, *Kill criteria (Proposed)*).
   - AGENTS.md forbids building on anything Proposed.
   - **To decide:** accept MvpPlan at least through M2, the spike's six questions, and the 1 m kill criterion.
2. **The pass mark cannot be measured as written,** and it is the kill criterion. It does not say:
   - whether the RTX 3060-class GPU is in a desktop or a laptop;
   - whether "the voxel pass" includes the two shadow passes;
   - which statistic counts (mean or 99th percentile), from which viewpoint and rotation;
   - whether units and edits are running during the measurement;
   - which Surface Pro model is used, at which internal resolution and aspect (its panel is 3:2).

   Nothing records that the owner has an RTX 3060-class machine and a Snapdragon X Surface Pro to measure on.
3. **The test scene is not ready for a 1 m grid.** `Reference/SiegePrototype/Siege Scene Prompt.md`, the layout M1 cites, was written for 0.5 m:
   - "Voxels: 0.5 m for structures …";
   - walls "1.5 m thick", 0.5 m merlons, a "1.5 m high" rampart;
   - a curtain on "x 58–91" (33 m), which does not divide into 8 m T3 sections and 7 m T4 towers;
   - terrain from unseeded float noise ("fbm").

   GameDesign.md asks for a generated scene with "a town with walls". `GameData/` has no trees, cottages, mantlets, banners, stock stacks or debris.

   **To decide:** which scene, who lays it out again on 1 m, and who draws the missing assets.
4. **The M0 paper tests have no inputs.**
   - **Breach prediction:** the stone span is a tuning value (ADR-004), the anchor depth is "a depth the generator fixes" with no value, and N per question is unset (GameDesign.md, *Hypotheses*).
   - **Hold rule test:** the waves' scoring has no weights, no size for "the best few", and no tie-break.
   - **Route table:** there is no Siege castle before M4.
   - **Repair spreadsheet:** it has no pass mark in M0's done list; the review gave 1.3–2×.

   **To decide:** provisional values marked Proposed, or permission for the implementer to choose them and record the choice.
5. **R14 needs two rulings.**
   - **The test framework:** vstest's native C++ framework (CppUnitTest) ships with Visual Studio. It is neither the Windows SDK nor the MSVC standard library, which is R14's closed list.
   - **Python in the build:** the content bake runs Python as a build step, while AGENTS.md §2 says "The build depends on the Windows SDK and the MSVC standard library, and on nothing else."
6. **The spike's estimate needs writing down,** along with a time log, so that the ratio of actual time to the estimate can rescale the plan (GameDesign.md, *Build budget*).

## B. Sequencing defects in the plan

1. **M1 needs work that is scheduled later.**
   - **Chunks:** the spike draws "world chunks of 1 m voxels from the simulation's snapshots" (MvpPlan.md, M1). The chunk store, snapshots and generator are M2 work, and chunk size is an M2 decision, though ADR-006 already assumes 32³.
   - **The bake:** the scene is built from `GameData/` through `Content`, but "the bake tool lands in M4" (ADR-005, ADR-008).
   - **ServerData:** `RendererSpike` links `Simulation`, which reads `ServerData`, whose format comes with the M4 bake.
   - **To decide:** pull minimal versions into M1 and move their estimates, or give the spike stand-ins and record them.
2. **ADR-006 cannot be accepted at the end of M1.**
   - Its acceptance needs "the M3 prediction accuracy reaches a median of 0.65", and its table has a 0.5 m cell for M3 that nothing will fill.
   - MvpPlan.md still schedules the acceptance for "End of M1".
   - **To fix:** accept it after M3 and drop the 0.5 m cell from the M3 row.
3. **ServerData is read in M2 but specified in M4.**
   - **To fix:** move the server half of the bake into M2, and have CI run it before the simulation tests.
4. **Generator parameters versus ADR-003.**
   - ADR-008 puts "generator parameters" in ServerData. ADR-003 makes the unedited world "a pure function of two seeds, the generator version and the chunk coordinate".
   - **To decide:** whether the parameters are part of the generator version. The golden hashes depend on it.
5. **The HUD's resolver queries have no channel.**
   - Hud.md sends readouts "through `Protocol` queries". ADR-002 lets only commands and state updates cross the boundary.
   - **To fix:** an ADR for a query channel whose answers pass the fog filter, before M3.
6. **M1's done-list includes work that is not in its build list.**
   - RendererLook.md's checks 3 and 6 need model debris keyed on a collapse id.
   - Check 8 needs a shallow-water rule that is not written.
   - No check covers shadows, so "if question 4 falls short" has no criterion.
   - Units are drawn in M1, but "How units are drawn" is an M4 decision.
7. **The trebuchet is both static and posed.**
   - Static: ADR-005 ("MVP engines are static models") and MvpAssets.md ("A static model in the MVP").
   - Posed: ADR-005 decision 8, ADR-006, RendererLook.md ("cocked, released, packed") and MvpPlan.md ("Engine poses are baked"). The HUD also shows packing and deploying states.
   - **To decide:** for M4.
8. **CI has no asset-check step,** although ADR-005 says `Tools/CheckAssets.py` "runs in CI, guarded on the script existing".

## C. Engineering decisions for M1's first commit

These go in the solution-layout ADR, or ADRs beside it.

- **Projects:** which exist in M1, their names, and a shared `.props` or not.
- **Headers:** a precompiled header per project, and which header owns the Windows macros (AGENTS.md §4 names none).
- **The output directory:** `$(SolutionDir)$(Platform)\$(Configuration)\`, which CI's test step already assumes (`build.yml`). That fixes ADR-008's `<outputdir>`.
- **The clang-tidy command:** the one documented in `.clang-tidy` passes `/DWIN32_LEAN_AND_MEAN /DNOMINMAX`. That collides with the rule that one header owns those macros, and `WarningsAsErrors: '*'` makes it fatal. Also, `HeaderFilterRegex` is unanchored, so generated headers in an intermediate directory would be linted.
- **The renderer choices R12 and ADR-001 leave open:**
  - the window style;
  - the swap chain and its sRGB view;
  - the render resolution and scaling filter (ADR-007);
  - the depth convention;
  - how quads are issued;
  - MSAA;
  - the shader compiler and model: fxc stops at Shader Model 5.1, and dxc gives 6.x.
- **The checker specification.** As written, `CheckProjectFiles.py` checks only that configurations agree, so two equally wrong configurations pass. It must also assert the mandated values: v145, `/std:c++latest`, `/permissive-`, `/W4`, `/WX`, `/fp:precise`, and each architecture's `/arch`. On an empty tree all three checkers pass trivially, so they need test fixtures.
- **How the work is verified.** Cloud sessions run on Linux. CI's Windows jobs can compile Debug|x64 and Debug|ARM64, but every "run, not just built" check of rendering falls to the owner, on Windows with a GPU.

## D. Stale or contradictory text

These are mechanical fixes, except where marked as the owner's choice.

- **Stochastic pruning:** ADR-001 decision 3 (Accepted) prunes stochastically, and AGENTS.md §3 asks that pruned voxels be checked for shimmer. RendererLook.md, ADR-006, GameDesign.md and MvpPlan.md say pruning is not used.
- **Contraction:** ADR-001 and ADR-003 still say `/arch:AVX2` contracts into FMA, contrary to the corrected R16.
- **Stale left-open entries:** ADR-001 still leaves "the size of a voxel" open, and ADR-003 still leaves the entity-state format open, which ADR-005 closed.
- **Skirmish (the owner's choice):** GameDescription.md says "one to three AI houses", against GameDesign.md's "One or two AI factions".
- **Interface sizes:** ADR-007's "sizes are physical" against Hud.md's stepped scale of 1, 1.5 or 2.
- **The palette:**
  - RendererLook.md marks iron (18) `models`, where Palette.json says `both`; under RendererLook's value, T7's iron fittings would be invalid.
  - Faction and cloth are each "a decoration in a template", yet faction colour is "never on structures".
  - ADR-005 decision 6 still lists banners as decorations, though banners are now models (ADR-006).
- **Content text from before ADR-008:**
  - GameDesign.md's content decision of 2026-10-31 still says "MagicaVoxel copies beside them … nothing is loaded at runtime", and its next section still says `Assets/`.
  - The `.gitattributes` comment still says the `.vox` must agree with its JSON.
  - ADR-005's "what this forecloses" mentions only the server's data, though ADR-008 also allows audio.
- **MvpPlan.md's materials** leave out grass, leaves and water, though M1's scene has a forest.
- **F1 and F2:** MvpPlan.md M3 toggles "the collapse preview and the passability view" with them; Hud.md uses them for the integrity and passability views.
- **The first interface:** ADR-007 says the hover rule binds "from the first interface (M4)", but text and the first HUD are now M3.
- **Touch:** two starting points are named for its layout, the HUD package's and the second asset package's.
- **Redraws:** T8 and T9 are redrawn in M4 by MvpPlan.md and "before M6" by MvpAssets.md.
- **Asset data:**
  - The template files' own `costM3` holds the package's figures (T1 185, T2 1718, T4 477, T7 28), not the recount in Manifest.json (188, 1722, 479, 32).
  - Model files carry `costM3`, though models set no costs.
  - GameData/README.md describes `BakeContent.py` in the present tense, and it does not exist yet.
- **The build budget:** its arithmetic does not close. 32–56 + 44–76 is 76–132, while the text says 79–137 less cuts of 3–6. GameDesign.md says "5–9 years", against MvpPlan.md's 5.5–10.
- **Section names:** Hud.md says saves keep a section's name under ADR-005 decision 10, which lists no name.
- **Small wording:** AGENTS.md says "the four rules in its row" where the row lists six, and "Neither checker exists yet" beside ".clang-tidy is configured". There is no R13.
- **Disk:** an empty, untracked `Assets/` folder remains on disk.

## E. Decisions due before later milestones

- **M2:**
  - acceptance of the proposals for tick rate, numbers, chunks and navigation;
  - the command byte encoding;
  - the pinned PRNG and hash;
  - the resolver's parameters: spans, warning time, fall length, the loose material's set height, the anchor depth, and the slump neighbourhood;
  - climb limits, clearance per class, and ford depth;
  - a per-tick work budget in units of work.
- **M3:**
  - N per prediction item;
  - six participants;
  - the query channel (B5);
  - the legibility kill criterion.
- **M4:**
  - unit statistics, ranges and vision;
  - a shot's stone price, critical-part thresholds and the repair rate;
  - the Siege castle and the army's composition;
  - the time limit and the sally leash;
  - Hud.md's Proposed items;
  - native redraws of T1–T7.
- **M5 and M7:**
  - the playtest protocol (Proposed);
  - recruiting 8–10 RTS players.
- **M6:**
  - the length of the build window;
  - the waves' composition;
  - dig and haul rates;
  - GATHER;
  - the redraws of T8 and T9.

## F. Tuning, and after the MVP

**Tuning,** nine items the trials are meant to set: unit statistics and costs, a shot's price, critical-part thresholds, the repair rate, collapse timing, material spans, crater sizes, the balance targets, and the colour of rock rubble.

**After the MVP,** fifteen items:
- the Skirmish AI;
- touch and pen;
- Android, and ADR-001 on mobile GPUs;
- colour-blind palettes;
- the sizes behind the Worlds rules;
- the online gates;
- networking;
- streaming;
- new server data during a season;
- palette indices for later materials;
- engine animation;
- the Earthstone race.

## Resolution, 2026-10-31

- **A, the owner's decisions:** settled (GameDesign.md, *Starting the MVP*):
  - the plan is accepted through M2;
  - the pass mark's measurement is defined;
  - the RTX 3060 machine is available, and the Surface is not yet;
  - the v5 layout, with placeholder models;
  - CppUnitTest and Python are allowed;
  - the paper tests' values are picked by the implementer, as Proposed.

  The spike's estimate is still to be written in M0. Also decided: Claude sessions write the code and the owner runs and judges it, the trebuchet switches between baked poses, and Skirmish has one or two AI houses.
- **B, the sequencing defects:** all applied. M1 gains:
  - the chunk store;
  - a scene script;
  - the client bake and the asset check;
  - units and debris;
  - the water rule and the shadow check.

  Also applied: ADR-006 is accepted after M3, the server bake moves to M2, generator parameters are compiled into the generator, ADR-009 adds queries, and CI runs the asset check.
- **D, the stale text:** fixed. AGENTS.md's missing R13 is left as it is, since the numbering came over from the sibling repositories.
- **Still open:**
  - C, the engineering decisions for M1's first commit, to be drafted as an ADR before that commit;
  - E and F, which are due later or after the MVP.
