# Hold rule test (F-45), 2026-10-31

**What this is.** This is the M0 paper test of the revised Hold wave rule ([MvpPlan.md](../../MvpPlan.md), M0). It is the review's "Cheapest tests" item 2, for finding F-45 ([GameDescription-2026-10-31.md](../GameDescription-2026-10-31.md)). This version applies the owner's decisions of 2026-10-31 to the first run.

**The question.** Does where the player quarries change where the waves breach?

**Pass.** The breach moves with quarry placement in at least two of three layouts.

**The script.** [`Tools/PaperTests/HoldRuleTest.py`](../../../Tools/PaperTests/HoldRuleTest.py) is Python 3, standard library only. Run it with `python Tools/PaperTests/HoldRuleTest.py`, or add `--scale 1` for plans at 1 m per character. It reads the castle's templates and the palette from `GameData/Server/`. Its scores are exact fractions and its draw is seeded, so it prints the same output on every machine. A run takes about 20 seconds. Its full output is at the end of this report.

## Verdict

**Fail at the revised values: the breach moves in 1 of 3 layouts, and that one move sits exactly on the bar.** Layout A moves 1/3 of its waves, B 1/15 and C 1/18. Read the fail with four points:

- **In B and C the quarry's two effects cancel.** The decided reading makes a quarry in front of a wall a ditch in front, which zeroes the wall's earth reason. The revised open term now sees the same quarry's slump as undermining, and reads 0.88 to 0.93 beside it. In B1, N2 scores 1.85, against 1.93 with the quarry elsewhere. It stays in the best few, and the breach stays where it was.
- **Rule (b) is what turns the first run's pass into a fail.** Without it, rules (a) and (c) pass in all three layouts: 1/3, 2/5 and 7/18.
- **A moves only through rule (c), and only on the bar.** Waves without a ram move 4/9 and waves with one 2/9. Their mean is exactly 1/3.
- **The fail is as fragile as the first run's pass.** 6 of the 19 single-value changes in the sensitivity table turn it into a pass. Two of them are doubling the open weight and halving it. Equal weights sit at the point where the quarry's two effects balance.

**Before the revision.** The first run used a 4 m stone span. Its open term was the widest support distance divided by (span + 1). It ranked an open section by score alone, and it scored the gate in every wave. It passed narrowly: the breach moved in B and C, each exactly at 1/3, and not in A (2/9). Both passes came through the ditch reason. The quarry in front zeroed the wall's earth reason and pushed the waves away, while the open term barely saw the undermining (0.20). 6 of 16 single-value changes turned it into a fail, among them counting only ditch designations as ditches. The same rules at the decided 2 m span still give 2/9, 1/3 and 1/3, so the span decision alone does not change that verdict.

## How the rule was run

**The decided rule** ([GameDesign.md](../../GameDesign.md):296). Each wave scores every stretch of wall it can reach on four reasons:

- how close the stretch is to opening, which it asks the collapse resolver;
- whether it stands on earth with no ditch in front;
- how far it is from the towers;
- whether it is a gate.

It then picks among the best few with a seeded draw.

**The owner's decisions of 2026-10-31**, all applied:

- **The stone span is 2 m.** T2's floors and roof are now timber: 192 voxels of palette material 7 in [`earthrise-t2-keep-1m.json`](../../../GameData/Server/Templates/earthrise-t2-keep-1m.json). The plans model each structure from the script's own numbers, so the script now checks them against the templates. It reads T1–T5 and the [palette](../../../GameData/Server/Palette.json). It confirms that the plan's keep, sections, towers and gatehouse have the footprints of T2–T5's bottom layers, and it computes each material's widest support distance on level ground:
  - T2's stone needs 0 m and its timber 4 m;
  - T4's and T5's stone need exactly 2 m;
  - T1's timber roof needs 5 m.

  Every template's stone therefore stands at 2 m. The plans hold the keep as its footprint, which rests on T2's stone bottom layer, so the stone span is the one that applies to it. Its timber floors rest on its walls, which no dig in the test reaches. Palette material 6, "masonry", is what the package calls "dressed stone": ordinary stone laid by a template, not the Masons' upgrade.
- **A quarry in front of a wall counts as a ditch in front**, as pillar 1 says ([GameDesign.md](../../GameDesign.md):193).
- **Rules (a), (b) and (c)**, below. They are the three rule changes the first run suggested.

As run:

- **score** = open + earth + towers + gate, each term from 0 to 1, with equal weights;
  - **open** is rule (b), defined below;
  - **earth** is 1 if any column under the stretch is earth and there is no ditch in front, and 0 otherwise;
  - **towers** is the distance to the nearest tower divided by the largest such distance among the stretches;
  - **gate** is 1 for the gatehouse and 0 otherwise. Under rule (c) it counts only for a wave that brings a ram.
- **(a) An open stretch always wins.** This is a rule, not a weight. When any stretch the wave scores is open, the wave draws among the open stretches alone, ranked by score. Intact stretches are candidates only when none is open. "Open" is what the stand-in for the resolver reports: a stretch whose widest support distance is more than the span. The other form of the rule lets open stretches lead the best few and intact ones fill the rest. It was rejected because one open section would then take only a third of the waves, which is not "always wins". The sensitivity table runs it.
- **(b) The open term reads how close the stretch is to opening.**
  - For an intact stretch, **open = max(0, 1 − T ÷ T₀)**. T is the least volume, in m³, that a sapper's trench must still dig to drop the stretch. T₀ is the same stretch's need in the layout's undug ground.
  - Open is 1 when the stretch is open, and 0 when no trench can drop it, now or undug. It therefore stays in [0, 1].
  - **A sapper's trench** is 2 m wide against the stretch's outer face. It runs along any part of the face, and is cut to any whole depth down to bedrock. It cuts whatever it meets, as a designation does, and its spoil leaves play, like a pit's. Its volume is what it digs from the ground as it stands, so ground a slump has already taken costs nothing.
  - In undug ground a section needs 36–42 m³. On 4 m of earth, for example, a trench 3 m long and 6 m deep drops E2. The gatehouse needs 24 m³ (ambiguity 12).
  - An untouched stretch reads 0. A stretch behind a rampart also reads 0, because the rampart raises the need above T₀ and the formula clamps it.
- **(c) The gate scores only for a wave that brings a ram.** The design does not say which waves bring rams. **Proposed: every second wave other than the sapper wave, starting with the second**, so half of them. A wave with a ram scores the gatehouse with its gate reason. A wave without one leaves the gatehouse out unless it is already open. The other reading keeps the gatehouse as a candidate scored without its gate reason. It fails in all three layouts and is reported with the results (ambiguity 2).
- **The draw.** The stretches are shuffled by the wave's seed, sorted by rank (ties keep their shuffled order), and the best 3 are kept. One of the three is then drawn uniformly, so ties at the cut are split evenly. Chances are computed exactly, and waves 1 to 8 are printed, wave n drawing with seed n.
- **The sapper wave** uses the same score and rule (a), over sections on earth only. The decided text has sappers "trench beside a section standing on earth" ([GameDesign.md](../../GameDesign.md):297), so the gatehouse is not a candidate. It brings no ram.
- **"The breach moves"** when at least 1/3 of the waves breach somewhere else. For one kind of wave, that share is the total variation distance between two placements' pick distributions. There are now two kinds, with a ram and without one, so the share over the waves is the mean of the two, each weighted by its share of waves. Rule (c) forced this extension. The bar of 1/3 is unchanged.
- **Which placements are compared.** The verdict compares each layout's quarry pair: the same stone dug in two places. The ditch placements are a separate choice and are reported as context.

**The stand-in for the resolver** applies [ADR-004](../../ADR/ADR-004-rule-based-destruction.md) decision 2 to a structure standing on a height field of 1 m columns:

1. **Slump.** Earth slumps until no earth column stands more than one voxel above its neighbor. It moves one voxel at a time to the lower neighbor, in a fixed order, so it is conserved. Rock holds vertical faces. Earth under a structure slumps like any other earth, and the structure stays where it is.
2. **Grounded columns.** A footprint column whose ground still touches the structure's base is grounded. Every other footprint column takes the fewest sideways steps, through the joined curtain's footprint, to a grounded column. That is its support distance. There are no tunnels, so every solid column is continuous down to its anchor, and the anchor depth never enters.
3. **Open.** When the widest support distance in the stretch is more than the span, the stretch falls and is open.

**The search for T.** The script tries trenches from the smallest volume up, and the first that drops the stretch gives its need. It takes two shortcuts:

- It skips any trench that lies inside one that has already failed, no longer and no deeper.
- It copies only the ground within 12 m of the stretch for each try, and runs the support search only over the curtain within a span of it.

Both shortcuts were checked against a search that tries every trench on a full copy of the world, with support over the whole curtain. All 742 needs the run uses agree.

**What changed in this revision, and why:**

