# Earthrise.World — the MVP's HUD

**Status:** Decided by the owner, 2026-10-31, except where marked Proposed. This is the HUD for the Siege and Hold trials. It comes from the owner's HUD package, kept unchanged in [Reference/HudPrototype/](Reference/HudPrototype/). Its live prototype is `reference/MVP HUD Prototype.dc.html` there, which needs an internet connection. The package was drawn before the owner decided on squads and on platforms ([ADR-007](ADR/ADR-007-platforms-and-input.md)), so this document adapts it. Each change from the package is listed in *What changed from the package*.

[GameDesign.md](GameDesign.md) and [GameDescription.md](GameDescription.md) govern rules, and where this document differs from them they win. [RendererLook.md](RendererLook.md) governs the world's look, and [AGENTS.md](../AGENTS.md) governs code. Read the prototype for behaviour; do not port its code.

## 1. Scope

The HUD covers what [MvpPlan.md](MvpPlan.md) lists under *Readability* and M4's interface:
- selection, control groups and the command card;
- the minimap;
- alerts;
- the attack cursor, the repair lock, and the placement and dig readouts.

The integrity, collapse, passability and fog views are **world overlays**, drawn in the world, not in the HUD. The HUD toggles them (§7) and links to them through alerts and readouts.

**The MVP builds layout A with mouse and keyboard (Decided).** Touch and layout B follow a positive MVP verdict (ADR-007). On a Surface, the MVP is played with its keyboard cover and trackpad.

The prototype has one tab for each situation it covers:

| # | Situation | Player | What it shows |
|---|---|---|---|
| 1 | Siege: bombard | Attacker | Trebuchet fire, the attack cursor, the stone each shot costs |
| 2 | Siege: storm | Attacker | Selecting the army, the storm order, the clock at the keep's foot |
| 3 | Hold: quarry and place | Defender | Digs, placing templates, the ground readout |
| 4 | Hold: defend a wave | Defender | Tower garrison, the repair lock, the collapse alert |
| 5 | Siege control build | Attacker | As tab 1, with health-bar walls |

**The control builds** differ from the voxel builds only where the control's rules differ (GameDesign.md, *MVP*); everything else is identical, and a test asserts it:
- **Siege control:** health bars over wall sections, and HEALTH in place of PLATFORM on towers.
- **Hold control (Proposed):** laborers GATHER from resource nodes in place of the six designations, and the dig readouts give way to the node's remaining amount.

## 2. Units, scale and type

- **Logical pixels.** The HUD is laid out in logical pixels, multiplied by a UI scale of 1, 1.5 or 2. No other factor is used.
- **The scale follows the display's DPI (Decided).** Windows' display scaling picks the step: 100–124% gives 1, 125–174% gives 1.5, and 175% and above gives 2 (the bands are Proposed). A setting overrides it.
- **Type.** There is one face, Earthrise Mason (§9), in two sizes:
  - **labels and values** at 20 logical px, 2 screen px per font pixel at 1×, with a line height of 24;
  - **names, icons and big numbers** at 30 logical px, 3 screen px per font pixel at 1×, with a line height of 30.
- **Glyphs always land on whole pixels.** At 1.5×, labels draw at 3 screen px per font pixel and names at **4**, rounded down from 4.5, so names stay larger than labels (Decided). At 2×, the two sizes draw at 4 and 6.
- **Colours** are sRGB. The interface pass converts them correctly for whatever back-buffer format the renderer picks.

| Token | Value | Used for |
|---|---|---|
| panel | rgba(21,19,16,0.92) | All panels |
| panel-deep | #1f1c18 / #2a2621 | Inner wells, buttons |
| line | #3a342c | 2 px borders of panels and buttons |
| text | #f3ede2 | Primary text |
| dim | #a69c8a | Labels, hints |
| sub | #d8d0c2 | Secondary values |
| accent | #e8b86a | Selection, hotkeys, active things, objectives |
| active-bg | #4a3d22 | A pressed or active button |
| bad | #d2584a | Enemy, urgent, out of range |
| good | #9cc26a | Health, in range, ground that cannot be undermined |
| off | #6a6258 | Disabled text |
| team-red / team-blue | #e0503f / #6aa0ff | Units on the minimap |

- **Hit targets** are at least 44 logical px, so the same sizes serve touch later.
- **Colour is never the only signal.** Every state colour is also carried by text (IN RANGE, ENEMY, LOCKED). Colour-blind palettes are deferred (Decided, by the owner in the package).

## 3. Layout A: the deck

All sizes are logical px. The deck is fixed to the bottom of the screen.

