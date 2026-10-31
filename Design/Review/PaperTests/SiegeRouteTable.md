# Paper test: the Siege castle's route table, range circles and repair model

**Status:** an M0 paper test, run on 2026-10-31. It answers findings F-01, F-02 and F-03 of the [design review](../GameDescription-2026-10-31.md), as its *Cheapest tests* item 4 and [MvpPlan.md](../../MvpPlan.md) M0 ask. The design leaves many values open. Each one chosen here is marked **Proposed** with its reason, for the owner to confirm or change ([GameDesign.md](../../GameDesign.md), *Starting the MVP*, item 7). Nothing here changes a Decided rule. Where a Decided rule fails the test, the options are evaluated and none is adopted.

**The script** is [`Tools/PaperTests/SiegeRouteTable.py`](../../../Tools/PaperTests/SiegeRouteTable.py). It uses Python 3 and the standard library only. It reads T1–T5 from `GameData/Server/Templates/`, so the castle is drawn from the real templates, builds the castle, and prints every table below. Run it with `python Tools/PaperTests/SiegeRouteTable.py`; it takes under ten seconds. Its output from Python 3.13 on Linux is pasted unchanged in the appendix. The tables in the body are copied from that output.

## Result

| Criterion | Verdict | In one line |
|---|---|---|
| 1. No breach route wins every column of the route table | **Pass, weakly** | No route is best in all six scored columns, at a stone span of 2 or of 4. But the gate by ram is dominated by ramming the weak section's base, and the fastest route by far is the trebuchet on the gate: two shots, 40 s. |
| 2. No engine circle from outside tower range covers the great tower, or the win rule makes that harmless | **Pass on the second clause** | 1,340 m² of stands outside every garrison's reach put the great tower in trebuchet range. The storming rule makes this harmless, because engines alone cannot win. The Decided guard on engine range is violated, and its formula is not sufficient. |
| 3. Repair adds 1.3–2× to breach time for one or two engines | **Fail** | Under the ten-second lock, no single repair rate gives 1.3–2× across reloads of 6–30 s, even for one engine. For one and two engines together, no rate gives it at any reload. Two engines firing alternately lock repair out at every reload up to 20 s, which is what GameDescription.md:137 intends. |

## How the test works

- **The castle** is drawn on the 1 m grid from T1–T5. x runs east, y north, and the attacker's camp lies to the south. The plan and its choices are in *The Siege castle* below.
- **Reach.** A garrison reaches a target on the ground if the horizontal distance from its building's outer face is within the archers' range, plus one band of range per elevation step. Shots start at the outer face (GameDescription.md:213). Where the test asks about soldiers at a wall's foot or in a breach, it also asks for line of sight. A flat shot leaves the platform 1 m above its floor, aims at a point 1 m above the ground, and must clear every other structure, sampled every 0.25 m.
- **Engines** lob, so they need range and nothing else. A trebuchet's *stand* is any 1 m cell outside the castle and its earthworks, out to 130 m. Whether a stand is outside tower range is judged by distance alone. Ignoring line of sight can only shrink the safe area, so this is the strictest reading of F-01.
- **Breaches** use ADR-004's support rule, run on the real T3 voxels with three sections end to end. A voxel's support distance is the fewest sideways steps on a path that moves only down or sideways through solid voxels to the ground. A voxel falls if that distance exceeds the stone span, and pieces cascade. A section is open when foot soldiers have a route: a column empty through the whole 2 m thickness for 2 m of height, from the ground or from 1 m up (the step onto a rubble heap). This is GameDesign.md:207 and :279 made concrete.
- **Repair** is the review's working for F-03, c / (c − r), with the lock and a fixed rate per section. A landing removes c m³. Repair is locked for ten seconds after each landing, then restores r m³ before the next one, never more than has been removed. The section opens when the net removed volume reaches the volume to open. Landings are counted as a real number, which averages over where in its last shot a section opens; a single siege moves in whole shots. The first version counted whole shots. Because the unrepaired shots were calibrated to remove exactly the volume to open, any repair at all then cost one whole extra shot or volley, an artefact of that calibration. The expected count removes the artefact.
- **Dominance.** A route *wins* a column when it is best or tied for best from the attacker's side: shorter times, less stone spent, more stone the defender must find, fewer kinds of defender that can hurt the breakers, and an earlier era. A route that wins every column is dominant (the review's F-02 test). The script also reports which routes are *dominated*, meaning another route is at least as good in every column and better in one.

## The Siege castle

The castle is 62 m square over its towers, with a 52 m bailey. Six T3 sections run on each of the east, west and north curtains. The south curtain has four sections, the gatehouse and a fifth tower, ST. A 9 m gatehouse and a 7 m tower together take the length of two 8 m sections, so this is the only way the south curtain closes on the 8 m grid. The keep stands in the middle, with its forebuilding towards the gate, and the hall in the south-west of the bailey. A standard ditch with its rampart rings the castle. It stops for a causeway in front of the gate, and in front of **E3**, the weak section, which stands on deep earth. Everything else stands on 1 m of soil over common rock.

- **Nearest section to the camp:** S3, 103 m away. It stands on rock behind the ditch, between the gatehouse and ST.
- **Weak section:** E3. It is the only section on earth and the only one with no ditch, and it is among the farthest from towers (20 m to the nearest, tied with E4, N3, N4, W3 and W4). It is 130 m from the camp, so the castle's nearest section is not its weakest, as GameDesign.md:294 requires.
- **Great tower to curtain:** 18 m to the nearest curtain's inner face and 20 m to its outer face, on the north side. Centring the keep makes this as large as the castle allows. That is the generous case for F-01.

One character is 1 m in both directions, so the plan looks stretched vertically. The key is in the first lines.

```text
T tower  G gatehouse  = timber gate  _ gate passage  # wall section (labelled)  K great tower
k forebuilding  H hall  ^ rampart  ~ ditch  . thin soil over rock  , deep earth

       -10       0         10        20        30        40        50        60        70
       |         |         |         |         |         |         |         |         |
      ....................................................................................
      ....................................................................................
  70  ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^................................................................^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
  60  ..~~~~~^^^.T<NW>TT................................................T<NE>TT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT###N1######N2######N3######N4######N5######N6###TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT################################################TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....W6....................................................E6....^^^~~~~~..
  50  ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....W5....................................................E5....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
  40  ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....W4....................KKKKKKKKKKKK....................E4....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKK<KEEP>KKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
  30  ..~~~~~^^^....##....................KKKKKKKKKKKK...................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##....................KKKKKKKKKKKK...................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##....................KKKKKKKKKKKK...................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....W3....................KKKKKKKKKKKK...................,E3,,,,,,,,,,,,,,
      ..~~~~~^^^....##........................kkkk.......................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##........................kkkk.......................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##........................kkkk.......................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##...................................................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
  20  ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....W2....................................................E2....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HH<HALL>HHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....W1....HHHHHHHHHHH.....................................E1....^^^~~~~~..
  10  ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................GGG___GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................GG<GH>GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT################GGG___GGG########TTTTTTT########TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT###S1######S2###GGG___GGG###S3###TTTTTTT###S4###TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................GGG___GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.T<SW>TT................GGG___GGG........T<ST>TT........T<SE>TT.^^^~~~~~..
   0  ..~~~~~^^^.TTTTTTT................GGG===GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^................................................................^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^.....^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^.....^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^.....^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
 -10  ....................................................................................
      ....................................................................................
       |         |         |         |         |         |         |         |         |
```

## Values chosen (Proposed)