- The stone span is 2 m (Decided).
- The ditch reading is now Decided and was already the one used.
- The open term, the ranking and the two kinds of wave are rules (a)–(c).
- The template check was added for decision 1.
- The first run's open term, support distance ÷ (span + 1), is kept only for the runs that undo rule (b).
- No layout or dig changed. The near quarries stop at the same berms at 2 m as at 4 m: 3, 2 and 3 m. The plans and dug volumes are identical to the first run's.

## The layouts

All three layouts share one castle. It is the hall and the finished keep the Hold trial starts with ([GameDesign.md](../../GameDesign.md), 2026-10-31). Around them is a curtain the player built in the build window: 11 T3 sections, four T4 corner towers and the T5 gatehouse, 39 × 38 m in all ([MvpAssets.md](../../MvpAssets.md)). The curtain costs 3,557 m³ of stone. Each plan below is 2 m per character, north up.

```
    TTT#############TTT      #  wall section    G  gatehouse    T  tower    K  keep    H  hall
    TTTT           TTTT      !  structure with a gap under it (undercut)    X  a piece that fell
    TTTT N1  N2  N3TTTT      q  quarry pit    v  ditch    ^  rampart    ~  ground lowered by slump
    #TTT           TTT#      o  building stone at the surface    :  rock at the surface
    #   KKKKKK        #      ,  earth 1-2 m    .  earth 3 m or more
    #W3 KKKKKK      E3#
    #   KKKKKK        #
    #   KKKKKK        #
    #   KKKKKK        #
    #W2 KKKKKK      E2#
    #   KKKKKK        #
    #     KK          #
    # HHHHHH          #
    #W1HHHHH        E1#
    # HHHHHH          #
    #THHHHHHG GG    TT#T
    TTTT S1GG GG S2 TTTT
    TTTT   GG GG    TTTT
    TTT####GGGGG####TTTT
```

**A: an outcrop inside the east curtain.**

- **Ground.** E1–E3, S1, S2 and the gate stand on 4 m of earth, with bedrock at 12 m. N1–N3 and W1–W3 stand on a rock shelf, with bedrock at 8 m.
- **Outcrops.** One lies inside the curtain, between the keep and E1–E3. The other lies 19 m west of the west wall, on the shelf.
- **Placements.**
  - **A1, near:** the inner outcrop, chased east towards E1–E3 to a 3 m berm. It yields 1,143 m³.
  - **A2, far:** the west outcrop. It yields 2,464 m³.
  - **A3:** A2, plus a standard ditch and rampart in front of E1–E3.

```
    :oooooooooo:::::::TTTT:::::::::::TTTT:::::::
    :oooooooooo:::::::#:::...........~..#^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~~~~~~~~#^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~#^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::..KK..~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:HHHHKK..~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:HHHHHH..~qqqqqq~#^^vvv..
    :oqqqqqqqqo:::::::#:HHHHHH...~~~~~~.#^^vvv..
    :oqqqqqqqqo:::::::#:HHHHHH..........#^^vvv..
    :oooooooooo:::::::TTTT...GG.GG....TTTT..~...
```

**B: an outcrop in front of an earth wall.**

- **Ground.** Everything stands on 3 m of earth, with bedrock at 10 m, except W1–W3, which stand on a rock shelf.
- **Outcrops.** One lies 8 m north of N1–N3. The other lies 15 m east of E1–E3.
- **Placements.**
  - **B1, near:** the north outcrop, chased south towards N1–N3 to a 2 m berm. It yields 1,992 m³.
  - **B2, far:** the east outcrop. It yields 2,396 m³.
  - **B3:** B2, plus a ditch in front of N1–N3. B1 and B3 dig the same ground, so the plan below shows only B1's pit.

```
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::TTT##!!!!!!!!###TTT................
    ::::::TTT#############TTT................
    ::::::TTTT...........TTTT................
    ::::::TTTT...........TTTT................
    ::::::#:::..............#......~~~~~~~~..
    ::::::#:::KKKKKK........#.....~qqqqqqqq~.
```

**C: outcrops by an earth wall and by a rock wall.**

- **Ground.** N1–N3 and E1–E3 stand on 4 m of earth, with bedrock at 10 m. W1–W3, S1, S2 and the gate stand on a rock shelf, with bedrock at 8 m.
- **Outcrops.** One lies 8 m east of E1–E3, in the earth. The other lies 5 m west of W1–W3, on the shelf.
- **Placements.**
  - **C1:** the east outcrop, chased west towards E1–E3 to a 3 m berm. It yields 2,226 m³.
  - **C2:** the west outcrop, which is already by its wall. It yields 1,728 m³.
  - **C3:** C2, plus a ditch in front of E1–E3.

```
    :ooooooooo#:::..............#.~~~~~~~~~...
    :oqqqqqqqo#:::KKKKKK........#~qqqqqqqqq~..
    :oqqqqqqqo#:::KKKKKK........!~qqqqqqqqq~..
    :oqqqqqqqo#:::KKKKKK........!~qqqqqqqqq~..
    :oqqqqqqqo#:::..KK..........!~qqqqqqqqq~..
    :oqqqqqqqo#:HHHHKK..........!~qqqqqqqqq~..
    :oqqqqqqqo#:HHHHHH..........#~qqqqqqqqq~..
    :ooooooooo#:HHHHHH..........#.~~~~~~~~~...
```

These are excerpts. The full plans are in the script output below.

## Proposed and decided values

GameDesign.md (2026-10-31, *Starting the MVP*, item 7) leaves these to the implementer, as Proposed with a reason; the readiness review listed the gaps ([MvpReadiness-2026-10-31.md](../MvpReadiness-2026-10-31.md):45). MvpPlan.md says they are recorded in GameDesign.md. This test modifies no existing file, so they are recorded here, and the last section gives the text to paste. Two values are now Decided. The rest stay Proposed: the first run's values are unchanged unless a decision forced the change, which the table says.

| Value | Status | Value used | Reason |
|---|---|---|---|
| Stone span | **Decided** (owner, 2026-10-31) | 2 m | Every template's stone stands at 2 m, T4's and T5's exactly. T2's floors and roof are timber. The script computes both from `GameData/Server/Templates/`. |
| A quarry is a ditch in front | **Decided** (owner, 2026-10-31) | Any cut counts, a quarry pit included | Pillar 1: "a quarry beside a wall is a ditch an attacker has to cross" ([GameDesign.md](../../GameDesign.md):193). A wave reads voxels, not designations (ADR-003). |
| A ditch in front, its measure | Proposed | A cut at least 2 m deep within 10 m of the face, along at least half the section's length | 2 m is the standard ditch's depth. 10 m spans ditch, berm and rampart (9 m). |
| Weights | Proposed | 1, 1, 1, 1 | The design names four reasons and ranks none of them. Every term is scaled to 0–1. |
| The best few, N | Proposed | 3 | The smallest number that is "few" rather than a pair. |
| The draw | Proposed | Uniform over the best N, from SplitMix64 seeded with the wave's number | The simplest seeded draw, from a pinned PRNG ([AGENTS.md](../../../AGENTS.md) R16). |
| The tie-break | Proposed | Tied stretches are shuffled by the same seed before the cut | A fixed order, such as by name, would favor sections for a reason no attacker reads. |
| (a) An open stretch wins | Proposed | When any candidate is open, the wave draws among the open ones only | "Always wins" read as a rule. With open stretches only leading the best few, one open section would take a third of the waves. |
| (b) The open term | Proposed, replacing the first run's | 1 − T ÷ T₀, clamped to [0, 1]; 1 when open; 0 when no trench drops it | "The trench a sapper would still need", against the same stretch undug. It reads a slump before the stretch falls, and the trench's target already comes from the resolver ([Hud.md](../../Hud.md):266). |
| (b) The sapper's trench | Proposed | 2 m wide against the outer face, any run of it, any whole depth to bedrock, cut through whatever it meets, spoil out of play | The width matches the Siege route table's trench ([SiegeRouteTable.md](SiegeRouteTable.md)). The least volume is the least work. It is dug from outside because the waves come from outside. |
| (c) Which waves bring a ram | Proposed | Every second wave other than the sapper wave, from the second: half | The design does not say. Half is the neutral split. |
| (c) A wave without a ram | Proposed | Leaves the gatehouse out unless it is open | "The gate scores only for waves that bring a ram". It also does what the suggestion was for: it stops the gatehouse filling a slot in every wave. |
| Standard foundation | Proposed | Nothing below the levelled surface | T3 draws no foundation voxels. [Hud.md](../../Hud.md):257 says any earth under a wall can be undermined, whatever the soil depth. |
| How digs cut | Proposed | Straight down to their depth or to bedrock, after which their earth faces slump. A pit's earth spoil leaves play. | Without this a dig could never undermine anything, and the slump preview would be empty ([GameDescription.md](../../GameDescription.md):414). |
| Quarry depth | Proposed | 8 m | Each quarry then yields a third to two-thirds of the curtain. |
| Outcrops | Proposed | Building stone exposed in a core, dipping 1 m per metre under the soil for 4 m, and 10 m deep | "Outcrops dip under the soil at their edges" ([GameDescription.md](../../GameDescription.md):75). |
| The near quarry | Proposed | Chases the dipping stone towards the wall up to the closest berm at which the slump drops nothing: 3 m in 4 m of earth, 2 m in 3 m | This is a player using the slump preview, which is the case pillar 1 warns of. The script finds the berm itself. |
| On earth | Proposed | Any column of the footprint has earth under it | [Hud.md](../../Hud.md):257, GROUND. |
| The ditch | Profile Decided, the rest Proposed | Rampart 1, 2, 2, 1 m against the face, a 1 m berm, then the ditch 1, 2, 2, 1 m deep | The ditch profile is Decided ([MvpAssets.md](../../MvpAssets.md):60). The rampart holds exactly its spoil, and the berm keeps both from slumping. |
| Towers | Proposed | T4 towers and the keep's great tower, not the gatehouse. Distance is centre to centre, to the nearest one. | Towers and keeps garrison and fire. The gatehouse's garrison fires into its own passage ([GameDescription.md](../../GameDescription.md):168, :217). |
| Reach | Proposed | Every outer stretch | Quarries leave a ramp, and foot soldiers cross ditches and ramparts. |
| Stretches scored | Proposed | The 11 sections and the gatehouse; towers are not scored | They are the "stretches of wall". |
| The bar for "moves" | Proposed; its measure extended by rule (c) | 1/3 of waves breaching elsewhere, as the mean over waves with and without a ram | Defined above. A stricter bar of 1/2 is reported with the results. |
| The castle and ground | Proposed | As drawn above. Earth is 3–4 m deep, bedrock 8–12 m, with rock shelves. | A plausible Keep-era curtain on mixed ground (MvpPlan.md, M6). |