| Element | Anchor | Size | Notes |
|---|---|---|---|
| Top bar | Top, full width | Height 44 | Resources on the left, the objective and its progress in the centre, camera buttons ↺ ↻ − + on the right (36 × 36) |
| Mode banner | Top 56, centred | Fits its text | Shown while a targeting mode is on, with CANCEL · ESC (44 high) |
| Alerts | Left 8, top 110 | Up to 440 wide, up to 3 | Newest first, with JUMP (44 high) when the alert has a place |
| Control groups | Left 212, bottom 212 | 5 × (120 × 40), plus SET (64 × 40) | A strip above the deck |
| Tooltip | Right 8, bottom 212 | 400 wide | Above the command card |
| Readout | Right 416, bottom 212 | 340 wide | Placement and dig readouts |
| **Deck** | Bottom, full width | Height 204, padding 8, gap 8 | Three columns |
| · Minimap | Deck, left | 188 × 188 | §6 |
| · Selection | Deck, centre | Takes the rest, at least 340 | §5; scrolls if its content overflows |
| · Command card | Deck, right | 4 × 3 cells, each 100–112 × 60, gap 4 | §7 |

**The HUD covers about a third of the screen's height** with the deck and top bar together. The game's central bet is reading breaches, so two rules follow (Proposed):
- **The camera frames the uncovered area.** Its default look-at point, and where JUMP and control-group centring put things, is the centre of the area above the deck and below the top bar, not the centre of the screen.
- **M4 measures it.** The M5 playtest logs whether players lose breaches under the deck. If they do, the deck gets slimmer before the verdict.

## 4. Layout B: corners, for touch (after the verdict)

The package's touch-first layout puts the minimap and camera buttons top right, the selection card bottom left, and a 3 × 2 command grid of 100 × 84 cells bottom right, with MORE and BACK paging. Both layouts read the same HUD model (§10). By default the layout follows the device used last. Layout B, its gestures and the touch column of the input table are the starting design for ADR-007's touch scheme. The package's HUD-SPEC.md §4 and §8 hold them.

## 5. Selection panel

**The player selects squads** (GameDesign.md, 2026-10-31). Laborers are the exception: they are not in squads, and clicking one selects that laborer. Clicking any other figure selects its squad. The click is tested against the squad's footprint on the ground, not against a figure's voxels: a figure is 6–7 px wide at the default zoom (Proposed).

- **Nothing selected:** NOTHING SELECTED, and three hint lines: CLICK A SQUAD, DRAG TO BOX-SELECT, 1–9 GROUPS. Each fits the panel's 340 px minimum.
- **One squad, laborer, engine or structure:**
  - a 96 × 96 portrait: a render of the figure's 0.5 m model, or the icon glyph for structures;
  - the name at 30 px and the owner tag on one line: YOURS in accent, ENEMY in bad;
  - for a squad, its figures as **n / 6** (or n / 3) and the figures' average hit points as a bar of 120 × 10 with "hp / max", shown only for things with hit points; a lone laborer or engine shows its own;
  - **up to three stat rows**, each `KEY value` on one line, at most about 220 px at 20 px, which is about 17 characters with the key.
- **Several:**
  - the title **N SQUADS · TAB CYCLES TYPE**;
  - one tile per type, 80 × 84, with a portrait, the squad count in accent (the count of laborers for laborers), the type's figure total, its average hit-point bar and a short label;
  - the **active type** has an accent border and drives the command card. Clicking a tile or pressing Tab moves it on.
- **The order of types:** trebuchet, ram, tower, gatehouse, keep, wall, men-at-arms, spearmen, archers, outriders, sappers, laborers.
- **Short labels:** TREB, RAM, TOWER, GATE, KEEP, WALL, ARMS, SPEAR, ARCHER, RIDER, SAPPER, LABOR.

| Kind | Stat rows (KEY value) |
|---|---|
| Laborers | ROLE Build, dig · ARMOR None |
| Spearmen | STRONG Horse · WEAK Archers, arms |
| Archers | HEIGHT +1 band, +15% · WEAK Horse, arms |
| Outriders | STRONG Bows, engines · WEAK Spear, arms |
| Men-at-arms | STRONG Spear, bows · ROLE Storm breach |
| Sappers | DIGS 2× trench · ARMOR Pierce |
| Ram | TARGET Gates, timber · ARMOR Arrows |
| Trebuchet | STATE Deployed, Packing…, Packed or Deploying… · RANGE min–max m · SHOT n m³ |
| Tower | GARRISON n / 6 · PLATFORM 100% (control: HEALTH hp / max) · REPAIR Ready, or Locked · n s |
| Gatehouse | GARRISON n / 6 · GATE 100% · REPAIR Ready, or Locked · n s |
| Keep | TOWER 100% · GARRISON n / 6 · TO WIN Hold foot 2:00 |
| Wall section | NAME W5 · OPEN 40% · REPAIR Ready, or Locked · n s |

