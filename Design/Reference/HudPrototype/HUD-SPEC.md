# HUD spec: Earthrise.World MVP (Siege and Hold trials)

This is the source of truth for the MVP HUD's **layout, content, states and input**. The reference implementation is `reference/MVP HUD Prototype.dc.html`. Open it in a browser with the other `reference/` files beside it; it needs an internet connection for three.js and opentype.js. The prototype is a design tool, so read it for behaviour, but do not port its code.

Where this spec and `Design/GameDescription.md`, `Design/GameDesign.md` or `Design/MvpPlan.md` disagree, those govern. `AGENTS.md` governs code.

## 1. Scope

The HUD covers what `MvpPlan.md › Scope › In › Readability` and `M4 › interface` list: selection, control groups, command card, minimap, alerts, attack cursor, repair lock, placement readout, and dig (designation) readouts.

The integrity, collapse, passability and fog overlays are **world overlays**, not HUD, and are out of this package. The HUD only links to them through alerts and readouts.

Covered situations, one per prototype tab:

| # | Situation | Player | Purpose |
|---|---|---|---|
| 1 | Siege: bombard | attacker (red) | Trebuchet fire, attack cursor, stone cost |
| 2 | Siege: storm | attacker | Army selection, storm order, keep-foot clock |
| 3 | Hold: quarry and place | defender (blue) | Digs, template placement, ground readout |
| 4 | Hold: defend a wave | defender | Tower garrison, repair lock, collapse alert |
| 5 | Control build | attacker | Same as 1, with health-bar walls for the A/B playtest (GameDesign › control) |

The control build differs **only** in the wall readout: health bars over sections, and HEALTH in place of PLATFORM for towers. Every other HUD element is identical, as the control rules require.

## 2. Units, scale, type

- **Logical pixels.** The HUD is laid out in logical px and scaled by an **integer-friendly UI scale**:
  - 1.0 below 1800 × 1000;
  - 1.5 at 1800 × 1000 and above (1080p);
  - 2.0 at 2400 × 1300 and above (1440p and 4K).
  This keeps the bitmap font on whole pixels. Never scale by a non-listed factor.
- **Type:** only **Earthrise Mason** (`font/`), at **20 px** (2 screen px per font pixel at 1.0×) for all labels and values, and **30 px** for names, icons and big numbers.
  - Line height is 24 px for 20 px text and 30 px for 30 px text.
  - At 1.5× these give 3 and 4.5 px. Snap 30 px text to 3 px pixels by drawing the glyph at a 3× atlas, not by filtering.
- **Icons:** command icons are glyphs U+E000–E018 in the same font (§9), drawn at 30 px.
- **Colours** (sRGB):

| Token | Hex | Use |
|---|---|---|
| panel | rgba(21,19,16,.92) | all panels |
| panel-deep | #1f1c18 / #2a2621 | inner wells, buttons |
| line | #3a342c | 2 px panel and button borders |
| text | #f3ede2 | primary text |
| dim | #a69c8a | labels, hints |
| sub | #d8d0c2 | secondary values |
| accent | #e8b86a | selection, hotkeys, active, objectives |
| active-bg | #4a3d22 | pressed or active button |
| bad | #d2584a | enemy, urgent, out of range |
| good | #9cc26a | health, in range, strong ground |
| off | #6a6258 | disabled text |
| team-red / team-blue (minimap) | #e0503f / #6aa0ff | units on the minimap |

- **Touch targets** are at least 44 logical px. Command buttons are at least 72 × 60.

## 3. Layout A: "deck" (default; PC first, good on Surface)

All values are logical px; the bottom deck is fixed.

| Element | Anchor | Size | Notes |
|---|---|---|---|
| Top bar | top, full width | h 44 | Resources (left), objective + progress (centre), camera buttons ↺ ↻ − + (right, 36 × 36) |
| Mode banner | top 56, centred | auto | Shown while a targeting mode is active, with a CANCEL · ESC button (44 h) |
| Alerts | left 8, top 110 | ≤ 440 w, up to 3 | Newest first; JUMP button (44 h) when it has a location |
| Control groups | left 212, bottom 212 | 5 × (120 × 40) + SET (64 × 40) | Strip above the deck |
| Tooltip | right 8, bottom 212 | w 400 | Above the command card |
| Readout | right 416, bottom 212 | w 340 | Placement and dig readouts |
| **Deck** | bottom, full width | h 204, pad 8, gap 8 | Three columns |
| · Minimap | deck left | 188 × 188 | §6 |
| · Selection | deck centre | flex, **min-w 340** | §5; scrolls if content overflows |
| · Command card | deck right | 4 × 3 grid, cells **72–96** × 60, gap 4 | Cells shrink before the selection panel does |

