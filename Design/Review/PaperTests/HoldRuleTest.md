# Hold rule test (F-45), 2026-10-31

**What this is.** This is the M0 paper test of the revised Hold wave rule ([MvpPlan.md](../../MvpPlan.md), M0). It is the review's "Cheapest tests" item 2, for finding F-45 ([GameDescription-2026-10-31.md](../GameDescription-2026-10-31.md)).

**The question.** Does where the player quarries change where the waves breach?

**Pass.** The breach moves with quarry placement in at least two of three layouts.

**The script.** [`Tools/PaperTests/HoldRuleTest.py`](../../../Tools/PaperTests/HoldRuleTest.py) is Python 3, standard library only. Run it with `python Tools/PaperTests/HoldRuleTest.py`, or add `--scale 1` for plans at 1 m per character. Its scores are exact fractions and its draw is seeded, so it prints the same output on every machine. Its full output is at the end of this report.

## Verdict

**Pass, narrowly, at the Proposed values: the breach moves in 2 of 3 layouts, B and C.** Read the pass with three cautions:

- **Both passes sit exactly on the bar.** In B and C, 1/3 of the waves breach somewhere else. At a stricter bar, where most waves must land elsewhere, no layout moves.
- **Both passes come through the ditch reason, and they push the waves away from the quarry.** A quarry dug in front of an earth wall counts as a ditch in front. That zeroes the wall's "on earth with no ditch" reason, and the waves go elsewhere. The same quarry also slumps the wall's foundation, and the rule barely sees that.
- **Layout A does not move (2/9).** There the quarry is inside the curtain, so it can only undermine the wall.

The pass is fragile. 6 of the 16 single-value changes in the sensitivity table turn it into a fail. One of them is reading "ditch" literally, as a ditch designation only, which leaves 0 of 3 layouts moving.

## How the rule was run

**The decided rule** ([GameDesign.md](../../GameDesign.md):296). Each wave scores every stretch of wall it can reach on four reasons:

- how close the stretch is to opening, which it asks the collapse resolver;
- whether it stands on earth with no ditch in front;
- how far it is from the towers;
- whether it is a gate.

It then picks among the best few with a seeded draw.

As run:

- **score** = open + earth + towers + gate, each term from 0 to 1, with equal weights;
  - **open** is the stand-in for the resolver, defined below;
  - **earth** is 1 if any column under the stretch is earth and there is no ditch in front, and 0 otherwise;
  - **towers** is the distance to the nearest tower divided by the largest such distance among the stretches;
  - **gate** is 1 for the gatehouse and 0 otherwise.
- **The draw.** The stretches are shuffled by the wave's seed, sorted by score (ties keep their shuffled order), and the best 3 are kept. One of the three is then drawn uniformly. Ties at the cut are therefore split evenly. Chances are computed exactly, and waves seeded 1 to 8 are printed.
- **The sapper wave** uses the same score over sections on earth only. The decided text has sappers "trench beside a section standing on earth" ([GameDesign.md](../../GameDesign.md):297), so the gatehouse is not a candidate.
- **"The breach moves"** when at least 1/3 of the waves breach somewhere else. That share is the total variation distance between two placements' pick distributions. With a uniform draw over three, 1/3 is the smallest change that replaces a candidate rather than splitting a tie.
- **Which placements are compared.** The verdict compares each layout's quarry pair: the same stone dug in two places. The ditch placements are a separate choice and are reported as context.

**The stand-in for the resolver** applies [ADR-004](../../ADR/ADR-004-rule-based-destruction.md) decision 2 to a structure standing on a height field of 1 m columns:

1. **Slump.** Earth slumps until no earth column stands more than one voxel above its neighbor. It moves one voxel at a time to the lower neighbor, in a fixed order, so it is conserved. Rock holds vertical faces. Earth under a structure slumps like any other earth, and the structure stays where it is.
2. **Grounded columns.** A footprint column whose ground still touches the structure's base is grounded. Every other footprint column takes the fewest sideways steps, through the joined curtain's footprint, to a grounded column. That is its support distance. There are no tunnels, so every solid column is continuous down to its anchor, and the anchor depth never enters.
3. **The open term** is the widest support distance in the stretch divided by (span + 1). When the widest support distance is more than the span, the stretch falls and open is 1.