All are **Proposed**. The design does not give any of them.

| Value | Proposed | Reason |
|---|---|---|
| Archers' range on the level | **30 m**, six bands | Mantlets are out of the trial, so the trebuchet has to outrange the towers to bombard from outside their fire. Its placeholder maximum is 70 m (Hud.md:119), and tower reach is set well inside it. 30 m plus two bands also lets two towers 48 m apart cover all the curtain between them: "towers that overlap their fire" (GameDescription.md:265). |
| One band of range | **5 m** | One sixth of the level range, so a band is a round number of metres and range reads in bands (Hud.md:107, "HEIGHT +1 band"). |
| Elevation | Towers and gatehouse fire from **11 m**, the keep from **16 m** | These are the floors the templates give: T4 and T5's merlon layer, and T2's roof floor. Each is at least 6 m over flat ground, so every garrison gets the cap of two steps (GameDescription.md:215). Tower reach is therefore **40 m** from the outer face. |
| Garrisons | A squad of **six archers** in each of the five towers, the gatehouse and the keep | Hud.md's GARRISON n / 6. Archers are the only ranged unit in the trial's roster (GameDesign.md:73). The gatehouse also has its Decided passage attack. |
| Garrisons put out | A gatehouse whose gate breaks, and a keep that stops working, put their garrisons out, as a tower does | GameDescription.md:217 states this for towers only. The same rule for every garrisoned building is the simplest. |
| Sally | Men-at-arms and outriders, out to **70 m** from the curtain | "A cavalry sally to catch the engines" is one of the defender's ways to win (GameDescription.md:266). A leash shorter than the trebuchet's range would leave a trebuchet untouchable. |
| Trebuchet reload | **20 s**: nine shots in the three-minute target | It is the middle of F-03's 6–30 s and the review's own example. At 20 s, two engines firing alternately land every 10 s, exactly the lock, which is the case GameDescription.md:137 describes. |
| Volume to open a section | **20 m³** net: a hole 5 m long and 2 m high through the 2 m wall at its base | This hole is wider than twice the stone span, so at span 2 the stone above it starts to fall, as in the design's picture of a breach (GameDescription.md:258). It is far more than the 1 m × 2 m minimum route, because lobbed shots do not cut a neat slot. It only scales the repair rate's units. The multipliers depend on the ratio of repair to crater. |
| Stone span | **2 voxels**, and the route table again at **4** | "Stone spans a little" (GameDescription.md:188), and 2 matches the breach-prediction test. T2 needs 4 to stand, which the script computes from its voxels, so the route table is run at both spans (ambiguity 3). |
| Repair rate | **0.08 m³/s a section** (4.8 m³ a minute) | At this rate, one engine at the Proposed 20 s reload takes 1.5× as long, the middle of the 1.3–2× band and below "at most doubles" (GameDescription.md:273). The repair test does not depend on this rate: it sweeps every rate. |
| The repair pass band | **1.3–2×**, for one engine, two in step and two offset | It is the review's suggestion, which MvpPlan.md:186 takes up. "For one or two engines" is read as each count separately, at the same rate. |
| Ram | A blow every **5 s**; **1 voxel** of stone a blow, from the bottom metre only; **4 voxels** of timber | 5 s stays inside the lock, as F-02 assumed ("contact hits keeping it inside the repair lockout"). Voxels have no damage state (GameDescription.md:176), so one voxel is the smallest crater there is. "Small, at the base only" (:180) is read as the bottom metre. "Large" in timber is four voxels. |
| Trebuchet's very large crater in timber | A voxel and its **six neighbours** | It is the next step up the ladder of crater sizes on a 1 m grid: small 1, medium about 2, large 4, very large 7. |
| Gate threshold | The gate works while at least **half** its 12 voxels stand | The simplest share. MvpAssets.md names the gate as T5's critical part but gives no threshold. |
| Sappers | A laborer digs earth at **0.1 m³/s**, so six sappers dig 1.2 m³/s | Twenty laborers quarrying at 0.1 m³/s for 15 minutes cut 1,800 m³, about the keep's 1,722 m³, which the tempo needs by minute 10–15 (GameDescription.md:124). Earth is "dug quickly", so this rate is if anything slow. Sappers dig at twice a laborer's speed, which is Decided. |
| Trench | **8 m × 2 m × 3 m** along the section's foot, 48 m³ | It runs the section's full length, so the undercut is longer than twice the span. At 3 m deep, earth slumping at 45° runs out from under a 2 m-wide foundation 1 m deep. |
| Standard foundation | **1 m** deep | It reaches rock under thin soil and rests on earth over deep earth (GameDescription.md:133, ambiguity 13). |
| Collapse warning | **1 s per metre** of the falling piece's height | "A taller piece warns for longer" (GameDescription.md:192), stated as simply as possible. |
| Ground | 1 m of soil over common rock, with deep earth under E3 | Walls founded on rock are a defender's strength (GameDescription.md:263), and the trial needs one section on earth (GameDesign.md:294). |
| Ditch | The standard ditch (MvpAssets.md:60), 4–9 m beyond the tower faces, with its rampart 1–4 m out; a causeway at the gate; no ditch in front of E3 | The ditch keeps engines and sappers at a distance (GameDescription.md:264). The gap follows the v5 scene's "one unditched stretch on deep earth". |
| Camp | 100 m south of the castle | It defines which section is nearest. |
| Times exclude | The approach, deployment and packing, for every route | The comparison is the time to open from the moment the breaker starts work. |

## Range findings (F-01)

Every garrison reaches 40 m from its building's outer face. The trebuchet reaches 12–70 m.

- **The trebuchet can hit the great tower from outside every garrison's reach.** By distance alone, 1,340 m² of stands do it: 498 m² on the north side, 360 m² on the east, 360 m² on the west and 122 m² on the south. The closest is 55.5 m from the great tower, about 33 m beyond the north towers' faces. The safest is 12 m beyond every reach.
- **The Decided guard is violated.** GameDesign.md:42 asks that engine range be less than tower reach plus the shortest distance from keep to curtain. Here that is 40 + 20 = 60 m, and the placeholder range is 70 m. The guard fails on any castle whose keep stands less than 30 m inside its curtain.
- **The guard's formula is not sufficient either.** Between two towers, their reach ends closer to the curtain than "tower reach measured from the curtain line". On this castle the real bound is 55.5 m, 4.5 m below what the formula allows. Grown to 9–12 sections a side with towers at the corners only, the castle satisfies the guard and still leaves 570–856 m² of covering stands. A longer curtain moves the keep away, but it also leaves longer stretches out of tower reach.
- **Sensitivity.** On this castle the covering area shrinks from 1,340 m² at 30 m archers to 368 m² at 35 m and 24 m² at 40 m, and vanishes from 45 m (55 m tower reach).
- **Every garrison can be shelled from outside every garrison's reach.** Each tower and the gatehouse can be hit from 40 m, and the keep from 55.5 m. An attacker can strip the castle's fire before storming, at a cost in stone. That is ordinary siege play, but it means overlapping towers do not protect each other from engines.

**What the win rule makes of it.** The keep is broken only when it has stopped working *and* the attacker's soldiers have held its foot for two minutes (GameDescription.md:36, GameDesign.md:42). Shelling the great tower therefore cannot win. The attacker's soldiers still need a breach or the gate, so H2's breach data is not empty, and F-01's failure scenario cannot happen. Under the storming rule, engine coverage of the great tower is more than harmless: it is how the precondition is met. Only engines, sappers on earth and a ram at the base can bring a keep below its threshold, because soldiers never change the land (GameDescription.md:221). **Criterion 2 passes on its second clause.** The guard is then redundant, and if it were enforced, the attacker would have to bring the trebuchet inside tower reach to bring the keep down.