- **While a squad moves,** its second row reads ORDER Moving.
- **Figures and values are Simulation's.** The package's hit points (laborer 25, spearman 45, archer 30, men-at-arms 70, sapper 40, trebuchet 150), the trebuchet's 12–70 m range, a shot's 6 m³, and a garrison of 6 are placeholders. A shot's stone price is an open tuning value (GameDesign.md, F-13). The wall's OPEN reading is the integrity view's (GameDescription.md, *Readability*).

## 6. Minimap

- **North-up.** It does not turn with the camera; the camera's quad shows which way the view faces.
- **It shows only what the player knows.** The base is the client's last-seen land, and units appear only where the player can see them (GameDescription.md, fog of war). The client has nothing else to draw from (ADR-002, ADR-003), and the minimap must not ask for more.
- **The base layer** is the top voxel's palette colour, shaded by height. It is built at the minimap's drawn size by reducing the 1 m columns, rather than scaling a 1 m texture to a size that does not divide it. It is rebuilt only when the world changes (Proposed).
- **The overlay,** redrawn every few frames:
  - squads as 3 px dots and structures as 7 px dots, in team colours, selected ones in #ffe2a8;
  - designations and blueprints as 1 px outlines;
  - the camera's view quad, the screen corners projected to the ground, as a 2 px polygon in the text colour;
  - alert pings, as pulsing squares in accent, or in bad when urgent.
- **Input:** press or drag moves the camera there, and right-click orders the selection to that point. The minimap keeps clicks from reaching the world.

## 7. Command card and keys

A 4 × 3 grid. Each cell shows:
- its hotkey, top left, in accent;
- an icon glyph at 30 px in the centre;
- its label at 20 px along the bottom.

**A label fits its cell.** A cell is 100–112 logical px wide, and a label is at most 90 logical px at 20 px, which every label below meets. A test measures every label with the font's metrics. The package's 72 px cells were too narrow for GARRISON and PALISADE, which are 90.

| State | Look |
|---|---|
| Active (its targeting mode is on) | Accent border, active-bg |
| Disabled | Off text, does nothing |
| Empty slot | Very dim, does nothing |

Hovering a cell shows the tooltip: hotkey, label and a one-line rule.

**Cards,** in slot order 1–12:

| Kind | Slots |
|---|---|
| Spearmen, archers, outriders, men-at-arms | MOVE, STOP, HOLD, ATTACK, STORM (attacker only), SPLIT, MERGE |
| Sappers | MOVE, STOP, HOLD, —, —, SPLIT, MERGE, TRENCH |
| Laborers, page 1 | MOVE, STOP, —, —, QUARRY, PIT, DITCH, TRENCH, FELL, CLEAR, REPAIR, BUILD |
| Laborers, page 2 (BUILD) | WALL, TOWER, GATE, PALISADE, PAL.GATE, YARD, CAMP, DEEP, —, —, —, BACK |
| Ram | MOVE, STOP, ATTACK |
| Trebuchet | FIRE, STOP, —, —, PACK or DEPLOY |
| Tower, gatehouse, keep | GARRISON, EJECT, —, —, REPAIR |
| Wall section | —, —, —, —, REPAIR |

- **GATE** places a gatehouse (T5), and **PAL.GATE** a palisade gate (T7). There is no hall or keep to place: the Hold trial starts with both (GameDesign.md, 2026-10-31).
- **DEEP** toggles a deep foundation for the next placement. The readout then shows DEEP FDN and its cost (GameDescription.md, *Undermining*).
- **SPLIT** detaches one figure from the squad as a squad of its own.
- **MERGE** combines the selected squads of the active type up to full size, six on foot or three mounted; any figures left over stay a squad. It is how a split figure rejoins (GameDesign.md, 2026-10-31).

**When a command is disabled:**
- FIRE: no deployed trebuchet is selected, or the stockpile is below one shot's stone.
- PACK: the trebuchet is already packing or deploying.
- EJECT: the garrison is empty.
- GARRISON: the building is full.
- SPLIT: the squad has one figure.
- MERGE: fewer than two squads of the active type are selected, or all of them are full.
- A template: there is not enough stone or timber.