## Results

Each wave picks from the best few; their scores and chances are given below. A wave without a ram never takes the gatehouse here, because no gatehouse is open.

| Placement | Waves without a ram: best few, score (chance) | Waves with a ram: best few | Waves 1–8 | Sapper wave's best few |
|---|---|---|---|---|
| A1 near, inside | E2 2.64, E3 1.83, E1 1.81 (1/3 each) | G 3.00, E2 2.64, E3 1.83 (1/3 each) | E2 E2 E2 G E2 G E3 E2 | E2, E3, E1 |
| A2 far | E2 1.97 (1/3); E3, S2 and S1 at 1.49 (2/9 each) | G 3.00, E2 1.97 (1/3 each); E3, S2 and S1 (1/9 each) | E2 E2 E2 G E2 G S2 E2 | E2, then E3, S2 and S1 tied |
| A3 far + ditch | S2 1.49, S1 1.49, E2 0.97 (1/3 each) | G 3.00, S2 1.49, S1 1.49 (1/3 each) | S1 S1 S2 G S2 G S1 S1 | S2, S1, E2 |
| B1 near, in front | E2 1.97, N2 1.85 (1/3 each); E3, S2 and S1 at 1.49 (1/9 each) | G 3.00, E2 1.97, N2 1.85 (1/3 each) | E2 E2 E2 G E2 G N2 E2 | E2, N2, then three tied |
| B2 far | E2 1.97, N2 1.93 (1/3 each); five sections at 1.49 (1/15 each) | G 3.00, E2 1.97, N2 1.93 (1/3 each) | E2 E2 E2 G E2 G N2 E2 | E2, N2, then five tied |
| B3 far + ditch | E2 1.97 (1/3); E3, S2 and S1 at 1.49 (2/9 each) | G 3.00, E2 1.97 (1/3 each); E3, S2 and S1 (1/9 each) | E2 E2 E2 G E2 G S2 E2 | E2, then E3, S2 and S1 tied |
| C1 by the earth wall | N2 1.93, E2 1.89 (1/3 each); N1 and N3 at 1.49 (1/6 each) | G 2.00, N2 1.93, E2 1.89 (1/3 each) | N2 N2 N2 G N2 G E2 N2 | N2, E2, then N1 and N3 tied |
| C2 by the rock wall | E2 1.97, N2 1.93 (1/3 each); N1, N3 and E3 at 1.49 (1/9 each) | G 2.00, E2 1.97, N2 1.93 (1/3 each) | E2 E2 E2 G E2 G N2 E2 | E2, N2, then three tied |
| C3 rock wall + ditch | N2 1.93, N1 1.49, N3 1.49 (1/3 each) | G 2.00, N2 1.93 (1/3 each); N1 and N3 (1/6 each) | N2 N2 N2 G N2 G N1 N2 | N2, N1, N3 |

**The open term at the players' berms.** The second column is the trench a sapper would still need, against the same stretch undug.

| Placement | Stretches beside the quarry | T ÷ T₀, m³ |
|---|---|---|
| A1 (inside, 3 m berm) | E1 0.33, E2 0.67, E3 0.33 | 24, 12 and 24 of 36 |
| B1 (in front, 2 m berm) | N1 0.93, N2 0.93, N3 0.88 | 3, 3 and 5 of 40 |
| C1 (in front, 3 m berm) | E1 0.89, E2 0.92, E3 0.92 | 4, 3 and 3 of 36 |

**Share of waves that breach elsewhere.** The quarry pair decides the verdict.

| Layout | Quarry pair | Without a ram | With a ram | All waves | Moves? | Sapper wave | Near quarry against the ditch |
|---|---|---|---|---|---|---|---|
| A | A1 against A2 | 4/9 | 2/9 | 1/3 | yes, exactly at the bar | 4/9 | A1 against A3: 2/3 |
| B | B1 against B2 | 2/15 | 0 | 1/15 | no | 2/15 | B1 against B3: 1/3 |
| C | C1 against C2 | 1/9 | 0 | 1/18 | no | 1/9 | C1 against C3: 1/3 |

B1 and C1 no longer score the same as their ditch placements, as they did in the first run. The open term now tells a quarry that undermines from a ditch that does not.

**The near quarry at every berm.** These rows, from the script's sweep, show what the open term reads as the quarry comes closer, and the share of waves that breach elsewhere than under the far placement:

| Berm | A1 (inside, 4 m of earth) | B1 (in front, 3 m) | C1 (in front, 4 m) |
|---|---|---|---|
| 1 m | E1–E3 fall; 1/2 elsewhere | N1–N3 fall; 3/5 | E1–E3 fall; 11/18 |
| 2 m | E2 and E3 fall, E1 reads 0.89; 1/2 | **the player's berm:** 0.93, 0.93, 0.88; 1/15 | E1–E3 fall; 11/18 |
| 3 m | **the player's berm:** 0.33, 0.67, 0.33; 1/3 | 0.47, 0.50, 0.45; 2/5 | **the player's berm:** 0.89, 0.92, 0.92; 1/18 |
| 4 m | every stretch reads 0; 0 | 0.10, 0.12, 0.07; 2/5 | 0.39, 0.58, 0.58; 1/18 |
| 5 m or more | every stretch reads 0; 0 | every stretch reads 0; 2/5 | every stretch reads 0; 7/18 |

**Sensitivity.** These are the verdicts with one value changed at a time. The 16 changes of the first run are kept; the span rows are now 4 m, the first run's value, and 6 m. One change is added for each new rule.

- **Still fails (13):**
  - the earth weight at ×½;
  - the towers weight at ×2 or ×½;
  - the gate weight at ×2 or ×½;
  - N = 2 and N = 4;
  - a 4 m span;
  - the towers term scaled over its range;
  - the bar at 1/2;
  - (a) with open stretches only leading the best few;
  - (b) with a 1 m trench;
  - (c) with rams in 3 waves of 4.
- **Passes (6):**
  - the open weight at ×2 (3 of 3: 1/3, 13/30, 11/18);
  - the open weight at ×½ (3 of 3: 1/3, 2/5, 7/18);
  - the earth weight at ×2 (3 of 3);
  - a 6 m span (2 of 3);
  - only ditch designations counting as ditches (3 of 3: 1/3, 13/30, 11/18);
  - digs that do not slump (2 of 3).
- **The two-value row of the first run**, only ditches with the open weight doubled, passes 3 of 3.

**What each revision did.** Each row undoes one revision, or reads it another way, at the decided span and ditch reading:

| Rules | A1 v A2 | B1 v B2 | C1 v C2 | Verdict |
|---|---|---|---|---|
| Revised: (a), (b) and (c) | 1/3 | 1/15 | 1/18 | 1 of 3, fail |
| Without (a): score alone | 1/3 | 1/15 | 1/18 | 1 of 3, fail |
| Without (b): the first run's open term | 1/3 | 2/5 | 7/18 | 3 of 3, pass |
| Without (c): a ram in every wave | 2/9 | 0 | 0 | 0 of 3, fail |
| (c) read as the gate's reason only | 2/9 | 0 | 1/18 | 0 of 3, fail |
| None of the three | 2/9 | 1/3 | 1/3 | 2 of 3, pass |
| None of the three, 4 m span: the first run | 2/9 | 1/3 | 1/3 | 2 of 3, pass |

