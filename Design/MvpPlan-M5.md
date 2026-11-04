# Earthrise.World — M5 work orders

**Status:** Proposed, 2026-11-03, with [MvpPlan.md](MvpPlan.md)'s M5 revision, which is accepted when the owner merges its pull request. The owner answered its eighteen inputs on 2026-11-03, taking every proposal (order M5-A1; GameDesign.md, *M5's values*). No order from phase B on starts before that pull request merges. "Input N" means MvpPlan.md, M5, input N. These orders carry out M5 as MvpPlan.md, M5, defines it. **M5 takes no new ADR:** B1 amends [ADR-026](ADR/ADR-026-scenarios-and-the-siege-trial.md), decision 5, with three events, and C2 amends its decision 1 so that a scenario may hold no trial. The next ADR stays ADR-030.

**This plan is written before M3 and M4 close.** M3's six-person session, order M3-G3, has not run, and M4's playthroughs and report, order M4-J5, are not in. Both bear on M5. M3's reading decides whether the kill criterion's one round of legibility work is still unspent (input 17), and M4's playthroughs are the first evidence that a person can win the trial at all. The scripted attacker cannot: it loses to the clock in both versions, and in the control the keep never stops (GameDesign.md, *M4's phase F*, item 8, and *after M4's phase F*, item 3). Phases B to D are code and need neither. **Before phase E, this file is corrected in a commit of its own** against the M3 and M4 reports, as M4's plan was corrected against M3's code.

**M5 is mostly a session, not code.** H2 has three clauses (GameDesign.md, *Hypotheses*), and M3 and M4 built most of the instruments for them. M3's lab asks the prediction question and scores it. M4's trial plays both versions on two layouts, logs breaches by role, and records where they began on screen. What is missing is small but load-bearing:
- **The log does not record where a player tried to breach,** only where a section opened. It has no row for a ram's blow or a sapper's digging. And in the control a stone removes no voxel, so counting the voxels removed would measure the two versions differently. Without those rows, H2's clustering clause cannot be scored for a player who chose a point and failed to open it.
- **Nothing assigns a participant's versions and layouts,** so the facilitator would type `--control` and `--mirror` where the participant can see them.
- **Nothing captures a reason at the moment of choice.**
- **Nothing scores the three clauses together** against one frozen build.

Phases B to D build those in eight orders. Phases E and F are the pilot and the session, run by the owner.

**An order is one commit.** There are 13 orders in six phases:

| Phase | Orders | What lands | Waits on |
|---|---|---|---|
| A | A1 | The owner's decisions, written down | The owner's answers to inputs 1–18 |
| B | B1–B3 | The records: siege work as the trial's events and log rows; the playtest's scorer; replays checked headless | A |
| C | C1–C3 | The participant's run: `--siege --game`, the practice ground, the reason prompt | A; C3 also B1 |
| D | D1–D2 | The crater items | A |
| E | E1–E2 | The facilitator's guide and sheet; the pilot, its retune and the freeze | B, C and D; M3 and M4 closed (input 1) |
| F | F1–F2 | The session; the report and the verdict | E2 |

B, C and D can run side by side once A has merged. C3 needs B1's events. E waits for M3 and M4 to close. **Owner runs** fall after C1, C2, C3 and D2, then the pilot in E2, the session in F1, and B3's check of the session's replays on the benchmark machine. Most of M5 is the owner's time with participants, which is what sets its pace.

## What M5 inherits

- **From M3:** `Earthrise --lab`, its 16 items split 8 open and 8 shut by the simulation's own key, its log, `Tools/BreachLab/ScoreBreachLab.py`, which scores any number of participants, and [BreachLab.md](BreachLab.md), the facilitator's guide. M4's input 24 decided that M5 asks its prediction items through this lab, unchanged. **Unvalidated until M3-G3 runs.**
- **From M4:** `Earthrise --siege <participant> [--control] [--mirror]`, the trial's log and replay ([MvpPlan-M4.md](MvpPlan-M4.md), *The trial's log*), `Earthrise --replay`, and `Tools/SiegeTrial/ReadSiegeLog.py`. The reader reports breaches as nearest, weak, gate or another from the log's comment lines: S3, E3 and GH on Siege, and S2, W3 and GH on SiegeMirror. It also reports how many openings and warnings began under the deck or off screen, and how long the player took to frame them (M4's input 25).
- **Not yet known:** whether a person can win either version inside 40 minutes, how long a game takes, and whether the two versions are near enough in difficulty that a preference between them is about the walls and not about winning.
- **T3 as converted.** The owner keeps it as it is until after M5's verdict (input 2; GameDesign.md, *M4's phases B and C*, item 2).