**Hotkeys are mnemonic, and there is one scheme (Decided).** The positional grid is not built in the MVP.
- **On the cards:** MOVE M, STOP X, HOLD H, ATTACK T, STORM R, SPLIT P, MERGE G, FIRE F, PACK P, GARRISON G, EJECT E, REPAIR R, QUARRY Q, PIT P, DITCH I, TRENCH T, FELL F, CLEAR L, BUILD B.
- **On the build page:** WALL L, TOWER T, GATE G, PALISADE P, PAL.GATE E, YARD Y, CAMP C, DEEP F, BACK B.
- **Letters are unique within a card.** W, A, S and D are never command keys, because they pan.
- **Global keys:**

| Key | Does |
|---|---|
| W A S D, arrows; middle-drag | Pan |
| Wheel, + − | Zoom |
| [ ] or PageUp PageDown | Rotate 90° |
| 1–9; twice | Select a control group; centre on it |
| Ctrl + 1–9 | Assign a control group |
| Tab | Cycle the active type |
| Esc | Cancel the mode, or deselect |
| Space | Jump to the newest alert |
| F1 | Integrity view, on by default |
| F2 | Passability view |

## 8. Input with mouse and keyboard

| Action | Mouse and keyboard |
|---|---|
| Select | Left-click a squad |
| Add to or remove from the selection | Shift + click, or Ctrl + click |
| Box-select | Left-drag on the ground |
| Select every squad of a type on screen | Double-click |
| Order: move, attack, fire | Right-click |
| Assign a control group | Ctrl + 1–9, or Ctrl + click a slot |

**Targeting modes** (MOVE, ATTACK, FIRE, REPAIR, the digs and placement):
- show the mode banner and a crosshair;
- end after one action, or stay open while Shift is held, for repeated placement.

**The attack cursor** is a ring with a label that follows the pointer. It has the four readings GameDescription.md asks for, for every ranged attack, the trebuchet included:
- **IN RANGE** · d m, plus a shot's stone for an engine (good);
- **BLOCKED** · d m: in range, but the shot's path meets something solid first, from the same path the shot would take (bad);
- **TOO CLOSE** · d m (bad);
- **OUT OF RANGE** · d m (bad).

**Hover and ADR-007.** Every reading the MVP shows is in a panel, or follows the pointer while a mode is on. When the touch scheme is designed, the tooltip and the attack cursor get press-and-hold equivalents; nothing else depends on hover.

## 9. Font: Earthrise Mason

- **Drawn for the project,** so it meets the MVP's rule of own content only (MvpPlan.md, input 3).
- **Metrics:**
  - caps and digits are 5 × 7, lowercase has a 5-row x-height and 2-row descenders;
  - the cell is 10 rows: one above the caps and two below the baseline;
  - a glyph advances by its width plus 1.