## What drives the result

1. **A quarry in front of a wall now cancels itself.**
   - The decided reading makes the quarry a ditch in front, which takes 1 off the earth reason.
   - Rule (b) sees the same quarry's slump as undermining. At B1's berm a sapper would need 3–5 m³ more to drop N1–N3, against 40 m³ undug, so open reads 0.88–0.93.
   - The net change is small: −0.075 for N2 and −0.083 for E2. N2 scores 1.85 in B1 against 1.93 in B2, and E2 1.89 in C1 against 1.97 in C2, so both stay in the best few.
   - Halving and doubling the open weight both pass, in opposite directions. At ×½ the ditch wins and the waves go elsewhere. At ×2 the undermining wins and the waves come to the quarry. Equal weights sit at the balance point.
2. **The decided ditch reading is what puts both effects into one score.** Count only ditch designations as ditches and all three layouts pass (1/3, 13/30, 11/18), because the undermining then draws the waves to the quarry. Under the first run's rules the same change was a fail, 0 of 3.
3. **Layout A moves only through rule (c), and exactly at the bar.**
   - Inside the curtain the quarry only undermines: E1 0.33, E2 0.67, E3 0.33.
   - A wave without a ram, with the gatehouse out, draws among E2, E3 and E1 in A1. In A2 it draws among E2 and three tied sections. 4/9 of those waves land elsewhere.
   - A wave with a ram keeps the gatehouse, and 2/9 land elsewhere. The mean is 1/3.
   - The share over all waves is 4/9 − 2/9 × the ram share, so A fails if more than half the waves bring a ram.
4. **No choice of ram waves rescues B or C.** Their waves with a ram never move (0), and their waves without one move 2/15 and 1/9. With no rams at all, neither reaches 1/3.
5. **Rule (a) changes nothing at the players' berms.** Each near quarry stops at the closest berm at which the slump drops nothing, so nothing there is open. Rule (a) decides the closer berms. At a 1 m berm B1 drops N1–N3, and 3/5 of the waves now land elsewhere, against 0 without (a). There the fallen N2 scored 1 + 0 + 0.93, no more than the intact N2 with the quarry elsewhere.
6. **In B and C, the closest berm is the case that moves least.**
   - B1 moves 1/15 at its 2 m berm, and 2/5 at 3 m or more.
   - C1 moves 1/18 at 3–4 m, and 7/18 at 5 m or more.
   - The wider berms move the waves away from the quarry, through the ditch reason, because the undermining fades with distance.
   - The player pillar 1 warns about quarries up to the slump limit. That player's quarry moves the breach least.
7. **The open term sees the slump only from outside.** In A1 at a 4 m berm, the slump lowers the ground at E2's inner foot by 1 m without undercutting it. The cheapest trench from outside is unchanged, so E2 reads 0. At the player's 3 m berm the quarry undercuts E1–E3, and they read 0.33–0.67.
8. **The span decision moved nothing by itself.** At 2 m the near quarries stop at the same berms as at 4 m. The first run's rules give the same 2/9, 1/3 and 1/3.

## Findings for the owner

None of these was tuned or adopted. Each is a value or rule that would have to change for the test to pass, with the evidence for it.

- **Decide which of a quarry's two effects a wave weighs more.** At equal weights the ditch and the undermining cancel. Doubling the open weight and halving it each pass in all three layouts, so either choice passes. The two are different games: at ×2 the waves go for the undermined wall, and at ×½ they avoid the quarried one. The owner has to choose; picking the value that passes would be tuning.
- **Keep rule (c), with the gatehouse left out.** It is the only revision that moves A. With (a) and the first run's open term, it passes all three layouts. Read as the gate's reason only, it fails all three.
- **Rule (a) is safe to keep.** It changes nothing at the players' berms and makes a fallen section the target.
- **Consider a draw weighted by score.** This is carried over from the first run, untested. With a uniform draw over three, a large change in score moves no more waves than a small one, and A's move sits exactly on 1/3, as B's and C's did in the first run.
- **The gatehouse is the cheapest stretch to undermine at the 2 m span.** A trench 2 m deep and 6 m long against its front, 24 m³, drops it. A section needs 36–42 m³. Once the piers' front columns lose their ground, the stone over the 3 m gate is 3 steps from support. That holds in T5's voxels as well as in the plan. The sapper wave never trenches the gatehouse, so the verdict does not depend on it. A gatehouse on earth may want a deep foundation or a ditch.

## Ambiguities and contradictions

The stone span, the quarry-as-ditch question and what a wave does with an open section are now settled, and are removed from this list.

1. **One quarry, two effects that cancel.** Pillar 1 makes the same quarry both "a ditch an attacker has to cross" and "a slope that can slump a foundation away" ([GameDesign.md](../../GameDesign.md):193). The rule now sees both, and at equal weights they cancel (findings above). The design does not say which an attacker should weigh more.
2. **"The gate scores only for waves that bring a ram."** This can mean that a wave without a ram leaves the gatehouse out, which is Proposed here. It can also mean that such a wave scores the gatehouse without its gate reason, which keeps the gatehouse in most pools on earth. The two give 1 and 0 layouts of 3.
3. **Which waves bring rams, and how many waves there are.** Neither is stated. "Rams go for the gate" ([GameDescription.md](../../GameDescription.md):257) is the only link. This test needs only the share, and Proposes half. A fails if more than half the waves bring a ram.
4. **What a trench need counts.** T counts m³, and a cubic metre of rock costs the same as one of earth. Rock is "dug slowly" ([GameDescription.md](../../GameDescription.md):60), and the cheapest undug trench beside A's east wall cuts 2 m of rock under 4 m of earth. A need in sapper-seconds would weigh rock more, but no dig rates by material are given.
5. **The two tests' trenches disagree.**
   - The Siege route table's trench runs along the section's foot, 2 m wide; its report gives it 3 m deep ([SiegeRouteTable.md](SiegeRouteTable.md)). In this test's slump a trench 3 m deep along the whole face drops nothing, and a 4 m one does.
   - Two differences make this. The route table gives T3 a 1 m foundation, and this test gives it none (ambiguity 7). This test also cuts every dig once, trench and quarry alike, and leaves the earth that slumps into it. The route table's script, as it stands in this branch, has its sappers carry out whatever slumps in.
   - Whether a designation keeps taking out the earth that slumps into it is not stated. How earth under a structure slumps is still open ([MvpReadiness-2026-10-31.md](../MvpReadiness-2026-10-31.md):141).
6. **"Terraced" digs against slumping digs.**
   - The quarry is cut "in terraces" and the pit is "a terraced hole" ([GameDescription.md](../../GameDescription.md):103–104).
   - The dig readout, though, shows "where the ground will slump and which structures that would undermine" (:414), and pillar 1 has a quarry slump a foundation.
   - If terraces are stable 1:1 steps, no dig ever undermines anything. In this test the digs that do not slump pass, 2 of 3, through the ditch reason alone.
7. **Foundations and thin soil.**
   - The texts say a template's foundation "reaches down to firm ground" ([GameDescription.md](../../GameDescription.md):133) and "reaches the ground" ([GameDesign.md](../../GameDesign.md):204). T3 draws no foundation voxels.
   - "Thin soil over rock makes strong foundations" ([GameDescription.md](../../GameDescription.md):75) conflicts with [Hud.md](../../Hud.md):257, under which any earth under a wall can be undermined, whatever its depth.
   - With nothing below the surface, thin soil helps only by making a deep foundation cheap, and by limiting how far a slump reaches (berms of 2 m against 3 m here).
8. **The timber span.** "Timber spans furthest" ([GameDescription.md](../../GameDescription.md):188), but no value is recorded. T2's timber floors need 4 m, and T1's timber roof needs 5 m. This test does not need the value, because no dig reaches the keep or the hall.
9. **"How far it is from the towers."** The rule does not say whether the keep counts, whether the gatehouse's garrison counts, or whether the measure is distance or cover.
10. **"Each stretch of wall it can reach."** Reach is not defined, and the old rule's "from where it arrives" was dropped.
11. **The sapper wave's choice.** "Trench beside a section standing on earth" does not say how the sapper wave scores its candidates. This test used the main score, with rule (a).
12. **The gatehouse's own foundation.** At the 2 m span, a gatehouse on earth is the cheapest stretch to undermine (findings). The design gives the gate's weakness as its timber, against rams ([GameDescription.md](../../GameDescription.md):168), not its footing.
13. **A pit's earth spoil.** Only a ditch's spoil has a place, its rampart ([GameDescription.md](../../GameDescription.md):93). This test removes a pit's spoil from play, and a sapper's trench's too.
14. **The keep around the hall.** "The keep, built in stone around the hall" ([GameDescription.md](../../GameDescription.md):280) cannot hold for these templates: T1 is 11 m wide and T2's interior is 8 × 8 m ([MvpAssets.md](../../MvpAssets.md):17–18). The plans put the hall beside the keep, which does not affect the rule. The plans also draw the hall as its 11 × 7 m bounds, not T1's frame, which no dig reaches.
15. **A ditch in front of a gate.** With no crossings in the design, the owner cannot ditch their own gate, so a gate on earth always keeps its earth reason.

