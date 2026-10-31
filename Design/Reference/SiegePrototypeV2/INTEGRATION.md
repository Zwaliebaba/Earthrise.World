# Integration guide for Claude Code

You are integrating the Look v2 package into the Earthrise.World repository. Work through the phases in order. **Before you start, read `AGENTS.md`, `Design/GameDescription.md`, `Design/GameDesign.md` and `Design/MvpPlan.md`.** `AGENTS.md` governs how code is written and named, and how projects are registered. Where this guide conflicts with it, `AGENTS.md` wins; say so in your summary.

Phases 1–2 are design documents only, and the owner must approve them before Phase 3. Phases 3–6 attach to milestones in `MvpPlan.md`, and nothing is built ahead of its milestone.

---

## Phase 0 — Place the package

1. Copy the package into the repo as `Design/LookV2/` (or whatever path `AGENTS.md` prescribes for design assets).
   - Keep `assets/`, `specs/`, `reference/`, `manifest.json` and `palette.json` together.
   - `.vox` and `.png` are binary, so check `.gitattributes`; add LFS or binary rules if `AGENTS.md` asks for them.
2. Copy `commands/integrate-look-v2.md` to `.claude/commands/`.
3. Do **not** copy `reference/*.dc.html` into any build. They are design references, not code.

**Done when:** the files are in the repo, nothing outside `Design/` and `.claude/` has changed, and the CI checkers (`Build/CheckFormat.py`, `Build/CheckProjectFiles.py`) still pass.

---

## Phase 1 — Record the decision (design docs, owner approval)

1. Copy `adr/ADR-006-voxel-sizes.md` to `Design/ADR/`.
   - Renumber it if 006 is taken, and follow the existing ADR template and headings.
   - Status stays **Proposed**. The M1 spike is what accepts or rejects it.
2. Apply `design-changes.md` to `Design/GameDescription.md` and `Design/GameDesign.md`.
   - Each change is listed as current text → proposed text.
   - Where GameDesign.md records decisions, add this one as **Proposed**, not Decided.
3. Update `Design/MvpPlan.md`:
   - **Architecture › Decisions to record:** "Render and simulation voxel sizes" now has a baseline proposal of 1 m world = simulation, 0.5 m models, with ADR-006 as the link.
   - **Risks:** the "coarser simulation voxel" response is now the baseline. Add a new risk: "1 m craters do not read which weapon made them" → response: tune crater radii, client debris, the integrity view.
   - **M1 test scene:** add "units and engines as 0.5 m models over the 1 m world, with contact AO".
   - **M1 done-when:** the spike measures **both** the 1 m + 0.5 m models baseline **and** a 0.5 m world, and ADR-006 records the figures.

**Done when:** the owner has approved the diff. Stop and ask before going further.

---

## Phase 2 — Make the look spec canonical

1. Put `specs/Renderer Look Spec v2.md` where design specs live (for example `Design/RendererLookSpec.md`), and link it from ADR-001 and from MvpPlan M1.
2. Any earlier look spec (v1, 0.5 m world) is superseded. Remove it, or mark it superseded at the top.

---

## Phase 3 — M1 renderer spike (Engine, RendererSpike)

Build to the spec. The points that differ from a single-grid renderer:

1. **Two voxel sources in one pipeline (ADR-001).**
   - **World chunks** use 1 m voxels and come from Simulation snapshots.
   - **Model instances** use 0.5 m voxels: a small voxel volume plus a transform (position, 90° yaw).
   - Both go through the same ray-box pass with conservative depth.
   - `Engine` knows voxels and palette indices only (R9). It does not know what a unit is.
2. **Palette.** Load `palette.json` at build time into an embedded table, with indices exactly as listed. Convert sRGB to linear before lighting.
3. **Jitter.** Use the hash in spec §4. The world cell is a world voxel and the model cell is a model voxel. It must be deterministic and stable under edits.
4. **AO.**
   - The world uses the AO tables in spec §5, with the terrain set for indices 1–5 and 13–15.
   - **Model AO also samples the world:** a neighbour is solid if the model voxel is set, or the world voxel at `floor(p_model_world / 1 m)` is set. This is the contact shading that keeps units from floating; it is acceptance check 5.