## 4. Layout B: "corners" (touch first: tablets)

| Element | Anchor | Size | Notes |
|---|---|---|---|
| Resources + objective | top-left 12 | panel, max-w 100% − 260 | Two rows |
| Minimap | top-right 12 | 200 × 200 in a panel | Camera buttons below it, 48 × 48 |
| Mode banner | top 12, centred | auto | |
| Alerts | top 70, centred | ≤ 440 w | |
| Control groups | left 12, top 130, **bottom 186** | 132 × 52 each + SET | Column that wraps into a second column on short screens; never overlaps the selection card |
| Selection card | bottom-left 12 | w min(480, 100% − 360), min-h 150 | Same content as A |
| Readout | left 12, bottom 176 | w 340 | |
| Tooltip | right 12, bottom 196 | w 316 | |
| Command grid | bottom-right 12 | **3 × 2** cells 100 × 84, gap 6 | Shows the first 5 commands + MORE when a card has more than 6, with BACK on page 2. Hotkeys are unchanged |

Both layouts read the same HUD model (§10). A setting chooses between them, defaulting to A on mouse and B when the last input was touch.

## 5. Selection panel

- **None:** "NOTHING SELECTED" plus two hint lines: "TAP OR CLICK A UNIT · DRAG TO BOX-SELECT" and "TOUCH: HOLD, THEN DRAG · 1–9 GROUPS".
- **Single:**
  - a portrait, 96 × 96: a render of the unit's 0.5 m model, or the icon glyph for structures;
  - the name at 30 px and the owner tag (YOURS in accent, ENEMY in bad) on **one line**;
  - an HP bar (120 × 10, good) plus "hp / max" (never wraps), shown only for things that have HP;
  - **up to 3 stat rows**, each `KEY value` on one line, **at most about 220 px** at 20 px. That is about 17 characters, key included, so values are written short (table below).
- **Multi:**
  - a title, "N SELECTED · TAB CYCLES TYPE";
  - one tile per unit type (80 × 84 in A, 72 × 76 in B), each with a portrait, a count (accent), the type's average HP bar and a short label (LABOR, SPEAR, ARCHER, ARMS, SAPPER, TREB, TOWER, KEEP);
  - the **active type** has an accent border and drives the command card. Tapping a tile or pressing Tab cycles it.
- **Order of types:** trebuchet, tower, keep, men-at-arms, spearmen, archers, sappers, laborers.

| Kind | Rows (KEY value) |
|---|---|
| Laborer | ROLE Builds, digs · ARMOR None |
| Spearman | STRONG Horse, knights · WEAK Archers, arms |
| Archer | RANGE +15% per 3 m · WEAK Horse, arms |
| Man-at-arms | STRONG Spear, bows · ROLE Storm breaches |
| Sapper | DIGS 2× trench speed · ARMOR Pierce |
| Trebuchet | STATE Deployed / Packing… / Packed / Deploying… · RANGE 12–70 m · SHOTS n × 6 m³ |
| Tower | GARRISON n / 6 · PLATFORM 100% (control build: HEALTH hp / max) · REPAIR Ready / Locked · n s |
| Keep | TOWER 100% · TO WIN Hold foot 2:00 |

While a unit is moving, its second row becomes ORDER Moving.

The prototype's HP values (laborer 25, spearman 45, archer 30, men-at-arms 70, sapper 40, trebuchet 150) are placeholders. The real stats come from Simulation.

## 6. Minimap

- **North-up.** It does **not** rotate with the camera; the camera quad shows the orientation.
- **Base layer:** one pixel per 1 m world column, coloured by the top voxel's palette colour and shaded by height. It is redrawn only on world edits. Draw it pixelated.
- **Overlay** (every few frames):
  - units as 3 px dots, structures as 7 px dots, in team colours; selected ones in #ffe2a8;
  - designations and blueprints as 1 px outlines;
  - the camera's view quad (screen corners projected to the ground) as a 2 px text-colour polygon;
  - alert pings as pulsing squares (accent, or bad when urgent).