## The session

What one participant does, as the inputs decide it. This is the protocol the orders build to. E1 writes it out for the facilitator as `Design/SiegePlaytest.md`, as M3-G2 wrote [BreachLab.md](BreachLab.md).

1. **The machine.** The benchmark machine, full screen at 1920 × 1080, UI scale 1.5 (the cap, M4's input 21), mouse and keyboard; one participant at a time (input 5).
2. **The briefing.** The facilitator explains the controls, the objective, the win and loss rules, and what the views and readouts are, and nothing about how walls, ground or craters behave (input 6).
3. **The practice ground,** five minutes: `Earthrise --practice <id>` (input 7).
4. **The first game:** `Earthrise --siege <id> --game 1`, to its outcome or the 40-minute limit. The game pauses for a reason the first time the player's stones strike a structure, the ram strikes one, and their sappers dig beside one (input 8). At its end the facilitator asks: *where did you break in, or try to, and why there?*
5. **A break.**
6. **The second game:** `--game 2`, the same.
7. **Preference:** *if you could play only one of the two again, which would it be?* The answer is the first or the second, and the reason is written down (input 10).
8. **The lab:** `Earthrise --lab <id>`, P0 and L01 to L16, with reasons taken before each answer, as BreachLab.md says (input 11).
9. **The crater items:** `Earthrise --craters <id>` (input 12).
10. **The debrief.** Only now may the facilitator explain the rules. Nothing said from here on is recorded.

**Assignment** (input 4). A participant's cell is (number − 1) mod 4, where the number is the two digits of their id. `--game` reads the cell from the id, so the facilitator never types a version or a layout:

| Cell | Ids | First game | Second game |
|---|---|---|---|
| 0 | S01, S05, N01, X01 | voxel, Siege | control, SiegeMirror |
| 1 | S02, S06, N02, X02 | control, Siege | voxel, SiegeMirror |
| 2 | S03, S07, N03 | voxel, SiegeMirror | control, Siege |
| 3 | S04, S08 | control, SiegeMirror | voxel, Siege |

With eight RTS players, two sit in each cell. Four play the voxel version first, four play it on each layout, and four start on each layout.

## How a session works an order

As [MvpPlan-M4.md](MvpPlan-M4.md), *How a session works an order*, items 1 to 13, with three differences:
- **Numbers are the inputs'** (item 11), and in M5 they move only in E2's retune. A test that pins a value the retune changes is updated in the retune's commit, and the commit says so.
- **An order that touches the trial changes no state.** B1's events and C3's pause are not state: they reach neither the save, nor the state hash, nor the state update. The scripted trial's committed tables (GameDesign.md, *M4's phase F*, item 9) and every replay test pass unchanged. An order that finds it must change state stops and reports.
- **Phases E and F commit data, not code:** logs, replays, sheets and reports. Their commits are named for the order as well, as in `M5-E2: Record the pilot and freeze the build`.

## Contracts

The seams more than one order builds on. M4's contracts stand, and these extend them.

### `Simulation` (B1)