**A caveat on the yields.** No single quarry supplies the curtain: they yield 1,143–2,464 m³, against 3,557 m³. A real Hold player digs both outcrops, so the choice is how much to dig near the wall, not near or far.

## Proposed text for GameDesign.md

This is the wording for each rule and value, ready to paste. Nothing here was written into GameDesign.md. The revised rule fails at these values (1 of 3), and the text says so where it matters. The owner decides finding 1 before the rule's weights are confirmed.

**1. The stone span** (Decided, owner, 2026-10-31). In *Where RTS logic meets destruction*, *Materials behave differently* (GameDesign.md:202), after "how far the material spans without support":

> Stone spans 2 m (Decided, 2026-10-31). Every MVP template's stone stands at that span; the keep's floors and roof are timber.

**2. A quarry in front is a ditch in front** (Decided, owner, 2026-10-31). In *Stages*, Hold, *How a wave picks its breach* (GameDesign.md:296), after "whether it stands on earth with no ditch in front":

> A quarry in front of a wall counts as a ditch in front, as pillar 1 says (Decided, 2026-10-31).

**3. The wave rule** (Proposed). It replaces the whole bullet at GameDesign.md:296 and already contains item 2's sentence, so paste item 2 or item 3, not both:

> - **How a wave picks its breach.** A wave scores each stretch of wall it can reach on the four reasons an attacker reads (GameDescription.md, *How a siege plays*): how close the section is to opening, which it asks the collapse resolver; whether it stands on earth with no ditch in front; how far it is from the towers; and whether it is a gate. A quarry in front of a wall counts as a ditch in front (Decided, 2026-10-31). If any stretch it can reach is already open, it picks among the open ones only. Otherwise it picks among the best few with a seeded draw. Only a wave that brings a ram scores a gatehouse; a wave without one leaves the gatehouse out unless it is open. (Proposed; the M0 paper test fails at the values below, 1 layout of 3.)

**4. The rule's values** (Proposed). Under *Starting the MVP*, item 7, or wherever the paper tests' values are kept:

> - **How close to opening** is 1 − T ÷ T₀. T is the least volume a sapper's trench must still dig to drop the section, and T₀ is the same section's need in undug ground. It is 1 when the section is open and 0 when no trench can drop it. (Proposed)
> - **A sapper's trench**, for that reading, is 2 m wide against the section's outer face, along any part of it, to any whole depth above bedrock. It cuts whatever it meets, and its spoil leaves play. (Proposed)
> - **On earth with no ditch in front** is 1 when any column under the stretch is earth and no cut at least 2 m deep lies within 10 m of its face along half its length or more, and 0 otherwise. (Proposed)
> - **How far from the towers** is the distance from the stretch's centre to the nearest tower or keep, centre to centre, divided by the largest such distance among the stretches. The gatehouse is not a tower. (Proposed)
> - **The gate's reason** is 1 for a gatehouse and 0 otherwise. (Proposed)
> - **Weights:** 1 for each reason. (Proposed; at these weights a quarry in front of a wall cancels itself, M0 Hold rule test.)
> - **The best few** are 3. **The draw:** the stretches are shuffled with the wave's seed from SplitMix64, sorted by score with ties in shuffled order, and one of the best 3 is drawn uniformly. (Proposed)
> - **Rams:** of the waves other than the sapper wave, every second one brings a ram, starting with the second. (Proposed)
> - **The sapper wave** brings sappers and no ram. It picks among the sections standing on earth by the same score and the same rule for open sections. (Proposed)
> - **Reach:** a wave can reach every outer stretch, sections and gatehouses; towers are not scored. (Proposed)

**5. The paper test's own inputs** (Proposed). These belong with the test rather than the game, and are listed for completeness:

> - The standard foundation draws nothing below the levelled surface. Digs cut straight down to their depth or to bedrock, and their earth faces then slump; a pit's earth spoil leaves play. (Proposed)
> - The test's quarries are 8 m deep. Outcrops expose building stone in a core, dip 1 m per metre under the soil for 4 m, and go 10 m deep. The near quarry chases the stone to the closest berm at which the slump drops nothing. (Proposed)
> - The breach moves when at least 1/3 of the waves breach somewhere else: the total variation distance between two placements' picks, averaged over waves with and without a ram. (Proposed)

## Script output