- **Input:**
  - press or drag to move the camera there;
  - right-click to order the selection to that point;
  - it stops pointer events from reaching the world.

## 7. Command card

A 4 × 3 grid. Each cell has:
- its hotkey, top-left in accent;
- an icon glyph at 30 px in the centre;
- a label at 20 px along the bottom, at most 8 characters.

| State | Look |
|---|---|
| Active (its targeting mode is on) | accent border, active-bg |
| Disabled | off text, no action |
| Empty slot | very dim, does nothing |

Hover (mouse) or press (touch) shows the tooltip: hotkey, label and a one-line rule.

**Cards**, in slot order 1–12 (Q W E R / A S D F / Z X C V when grid hotkeys are on):

| Kind | Slots |
|---|---|
| Infantry (spear, archer, men-at-arms) | MOVE, STOP, HOLD, ATTACK, STORM (attacker only), — … |
| Sapper | MOVE, STOP, HOLD, —, —, —, —, TRENCH |
| Laborer, page 1 | MOVE, STOP, —, —, QUARRY, PIT, DITCH, TRENCH, FELL, CLEAR, REPAIR, BUILD |
| Laborer, page 2 (BUILD) | WALL, TOWER, GATE, PALISADE, YARD, CAMP, —, —, —, —, —, BACK |
| Trebuchet | FIRE, STOP, —, —, PACK/DEPLOY |
| Tower | GARRISON, EJECT, —, —, REPAIR |
| Keep | (empty) |

**Disabled rules:**
- FIRE: no deployed trebuchet is selected, or stone is under 6 m³.
- PACK: the trebuchet is already packing or deploying.
- EJECT: the garrison is empty.
- GARRISON: the tower is full (6).
- Templates: not enough stone or timber.

**Hotkeys** have two schemes, chosen in settings:

| Scheme | Commands | Camera pan |
|---|---|---|
| **Mnemonic (default)** | MOVE M, STOP X, HOLD H, ATTACK T, STORM R, FIRE F, PACK P, GARRISON G, EJECT E, REPAIR R, QUARRY Q, PIT P, DITCH I, TRENCH T, FELL F, CLEAR L, BUILD B · build page: WALL L, TOWER T, GATE G, PALISADE P, YARD Y, CAMP C, BACK B | **WASD** + arrow keys |
| Grid | slot position Q W E R / A S D F / Z X C V | arrow keys only |

Letters are unique within each card. W, A, S and D are never command keys in the mnemonic scheme.

## 8. Input mapping

| Action | Mouse + keyboard | Touch |
|---|---|---|
| Select | Left click a unit | Tap a unit |
| Add or remove from selection | Shift / Ctrl + click | (not in MVP) |
| Box-select | Left-drag on the ground | **Long-press (450 ms) then drag**. A ring with "DRAG TO SELECT" confirms the press |
| Select all of a type on screen | Double-click | Long-press and release without dragging |
| Order (move / attack / fire) | Right-click | Tap the ground with your own units selected |
| Pan | WASD / arrow keys, middle-drag | **One-finger drag** (default) |
| Zoom | Wheel, + / − | Pinch |
| Rotate 90° | [ ] or PageUp / PageDown, ↺ ↻ buttons | ↺ ↻ buttons |
| Control group select / centre | 1–9; press twice to centre | Tap the slot; tap twice to centre |
| Control group assign | Ctrl + 1–9, or Ctrl + click a slot | SET, then tap a slot |
| Cycle active type | Tab | Tap a type tile |
| Cancel mode, deselect | Esc | CANCEL in the mode banner |
| Jump to alert | Space | JUMP on the alert |

**Targeting modes** (MOVE, ATTACK, FIRE, REPAIR, digs, placement):
- show the mode banner and a crosshair cursor;
- **one action** completes the mode; Shift keeps it open, for repeated placement.

**Attack cursor** (FIRE): a ring plus a label that follows the pointer:
- IN RANGE · d m · 6 m³ (good);
- TOO CLOSE · d m (bad);
- OUT OF RANGE · d m (bad).

A shot in range spends 6 m³ immediately. The MVP's archers and towers should add a third state, **BLOCKED** (in range, line blocked by a wall), from the shot path. The prototype does not draw it.

## 9. Font: Earthrise Mason

