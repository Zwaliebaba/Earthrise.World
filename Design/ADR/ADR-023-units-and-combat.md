# ADR-023 — Units and combat: statistics as server data, damage as integers, arrows as physical shots

**Status:** Proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). It records how M4's units fight: where their statistics live, how damage is worked out, how arrows fly, and how engines, collapses and falls hurt units. The numbers are the owner's inputs (MvpPlan.md, M4, inputs 6 to 9), Proposed for tuning in M5. It binds nothing until the owner gives it the go-ahead (input 1); order M4-C1, which first implements it, marks it Accepted (AGENTS.md §6).

## Context

- **The design fixes the model** (GameDesign.md, *Where RTS logic meets destruction*; GameDescription.md, *Combat model*): stat-based combat with hit points, attack, armor, range, speed and bonuses against classes; three damage types, melee, pierce and crush, with armor against each; shots that travel through the voxel grid and stop at the first solid thing; a shot passes units other than its target; arrows fly flat, engines lob; elevation in steps of 3 m, up to two, each +15 % damage and one band of range for arrows, and the same lost shooting uphill; engines can target engines; a unit whose line is blocked moves to where it can shoot.
- **The counters** are two linked cycles (GameDescription.md, *Roster*); the MVP has the Hall-era cycle and men-at-arms (GameDesign.md, *MVP*).
- **The simulation holds no float** ([ADR-013](ADR-013-ticks-numbers-and-randomness.md), decision 3); positions are Q16.16; durations are ticks.
- **Server data is tuned without recompiling the client** ([ADR-008](ADR-008-server-and-client-game-data.md)); [ADR-016](ADR-016-server-data-format.md), decision 3, names unit statistics as a section M4 adds.
- **M2 left three amounts to M4:** the collapse's crush damage, a drop's crush damage (GameDesign.md, *M2's values*, item 8; [ADR-015](ADR-015-navigation.md), decision 7), and the ram's craters ([ADR-006](ADR-006-voxel-sizes.md), *Left open*).
- **The arrow is drawn as a line model** ([ADR-018](ADR-018-line-models-and-the-line-pass.md)); the simulation sees a point.

## Decision

1. **Unit statistics are a server-data section, `UNIT`,** baked from `GameData/Server/Units.json`, one record per `UnitKind`: its navigation class, figures a squad, hit points, armor against each damage type, its attack's type, amount, reload in ticks and range in voxels (0 for melee), its bonus against each kind, its vision in voxels, its speed, and, for sappers and laborers, its digging rate. The bake converts each exactly, as `RULE`'s values are (ADR-016, decision 4). Siege engines' bodies are kinds of their own, `Ram` and `Trebuchet`, so that one table serves every unit. **Each value has one owner:** movement speeds move from `RULE`'s class keys to `UNIT`'s kinds, `RULE` keeping `footSpeedMetersPerSecond` for the collapse's warning alone (GameDesign.md, *M2's values*, item 4); the trebuchet's reload and range stay `RULE`'s shot keys and the ram's cadence `RULE`'s `ramBlowSeconds`, and their `UNIT` rows carry 0 there.
2. **Damage is one integer formula.** A hit deals `max(1, floor((attack + bonus) × (100 + 15 × steps) ÷ 100) − armor)`, where `armor` is the target's against the attack's type and `steps` is the elevation, −2 to 2, for pierce alone and 0 otherwise. A unit at 0 hit points dies at the point its damage is applied and is removed at the end of that step, in unit-id order; a squad with no figures left is removed with it.
3. **Melee reaches the eight neighbouring cells.** In step 3, after movement, every figure whose reload is due and whose target, or the nearest enemy figure in a neighbouring cell, is there strikes it. The tick's strikes are all worked out from the state before any of them land, then applied together, so the order of ids decides nothing about who strikes first.
4. **Squads choose their own targets inside their orders.** A squad that is idle, holding, storming or ordered to attack engages enemies within its kind's acquisition range, Proposed at its vision; each figure takes the nearest enemy figure, ties to the lower id. A holding squad strikes but does not leave its cells. An ATTACK order names a squad, an engine or, for the ram, a structure; STORM moves to a point engaging what it meets.
5. **An arrow is a physical shot.** A ranged figure whose reload is due looses at its target if the target is within its range plus a 5 m band for each elevation step, and the straight line from its loosing point, 1.5 m above its cell or, for a garrison, the building's outer face at the garrison's stand height (ADR-024), to a point 1 m above the target's cell meets no solid voxel. The arrow flies that line at `arrowSpeedMetersPerSecond`, Proposed at 40 (2 m a tick), traced voxel by voxel through the grid as the trebuchet's shot is; it stops at the first solid voxel and does nothing, or reaches its end and hits its target if the target stands within one cell of the point it was aimed at, and otherwise falls harmlessly. It passes every other unit. Bolts are crossbowmen's, which the MVP leaves out.
6. **A ranged squad whose line is blocked moves.** Ordered to shoot a target it cannot see a line to, it walks towards the target and tries again each tick, which is "the nearest place it can shoot from" at the cost of no search beyond its squad's path.
7. **Engines hurt units, and engines.** A trebuchet's shot deals its crush damage to every unit standing in the cell above the struck voxel or in its eight neighbours, and to an engine whose footprint holds the struck voxel's column. Firing at an engine is firing at its cell (Hud.md §8's attack cursor reads it as any target).
8. **Collapses and falls deal crush.** A collapse deals `collapseCrushDamage` to each unit it strikes, once (ADR-015, M2-F6); a drop deals `dropCrushDamagePerMeter` for each metre beyond 3. Both are in `RULE`.
9. **The ram strikes structures, at contact.** A ram ordered at a structure drives to the nearest cell from which its front touches it, and strikes every `ramBlowSeconds`, at the lowest standing voxel of the structure in the column before its centre, then the columns either side, alternately. A blow is a crater by the contact column of GameDescription.md's table, on the paper tests' ladder: timber 4 voxels, earth 1, and stone 1 when the voxel lies within the bottom metre of the structure and none above it. Its roof is its pierce armor; it has no attack on units.
10. **Shots cost stone,** `shotStoneCubicMeters` from the firing player's stockpile, and a `FireEngine` is refused below it. A refused command comes back to its player in the state update (ADR-020, amended).

## Consequences for the code

- **`ServerData`** gains `UNIT`, and `RULE` gains the arrow's speed, the band, the crush amounts, the ram's blow and craters and the shot's price; `Tools/CheckAssets.py` checks `Units.json` (every kind once, every bonus naming a kind, every value converting exactly).
- **`Simulation`** gains `UnitStats`, `Damage`, melee and targeting in step 3, arrows in step 4 beside the trebuchet's shot, the ram's blows in step 4, and hit points, reload and target on each unit, saved and hashed. Engines move as a squad of one unit, their body, whose kind is `Ram` or `Trebuchet`, while the engine's own record keeps its reload, its state and its facing.
- **`Protocol`** gains the new commands and the views' new fields (MvpPlan-M4.md, *Contracts*).
- **Tests** pin the formula, a counter's time to kill on each pairing of the roster, an arrow stopped by a wall, an arrow from 11 m reaching 40 m and one from the ground 30 m, a ram's blows on T5's gate and a T3's foot, and a shot's splash.

## What this forecloses

- Float, randomness or wall time in combat: a hit is decided by the rule, never by a roll.
- Arrows that pass walls or strike units other than their target.
- Soldiers changing the land: only the ram, engines, sappers, laborers, collapses and fire remove voxels (GameDesign.md).
- Statistics compiled into the code.

## Left open

- **Every number,** for M5's playtest to tune. MvpPlan.md, M4, inputs 6 to 9, are first values.
- **Hit chance and spread.** An arrow hits a target within a cell of its aim point, always.
- **Area attacks beyond the trebuchet's splash,** the mangonel's scatter shot among them, which the Trials leave out.
- **Where on the arrow the shot's point sits** for drawing (ADR-018, *Left open*).
