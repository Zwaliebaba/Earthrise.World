# Breach prediction: a paper test

**Status:** Built 2026-10-31, not yet run. Every value below is **Proposed**. The implementer picked it under [GameDesign.md](../../GameDesign.md), *Starting the MVP*, decision 7, and the owner confirms or changes it after the results. No participant has taken the test, and nothing here reports a result.

This is the design review's cheapest test 3 ([GameDescription-2026-10-31.md](../GameDescription-2026-10-31.md), F-04 and F-47), one of the M0 paper tests ([MvpPlan.md](../../MvpPlan.md), *M0*).

**The question.** Can players predict which damaged wall sections are open better from a readout derived from the support rule than from a thickness readout?

**The pass mark.** Participants shown the support readout reach a median accuracy of 0.65 or better, on items split evenly between open and shut. Six people is the floor for that group (ambiguity 8).

## Files

| File | What it is |
|---|---|
| `Tools/PaperTests/BreachPrediction.py` | The model, the resolver's stand-in, the 16 states, both readouts and the answer key. Python 3, standard library only. |
| `Tools/PaperTests/BreachPrediction.json` | What the script writes: each state as shown and as settled, both readouts, the key, the robustness checks and the scoring arithmetic. |
| `Tools/PaperTests/BreachPrediction.html` | The page a facilitator runs with each participant. Self-contained; the script writes the items into it. |

To regenerate both files after changing a value, run `python Tools/PaperTests/BreachPrediction.py`. With `--check`, it exits 1 if either file is stale. The script refuses to write unless the 16 states split 8 open and 8 shut, and it does: **8 open, 8 shut.** The JSON's `version`, currently `cae945ae93d9`, is a hash of its content. Each participant's results file records the version they took.

## The model

One standard wall section on flat ground, in 1 m voxels ([ADR-006](../../ADR/ADR-006-voxel-sizes.md)).

- **The section is T3** (`GameData/Server/Templates/earthrise-t3-stone-wall-standard-1m.json`), copied into the script so that a redrawn template does not move the instrument. It is 8 m long, 2 m thick and 7 m high: 5 m solid and 2 m of battlements, 96 voxels of masonry. The script says if the template file stops matching its copy.
- **Its neighbours.** One column of each neighbouring section stands at each end, intact. Support runs into them as it would along a real wall line ("Sections join end to end", GameDescription.md:170), so a neighbour bridges a hole at the end of the section.
- **The ground**, from the bottom: 1 m of bedrock, 1 m of common rock and 2 m of earth. The wall stands on the earth with no foundation of its own, so it can be undermined. There are 7 m of ground on each side of the wall.
- **Materials** are the palette's (`GameData/Server/Palette.json`): masonry 6, earth 2, common rock 3, bedrock 5, stone rubble 8.

### Proposed values

| Value | Proposed | Reason |
|---|---|---|
| Stone span (masonry) | **2 m** | "Stone spans a little" (GameDescription.md:188). At 1 m, a hole 3 m wide would drop its top, so nearly every hole a few shots make would collapse and the readout would rarely say "holds". At 2 m the fall width is 5 m, which has three effects. A section can drop its top without its neighbours, as a per-section integrity view assumes. An undercut has to run 5 of a section's 8 m, which is real work for sappers under fire. And F-24's overhang concern stays at 2 m. |
| Common rock span | 2 m | The same as stone, as a placeholder. Not exercised: the rock in this model rests on bedrock. |
| Anchor depth | **8 m** below the generated surface | Deeper than any earthwork the MVP draws, such as a ditch or a trench beside a foundation, so a siege dig never reaches it. A quarry can, and removes anchors as it goes, which is what ADR-004 intends. Not exercised here: bedrock lies 3 m down, so every anchor is bedrock. |
| Climb for foot soldiers | **1 m in each 1 m step** | "Foot soldiers climb slopes as steep as settled earth" (GameDescription.md:219), and earth settles to 45°, one voxel per voxel (:59). |
| Set height of loose material | **1 m** | The smallest height that keeps a heap's top below a structure's top. Because units never stand on a structure and loose material never rests on one, that is all F-06 needs. A larger value would push rubble off the low stubs a breach leaves, against "a fallen wall section leaves a ramp into the breach". 2 m was tried first and bound in W08, where rubble leans 1 m high against a 2 m sill. The stand-in does not enforce this rule; the script fails if the rule would bind anywhere. |
| N, the shots ahead | **0** | H2 asks about "the next N shots", with N fixed for each question (GameDesign.md:279). Craters at 1 m are Open (ADR-006, *Left open*), so this test shows the damage as done and asks only about the collapse that follows. That separates reading the rule from predicting where shots land. |
| Questions per item | **1** | "Open or shut?", with a free-text reason. |