```cpp
// Simulation/World.h: TrialEventKind gains three kinds after Outcome, so that no kind already raised moves:
//   StoneStruck - a stone that struck a voxel of a structure, in step 4;
//   RamStruck   - a ram's blow on a structure, in step 4;
//   SapperDug   - a cubic metre a sapper dug within 2 m of a section, in step 2.
// Each names the attacking player, the engine or the sappers' squad as subject, the structure as other, and the voxel
// struck or dug. Its amount is what the control's rates in RULE price the hit at, in hit points: controlShotDamage for a
// stone; controlRamDamage for a blow, or controlRamStonePercent of it when the voxel is stone; and
// controlTrenchDamagePerCubicMeter for a cubic metre dug (MvpPlan.md, M4, input 19). Each is raised in both versions,
// before the scenario's StructureRule applies the hit, so that both versions count siege work in one currency (input 9).
// The section a dug cubic metre counts against is found by one function, which HealthStructureRule::TrenchDug calls too,
// so the control and the count cannot drift apart.
```

### The trial's log (B1, C1, C3)

M4's log stands, and the reader still reads a log without the new rows. It gains:
- **`hit,<tick>,<player>,<stone|blow|trench>,<structure name>,<hit points>,`**, one row per B1 event;
- **`asked,<tick>,<structure name>,<stone|blow|trench>,<seconds paused>,,`**, one row per reason prompt (C3), written when the participant goes on;
- **three comment lines under `--game`** (C1): `game: 1` or `2`, `cell: 0` to `3`, and `group: rts`, `novice` or `pilot`, from the id's letter.

**A game's main points** (input 9, computed by the reader): every structure that took a quarter or more of the game's siege work, summed over its `hit` rows, each with its role. **Its breach point** is the main point with the most work, ties naming each. **A game clusters** when every one of its main points is the nearest section or the gatehouse, or when it has no `hit` row.

### `Earthrise` (C1 to C3, D2)

- **`Earthrise --siege <id> --game <1|2>`.** The id is S, N or X and two digits, from 01. The version and the layout come from the id's cell. `--control` and `--mirror` are refused beside `--game`. A bare `--siege` stays M4's.
- **`Earthrise --practice [<id>]`.** The practice ground, scenario `Practice`. It writes no log and no replay.
- **The reason prompt.** Only under `--game`; never under `--watch`, `--replay` or a bare `--siege`.
- **`Earthrise --craters <id>`** (D2): the crater items, with a log `Craters-<id>-<yyyymmdd-hhmmss>.csv` beside the lab's.

### The session's folder (B2, E1, F1)

```
Design/Review/SiegePlaytest/
  Session.csv   comment lines "# build ids: <id> [<id> ...]" and "# server-data hash: <hash>", then
                participant,group,cell,preferred,dropped   (preferred: first or second; dropped: empty, or why)
  Reasons.md    what each participant said, under their id, against each prompt and question, in their words
  Siege/        the trial's logs and replays, two of each a participant
  BreachLab/    the lab's logs, one a participant
  Craters/      the crater items' logs
  Pilot/        the pilot's logs, replays and sheet, which nothing scores
  Dropped/      the logs of a participant who did not finish
```

## Phase A — Decisions

### M5-A1 — Write down the owner's decisions
- **Waits on:** the owner's answers to MvpPlan.md, M5, inputs 1–18. Paper only.
- **Read first:** MvpPlan.md, M5, *The owner's inputs*; GameDesign.md, *Hypotheses*, *The playtest*, *Kill criteria*, *M4's values* items 24, 25 and 28, *M4's phases B and C* item 2, and *Open questions*; Hud.md §3.
- **Changes:**
  - **GameDesign.md** gains `### <date>: M5's values`, each input as Decided in the owner's words or as the owner changed it. *The playtest (Proposed)* and H2's line under *Kill criteria (Proposed)* become Decided as the inputs settle them (inputs 3, 9, 10, 11 and 17). The Open question on craters moves or stays as input 12 decides. If input 2 is taken, *M4's phases B and C*, item 2, is amended.
  - **MvpPlan.md:** each input's row says Decided. In both plans, phase D stays or goes with input 12, and order C2 with input 7.
  - **MvpAssets.md:** T3's known issue follows input 2.
- **Tests:** none. The checkers run, and every relative link resolves.

## Phase B — The records

