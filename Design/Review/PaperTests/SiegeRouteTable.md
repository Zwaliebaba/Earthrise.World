# Paper test: the Siege castle's route table, range circles and repair model

**Status:** an M0 paper test, run on 2026-10-31 and revised the same day on the owner's decisions. It answers findings F-01, F-02 and F-03 of the [design review](../GameDescription-2026-10-31.md), as its *Cheapest tests* item 4 and [MvpPlan.md](../../MvpPlan.md) M0 ask. After the first run, the owner decided three things: stone spans 2 m, masonry is ordinary stone, and repair's pass mark is for one engine. Those are marked **Decided (owner, 2026-10-31)**. Every other value chosen here is **Proposed**, with its reason, for the owner to confirm or change ([GameDesign.md](../../GameDesign.md), *Starting the MVP*, item 7). Where the test breaks a Decided rule, this revision drafts the smallest fix it can find, checks it with the script, and marks it Proposed. Nothing here edits a template or GameDesign.md. The [last section](#proposed-text-for-gamedesignmd) gives the text to paste.

**The script** is [`Tools/PaperTests/SiegeRouteTable.py`](../../../Tools/PaperTests/SiegeRouteTable.py). It uses Python 3 and the standard library only. It reads T1–T5 from `GameData/Server/Templates/` and their materials from [`GameData/Server/Palette.json`](../../../GameData/Server/Palette.json). It then builds the castle and prints every table below. Run it with `python Tools/PaperTests/SiegeRouteTable.py`; it takes about ten seconds and prints the same output on every run. Its output from Python 3.13 on Linux is pasted unchanged in the appendix, and the tables in the body are copied from it.

## Result

| Criterion | Verdict | In one line |
|---|---|---|
| 1. No breach route wins every column of the route table | **Pass, weakly** | No route is best in all six scored columns. But the trebuchet takes T5's gate in two shots, 40 s, from outside every reach. At the weak section, the ram and the sappers open E3 in 56 s and 68 s, against the trebuchet's 180 s. |
| 2. No engine stand outside tower range covers the great tower, unless the win rule makes that harmless | **Pass on the second clause** | 1,340 m² of stands outside every garrison's reach put the great tower in trebuchet range. The storming rule makes that harmless. The Decided guard on engine range is violated, and its formula is not sufficient. |
| 3. With one engine, repair adds 1.3–2× to the time to open a section, at the Proposed trebuchet reload | **Pass** | At the Proposed 20 s reload and 0.08 m³/s, one engine takes 1.50× as long (1.56× in whole shots). This holds at reloads of 16–30 s. It fails below that: 1.29× at 15 s, and 1.00× at 10 s or less. Two engines firing alternately lock repair out, as GameDescription.md:137 intends. |

**The four Proposed fixes**, for the places where the test breaks a Decided rule:

| Fix | Change | Kind of change | Check | What it costs |
|---|---|---|---|---|
| (a) The engine range guard | The trebuchet's range becomes 12–50 m, and the guard is restated as the condition it is meant to secure | A tuning value, and a rule | **Pass.** No covering stand on the Siege castle, with 5.5 m to spare | The trebuchet must stand 40–50 m from a garrison. It can shell the keep from outside reach only after a side's towers are silenced. One larger castle, 10 sections a side, still fails by 4 m² |
| (b) The gate faces outwards | T5's gate moves to the inner end of its passage, and a trebuchet's shot descends at 45° or steeper | A template change (proposed only), and a tuning value | **Pass.** No shot steeper than 30.3° reaches the gate. By trebuchet it takes 231 s (316 s with repair), against 180 s (270 s) for a section | T5 must be redrawn, and the fix rests on the Proposed descent |
| (c) Undermining and the ram at a wall's base have no target | Both get the trebuchet's target of about three minutes. A ram blow comes every 15 s, and a standard foundation reaches 4 m down on deep earth | A rule, and two tuning values | **Pass.** 156 s for the ram (246 s with repair), and 198 s for the sappers | Repair now works between ram blows. A section on deep earth needs 64 m³ of foundation |
| (d) The shot price | 1.5 m³ a shot | A tuning value | **Pass.** A ratio of 0.47, or 0.52 with repair | A breach costs the attacker only 14–20 m³ |

With all four applied, criterion 1 still passes: no route wins every column, and the gate by trebuchet is now dominated. Criterion 2 passes on its first clause.

**What still fails:**
- Below a 16 s reload, one engine gains less than 1.3× from repair, and nothing at 10 s or less. Criterion 3 passes because the reload is Proposed at 20 s.
- At 50 m, the 10-section variant still has 4 m² of covering stands. Only the restated guard catches that.
- No ram cadence gives the gate its four minutes (ambiguity 3).

**Before the revision.** The first run gave three verdicts. Criterion 1 passed weakly, at stone spans of 2 and 4. Criterion 2 passed on its second clause. Criterion 3 failed: under the ten-second lock, no single repair rate gave 1.3–2× across reloads of 6–30 s for one engine and two together.

## How the test works

- **The castle** is drawn on the 1 m grid from T1–T5. x runs east, y north, and the attacker's camp lies to the south. The plan and its choices are in *The Siege castle* below.
- **Materials** come from the palette, by behaviour. Masonry, index 6, has the behaviour of building stone. So it takes the stone column of the crater table, and its rubble yields stone (owner, 2026-10-31). Timber and roof (7 and 12) are timber. An opening is a decoration, and counts as its template's main material (ADR-005).
- **Reach.** A garrison reaches a target on the ground if the horizontal distance from its building's outer face is within the archers' range, plus one band of range per elevation step. Shots start at the outer face (GameDescription.md:213). Where the test asks about soldiers at a wall's foot or in a breach, it also asks for line of sight. A flat shot leaves the platform 1 m above its floor and aims at a point 1 m above the ground. It must clear every other structure, sampled every 0.25 m.
- **Engines** lob, so they need range and nothing else. A trebuchet's *stand* is any 1 m cell outside the castle and its earthworks, out to 130 m. Whether a stand is outside tower range is judged by distance alone. Ignoring line of sight can only shrink the safe area, so this is the strictest reading of F-01.
- **Breaches** use [ADR-004](../../ADR/ADR-004-rule-based-destruction.md)'s support rule, run on the real T3 voxels, with three sections end to end. A voxel's support distance is the fewest sideways steps on a path that moves only down or sideways through solid voxels to the ground. A voxel falls if that distance exceeds its material's span, and pieces cascade. Stone spans 2 m (owner, 2026-10-31). A section is open when foot soldiers have a route. That is a column empty through the whole 2 m thickness for 2 m of height, either from the ground or from 1 m up (the step onto a rubble heap). This is GameDesign.md:207 and :279 made concrete.
- **Undermining** follows the slump rule (GameDesign.md:202). Earth stands at 45°: no column stands more than one voxel above its neighbour, and a column that does gives a voxel to it. The sappers dig at the wall's foot and carry out whatever slumps in, until the earth under the whole foundation has gone. The script counts what they dig in a cross-section of 1 m columns, and multiplies it by the section's 8 m. E3's neighbours stand on rock, which holds a vertical face at the trench's ends. The first version dug a fixed trench, 8 × 2 × 3 m. The slump rule makes the dig 72 m³, not 48.
- **The gate** is attacked by lobbed shots along the passage, the attacker's best line. Each shot descends at a fixed angle and strikes the first solid voxel on its path. A hit on the gate takes a very large crater of timber: the voxel struck and its six neighbours, timber only. The stone beside the gate takes the trebuchet's medium crater, which reaches no sideways neighbour. The first version also took 1 m³ of stone from beside the gate.
- **Repair** uses the review's working for F-03, c / (c − r), with the lock and a fixed rate per section. A landing removes c m³. Repair is locked for ten seconds after each landing. It then restores r m³ a second until the next landing, never more than has been removed. The section opens when the net removed volume reaches the volume to open. Landings are counted as a real number, which averages over where in its last shot a section opens. A single siege moves in whole shots, so the repair table shows both counts. The pass mark is the owner's: with one engine, 1.3–2× at the Proposed reload.
- **Dominance.** A route *wins* a column when it is best or tied for best from the attacker's side. The columns are: shorter times, less stone spent, more stone the defender must find, fewer kinds of defender that can hurt the breakers, and an earlier era. A route that wins every column is dominant (the review's F-02 test). The script also reports which routes are *dominated*: another route is at least as good in every column and better in one.

## The Siege castle

The castle is 62 m square over its towers, with a 52 m bailey. Six T3 sections run on each of the east, west and north curtains. The south curtain has four sections, the gatehouse and a fifth tower, ST. A 9 m gatehouse and a 7 m tower together take the length of two 8 m sections, so this is the only way the south curtain closes on the 8 m grid. The keep stands in the middle, with its forebuilding towards the gate. The hall stands in the south-west of the bailey. A standard ditch with its rampart rings the castle. It stops for a causeway in front of the gate, and in front of **E3**, the weak section, which stands on deep earth. Everything else stands on 1 m of soil over common rock.