### The resolver's stand-in

It follows [ADR-004](../../ADR/ADR-004-rule-based-destruction.md), decision 2, as written:

- **Support distance:** the fewest horizontal steps on any path from a voxel to an anchor that moves only down or sideways, in four directions, through solid voxels of any material. The script searches backwards from the anchors.
- **Anchors:** bedrock voxels, and any voxel below the anchor depth.
- **Unsupported:** a masonry or rock voxel whose support distance is more than its span, or that reaches no anchor. Earth and rubble carry support and are not judged by span.
- **Pieces:** unsupported voxels joined by faces. All the pieces found on one tick fall together. They are ordered by their lowest voxel, which stands in for entity-id order.
- **Applied until stable:** after the pieces fall and the loose material settles, support is searched again, until a search finds nothing. A cascade falls on later ticks.

**The collapse simplification (Proposed).** A piece is laid down as rubble in the columns it fell from, straight down, and then settles. It does not damage the stone it lands on. Warning time, fall length, damage to units and timing are not modelled; only the settled state is scored. The design fixes the rules but not their numbers (ADR-004, *Left open*), and the question is only whether a route is left.

The script checks that this simplification decides no answer. It resolves every state three more times:

- with the piece toppled outward, laid along the line of the fall as far out as the piece is tall;
- toppled inward, the same way;
- with fall damage: the first stone under each falling column knocked out.

**No answer changes under any of the three.**

**Loose material (Proposed).** Earth and rubble follow one rule. The script sweeps from the bottom up in a fixed order until nothing moves, and each loose voxel:

- falls into an empty cell below it;
- slides down to a side where the drop is 2 m or more, which is the 45° slump (GameDescription.md:59);
- never rests on top of a standing structure, but slides off it.

Rubble keeps its material, so masonry becomes stone rubble.

### What "open" means

**Open (Proposed)** means foot soldiers have a route through the section once everything has settled. That is the passability view's route (GameDesign.md:279), made concrete:

- **Walkable cells.** Each column has one: the highest empty cell with footing below it and 2 m of room above it (GameDesign.md:207). Footing is earth, rock, bedrock or rubble, never structure, because no unit stands on a structure (GameDesign.md:205). So a gap under an intact top is walkable, and a stone sill at any height is not.
- **Steps.** A step goes to a neighbouring column that shares a face, at most 1 m up or down. Both columns need 2 m of room above the higher cell.
- **The route.** It runs from the outer edge of the ground to the inner edge, and crosses the wall line inside this section's 8 m.
- **Rubble.** It is walked over like ground. It is slower (GameDescription.md:219), but speed does not decide open or shut.

### The readouts

Both are computed on the state as shown, before the resolver runs, and both end with the same row, STANDS ON. Their rows follow [Hud.md](../../Hud.md) §10's readout model, keys and values, with a warn flag on each row.

**The thickness readout (Proposed formula)** is the integrity view as first written: "each wall section's thinnest remaining cross-section" (GameDescription.md at `2d739df`, :244 and :387).

- THINNEST: the least remaining thickness through the wall, in metres, at any point of its solid body (rows 0–4). A hole that goes through counts as 0 m. The battlements are left out because they are thin by design.
- WEAK POINT: where the wall is that thin. The page outlines it on the front view.

**The support readout (Proposed formulas)** is the integrity view as revised (GameDescription.md:408).

- WIDEST HOLE: the widest opening through the whole thickness, in rows 0–5, with its width and height. It is shown against the **fall width**, 2 × span + 1 = 5 m: the narrowest hole, bridged at both ends, whose middle lies further than the span from either end. The verdict, FALLS or holds, is the resolver's: whether any stone above that opening is unsupported. ADR-004, decision 5, lets nothing else predict a collapse.
- LOWEST OPENING: the opening with the lowest floor. It gives the floor's height, what the floor is (ground or stone) and the clear height. Its verdict is "a route now", "no footing: soldiers never stand on stone", or "too low: soldiers need 2 m".
- GAP UNDER FOUNDATION: the longest run of columns where the ground under the wall's lowest row is gone under the full thickness. It is shown against the same 5 m, with the resolver's verdict. A gap under one face only is given in brackets, because the other face still carries the wall.
- WEAK POINT: the first reading past its mark, or else the one nearest to it. The page outlines it on the front view.