**Changes after the first run.** None of them changed a value or a layout:

- B3's ditch crossed the north outcrop and stopped the run, so a ditch now cuts whatever it meets.
- The sapper wave was limited to sections, as quoted above.
- The rest was presentation.

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

## Proposed values

GameDesign.md (2026-10-31, *Starting the MVP*, item 7) leaves these to the implementer, as Proposed with a reason; the readiness review listed the gaps ([MvpReadiness-2026-10-31.md](../MvpReadiness-2026-10-31.md):45). MvpPlan.md says they are recorded in GameDesign.md. This test modifies no existing file, so they are recorded here only.

| Value | Proposed | Reason |
|---|---|---|
| Weights | 1, 1, 1, 1 | The design names four reasons and ranks none of them. Every term is scaled to 0–1. |
| The best few, N | 3 | The smallest number that is "few" rather than a pair. |
| The draw | Uniform over the best N, from SplitMix64 seeded with the wave's seed | The simplest seeded draw, from a pinned PRNG ([AGENTS.md](../../../AGENTS.md) R16). |
| The tie-break | Tied stretches are shuffled by the same seed before the cut | A fixed order, such as by name, would favor sections for a reason no attacker reads. |
| Stone span | 4 m | The smallest span at which T2 stands. Its dressed-stone floors cover an 8 m interior, so its widest support distance is 4. T5 needs 2 m, for its 3 m gate, and T3 needs 0. This was computed from the templates in `GameData/Server/Templates/`. |
| The open term | Widest support distance ÷ (span + 1); 1 if more than the span | ADR-004 decision 2 read directly. It is 0 when intact, rises in fifths, and is 1 when the resolver would drop the stretch. |
| Standard foundation | Nothing below the levelled surface | T3 draws no foundation voxels. [Hud.md](../../Hud.md):257 says any earth under a wall can be undermined, whatever the soil depth. |
| How digs cut | Straight down to their depth or to bedrock, after which their earth faces slump. A pit's earth spoil leaves play. | Without this a dig could never undermine anything, and the slump preview would be empty ([GameDescription.md](../../GameDescription.md):414). |
| Quarry depth | 8 m | Each quarry then yields a third to two-thirds of the curtain. |
| Outcrops | Building stone exposed in a core, dipping 1 m per metre under the soil for 4 m, and 10 m deep | "Outcrops dip under the soil at their edges" ([GameDescription.md](../../GameDescription.md):75). |
| The near quarry | Chases the dipping stone towards the wall up to the closest berm at which the slump drops nothing. That came out at 3 m in 4 m of earth and 2 m in 3 m. | This is a player using the slump preview, which is the case pillar 1 warns of. The script finds the berm itself. |
| On earth | Any column of the footprint has earth under it | [Hud.md](../../Hud.md):257, GROUND. |
| A ditch in front | A cut at least 2 m deep within 10 m of the face, along at least half the section's length. Any cut counts, a quarry pit included. | 2 m is the standard ditch's depth. 10 m spans ditch, berm and rampart (9 m). A wave reads voxels, not designations (ADR-003), and pillar 1 calls a quarry in front "a ditch an attacker has to cross". |
| The ditch | Rampart 1, 2, 2, 1 m against the face, a 1 m berm, then the ditch 1, 2, 2, 1 m deep | The ditch profile is Decided ([MvpAssets.md](../../MvpAssets.md):60). The rampart holds exactly its spoil, and the berm keeps both from slumping. |
| Towers | T4 towers and the keep's great tower, not the gatehouse. Distance is centre to centre, to the nearest one. | Towers and keeps garrison and fire. The gatehouse's garrison fires into its own passage ([GameDescription.md](../../GameDescription.md):168, :217). |
| Reach | Every outer stretch | Quarries leave a ramp, and foot soldiers cross ditches and ramparts. |
| Stretches scored | The 11 sections and the gatehouse; towers are not scored | They are the "stretches of wall". |
| The bar for "moves" | 1/3 of waves breaching elsewhere | Defined above. A stricter bar of 1/2 is reported with the results. |
| The castle and ground | As drawn above. Earth is 3–4 m deep, bedrock 8–12 m, with rock shelves. | A plausible Keep-era curtain on mixed ground (MvpPlan.md, M6). |