### M5-B1 — Siege work as the trial's events
- **Waits on:** A1, and input 9.
- **Read first:** ADR-026, decision 5, with its M4-F1 and M4-J2 amendments; [ADR-028](ADR/ADR-028-the-control.md), decisions 2 to 4; `Simulation/StructureRule.h`; `Simulation/HealthStructureRule.cpp`, `RamBlow` and `TrenchDug`; `Simulation/World.h`, `TrialEventKind`, `TrialEvent` and `RamBlows`; `Client/SiegeLog.h` and `.cpp`; `Tools/SiegeTrial/ReadSiegeLog.py`; *Contracts*.
- **Changes:**
  - The three event kinds, as *Contracts* fixes them, priced from `RULE`. The section a dug cubic metre counts against becomes one function, which the control's `TrenchDug` and the event share.
  - `SiegeLog` writes the `hit` rows. `ReadSiegeLog.py` sums each structure's work by kind, and gives each run's main points, its breach point and whether it clusters, each point with its role, as text and in `--json`.
  - ADR-026, decision 5, amended with the three kinds and the reason they are priced in the control's hit points in both versions. No state, save, state hash or state update changes.
- **Tests:**
  - **`SiegeWorkTests`** (`SimulationTests`), on the Siege castle in both versions:
    - one scripted stone on S3 raises `StoneStruck` at `controlShotDamage`;
    - one blow on GH's gate raises `RamStruck` at `controlRamDamage`, and one on stone at its 25 %;
    - a sapper's cubic metre within 2 m of E3 raises `SapperDug` at `controlTrenchDamagePerCubicMeter`, and one 3 m out raises none;
    - for each of the three, the voxel version's event and the control's are equal, field by field.
  - **The scripted trial's committed tables** (`SiegeTrialTests`) and the replay tests pass unchanged.
  - **`SiegeLogTests`** (`ClientTests`): a `hit` row has every column.
  - **`Tools/TestReadSiegeLog.py`:**
    - a hand-written Siege log whose stones went to E3 and whose blows went to GH names E3, weak, as its breach point, and does not cluster;
    - its mirror, with the same work on W3, names W3, weak;
    - a log with forty stones on S3 and 342 m³ dug beside E3 has both as main points, S3 its breach point, and does not cluster;
    - a log whose work all went to S3 and GH clusters, and so does a log with no `hit` row;
    - M4's logs, without `hit` rows, still read.

### M5-B2 — The playtest's scorer
- **Waits on:** B1, and inputs 3, 4, 9, 10, 11, 13, 15 and 17.
- **Read first:** `Tools/SiegeTrial/ReadSiegeLog.py`; `Tools/BreachLab/ScoreBreachLab.py`; MvpPlan.md, M5, inputs 9 to 11 and 13; *The session*; *The session's folder*.
- **Changes:** `Tools/SiegeTrial/ScoreSiegePlaytest.py <folder> [--json]`, standard library only. It imports the reader and the lab's scorer by path, as `Tools/TestReadSiegeLog.py` imports the reader.
  - **It refuses, naming the cause, and scores nothing** when:
    - a participant in `Session.csv`, not dropped, lacks a game 1 log, a game 2 log or a lab log;
    - a log's version, layout or game disagrees with its id's cell;
    - a log's build id is not in the sheet's `# build ids:`, or its server-data hash is not the sheet's;
    - the lab's scorer refuses the `BreachLab/` folder;
    - the RTS group, not dropped, is not the number input 3 fixes.
  - **It prints, for each participant,** each game's version, layout, outcome and time, its main points and breach point with their roles, whether it clusters, and the structures it prompted for.
  - **It prints, for the RTS group:**
    - **clustering** (input 9): how many of the voxel games cluster, against the mark;
    - **prediction** (input 11): the lab's median over the group, against 0.65;
    - **preference** (input 10): how many preferred the game they played in the voxel version, against the mark;
    - each clause's PASS or FAIL, and H2's.
  - **It reports, without a mark:**
    - the same clustering count in the control games;
    - the voxel games' roles on each layout;
    - each version's wins;
    - which preferences went to the game the participant won;
    - the deck's figures under input 13;
    - the novices' figures, on their own.
  - **The crater items' median,** if `Craters/` exists, under input 12.