The script checks that each verdict agrees with the bare widths in all 16 states, so in this set no number contradicts its verdict. Ambiguity 3 gives states where one would.

## The 16 states

The ids are neutral and do not follow the answers. The page never shows ids or titles to a participant. Columns are numbered 1–8 from the left as seen from outside, and rows count up from the ground. *Fell* is the number of voxels the resolver drops. The support column gives the hole, the opening and the gap, in that order.

| Id | Damage | Thickness | Support: hole · opening · gap | Fell | Key | Why |
|---|---|---|---|---|---|---|
| W01 | Slot at the foot, 4 m wide and 1 m high, columns 3–6 | 0 m | 4 m, holds · ground, 1 m clear · none | 0 | shut | The middle of the slot is 2 steps from a pillar, so it holds; 1 m is too low. |
| W02 | Gap under the foundation, 6 m, columns 2–7, with the sappers' trench | 2 m | none · none · 6 m, FALLS | 24 | open | The middle two columns are 3 steps from ground and fall, leaving a gap to the sky. |
| W03 | Slot at the foot, 5 m wide and 1 m high, columns 2–6 | 0 m | 5 m, FALLS · ground, 1 m clear · none | 9 | open | The middle column is 3 steps from a pillar and falls, from the slot up. |
| W04 | Outer face shaved off the whole body | 1 m | none · none · none | 0 | shut | The inner face stands on the ground and holds the outer merlons one step away. |
| W05 | Tall slit, 1 m wide, 1–5 m up, column 5 | 0 m | 1 m, holds · stone 1 m up, 4 m clear · none | 0 | shut | The slit's floor is stone. |
| W06 | Outer face shaved off, and a 5 m gap under the foundation, columns 2–6 | 1 m | none · none · 5 m, FALLS | 8 | open | The inner face's middle column is 3 steps from ground and falls. |
| W07 | Hole 1 m wide and 2 m high at the foot, column 6 | 0 m | 1 m, holds · ground, 2 m clear, a route now · none | 0 | open | A unit fits through already, and nothing falls. |
| W08 | Window 6 m wide and 2 m high, 2–4 m up, columns 2–7 | 0 m | 6 m, FALLS · stone 2 m up, 2 m clear · none | 8 | shut | The top's middle falls, but the floor is a stone sill 2 m up. |
| W09 | Slot at the foot, 7 m wide and 1 m high, columns 1–7 | 0 m | 7 m, FALLS · ground, 1 m clear · none | 31 | open | The middle three columns fall. |
| W10 | Gap under the foundation, 4 m, columns 3–6, with the sappers' trench | 2 m | none · none · 4 m, holds | 0 | shut | The middle of the gap is 2 steps from ground, so it holds. |
| W11 | Two slots at the foot, each 3 m wide and 1 m high, columns 1–3 and 5–7 | 0 m | 3 m, holds · ground, 1 m clear · none | 0 | shut | The pillar between them keeps every voxel within 2 steps. |
| W12 | Outer face shaved off, and a 5 m slot at the foot of the inner face, columns 3–7 | 0 m | 5 m, FALLS · ground, 1 m clear · none | 7 | open | The middle column of the inner face falls, and the outer face is already gone. |
| W13 | Gap under the foundation, 5 m, columns 1–5, against the left neighbour | 2 m | none · none · 5 m, FALLS | 13 | open | Column 3 is 3 steps from the neighbour and from ground, and falls. |
| W14 | Window 3 m wide and 2 m high, 2–4 m up, columns 3–5 | 0 m | 3 m, holds · stone 2 m up, 2 m clear · none | 0 | shut | The floor is a stone sill, and a 3 m hole holds. |
| W15 | Slot at the foot, 6 m wide and 1 m high, columns 3–8, against the right neighbour | 0 m | 6 m, FALLS · ground, 1 m clear · none | 20 | open | The middle two columns fall. |
| W16 | Gap under the outer face only, all 8 m, with the sappers' trench | 2 m | none · none · none (8 m under the outer face), holds | 0 | shut | The inner face is one step away and stands on earth. |

The brief asked for three kinds of deceptive state:

- **A face shaved thin everywhere that stays shut: W04.** It has the thinnest reading of any state without a hole, 1 m, and it stays shut.
- **A full-thickness section with one hole wider than twice the span, which drops its top and opens: W03,** and W09 and W15 at other widths. The slot alone is too low to pass, but its middle drops from the slot to the battlements and leaves a gap to the sky. W12 is the same on a shaved face. W08 is the same kind of hole 2 m up: its top falls too, but the floor is the stone sill, so it stays shut.
- **Undercut foundations: W02, W06 and W13 fall.** W06 reads exactly like W04 on the thickness readout, and opens. W10 (4 m) and W16 (under one face only) are undercuts that hold.