- **Nearest section to the camp:** S3, 103 m away. It stands on rock behind the ditch, between the gatehouse and ST.
- **Weak section:** E3. It is the only section on earth and the only one with no ditch. It is also among the farthest from towers: 20 m to the nearest, tied with E4, N3, N4, W3 and W4. It is 130 m from the camp, so the castle's nearest section is not its weakest, as GameDesign.md:294 requires.
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

## Values

### Decided by the owner, 2026-10-31

| Value | Decided | What it changes here |
|---|---|---|
| Stone span | **2 m**, two voxels | It is the only span used. T2's floors and roof are now timber, so every template's stone stands at 2 m; the widest stone support distance is T4's and T5's 2 m. The rerun at a span of 4 is dropped. One row of the span table keeps 4 as a sensitivity. |
| Masonry, palette index 6 | **The package's "dressed stone"**: ordinary stone laid by a template | The templates take the stone column of the crater table, and their rubble yields stone. The Masons' dressed-stone upgrade will get an index of its own ([Palette.json](../../../GameData/Server/Palette.json), index 6). The script now reads materials from the palette, not from the legend's names. |
| Repair's pass mark | **One engine: repair adds 1.3–2× to the time to open a section, at the Proposed trebuchet reload** | Criterion 3 is restated. Two engines are reported as the behaviour the lock intends. |
| Repair lock | **Ten seconds stays** | Two engines firing alternately keep a section locked (GameDescription.md:137). |

### Proposed by this test

| Value | Proposed | Reason |
|---|---|---|
| Archers' range on the level | **30 m**, six bands | Mantlets are out of the trial, so the trebuchet has to outrange the towers to bombard from outside their fire. Tower reach is set well inside the trebuchet's range. 30 m plus two bands also lets two towers 48 m apart cover all the curtain between them: "towers that overlap their fire" (GameDescription.md:265). |
| One band of range | **5 m** | One sixth of the level range, so a band is a round number of metres and range reads in bands (Hud.md:107, "HEIGHT +1 band"). |
| Elevation | Towers and gatehouse fire from **11 m**, the keep from **16 m** | These are the floors the templates give: T4's and T5's merlon layer, and T2's roof floor. Each is at least 6 m over flat ground, so every garrison gets the cap of two steps (GameDescription.md:215). Tower reach is therefore **40 m** from the outer face. |
| Garrisons | A squad of **six archers** in each of the five towers, the gatehouse and the keep | Hud.md's GARRISON n / 6. Archers are the only ranged unit in the trial's roster (GameDesign.md:73). The gatehouse also has its Decided passage attack. |
| Garrisons put out | A gatehouse whose gate breaks, and a keep that stops working, put their garrisons out, as a tower does | GameDescription.md:217 states this for towers only. The same rule for every garrisoned building is the simplest. |
| Sally | Men-at-arms and outriders, out to **70 m** from the curtain | "A cavalry sally to catch the engines" is one of the defender's ways to win (GameDescription.md:266). A leash shorter than the trebuchet's range would leave a trebuchet untouchable. |
| Trebuchet reload | **20 s**: nine shots in the three-minute target | It is the middle of F-03's 6–30 s and the review's own example. |
| Volume to open a section | **20 m³** net: a hole 5 m long and 2 m high through the 2 m wall at its base | This hole is wider than twice the stone span, so the stone above it starts to fall, as in the design's picture of a breach (GameDescription.md:258). It is far more than the 1 m × 2 m minimum route, because lobbed shots do not cut a neat slot. It only scales the repair rate's units. |
| Repair rate | **0.08 m³/s a section** (4.8 m³ a minute) | At this rate, one engine at the Proposed 20 s reload takes 1.5× as long, the middle of the 1.3–2× band. The repair table gives the rates that pass at every reload. |
| Repair rates a fix must hold at | **0.06–0.10 m³/s** | A quarter either side of 0.08 m³/s, because the rate is unset and fix (c) lets repair act between ram blows. |
| Ram | A blow every **5 s**; **1 voxel** of stone a blow, from the bottom metre only; **4 voxels** of timber | 5 s stays inside the lock, as F-02 assumed ("contact hits keeping it inside the repair lockout"). Voxels have no damage state (GameDescription.md:176), so one voxel is the smallest crater there is. "Small, at the base only" (:180) is read as the bottom metre. "Large" in timber is four voxels. Fix (c) changes the cadence. |
| Trebuchet's very large crater in timber | A voxel and its **six neighbours**, timber only | It is the next step up the ladder of crater sizes on a 1 m grid: small 1, medium about 2, large 4, very large 7. Stone beside the timber takes the trebuchet's medium crater in stone, which is the voxel struck and the one behind it, so a hit on timber takes no stone. |
| Trebuchet's descent | **45°** or steeper | Engines lob (GameDescription.md:213). On flat ground a lob descends at the angle it was thrown, and 45° is the throw of a lob's longest range. A trebuchet shortens its throw by throwing slower, not flatter. Only fix (b) depends on this value. |
| Gate threshold | The gate works while at least **half** its 12 voxels stand | The simplest share. MvpAssets.md names the gate as T5's critical part but gives no threshold. |
| Sappers | A laborer digs earth at **0.1 m³/s**, so six sappers dig 1.2 m³/s | Twenty laborers quarrying at 0.1 m³/s for 15 minutes cut 1,800 m³. The keep needs 1,530 m³ of that as stone by minute 10–15 (GameDescription.md:124). Earth is "dug quickly" (:59), so this rate is if anything slow. Sappers dig at twice a laborer's speed, which is Decided. |
| Undermining | The slump rule on 1 m columns, under the section's whole length | It is GameDesign.md:202's slope rule, applied rather than assumed. |
| Standard foundation | **1 m** deep | It reaches rock under thin soil, and rests on earth over deep earth (GameDescription.md:133, ambiguity 9). Fix (c) deepens it on deep earth. |
| Collapse warning | **1 s per metre** of the falling piece's height | "A taller piece warns for longer" (GameDescription.md:192), stated as simply as possible. |
| Ground | 1 m of soil over common rock, with deep earth under E3 | Walls founded on rock are a defender's strength (GameDescription.md:263), and the trial needs one section on earth (GameDesign.md:294). |
| Ditch | The standard ditch (MvpAssets.md:60), 4–9 m beyond the tower faces, with its rampart 1–4 m out. A causeway crosses it at the gate, and there is no ditch in front of E3 | The ditch keeps engines and sappers at a distance (GameDescription.md:264). The gap follows the v5 scene's "one unditched stretch on deep earth". |
| Camp | 100 m south of the castle | It defines which section is nearest. |
| Times exclude | The approach, deployment and packing, for every route | The comparison is the time to open from the moment the breaker starts work. |
| "About" a target time | Within **a quarter** of it, so about three minutes is 135–225 s | The design's targets say "about" and give no tolerance. A quarter separates the three-minute target from the four-minute gate target. |
| "About half" | **0.4–0.6** | The same reading for GameDesign.md:57's ratio. |

## Range findings (F-01)

Every garrison reaches 40 m from its building's outer face. The trebuchet's placeholder range is 12–70 m (Hud.md:119).

- **The trebuchet can hit the great tower from outside every garrison's reach.** By distance alone, 1,340 m² of stands do it: 498 m² on the north side, 360 m² on the east, 360 m² on the west and 122 m² on the south. The closest is 55.5 m from the great tower, about 33 m beyond the north towers' faces. The safest is 12 m beyond every reach.
- **The Decided guard is violated.** GameDesign.md:42 asks that engine range be less than tower reach plus the shortest distance from keep to curtain. Here that is 40 + 20 = 60 m, and the placeholder range is 70 m.
- **The guard's formula is not sufficient either.** Between two towers, their reach ends closer to the curtain than "tower reach measured from the curtain line". On this castle the real bound is 55.5 m, 4.5 m below what the formula allows. Grown to 9–12 sections a side, with towers at the corners only, the castle satisfies the guard and still leaves 570–856 m² of covering stands.
- **Sensitivity.** On this castle the covering area shrinks from 1,340 m² at 30 m archers to 368 m² at 35 m and 24 m² at 40 m, and vanishes from 45 m (55 m tower reach).
- **Every garrison can be shelled from outside every garrison's reach.** Each tower and the gatehouse can be hit from 40 m, and the keep from 55.5 m.

**What the win rule makes of it.** The keep is broken only when it has stopped working *and* the attacker's soldiers have held its foot for two minutes (GameDescription.md:36, GameDesign.md:42). Shelling the great tower therefore cannot win. The attacker's soldiers still need a breach or the gate, so H2's breach data is not empty, and F-01's failure scenario cannot happen. **Criterion 2 passes on its second clause.** The guard is a Decided rule all the same, and fix (a) makes it hold.

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

## The route table (F-02), as the documents stand

All seven ways in are scored. The trebuchet takes the same time and stone on every T3 section, so most sections share one row. The per-section table in the appendix gives what differs between them: who covers each section's foot and its inner mouth, the stand-off, and the distance to the keep. Repair is at the Proposed 0.08 m³/s, with one trebuchet at a 20 s reload and Hud.md's 6 m³ a shot. Times and stone with repair are expected values.