The range map below uses 3 m × 6 m a character, so it looks roughly square. `*` marks a stand outside every reach with the great tower in range; `+` marks one with only E3 in range.

```text
...............................*******************...............................
.........................oooooooooooo*******ooooooooooo..........................
......................ooooooooooooooooooooooooooooooooooooo......................
....................ooooooooooooooooooooooooooooooooooooooooo+...................
..................oooooooooooooooooooooooooooooooooooooooooooo+++................
.................oooooooooooooooooooooooooooooooooooooooooooooo++++..............
.................oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo++++.............
................*oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo++++++...........
................*oooooooooo~~~#####################~~~oooooooooo*++++++..........
...............**oooooooooo~~~#####################~~~oooooooooo*+++++++.........
...............**oooooooooo~~~#####################~~~oooooooooo**++++++.........
...............***ooooooooo~~~########KKKK#########~~~ooooooooo***+++++++........
...............****oooooooo~~~########KKKK#########~~~oooooooo****+++++++........
...............****oooooooo~~~########KKKK#########~~~oooooooo****+++++++........
...............***ooooooooo~~~#####################~~~ooooooooo***+++++++........
...............**oooooooooo~~~#####################~~~oooooooooo*++++++++........
................*oooooooooo~~~#####################~~~oooooooooo*+++++++.........
................*oooooooooo~~~#####################~~~oooooooooo++++++++.........
.................oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo+++++++..........
.................oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo++++++...........
..................ooooooooooooooooooooooooooooooooooooooooooooo+++++.............
...................ooooooooooooooooooooooooooooooooooooooooooo+++++..............
....................ooooooooooooooooooooooooooooooooooooooooo++++................
......................oooooooooooooooooooooooooooooooooooo++++...................
..........................oooooooooooooooooooooooooooo+++........................
```

## The route table (F-02)

All seven ways in are scored. The trebuchet takes the same time and stone on every T3 section, so most sections share one row. The per-section table in the appendix gives what differs between them: who covers each section's foot and its inner mouth, the stand-off, and the distance to the keep. Repair is at the Proposed 0.08 m³/s, with one trebuchet at a 20 s reload and 6 m³ a shot. Times and stone with repair are expected values.

**At a stone span of 2:**

| Route | Open (s) | Open with repair (s) | Attacker stone m³ (unrepaired / repaired) | Defender stone m³ | Defenders who can hurt the breakers | Era (breaker) | Columns won | Dominated | Why pick it |
|---|---|---|---|---|---|---|---|---|---|
| Gate · ram | 240 | 240 | 0 / 0 | 0 (+7 timber) | GH (passage), sally (2) | Hall | 2/6 | yes | it is the gate; no stone; the ram's roof stops tower arrows; blows keep repair locked |
| Gate · trebuchet | 40 | 40 | 12 / 12 | 1 (+8 timber) | sally (1) | Citadel | 3/6 | no | it is the gate; two shots from outside every reach; breaking the gate puts out the gatehouse's garrison |
| S3 · trebuchet (nearest) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | nearest the camp; stand-off outside every reach |
| E3 · trebuchet (weak) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | on earth, no ditch, far from towers; stand-off outside every reach |
| Other sections · trebuchet | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | the same in every scored column as the two above; the storm leg differs |
| E3 · sappers (undermining) | 47 | 47 | 0 / 0 | 48 (+ the ground under it) | SE, NE, sally (3) | Keep | 2/6 | no | the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild |
| E3 · ram at the base | 56 | 56 | 0 / 0 | 21 | sally (1) | Hall | 3/6 | no | no ditch, so the ram can reach the foot; arrow-proof; blows keep repair locked |

**At a stone span of 4**, the span at which T2 stands, three things change:

- A trebuchet breach drops nothing above its hole, so the defender rebuilds 20 m³, not 29.
- The ram needs 18 blows, 96 s, carrying its slot on into E4.
- **Undermining E3 never opens.** Every voxel of an 8 m undercut is then within four steps of a sound neighbour, and the neighbours stand on rock. That leaves undermining dominated as well, by both ram routes.

No route is dominant at either span.

**What the table says:**

1. **No route wins every column, so criterion 1 passes.** The pass is weak, because six columns that pull in different directions make a clean sweep unlikely. The findings below say more.
2. **The gate by ram is dominated.** Ramming E3's base is faster (56 s or 96 s against the 240 s target), leaves the defender more to rebuild and faces only a sally, for the same stone and era. The only way to the gate that is not dominated is by trebuchet.
3. **The gate by trebuchet is the fastest route, at 40 s.** T5's gate is a 12 m³ timber panel in the gatehouse's outer face (MvpAssets.md:21). A trebuchet's very large crater in timber (GameDescription.md:182) takes it in two shots, from a stand 40 m off that is outside every garrison's reach. Breaking the gate also puts the gatehouse's garrison out. **This is F-02's failure, arriving through another engine.** In both arms of the playtest, the gate is attractive whatever the wall and ground are like, which is H2's "cluster at the gate" failure (GameDesign.md:279).
4. **The weak section works as designed.** Its own two routes, sappers and the ram, take 47–56 s at span 2 and are possible only there. At span 4 only the ram remains, at 96 s. The nearest section is never competitive. A trebuchet takes the same 180 s (270 s with repair) on every section, so for a trebuchet the choice of section comes down to the storm leg.
5. **Two fast routes have no balance target.** At the Proposed rates, undermining and ramming the base of an unditched stone wall are 3–4× faster than the trebuchet's target. Their balance then rests entirely on exposure. Sappers at E3's foot face two towers, but they are armoured against arrows. The ram faces nothing but a sally.
6. **The shot price does not meet its rule.** A breach costs the attacker 54 m³ (nine shots of 6 m³) and the defender 29 m³ to rebuild, a ratio of 1.86. GameDescription.md:250 wants about 0.5. That needs about 1.6 m³ a shot at span 2, or 1.1 m³ at span 4. Even if the whole 96 m³ section fell, nine shots at 6 m³ would come to 0.56.
7. **The ram's gate target does not fit its craters.** A ram making large craters of 4 voxels breaks the 12-voxel gate in two blows: 10 s at one blow every 5 s. At one blow every 60 s or slower, repair keeps up and the gate never breaks. No cadence gives about 240 s. The target holds only if the passage attack keeps the ram off the gate for most of the four minutes.

**Storming.** Every route ends with soldiers walking to the keep's foot and holding it for two minutes.

