# Earthrise.World — running the breach lab

The facilitator's guide to M3's breach lab ([MvpPlan.md](MvpPlan.md), M3; [MvpPlan-M3.md](MvpPlan-M3.md), M3-G1 to G3). The lab asks the half of H2 that M3 can ask without the Siege trial: can players who see the integrity view and the passability view predict whether a section will be open to foot after N more shots ([GameDesign.md](GameDesign.md), *Hypotheses*, H2)? Everything here was decided with M3's values ([GameDesign.md](GameDesign.md), *M3's values*, items 2 to 4 and 13). What is not from those inputs is marked **Proposed**, for the owner to keep or change before the session.

## Who takes part

- **Six people who play RTS games.** Each takes all sixteen items once (item 3). Number them in the order they arrive, P01 to P06; the number names the log and seeds the item order, so each person's order is their own.
- **Nobody who has seen the lab's items, the plan's work orders or this repository (Proposed).** Someone who knows the support rule's span answers a different question.
- **One at a time,** at the benchmark machine (MvpPlan.md, M3, *Done when*): the briefing, two minutes of free fire, the practice item and sixteen items. How long that takes is not measured yet; time the rehearsal below and book the sessions by it.

## Before the first participant

1. Build Release. The build writes `ServerData\` beside `Earthrise.exe`, and Earthrise refuses to start when the server's data and the client's were baked from different sources, and says why.
2. Rehearse: run `Earthrise --lab P00` through free fire, the practice item and all sixteen, and time it. It writes a log to `Logs\`; move it out of the way, since it is not a participant's.
3. Have a sheet with a line for each item id, P0 and L01 to L16, to write the reasons on.

## The command line

```
Earthrise --lab P03
```

- `--lab <participant>` is required. A participant name is letters, digits, `-` and `_`.
- `--seed <n>` sets the item order's seed. Without it, the seed is the participant name's hash, which is what the session uses.
- `--windowed` runs in a window. Without it the lab runs full screen, at the display's own UI scale; `--ui-scale 1`, `1.5` or `2` sets it.
- `Alt+Enter` switches between full screen and a window.

## What the participant sees

1. **Free fire, two minutes,** on the intact site: the trebuchet 40 m south of W2, the middle of the enclosure's south side, and six men-at-arms south of the wall, sent into the courtyard. Select the trebuchet with a left click, press F for FIRE, and right-click a target; Shift keeps FIRE on. The banner counts down.
2. **The practice item, P0,** then the sixteen in the participant's order. Each item is a fresh copy of the site, with prior damage the lab fires first, under the banner WATCH, at ten times speed.
3. **The question:** WILL W2 BE OPEN TO FOOT AFTER N SHOTS AT THE MARK? The clock is paused. The mark is drawn on the wall in the accent colour, with the voxel the shot's arc strikes beside it. The camera, the views and the HUD all work; FIRE does not.
4. **The answer:** O, or OPEN · O on the banner, for open; H, or SHUT · H, for shut. S pans the camera, so it is not an answer. There is no time limit, and no going back.
5. **The result:** the lab fires the N shots at the mark at ten times, with the passability view on, and shows W2 IS OPEN or W2 IS SHUT. Space goes on to the next item. Between items the lab runs the same item again with no screen, to check the live run against it, so the next item takes a moment to come up.

## What the facilitator says, and what not

**Say:**
- what *open* means: foot soldiers have a route into the courtyard through W2 once everything has settled. It is what the passability view shows, its cells in good;
- that no more shots will be fired than the question says, all of them at the mark;
- what the views are and how to toggle them: F1 the integrity view, on by default, with each section's HOLE, OPENING, GAP and GROUND and its weak point; F2 the passability view, the cells foot can reach in good and the rest it could stand in, in dim;
- how to move the camera: W, A, S and D or the arrows to pan, the wheel or + and - to zoom, [ and ] to turn, or the top bar's buttons;
- during free fire, how FIRE works, as above.

**Do not say:**
- how the support rule works, its span, or why stone falls. The views carry what the game shows; the rules are what the lab tests;
- whether an answer was right, during the session. The result shows the participant what happened, and that is all the feedback there is;
- anything about one item while another is up.

If a participant asks what a reading means, read the words on screen back to them. If they ask what will happen, say that is the question.

## Reasons

Before the participant answers, ask them to say why (Proposed: when). Write the reason down against the item's id, in their words, as briefly as they give it. The client takes no typed text (item 3). Taken after the result, a reason is hindsight. A reason is what tells a misread view from a misread rule when an item is missed ([Review/PaperTests/BreachPrediction.md](Review/PaperTests/BreachPrediction.md), *Scoring*), so a missing one is worth a moment's wait.

## The logs

- **Where:** `Logs\BreachLab-<participant>-<yyyymmdd-hhmmss>.csv` beside `Earthrise.exe`, the time in UTC. One a run; a run that is restarted starts a new log, and only a complete one counts.
- **What:** comment lines naming the participant, the build, the server data, the item set, the display and the UI scale; then a row for each command in free fire and a row for each item: its N, the key, the answer, whether it was right, the seconds to answer, which views were on at the answer, the camera's turns and zoom notches while the question was up, and the live and headless state hashes and whether they matched (MvpPlan-M3.md, *The lab's log*).
- **After the session:** commit the six logs to `Design/Review/BreachLab/`, with the reasons as `Reasons.md` beside them (item 13; M3-G3).

## Scoring

```
python3 Tools/BreachLab/ScoreBreachLab.py Design/Review/BreachLab
```

It checks that every item's live hash matched its headless one, that every log answers P0 and L01 to L16 once, and that the keys agree across the logs and split 8 open and 8 shut; a log that fails a check is named, and nothing is scored. It then prints each participant's accuracy over L01 to L16, P0 left out, the median over the six, and PASS or FAIL against 0.65 (*Hypotheses*). At sixteen items 0.65 needs 11 right; with six participants the median is the mean of the third and fourth best, so it passes when those two add up to 21 or more. `--json` prints the same as JSON for the report (M3-G3).