- **Metrics:** a 5 × 7 cap height. Lowercase has a 5-row x-height and 2-row descenders; the cell is 10 rows (1 above caps, 2 below baseline); the advance is glyph width + 1.
- **Coverage:** 127 glyphs, ASCII printable plus · × ² ³ ° ← → ↑ ↓ ↺ ↻ … – — ’.
- **Icons:** 9 × 9, in U+E000–E018: move, attack, stop, hold, fire, pack, storm, garrison, eject, repair, quarry, pit, ditch, fell, clear, trench, build, tower, gate, palisade, yard, camp, back, trebuchet, keep.
- **Files in `font/`:**
  - `EarthriseMason.otf` for tools and mockups only. The engine uses the atlas, per MvpPlan's text decision.
  - `EarthriseMason-atlas-{1,2,3}x.png`: a 16-column grid of 10 × 10 cells, white on transparent.
  - `EarthriseMason-glyphs.json`: per glyph `{char, code, icon, x, y, w, top, advance, rows}`, plus `baselineRow: 7`. `rows` is the bitmap as strings, so the atlas tool can regenerate it.
  - `earthrise-font.js`: the source of truth for the bitmaps. Edit glyphs there and re-export.
- **Licence:** drawn for the project, so it satisfies MvpPlan input 3.

## 10. HUD model (what Client must expose; all values come from Protocol state)

```
resources[]        { key, value, warn }    // Siege: STONE m³, ARMY, TIME · Hold: STONE m³, TIMBER m³, LABOR n · idle, WAVE n / N
objective          { title, detail, progress? 0..1 }   // e.g. HOLD THE KEEP’S FOOT, 0:42 / 2:00
selection          { entities[], activeKind }
controlGroups[1..9]{ entityIds[] }         // client-side, saved per trial
commandCard        { kind, page, slots[12]: { command, enabled, active } }
mode               { command, banner }     // null when none
alerts[]           { text, sub, location?, urgent, expiresAt? }
readout            { title, rows[{key,value}], warn }
minimap            { baseDirty, unitDots[], viewQuad, pings[] }
```

- **The placement readout** comes from a Simulation query for the footprint:
  - SOIL: mean earth depth;
  - BEDROCK: depth to bedrock;
  - GROUND: "Thin soil on rock · strong" when soil is 1.2 m or less, otherwise "Deep earth · can be sapped";
  - CUT/FILL: m³ to level;
  - COST: m³;
  - DEEP FDN: footprint × depth to bedrock, in m³.

  "Can be sapped" and the slump preview **must ask the resolver** (ADR-004 decision 5), never a local guess. The prototype approximates them.
- **Dig readouts** show AREA, DEPTH, YIELD (building stone counted in the dug cells; "None · common rock" in red when zero) and CREW.
  - Ditch shows RAMPART volume and STOPS.
  - Trench shows TARGET ("Undermines W*n*"), resolved by the resolver.
  - Fell shows timber; Clear shows rubble and half its stone.
- **The repair lock** shows on a section or tower as "Locked · n s" for 10 s after a hit (GameDescription › repair).
- **Alerts in the MVP:**

| Alert | When | Urgent |
|---|---|---|
| *W5* WILL FALL IN *n* S, then *W5* HAS FALLEN | Collapse warning, from the resolver | Yes |
| SAPPERS AT … | Enemy sappers seen | No |
| BREACH OPEN AT … | Passability opens a route | No |
| HIT *Wn* · repair locked 10 s | Your shot lands on a section | No |
| NOT ENOUGH STONE / TIMBER | A command is refused | No |
| *X* DESIGNATED / PLACED | Confirmation | No |

  Wall sections are named W1… per wall line in the Siege castle template (a template tag), so alerts and readouts can name them.

## 11. Not decided here (owner to confirm)

1. The UI scale steps (1 / 1.5 / 2) and the thresholds that pick them.
2. Mnemonic letters (M, X, H, T…) as the default over the positional grid. The two conflicted in the brief, and the grid is kept as a setting.
3. Layout A as the default for mouse and B for touch, versus a single layout.
4. Placeholder unit HP values and stat wording (§5). They must be replaced by Simulation stats under the about 220 px limit.
5. Colour-blind-safe overlays are **deferred** (owner's choice). Every state colour here is also carried by text (IN RANGE, ENEMY, Locked), so nothing depends on colour alone.