| Route | Open (s) | Open with repair (s) | Attacker stone m³ (unrepaired / repaired) | Defender stone m³ | Defenders who can hurt the breakers | Era (breaker) | Columns won | Dominated | Why pick it |
|---|---|---|---|---|---|---|---|---|---|
| Gate · ram | 240 | 240 | 0 / 0 | 0 (+7 timber) | GH (passage), sally (2) | Hall | 2/6 | yes | it is the gate; no stone; the ram's roof stops tower arrows |
| Gate · trebuchet | 40 | 40 | 12 / 12 | 0 (+8 timber) | sally (1) | Citadel | 3/6 | no | it is the gate; two shots from outside every reach; breaking the gate puts out the gatehouse's garrison |
| S3 · trebuchet (nearest) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | nearest the camp; stand-off outside every reach |
| E3 · trebuchet (weak) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | on earth, no ditch, far from towers; stand-off outside every reach |
| Other sections · trebuchet | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | the same in every scored column as the two above; the storm leg differs |
| E3 · sappers (undermining) | 68 | 68 | 0 / 0 | 56 (+ the ground under it) | SE, NE, sally (3) | Keep | 2/6 | no | the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild |
| E3 · ram at the base | 56 | 56 | 0 / 0 | 21 | sally (1) | Hall | 3/6 | no | no ditch, so the ram can reach the foot; arrow-proof; blows keep repair locked |

No route is dominant. The script keeps one sensitivity row at a stone span of 4: there, nothing falls above the trebuchet's hole, the ram needs 18 blows (96 s), and an undercut must run 9 m, into the neighbouring sections.

**What the table says:**