- **Coverage:** 127 glyphs:
  - 87 printable ASCII characters (it lacks `$ @ \ ^ ` { } ~`);
  - 15 symbols: · × ² ³ ° ← → ↑ ↓ ↺ ↻ … – — ’;
  - 25 command icons of 9 × 9 at U+E000–U+E018.
  
  A test checks that every string the HUD can draw has its glyphs (Proposed).
- **The source is `GameData/Client/Fonts/EarthriseMason-glyphs.json`.** Each glyph's `rows` are its bitmap as text, and the atlases beside it are previews. The package's `earthrise-font.js` and `.otf` stay in Reference/HudPrototype/ as its own editing tools. Its JavaScript and its JSON agree glyph for glyph, and its three atlases match the rows pixel for pixel; both were checked on import.
- **It reaches the game the way voxel content does:** a tool under `Tools/` bakes the rows into the `Content` library, and nothing is loaded at runtime ([ADR-005](ADR/ADR-005-content-formats.md)). The text ADR in M3 settles the baked form.
- **Drawing:** the renderer draws glyphs only at whole multiples, 2 to 6 screen px per font pixel, with no filtering.

## 10. The HUD model

`Client` builds the model from `Protocol` state and never includes `Simulation` (MvpPlan.md, *Solution layout*):

```
resources[]         { key, value, warn }   // Siege: STONE m³, ARMY, TIME · Hold: STONE m³, TIMBER m³, LABOR n · idle, WAVE n / N
objective           { title, detail, progress? 0..1 }    // HOLD THE KEEP'S FOOT, 0:42 / 2:00
selection           { squadIds[], activeKind }
controlGroups[1..9] { squadIds[] }         // held by the client, saved with the trial
commandCard         { kind, page, slots[12]: { command, enabled, active } }
mode                { command, banner }    // null when none
alerts[]            { text, sub, location?, urgent, expiresAt? }
readout             { title, rows[{ key, value }], warn }
minimap             { baseDirty, squadDots[], viewQuad, pings[] }
```

**Readouts that need the resolver ask it.** "Can be undermined", the slump preview, the trench's target and collapse warnings come from `Simulation` through `Protocol` queries ([ADR-009](ADR/ADR-009-queries-across-the-server-boundary.md); [ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 5). Plain sums may come from snapshots.

**The placement readout:**
- **SOIL:** the mean depth of earth under the footprint;
- **BEDROCK:** the depth to bedrock;
- **GROUND:** "On rock · cannot be undermined" in good, or "On earth · can be undermined" in bad. This is the resolver's answer, not a soil-depth threshold: a wall can be undermined wherever it stands on earth, and only bedrock or a deep foundation stops it (GameDescription.md, *Undermining*);
- **CUT/FILL:** m³ to level the footprint;
- **COST:** m³;
- **DEEP FDN:** the footprint times the depth to bedrock, in m³, when DEEP is on.

**The dig readouts:**
- AREA, DEPTH and CREW;
- **YIELD** counts only the building stone the player can see (Decided). When the dig reaches rock the player has not seen, it adds MORE MAY LIE BELOW. Where stone lies is kept on the server ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)), and a readout must not reveal it. When nothing visible yields stone, it reads "None seen" in bad.
- A ditch adds its RAMPART volume and what it STOPS;
- a trench adds its TARGET, "Undermines W5", from the resolver;
- felling adds its timber, and clearance its rubble and the stone it returns, half the volume of rubble of building stone.

**The repair lock** shows on a section, tower or gatehouse as "Locked · n s" for ten seconds after a hit.

**Section names** such as W5 belong to the scenario, not to a template: T3 is one section. The Siege castle's scenario names each placed section, and saves keep the name with the structure (ADR-005, decision 10).

**The MVP's alerts:**

| Alert | When | Urgent |
|---|---|---|
| W5 WILL FALL IN n S, then W5 HAS FALLEN | The resolver warns of a collapse | Yes |
| SAPPERS AT … | Enemy sappers are seen | No |
| BREACH OPEN AT … | Passability opens a route | No |
| HIT W5 · REPAIR LOCKED 10 S | Your shot lands on a section | No |
| NOT ENOUGH STONE, or NOT ENOUGH TIMBER | A command is refused | No |
| … DESIGNATED, or … PLACED | Confirmation | No |

## 11. Building it

- **M3, the breach lab:**
  - the font tool and the text renderer;
  - the top bar, the mode banner and the camera buttons;
  - the collapse alert;
  - the attack cursor for FIRE;
  - F1 and F2;
  - the prediction prompts.
- **M4, the Siege trial:** the rest of layout A, built from the HUD model, and the control build's differences.
- **M6, the Hold trial:** the laborer cards, the placement and dig readouts, and the Hold control's GATHER.
- **Done when** layout A renders with no overlap and no clipped text at 1280 × 720 and 1920 × 1080, and at the Surface's own resolution and display scaling, at each scale step. Each command logs its source (hotkey or click) and time for the playtests.

## What changed from the package

- **Squads** are selected, tiled and grouped in place of single units, laborers excepted, with SPLIT and MERGE (§5, §7, §10).
- **Touch and layout B** wait for the verdict (§1, §4); M4 builds layout A only.
- **One hotkey scheme,** mnemonic; the grid scheme and its arrows-only panning are not built (§7).
- **The UI scale follows DPI,** and names draw at 4 px at 1.5× (§2).
- **YIELD counts visible stone only** (§10).
- **Added by this integration, and confirmed by the owner:**
  - **cards:** cards for outriders, the ram, the gatehouse, the keep's garrison and wall sections; PAL.GATE, DEEP, SPLIT and MERGE;
  - **sizes:** cells 100–112 wide, so labels fit;
  - **readings:** BLOCKED for every ranged attack, the trebuchet included, as GameDescription.md requires; the archer's row, which the package gave as "+15% per 3 m" of range when height gives one band of range and +15% damage; GROUND from the resolver in place of the package's 1.2 m soil rule;
  - **where things live:** section names in the scenario rather than the template; F1 and F2 for the integrity and passability views.
- **Still Proposed:** the DPI bands; selecting by a squad's footprint; the deck's coverage and camera framing; the minimap built at its drawn size; the glyph coverage test; the Hold control's GATHER.
- **The font's source** is the JSON in `GameData/Client/Fonts/`, not the package's JavaScript (§9).

## Open

- The real figures behind the placeholders in §5.
- The touch scheme: its gestures, layout B, and press-and-hold for the tooltip and the attack cursor (ADR-007).