- **Tests:** `Tools/TestScoreSiegePlaytest.py`, with a step in the workflow's Linux job, over hand-written folders:
  - one that passes all three clauses;
  - one that fails each clause alone;
  - five clustering voxel games of eight, which fails clustering under input 9, and four, which passes;
  - a log from a build the sheet does not list, which is refused by name;
  - a game 1 log in the wrong version for its cell, which is refused;
  - a dropped participant, who is left out.
- **AGENTS.md:** the Linux job's list of tool tests gains the scorer's.

### M5-B3 — Replays checked headless
- **Waits on:** A1.
- **Read first:** ADR-014, decision 5; ADR-026, decision 6; `Simulation::ReplayPlayer`; `Earthrise/ReplayRun.cpp`; `SimulationBench/SimulationBench.cpp`'s modes.
- **Changes:** `SimulationBench --check-replays <folder>` plays every `.errp` in a folder headless, from the scenario its log names, and prints for each whether every state hash matched, the ticks played and the outcome. It exits 1 if one did not match. The check itself is a function in `Simulation`, so a test can call it.
- **Tests:** **`SiegeReplayTests`** gains: a short scripted run's replay passes the check, and the same replay with one command's tick moved fails it.
- **Owner runs:** after F1, on the benchmark machine, Release x64, over `Design/Review/SiegePlaytest/Siege/`. Every replay matches.

## Phase C — The participant's run

### M5-C1 — `Earthrise --siege <id> --game <1|2>`
- **Waits on:** A1, and inputs 3 and 4.
- **Read first:** `Earthrise/Earthrise.cpp`, the command line; `Earthrise/Siege.h` and `.cpp`; `Client/SiegeLog.h`; *The session*, the assignment table.
- **Changes:** `--game` as *Contracts* fixes it. The id's cell picks the version and the layout. The camera starts as M4-J1 starts it on each layout. The log gains its three comment lines. An id outside the form, or `--control` or `--mirror` beside `--game`, refuses with a message. Nothing the participant sees names the version or the layout: the banner, the objective and the end screen read as they do under a bare `--siege`.
- **Tests:** **`PlaytestTests`** (`ClientTests`):
  - S01 to S08, N01 to N03 and X01 give the table's cells, versions and layouts for both games;
  - Q01, S00 and S1 are refused;
  - over the eight S ids, four play the voxel version first, four play it on each layout, and four start on each layout;
  - the objective, the banner and the end screen read the same under `--game` as under a bare `--siege`.
- **Owner runs:** `Earthrise --siege X01 --game 1 --windowed`, then `--game 2`: the voxel version on Siege, then the control on SiegeMirror, with the log's comment lines as above.

### M5-C2 — The practice ground
- **Waits on:** A1, and input 7.
- **Read first:** ADR-026, decisions 1 to 3; `GameData/Server/Scenarios/Siege.json`, its camp; `Tools/CheckAssets.py`, check 16; `Earthrise/Run.h`.
- **Changes:**
  - **`GameData/Server/Scenarios/Practice.json`:** a levelled site of its own on the MVP map, away from the castle's, on the same ground as the camp; the attacker's army as it stands in the Siege camp; no structure and no defender's unit. ADR-026, decision 1, is amended so that a scenario may hold no `trial`, and check 16 accepts one without it.
  - **`Earthrise --practice [<id>]`,** in `Earthrise/Practice.h` and `.cpp`, filter `Siege`: the scenario under layout A's HUD, the objective `PRACTICE · ESC TO END`, every command card as in the trial, no clock and no outcome, until Esc.
- **Tests:**
  - `Tools/TestCheckAssets.py`: a scenario with no `trial` passes check 16, and one whose site overlaps the Siege castle's fails it;
  - `Tools/TestBakeContent.py`: `Practice` bakes;
  - **`PracticeTests`** (`SimulationTests`): `Practice` loads twice to one state hash, and holds no structure.
- **Owner runs:** `Earthrise --practice --windowed`. Every command a game uses can be tried: move, the squads' orders, control groups, deploy and pack, FIRE at the ground, TRENCH on open ground, pause.