## Results

Each wave picks from the best few; their scores and chances are given in the second column.

| Layout | Placement | Best few: score (chance) | Waves seeded 1–8 | Sapper wave's best few |
|---|---|---|---|---|
| A | A1 near, inside | G 3.00 (1/3), E2 2.17 (1/3), E3 1.69 (1/3) | E2 E2 G G E2 G E2 E2 | E2, E3, E1 |
| A | A2 far | G 3.00 (1/3), E2 1.97 (1/3), E3, S2 and S1 at 1.49 (1/9 each) | E2 E2 G G E2 G E2 E2 | E2, then E3, S2 and S1 tied |
| A | A3 far + ditch | G 3.00 (1/3), S2 1.49 (1/3), S1 1.49 (1/3) | S2 S1 G G S1 G S1 S1 | S2, S1, E2 |
| B | B1 near, in front | G 3.00 (1/3), E2 1.97 (1/3), E3, S2 and S1 at 1.49 (1/9 each) | E2 E2 G G E2 G E2 E2 | E2, then E3, S2 and S1 tied |
| B | B2 far | G 3.00 (1/3), E2 1.97 (1/3), N2 1.93 (1/3) | E2 E2 G G E2 G E2 E2 | E2, N2, then five tied |
| B | B3 far + ditch | Same as B1 | Same as B1 | Same as B1 |
| C | C1 by the earth wall | G 2.00 (1/3), N2 1.93 (1/3), N1 and N3 at 1.49 (1/6 each) | N2 N2 G G N2 G N2 N2 | N2, N1, N3 |
| C | C2 by the rock wall | G 2.00 (1/3), E2 1.97 (1/3), N2 1.93 (1/3) | E2 E2 G G E2 G E2 E2 | E2, N2, then three tied |
| C | C3 rock wall + ditch | Same as C1 | Same as C1 | Same as C1 |

**Share of waves that breach elsewhere.** The quarry pair decides the verdict.

| Layout | Quarry pair | Main waves | Moves? | Sapper wave | Near quarry against the ditch |
|---|---|---|---|---|---|
| A | A1 against A2 | 2/9 | no | 4/9 | A1 against A3: 2/3 |
| B | B1 against B2 | 1/3 | yes, exactly at the bar | 7/15 | B1 against B3: 0 |
| C | C1 against C2 | 1/3 | yes, exactly at the bar | 4/9 | C1 against C3: 0 |

**The near quarry at every berm.** These rows, from the script's sweep, show what the open term sees as the quarry comes closer:

| Berm | A1 (inside, 4 m of earth) | B1 (in front, 3 m) | C1 (in front, 4 m) |
|---|---|---|---|
| 1 m | E2 and E3 fall; 2/9 elsewhere | N1 and N2 fall; 0 elsewhere | E1–E3 fall; 0 elsewhere |
| 2 m | E2 falls; 2/9 | N1–N3 read 0.20; 1/3 | E1 and E2 fall; 0 |
| 3 m | E1–E3 read 0.20; 2/9 | not undercut; 1/3 | E1–E3 read 0.20; 1/3 |
| 4 m or more | not undercut; 0 | not undercut; 1/3 | not undercut; 1/3 |

**Sensitivity.** These are the verdicts with one value changed at a time:

- **Still passes:** the open weight at ×2 or ×½, the earth weight at ×2, the towers weight at ×½, the gate weight at ×2 or ×½, a stone span of 2 m or 6 m, and digs that do not slump. N = 4 passes in all 3 layouts.
- **Fails:** the earth weight at ×½, the towers weight at ×2, N = 2, the towers term scaled over its range, only ditch designations counting as ditches, and the bar at 1/2.

## What drives the result