| Opening | Width | Garrisons that can shoot its inner mouth | To the keep's foot (m) |
|---|---|---|---|
| Gate passage | 3 m, 7 m long | SW, SE, ST, KEEP (GH's own is put out when its gate breaks) | 16 |
| S3 | 5 m (trebuchet) | GH, ST, KEEP | 18 |
| E3 | 5 m (trebuchet), 1 m (ram), 4 m (sappers) | SE, NE, GH, ST, KEEP | 20 |

The keep's foot is covered by two to three tower garrisons on every side. The north side, with NW and NE only, is the least covered. Every way in is under fire from three to five garrisons until the trebuchet brings towers down.

## The repair model (F-03)

The three-minute target holds at every reload, so one engine needs round(180 / reload) shots, each removing 20 m³ / shots. Repair is locked for 10 s after every landing. The multiplier is the breach time with repair over the time without, for the same engines. Two engines *in step* land together; *offset* means they alternate, half a reload apart.

| Reload (s) | Shots | × one engine | × two in step | × two offset | Rates for 1.3–2×, one engine (m³/s) | Two in step | Two offset |
|---|---|---|---|---|---|---|---|
| 6 | 30 | 1.00 | 1.00 | 1.00 | none | none | none |
| 8 | 22 | 1.00 | 1.00 | 1.00 | none | none | none |
| 10 | 18 | 1.00 | 1.00 | 1.00 | none | none | none |
| 12 | 15 | 1.13 | 1.06 | 1.00 | 0.162–0.345 | 0.343–0.714 | none |
| 15 | 12 | 1.29 | 1.11 | 1.00 | 0.082–0.174 | 0.176–0.364 | none |
| 20 | 9 | 1.50 | 1.17 | 1.00 | 0.056–0.118 | 0.124–0.250 | none |
| 25 | 7 | 1.62 | 1.19 | 1.06 | 0.049–0.103 | 0.113–0.222 | 0.296–0.615 |
| 30 | 6 | 1.77 | 1.21 | 1.11 | 0.044–0.091 | 0.103–0.200 | 0.176–0.364 |

The multipliers are at the Proposed 0.08 m³/s.

**Can one rate serve the whole range?**

| Engines | One rate for 6–30 s? | Shortest reload a single rate serves, up to 30 s | That rate (m³/s) |
|---|---|---|---|
| One engine | no | 15 s | 0.082–0.090 |
| Two in step | no | 15 s | 0.176–0.196 |
| Two offset | no | 24 s | 0.319–0.364 |
| One, and two in step | no | no reload | none |
| One, and two either way | no | no reload | none |

- **At 10 s or less, repair does nothing,** whatever the rate. A single engine's landings never leave the section unlocked.
- **Two engines firing alternately keep the lock permanent at every reload up to 20 s.** This is the design's intent: "that is what concentrating engines buys" (GameDescription.md:137).
- **One engine and two in step never share a rate.** Two engines remove twice as much between repair windows, so the same rate means half as much relative to the damage. A 1.3× multiplier for two engines needs about 1.9–2× for one. The two bands meet only in a sliver at 12 s (0.343–0.345 m³/s), where one engine takes 2× and two take 1.3×; at every other reload they miss.
- **With the crater fixed instead,** at nine shots for every reload, so that the unrepaired time is 9 × reload, a single rate serves one engine only from 20 s.

**So criterion 3 fails, and it fails for two reasons.** The lock zeroes repair whenever landings come 10 s apart or less. And a fixed rate per section cannot keep one and two engines in the same narrow band.

## Pass or fail

1. **No breach route wins every column: pass.** This holds at a stone span of 2 and of 4. The pass is weak: the gate by ram is dominated, the trebuchet on the gate is the fastest route, and two fast routes at an unditched or earth-founded wall have no balance target.
2. **Engine circles from outside tower range over the great tower: they do cover it, and the win rule makes that harmless. Pass on the second clause.** The Decided range guard is violated by the placeholder range, and its formula is not sufficient.
3. **Repair adds 1.3–2× for one or two engines: fail.** The ten-second lock makes it impossible across 6–30 s, as the design intends for alternating engines. A fixed rate per section also makes it impossible for one and two engines together.

## Options for the rules that fail (none adopted)

**Repair (criterion 3).** The script evaluates each option over every reload from 6 to 30 s.

| Option | One engine | Two in step | Two offset | All three at one rate | What it gives up |
|---|---|---|---|---|---|
| As decided: a 10 s lock, a fixed rate per section | from 15 s only | from 15 s only | from 24 s only | **never** | — |
| **A.** No lock: repair also works under fire | 0.032–0.056 m³/s | 0.074–0.114 | 0.063–0.111 | **never** | It reverses "repairs happen between volleys" and "two engines alternating keep it locked" (GameDescription.md:137). One rate fits each engine count but not both, for the reason above. |
| **A′.** A 3 s lock | 0.053–0.063 | 0.108–0.136 | from 10 s only | **never** | The same, and alternating engines at short reloads still lock repair out. |
| **B.** No lock, and repair restores at most a third of each hit, at a fixed rate per section | 0.032 m³/s or more | 0.074 or more | 0.063 or more | **any rate of 0.074 m³/s or more** | The multiplier sits at 1.33–1.48× for one or two engines at every reload. The rate becomes a ceiling on repair's speed, and the share sets its effect, so "a fixed rate per section" no longer decides the outcome alone. Concentrating engines then buys only double fire, not a lockout. The share must lapse once a section has gone unhit for a while, or a breached section could never be fully rebuilt. |
| **C.** Keep the rule and change the target | One engine at a reload of 15–30 s, at 0.082–0.090 m³/s, meets 1.3–2× | — | — | — | Two engines then gain 1.0–1.2×, by design. The pass mark is restated as 1.3–2× for one engine, with two engines removing most of repair's effect. This is the reading GameDescription.md:137 and :273 already support, and the test then passes for one engine with the reload fixed at 15 s or more. |

**The range guard (criterion 2's first clause).**

- (a) Drop the guard, which is redundant under the storming rule.
- (b) Restate it as the geometric test this script runs: no stand outside every garrison's reach is within engine range of the great tower. Check that test on every authored castle, and on the castles the Skirmish AI builds.
- (c) For this castle, a trebuchet range under 55 m, or archers at 45 m or more on the level.

**The gate by trebuchet (criterion 1's weak pass).**

- (a) Redraw T5 with the gate at the inner end of its 7 m passage. A lob steeper than about 30° (atan 4/7) then strikes the passage floor before the gate. T1–T7 are due to be redrawn natively at 1 m before M4 anyway (MvpAssets.md, *Known issues*).
- (b) Give the trebuchet a smaller crater in timber than "very large".

**The ram on stone.**

- (a) A balance target, for example "a ram opens an unditched standard section in about N minutes".
- (b) A ram that cannot damage stone at all: drop "chips stone at the base".

## Ambiguities and contradictions in the design

1. **The lock against the pass mark.** GameDescription.md:137, which is Decided, says that "two engines alternating on one section keep it locked; that is what concentrating engines buys". The review's F-03 and its *Cheapest tests* item 4 ([GameDescription-2026-10-31.md](../GameDescription-2026-10-31.md):47–60, :308), which MvpPlan.md:186 takes up, ask for 1.3–2× with two engines as well. Both cannot hold. The owner should say which governs.
2. **The range guard.** GameDesign.md:42 bounds engine range by "tower reach plus the shortest distance from keep to curtain". The placeholder 70 m range (Hud.md:119) breaks it on any castle whose keep stands less than 30 m inside its curtain. The formula is not sufficient between towers. "Tower reach" also has no definition, because the archers' range and a "band of range" (GameDescription.md:215) are unset.
3. **The stone span against the keep.** "Stone spans a little" (GameDescription.md:188), but T2's dressed-stone floors span its 8 m interior and need a span of 4 to stand, as the script computes from the voxels. T4 and T5 need 2. The M0 tests disagree: the breach-prediction test uses 2, the Hold test 4, and this test both. At 4, undermining a section needs ground lost under at least 9 m, more than one section, so GameDescription.md:201's undermining of "a wall" reaches into its neighbours. A timber floor in T2, or a span per material, would remove the conflict.
4. **The gate's four minutes.** GameDescription.md:274 and GameDesign.md:53 give about four minutes under fire. GameDescription.md:180 gives the ram a "Large" crater in timber, and T5's gate is 12 m³ (MvpAssets.md:21). With the ten-second lock, no cadence fits.
5. **The gate faces outwards.** T5's gate is flush with its outer face, so a lobbed "very large" timber crater (GameDescription.md:182) breaks it in two shots. F-02's fix covers the ram and leaves the trebuchet's route open.
6. **The ram on stone** has no balance target (GameDescription.md:270–276). On the 1 m grid its smallest crater is one voxel, because voxels have no damage state (:176), so only its cadence sets its rate. "At the base only" (:180) is unspecified: the bottom metre, or the bottom two? If two, a 1 × 2 m hole, four voxels, is a route through a 2 m wall in four blows.
7. **Undermining has no balance target.** At a tempo-derived dig rate it opens a section in under a minute.
8. **The shot price.** "About half what rebuilding it costs the defender" (GameDescription.md:250) does not fit Hud.md:119's 6 m³ a shot at nine shots a breach, unless a breach costs the defender 108 m³, more than a whole 96 m³ section.
9. **The keep around the hall.** "The keep, built in stone around the hall" (GameDescription.md:280). T2's great tower is 8 m × 8 m inside, and T1 is 11 m × 7 m, so the hall cannot fit. This castle puts the hall in the bailey.
10. **Who is put out.** Only a tower that stops working puts its garrison out (GameDescription.md:217). The rule is silent on gatehouses and keeps, and both matter: the gate route, and the hold at the keep's foot.
11. **"From the owner's side" and "one whole building".** For a free-standing keep, every side is the owner's. For the gate, the outer face is the attacker's. Because a section is "one whole building" (GameDescription.md:137), the keep's 1,722 m³ repairs at the same rate as a 96 m³ wall section, and a gatehouse's gate shares one lock with its stone.
12. **A wall on common rock.** GameDescription.md:201 says undermining "works only where the wall stands on earth". Hud.md:257 says that "only bedrock or a deep foundation stops it". This test reads a wall founded on common rock as safe, because there is no tunnelling and rock does not slump.
13. **The standard foundation.** It "reaches down to firm ground" (GameDescription.md:133), which is undefined on deep earth.
14. **Dressed stone.** The templates are built of dressed stone (their legends). GameDescription.md:184 makes dressed stone an upgrade material with smaller craters, and :66 lets only rubble of building stone yield stone. Which crater table, and which clearance rule, applies to the Siege castle's walls is not said.
15. **Elevation never differs in the trial.** Every MVP garrison stands 11 m or more above flat ground, so all of them hit the two-step cap (GameDescription.md:215). Height separates no tower from the keep.
16. **The Siege defender's scope.** GameDesign.md:291 says it "fights and repairs, and does nothing else". MvpPlan.md:49 adds sallies to a leash and shooting sappers, and GameDescription.md:257 has it sally against the engines.
17. **Rubble carries support** (ADR-004, consequences). A collapse's rubble landing in a slot or a trench could support the wall above again. This test does not model it, and the resolver needs a rule.
18. **One trebuchet.** The Siege trial's fixed army has one trebuchet (GameDesign.md:73). In the trial, two engines on one section means the trebuchet and the ram, so F-03's two-engine case is mostly a Skirmish question.
19. **"Open" on the 1 m grid is cheap.** A route needs one voxel by two (GameDesign.md:207), so a 2 m wall opens once 4 m³ goes from its base in the right place. The trebuchet's three-minute target therefore constrains its craters and its aim more than the wall.

## Not modelled

These are left out: combat and attrition, because hit points and damage are unset; aim scatter and the shape of trebuchet craters, which belong to the M3 breach lab; rubble carrying support; and the approach, deployment and packing times. Lines of sight ignore ramparts, and the F-01 stands ignore line of sight altogether, which is deliberately strict. The defender AI's choices, such as when it repairs or sallies, are not simulated.

## Appendix: the script's output

Produced by `python Tools/PaperTests/SiegeRouteTable.py` with Python 3.13 on Linux, unchanged.

```text
SIEGE CASTLE ROUTE TABLE · M0 paper test for review findings F-01, F-02, F-03

== Templates read from GameData/Server/Templates ==
| Id | Template | W x H x D (m) | Stone m³ | Timber m³ | Critical part: voxels |
|---|---|---|---|---|---|
| T1 | Hall | 11 x 7 x 7 | 0 | 188 | Hall frame: 188 |
| T2 | Keep | 12 x 19 x 15 | 1722 | 0 | Great tower: 1632 |
| T3 | Stone wall, standard | 8 x 7 x 2 | 96 | 0 | None (sections open by the breach rule): 0 |
| T4 | Tower | 7 x 12 x 7 | 479 | 0 | Firing platform: 110 |
| T5 | Gatehouse | 9 x 12 x 7 | 585 | 12 | Gate: 12 |

== Values this test chose (Proposed) ==
| Value | Proposed |
|---|---|
| Archers' range on the level | 30 m, six bands of 5 m |
| Elevation | towers and gatehouse fire from 11 m, the keep from 16 m: two steps each |
| Garrisons | a squad of 6 archers in each tower, the gatehouse and the keep; GH adds its passage attack |
| Sally | men-at-arms and outriders, out to 70 m from the curtain |
| Trebuchet reload | 20 s, so 9 shots open a standard section in 180 s |
| Volume to open a section | 20 m³ net: 5 m x 2 m through the wall at its base |
| Stone span | 2 voxels |
| Repair rate | 0.08 m³/s a section (4.8 m³ a minute): one engine at 20 s then takes 1.5x as long |
| Ram | a blow every 5 s; 1 voxel of stone a blow, bottom metre only; 4 voxels of timber |
| Very large crater (trebuchet, timber) | a voxel and its six neighbors |
| Gate threshold | the gate works while at least 50% of its voxels stand |
| Sappers | a laborer digs earth at 0.1 m³/s, so six sappers dig 1.2 m³/s |
| Trench to undermine | 8 m x 2 m x 3 m along the section's foot |
| Collapse warning | 1 s per metre of the falling piece's height |
| Ground | 1 m of soil over common rock, except deep earth under the weak section |
| Camp | 100 m south of the castle |

== The Siege castle: 62 m x 62 m over its towers, 1 character = 1 m, north up ==
T tower  G gatehouse  = timber gate  _ gate passage  # wall section (labelled)  K great tower
k forebuilding  H hall  ^ rampart  ~ ditch  . thin soil over rock  , deep earth
The attacker's camp is 100 m south of the castle. The ditch stops at the causeway in front of the gate
and in front of E3, the one section that stands on deep earth.

       -10       0         10        20        30        40        50        60        70 
       |         |         |         |         |         |         |         |         |  
      ....................................................................................
      ....................................................................................
  70  ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^................................................................^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
  60  ..~~~~~^^^.T<NW>TT................................................T<NE>TT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT###N1######N2######N3######N4######N5######N6###TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT################################################TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................................................TTTTTTT.^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....W6....................................................E6....^^^~~~~~..
  50  ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....W5....................................................E5....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
  40  ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....W4....................KKKKKKKKKKKK....................E4....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKK<KEEP>KKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
      ..~~~~~^^^....##....................KKKKKKKKKKKK....................##....^^^~~~~~..
  30  ..~~~~~^^^....##....................KKKKKKKKKKKK...................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##....................KKKKKKKKKKKK...................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##....................KKKKKKKKKKKK...................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....W3....................KKKKKKKKKKKK...................,E3,,,,,,,,,,,,,,
      ..~~~~~^^^....##........................kkkk.......................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##........................kkkk.......................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##........................kkkk.......................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##...................................................,##,,,,,,,,,,,,,,
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
  20  ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....W2....................................................E2....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HH<HALL>HHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....W1....HHHHHHHHHHH.....................................E1....^^^~~~~~..
  10  ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....HHHHHHHHHHH.....................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^....##....................................................##....^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................GGG___GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................GG<GH>GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT################GGG___GGG########TTTTTTT########TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT###S1######S2###GGG___GGG###S3###TTTTTTT###S4###TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.TTTTTTT................GGG___GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^.T<SW>TT................GGG___GGG........T<ST>TT........T<SE>TT.^^^~~~~~..
   0  ..~~~~~^^^.TTTTTTT................GGG===GGG........TTTTTTT........TTTTTTT.^^^~~~~~..
      ..~~~~~^^^................................................................^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^.....^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^.....^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^^^^.....^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
      ..~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.....~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~..
 -10  ....................................................................................
      ....................................................................................
       |         |         |         |         |         |         |         |         |  

Bailey 52 m x 52 m inside the curtain. Great tower to the curtain, on the shortest side:
18 m to its inner face, 20 m to its outer face. Curtain: 22 sections of T3;
towers SW, SE, NW, NE and ST (T4); gatehouse GH (T5); keep (T2); hall (T1).

== Range circles ==
| Garrison | Building | Stands at (m) | Steps | Reach from its outer face (m) |
|---|---|---|---|---|
| SW | tower | 11 | 2 | 40 |
| SE | tower | 11 | 2 | 40 |
| NW | tower | 11 | 2 | 40 |
| NE | tower | 11 | 2 | 40 |
| GH | gatehouse | 11 | 2 | 40 |
| ST | tower | 11 | 2 | 40 |
| KEEP | keep | 16 | 2 | 40 |
Archers on the level: 30 m. Trebuchet: 12–70 m. Engines lob, so they need no line of sight.

Engine stands searched: every 1 m cell out to 130 m beyond the castle, outside the ditch
ring. Stands outside every garrison's reach, by distance alone (the smallest the safe area can be): 85272 m².
Of those, stands from which the trebuchet reaches the great tower: 1340 m².
  The closest is 55.5 m from the great tower, at (30.5, 94.5),
  0.1 m beyond the nearest garrison's reach.
  The safest is 12.1 m beyond every reach and 69.5 m from the great tower.
  By side (m²): north 498, east 360, south 122, west 360
The Decided guard (GameDesign.md, the design review, decision 2): engine range < tower reach
  + keep-to-curtain distance;
  here 70 < 40 + 20 = 60: VIOLATED.
  The range below which no stand outside reach reaches the great tower is 55.5 m on this castle,
  4.5 m less than the guard's formula allows: between towers, their reach ends closer
  to the curtain than tower reach measured from the curtain line.

Sensitivity on this castle (distance only):
| Archer range (m) | Garrison reach (m) | Stands covering the great tower (m²) |
|---|---|---|
| 30 | 40 | 1340 |
| 35 | 45 | 368 |
| 40 | 50 | 24 |
| 45 | 55 | 0 |
| 50 | 60 | 0 |
| 55 | 65 | 0 |
| 60 | 70 | 0 |

| Sections a side | Castle | Keep to curtain (m) | Guard | Stands covering the great tower (m²) | Curtain walls (m³) |
|---|---|---|---|---|---|
| 6 | 62 m | 20 | violated | 1340 | 2112 |
| 7 | 70 m | 24 | violated | 766 | 2496 |
| 8 | 78 m | 28 | violated | 590 | 2880 |
| 9 | 86 m | 32 | holds | 570 | 3264 |
| 10 | 94 m | 36 | holds | 708 | 3648 |
| 11 | 102 m | 40 | holds | 840 | 4032 |
| 12 | 110 m | 44 | holds | 856 | 4416 |
The larger castles keep towers only at the corners and beside the gate, as this one does.

Every garrison can itself be shelled from outside every garrison's reach:
| Target | Safe stands in range | Closest (m) |
|---|---|---|
| SW | 5549 m² | 40.0 |
| SE | 5549 m² | 40.0 |
| NW | 5669 m² | 40.0 |
| NE | 5669 m² | 40.0 |
| GH | 3671 m² | 40.0 |
| ST | 4112 m² | 40.5 |
| KEEP | 1340 m² | 55.5 |

Range map, 3 m x 6 m a character, north up: # castle, K great tower, ~ earthworks, C camp,
o inside a garrison's reach, * outside every reach with the great tower in trebuchet range,
+ outside every reach with E3 in range but not the great tower, . outside every reach.

.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
...............................*******************...............................
.........................oooooooooooo*******ooooooooooo..........................
......................ooooooooooooooooooooooooooooooooooooo......................
....................ooooooooooooooooooooooooooooooooooooooooo+...................
..................oooooooooooooooooooooooooooooooooooooooooooo+++................
.................oooooooooooooooooooooooooooooooooooooooooooooo++++..............
.................oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo++++.............
................*oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo++++++...........
................*oooooooooo~~~#####################~~~oooooooooo*++++++..........
...............**oooooooooo~~~#####################~~~oooooooooo*+++++++.........
...............**oooooooooo~~~#####################~~~oooooooooo**++++++.........
...............***ooooooooo~~~########KKKK#########~~~ooooooooo***+++++++........
...............****oooooooo~~~########KKKK#########~~~oooooooo****+++++++........
...............****oooooooo~~~########KKKK#########~~~oooooooo****+++++++........
...............***ooooooooo~~~#####################~~~ooooooooo***+++++++........
...............**oooooooooo~~~#####################~~~oooooooooo*++++++++........
................*oooooooooo~~~#####################~~~oooooooooo*+++++++.........
................*oooooooooo~~~#####################~~~oooooooooo++++++++.........
.................oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo+++++++..........
.................oooooooooo~~~~~~~~~~~~~~~~~~~~~~~~~~~oooooooooo++++++...........
..................ooooooooooooooooooooooooooooooooooooooooooooo+++++.............
...................ooooooooooooooooooooooooooooooooooooooooooo+++++..............
....................ooooooooooooooooooooooooooooooooooooooooo++++................
......................oooooooooooooooooooooooooooooooooooo++++...................
..........................oooooooooooooooooooooooooooo+++........................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................
.................................................................................

== Every wall section ==
| Section | Ground | Ditch | From camp (m) | Nearest garrison (m) | Garrisons on its foot | Closest safe trebuchet stand (m) | Garrisons on its inner mouth | Inner mouth to keep (m) |
|---|---|---|---|---|---|---|---|---|
| S1 | rock | yes | 105 | 4 | SW, GH | 43 | 2 | 23 |
| S2 | rock | yes | 104 | 4 | SW, GH | 43 | 3 | 19 |
| S3 (nearest) | rock | yes | 103 | 4 | GH, ST | 44 | 3 | 18 |
| S4 | rock | yes | 105 | 4 | SE, ST | 44 | 3 | 23 |
| E1 | rock | yes | 114 | 4 | SE | 41 | 4 | 23 |
| E2 | rock | yes | 122 | 12 | SE, NE | 37 | 5 | 20 |
| E3 (weak) | deep earth | no | 130 | 20 | SE, NE | 36 | 5 | 20 |
| E4 | rock | yes | 138 | 20 | SE, NE | 36 | 5 | 20 |
| E5 | rock | yes | 146 | 12 | SE, NE | 37 | 4 | 20 |
| E6 | rock | yes | 154 | 4 | NE | 41 | 2 | 23 |
| N1 | rock | yes | 160 | 4 | NW | 41 | 2 | 22 |
| N2 | rock | yes | 159 | 12 | NW, NE | 37 | 3 | 18 |
| N3 | rock | yes | 159 | 20 | NW, NE | 36 | 3 | 18 |
| N4 | rock | yes | 159 | 20 | NW, NE | 36 | 3 | 18 |
| N5 | rock | yes | 159 | 12 | NW, NE | 37 | 3 | 18 |
| N6 | rock | yes | 160 | 4 | NE | 41 | 2 | 22 |
| W1 | rock | yes | 114 | 4 | SW | 41 | 1 | 23 |
| W2 | rock | yes | 122 | 12 | SW, NW | 37 | 3 | 20 |
| W3 | rock | yes | 130 | 20 | SW, NW | 36 | 5 | 20 |
| W4 | rock | yes | 138 | 20 | SW, NW | 36 | 4 | 20 |
| W5 | rock | yes | 146 | 12 | SW, NW | 37 | 3 | 20 |
| W6 | rock | yes | 154 | 4 | NW | 41 | 2 | 23 |

Nearest section to the camp: S3. Weak section: E3 (deep earth, no ditch, among the farthest from towers).
Closest safe trebuchet stand to the gate: 40 m.
Every stand that covers the great tower lies within 46 m of the castle's outer faces, inside the
sally's Proposed leash of 70 m; so does every closest stand in the table above.

== Breach mechanics on T3, from ADR-004's support rule with a stone span of 2 voxels ==
Trebuchet: a hole of 20 m³ (5 m x 2 m through 2 m) at the base is a route (yes); 9 m³ above it then falls.
Ram: 10 blows cut a slot along the bottom metre; the stone above it falls in pieces, 11 m³ in all,
  leaving a gap 1 m wide over the rubble, the last piece after a 6 s warning. Route: yes.
Sappers: a trench of 48 m³ at 1.2 m³/s for a squad takes 40 s; 48 m³ falls,
  4 m of the section, after a 7 s warning. The ends within 2 m of sound sections stay up. Route: yes.
Gate by trebuchet: 2 very large craters take 8 of the gate's 12 timber voxels and 1 m³ of stone;
  the gate works while 6 stand, so it is broken after 40 s, repair between the shots included.
Gate by ram, checked against the craters: a large crater of 4 voxels a blow, every 5 s, breaks the gate
  in 2 blows, 10 s, against the Decided target of 240 s.
  A blow every 30 s: 2 blows, 60 s.
  A blow every 60 s: repair keeps up, the gate never breaks.
  A blow every 120 s: repair keeps up, the gate never breaks.
  No cadence gives about 240 s at 4 voxels a blow: the target holds only if the passage attack
  keeps the ram off the gate most of the time.

The stone span decides what falls. The span each template needs to stand whole: T2 4, T3 0, T4 2, T5 2.
At each span, on T3 sections end to end:
| Span (voxels) | Falls above the trebuchet's 5 m hole (m³) | Ram at the base, to a route | Shortest undercut that opens a route | Within one 8 m section? |
|---|---|---|---|---|
| 2 | 9 | 10 blows, 56 s | 5 m (11 m³ falls) | yes |
| 3 | 0 | 14 blows, 75 s | 7 m (11 m³ falls) | yes |
| 4 | 0 | 18 blows, 96 s | 9 m (11 m³ falls) | no: it reaches into the neighbours |

== Route table ==
| Route | Open (s) | Open with repair (s) | Attacker stone m³ (unrepaired / repaired) | Defender stone m³ | Defenders who can hurt the breakers | Era (breaker) | Columns won | Dominated | Why pick it |
|---|---|---|---|---|---|---|---|---|---|
| Gate · ram | 240 | 240 | 0 / 0 | 0 (+7 timber) | GH (passage), sally (2) | Hall | 2/6 | yes | it is the gate; no stone; the ram's roof stops tower arrows; blows keep repair locked |
| Gate · trebuchet | 40 | 40 | 12 / 12 | 1 (+8 timber) | sally (1) | Citadel | 3/6 | no | it is the gate; two shots from outside every reach; breaking the gate puts out the gatehouse's garrison |
| S3 · trebuchet (nearest) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | nearest the camp; stand-off outside every reach |
| E3 · trebuchet (weak) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | on earth, no ditch, far from towers; stand-off outside every reach |
| Other sections · trebuchet | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | the same in every scored column as the two above; the storm leg differs |
| E3 · sappers (undermining) | 47 | 47 | 0 / 0 | 48 (+ the ground under it) | SE, NE, sally (3) | Keep | 2/6 | no | the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild |
| E3 · ram at the base | 56 | 56 | 0 / 0 | 21 | sally (1) | Hall | 3/6 | no | no ditch, so the ram can reach the foot; arrow-proof; blows keep repair locked |

Dominant route: none
  Gate · ram: wins Attacker stone, Era
  Gate · trebuchet: wins Open, Open, repaired, Breakers hurt by
  S3 · trebuchet (nearest): wins Breakers hurt by
  E3 · trebuchet (weak): wins Breakers hurt by
  Other sections · trebuchet: wins Breakers hurt by
  E3 · sappers (undermining): wins Attacker stone, Defender stone
  E3 · ram at the base: wins Attacker stone, Breakers hurt by, Era
Not dominated by any other route: Gate · trebuchet, S3 · trebuchet (nearest), E3 · trebuchet (weak), Other sections · trebuchet, E3 · sappers (undermining), E3 · ram at the base
Dominated: Gate · ram, by E3 · ram at the base
Times and stone with repair are expected values (see the repair model); one siege moves in whole shots.
Repair at 0.08 m³/s a section; one trebuchet at a 20 s reload; 6 m³ a shot.
Shot price check: a trebuchet breach costs the attacker 54 m³ against 29 m³ to rebuild, a ratio of
  1.86; the Decided target is about 0.5, which at 9 shots means 1.6 m³ a shot.

== Route table again with a stone span of 4, at which T2 stands ==
| Route | Open (s) | Open with repair (s) | Attacker stone m³ (unrepaired / repaired) | Defender stone m³ | Defenders who can hurt the breakers | Era (breaker) | Columns won | Dominated | Why pick it |
|---|---|---|---|---|---|---|---|---|---|
| Gate · ram | 240 | 240 | 0 / 0 | 0 (+7 timber) | GH (passage), sally (2) | Hall | 2/6 | yes | it is the gate; no stone; the ram's roof stops tower arrows; blows keep repair locked |
| Gate · trebuchet | 40 | 40 | 12 / 12 | 1 (+8 timber) | sally (1) | Citadel | 3/6 | no | it is the gate; two shots from outside every reach; breaking the gate puts out the gatehouse's garrison |
| S3 · trebuchet (nearest) | 180 | 270 | 54 / 81 | 30 (10 repaired + 20 rebuilt) | sally (1) | Citadel | 2/6 | no | nearest the camp; stand-off outside every reach |
| E3 · trebuchet (weak) | 180 | 270 | 54 / 81 | 30 (10 repaired + 20 rebuilt) | sally (1) | Citadel | 2/6 | no | on earth, no ditch, far from towers; stand-off outside every reach |
| Other sections · trebuchet | 180 | 270 | 54 / 81 | 30 (10 repaired + 20 rebuilt) | sally (1) | Citadel | 2/6 | no | the same in every scored column as the two above; the storm leg differs |
| E3 · sappers (undermining) | never | never | 0 / 0 | 0 (+ the ground under it) | SE, NE, sally (3) | Keep | 1/6 | yes | never opens: every voxel of an 8 m undercut is within the span of a sound neighbor |
| E3 · ram at the base | 96 | 96 | 0 / 0 | 29 | sally (1) | Hall | 3/6 | no | no ditch, so the ram can reach the foot; arrow-proof; blows keep repair locked |

Dominant route: none
  Gate · ram: wins Attacker stone, Era
  Gate · trebuchet: wins Open, Open, repaired, Breakers hurt by
  S3 · trebuchet (nearest): wins Defender stone, Breakers hurt by
  E3 · trebuchet (weak): wins Defender stone, Breakers hurt by
  Other sections · trebuchet: wins Defender stone, Breakers hurt by
  E3 · sappers (undermining): wins Attacker stone
  E3 · ram at the base: wins Attacker stone, Breakers hurt by, Era
Not dominated by any other route: Gate · trebuchet, S3 · trebuchet (nearest), E3 · trebuchet (weak), Other sections · trebuchet, E3 · ram at the base
Dominated: Gate · ram, by E3 · ram at the base
Dominated: E3 · sappers (undermining), by Gate · ram, E3 · ram at the base
Times and stone with repair are expected values (see the repair model); one siege moves in whole shots.
Repair at 0.08 m³/s a section; one trebuchet at a 20 s reload; 6 m³ a shot.
Shot price check: a trebuchet breach costs the attacker 54 m³ against 20 m³ to rebuild, a ratio of
  2.70; the Decided target is about 0.5, which at 9 shots means 1.1 m³ a shot.

== Storming: from each opening to the keep's foot ==
| Opening | Width | Garrisons that can shoot its inner mouth | To the keep's foot (m) |
|---|---|---|---|
| Gate passage | 3 m, 7 m long | SW, SE, ST, KEEP (GH's own is put out when its gate breaks) | 16 |
| S3 | 5 m (trebuchet) | GH, ST, KEEP | 18 |
| E3 | 5 m (trebuchet), 1 m (ram), 4 m (sappers) | SE, NE, GH, ST, KEEP | 20 |

| Keep's foot | Garrisons that can shoot it (the keep's own is put out once it stops working) | Count |
|---|---|---|
| north | NW, NE | 2 |
| east | SE, NE, ST | 3 |
| south | SE, GH, ST | 3 |
| west | SW, NW, GH | 3 |

== Repair model (F-03) ==
A section opens at 20 m³ net. The three-minute target holds at every reload, so one engine
needs round(180 / reload) shots and each removes 20 m³ / shots. Repair is locked for 10 s after every hit.
A multiplier is the breach time with repair over the time without, for the same engines.
Repair: 0.08 m³/s.

| Reload (s) | Shots | m³ a shot | One engine, unrepaired (s) | x one engine | x two in step | x two offset |
|---|---|---|---|---|---|---|
| 6 | 30 | 0.67 | 180 | 1.00 | 1.00 | 1.00 |
| 8 | 22 | 0.91 | 176 | 1.00 | 1.00 | 1.00 |
| 10 | 18 | 1.11 | 180 | 1.00 | 1.00 | 1.00 |
| 12 | 15 | 1.33 | 180 | 1.13 | 1.06 | 1.00 |
| 15 | 12 | 1.67 | 180 | 1.29 | 1.11 | 1.00 |
| 20 | 9 | 2.22 | 180 | 1.50 | 1.17 | 1.00 |
| 25 | 7 | 2.86 | 175 | 1.62 | 1.19 | 1.06 |
| 30 | 6 | 3.33 | 180 | 1.77 | 1.21 | 1.11 |

Repair rates (m³/s per section) that give 1.3–2x:
| Reload (s) | One engine | Two in step | Two offset | One and two in step | All three |
|---|---|---|---|---|---|
| 6 | none | none | none | none | none |
| 8 | none | none | none | none | none |
| 10 | none | none | none | none | none |
| 12 | 0.162–0.345 | 0.343–0.714 | none | 0.343–0.345 | none |
| 15 | 0.082–0.174 | 0.176–0.364 | none | none | none |
| 20 | 0.056–0.118 | 0.124–0.250 | none | none | none |
| 25 | 0.049–0.103 | 0.113–0.222 | 0.296–0.615 | none | none |
| 30 | 0.044–0.091 | 0.103–0.200 | 0.176–0.364 | none | none |

As decided (ten-second lock, a fixed rate per section):
| Engines | One rate for 6–30 s? | Rate window (m³/s) | Shortest reload a single rate serves, up to 30 s |
|---|---|---|---|
| One engine | no | 0.082–0.090 | 15 s |
| Two in step | no | 0.176–0.196 | 15 s |
| Two offset | no | 0.319–0.364 | 24 s |
| One, and two in step | no | none | no reload |
| One, and two either way | no | none | no reload |

Variant with the crater fixed instead (nine shots at every reload, so the unrepaired time is 9 x reload):
  one rate for 6–30 s: no; a single rate serves from 20 s.

Option A: no lock:
| Engines | One rate for 6–30 s? | Rate window (m³/s) | Shortest reload a single rate serves, up to 30 s |
|---|---|---|---|
| One engine | yes | 0.032–0.056 | 6 s |
| Two in step | yes | 0.074–0.114 | 6 s |
| Two offset | yes | 0.063–0.111 | 6 s |
| One, and two either way | no | none | no reload |

Option A': a 3 s lock:
| Engines | One rate for 6–30 s? | Rate window (m³/s) | Shortest reload a single rate serves, up to 30 s |
|---|---|---|---|
| One engine | yes | 0.053–0.063 | 6 s |
| Two in step | yes | 0.108–0.136 | 6 s |
| Two offset | no | 0.134–0.147 | 10 s |
| One, and two either way | no | none | no reload |

Option B: no lock, and repair restores at most a third of each hit:
| Engines | One rate for 6–30 s? | Rate window (m³/s) | Shortest reload a single rate serves, up to 30 s |
|---|---|---|---|
| One engine | yes | 0.032 or more | 6 s |
| Two in step | yes | 0.074 or more | 6 s |
| Two offset | yes | 0.063 or more | 6 s |
| One, and two either way | yes | 0.074 or more | 6 s |

Option B at a repair rate of 0.5 m³/s, multipliers:
| Reload (s) | x one engine | x two in step | x two offset |
|---|---|---|---|
| 6 | 1.48 | 1.47 | 1.48 |
| 10 | 1.47 | 1.44 | 1.47 |
| 20 | 1.44 | 1.39 | 1.44 |
| 30 | 1.42 | 1.33 | 1.42 |

== Verdict ==
1. No breach route wins every column, at a stone span 2: PASS.
   Fastest: Gate · trebuchet 40 s, E3 · sappers (undermining) 47 s, E3 · ram at the base 56 s.
   Dominated: Gate · ram.
1. No breach route wins every column, at a stone span 4: PASS.
   Fastest: Gate · trebuchet 40 s, E3 · ram at the base 96 s, Gate · ram 240 s.
   Dominated: Gate · ram, E3 · sappers (undermining).
2. Engine circles from outside tower range over the great tower: 1340 m² of stands cover it.
   The win rule needs the attacker's soldiers to hold the keep's foot for two minutes, so shelling
   the great tower cannot win alone and a breach or the gate is still needed: harmless under the
   storming rule, PASS on the second clause. The placeholder 70 m range violates the Decided guard.
3. One repair rate giving 1.3–2x for one and two engines over reloads of 6–30 s: FAIL.
```