### What each readout gets right if read literally

| Reading rule | Right, of 16 |
|---|---|
| Always open, or always shut | 8 |
| Thickness readout: open if and only if THINNEST is 0 m | 8 |
| As it stands: open if and only if there is a route before anything falls | 9 |
| Support readout: open if there is a route now, a gap that falls, or a hole that falls over a floor at ground | 16 |

The two readouts point different ways on 8 of the 16: W01, W02, W05, W06, W08, W11, W13 and W14. A participant who also reads the views can beat either readout's literal rule, and that comparison is what the test measures.

### How the key moves with the span

The script records the answer at other spans for each item, under `sensitivity`:

- **1 m:** W01, W10 and W11 open, giving 11 open and 5 shut.
- **3 m:** W02, W03, W06, W12, W13 and W15 shut, giving 2 open and 14 shut.

Either change breaks H2's even split. A different span needs a different set of items, not just a new key.

### What the test does not show

- **It is an upper bound on what the integrity view gives.** With N = 0, and the resolver's verdict in the readout, it checks that the readings are read and combined. It does not check that players can project the next shots; the M3 breach lab does that.
- **It does not test whether the readout is complete.** It has one section on one kind of ground, with intact neighbours, and avoids the states in ambiguity 3 where the readout as specified misreports. It tests whether the readout is read correctly.
- **It leaves most of the game out.** There are no units, no timing, no collapse preview and no passability view.

## Running a session

Before the first participant:

1. Run the script. Check that the version on the page's set-up screen matches `BreachPrediction.json`.
2. Open `BreachPrediction.html` in a browser on the facilitator's machine. It needs no network. Use the answer key link only when no participant can see the screen.

For each participant:

1. **Assign the readout.** Number participants in the order they arrive: P01, P02 and so on. Odd numbers get the thickness readout and even numbers the support readout, so both groups fill evenly and arrival order does not pick the group. Do not tell participants that there are two readouts.
2. **Set up.** Enter the participant number and choose the readout. Keep the random seed or type a new one. The seed fixes the item order, and it stays on screen.
3. **Brief.** The participant reads *How this works* on screen, which is the same for both readouts. It defines open and says that no more shots will be fired. It does not explain the support rule or the span. Answer questions about the pictures, not about what will fall.
4. **Practice.** One undamaged section, which is shut, with its answer shown. It teaches the views and is not scored.
5. **The 16 items.** For each, the participant chooses open or shut and writes a few words of reason. There is no time limit and no going back. The page records each answer, its reason and the time taken. Do not comment on answers.
6. **Finish.** The page shows the participant's accuracy. Download the results JSON before starting the next participant. The download is the record; the copy the browser keeps is only for recovering from a reload.

Each results file holds:

- the participant, readout, seed, item order and instrument version;
- the practice answer;
- for each item: the answer, the key, whether it was right, the reason and the time in milliseconds.

## Scoring

- **Accuracy.** A participant's accuracy is the number of items right, divided by 16. On 16 items, 0.65 needs 11 right (0.6875); 10 right is 0.625.
- **Median.** For each readout, take the median accuracy of the participants who saw it. With six, that is the mean of the third and fourth best. It reaches 0.65 when those two scores add up to 21 or more.
- **Pass:** the support readout's median is 0.65 or more (GameDesign.md:286; MvpPlan.md:184).
- **The even split.** H2 requires it (GameDesign.md:286; review F-47), and it is built in: 8 open and 8 shut, enforced by the script. A participant who always says open, or always says shut, scores 0.5.
- **The thickness median.** Report it beside the support median. The test bears on F-04 only if the support readout also beats the thickness readout. A pass with both groups equal says the views did the work, not the readout.
- **Wrong answers.** Read the reason given for each before changing anything. A misread view is a fix to the page; a misread rule is a fix to the readout.

**How strong the test is.** The script's arithmetic, `scoring` in the JSON, assumes that each participant answers each item right with a fixed chance, independently of the other items:

| Chance per item | Median of 3 passes | Median of 6 passes |
|---|---|---|
| 0.50, guessing | 0.03 | 0.008 |
| 0.60 | 0.25 | 0.24 |
| 0.70 | 0.73 | 0.87 |
| 0.75 | 0.91 | 0.98 |
| 0.80 | 0.98 | 0.999 |