```
Hold rule test (review F-45), revised 2026-10-31. Values:
  weights open 1, earth 1, towers 1, gate 1; best few N = 3; stone span 2 m (Decided)
  a ditch in front: a cut 2 m deep within 10 m of the face along 1/2 of the section; quarry pits count (Decided)
  (a) an open stretch always wins: when any candidate is open, the wave draws among the open ones only
  (b) open = 1 - (the least m^3 of trench that drops the stretch now) / (the same, in undug ground), 1 when
      open, 0 when no trench drops it; a trench is 2 m wide against the outer face, along any part of it, any depth
  (c) only a wave that brings a ram scores the gatehouse, unless it is open: 1/2 of waves, numbers 2, 4, 6, 8...
  quarries 8 m deep; the breach moves when 1/3 of waves or more land elsewhere

  The templates (GameData/Server/Templates), on level ground: voxels of stone (palette behaviour
  buildingStone, which masonry has) and of timber (roofs included), and the widest support distance of each
    T1 Hall                 timber  185 voxels, widest 5 m
    T2 Keep                 stone 1526 voxels, widest 0 m;  timber  192 voxels, widest 4 m
    T3 Stone wall, standard stone   96 voxels, widest 0 m
    T4 Tower                stone  477 voxels, widest 2 m
    T5 Gatehouse            stone  585 voxels, widest 2 m;  timber   12 voxels, widest 0 m
  Every template's stone stands at the 2 m stone span: yes (the widest is 2 m).
  The plans' keep, sections, towers and gatehouse have the footprints of T2-T5's bottom layers: yes.
  The plans hold each structure as its footprint on the ground, and T2's bottom layer is stone, so the stone
  span is the one that applies to it. Its timber floors and roof rest on its walls, which no dig here
  reaches.

  The castle every layout shares, 2 m per character (stretches are named inside their wall):
    TTT#############TTT
    TTTT           TTTT
    TTTT N1  N2  N3TTTT
    #TTT           TTT#
    #   KKKKKK        #
    #W3 KKKKKK      E3#
    #   KKKKKK        #
    #   KKKKKK        #
    #   KKKKKK        #
    #W2 KKKKKK      E2#
    #   KKKKKK        #
    #     KK          #
    # HHHHHH          #
    #W1HHHHH        E1#
    # HHHHHH          #
    #THHHHHHG GG    TT#T
    TTTT S1GG GG S2 TTTT
    TTTT   GG GG    TTTT
    TTT####GGGGG####TTTT

  Plan legend:
    #  wall section    G  gatehouse    T  tower    K  keep    H  hall
    !  structure with a gap under it (undercut)    X  a piece that fell
    q  quarry pit    v  ditch    ^  rampart    ~  ground lowered by slump
    o  building stone at the surface    :  rock at the surface    ,  earth 1-2 m    .  earth 3 m or more

=== Layout A: an outcrop inside the east curtain ===
  East and south stand on 4 m of earth over rock, with bedrock at 12 m. North and west stand on a rock
  shelf, with bedrock at 8 m. One outcrop lies inside the curtain, between the keep and E1-E3. The other
  lies 19 m west of the west wall, on the shelf.

  Plan, 2 m per character, north up; every placement's digs drawn at once:
    ::::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::TTT#############TTT:::::::
    ::::::::::::::::::TTT#############TTT:::::::
    ::::::::::::::::::TTTT:::::::::::TTTT:::::::
    :oooooooooo:::::::TTTT:::::::::::TTTT:::::::
    :oooooooooo:::::::#:::...........~..#^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~~~~~~~~#^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~#^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::KKKKKK~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:::..KK..~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:HHHHKK..~qqqqqq~!^^vvv..
    :oqqqqqqqqo:::::::#:HHHHHH..~qqqqqq~#^^vvv..
    :oqqqqqqqqo:::::::#:HHHHHH...~~~~~~.#^^vvv..
    :oqqqqqqqqo:::::::#:HHHHHH..........#^^vvv..
    :oooooooooo:::::::TTTT...GG.GG....TTTT..~...
    :oooooooooo:::::::TTTT...GG.GG....TTTT......
    ::::::::::::::::::TTT#####G.GG####TTTT......
    ::::::::::::::::::TTT#####GGGG####TTTT......
    ::::::::::::::::::::::......................
    ::::::::::::::::::::::......................
    ::::::::::::::::::::::......................
    ::::::::::::::::::::::......................
    ::::::::::::::::::::::......................

  Ground under each stretch (generated): earth m / bedrock m
     N1 rock    0/8     N2 rock    0/8     N3 rock    0/8     E1 earth   4/12
     E2 earth   4/12    E3 earth   4/12    S2 earth   4/12     G earth   4/12
     S1 earth   4/12    W1 rock    0/8     W2 rock    0/8     W3 rock    0/8

  A1  near: the inner outcrop, chased east towards E1-E3
      berm 3 m; dug 1320 m^3 (earth 177, rock 1143), building stone 1143 m^3
  A2  far: the west outcrop
      dug 2464 m^3 (earth 0, rock 2464), building stone 2464 m^3
  A3  far, plus a ditch and rampart in front of E1-E3
      dug 2464 m^3 (earth 0, rock 2464), building stone 2464 m^3
      The curtain costs 3557 m^3 of stone.

  Scores: open + earth + towers + gate = score. A wave without a ram reads the same score for every section
  and leaves the gatehouse out unless it is open (rule (c)). * marks the best few of a wave without a ram,
  + of a wave with one. ditch: a ditch in front, which zeroes earth.
    stretch | A1                           | A2                           | A3
    N1      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49
    N2      |  .00 0  .93 0 =  .93          |  .00 0  .93 0 =  .93          |  .00 0  .93 0 =  .93
    N3      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49
    E1      |  .33 1  .47 0 = 1.81*         |  .00 1  .47 0 = 1.48          |  .00 0  .47 0 =  .47   ditch
    E2      |  .67 1  .97 0 = 2.64*+        |  .00 1  .97 0 = 1.97*+        |  .00 0  .97 0 =  .97*  ditch
    E3      |  .33 1  .49 0 = 1.83*+        |  .00 1  .49 0 = 1.49*+        |  .00 0  .49 0 =  .49   ditch
    S2      |  .00 1  .49 0 = 1.49          |  .00 1  .49 0 = 1.49*+        |  .00 1  .49 0 = 1.49*+
    G       |  .00 1 1.00 1 = 3.00 +        |  .00 1 1.00 1 = 3.00 +        |  .00 1 1.00 1 = 3.00 +
    S1      |  .00 1  .49 0 = 1.49          |  .00 1  .49 0 = 1.49*+        |  .00 1  .49 0 = 1.49*+
    W1      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49
    W2      |  .00 0  .85 0 =  .85          |  .00 0  .85 0 =  .85          |  .00 0  .85 0 =  .85
    W3      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49

  The open term's trench: the least m^3 a sapper would still dig to drop the stretch, now / in undug ground
  (-: no trench 2 m wide beside the face drops it). gap: columns with a gap under them; sup: the
  widest support distance in m, against the 2 m span.
    stretch | A1                           | A2                           | A3
    N1      |     -/-                      |     -/-                      |     -/-
    N2      |     -/-                      |     -/-                      |     -/-
    N3      |     -/-                      |     -/-                      |     -/-
    E1      |   24/36  gap 1 sup1          |   36/36                      |   44/36
    E2      |   12/36  gap 8 sup1          |   36/36                      |   45/36
    E3      |   24/36  gap 2 sup1          |   36/36                      |   45/36
    S2      |   42/42                      |   42/42                      |   42/42
    G       |   24/24                      |   24/24                      |   24/24
    S1      |   40/40                      |   40/40                      |   40/40
    W1      |     -/-                      |     -/-                      |     -/-
    W2      |     -/-                      |     -/-                      |     -/-
    W3      |     -/-                      |     -/-                      |     -/-

  The breach: a uniform draw over the best 3, chance in brackets. Waves 1-8 draw with seeds 1-8;
  waves 2, 4, 6, 8 bring a ram.
    A1  no ram: E2 2.64 (1/3), E3 1.83 (1/3), E1 1.81 (1/3)
        ram:    G 3.00 (1/3), E2 2.64 (1/3), E3 1.83 (1/3)
        waves:  E2 E2 E2 G  E2 G  E3 E2
        sapper wave (sections on earth): E2 2.64 (1/3), E3 1.83 (1/3), E1 1.81 (1/3)
        sapper seeds: E2 E3 E2 E3 E3 E2 E3 E3
    A2  no ram: E2 1.97 (1/3), E3 1.49 (2/9), S2 1.49 (2/9), S1 1.49 (2/9)
        ram:    G 3.00 (1/3), E2 1.97 (1/3), E3 1.49 (1/9), S2 1.49 (1/9), S1 1.49 (1/9)
        waves:  E2 E2 E2 G  E2 G  S2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), E3 1.49 (2/9), S2 1.49 (2/9), S1 1.49 (2/9)
        sapper seeds: E2 S2 E2 E3 S1 E2 S1 S1
    A3  no ram: S2 1.49 (1/3), S1 1.49 (1/3), E2 .97 (1/3)
        ram:    G 3.00 (1/3), S2 1.49 (1/3), S1 1.49 (1/3)
        waves:  S1 S1 S2 G  S2 G  S1 S1
        sapper wave (sections on earth): S2 1.49 (1/3), S1 1.49 (1/3), E2 .97 (1/3)
        sapper seeds: S1 S1 S1 S2 S2 S2 S2 S2

  Does the breach move? (share of waves that breach elsewhere, without a ram, with one, and over all; it
  moves at 1/3 or more)
    A1 vs A2: no ram  4/9, ram  2/9, all   1/3, moves          sapper wave  4/9 <- the quarry pair
    A1 vs A3: no ram  2/3, ram  2/3, all   2/3, moves          sapper wave  2/3
    A2 vs A3: no ram  2/9, ram  4/9, all   1/3, moves          sapper wave  2/9

=== Layout B: an outcrop in front of an earth wall ===
  The curtain stands on 3 m of earth, with bedrock at 10 m, except the west side, which stands on a rock
  shelf. One outcrop lies 8 m north of N1-N3; the other lies 15 m east of E1-E3.

  Plan, 2 m per character, north up; every placement's digs drawn at once:
    ::::::::::...............................
    ::::::::::.~~~~~~~~~.....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::::::~qqqqqqqqq~....................
    ::::::TTT##!!!!!!!!###TTT................
    ::::::TTT#############TTT................
    ::::::TTTT...........TTTT................
    ::::::TTTT...........TTTT................
    ::::::#:::..............#......~~~~~~~~..
    ::::::#:::KKKKKK........#.....~qqqqqqqq~.
    ::::::#:::KKKKKK........#.....~qqqqqqqq~.
    ::::::#:::KKKKKK........#.....~qqqqqqqq~.
    ::::::#:::KKKKKK........#.....~qqqqqqqq~.
    ::::::#:::KKKKKK........#.....~qqqqqqqq~.
    ::::::#:::KKKKKK........#.....~qqqqqqqq~.
    ::::::#:::..KK..........#.....~qqqqqqqq~.
    ::::::#:HHHHKK..........#.....~qqqqqqqq~.
    ::::::#:HHHHHH..........#.....~qqqqqqqq~.
    ::::::#:HHHHHH..........#.....~qqqqqqqq~.
    ::::::#:HHHHHH..........#.....~qqqqqqqq~.
    ::::::TTTT...GG.GG....TTTT.....~~~~~~~~..
    ::::::TTTT...GG.GG....TTTT...............
    ::::::TTT#####G.GG####TTTT...............
    ::::::TTT#####GGGG####TTTT...............
    ::::::::::...............................
    ::::::::::...............................
    ::::::::::...............................
    ::::::::::...............................
    ::::::::::...............................
  B3's ditch is not drawn: it lies where the near quarry is. It runs along the N face, 7-30: rampart 1-4 m out, berm 5 m, ditch 6-9 m.

  Ground under each stretch (generated): earth m / bedrock m
     N1 earth   3/10    N2 earth   3/10    N3 earth   3/10    E1 earth   3/10
     E2 earth   3/10    E3 earth   3/10    S2 earth   3/10     G earth   3/10
     S1 earth   3/10    W1 rock    0/8     W2 rock    0/8     W3 rock    0/8

  B1  near: the north outcrop, chased south towards N1-N3
      berm 2 m; dug 2304 m^3 (earth 232, rock 2072), building stone 1992 m^3
  B2  far: the east outcrop
      dug 2464 m^3 (earth 68, rock 2396), building stone 2396 m^3
  B3  far, plus a ditch and rampart in front of N1-N3
      dug 2464 m^3 (earth 68, rock 2396), building stone 2396 m^3
      The curtain costs 3557 m^3 of stone.

  Scores: open + earth + towers + gate = score. A wave without a ram reads the same score for every section
  and leaves the gatehouse out unless it is open (rule (c)). * marks the best few of a wave without a ram,
  + of a wave with one. ditch: a ditch in front, which zeroes earth.
    stretch | B1                           | B2                           | B3
    N1      |  .93 0  .49 0 = 1.42   ditch  |  .00 1  .49 0 = 1.49*         |  .00 0  .49 0 =  .49   ditch
    N2      |  .93 0  .93 0 = 1.85*+ ditch  |  .00 1  .93 0 = 1.93*+        |  .00 0  .93 0 =  .93   ditch
    N3      |  .88 0  .49 0 = 1.37   ditch  |  .00 1  .49 0 = 1.49*         |  .00 0  .49 0 =  .49   ditch
    E1      |  .00 1  .47 0 = 1.48          |  .00 1  .47 0 = 1.48          |  .00 1  .47 0 = 1.48
    E2      |  .00 1  .97 0 = 1.97*+        |  .00 1  .97 0 = 1.97*+        |  .00 1  .97 0 = 1.97*+
    E3      |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*+
    S2      |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*+
    G       |  .00 1 1.00 1 = 3.00 +        |  .00 1 1.00 1 = 3.00 +        |  .00 1 1.00 1 = 3.00 +
    S1      |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*+
    W1      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49
    W2      |  .00 0  .85 0 =  .85          |  .00 0  .85 0 =  .85          |  .00 0  .85 0 =  .85
    W3      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49

  The open term's trench: the least m^3 a sapper would still dig to drop the stretch, now / in undug ground
  (-: no trench 2 m wide beside the face drops it). gap: columns with a gap under them; sup: the
  widest support distance in m, against the 2 m span.
    stretch | B1                           | B2                           | B3
    N1      |    3/40  gap 4 sup1          |   40/40                      |   52/40
    N2      |    3/40  gap 8 sup1          |   40/40                      |   55/40
    N3      |    5/40  gap 3 sup1          |   40/40                      |   54/40
    E1      |   36/36                      |   36/36                      |   36/36
    E2      |   36/36                      |   36/36                      |   36/36
    E3      |   36/36                      |   36/36                      |   36/36
    S2      |   42/42                      |   42/42                      |   42/42
    G       |   24/24                      |   24/24                      |   24/24
    S1      |   40/40                      |   40/40                      |   40/40
    W1      |     -/-                      |     -/-                      |     -/-
    W2      |     -/-                      |     -/-                      |     -/-
    W3      |     -/-                      |     -/-                      |     -/-

  The breach: a uniform draw over the best 3, chance in brackets. Waves 1-8 draw with seeds 1-8;
  waves 2, 4, 6, 8 bring a ram.
    B1  no ram: E2 1.97 (1/3), N2 1.85 (1/3), E3 1.49 (1/9), S2 1.49 (1/9), S1 1.49 (1/9)
        ram:    G 3.00 (1/3), E2 1.97 (1/3), N2 1.85 (1/3)
        waves:  E2 E2 E2 G  E2 G  N2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), N2 1.85 (1/3), E3 1.49 (1/9), S2 1.49 (1/9), S1 1.49 (1/9)
        sapper seeds: E2 E3 N2 S2 E2 E2 E2 E2
    B2  no ram: E2 1.97 (1/3), N2 1.93 (1/3), N1 1.49 (1/15), N3 1.49 (1/15), E3 1.49 (1/15), S2 1.49 (1/15), S1 1.49 (1/15)
        ram:    G 3.00 (1/3), E2 1.97 (1/3), N2 1.93 (1/3)
        waves:  E2 E2 E2 G  E2 G  N2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), N2 1.93 (1/3), N1 1.49 (1/15), N3 1.49 (1/15), E3 1.49 (1/15), S2 1.49 (1/15), S1 1.49 (1/15)
        sapper seeds: E2 E3 N2 S2 E2 E2 E2 E2
    B3  no ram: E2 1.97 (1/3), E3 1.49 (2/9), S2 1.49 (2/9), S1 1.49 (2/9)
        ram:    G 3.00 (1/3), E2 1.97 (1/3), E3 1.49 (1/9), S2 1.49 (1/9), S1 1.49 (1/9)
        waves:  E2 E2 E2 G  E2 G  S2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), E3 1.49 (2/9), S2 1.49 (2/9), S1 1.49 (2/9)
        sapper seeds: E2 S1 S1 E3 E2 E2 E2 E2

  Does the breach move? (share of waves that breach elsewhere, without a ram, with one, and over all; it
  moves at 1/3 or more)
    B1 vs B2: no ram 2/15, ram    0, all  1/15, does not move  sapper wave 2/15 <- the quarry pair
    B1 vs B3: no ram  1/3, ram  1/3, all   1/3, moves          sapper wave  1/3
    B2 vs B3: no ram 7/15, ram  1/3, all   2/5, moves          sapper wave 7/15

=== Layout C: outcrops by an earth wall and by a rock wall ===
  East and north stand on 4 m of earth, with bedrock at 10 m. West and south, the gate side, stand on a rock
  shelf, with bedrock at 8 m. One outcrop lies 8 m east of E1-E3, in the earth. The other lies 5 m west
  of W1-W3, on the shelf.

  Plan, 2 m per character, north up; every placement's digs drawn at once:
    ::::::::::::::............................
    ::::::::::::::............................
    ::::::::::::::............................
    ::::::::::::::............................
    ::::::::::::::............................
    ::::::::::::::............................
    ::::::::::TTT#############TTT.............
    ::::::::::TTT#############TTT.............
    ::::::::::TTTT...........TTTT.............
    ::::::::::TTTT...........TTTT.............
    :ooooooooo#:::..............#.~~~~~~~~~...
    :oqqqqqqqo#:::KKKKKK........#~qqqqqqqqq~..
    :oqqqqqqqo#:::KKKKKK........!~qqqqqqqqq~..
    :oqqqqqqqo#:::KKKKKK........!~qqqqqqqqq~..
    :oqqqqqqqo#:::KKKKKK........!~qqqqqqqqq~..
    :oqqqqqqqo#:::KKKKKK........!~qqqqqqqqq~..
    :oqqqqqqqo#:::KKKKKK........!~qqqqqqqqq~..
    :oqqqqqqqo#:::..KK..........!~qqqqqqqqq~..
    :oqqqqqqqo#:HHHHKK..........!~qqqqqqqqq~..
    :oqqqqqqqo#:HHHHHH..........!~qqqqqqqqq~..
    :oqqqqqqqo#:HHHHHH..........#~qqqqqqqqq~..
    :ooooooooo#:HHHHHH..........#.~~~~~~~~~...
    ::::::::::TTTT:::GG:GG::::TTTT::::::::::::
    ::::::::::TTTT:::GG:GG::::TTTT::::::::::::
    ::::::::::TTT#####G:GG####TTTT::::::::::::
    ::::::::::TTT#####GGGG####TTTT::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::
    ::::::::::::::::::::::::::::::::::::::::::
  C3's ditch is not drawn: it lies where the near quarry is. It runs along the E face, 7-30: rampart 1-4 m out, berm 5 m, ditch 6-9 m.

  Ground under each stretch (generated): earth m / bedrock m
     N1 earth   4/10    N2 earth   4/10    N3 earth   4/10    E1 earth   4/10
     E2 earth   4/10    E3 earth   4/10    S2 rock    0/8      G rock    0/8
     S1 rock    0/8     W1 rock    0/8     W2 rock    0/8     W3 rock    0/8

  C1  near the earth wall: the east outcrop, chased west towards E1-E3
      berm 3 m; dug 2448 m^3 (earth 222, rock 2226), building stone 2226 m^3
  C2  near the rock wall: the west outcrop
      dug 1728 m^3 (earth 0, rock 1728), building stone 1728 m^3
  C3  the west outcrop, plus a ditch and rampart in front of E1-E3
      dug 1728 m^3 (earth 0, rock 1728), building stone 1728 m^3
      The curtain costs 3557 m^3 of stone.

  Scores: open + earth + towers + gate = score. A wave without a ram reads the same score for every section
  and leaves the gatehouse out unless it is open (rule (c)). * marks the best few of a wave without a ram,
  + of a wave with one. ditch: a ditch in front, which zeroes earth.
    stretch | C1                           | C2                           | C3
    N1      |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*+
    N2      |  .00 1  .93 0 = 1.93*+        |  .00 1  .93 0 = 1.93*+        |  .00 1  .93 0 = 1.93*+
    N3      |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*         |  .00 1  .49 0 = 1.49*+
    E1      |  .89 0  .47 0 = 1.36   ditch  |  .00 1  .47 0 = 1.48          |  .00 0  .47 0 =  .47   ditch
    E2      |  .92 0  .97 0 = 1.89*+ ditch  |  .00 1  .97 0 = 1.97*+        |  .00 0  .97 0 =  .97   ditch
    E3      |  .92 0  .49 0 = 1.41   ditch  |  .00 1  .49 0 = 1.49*         |  .00 0  .49 0 =  .49   ditch
    S2      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49
    G       |  .00 0 1.00 1 = 2.00 +        |  .00 0 1.00 1 = 2.00 +        |  .00 0 1.00 1 = 2.00 +
    S1      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49
    W1      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49   ditch  |  .00 0  .49 0 =  .49   ditch
    W2      |  .00 0  .85 0 =  .85          |  .00 0  .85 0 =  .85   ditch  |  .00 0  .85 0 =  .85   ditch
    W3      |  .00 0  .49 0 =  .49          |  .00 0  .49 0 =  .49   ditch  |  .00 0  .49 0 =  .49   ditch

  The open term's trench: the least m^3 a sapper would still dig to drop the stretch, now / in undug ground
  (-: no trench 2 m wide beside the face drops it). gap: columns with a gap under them; sup: the
  widest support distance in m, against the 2 m span.
    stretch | C1                           | C2                           | C3
    N1      |   40/40                      |   40/40                      |   40/40
    N2      |   40/40                      |   40/40                      |   40/40
    N3      |   40/40                      |   40/40                      |   40/40
    E1      |    4/36  gap 4 sup1          |   36/36                      |   45/36
    E2      |    3/36  gap 8 sup1          |   36/36                      |   45/36
    E3      |    3/36  gap 4 sup1          |   36/36                      |   44/36
    S2      |     -/-                      |     -/-                      |     -/-
    G       |     -/-                      |     -/-                      |     -/-
    S1      |     -/-                      |     -/-                      |     -/-
    W1      |     -/-                      |     -/-                      |     -/-
    W2      |     -/-                      |     -/-                      |     -/-
    W3      |     -/-                      |     -/-                      |     -/-

  The breach: a uniform draw over the best 3, chance in brackets. Waves 1-8 draw with seeds 1-8;
  waves 2, 4, 6, 8 bring a ram.
    C1  no ram: N2 1.93 (1/3), E2 1.89 (1/3), N1 1.49 (1/6), N3 1.49 (1/6)
        ram:    G 2.00 (1/3), N2 1.93 (1/3), E2 1.89 (1/3)
        waves:  N2 N2 N2 G  N2 G  E2 N2
        sapper wave (sections on earth): N2 1.93 (1/3), E2 1.89 (1/3), N1 1.49 (1/6), N3 1.49 (1/6)
        sapper seeds: N1 N2 E2 E2 E2 N1 N2 N2
    C2  no ram: E2 1.97 (1/3), N2 1.93 (1/3), N1 1.49 (1/9), N3 1.49 (1/9), E3 1.49 (1/9)
        ram:    G 2.00 (1/3), E2 1.97 (1/3), N2 1.93 (1/3)
        waves:  E2 E2 E2 G  E2 G  N2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), N2 1.93 (1/3), N1 1.49 (1/9), N3 1.49 (1/9), E3 1.49 (1/9)
        sapper seeds: N1 E2 N2 N2 N2 N1 E2 E2
    C3  no ram: N2 1.93 (1/3), N1 1.49 (1/3), N3 1.49 (1/3)
        ram:    G 2.00 (1/3), N2 1.93 (1/3), N1 1.49 (1/6), N3 1.49 (1/6)
        waves:  N2 N2 N2 G  N2 G  N1 N2
        sapper wave (sections on earth): N2 1.93 (1/3), N1 1.49 (1/3), N3 1.49 (1/3)
        sapper seeds: N3 N2 N1 N3 N1 N3 N2 N2

  Does the breach move? (share of waves that breach elsewhere, without a ram, with one, and over all; it
  moves at 1/3 or more)
    C1 vs C2: no ram  1/9, ram    0, all  1/18, does not move  sapper wave  1/9 <- the quarry pair
    C1 vs C3: no ram  1/3, ram  1/3, all   1/3, moves          sapper wave  1/3
    C2 vs C3: no ram  4/9, ram  1/3, all  7/18, moves          sapper wave  4/9

=== Verdict: the breach moves with quarry placement in 1 of 3 layouts: FAIL (pass needs 2) ===

=== The near quarry at every berm (Proposed rules otherwise) ===
  berm m: the open term of each stretch that reads above 0 (FALL: the slump drops it); the best few of a
  wave without a ram | with one, ties at the cut included; and the share of waves that breach elsewhere
  than under the quarry pair's other placement
  A1 (the player's berm is 3 m):
    1: E1 FALL, E2 FALL, E3 FALL          E1, E2, E3 | E1, E2, E3              elsewhere 1/2
    2: E1 .89, E2 FALL, E3 FALL           E2, E3 | E2, E3                      elsewhere 1/2
    3: E1 .33, E2 .67, E3 .33             E1, E2, E3 | E2, E3, G               elsewhere 1/3
    4: every stretch reads 0              E2, E3, S2, S1 | E2, E3, S2, G, S1   elsewhere 0
    5: every stretch reads 0              E2, E3, S2, S1 | E2, E3, S2, G, S1   elsewhere 0
    6: every stretch reads 0              E2, E3, S2, S1 | E2, E3, S2, G, S1   elsewhere 0
  B1 (the player's berm is 2 m):
    1: N1 FALL, N2 FALL, N3 FALL          N1, N2, N3 | N1, N2, N3              elsewhere 3/5
    2: N1 .93, N2 .93, N3 .88             N2, E2, E3, S2, S1 | N2, E2, G       elsewhere 1/15
    3: N1 .47, N2 .50, N3 .45             E2, E3, S2, S1 | E2, E3, S2, G, S1   elsewhere 2/5
    4: N1 .10, N2 .12, N3 .07             E2, E3, S2, S1 | E2, E3, S2, G, S1   elsewhere 2/5
    5: every stretch reads 0              E2, E3, S2, S1 | E2, E3, S2, G, S1   elsewhere 2/5
    6: every stretch reads 0              E2, E3, S2, S1 | E2, E3, S2, G, S1   elsewhere 2/5
  C1 (the player's berm is 3 m):
    1: E1 FALL, E2 FALL, E3 FALL          E1, E2, E3 | E1, E2, E3              elsewhere 11/18
    2: E1 FALL, E2 FALL, E3 FALL          E1, E2, E3 | E1, E2, E3              elsewhere 11/18
    3: E1 .89, E2 .92, E3 .92             N1, N2, N3, E2 | N2, E2, G           elsewhere 1/18
    4: E1 .39, E2 .58, E3 .58             N1, N2, N3, E2 | N2, E2, G           elsewhere 1/18
    5: every stretch reads 0              N1, N2, N3 | N1, N2, N3, G           elsewhere 7/18
    6: every stretch reads 0              N1, N2, N3 | N1, N2, N3, G           elsewhere 7/18

=== Sensitivity: one value changed at a time; share of waves elsewhere for each quarry pair ===
    rules                           A1 v A2    B1 v B2    C1 v C2    moved   verdict
    Proposed                        1/3        1/15       1/18       1 of 3  FAIL
    open weight x2                  1/3        13/30      11/18      3 of 3  PASS
    open weight x1/2                1/3        2/5        7/18       3 of 3  PASS
    earth weight x2                 1/3        2/5        4/9        3 of 3  PASS
    earth weight x1/2               1/3        2/15       1/9        1 of 3  FAIL
    towers weight x2                1/3        1/15       1/18       1 of 3  FAIL
    towers weight x1/2              1/3        1/15       1/18       1 of 3  FAIL
    gate weight x2                  1/3        1/15       1/18       1 of 3  FAIL
    gate weight x1/2                1/3        1/15       1/18       1 of 3  FAIL
    best few N = 2                  1/6        0          1/4        0 of 3  FAIL
    best few N = 4                  7/24       3/20       1/8        0 of 3  FAIL
    stone span 4 m (the old value)  1/3        1/15       1/18       1 of 3  FAIL
    stone span 6 m                  0          2/5        7/18       2 of 3  PASS
    towers term over range          1/3        1/15       1/18       1 of 3  FAIL
    only ditches are ditches        1/3        13/30      11/18      3 of 3  PASS
    digs do not slump               0          2/5        7/18       2 of 3  PASS
    moves at 1/2 of waves           1/3        1/15       1/18       0 of 3  FAIL
    (a) open first, intact fill     1/3        1/15       1/18       1 of 3  FAIL
    (b) trench 1 m wide             1/3        1/15       1/18       1 of 3  FAIL
    (c) rams with 3 waves in 4      5/18       1/30       1/36       0 of 3  FAIL
    only ditches, open x2           1/3        3/5        11/18      3 of 3  PASS

=== Each revision undone or read another way, at the decided span and ditch reading ===
    rules                           A1 v A2    B1 v B2    C1 v C2    moved   verdict
    revised: (a), (b) and (c)       1/3        1/15       1/18       1 of 3  FAIL
    without (a): score alone        1/3        1/15       1/18       1 of 3  FAIL
    without (b): support reading    1/3        2/5        7/18       3 of 3  PASS
    without (c): a ram every wave   2/9        0          0          0 of 3  FAIL
    (c) as the gate's reason only   2/9        0          1/18       0 of 3  FAIL
    none of the three               2/9        1/3        1/3        2 of 3  PASS
    none, span 4 m (the first run)  2/9        1/3        1/3        2 of 3  PASS
```