1. **The rule sees quarrying through the ditch reason, and it reads a quarry in front of a wall as protection.** B1 and B3 score the same, and so do C1 and C3. To the waves, a quarry against the north wall is the same thing as a ditch there, and the breach moves away from the quarry. Count only ditch designations as ditches and nothing moves (0 of 3).
2. **The open term barely registers undermining before the wall falls.** The keep forces a span of 4 m, and walls are 2 m thick. Undercutting one face therefore reads 0.20. One metre closer, both faces go along the pit's length and the section falls. Nothing lies between those two readings at quarry scale. Doubling the open weight changes nothing.
3. **Even a fallen section can score as it did intact.** B1 at a 1 m berm drops N2 and still sends no wave elsewhere: 1 + 0 + 0.93, against 0 + 1 + 0.93 with the quarry elsewhere. In A, the dropped E2 scores 2.97, below the intact gate's 3.00.
4. **Layout A cannot move.** Inside the curtain the only channel is undermining. There the quarry lifts E1–E3 by 0.20, which only reorders the third slot, because E2 was already a candidate.
5. **The gate takes a third of every wave.** With equal weights the gate is always first: 3.00 on earth, 2.00 on rock. It is in every pool, so no dig can move more than 2/3 of the waves. The sapper wave has no gate, and it moves more in every layout: 4/9, 7/15 and 4/9.
6. **Distance from the towers decides who else is in the pool.** The mid-side sections are about 15.5 m from a tower and the corner sections about 7.9 m. With the towers weight doubled or scaled over its range, the quarry's −1 on the earth term no longer pushes N2 or E2 out, and B and C fail.

## What would make the pass robust

None of these was tuned or tested here, except where the sensitivity table already shows it.

- **Write down whether a quarry pit is a ditch in front.** The verdict depends on it. If it is, also say whether a quarry that leaves a level berm along the foot keeps a ram off the foot, because a ditch with its rampart does.
- **Make an open section win.** A stretch the resolver reports open should be taken, or the open weight should be more than the other three together. Otherwise a breach can rank below an intact gate, as in finding 3.
- **Let the open term see undermining before collapse.** For example, read it as the trench a sapper would still need to drop the section. The trench readout already asks the resolver for its target ([Hud.md](../../Hud.md):266). A section beside a slumped quarry then reads closer to opening than an untouched one before it falls, rather than jumping from 0.20 to fallen.
- **Stop the gate filling a slot in every wave.** Either let its reason replace the earth reason rather than add to it, or score it only for waves that bring a ram.
- **Consider a draw weighted by score.** With a uniform draw over three, movement comes only in thirds, and a large change in score moves no more waves than a small one.

## Ambiguities and contradictions

1. **Is a quarry in front "a ditch in front"?** The wave rule says "no ditch in front" ([GameDesign.md](../../GameDesign.md):296). Pillar 1 says "a quarry beside a wall is a ditch an attacker has to cross" ([GameDesign.md](../../GameDesign.md):193, [GameDescription.md](../../GameDescription.md):46). The two readings give opposite verdicts.
2. **One quarry, two opposite effects.** Pillar 1 says the same quarry is both a ditch and a slope that slumps the foundation. Under the rule the ditch effect (−1) outweighs the slump (+0.20). The waves therefore avoid an undermined wall, and the player learns to quarry in front of their own walls.
3. **What a wave does with a section that is already open is not stated.** A breach is a route ([GameDesign.md](../../GameDesign.md), *A breach is a path*), yet the score can rank one below an intact gate.
4. **"Terraced" digs against slumping digs.**
   - The quarry is cut "in terraces" and the pit is "a terraced hole" ([GameDescription.md](../../GameDescription.md):103–104).
   - The dig readout, though, shows "where the ground will slump and which structures that would undermine" (:414), and pillar 1 has a quarry slump a foundation.
   - If terraces are stable 1:1 steps, no dig ever undermines anything. In this test that turns A's 2/9 into 0.