### M5-C3 — The reason prompt
- **Waits on:** B1 and C1, and input 8.
- **Read first:** `Earthrise/Lab.cpp`, how the lab pauses the clock for a question; Hud.md §3, the mode banner; *Contracts*, the `asked` row.
- **Changes:** under `--game`, the run reads each tick's B1 events. On the tick of the first `StoneStruck`, the first `RamStruck` and the first `SapperDug` in a game, it pauses the clock after the tick and shows the banner `WHY <name>? SAY IT ALOUD · SPACE GOES ON`, naming the event's structure, outlined as the lab outlines its mark. Space unpauses and writes the `asked` row. At most three prompts a game, one of each kind. The commands and the ticks are unchanged, so a replay neither prompts nor differs.
- **Tests:** **`PromptTests`** (`ClientTests`, over the model the loop drives):
  - a game's first stone on E3 prompts `WHY E3?`, and its second stone does not;
  - a blow and a trench each prompt once;
  - nothing prompts under a bare `--siege` or `--watch`;
  - the clock is paused while the banner is up, and the next tick is the one after;
  - the banner fits at 1280 × 720 and 1920 × 1080 at each UI scale the width allows;
  - the voxel version and the control prompt alike for the same events (ADR-028, decision 6).
- **Owner runs:** `Earthrise --siege X01 --game 1 --windowed`: the three prompts, each once, and the `asked` rows in the log.

## Phase D — The crater items

### M5-D1 — The crater items
- **Waits on:** A1, and input 12.
- **Read first:** `Simulation/BreachLab.h` and `.cpp`; the ram's and the stone's crater code in `Simulation`; MvpPlan.md, M2, input 12, and M4, input 9; GameDesign.md, *M4's phase D*, item 3.
- **Changes:** `CraterItems()` in `Simulation/CraterLab.h` and `.cpp`, filter `World`, beside `BreachLab`: eight items, C1 to C8, and a practice item, C0. Each is a fresh world at the lab's site with W2 struck at a mark within its bottom metre, where the ram's blows land, by k blows of the ram or k stones of the trebuchet. The hits are stamped by the same crater functions the engines call, from the direction each engine strikes, with no engine left on the site, so that only the damage can tell. Four items are by the ram and four by the trebuchet, with k from 1 to 3 and the marks spread along W2. The key is the engine, by construction. `CraterItemsHash()` names the set.
- **Tests:** **`CraterLabTests`:** each item's world differs from the intact site in its crater's voxels alone; the set splits four and four; the hash is stable.

### M5-D2 — `Earthrise --craters <id>`
- **Waits on:** D1 and C1.
- **Read first:** M3-G1's order and `Earthrise/Lab.cpp`.
- **Changes:**
  - **The run:** C0, then C1 to C8, in an order seeded by the id as the lab seeds it. Each item is built headless to quiet and shown with the camera on W2 at the lab's framing. The banner reads `WHICH ENGINE MADE THE DAMAGE AT THE MARK? · RAM · R · TREBUCHET · T`, with no time limit. The camera and the views work. No result is shown.
  - **The log:** one row an item with its answer, the key and `correct`.
  - **The scorer:** `ScoreSiegePlaytest.py` reads `Craters/`.
- **Tests:** **`CraterRunTests`** (`ClientTests`): the order for one seed is a fixed permutation; an answer before the item is up is ignored; a row has every column.
- **Owner runs:** `Earthrise --craters X01 --windowed`: nine items, and nothing on screen but the damage tells the engine.

## Phase E — The pilot

### M5-E1 — The facilitator's guide and the session's sheet
- **Waits on:** C1 to C3 and D2, and inputs 3 to 8, 10, 11, 16 and 17.
- **Read first:** BreachLab.md, which E1's guide follows in shape; *The session*.
- **Changes:**
  - **`Design/SiegePlaytest.md`:** the facilitator's guide, written to the inputs as decided:
    - who takes part and how ids are given;
    - the consent line read to each participant (input 16);
    - the session step by step, with each command line;
    - what the facilitator says and does not say (input 6), and the end-of-game question;
    - how reasons are written down against the `asked` rows and the questions;
    - what to do when a game crashes or a participant stops (input 17);
    - where everything goes.
  - **`Design/Review/SiegePlaytest/Session.csv`,** its header and comment lines and no rows, and **`Reasons.md`,** its headings.
  - **AGENTS.md:** the tree's description of `Design/` names the Siege playtest's guide beside the breach lab's.