A single person guessing reaches 11 of 16 with a chance of 0.105.

## Ambiguities and contradictions

1. **The climb reading cannot apply to a stone sill.** The integrity view weighs "its lowest opening against what foot soldiers can climb" (GameDescription.md:408). But no unit stands on a structure (GameDesign.md:205; GameDescription.md:219), and loose material never rests on top of one (GameDescription.md:71). So a hole with a stone floor is never a route, however low its sill, and the climb limit matters only for floors of ground or rubble. The test follows the rule: W05, W08 and W14 are through-holes that stay shut for that reason alone. If the owner meant a low sill to be climbable, both the rule and the key change.
2. **A sapper trench alone does not undercut a 2 m wall here.** Undermining works "only where the wall stands on earth" (GameDescription.md:201), but the 45° slump (:59) reaches only one voxel under the foundation. In this stand-in, a trench 2 m deep down to the rock, along the whole outer foot, takes the earth from under the outer face only. The inner face stays on earth, and the wall stands: one sideways step is within the span. So the undercut states here give the gap under the full thickness directly. How earth under a structure slumps, the slump neighbourhood, is still open (MvpReadiness-2026-10-31.md:141). Where earth over rock is shallow, it decides whether undermining works at all.
3. **The three readings are not the whole rule.** Two cases from this stand-in were kept out of the 16, because the readout as specified would misreport them:
   - A 4 m slot at the foot falls when a cut takes away one of the pillars that bridge it, though 4 m is under the 5 m fall width. That width holds only for a hole bridged at both ends, so a number on the readout must come from the resolver for each hole, not from 2 × span + 1.
   - Pockets in the two faces at different heights (the outer face at rows 0–2, the inner at rows 3–4) drop 8 voxels with no through-hole and no gap under the foundation, so all three readings say nothing. That section stays shut, but stone falls that the view does not show.
4. **The wall's dimensions disagree.** ADR-006, decision 3, sizes the standard section at 8 × 2 × 6 m for 96 m³. This test's brief said 6 m high plus 1 m of crenellation, which would cost more than 96 m³. T3 is 5 m solid plus 2 m of battlements, 96 voxels ([MvpAssets.md](../../MvpAssets.md):19). The test uses T3, because it is the content the simulation will load and it matches the 96 m³. T3's battlements are an automatic conversion: merlons on the outer face at 5–6 m and on the inner face at 6–7 m.
5. **Hud.md's single OPEN figure has no formula.** The wall section's HUD row reads "OPEN 40%" (Hud.md:116), which is "the integrity view's" reading (:119). The view gives three readings, and nothing says how they combine into one percentage. This test shows the three readings and no percentage.
6. **"Thinnest remaining cross-section" is ambiguous.** It does not say whether a through-hole counts as 0 m or is left out, whether the battlements count, or whether it means a thickness or a horizontal area. The test reads it as a thickness, with holes at 0 m and the battlements left out. That is the literal reading, and leaving holes out would handicap the control.
7. **The fall direction and fall damage are undefined for a wall.** A piece falls "towards the side that lost support, or straight down if it lost support evenly" (GameDescription.md:193). A section undercut along its outer foot, or holed through, has no stated side. "It removes voxels from structures in its path" (:194) does not say whether a section's falling top damages its own lower part. The test drops pieces straight down without damage, and shows that neither choice changes an answer here.
8. **How many people, and in what design.** The pass is stated for "six people": the review's cheapest test 3 (:307), and MvpPlan.md:296 for M3. The review splits them between the two readouts, while GameDesign.md's playtest has each person play both versions (:284). Split three and three, each readout's median is of three people, which passes a true per-item accuracy of 0.70 only 73% of the time. **Proposed:** six people for each readout, twelve in all, each seeing one readout, because the same items cannot be shown twice with different readouts.
9. **A section's readings depend on its neighbours.** ADR-004, decision 2, does not stop support at a section's end, so damage to the neighbour changes a section's readings. The view is per section (GameDescription.md:408). The test assumes intact neighbours.
10. **Where the Proposed values are recorded.** GameDesign.md, decision 7 (:185), and MvpPlan.md, *M0*, ask for them to be recorded in GameDesign.md. This work created new files only, so they are recorded here, and GameDesign.md still has to list them.

## Results

None yet: no participant has taken the test. Record results in a new section here, with the instrument version, the participants' results files, both medians and the verdict.