5. **Foundations and thin soil.**
   - The texts say a template's foundation "reaches down to firm ground" ([GameDescription.md](../../GameDescription.md):133) and "reaches the ground" ([GameDesign.md](../../GameDesign.md):204). T3 draws no foundation voxels.
   - "Thin soil over rock makes strong foundations" ([GameDescription.md](../../GameDescription.md):75) conflicts with [Hud.md](../../Hud.md):257, under which any earth under a wall can be undermined, whatever its depth.
   - With nothing below the surface, thin soil helps only by making a deep foundation cheap, and by limiting how far a slump reaches (berms of 2 m against 3 m here).
6. **The stone span.**
   - "Stone spans a little" ([GameDescription.md](../../GameDescription.md):188), but T2's stone floors need 4 m.
   - The other M0 paper tests in `Tools/PaperTests/` use 2 m: `STONE_SPAN_METERS` in `BreachPrediction.py` and `STONE_SPAN` in `SiegeRouteTable.py`. At 2 m the MVP keep's floors fall.
   - One value should serve all three tests. This test's verdict is the same at 2 m.
7. **"How far it is from the towers."** The rule does not say whether the keep counts, whether the gatehouse's garrison counts, or whether the measure is distance or cover.
8. **"Each stretch of wall it can reach."** Reach is not defined, and the old rule's "from where it arrives" was dropped.
9. **The sapper wave's choice.** "Trench beside a section standing on earth" does not say how the sapper wave scores its candidates. This test used the main score.
10. **A pit's earth spoil.** Only a ditch's spoil has a place, its rampart ([GameDescription.md](../../GameDescription.md):93). This test removes a pit's spoil from play.
11. **The keep around the hall.** "The keep, built in stone around the hall" ([GameDescription.md](../../GameDescription.md):280) cannot hold for these templates: T1 is 11 m wide and T2's interior is 8 × 8 m ([MvpAssets.md](../../MvpAssets.md):17–18). The plans put the hall beside the keep, which does not affect the rule.
12. **A ditch in front of a gate.** With no crossings in the design, the owner cannot ditch their own gate, so a gate on earth always keeps its earth reason.

**A caveat on the yields.** No single quarry supplies the curtain: they yield 1,143–2,464 m³, against 3,557 m³. A real Hold player digs both outcrops, so the choice is how much to dig near the wall, not near or far.

## Script output