5. **Shadows.** World shadows update lazily, on edit or sun change. Models cast and receive every frame.
6. **LOD.** When a model voxel drops below about 3 px, draw units as a 1 × 1 × 2 impostor (spec §7).
7. **Test scene.** Build it from `assets/` (spearmen, archers, a trebuchet, a ram) on terrain with a walled town. `reference/Earthrise Siege Scene v5.dc.html` is the target look. Match it by eye, not pixel for pixel.
8. **Measure the alternative too.** Run the same scene with a 0.5 m world, for example by doubling a world chunk, so ADR-006 can be decided on numbers.

**Done when:** spec §8 checks 1–7 pass on the stated hardware, and the figures for both configurations are in ADR-006.

---

## Phase 4 — M2 headless simulation (Simulation, Protocol)

1. **The voxel size is 1 m.** Integer voxel coordinates in metres, chunk 32³ = 32 m, as MvpPlan proposes. This is ADR-002/003 compatible: the world is still seed + edits.
2. **Materials.** Simulation materials are only: earth, common rock, building stone, bedrock, timber, rubble(from). `palette.json › simulationMaterial` gives the mapping from every **world** palette index to a simulation material.
   - Grass is drawn as the top of earth.
   - Dressed stone and openings are building stone as built.
   - Roof is timber.
   - Rubble keeps `from`.

   Put the mapping in `Protocol` or `Client`, which pick the palette index for presentation. **Never** put render indices in Simulation rules.
3. **Model-only indices** never appear in Simulation: faction, iron, laborer cloth, counterweight, skin, cloth, horse, leather. Units and engines are entities with fixed-point positions.
4. **Cost.** Cost = counted 1 m voxels × 1 m³. Recompute it from the template; never copy it from the manifest.
5. **Tests in `SimulationTests`:**
   - the T3 standard wall costs exactly **96 m³** of stone;
   - every template with `criticalPart` has a non-empty part-1 set;
   - no template contains a model-only index;
   - a scripted trebuchet breach on T3 leaves rubble(from: building stone), replays tick for tick, and its rubble ramp is walkable for the foot class.

---

## Phase 5 — M4 templates and drawing

1. **The template tool (`Tools/`, Python)** reads `assets/world/**/**.json`:
   - `layers[y][z]` is a string over x, one character per voxel, and `legend` maps characters to palette indices;
   - `parts[y][z]` uses `1` critical, `2` foundation, `3` stock (not costed), `.` none;
   - y is up, +z is the front, and the origin is the min corner.

   It writes whatever runtime format the template ADR chooses, applying the simulation-material mapping above.
2. **Redraw the world templates natively at 1 m**, in MagicaVoxel from the `.vox` files or in the tool. The 1 m files here are automatic conversions; keep the same dimensions so costs stay valid. Check the battlement rhythm, door and slit placement, and the tower and keep wall thickness (2 m).
3. **Unit and engine models.** Load `assets/models/**/**.vox` or `.json` as model volumes.
   - Each U file is a squad (6, or 3 riders). Split it into single-figure models, cropping one figure plus its prop.
   - The trebuchet arm pivots at the axle (0.5 m model voxels, axle at 6.5–7 m). Animate it by swapping between baked poses: cocked, released, packed.
4. **Faction.** Index 16 is swapped per faction at draw time.

---

## Phase 6 — Verify against the reference

- Open `reference/Earthrise Siege Scene v5.dc.html` and put the engine's view at the same framing beside it: default azimuth −45° and about 84 m view height. Note differences in the spike report. Don't chase pixel parity.
- Regenerating assets: open `reference/MVP Asset Sheet.dc.html` (mode "1 m world + 0.5 m models") and re-export. Builders live in its logic class, `builders()`. Once the template tool is authoritative, edit there and stop using the sheet.

---

## Decisions made in this package (confirm with the owner)

1. **Wall thickness:** standard **2 m**, thick **3 m** (1.5× cost).
2. **Proportions** of the hall, keep, gatehouse and drop sites. **Critical parts** for the hall ("hall frame") and drop sites ("shed").
3. **Towers and keep are hollow,** with 2 m walls and floors.
4. **Ditch:** 4–5 m wide and 2 m deep. **Rampart:** 2 m high.
5. **Unit cues** at 0.5 m: spear tip, hood + bow, iron + shield, raised pick, horse.
6. **Render-only colours** for horse, hides, timber rubble and earth rubble.
7. **Model debris over rubble** is deterministic from the collapse id, and purely cosmetic.

## Summary to write when done

List the files changed per phase, the ADR number used, any `AGENTS.md` conflicts, and the open items from the list above.