1. **No route wins every column, so criterion 1 passes.** The pass is weak, because six columns that pull in different directions make a clean sweep unlikely. The findings below say more.
2. **The gate by ram is dominated** by ramming E3's base. That route is faster (56 s against the 240 s target), faces only a sally, and leaves the defender more to rebuild, for the same stone and era.
3. **The gate by trebuchet is the fastest route, at 40 s.** T5's gate is a 12 m³ timber panel in the gatehouse's outer face (MvpAssets.md:21). A trebuchet's very large crater in timber (GameDescription.md:182) takes 8 of its 12 voxels in two shots. It does this from a stand 40 m off, outside every garrison's reach. Breaking the gate also puts the gatehouse's garrison out. **This is F-02's failure, arriving through another engine** ([fix b](#b-the-gate-faces-outwards)).
4. **The weak section works as designed.** Its own two routes, the ram and the sappers, take 56 s and 68 s and are possible only there. The nearest section is never competitive. A trebuchet takes the same 180 s (270 s with repair) on every section, so for a trebuchet the choice of section comes down to the storm leg.
5. **Two fast routes have no balance target.** At the Proposed rates, ramming the base of an unditched stone wall and undermining it are 2.6–3.2× faster than the trebuchet's target. Both keep repair locked throughout ([fix c](#c-undermining-and-the-ram-at-a-walls-base-have-no-target)).
6. **The shot price does not meet its rule.** A breach costs the attacker 54 m³, nine shots of 6 m³. The defender rebuilds 29 m³, so the ratio is 1.86 against GameDesign.md:57's "about half" ([fix d](#d-the-shot-price)).
7. **The ram's gate target does not fit its craters.** A ram making large craters of 4 voxels breaks the 12-voxel gate in two blows: 10 s at a blow every 5 s, or 30 s at a blow every 15 s. At a blow every 60 s or slower, repair keeps up and the gate never breaks. No cadence gives about 240 s. The target holds only if the passage attack keeps the ram off the gate for most of the four minutes (ambiguity 3).

**Storming.** Every route ends with soldiers walking to the keep's foot and holding it for two minutes.

| Opening | Width | Garrisons that can shoot its inner mouth | To the keep's foot (m) |
|---|---|---|---|
| Gate passage | 3 m, 7 m long | SW, SE, ST, KEEP (GH's own is put out when its gate breaks) | 16 |
| S3 | 5 m (trebuchet) | GH, ST, KEEP | 18 |
| E3 | 5 m (trebuchet), 1 m (ram), 4 m (sappers) | SE, NE, GH, ST, KEEP | 20 |

The keep's foot is covered by two or three tower garrisons on every side. The north side, with only NW and NE, is the least covered. Every way in is under fire from three to five garrisons until the trebuchet brings towers down.

## The repair model (F-03)

**The criterion, as the owner restated it:** with one engine, repair adds 1.3–2× to the time to open a section, at the Proposed trebuchet reload. The three-minute target holds at every reload, so one engine needs round(180 / reload) shots, each removing 20 m³ / shots. Repair is locked for 10 s after every landing. The multiplier is the breach time with repair over the time without, for the same engines. Two engines *in step* land together; *offset* means they alternate, half a reload apart.

| Reload (s) | Shots | m³ a shot | One engine, unrepaired (s) | x one engine | x one engine, whole shots | In 1.3–2x? | Rates for 1.3–2x, one engine (m³/s) | x two in step (intended) | x two offset (intended) |
|---|---|---|---|---|---|---|---|---|---|
| 6 | 30 | 0.67 | 180 | 1.00 | 1.00 | no | none | 1.00 | 1.00 |
| 8 | 22 | 0.91 | 176 | 1.00 | 1.00 | no | none | 1.00 | 1.00 |
| 10 | 18 | 1.11 | 180 | 1.00 | 1.00 | no | none | 1.00 | 1.00 |
| 12 | 15 | 1.33 | 180 | 1.13 | 1.13 | no | 0.162–0.345 | 1.06 | 1.00 |
| 15 | 12 | 1.67 | 180 | 1.29 | 1.33 | no | 0.082–0.174 | 1.11 | 1.00 |
| 20 | 9 | 2.22 | 180 | 1.50 | 1.56 | yes | 0.056–0.118 | 1.17 | 1.00 |
| 25 | 7 | 2.86 | 175 | 1.62 | 1.71 | yes | 0.049–0.103 | 1.19 | 1.06 |
| 30 | 6 | 3.33 | 180 | 1.77 | 1.83 | yes | 0.044–0.091 | 1.21 | 1.11 |

The multipliers are at the Proposed 0.08 m³/s.

- **At the Proposed 20 s reload, one engine takes 1.50× as long, or 1.56× in whole shots** (14 shots against 9). **Criterion 3 passes.** The Proposed rate sits inside the passing window at 20 s, 0.056–0.118 m³/s.
- **It holds at reloads of 16–30 s.** At 15 s it gives 1.29×, just short of the band; 1.33× in whole shots. If the owner wants it to hold from 15 s, a rate of 0.082–0.090 m³/s serves every reload from 15 to 30 s.
- **At 10 s or less, repair does nothing,** whatever the rate. A single engine's landings never leave the section unlocked. This is the lock working as Decided, not a defect in the rate.
- **Two engines, as intended.** Firing alternately, they keep repair locked out (1.00×) at every reload up to 20 s, and allow 1.06–1.11× at 25–30 s. Landing together at 20 s, they allow 1.17×. This is "what concentrating engines buys" (GameDescription.md:137), and it is reported here as intended behaviour.

## The Proposed fixes

Each fix below is the smallest change found that makes its check pass. A tuning value is preferred over a rule, and a rule over a template. Every fix is checked by the script; the numbers are in the appendix.

### (a) The engine range guard

**What breaks.** GameDesign.md:42 bounds an engine's range by tower reach plus the shortest keep-to-curtain distance. That is 60 m here, against the placeholder of 70 m. The formula is also not sufficient. On this castle it holds at 56–59 m, while 2–38 m² of stands still cover the great tower. At 70 m it holds on castles of 9–12 sections a side that leave 570–856 m² of covering stands.

**The fix, in two parts.**
- **(a1) A tuning value:** the trebuchet's longest range becomes 50 m, in place of Hud.md's 70 m placeholder.
- **(a2) A rule:** the guard is restated as the condition it is meant to secure. No point outside every garrison's reach lies within an engine's range of the great tower. It is measured on the castle's plan.

**Proof.**

| Sections a side | Formula: reach + keep to curtain (m) | Closed form (m) | Measured bound (m) | Covering m² at 70 m | Guard as written at 70 m | Covering m² at 50 m | Measured guard at 50 m |
|---|---|---|---|---|---|---|---|
| 6 (the Siege castle) | 60 | 55.0 | 55.5 | 1340 | violated | 0 | holds |
| 7 | 64 | 55.6 | 56.5 | 766 | violated | 0 | holds |
| 8 | 68 | 55.0 | 56.5 | 590 | violated | 0 | holds |
| 9 | 72 | 52.4 | 53.5 | 570 | holds | 0 | holds |
| 10 | 76 | 39.0 | 48.5 | 708 | holds | 4 | violated |
| 11 | 80 | 43.0 | 52.5 | 840 | holds | 0 | holds |
| 12 | 84 | 47.0 | 56.5 | 856 | holds | 0 | holds |

- On the Siege castle, no stand covers the great tower at 50 m. That is 5.5 m inside the measured bound.
- Every section still has a safe stand within 36–44 m of its face, so every route in the route table survives. Every tower and the gatehouse can still be shelled from 40 m.
- The closed form is a hand check that is never above the measured bound. It is the distance from the great tower to the nearest point where two neighbouring garrisons' reaches meet outside the castle. On the Siege castle that point is 23 m from the great tower to the north towers' faces, plus √(40² − 24²) = 32 m beyond them, which makes 55 m.
- At 50 m the formula passes every castle here. The measured guard still fails the 10-a-side castle, by 4 m², where the corner towers' reaches only just meet at the curtain. Only the restated rule catches it. On such a castle the Skirmish AI must add a tower to the long side, or set the keep further in. At 45 m every castle here passes.

**What it costs.**
- A trebuchet outside every reach must now stand 40–50 m from a garrison's face. The safe stands in range of each tower fall from about 5,500 m² to about 1,400 m², and of the gatehouse from 3,671 m² to 506 m². Every one of them is inside the sally's 70 m leash, so the sally becomes the engine's main threat.
- The keep can be shelled from outside every reach only after the towers on one side have been silenced, which the trebuchet can do from 40 m. The siege gains a step.
- The guard becomes a check that a tool runs over a plan, not a sum a designer does in their head. The closed form remains a quick test.

**Alternatives.**
- Archers at 45 m on the level, so 55 m of reach, clear this castle at 70 m (the sensitivity table). But they lengthen every garrison's reach, which moves every other number in this test.
- Dropping the guard, which the storming rule makes redundant, changes a Decided rule. It also gives up the guarantee on castles the Skirmish AI builds.

**Recommended:** (a1) and (a2) together.

### (b) The gate faces outwards

**What breaks.** T5's gate is the front row of its passage (row z = 6 of 0–6), flush with the outer face ([the template](../../../GameData/Server/Templates/earthrise-t5-gatehouse-1m.json)). A trebuchet breaks it in two shots, 40 s, from outside every reach, at every descent from 15° to 70°. This is F-02 again. It also contradicts GameDescription.md:168, which has the garrison fire "down into the passage in front of it". With the gate at the front there is no passage in front of it, so the Decided passage attack (GameDesign.md:53) cannot reach a ram working on the gate.

**Neither tuning nor a rule fixes it.** The crater table orders very large above large. Even a very large crater no bigger than the ram's large one, 4 voxels, breaks the gate in 2 shots. At 3 voxels it takes 3 shots (60 s), and at 2 voxels, below the ram's crater, 5 shots (100 s). A rule that engines cannot harm a gate would break "shots are physical" (GameDescription.md:213). So this fix is a template change, the last resort, plus one tuning value.

**The fix.**
- **A template change, proposed only:** T5's 12 gate voxels move from row z = 6 to row z = 0, the inner end of the passage. The 3 m × 4 m passage is left open in front of the gate, under the gatehouse's 5 m of stone. The template keeps its 585 m³ of stone and 12 m³ of timber.
- **A tuning value:** a trebuchet's shot descends at 45° or steeper.

**Proof.** Shots come straight along the passage, the attacker's best line.

| Descent (degrees) | T5 as drawn | T5 with the gate at the inner end |
|---|---|---|
| 15–20 | broken in 2 shots, 40 s | stands: 3 shots take 7 voxels, and no later shot reaches it |
| 25–30 | broken in 2 shots, 40 s | stands: 1 shot takes 4 voxels, and no later shot reaches it |
| 35–70 | broken in 2 shots, 40 s | no shot reaches it |

- The steepest descent at which any shot reaches the recessed gate is 30.3°. That leaves 14.7° of margin below the Proposed 45°.
- At 45°, the trebuchet must first cut back the arch over the passage. That is 19 m³ of stone, about 8.5 shots at the section rate, or 12.8 with repair. Then 3 shots break the gate. The gate takes 231 s unrepaired and 316 s with repair, against 180 s and 270 s for a section. In the fixed route table, the gate by trebuchet is dominated.
- A ram must now enter the passage to reach the gate, where the passage attack reaches it. That is the mechanism F-02's fix intended.

**What it costs.**
- T5 must be redrawn. The template file is not changed here. T1–T7 are due to be redrawn natively at 1 m before M4 anyway (MvpAssets.md:66).
- The fix rests on the Proposed descent. At 30° or flatter, shots reach the recessed gate. At 15–20°, three shots take 7 of its 12 voxels, and repair alone saves it. A flatter engine may reach it; that is the mangonel, which is out of the trial, so this is a Skirmish question.
- The geometry is simple: a 45° shot aimed at the gate's bottom row reaches it only if less than 3.5 m of passage roof lies in front of it. A gate set only halfway into the 7 m passage would still be reachable.
- T5's passage roof is solid. If the passage attack is to be physical (GameDescription.md:213), the redraw needs openings above the passage (ambiguity 19).

**Recommended:** the template change and the descent together.

### (c) Undermining and the ram at a wall's base have no target

**What breaks.** The balance targets (GameDescription.md:270–276) set three minutes for the trebuchet and about four for a ram at a gate. They set nothing for undermining, or for a ram at a wall's base. At this test's rates, the ram opens E3 in 56 s and the sappers in 68 s: 3.2× and 2.6× faster than the trebuchet. Both keep repair locked while they work, so against the trebuchet with repair (270 s) they are 4–5× faster.

**The fix.**
- **A rule:** each opens a standard section in about three minutes (135–225 s), unopposed and without repair, as one trebuchet does. Repair at most doubles that, at every repair rate from 0.06 to 0.10 m³/s.
- **Two tuning values that meet it:** a ram blow every 15 s, and a standard foundation that reaches 4 m down on deep earth.

**Proof.** The ram takes 10 voxels from the bottom metre of E3 before the stone above falls and a route opens.

| Ram blow every (s) | Open, unrepaired (s) | With repair at 0.06 / 0.08 / 0.10 m³/s (s) | Worst × | Meets the target |
|---|---|---|---|---|
| 5 (as now) | 56 | 56 / 56 / 56 | 1.00 | no |
| 10 | 106 | 106 / 106 / 106 | 1.00 | no |
| 12 | 126 | 141 / 147 / 153 | 1.21 | no |
| 13 | 136 | 162 / 173 / 186 | 1.37 | yes |
| 14 | 146 | 186 / 205 / 230 | 1.58 | yes |
| **15** | **156** | **214 / 246 / 291** | **1.87** | **yes** |
| 16 | 166 | 247 / 299 / 382 | 2.30 | no |

| Foundation on deep earth (m) | Earth the sappers dig (m³) | Open (s) | Stone that falls (m³) | Foundation stone a section (m³) | Meets the target |
|---|---|---|---|---|---|
| 1 (as now) | 72 | 68 | 56 | 16 | no |
| 2 | 120 | 109 | 72 | 32 | no |
| 3 | 168 | 150 | 88 | 48 | yes |
| **4** | **224** | **198** | **104** | **64** | **yes** |
| 5 | 288 | 252 | 120 | 80 | no |

- A ram inside the lock, a blow every 10 s or faster, can never pass: it opens the section in 106 s at most. From a blow every 16 s, repair more than doubles the time at 0.10 m³/s.
- Keeping the 1 m foundation instead, a laborer would have to dig earth at 0.035 m³/s. That is 35% of the Proposed 0.1 m³/s, which the tempo's quarrying already needs, and it contradicts "Earth: dug quickly" (GameDescription.md:59).
- The proposed values are the passing ones nearest 180 s. The smallest passing values are a blow every 13 s and a 3 m foundation. They sit at 136 s and 150 s, near the band's lower edge, and are the alternative if the owner prefers the smaller change.

**What it costs.**
- The ram no longer holds the lock: repair works between its blows. At a gate it still breaks through in 2 blows (30 s) unopposed, so the gate's four minutes still rests on the passage attack, as before.
- The palisade target, under a minute, is not checked here. At a blow every 15 s it allows four blows.
- The foundation costs 64 m³ of stone a section on deep earth, if foundations are paid for at their volume (GameDescription.md:91). That is 48 m³ more than at 1 m, half a section. Building on deep earth gets slower too.
- After the fixes, E3's ram and sappers are still its fastest routes: 156 s unrepaired, and 198 s with repair. The weak section is still the weakest, by about 1.2–1.4× instead of 3–5×.

**Alternatives.**
- **For the ram, a rule:** it cannot damage stone. That drops "chips stone at the base" (GameDescription.md:243) and the ram's stone crater (:180). Hud.md's ram card already reads "TARGET Gates, timber". But it removes the route instead of balancing it, and it leaves a Hall-era attacker nothing against stone.
- **For the sappers:** the slower earth rate above, which contradicts "dug quickly".

**Recommended:** the target with the two tuning values.

### (d) The shot price

**What breaks.** GameDesign.md:57 tunes a shot's stone so that a breach costs the attacker about half the defender's rebuild. At Hud.md's 6 m³ a shot, a breach costs the attacker 54 m³ against 29 m³ rebuilt, a ratio of 1.86. With repair the attacker pays 81 m³ against 39 m³, a ratio of 2.08.

**The fix.** A tuning value: 1.5 m³ a shot.

**Proof.**

| Shot (m³) | Attacker, unrepaired / repaired (m³) | Ratio | Ratio, repaired | Net of rubble | Net, repaired | About half? |
|---|---|---|---|---|---|---|
| 6.0 (as now) | 54.0 / 81.0 | 1.86 | 2.08 | 2.20 | 2.35 | no |
| 2.0 | 18.0 / 27.0 | 0.62 | 0.69 | 0.73 | 0.78 | no |
| **1.5** | **13.5 / 20.2** | **0.47** | **0.52** | **0.55** | **0.59** | **yes** |
| 1.0 | 9.0 / 13.5 | 0.31 | 0.35 | 0.37 | 0.39 | no |

Exactly half needs 1.61 m³ a shot unrepaired, and 1.44 m³ with repair. The 9 m³ that falls is stone rubble, because masonry is building stone, and it clears for 4.5 m³ of stone. The "net" columns count that.

**What it costs.**
- Stone hardly limits the trebuchet any more. A breach costs the attacker 14–20 m³, about a fifth of one 96 m³ section, and shelling a tower or the keep becomes cheap in stone.
- Hud.md's SHOT 6 m³ placeholder changes.
- If a shot must cost whole cubic metres, 2 m³ gives 0.62–0.69, a little above "about half".

**Alternative, a rule:** keep 6 m³ and restate the ratio as about two. That reverses the review's F-13, which priced shots so that the attacker keeps an edge in stone.

**Recommended:** 1.5 m³.

## The route table with the fixes

All four fixes applied: a range of 50 m, the gate at the inner end with a 45° descent, a ram blow every 15 s, a 4 m foundation on deep earth, and 1.5 m³ a shot.

| Route | Open (s) | Open with repair (s) | Attacker stone m³ (unrepaired / repaired) | Defender stone m³ | Defenders who can hurt the breakers | Era (breaker) | Columns won | Dominated | Why pick it |
|---|---|---|---|---|---|---|---|---|---|
| Gate · ram | 240 | 240 | 0 / 0 | 0 (+7 timber) | GH (passage), sally (2) | Hall | 2/6 | no | it is the gate; no stone; the ram's roof stops tower arrows |
| Gate · trebuchet (notch, then gate) | 231 | 316 | 17 / 24 | 19 (+8 timber) | sally (1) | Citadel | 1/6 | yes | it is the gate, but the arch over the passage must be cut back first |
| S3 · trebuchet (nearest) | 180 | 270 | 14 / 20 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | nearest the camp; stand-off outside every reach |
| E3 · trebuchet (weak) | 180 | 270 | 14 / 20 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | on earth, no ditch, far from towers; stand-off outside every reach |
| Other sections · trebuchet | 180 | 270 | 14 / 20 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | the same in every scored column as the two above; the storm leg differs |
| E3 · sappers (undermining) | 198 | 198 | 0 / 0 | 104 (+ the ground under it) | SE, NE, sally (3) | Keep | 3/6 | no | the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild |
| E3 · ram at the base | 156 | 246 | 0 / 0 | 27 | sally (1) | Hall | 4/6 | no | no ditch, so the ram can reach the foot; arrow-proof; repair works between its blows |

- **No route is dominant, so criterion 1 still passes.** No route wins more than four of the six columns.
- **The gate is no longer the fastest route.** By trebuchet it now takes 231 s (316 s with repair), and that route is dominated by every section breach.
- **The gate by ram is no longer dominated**, because the ram at E3's base now takes 246 s with repair, against the gate's 240 s target.
- **The weak section is still the weakest,** but by a margin of about 1.2–1.4×, not 3–5×. The fastest unrepaired route is E3's ram, at 156 s; the fastest with repair is E3's sappers, at 198 s.

## Pass or fail

1. **No breach route wins every column: pass, weakly.** The trebuchet on the gate is the fastest route, at 40 s, and two fast routes at the unditched, earth-founded wall have no balance target. With fixes (b) and (c) it still passes, and those two weaknesses are gone.
2. **Engine stands outside tower range over the great tower: they do cover it, and the win rule makes that harmless. Pass on the second clause.** The Decided range guard is violated by the placeholder range, and its formula is not sufficient. With fix (a) it passes on the first clause on the Siege castle.
3. **With one engine, repair adds 1.3–2× at the Proposed 20 s reload: pass, at 1.50× (1.56× in whole shots).** It holds at reloads of 16–30 s. It fails at 15 s (1.29×), and at 10 s or less repair does nothing. Two engines firing alternately lock repair out at every reload up to 20 s, as intended.

**What remains open.** Criterion 3 passes because the reload is Proposed at 20 s; a faster trebuchet would fail it. Fix (a) at 50 m still fails one larger castle by 4 m². Fix (b) rests on a Proposed descent, and fix (c) on a Proposed range of repair rates. No ram cadence gives the gate its four minutes; with fix (b), that target rests on the passage attack, which combat must deliver.

## Ambiguities and contradictions in the design

**Settled by the owner on 2026-10-31,** and removed from this list: the lock against the pass mark, the stone span against the keep, and what "dressed stone" means in the templates.

1. **The range guard.** GameDesign.md:42 bounds engine range by "tower reach plus the shortest distance from keep to curtain". That formula is not sufficient between towers (fix a). "Tower reach" also has no definition, because the archers' range and a "band of range" (GameDescription.md:215) are unset.
2. **The timber span.** "Timber spans furthest" (GameDescription.md:188) is not a number. T2's floors and roof need at least 4 m, and T1's roof 5 m, as the script computes from their voxels.
3. **The gate's four minutes.** GameDescription.md:274 and GameDesign.md:53 give about four minutes under fire. GameDescription.md:180 gives the ram a "Large" crater in timber, and T5's gate is 12 m³ (MvpAssets.md:21). With the ten-second lock, no cadence fits. With fix (b) the ram works in the passage, so the target rests on the passage attack.
4. **The gate faces outwards.** T5's gate is flush with its outer face, which contradicts GameDescription.md:168 (fix b).
5. **The ram on stone** has no balance target (fix c). "At the base only" (GameDescription.md:180) is also unspecified: the bottom metre, or the bottom two? If two, a 1 × 2 m hole, four voxels, is a route through a 2 m wall in four blows.
6. **Undermining has no balance target** (fix c).
7. **The shot price.** "About half what rebuilding it costs the defender" (GameDescription.md:250) does not fit Hud.md:119's 6 m³ a shot at nine shots a breach (fix d). The rule is also silent on whether the rubble the defender clears for stone counts against the rebuild; this test shows both.
8. **Whether a foundation is paid for.** "A building costs exactly the volume it is made of" (GameDescription.md:91), but MvpAssets.md leaves foundations out of its costs. That decides what fix (c) costs.
9. **The standard foundation.** It "reaches down to firm ground" (GameDescription.md:133), which is undefined on deep earth. Fix (c) proposes 4 m.
10. **The trebuchet's trajectory** is stated nowhere beyond "engines lob" (GameDescription.md:213). Fix (b) needs a descent.
11. **The keep around the hall.** "The keep, built in stone around the hall" (GameDescription.md:280). T2's great tower is 8 m × 8 m inside, and T1 is 11 m × 7 m, so the hall cannot fit. This castle puts the hall in the bailey.
12. **Who is put out.** Only a tower that stops working puts its garrison out (GameDescription.md:217). The rule is silent on gatehouses and keeps, and both matter: the gate route, and the hold at the keep's foot.
13. **"From the owner's side" and "one whole building".** For a free-standing keep, every side is the owner's. For the gate, the outer face is the attacker's. A section is "one whole building" (GameDescription.md:137), so the keep's 1,722 m³ repairs at the same rate as a 96 m³ wall section, and a gatehouse's gate shares one lock with its stone.
14. **A wall on common rock.** GameDescription.md:201 says undermining "works only where the wall stands on earth". Hud.md:257 says that "only bedrock or a deep foundation stops it". This test reads a wall founded on common rock as safe, because there is no tunnelling and rock does not slump.
15. **Elevation never differs in the trial.** Every MVP garrison stands 11 m or more above flat ground, so all of them hit the two-step cap (GameDescription.md:215). Height separates no tower from the keep.
16. **The Siege defender's scope.** GameDesign.md:291 says it "fights and repairs, and does nothing else". MvpPlan.md:49 adds sallies to a leash and shooting sappers, and GameDescription.md:257 has it sally against the engines.
17. **Rubble carries support** (ADR-004, consequences). A collapse's rubble landing in a slot or a trench could support the wall above again. This test does not model it, and the resolver needs a rule.
18. **One trebuchet.** The Siege trial's fixed army has one trebuchet (GameDesign.md:73). In the trial, two engines on one section means the trebuchet and the ram. With criterion 3 restated for one engine, the two-engine case is a Skirmish question.
19. **The passage attack and physical shots.** T5's passage roof is solid stone, 5 m thick. If the garrison's passage attack is to be a physical shot (GameDescription.md:213), the redraw needs openings above the passage.
20. **"Open" on the 1 m grid is cheap.** A route needs one voxel by two (GameDesign.md:207), so a 2 m wall opens once 4 m³ goes from its base in the right place. The trebuchet's three-minute target therefore constrains its craters and its aim more than the wall.
21. **T2's listed cost is stale.** MvpAssets.md:18 still gives T2 as 1,722 m³ of stone. With its floors and roof in timber it is 1,530 m³ of stone and 192 m³ of timber.

## Not modelled

These are left out: combat and attrition, because hit points and damage are unset; aim scatter and the shape of trebuchet craters, which belong to the M3 breach lab; rubble carrying support; and the approach, deployment and packing times. Lines of sight ignore ramparts, and the F-01 stands ignore line of sight altogether, which is deliberately strict. Undermining is worked on one cross-section, times the section's length; the trench's ends and the rubble of the fall are not modelled. In fix (b), the arch is assumed to stand while the trebuchet cuts it back. The defender AI's choices, such as when it repairs or sallies, are not simulated.

## Appendix: the script's output

Produced by `python Tools/PaperTests/SiegeRouteTable.py` with Python 3.13 on Linux, unchanged.

```text
SIEGE CASTLE ROUTE TABLE · M0 paper test for review findings F-01, F-02, F-03
Revised on the owner's decisions of 2026-10-31, with Proposed fixes (a) to (d)

== Templates read from GameData/Server/Templates, materials from GameData/Server/Palette.json ==
| Id | Template | W x H x D (m) | Stone m³ | Timber m³ | Critical part: voxels | Palette materials (index): voxels | Widest support distance (m) |
|---|---|---|---|---|---|---|---|
| T1 | Hall | 11 x 7 x 7 | 0 | 188 | Hall frame: 188 | opening (20) 3, roof (12) 87, timber (7) 98 | timber 5 |
| T2 | Keep | 12 x 19 x 15 | 1530 | 192 | Great tower: 1632 | masonry (6) 1526, opening (20) 4, timber (7) 192 | stone 0, timber 4 |
| T3 | Stone wall, standard | 8 x 7 x 2 | 96 | 0 | None (sections open by the breach rule): 0 | masonry (6) 96 | stone 0 |
| T4 | Tower | 7 x 12 x 7 | 479 | 0 | Firing platform: 110 | masonry (6) 477, opening (20) 2 | stone 2 |
| T5 | Gatehouse | 9 x 12 x 7 | 585 | 12 | Gate: 12 | masonry (6) 585, timber (7) 12 | stone 2, timber 0 |
Every template's stone stands at the Decided stone span of 2 m: yes (the widest is 2 m).
Masonry is stone like any other: craters from the stone column, rubble that yields stone.
Timber spans furthest. The timber span each template needs to stand whole: T1 5 m, T2 4 m.

== Values Decided by the owner on 2026-10-31 ==
| Value | Decided |
|---|---|
| Stone span | 2 m; T2's floors and roof are timber |
| Masonry (palette 6) | the package's "dressed stone": stone laid by a template, under the ordinary stone rules |
| Repair's pass mark | one engine: repair adds 1.3–2x to the time to open a section, at the Proposed reload |
| Repair lock | 10 s after each hit; two engines alternating keep a section locked |

== Values this test chose (Proposed) ==
| Value | Proposed |
|---|---|
| Archers' range on the level | 30 m, six bands of 5 m |
| Elevation | towers and gatehouse fire from 11 m, the keep from 16 m: two steps each |
| Garrisons | a squad of 6 archers in each tower, the gatehouse and the keep; GH adds its passage attack |
| Sally | men-at-arms and outriders, out to 70 m from the curtain |
| Trebuchet reload | 20 s, so 9 shots open a standard section in 180 s |
| Volume to open a section | 20 m³ net: 5 m x 2 m through the wall at its base |
| Repair rate | 0.08 m³/s a section (4.8 m³ a minute): one engine at 20 s then takes 1.5x as long |
| Ram | a blow every 5 s; 1 voxel of stone a blow, bottom metre only; 4 voxels of timber |
| Very large crater (trebuchet, timber) | a voxel and its six neighbors, of timber only |
| Gate threshold | the gate works while at least 50% of its voxels stand |
| Sappers | a laborer digs earth at 0.1 m³/s, so six sappers dig 1.2 m³/s |
| Standard foundation | 1 m deep: on rock under thin soil, on earth over deep earth |
| Collapse warning | 1 s per metre of the falling piece's height |
| Ground | 1 m of soil over common rock, except deep earth under the weak section |
| Camp | 100 m south of the castle |
| "About" a target time | within 25% of it; "about half" is 0.4–0.6 |

== Values the Proposed fixes change ==
| Value | As the documents stand | Proposed fix |
|---|---|---|
| (a) Trebuchet range | 12–70 m (Hud.md placeholder) | 12–50 m |
| (a) Range guard | engine range < tower reach + keep-to-curtain distance | no stand outside every garrison's reach is in engine range of the great tower |
| (b) T5's gate | the front row of its passage, flush with the outer face | the inner end of its passage (a template change, proposed only) |
| (b) Trebuchet's descent | not stated | 45 degrees or steeper |
| (c) Targets | none for undermining or a ram at a wall's base | each opens a standard section in about 180 s, unopposed |
| (c) Ram | a blow every 5 s | a blow every 15 s |
| (c) Standard foundation on deep earth | 1 m | 4 m |
| (d) A shot's stone | 6 m³ (Hud.md placeholder) | 1.5 m³ |

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
| Target | Safe stands in range at 70 m | At 50 m (fix a) | Closest (m) |
|---|---|---|---|
| SW | 5549 m² | 1370 m² | 40.0 |
| SE | 5549 m² | 1366 m² | 40.0 |
| NW | 5669 m² | 1473 m² | 40.0 |
| NE | 5669 m² | 1473 m² | 40.0 |
| GH | 3671 m² | 506 m² | 40.0 |
| ST | 4112 m² | 510 m² | 40.5 |
| KEEP | 1340 m² | 0 m² | 55.5 |

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

== Breach mechanics on T3, from ADR-004's support rule with the Decided stone span of 2 m ==
T3 is masonry (palette 6), which takes the ordinary stone rules: what falls is stone rubble, which clears
for stone at 50% of its volume.
Trebuchet: a hole of 20 m³ (5 m x 2 m through 2 m) at the base is a route (yes); 9 m³ above it then falls.
Ram: 10 blows cut a slot along the bottom metre; the stone above it falls in pieces, 11 m³ in all,
  leaving a gap 1 m wide over the rubble, the last piece after a 6 s warning. Route: yes.
Sappers: under the slump rule, a squad digs 72 m³ (9 m³ a metre) before the ground has gone from under
  E3's 1 m foundation along its 8 m; at 1.2 m³/s that takes 60 s. 56 m³ falls, 4 m of the section,
  after a warning of 8 s. The ends within 2 m of sound sections stay up. Route: yes.
Gate by trebuchet: 2 very large craters take 8 of the gate's 12 timber voxels, and no stone;
  the gate works while 6 stand, so it is broken after 40 s, repair between the shots included.
Gate by ram, checked against the craters: a large crater of 4 voxels a blow, every 5 s, breaks the gate
  in 2 blows, 10 s, against the Decided target of 240 s.
  A blow every 15 s: 2 blows, 30 s.
  A blow every 30 s: 2 blows, 60 s.
  A blow every 60 s: repair keeps up, the gate never breaks.
  A blow every 120 s: repair keeps up, the gate never breaks.
  No cadence gives about 240 s at 4 voxels a blow: the target holds only if the passage attack
  keeps the ram off the gate most of the time.

At each span, on T3 sections end to end (the Decided span, and 4 m as a sensitivity only):
| Span (voxels) | Falls above the trebuchet's 5 m hole (m³) | Ram at the base, to a route | Shortest undercut that opens a route | Within one 8 m section? |
|---|---|---|---|---|
| 2 (Decided) | 9 | 10 blows, 56 s | 5 m (11 m³ falls) | yes |
| 4 (sensitivity) | 0 | 18 blows, 96 s | 9 m (11 m³ falls) | no: it reaches into the neighbours |

== Route table, as the documents stand ==
| Route | Open (s) | Open with repair (s) | Attacker stone m³ (unrepaired / repaired) | Defender stone m³ | Defenders who can hurt the breakers | Era (breaker) | Columns won | Dominated | Why pick it |
|---|---|---|---|---|---|---|---|---|---|
| Gate · ram | 240 | 240 | 0 / 0 | 0 (+7 timber) | GH (passage), sally (2) | Hall | 2/6 | yes | it is the gate; no stone; the ram's roof stops tower arrows |
| Gate · trebuchet | 40 | 40 | 12 / 12 | 0 (+8 timber) | sally (1) | Citadel | 3/6 | no | it is the gate; two shots from outside every reach; breaking the gate puts out the gatehouse's garrison |
| S3 · trebuchet (nearest) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | nearest the camp; stand-off outside every reach |
| E3 · trebuchet (weak) | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | on earth, no ditch, far from towers; stand-off outside every reach |
| Other sections · trebuchet | 180 | 270 | 54 / 81 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | the same in every scored column as the two above; the storm leg differs |
| E3 · sappers (undermining) | 68 | 68 | 0 / 0 | 56 (+ the ground under it) | SE, NE, sally (3) | Keep | 2/6 | no | the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild |
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
Repair at 0.08 m³/s a section; one trebuchet at a 20 s reload; 6.0 m³ a shot; a ram blow every 5 s.

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
A multiplier is the breach time with repair over the time without, for the same engines. The pass mark
is for one engine (owner, 2026-10-31); two engines are shown as the behaviour the lock intends.
Repair: 0.08 m³/s.

| Reload (s) | Shots | m³ a shot | One engine, unrepaired (s) | x one engine | x one engine, whole shots | In 1.3–2x? | Rates for 1.3–2x, one engine (m³/s) | x two in step (intended) | x two offset (intended) |
|---|---|---|---|---|---|---|---|---|---|
| 6 | 30 | 0.67 | 180 | 1.00 | 1.00 | no | none | 1.00 | 1.00 |
| 8 | 22 | 0.91 | 176 | 1.00 | 1.00 | no | none | 1.00 | 1.00 |
| 10 | 18 | 1.11 | 180 | 1.00 | 1.00 | no | none | 1.00 | 1.00 |
| 12 | 15 | 1.33 | 180 | 1.13 | 1.13 | no | 0.162–0.345 | 1.06 | 1.00 |
| 15 | 12 | 1.67 | 180 | 1.29 | 1.33 | no | 0.082–0.174 | 1.11 | 1.00 |
| 20 | 9 | 2.22 | 180 | 1.50 | 1.56 | yes | 0.056–0.118 | 1.17 | 1.00 |
| 25 | 7 | 2.86 | 175 | 1.62 | 1.71 | yes | 0.049–0.103 | 1.19 | 1.06 |
| 30 | 6 | 3.33 | 180 | 1.77 | 1.83 | yes | 0.044–0.091 | 1.21 | 1.11 |

At 0.08 m³/s, one engine's multiplier is inside 1.3–2x at reloads of 16–30 s.
Repair does nothing for one engine at reloads of 10 s or less: no landing leaves the section unlocked.
Two engines firing alternately keep repair locked out (x1.00) at every reload up to 20 s.
One rate serves one engine at every reload from 15 s to 30 s: 0.082–0.090 m³/s.

== Proposed fix (a): the engine range guard ==
The trebuchet's longest range becomes 50 m, and the guard reads: no stand outside every garrison's reach
is within an engine's range of the great tower, measured on the castle's plan.
| Sections a side | Castle | Formula: reach + keep to curtain (m) | Closed form: nearest meeting of two reaches (m) | Measured: nearest stand outside reach (m) | Covering m² at 70 m | Guard as written at 70 m | Covering m² at 50 m | Guard as written at 50 m | Measured guard at 50 m |
|---|---|---|---|---|---|---|---|---|---|
| 6 | 62 m | 60 | 55.0 | 55.5 | 1340 | violated | 0 | holds | holds |
| 7 | 70 m | 64 | 55.6 | 56.5 | 766 | violated | 0 | holds | holds |
| 8 | 78 m | 68 | 55.0 | 56.5 | 590 | violated | 0 | holds | holds |
| 9 | 86 m | 72 | 52.4 | 53.5 | 570 | holds | 0 | holds | holds |
| 10 | 94 m | 76 | 39.0 | 48.5 | 708 | holds | 4 | holds | violated |
| 11 | 102 m | 80 | 43.0 | 52.5 | 840 | holds | 0 | holds | holds |
| 12 | 110 m | 84 | 47.0 | 56.5 | 856 | holds | 0 | holds | holds |
The Siege castle (6 a side): no stand covers the great tower at 50 m, 5.5 m inside the measured bound.
The formula is not sufficient: on the Siege castle it holds at 56–59 m, where 2–38 m² of stands cover the great tower.
At 70 m it holds on castles of 9, 10, 11, 12 sections a side, which leave stands in range.
At the fixed range, the measured guard still fails on castles of 10 a side (4 m²): a castle the Skirmish AI builds
must add a tower to a long side, or keep its keep further in.
The closed form is no larger than the measured bound, within the 1 m grid, on every castle here: yes.
Every section still has a safe stand within 44 m of its face, inside the fixed range.

== Proposed fix (b): T5's gate at the inner end of its passage ==
T5's gate is row z = 6 of 0–6, its front face, so nothing stands between it and a lobbed shot. The fix moves its
12 timber voxels to row z = 0 and leaves the 3 m x 4 m passage open in front of it, under 5 m of the gatehouse's stone.
The template keeps 585 m³ of stone and 12 m³ of timber. Shots come along the passage, the attacker's best line.
| Descent (degrees) | T5 as drawn | T5 with the gate at the inner end |
|---|---|---|
| 15 | broken in 2 shots, 40 s | stands: 3 shots take 7 voxels, and no later shot reaches it |
| 20 | broken in 2 shots, 40 s | stands: 3 shots take 7 voxels, and no later shot reaches it |
| 25 | broken in 2 shots, 40 s | stands: 1 shot takes 4 voxels, and no later shot reaches it |
| 30 | broken in 2 shots, 40 s | stands: 1 shot takes 4 voxels, and no later shot reaches it |
| 35 | broken in 2 shots, 40 s | no shot reaches it |
| 40 | broken in 2 shots, 40 s | no shot reaches it |
| 45 | broken in 2 shots, 40 s | no shot reaches it |
| 50 | broken in 2 shots, 40 s | no shot reaches it |
| 60 | broken in 2 shots, 40 s | no shot reaches it |
| 70 | broken in 2 shots, 40 s | no shot reaches it |
Tuning the crater alone does not do it. If the trebuchet's very large crater in timber were no larger than
the ram's large one, each shot taking that many gate voxels, the gate as drawn would break at:
  4 voxels, 2 shots (40 s); 3 voxels, 3 shots (60 s); 2 voxels, 5 shots (100 s).
The steepest descent at which any shot reaches the recessed gate: 30.3 degrees.
At the Proposed 45 degrees the trebuchet must first cut back the arch: 19 m³ of stone, about 8.5 shots at the section rate
(12.8 with repair); then 3 shots break the gate. Expected: 231 s unrepaired and 316 s with repair,
against 180 s and 270 s for a section. A ram must now enter the passage to reach the gate, where the gatehouse's
passage attack reaches it, as GameDescription.md (Buildings) describes.

== Proposed fix (c): targets for undermining and for a ram at a wall's base ==
Target: each opens a standard section in about 180 s (135–225 s), unopposed and without repair, like one
trebuchet; repair at most doubles that, at every repair rate from 0.06 to 0.10 m³/s.
The ram takes 10 voxels from the bottom metre of E3 before the stone above falls and a route opens, after a 6 s warning.
| Ram blow every (s) | Open, unrepaired (s) | With repair at 0.06 m³/s (s) | With repair at 0.08 m³/s (s) | With repair at 0.10 m³/s (s) | Worst x | Meets the target |
|---|---|---|---|---|---|---|
| 5 | 56 | 56 | 56 | 56 | 1.00 | no |
| 8 | 86 | 86 | 86 | 86 | 1.00 | no |
| 10 | 106 | 106 | 106 | 106 | 1.00 | no |
| 12 | 126 | 141 | 147 | 153 | 1.21 | no |
| 13 | 136 | 162 | 173 | 186 | 1.37 | yes |
| 14 | 146 | 186 | 205 | 230 | 1.58 | yes |
| 15 | 156 | 214 | 246 | 291 | 1.87 | yes |
| 16 | 166 | 247 | 299 | 382 | 2.30 | no |
| 17 | 176 | 287 | 371 | 533 | 3.03 | no |
| 20 | 206 | 476 | 926 | never | never opens | no |
| Foundation on deep earth (m) | Earth the sappers dig (m³) | Digging (s) | Warning (s) | Open (s) | Stone that falls (m³) | Foundation stone a section (m³) | Meets the target |
|---|---|---|---|---|---|---|---|
| 1 | 72 | 60 | 8 | 68 | 56 | 16 | no |
| 2 | 120 | 100 | 9 | 109 | 72 | 32 | no |
| 3 | 168 | 140 | 10 | 150 | 88 | 48 | yes |
| 4 | 224 | 187 | 11 | 198 | 104 | 64 | yes |
| 5 | 288 | 240 | 12 | 252 | 120 | 80 | no |
| 6 | 360 | 300 | 13 | 313 | 136 | 96 | no |
Keeping the 1 m foundation instead, a laborer would have to dig earth at 0.035 m³/s, 35% of the
Proposed 0.1 m³/s that the tempo's quarrying already needs, against "Earth: dug quickly"
(GameDescription.md, Materials).
Ram cadences that meet the target: 13 s, 14 s, 15 s. Foundations that meet it: 3 m, 4 m.
The passing values nearest 180 s: a blow every 15 s and a 4 m foundation. The smallest: 13 s and 3 m.
Proposed: a blow every 15 s (156 s, 246 s with repair) and a 4 m foundation on deep earth (198 s).

== Proposed fix (d): the shot price ==
A breach is 9 shots unrepaired and 13.5 with repair. The defender rebuilds 29 m³ (20 m³ of crater and 9 m³ that fell),
and repairs 10.0 m³ under fire. The 9 m³ that fell is stone rubble, which clears for 4.5 m³ of stone (net columns).
| Shot (m³) | Attacker, unrepaired (m³) | Attacker, repaired (m³) | Ratio | Ratio, repaired | Ratio, net of rubble | Ratio, net and repaired | About 0.5 (first two)? |
|---|---|---|---|---|---|---|---|
| 6.0 | 54.0 | 81.0 | 1.86 | 2.08 | 2.20 | 2.35 | no |
| 3.0 | 27.0 | 40.5 | 0.93 | 1.04 | 1.10 | 1.17 | no |
| 2.0 | 18.0 | 27.0 | 0.62 | 0.69 | 0.73 | 0.78 | no |
| 1.5 | 13.5 | 20.2 | 0.47 | 0.52 | 0.55 | 0.59 | yes |
| 1.0 | 9.0 | 13.5 | 0.31 | 0.35 | 0.37 | 0.39 | no |
Exactly 0.5 unrepaired needs 1.61 m³ a shot; exactly 0.5 with repair needs 1.44 m³.

== Route table with the Proposed fixes (a) to (d) ==
| Route | Open (s) | Open with repair (s) | Attacker stone m³ (unrepaired / repaired) | Defender stone m³ | Defenders who can hurt the breakers | Era (breaker) | Columns won | Dominated | Why pick it |
|---|---|---|---|---|---|---|---|---|---|
| Gate · ram | 240 | 240 | 0 / 0 | 0 (+7 timber) | GH (passage), sally (2) | Hall | 2/6 | no | it is the gate; no stone; the ram's roof stops tower arrows |
| Gate · trebuchet (notch, then gate) | 231 | 316 | 17 / 24 | 19 (+8 timber) | sally (1) | Citadel | 1/6 | yes | it is the gate, but the arch over the passage must be cut back first |
| S3 · trebuchet (nearest) | 180 | 270 | 14 / 20 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | nearest the camp; stand-off outside every reach |
| E3 · trebuchet (weak) | 180 | 270 | 14 / 20 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | on earth, no ditch, far from towers; stand-off outside every reach |
| Other sections · trebuchet | 180 | 270 | 14 / 20 | 39 (10 repaired + 29 rebuilt) | sally (1) | Citadel | 1/6 | no | the same in every scored column as the two above; the storm leg differs |
| E3 · sappers (undermining) | 198 | 198 | 0 / 0 | 104 (+ the ground under it) | SE, NE, sally (3) | Keep | 3/6 | no | the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild |
| E3 · ram at the base | 156 | 246 | 0 / 0 | 27 | sally (1) | Hall | 4/6 | no | no ditch, so the ram can reach the foot; arrow-proof; repair works between its blows |

Dominant route: none
  Gate · ram: wins Attacker stone, Era
  Gate · trebuchet (notch, then gate): wins Breakers hurt by
  S3 · trebuchet (nearest): wins Breakers hurt by
  E3 · trebuchet (weak): wins Breakers hurt by
  Other sections · trebuchet: wins Breakers hurt by
  E3 · sappers (undermining): wins Open, repaired, Attacker stone, Defender stone
  E3 · ram at the base: wins Open, Attacker stone, Breakers hurt by, Era
Not dominated by any other route: Gate · ram, S3 · trebuchet (nearest), E3 · trebuchet (weak), Other sections · trebuchet, E3 · sappers (undermining), E3 · ram at the base
Dominated: Gate · trebuchet (notch, then gate), by S3 · trebuchet (nearest), E3 · trebuchet (weak), Other sections · trebuchet, E3 · ram at the base
Times and stone with repair are expected values (see the repair model); one siege moves in whole shots.
Repair at 0.08 m³/s a section; one trebuchet at a 20 s reload; 1.5 m³ a shot; a ram blow every 15 s.

== Verdict ==
As the documents stand, with the owner's decisions of 2026-10-31:
1. No breach route wins every column: PASS.
   Fastest: Gate · trebuchet 40 s, E3 · ram at the base 56 s, E3 · sappers (undermining) 68 s.
   Dominated: Gate · ram.
2. Engine circles from outside tower range over the great tower: 1340 m² of stands cover it.
   The win rule needs the attacker's soldiers to hold the keep's foot for two minutes, so shelling
   the great tower cannot win alone and a breach or the gate is still needed: harmless under the
   storming rule, PASS on the second clause. The placeholder 70 m range violates the Decided guard.
3. With one engine at the Proposed 20 s reload, repair adds 1.50x (1.56x in whole shots): PASS.
   Two engines at 20 s: x1.17 in step, x1.00 offset; the lock intends this.
The Proposed fixes:
(a) Trebuchet range 50 m and the guard measured: PASS on the Siege castle.
(b) T5's gate at the inner end of its passage: PASS; the gate takes 231 s by trebuchet (316 s with repair).
(c) A ram blow every 15 s, a 4 m foundation on deep earth: PASS; 156 s and 198 s.
(d) A shot at 1.5 m³: PASS.
With all four: 1. no route wins every column: PASS. 2. no stand outside every reach covers the great
tower: PASS. 3. is unchanged by them.
```

## Proposed text for GameDesign.md

Ten changes, ready to paste. Three are **Decided (owner, 2026-10-31)** and record what the owner settled; seven are **Proposed**, for the owner to confirm or change. GameDesign.md itself is not edited here.

| # | Change | Status | Where in GameDesign.md |
|---|---|---|---|
| 1 | Stone spans 2 m | Decided (owner, 2026-10-31) | New subsection, item 1. It settles an open value that :185 leaves to the paper tests, and replaces no line. |
| 2 | Masonry is ordinary stone | Decided (owner, 2026-10-31) | New subsection, item 2. It replaces no line; :202 does not name masonry. |
| 3 | Repair's pass mark is for one engine | Decided (owner, 2026-10-31) | New subsection, item 3. It settles "the repair spreadsheet's pass mark" of :185. |
| 4 | The range guard is measured, not computed | Proposed | Replaces the guard sentence of :42. |
| 5 | The trebuchet's range is 12–50 m | Proposed | New subsection, item 4. :333 keeps unit statistics open; Hud.md:119 holds the placeholder. |
| 6 | A trebuchet's shot descends at 45° or steeper | Proposed | New subsection, item 5. |
| 7 | T5's gate stands at the inner end of its passage | Proposed | New subsection, item 6. It extends the *Gatehouses* bullet at :53. |
| 8 | Undermining and a ram at a wall's base take about three minutes | Proposed | New subsection, item 7. |
| 9 | A ram blow every 15 s; a standard foundation 4 m deep on deep earth | Proposed | New subsection, item 8. It defines the foundation of :204 on deep earth. |
| 10 | A shot costs 1.5 m³ of stone | Proposed | New subsection, item 9. It sets the value that the *Shot price* bullet at :57 tunes. |

**A new subsection**, inserted after GameDesign.md:187, the end of *2026-10-31: starting the MVP*, and before *Pillars (Decided)*:

```markdown
### 2026-10-31: the Siege route table

The [Siege route table](Review/PaperTests/SiegeRouteTable.md) tested the Siege castle against its three criteria. After its first run, the owner settled three of the paper tests' open values (*Starting the MVP*, item 7). The rest are its fixes, Proposed for the owner to confirm or change.

1. **Stone spans 2 m** (Decided). T2's floors and roof are timber, so every template's stone stands at that span.
2. **Masonry is ordinary stone** (Decided). Palette index 6, masonry, is the stone a template lays, which the asset package calls dressed stone. It takes the crater table's stone column, and its rubble yields stone. The Masons' dressed-stone upgrade gets a palette index of its own.
3. **Repair's pass mark is for one engine** (Decided). With one engine, repair adds 1.3–2× to the time to open a section, at the Proposed trebuchet reload of 20 s. The ten-second lock stays, so two engines firing alternately keep a section locked, as intended.
4. **The trebuchet's range is 12–50 m** (Proposed), in place of Hud.md's placeholder of 12–70 m.
5. **A trebuchet's shot descends at 45° or steeper** (Proposed).
6. **T5's gate stands at the inner end of its passage** (Proposed). The gatehouse then fires into the passage in front of the gate, and a lobbed shot cannot reach the gate until the arch over the passage is cut back. T5 is redrawn with it before M4.
7. **Undermining, and a ram at a wall's base, each open a standard section in about three minutes, unopposed** (Proposed), as one trebuchet does. Repair at most doubles that.
8. **A ram strikes every 15 s, and a standard foundation on deep earth reaches 4 m down** (Proposed). These meet item 7.
9. **A shot costs 1.5 m³ of stone** (Proposed), so that a breach costs the attacker about half the defender's rebuild.
```

**A replacement at GameDesign.md:42.** In decision 2 of *2026-10-31: the design review*, the sentence "A balance bound stays as a guard: an engine's range is less than tower reach plus the shortest distance from keep to curtain that the Siege castle and the Skirmish AI build." becomes:

```markdown
A balance bound stays as a guard (Proposed, 2026-10-31): no point outside every garrison's reach lies within an engine's range of the great tower, on the Siege castle or on any castle the Skirmish AI builds. It is measured on the castle's plan, because between towers their reach ends closer to the curtain than tower reach measured from the curtain line.
```

**Other documents that would follow** if the owner adopts these, none of them edited here:
- GameDescription.md's balance targets (:270–276) would gain item 7.
- Hud.md:119's placeholders would change for the range and the shot.
- MvpAssets.md:18 would give T2 as 1,530 m³ of stone and 192 m³ of timber, and MvpAssets.md:21 would note where T5's gate stands.
- T5's template would be redrawn.