```
Hold rule test (review F-45), Proposed values:
  weights open 1, earth 1, towers 1, gate 1; best few N = 3; stone span 4 m
  a ditch in front: a cut 2 m deep within 10 m of the face along 1/2 of the section; quarry pits count: True
  quarries 8 m deep; the breach moves when 1/3 of waves or more land elsewhere

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

  Scores: open + earth + towers + gate = score; * marks the best few. gap: columns with a gap under them;
  sup: the widest support distance in m, against the span; ditch: a ditch in front, which zeroes earth.
    stretch | A1                                     | A2                                     | A3
    N1      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49
    N2      |  .00 0  .93 0 =  .93                   |  .00 0  .93 0 =  .93                   |  .00 0  .93 0 =  .93
    N3      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49
    E1      |  .20 1  .47 0 = 1.68  gap 1 sup1       |  .00 1  .47 0 = 1.48                   |  .00 0  .47 0 =  .47  ditch
    E2      |  .20 1  .97 0 = 2.17* gap 8 sup1       |  .00 1  .97 0 = 1.97*                  |  .00 0  .97 0 =  .97  ditch
    E3      |  .20 1  .49 0 = 1.69* gap 2 sup1       |  .00 1  .49 0 = 1.49*                  |  .00 0  .49 0 =  .49  ditch
    S2      |  .00 1  .49 0 = 1.49                   |  .00 1  .49 0 = 1.49*                  |  .00 1  .49 0 = 1.49*
    G       |  .00 1 1.00 1 = 3.00*                  |  .00 1 1.00 1 = 3.00*                  |  .00 1 1.00 1 = 3.00*
    S1      |  .00 1  .49 0 = 1.49                   |  .00 1  .49 0 = 1.49*                  |  .00 1  .49 0 = 1.49*
    W1      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49
    W2      |  .00 0  .85 0 =  .85                   |  .00 0  .85 0 =  .85                   |  .00 0  .85 0 =  .85
    W3      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49

  The breach: a uniform draw over the best 3, chance in brackets; then the pick of waves seeded 1-8
    A1  G 3.00 (1/3), E2 2.17 (1/3), E3 1.69 (1/3)
        seeds: E2 E2 G  G  E2 G  E2 E2
        sapper wave (sections on earth): E2 2.17 (1/3), E3 1.69 (1/3), E1 1.68 (1/3)
        sapper seeds: E2 E3 E2 E3 E3 E2 E3 E3
    A2  G 3.00 (1/3), E2 1.97 (1/3), E3 1.49 (1/9), S2 1.49 (1/9), S1 1.49 (1/9)
        seeds: E2 E2 G  G  E2 G  E2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), E3 1.49 (2/9), S2 1.49 (2/9), S1 1.49 (2/9)
        sapper seeds: E2 S2 E2 E3 S1 E2 S1 S1
    A3  G 3.00 (1/3), S2 1.49 (1/3), S1 1.49 (1/3)
        seeds: S2 S1 G  G  S1 G  S1 S1
        sapper wave (sections on earth): S2 1.49 (1/3), S1 1.49 (1/3), E2 .97 (1/3)
        sapper seeds: S1 S1 S1 S2 S2 S2 S2 S2

  Does the breach move? (share of waves that breach elsewhere; it moves at 1/3 or more)
    A1 vs A2:  2/9 elsewhere, does not move  sapper wave  4/9 <- the quarry pair
    A1 vs A3:  2/3 elsewhere, moves          sapper wave  2/3
    A2 vs A3:  4/9 elsewhere, moves          sapper wave  2/9

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

  Scores: open + earth + towers + gate = score; * marks the best few. gap: columns with a gap under them;
  sup: the widest support distance in m, against the span; ditch: a ditch in front, which zeroes earth.
    stretch | B1                                     | B2                                     | B3
    N1      |  .20 0  .49 0 =  .69  gap 4 sup1 ditch |  .00 1  .49 0 = 1.49                   |  .00 0  .49 0 =  .49  ditch
    N2      |  .20 0  .93 0 = 1.12  gap 8 sup1 ditch |  .00 1  .93 0 = 1.93*                  |  .00 0  .93 0 =  .93  ditch
    N3      |  .20 0  .49 0 =  .69  gap 3 sup1 ditch |  .00 1  .49 0 = 1.49                   |  .00 0  .49 0 =  .49  ditch
    E1      |  .00 1  .47 0 = 1.48                   |  .00 1  .47 0 = 1.48                   |  .00 1  .47 0 = 1.48
    E2      |  .00 1  .97 0 = 1.97*                  |  .00 1  .97 0 = 1.97*                  |  .00 1  .97 0 = 1.97*
    E3      |  .00 1  .49 0 = 1.49*                  |  .00 1  .49 0 = 1.49                   |  .00 1  .49 0 = 1.49*
    S2      |  .00 1  .49 0 = 1.49*                  |  .00 1  .49 0 = 1.49                   |  .00 1  .49 0 = 1.49*
    G       |  .00 1 1.00 1 = 3.00*                  |  .00 1 1.00 1 = 3.00*                  |  .00 1 1.00 1 = 3.00*
    S1      |  .00 1  .49 0 = 1.49*                  |  .00 1  .49 0 = 1.49                   |  .00 1  .49 0 = 1.49*
    W1      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49
    W2      |  .00 0  .85 0 =  .85                   |  .00 0  .85 0 =  .85                   |  .00 0  .85 0 =  .85
    W3      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49

  The breach: a uniform draw over the best 3, chance in brackets; then the pick of waves seeded 1-8
    B1  G 3.00 (1/3), E2 1.97 (1/3), E3 1.49 (1/9), S2 1.49 (1/9), S1 1.49 (1/9)
        seeds: E2 E2 G  G  E2 G  E2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), E3 1.49 (2/9), S2 1.49 (2/9), S1 1.49 (2/9)
        sapper seeds: E2 S1 S1 E3 E2 E2 E2 E2
    B2  G 3.00 (1/3), E2 1.97 (1/3), N2 1.93 (1/3)
        seeds: E2 E2 G  G  E2 G  E2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), N2 1.93 (1/3), N1 1.49 (1/15), N3 1.49 (1/15), E3 1.49 (1/15), S2 1.49 (1/15), S1 1.49 (1/15)
        sapper seeds: E2 E3 N2 S2 E2 E2 E2 E2
    B3  G 3.00 (1/3), E2 1.97 (1/3), E3 1.49 (1/9), S2 1.49 (1/9), S1 1.49 (1/9)
        seeds: E2 E2 G  G  E2 G  E2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), E3 1.49 (2/9), S2 1.49 (2/9), S1 1.49 (2/9)
        sapper seeds: E2 S1 S1 E3 E2 E2 E2 E2

  Does the breach move? (share of waves that breach elsewhere; it moves at 1/3 or more)
    B1 vs B2:  1/3 elsewhere, moves          sapper wave 7/15 <- the quarry pair
    B1 vs B3:    0 elsewhere, does not move  sapper wave    0
    B2 vs B3:  1/3 elsewhere, moves          sapper wave 7/15

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

  Scores: open + earth + towers + gate = score; * marks the best few. gap: columns with a gap under them;
  sup: the widest support distance in m, against the span; ditch: a ditch in front, which zeroes earth.
    stretch | C1                                     | C2                                     | C3
    N1      |  .00 1  .49 0 = 1.49*                  |  .00 1  .49 0 = 1.49                   |  .00 1  .49 0 = 1.49*
    N2      |  .00 1  .93 0 = 1.93*                  |  .00 1  .93 0 = 1.93*                  |  .00 1  .93 0 = 1.93*
    N3      |  .00 1  .49 0 = 1.49*                  |  .00 1  .49 0 = 1.49                   |  .00 1  .49 0 = 1.49*
    E1      |  .20 0  .47 0 =  .68  gap 4 sup1 ditch |  .00 1  .47 0 = 1.48                   |  .00 0  .47 0 =  .47  ditch
    E2      |  .20 0  .97 0 = 1.17  gap 8 sup1 ditch |  .00 1  .97 0 = 1.97*                  |  .00 0  .97 0 =  .97  ditch
    E3      |  .20 0  .49 0 =  .69  gap 4 sup1 ditch |  .00 1  .49 0 = 1.49                   |  .00 0  .49 0 =  .49  ditch
    S2      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49
    G       |  .00 0 1.00 1 = 2.00*                  |  .00 0 1.00 1 = 2.00*                  |  .00 0 1.00 1 = 2.00*
    S1      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49
    W1      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49  ditch            |  .00 0  .49 0 =  .49  ditch
    W2      |  .00 0  .85 0 =  .85                   |  .00 0  .85 0 =  .85  ditch            |  .00 0  .85 0 =  .85  ditch
    W3      |  .00 0  .49 0 =  .49                   |  .00 0  .49 0 =  .49  ditch            |  .00 0  .49 0 =  .49  ditch

  The breach: a uniform draw over the best 3, chance in brackets; then the pick of waves seeded 1-8
    C1  G 2.00 (1/3), N2 1.93 (1/3), N1 1.49 (1/6), N3 1.49 (1/6)
        seeds: N2 N2 G  G  N2 G  N2 N2
        sapper wave (sections on earth): N2 1.93 (1/3), N1 1.49 (1/3), N3 1.49 (1/3)
        sapper seeds: N3 N2 N1 N3 N1 N3 N2 N2
    C2  G 2.00 (1/3), E2 1.97 (1/3), N2 1.93 (1/3)
        seeds: E2 E2 G  G  E2 G  E2 E2
        sapper wave (sections on earth): E2 1.97 (1/3), N2 1.93 (1/3), N1 1.49 (1/9), N3 1.49 (1/9), E3 1.49 (1/9)
        sapper seeds: N1 E2 N2 N2 N2 N1 E2 E2
    C3  G 2.00 (1/3), N2 1.93 (1/3), N1 1.49 (1/6), N3 1.49 (1/6)
        seeds: N2 N2 G  G  N2 G  N2 N2
        sapper wave (sections on earth): N2 1.93 (1/3), N1 1.49 (1/3), N3 1.49 (1/3)
        sapper seeds: N3 N2 N1 N3 N1 N3 N2 N2

  Does the breach move? (share of waves that breach elsewhere; it moves at 1/3 or more)
    C1 vs C2:  1/3 elsewhere, moves          sapper wave  4/9 <- the quarry pair
    C1 vs C3:    0 elsewhere, does not move  sapper wave    0
    C2 vs C3:  1/3 elsewhere, moves          sapper wave  4/9

=== Verdict: the breach moves with quarry placement in 2 of 3 layouts: PASS (pass needs 2) ===

=== The near quarry at every berm (Proposed rules otherwise) ===
  berm m: the open term of each stretch it undercuts (FALL: the slump drops it); the best few, with ties at
  the cut; and the share of waves that breach elsewhere than under the quarry pair's other placement
  A1:
    1: E1 .80, E2 FALL, E3 FALL           pool E2, E3, G      elsewhere 2/9
    2: E1 .40, E2 FALL, E3 .60            pool E2, E3, G      elsewhere 2/9
    3: E1 .20, E2 .20, E3 .20             pool E2, E3, G      elsewhere 2/9
    4: no stretch undercut                pool E2, E3, S2, G, S1 elsewhere 0
    5: no stretch undercut                pool E2, E3, S2, G, S1 elsewhere 0
    6: no stretch undercut                pool E2, E3, S2, G, S1 elsewhere 0
  B1:
    1: N1 FALL, N2 FALL, N3 .80           pool N2, E2, G      elsewhere 0
    2: N1 .20, N2 .20, N3 .20             pool E2, E3, S2, G, S1 elsewhere 1/3
    3: no stretch undercut                pool E2, E3, S2, G, S1 elsewhere 1/3
    4: no stretch undercut                pool E2, E3, S2, G, S1 elsewhere 1/3
    5: no stretch undercut                pool E2, E3, S2, G, S1 elsewhere 1/3
    6: no stretch undercut                pool E2, E3, S2, G, S1 elsewhere 1/3
  C1:
    1: E1 FALL, E2 FALL, E3 FALL          pool N2, E2, G      elsewhere 0
    2: E1 FALL, E2 FALL, E3 .80           pool N2, E2, G      elsewhere 0
    3: E1 .20, E2 .20, E3 .20             pool N1, N2, N3, G  elsewhere 1/3
    4: no stretch undercut                pool N1, N2, N3, G  elsewhere 1/3
    5: no stretch undercut                pool N1, N2, N3, G  elsewhere 1/3
    6: no stretch undercut                pool N1, N2, N3, G  elsewhere 1/3

=== Sensitivity: one value changed at a time; share of waves elsewhere for each quarry pair ===
    rules                       A1 v A2    B1 v B2    C1 v C2    moved   verdict
    Proposed                    2/9        1/3        1/3        2 of 3  PASS
    open weight x2              2/9        1/3        1/3        2 of 3  PASS
    open weight x1/2            2/9        1/3        1/3        2 of 3  PASS
    earth weight x2             2/9        1/3        4/9        2 of 3  PASS
    earth weight x1/2           2/9        0          0          0 of 3  FAIL
    towers weight x2            2/9        0          0          0 of 3  FAIL
    towers weight x1/2          2/9        1/3        1/3        2 of 3  PASS
    gate weight x2              2/9        1/3        1/3        2 of 3  PASS
    gate weight x1/2            2/9        1/3        1/3        2 of 3  PASS
    best few N = 2              0          0          1/2        1 of 3  FAIL
    best few N = 4              1/3        7/20       1/3        3 of 3  PASS
    stone span 2 m (T2 falls)   2/9        1/3        1/3        2 of 3  PASS
    stone span 6 m              2/9        1/3        1/3        2 of 3  PASS
    towers term over range      2/9        0          0          0 of 3  FAIL
    only ditches are ditches    2/9        0          0          0 of 3  FAIL
    digs do not slump           0          1/3        1/3        2 of 3  PASS
    moves at 1/2 of waves       2/9        1/3        1/3        0 of 3  FAIL
    only ditches, open x2       2/9        0          0          0 of 3  FAIL
```