- **Tests:** none. The checkers run, and every relative link resolves.

### M5-E2 — The pilot, its retune and the freeze
- **Waits on:** E1, M3 closed and M4 closed (input 1), and inputs 13 to 15.
- **Read first:** MvpPlan.md, M5, inputs 13 to 15; the M3 and M4 reports.
- **Changes, in this order, each a commit of its own:**
  1. **The pilot** (input 14): the owner and the two pilot players, X01 and X02, through the whole session. Their logs, replays and sheet go to `Pilot/`.
  2. **The pilot's findings**, in `Pilot/Findings.md`:
     - the session's length, step by step;
     - each game's outcome;
     - the deck's figures under input 13, from the pilot's games and the owner's M4-J5 games;
     - every point where the protocol, the guide or the tools failed.
  3. **The retune, if the owner calls for one:** server data alone, in `GameData/Server/`, each value as `old → new` with its reason in the commit and in GameDesign.md, and the tests that pin a value updated in the same commit. A slimmer deck, if input 13's mark is met, is the one change to the client a retune may carry. The pilot runs again after a retune, with the same pilot players or new ones.
  4. **The freeze** (input 15): GameDesign.md gains `### <date>: M5's frozen build`, with the commit, its build ids (x64 Release) and its server-data hash. `Session.csv`'s comment lines carry them. From this commit until F2, nothing changes `GameData/Server/` or any code that runs in `Earthrise`.
- **Tests:** the suites pass on each retune commit.
- **Owner runs:** the pilot, on the benchmark machine, Release x64.

## Phase F — The session and the verdict

### M5-F1 — The session
- **Waits on:** E2's freeze, and the participants, whom the owner recruits (input 3).
- **Read first:** `Design/SiegePlaytest.md`.
- **Changes:** the session's logs, replays and sheet committed to `Design/Review/SiegePlaytest/` as the folder fixes them, with `Reasons.md`. A dropped participant's logs go to `Dropped/`, and their cell is filled by a new id: S09 takes S01's cell, S10 S02's, and so on.
- **Tests:** `ScoreSiegePlaytest.py` reads the folder with no refusal, and B3's check passes on the benchmark machine.
- **Owner runs:** the session; then B3's check of the replays.

### M5-F2 — The report and the verdict
- **Waits on:** F1, and input 17.
- **Read first:** MvpPlan.md, M5, *Done when* and input 17; GameDesign.md, *Kill criteria*; the M2 report's shape; MvpPlan-M3.md, M3-G3.
- **Changes:**
  - **`Design/Review/M5-<date>.md`** reports:
    - each clause's figures and verdict, and H2's, from `ScoreSiegePlaytest.py --json`;
    - each game's main points with the reasons given at their prompts and at the game's end. The owner codes each reason by the four GameDescription.md names (*How a siege plays*, 2: how near the section is to opening, its ground and what lies in front of it, its distance from the towers, the gate), or as distance from the camp, a test shot, or none. The report gives the counts, with every reason verbatim in `Reasons.md`, so anyone can code them again;
    - the lab's misses with their reasons, as M3-G3 reports them;
    - preference set beside each participant's outcomes;
    - the novices';
    - the craters', if phase D ran;
    - the deck's figures;
    - the session's length;
    - what the session suggests retuning, as proposals for M6 and M7, not applied to M5;
    - the CI runs per order, and each ADR amendment.
  - **On a fail,** the report names the failed clauses and what the reasons say about each. If the kill criterion's round is unspent, it proposes the round's changes under input 17: views, the HUD and on-screen words, never the rules, the values or the castle. The owner decides.
  - **GameDesign.md** gains `### <date>: closing M5`, with the owner's verdict. **MvpPlan.md:** the M5 record is complete.
- **Tests:** none. The checkers run, and every relative link resolves.
