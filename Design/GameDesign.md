# Earthrise.World — Game Design

**Status:** Draft, 2026-10-30; revised 2026-10-31, when the owner accepted the game description, and again the same day after the [design review](Review/GameDescription-2026-10-31.md), and on 2026-11-01 for M1's values and M2's. M1's renderer spike implements part of the MVP; the rest is not implemented. Each section says what kind of statement it holds: **Decided** records the owner's answers, **Proposed** is a recommendation awaiting the owner, **Open** is what nobody has decided yet. A proposal becomes a decision only when the owner accepts it, and this document is updated when that happens.

This document says what the game is. [GameDescription.md](GameDescription.md) describes the whole game at length, for designers reviewing it. The owner accepted it in full on 2026-10-31, so everything it states is Decided; this document records that and holds what is still Proposed or Open. The design review that followed the same day is recorded below, and its decisions are written into both documents. Where the two differ, this document governs. [AGENTS.md](../AGENTS.md) says how the code is written, and [`Design/ADR/`](ADR/) records the engineering decisions that follow from it.

## The game in one paragraph

Earthrise.World is a real-time strategy game about medieval lords on land made entirely of voxels. It plays like a classic historical RTS: workers gather, a settlement grows, technology advances, and armies built around counters fight. Every structure and every hillside is made of voxels that can be dug, quarried, built and knocked down, so resources are dug out of the land and walls are breached where they are hit. The game is played in shared online worlds that each last an eight-week season, in which lords race to excavate Earthstone from the Deep at the world's centre and take it from one another by siege. Single-player Trials and Skirmish against AI houses come first. The first playable version is single-player against AI factions on one generated map.

## Decided by the owner

### 2026-10-30

1. **Voxels exist for destruction.** Teardown is the reference for both the destruction and the look. How close the look can get from an RTS camera is what the renderer spike finds out (below). Since 2026-10-31 the reference for the look is [RendererLook.md](RendererLook.md), which comes from a visual prototype; Teardown remains the reference for destruction.
2. **The game logic is the classic historical RTS:** a worker economy, technology progression and counter-based unit combat. Age of Empires II is the reference for how those mechanics feel. It is not a source of content: factions, units, resources and technology are this game's own.
3. **The real goal is a shared, persistent online world.** It is not part of the MVP.
4. **The MVP has one human player.** AI factions fill the other seats.
5. **Voxel sizes and the camera's projection were to be settled by a renderer spike** that shows and measures them, not on paper. The render voxel size and the simulation voxel size may differ by an integer ratio. Superseded on 2026-10-31: the owner set the camera as orthographic at 34°, and both voxel sizes at 0.5 m, from a visual prototype ([RendererLook.md](RendererLook.md)). Later the same day the owner changed the sizes to a 1 m world and simulation with 0.5 m models ([ADR-006](ADR/ADR-006-voxel-sizes.md)). The spike now measures what they cost.
6. **The setting is medieval.**

### 2026-10-31: the game description

The owner accepted [GameDescription.md](GameDescription.md) in full. Its numbers are design targets to be tuned in the Trials and Skirmish, not measurements. The ADRs it summarises, [ADR-002](ADR/ADR-002-authoritative-simulation.md) to [ADR-004](ADR/ADR-004-rule-based-destruction.md), were accepted after the design review (below). Its main decisions:

1. **The product is the shared world.** Worlds are shared online seasons. Trials (Siege and Hold) and Skirmish against AI houses come first and lead to them. The first release ships one culture; Worlds open with three, each built around a material: Masons (stone), Wealdfolk (timber) and Delvers (earth and iron).
2. **The objective.** A match is won by breaking the enemy keep, or by killing every enemy unit. Since the design review, breaking a keep means bringing it out of working and holding its foot with soldiers for two minutes without a repair. A world is won by the banner whose lords hold the most Earthstone when the season ends; since the review, that counts Earthstone wherever it is held. Titles, heraldry and cosmetic keep styles carry from one season to the next; nothing that wins battles does.
3. **Seasons.** A world has 8–32 seats, at least half of them human when it opens, and AI houses in the rest, on land sized to its seats, up to about 2 km × 2 km (revised by the design review), for eight weeks: Settlement (weeks 1–2), Contest (weeks 3–6) and Reckoning (weeks 7–8). At the end it freezes and is archived, with a timelapse built from a daily snapshot.
4. **Siege windows and the steward.** Inside a lord's holding, nobody else can change anything except during a daily two-hour window the lord chooses inside the world's contest band. The window grows by thirty minutes for every 25 blocks of Earthstone the lord holds, up to twelve hours. Only a siege declared a day ahead can sack. Outposts are open to attack during the band, and armies and carts at any time. While a lord is offline, an AI steward works their standing orders, defends and repairs, and never attacks another lord. (Revised by the design review.)
5. **Pace.** In Worlds, gathering and new construction draw on a daily work allowance per holding (revised by the design review). Movement, combat, repair and siege fire run at full speed.
6. **The economy.** Four resources, provisions from fields, timber, stone and iron, plus Earthstone in Worlds. Stone and timber are counted in cubic metres, and a building costs exactly the volume it is made of. Every engine shot costs stone. Provisions are the one resource that is not dug, and fields compete with quarries for flat earth.
7. **Eras are built.** Hall, Keep and Citadel. Advancing means completing the keep, and then the citadel's great tower, in quarried stone.
8. **Armies.** Seven units in two linked counter cycles, plus rams, sappers, mantlets, mangonels, trebuchets and carts, as the description lists them. Combat is stat-based with melee, pierce and crush damage. Shots are physical, and elevation counts in steps of 3 m.
9. **What the game never has:** units on walls, ladders or siege towers; building outside templates; tunnels; gunpowder; flowing water; debris that affects play.
10. **The camera** is an isometric strategy view that rotates in 90° steps and zooms from single stones to a whole holding. Since 2026-10-31 it is orthographic, at 34° above the horizon (RendererLook.md).

### 2026-10-31: the design review

The [review](Review/GameDescription-2026-10-31.md) of the game description kept 62 of its 63 merged findings after verification, 12 of them as Blockers. The owner answered its open questions and asked for its other recommendations to be written in, so both documents now carry them. Its numbers are design targets, like the description's. Finding ids refer to the review.

1. **Hobby first.** Earthrise.World is a hobby project with one developer, and scope is sized for that. The commercial steps (a counted demand signal, a store, identity) are gates to pass, not assumptions. (F-52, F-55)
2. **The keep falls to storming.** A keep is broken only when it has stopped working and the attacker's soldiers have held its foot for two minutes. Engines can bring it down, but cannot win alone. A balance bound stays as a guard: an engine's range is less than tower reach plus the shortest distance from keep to curtain that the Siege castle and the Skirmish AI build. (F-01)
3. **Hold's waves are scored, and H1 is stricter.** See *Stages* and *Hypotheses* below. (F-45)
4. **Worlds pace is a daily work allowance.** Each holding has a daily budget of gathering and building. The lord spends it at full speed while online, and the steward spends what is left. It is sized so that a lord who plays most days reaches the Keep era in about a week and the Citadel about two weeks after that. Holding the Citadel back to week 3 keeps the era-timed culture traits alive into Contest. (F-27)
5. **Windows sit in a contest band, and sacks need a declared siege.** Each world sets a home time zone and an evening band, and every window starts inside it. A sack needs a siege declared a day ahead, so the banner can defend an absent lord. Outposts are open during the band. (F-33, F-34)
6. **Earthstone and sacking.** All Earthstone a lord owns counts and can be sacked, wherever it is. The window uses the most held at any moment of the previous day, capped at twelve hours. A sack needs the keep broken under a declared siege during the window and still down at its close. A third passes to whoever did the most damage, and a sacked holding cannot be sacked again for 48 hours, in place of the two-way truce. (F-31, F-32)
7. **Worlds open from a queue.** A world opens when lords have queued for at least half its seats. Seats run from 8 to 32, and the land is sized to them. Online work starts only after a counted demand signal (*Persistence*). One person holds one seat. (F-55, F-42)
8. **Two cuts now.** Titles are recorded in each world's archive, not by a central account service. The season timelapse is rendered from top-down daily images by a tool under `Tools/`, not by the game's renderer. AI houses that play the whole game, and Worlds that open with three cultures, stay; they head the cut order (*Build budget*). (The build budget and its cut order were removed on 2026-11-01, *Planning M2*.) (F-52, F-56, F-63)
9. **The review's other fixes, as recommended.** Each is written into the description:
   - **Holdings.** Holdings protect their ground, not just their structures. Designations go only in a lord's own holding, or on shared ground outside a margin from every boundary. Inside another's holding, during its window, an attacker may only trench and clear rubble. (F-30)
   - **Repair.** It works at a fixed rate per section, from the owner's side, and restores only voxels once built. A hit is an engine shot, a ram blow or a trench. A critical part's threshold is a share of its voxels. (F-03, F-40, F-05)
   - **The integrity view** reads the collapse rule, not thickness, and is on by default. (F-04)
   - **Gatehouses.** Their garrisons fire into their own passage with crush damage a ram's roof does not stop, and a gate's target time is about four minutes. (F-02)
   - **Loose material** never rests on top of a standing structure or near its top. (F-06)
   - **Earthworks.** Foot soldiers climb settled earth and stand on ramparts. Cavalry, carts, rams and engines cannot cross a ditch or a rampart. (F-07)
   - **Rubble** keeps the material it fell from. Only rubble of building stone yields stone. (F-17)
   - **Shot price.** A shot's stone is tuned so that a breach costs the attacker about half the defender's rebuild. (F-13)
   - **Cultures.** Culture bonuses are rates or materials of their own, never discounts on volume. Delver sappers in the Hall era pay timber for iron. Fire depends on the attacker's era. (F-14, F-15)
   - **Counters.** Knights beat men-at-arms and archers, as the roster table says. (F-11)
   - **Elevation** steps apply to arrows and bolts, not engines, and engines can target engines. (F-12)
   - **The Deep** opens level by level, by phase. (F-29)
   - **Banners and ranking.** Banner membership is fixed from Contest. A lord without a banner is a banner of one. AI houses are ranked but cannot win. A tie goes to whoever reached the total first. (F-38)
   - **Retirement.** A lord who does not log in for seven days retires. (F-36)
   - **The steward** sends carts with an escort, and fights out to a set distance from its walls. (F-34, F-35)
   - **Ending a match.** The hall is the keep until the keep is complete. Every house in Skirmish plays for itself. Garrisoned units count as units. (F-05)
   - **Season tests.** Worlds are gated on tests of the season as well as on the Trials; the thresholds are Proposed under *Kill criteria*. (F-54)
   - **Wording.** The smaller fixes from the review's table of contradictions are applied too.
10. **ADR-002 to ADR-004 are accepted.** ADR-004 gains three consequences from the review: more callers of the resolver, rubble that keeps its material, and holding protection on the resolver's edits.

### 2026-10-31: the MVP's open questions

1. **The spike's pass mark.** An RTX 3060-class GPU at 1920×1080 and 60 frames a second. A frame is 16.7 ms, of which the voxel pass may take 8 ms at the default zoom.
2. **The Trials' roster.** Each trial gives the player a fixed army of spearmen, archers, outriders and men-at-arms, with the ram, sappers and the trebuchet. The Trials need no unit training, provisions, iron or fields.
3. **Content.** The MVP uses only art, fonts and sound made for the project or in the public domain, and no licensed assets.
4. **The MVP ends at the verdict on the Trials.** Skirmish is built after it, under its own plan, once both pillars pass.
5. **The first culture is the Masons.**
6. **The steward's strength.** On its own, against an attacker who has spent the same, the steward holds about half of declared sieges, and the banner tips the balance.
7. **The contest band** is six evening hours, for example 18:00 to 24:00.
8. **Truces.** A banner can be in one enforced truce at a time, and a truce lapses after seven days unless renewed.

### 2026-10-31: the renderer's look

The owner adopted a look specification from a visual prototype, and settled where it collided with the design. [RendererLook.md](RendererLook.md) holds it in full.

1. **The voxel is 0.5 m,** for rendering and for the simulation alike, and for everything solid. Terrain heights come in whole metres, built from 1 m terraces. (Superseded the same day by the 1 m world, below.)
2. **Earth stands at up to 45°,** so the generator's terraces are stable, and [ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)'s slope invariant uses the same rule. At 1 m voxels, no earth column stands more than one voxel above any of its four side neighbours (*M2's values*, item 9).
3. **The camera is orthographic,** 34° above the horizon. The floors of deep ditches and the near terraces of pits can be hidden at that angle, and units in them show as silhouettes.
4. **Units are tokens** of 1 × 1 × 4 voxels, banded in colour. No limbs animate.
5. **Four new materials** settle what the prototype's colours do in the simulation:
   - grass is earth that only the generator makes;
   - leaves span as wide as a canopy, yield no timber, and burn;
   - openings in structures are a colour on solid voxels;
   - shallow water is lighter, so fords show.

   Rubble takes its colour from what it fell from, so rubble that yields stone looks different from waste rubble.

### 2026-10-31: the MVP's assets and the content format

1. **The MVP's voxel assets are the owner's package,** now in `GameData/` (`Assets/` until [ADR-008](ADR/ADR-008-server-and-client-game-data.md)) and catalogued in [MvpAssets.md](MvpAssets.md). It brings nine templates (T1–T9), unit and engine models, and illustrations, and its own choices with them:
   - the proportions of the hall, keep, gatehouse, tower and drop sites;
   - the hall's frame and each drop site's shed as critical parts;
   - hollow towers and keeps;
   - a standard ditch 5 m wide and 2 m deep, with 1:1 sides;
   - one cue per unit type;
   - render-only colours for horse, leather and rubble. (Rubble's colours are no longer render-only: the simulation leaves timber, earth and rock rubble, *M2's values*, item 7.)
2. **The content format is [ADR-005](ADR/ADR-005-content-formats.md),** which closes that question. JSON layer files are the source, with MagicaVoxel copies beside them. Decorations behave and cost as the material they sit in. Everything is baked into a `Content` library, and nothing is loaded at runtime. (Changed the same day by [ADR-008](ADR/ADR-008-server-and-client-game-data.md): the `.vox` copies are no longer committed, and the server reads its data from baked files.)
3. **The palisade gate (T7) is one of the MVP's templates.**

### 2026-10-31: the 1 m world

The owner adopted a second asset package and its proposed change of voxel sizes as the design baseline. [ADR-006](ADR/ADR-006-voxel-sizes.md) records the engineering decision. It stays Proposed until the M1 spike measures it, with a 0.5 m world measured as the fallback, and the M3 breach lab reads predictions on it.

1. **The world and the simulation use 1 m voxels:** terrain, templates, earthworks, rubble and trees.
2. **Models use 0.5 m voxels:** units, siege engines, mantlets, carts, banners and debris. They are drawn over the world, never written into it, and never simulated.
3. **Walls.** A standard wall is 2 m thick, so a section of 8 × 2 × 6 m costs 96 m³ of stone. A thick wall is 3 m thick and costs half as much again. The balance targets stay as they are, and are now tuned for these walls.
4. **Craters show their weapon by size and depth** rather than by shape, and the client's debris adds texture. Whether players can still tell the weapons apart is an open question for the M3 breach lab.
5. **The second package replaces the first in `Assets/`** (now `GameData/`, ADR-008) ([MvpAssets.md](MvpAssets.md)). Its world templates are automatic conversions from 0.5 m, to be redrawn natively at 1 m before the M4 Siege castle is final.
6. **Rendering.** Models shade where they touch the world, and units far away are drawn as impostors ([RendererLook.md](RendererLook.md)).

### 2026-10-31: platforms, input and squads

The owner wants the game on a laptop with mouse and keyboard, on the Surface Pro by touch, and later on Android. [ADR-007](ADR/ADR-007-platforms-and-input.md) records the engineering side.

1. **Windows on x64 and ARM64, both native from the first project.** The target Surface Pro is a Snapdragon X, which is ARM64.
2. **Two input schemes over one set of commands:** mouse and keyboard, and touch. Hover can speed things up but is never the only way to see something. That rule binds the MVP's interface, although the MVP has only the mouse-and-keyboard scheme.
3. **The order of delivery:**
   - **The MVP** is mouse and keyboard, on both architectures.
   - **Touch** follows a positive MVP verdict.
   - **Android** is an ambition with no date. AGENTS.md's rules on Direct3D 12 and on dependencies stand until an ADR reopens them. Keeping Windows and Direct3D inside the engine keeps Android possible.
4. **The Surface is measured from the renderer spike on,** at a reduced internal resolution. Its pass mark is set from those figures.
5. **The player commands squads,** not single figures: six on foot, or three outriders. Selection, orders and control groups act on squads. Each figure is drawn, hit and killed on its own, and a squad can be split when a single figure is needed. At the default zoom a unit is a few pixels wide, so squads help the mouse as well as the finger. (Extended the same day, below.)

### 2026-10-31: the MVP's HUD

The owner delivered a HUD package, kept unchanged in [Reference/HudPrototype/](Reference/HudPrototype/). [Hud.md](Hud.md) adapts it to the decisions above.

1. **Hud.md is the MVP's HUD:** its layout, command cards, minimap, alerts and readouts. What Hud.md marks Proposed is tried before it is settled, in the milestone Hud.md names for each, from M3 to M6.
2. **The MVP builds layout A, played with mouse and keyboard.** The package's touch layout and gestures are the starting design for touch, after the verdict.
3. **One hotkey scheme, the mnemonic one,** with WASD to pan.
4. **The interface scale follows the display's DPI,** in steps of 1, 1.5 and 2, and names stay larger than labels at every step.
5. **A dig's yield counts only the building stone the player can see,** so a readout never reveals where stone lies.
6. **The font is Earthrise Mason,** drawn for the project. Colour-blind palettes are deferred, and no state is shown by colour alone.

### 2026-10-31: squads, the Hold start and confirmations

1. **Squads are trained whole,** and the population cap counts figures, so a squad of six takes six. A worn squad refills figure by figure at the building that trains it.
2. **MERGE combines squads of one type** up to full size, and any figures left over stay a squad. It is how a split figure rejoins, and how worn squads are made whole in the field.
3. **Laborers are not in squads.** Each one takes jobs from designations, and selecting a laborer selects one.
4. **Knights ride in threes,** like outriders. Every mounted squad is three, and every squad on foot is six.
5. **The Hold trial starts with the hall and a finished keep,** so the build window goes into quarries, walls, towers and ditches, the choices H1 tests.
6. **Confirmed as Decided:**
   - the ARM64 floor of `/arch:armv8.0+lse` (ADR-007);
   - AGENTS.md R16's correction on contraction;
   - ADR-005's amendments: the asset check that every structure voxel connects to its template's base, and a folder for the font (now `GameData/Client/Fonts/`);
   - Hud.md's additions: the cards, the cell widths, BLOCKED, GROUND from the resolver, section names in the scenario, and F1 and F2.
7. **The prototypes' support code is the owner's.** `support.js` in Design/Reference/ was produced with Claude Design by the owner, as were all the packages' assets ([Reference/README.md](Reference/README.md)).

### 2026-10-31: game data, server and client

[ADR-008](ADR/ADR-008-server-and-client-game-data.md) records the engineering side.

1. **Content is split by who reads it.** Sources live in `GameData/Server/` and `GameData/Client/`. Content both sides read, the templates and the palette, has its source on the server's side.
2. **The client has everything compiled in.** The server reads its own data from baked files in `<outputdir>\ServerData`, which can be rebuilt without recompiling, so server-only data such as statistics and waves can be tuned between playtests.
3. **`<outputdir>\GameData` is kept for client content too large to compile in,** audio first.
4. **The JSON layer file is the only committed voxel source.** A tool writes a `.vox` for MagicaVoxel and writes an edit back, so the owner can keep authoring in Claude Design and touch up in MagicaVoxel.
5. **Shaders stay compiled into the executable.**

### 2026-10-31: starting the MVP

The owner settled the open items of the readiness review ([Review/MvpReadiness-2026-10-31.md](Review/MvpReadiness-2026-10-31.md)) that block M0 and M1.

1. **Claude sessions write the code;** the owner runs it and judges it.
   - Code, tools and checkers are written in Claude sessions and pushed to branches, and CI compiles Debug|x64. It compiled Debug|ARM64 as well until the owner dropped that the same day ([ADR-007](ADR/ADR-007-platforms-and-input.md), amended); ARM64 is now built by hand.
   - The owner runs the builds on Windows, judges the look and the measurements, and approves each milestone.
   - The plan's estimates are rescaled from the real pace of the first milestones. (Since 2026-11-01 the plan forecasts calendar weeks and owner hours instead, *Planning M2*.)
2. **MvpPlan.md is accepted through M2,** together with the renderer spike's six questions, its pass mark and the 1 m kill criterion. M3 to M7 stay Proposed until after M1.
3. **How the pass mark is measured:**
   - on a desktop RTX 3060 at 1920 × 1080;
   - as the 95th percentile of the voxel pass's GPU time, shadow passes included;
   - over a scripted camera path at the default zoom through all four rotations;
   - with units moving and the scripted edits running.

   The owner settled three details on 2026-11-01:
   - **The path:** at the default 84 m view height, the camera's target goes round a closed square about the town at a steady ground speed, with a quarter turn eased over 0.3 s at each corner, as a player's would be. Four legs of 15 s make 60 s, or 3600 frames. The path is stepped by frame at a fixed 1/60 s, so every run sees the same views whatever the frame rate. The first second is left out as warm-up.
   - **Presenting:** the measured run does not wait for the vertical blank, so the GPU stays busy at its usual clocks, and the CSV records the adapter and driver version so that runs can be compared.
   - **Edit-to-screen latency** (ADR-006's table) runs from the frame in which the chunk store publishes an edit to the first present that shows it, as DXGI's frame statistics report it. It includes the swap chain's queue, so it is what a player sees.

   And on the same day, when the spike's default became borderless full screen:
   - **The measured run's scene draws at 1920 × 1080 on any display,** and is scaled to the display, so the voxel pass does the pass mark's work whatever the monitor. On a 1440p monitor that is a render scale of 0.75. The CSV states the display's size and the render size.

   And for the test scene's edits and units (MvpPlan.md, M1 task 9):
   - **Quarrying runs at 94 voxels a second,** the four-faction peak. The design review back-solved 28,000–94,000 edits a second at 10 cm for two to four factions (F-51), which is 28–94 at 1 m. If the pass mark holds at the peak, it holds for every smaller match.
   - **The wall collapse** is the 8 m T3 section of the west curtain just north of the breach, at 10 s into the 60 s path, while the camera is on the west side. Its stone falls as a heap of rubble at its foot.
   - **Units move:** each walks a circle 2 m across about its place in the v5 scene, at 1.4 m/s, seeded and stepped by frame like the path.
4. **Hardware:** the owner has the RTX 3060-class machine but no Surface Pro yet. The Surface's measurement waits until one is available, and its pass mark stays open ([ADR-007](ADR/ADR-007-platforms-and-input.md)).
5. **The spike's scene takes its layout from the v5 scene,** which is already on the 1 m grid ([Reference/SiegePrototypeV2/](Reference/SiegePrototypeV2/)). A seeded scene script places it. The implementer draws placeholder models for what `GameData/` lacks (trees, cottages, mantlets, banners and stock stacks; in the event, trees and cottages became 1 m world voxels and the stock is T8's, [MvpAssets.md](MvpAssets.md)), and the owner may replace them with Claude Design versions.
6. **Two exceptions to R14** (AGENTS.md): test projects use Visual Studio's native unit-test framework, which vstest runs and which never reaches the game, and build-time tools run on Python, which never ships.
7. **The paper tests' open values** (the stone span, the anchor depth, the questions per prediction item, how a wave scores and picks, and the repair spreadsheet's pass mark) are picked by the implementer. Each is recorded as Proposed with its reason, and the owner confirms or changes them after the results.
8. **The MVP trebuchet switches between baked poses:** packed, cocked and released, three static models.
9. **Skirmish has one or two AI houses.**

### 2026-10-31: the paper tests' values

M0's three paper tests ran: the [Hold rule test](Review/PaperTests/HoldRuleTest.md), the [breach-prediction prototype](Review/PaperTests/BreachPrediction.md) and the [Siege route table](Review/PaperTests/SiegeRouteTable.md). After their runs the owner settled the values below marked Decided. The rest are the implementer's, Proposed with their reasons in the reports, for the owner to confirm or change (*Starting the MVP*, item 7).

**Decided (owner, 2026-10-31):**
1. **Stone spans 2 m.** T2's two floors and its roof are timber: 192 voxels, which make the keep 1,530 m³ of stone and 192 m³ of timber. Every template's stone now stands at that span.
2. **Masonry is ordinary stone.** Palette index 6, masonry, is the stone a template lays, which the asset package calls dressed stone. It uses the crater table's stone column, and its rubble yields stone. The Masons' dressed-stone upgrade gets a palette index of its own.
3. **Repair's pass mark is for one engine.** With one engine, repair adds 1.3–2× to the time to open a section, at the trebuchet's reload. The ten-second lock stays, so two engines firing alternately keep a section locked, as intended (GameDescription.md, *Templates*, on the repair lock). The Siege table measures 1.50× at a 20 s reload.
4. **The Hold's wave rule takes three changes.** An open section is always taken ahead of any intact one. The open term reads the trench a sapper would still need, so undermining shows before collapse. The gate scores only for waves that bring a ram.
5. **A quarry's undermining outweighs its ditch reason.** A quarry in front of a wall still counts as a ditch in front, as pillar 1 says; where it also undermines the wall, the undermining weighs more. The rerun passes in all three M0 layouts, one of them exactly at the bar.

**Proposed, from the breach-prediction prototype:**
6. **The anchor depth is 8 m** below the generated surface, deeper than any siege dig the MVP draws. (Decided 2026-11-01, *M1's open values*.)
7. **Foot soldiers climb 1 m in each 1 m step**, the slope of settled earth. (Decided 2026-11-01, *M2's values*.)
8. **Loose material comes to rest at least 1 m below a structure's top:** a heap's top stays 1 m or more below it ([breach-prediction prototype](Review/PaperTests/BreachPrediction.md), its set height). (Decided 2026-11-01, *M2's values*, item 9; first written "at most", a slip.)
9. **A breach-prediction item shows the damage as done (N = 0)** and asks one question, "open or shut?", with a free-text reason.
10. **Six people take each readout**, twelve in all. The prototype's key splits 8 open and 8 shut. M0 asks the support readout to reach a median accuracy of 0.65, and nobody has taken the test yet.

**Proposed, from the Siege route table:**
11. **The engine-range guard is measured, not computed.** No point outside every garrison's reach lies within an engine's range of the great tower, on the Siege castle or on any castle the Skirmish AI builds. It is measured on the castle's plan, because between towers their reach ends closer to the curtain than the formula assumes. When confirmed, this replaces the guard sentence in decision 2 of *2026-10-31: the design review*.
12. **The trebuchet's range is 12–50 m**, in place of Hud.md's placeholder of 12–70 m. (Decided 2026-11-01 for M2, *M2's values*.)
13. **A trebuchet's shot descends at 45° or steeper.** (Decided 2026-11-01 for M2, *M2's values*.)
14. **T5's gate stands at the inner end of its passage.** The gatehouse then fires into the passage in front of the gate. A lobbed shot cannot reach the gate until the arch over the passage is cut back. T5 is redrawn with it before M4.
15. **Undermining, and a ram at a wall's base, each open a standard section in about three minutes, unopposed,** as one trebuchet does. Repair at most doubles that.
16. **A ram strikes every 15 s, and a standard foundation on deep earth reaches 4 m down.** These are the values that meet item 15.
17. **A shot costs 1.5 m³ of stone,** so that a breach costs the attacker about half of what the defender pays to rebuild it.

**Proposed, from the Hold rule test:**
18. **How close to opening** is 1 − T ÷ T₀. T is the least volume a sapper's trench must still dig to drop the section, and T₀ is the same section's need in undug ground. It is 1 when the section is open, and 0 when no trench can drop it. A sapper's trench, for this reading, is 2 m wide against the section's outer face, along any part of it, to any whole depth above bedrock. It cuts whatever it meets, and its spoil leaves play.
19. **The weights are 2 for how close the section is to opening, and 1 each for the other three reasons.** At these values the breach moves with the quarry in all three M0 layouts, for 1/3, 13/30 and 11/18 of the waves, against a bar of 1/3.
20. **The reasons' measures.** Earth counts 1 when any column under the stretch is earth and no cut at least 2 m deep lies within 10 m of its face along half its length or more. Distance from the towers runs centre to centre, to the nearest tower or keep, divided by the largest such distance; the gatehouse is not a tower. The gate's reason is 1 for a gatehouse.
21. **The draw.** The stretches are shuffled with the wave's seed from SplitMix64 and sorted by score, ties keeping the shuffle, and one of the best 3 is drawn uniformly. When any stretch is open, the draw is among the open ones only.
22. **Rams.** Of the waves other than the sapper wave, every second one brings a ram, starting with the second. A wave without a ram leaves the gatehouse out unless it is open.
23. **The sapper wave** brings sappers and no ram, and picks among the sections standing on earth by the same score and rule.
24. **Reach.** A wave can reach every outer stretch, sections and gatehouses alike; towers are not scored.
25. **The test's own inputs.** Standard foundations draw nothing below the levelled surface. Digs cut once to their depth and then slump. Quarries are 8 m deep, and the near quarry stops at the closest berm at which the slump drops nothing. The breach moves when at least 1/3 of the waves breach elsewhere, averaged over waves with and without a ram.

**Open:**
26. **Timber's span.** T1's roof needs 5 m and T2's timber floors 4 m. (Decided 2026-11-01: 5 m, *M1's open values*.)

### 2026-11-01: M1's open values

The owner settled what M1's remaining tasks needed and no document gave (MvpPlan.md, M1 tasks 7, 11 and 12).

1. **Timber spans 5 m,** the smallest span at which every MVP template's timber stands as drawn, T1's roof included.
2. **The anchor depth is 8 m** below the generated surface, as the paper tests proposed (ADR-004, decision 2).
3. **A navigation tile is 32 × 32 m:** one per chunk column, of 1,024 cells of 1 m, each holding its walkable height and its links to its eight neighbours. An edit rebuilds the tiles its chunks touch.
4. **A season is about a million voxel edits.** A Skirmish castle is about 5,400 m³ of building, quarried from as much again, plus its earthworks: about 12,000 edits a lord. Thirty-two lords building about twice over in eight weeks, and a dozen breached sieges, come to about a million. The synthetic 2 km world of the renderer spike's question 6 carries that many, where holdings would stand.
5. **Until the generator (M2), the MVP map in `SimulationBench` is the test scene's ground at 512 m:** its seeded integer height field, rock and river carried out to 512 m × 512 m, with the castle and quarry of the v5 scene at its middle.
6. **Models get contact shading baked per instance on the CPU:** a placed model's corner values are worked out against the world when it moves or the world under it changes, and go in its own records, as world chunks' do (ADR-006, *Left open*).
7. **Collapse debris tumbles, then rests.** Its pieces fall with the collapse for about 1.5 s, turning, as oriented voxels, and then lie on the rubble at their final angles for good, so a breach keeps its ragged look. They are seeded by the collapse's id, so every client shows the same (RendererLook.md, checks 3 and 6).
8. **The 0.5 m world for ADR-006's comparison is the test scene with each 1 m voxel split into eight,** so the two columns differ only in voxel size.
9. **The test world is built in Simulation,** so the renderer spike and `SimulationBench` measure the same town. Simulation takes its templates through a lookup and never reads client content. Until the server bake writes the templates (M2), both executables hand it Content's, so `SimulationBench` depends on Content until then ([ADR-010](ADR/ADR-010-solution-and-build-layout.md), amended).

### 2026-11-01: planning M2

The owner settled how M2 is planned and when it starts ([MvpPlan.md](MvpPlan.md), M2).

1. **No M2 code until M1 closes.** M2 starts once the pass mark is measured on the RTX 3060 machine, ADR-006's table is filled in, and M1 is signed off. Until then M2 is planned on paper only.
2. **Effort is not estimated, and there is no build budget.**
   - FTE-months and FTE-days are dropped from the plan.
   - The build budget is removed, with its cut order. What to cut, if scope must shrink, is the owner's call when it arises.
   - Claude sessions write the code, so what sets the pace is the owner's hours, the owner's decisions and CI's round trips.
   - Each milestone forecasts its calendar weeks and the owner's hours, and logs both.
3. **Work is planned as work orders,** each a self-contained brief a Claude session can carry out without rebuilding its scope from the other documents. M2's are in [MvpPlan-M2.md](MvpPlan-M2.md).
4. **One commit per order, one pull request per phase.** A phase is a group of orders that lands together (AGENTS.md §6).
5. **An ADR may be written ahead of its code.** It stays Proposed until the pull request that first implements it marks it Accepted (AGENTS.md §6).

### 2026-11-01: M2's values

The owner answered MvpPlan.md's thirteen M2 inputs, taking each proposal as written (work order M2-A1). Input 13 is the implementer's, measured in M2 and recorded here as Proposed when it is. The same day, after a check of the documents' consistency, the owner changed item 7 and settled items 14 to 16.

1. **ADR-013 to ADR-016 have the go-ahead,** as written. Each is marked Accepted by the commit that first implements it. With ADR-014's go-ahead, a scenario is a baked source, not a save (decision 7), and the MVP's line on scenarios changed with it.
2. **A tick takes at most 25 ms at the 95th percentile,** on one core of the RTX 3060 machine, measured in `SimulationBench` over the M2 breach run. That is half of a 50 ms tick.
3. **The generator makes the MVP map** at 512 m × 512 m and 64 m high:
   - the relief and soil depths of M1's test ground, carried over the map;
   - bedrock depth varying, so that thin soil over rock and deep earth both occur;
   - sparse outcrops of building stone;
   - forest;
   - one river with fords.

   The owner judges the first maps in the renderer spike.
4. **A collapse warns for 1 s plus the time a foot soldier at 1.4 m/s takes to walk the piece's height,** rounded up to whole ticks: 135 ticks for an 8 m piece.
5. **The fall lasts the free-fall time from the piece's height,** under gravity of 9.81 m/s², rounded up to whole ticks: 26 ticks for 8 m.
   - The piece leaves the grid on the fall's first tick.
   - Its rubble and damage land in equal parts over the fall, nearest the foot first.
6. **A piece falls towards the one of four sides where the voxels whose removal unsupported it lie,** measured from its centre. It falls straight down when that offset is less than a quarter of its width along it.
7. **Each voxel that falls leaves one rubble voxel:**
   - building stone and masonry, stone rubble (8);
   - common rock, rock rubble (28);
   - timber and roof, timber rubble (25);
   - earth, grass, field and road, earth rubble (26);
   - leaves, nothing.

   **A decoration takes its host's behaviour, voxel by voxel.** The bake writes each decoration into the world as a palette index for its host, with the decoration's look: an opening in timber and an opening in stone are two indices. Each index then has one rule, and decorations behave and cost as the material they sit in. (Changed the same day from "the most common host across the templates", which would have made T1's openings in timber behave as stone.)
8. **A fall removes every structure voxel in its landing strip below the rubble's top,** and each becomes rubble of its own material. Units in the strip take the collapse's crush damage, whose amount is M4's.
9. **Loose material slumps over the four side neighbours,** and rests at least 1 m below a structure's top (the paper tests' item 8).
10. **The navigation classes** ([ADR-015](ADR/ADR-015-navigation.md)):

    | Class | Climbs | Headroom | Width |
    |---|---|---|---|
    | Foot | 1 m in 1 m | 2 m | 1 |
    | Cavalry | 1 m in 2 m | 3 m | 1 |
    | Machines | 1 m in 3 m | 4 m | 3 |

    Every class fords one voxel of water. Rubble and fords halve speed. Foot moves at 1.4 m/s until M4's statistics.
11. **A rubble breach is open to foot alone.** Settled earth and rubble lie at 45°, steeper than cavalry and machines climb. Men-at-arms storm up the rubble; rams go for gates.
12. **The trebuchet's craters follow the paper tests' ladder:**
    - **Timber:** the voxel struck and its six face neighbours, timber only.
    - **Earth:** 4 voxels, which slump closed.
    - **Stone:** the voxel struck and the one behind it along the shot.

    The other materials a shot can strike take the nearest of these:
    - **Common rock:** as stone.
    - **Bedrock:** nothing, since it cannot be dug.
    - **Leaves:** as timber, leaves only.
    - **Every rubble:** as earth, slumping closed.
    - **Water:** nothing; the shot stops.

    For M2 the shot reloads in 20 s, the Siege route table's reload, and reaches 12–50 m and descends at 45° or steeper (the paper tests' items 12 and 13). It leaves from 10 m above the engine's cell, about where the cocked model's arm releases, and falls under gravity of 9.81 m/s². The ram's craters are M4's.
13. **The work budgets a tick,** voxels slumped and path nodes expanded, are set by the implementer from M2's measurements to fit item 2 (Proposed until measured).
14. **Ground, and the spans not yet decided.**
    - Palette.json gives each index a server-side `ground` flag. Units stand only on ground, and building stone in an outcrop is ground while masonry, the same behaviour laid by a template, is not.
    - Ground: terrain, fields, roads and every rubble. Not ground: masonry, timber, roof, leaves and decorations.
    - Leaves span 3 m, a canopy's 2.2 m radius rounded up. Common rock spans 2 m, as stone does. Stone's 2 m and timber's 5 m stand.
15. **Queries are budgeted in units of work,** a number answered a tick, oldest dropped first. The simulation reads no clock anywhere ([ADR-009](ADR/ADR-009-queries-across-the-server-boundary.md), amended).
16. **Five older inconsistencies are Open questions,** each for the milestone that must settle it (*Open questions*).

## Pillars (Decided)

A pillar is a test every feature has to pass. A feature that serves none of them is a candidate for cutting.

1. **Where you dig decides how you can be attacked.** Resources are voxels in finite, located deposits: building stone in outcrops, ore in underground veins, timber in forests that stay cut for the life of a world. A quarry beside a wall is a ditch an attacker has to cross; a quarry in earth is a slope that can slump a foundation away.
2. **Walls fall where they are struck.** A wall is breached where it is hit, not when a health bar runs out. A breach is a hole an army can walk through. A collapse damages what it lands on, and undermining a foundation brings down what stands on it. A structure stands only where it is supported: connected down to bedrock or below a fixed depth, with no more than a maximum unsupported span for each material ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 2).
3. **The land remembers for the life of a world.** Changes to the land last until the world's season ends. A world that ends is archived, not deleted: its seeds, generator version and edits stay viewable.

## Where RTS logic meets destruction (Decided)

- **Unit-versus-unit combat stays stat-based**: attack, armor and counters, with melee, pierce and crush damage. At RTS distance a hit location on a soldier is invisible, and counter-based combat is what keeps a battle legible.
- **Only engines, sappers, laborers on a designation, collapses and fire remove voxels.** Soldiers never do. A shot travels through the voxel grid and stops at the first solid thing it meets, so a wall stops an arrow. Vision is a radius and is not blocked by walls. An engine sees as far as it shoots.
- **Structures and terrain are physical.** A hit removes a crater where it lands, with a radius that depends on the weapon and the material. Voxels carry no hit points.
- **Materials behave differently.** Wood, earth and stone each have their own strength: how large a crater a hit leaves, and how far the material spans without support. Earth slumps to 45°, so no earth column stands more than one voxel above any of its four side neighbours, and digging beside a wall's foundation is a siege tactic. Grass is earth that only the generator makes. Leaves span as wide as a canopy, yield no timber and burn. Common rock holds vertical faces. Bedrock cannot be dug, and nothing founded on it can be undermined. The generated world holds earth only at or below its slope; steeper ground is stone. Timber burns once the attacker has fire: from the Citadel era, or the Keep era for the Wealdfolk. Fire is not in the MVP.
- **Excavation is a designation.** The player draws a quarry, pit, ditch, trench, felling, clearance or demolition designation with a footprint and a depth. Workers dig it in a pattern that always leaves a ramp out, so a dig never traps its own workers. A moat is a ditch, and undermining is a trench beside a foundation.
- **A building is a template.** Construction fills in the template's voxels as workers build. Each template has a foundation that reaches the ground, and construction levels the footprint first; a deep foundation reaches bedrock. Repair puts back voxels that were built and later removed, and pays for the material. It works from the owner's side at a fixed rate per section, and not on a section hit in the last ten seconds by an engine shot, a ram blow or a trench. A building stops working when one of its critical parts, such as a gatehouse's gate or a tower's firing platform, falls below its threshold, the share of the part's voxels still standing and supported; it is then a ruin until repaired. A building never finished is under construction, not a ruin. Nothing is built outside templates.
- **No unit stands on a structure.** Towers and keeps garrison and fire. Walls are crossed only through gates and breaches. Ramparts are earth, not structures, and units stand on them. Loose material never comes to rest on top of a structure or near the top of one beside it, so no heap makes a ramp over an intact wall.
- **A unit whose ground is removed drops to the next walkable surface in its column** and takes crush damage if the drop is more than 3 m. A unit whose cell fills with rubble is placed on top and takes the collapse's damage. Navigation moves it; a collapse never does ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 6).
- **A breach is a path.** Navigation updates from edits once per tick, on a grid sized for units rather than voxels; at the 1 m world the two are the same, cells of 1 m (*M1's open values*, item 3). A gap a foot soldier fits through, one world voxel wide and two high, is a route; cavalry and machines need more (*M2's values*, item 10). Each column has one walkable surface, its highest walkable cell, because units never stand on structures and nothing is tunnelled; the space under a rock overhang is not walkable. Foot soldiers climb slopes as steep as settled earth, and cavalry, carts, rams and engines climb less.
- **A piece that loses its support collapses** through a rule in the simulation ([ADR-004](ADR/ADR-004-rule-based-destruction.md)). It gives a visible warning first. It falls towards the side that lost support, or straight down if it lost support evenly. It does not rotate in the simulation; it is laid down as rubble along the fall, over a length up to its height, and damages the voxels and units in that strip as it falls. A collapse that removes another structure's support starts a new collapse on the next tick. Visual debris is the client's business and never affects play.
- **Rubble is loose material.** It slumps like earth, can be walked over more slowly, and keeps the material it fell from. Workers clear rubble of building stone for stone at half its volume; other rubble yields nothing.

## Persistence and the shared world (Decided, except where marked Proposed)

A classic RTS match ends; a world lasts a season, and the season replaces the match arc. [GameDescription.md](GameDescription.md) gives the detail.

- **Offline protection.** Outside its window, which starts inside the world's contest band, nobody else can change anything in a holding: no structure is damaged, and no voxel is removed or added, whatever the cause. Only a siege declared a day ahead can sack, so the banner can answer it. The steward ([ADR-002](ADR/ADR-002-authoritative-simulation.md), decision 5) defends and repairs when the lord is away.
- **Snowballing.** The ranking and every lord's Earthstone are public, and siege windows lengthen with Earthstone held, up to twelve hours. Upkeep is paid daily, in provisions for every soldier and in stone for every stone fortification in proportion to its volume. Unpaid upkeep makes walls weather and soldiers desert. Storehouses cap stockpiles, banners hold at most four lords, a sack takes a third of the lord's Earthstone, and the season ends.
- **Newcomers.** A world opens when lords have queued for at least half its seats. A lord claims a holding during Settlement; after that, newcomers queue for the next world. A lord who does not log in for seven days retires, and their holding becomes a ruin.
- **Ending.** Every world runs an eight-week season, then freezes and is archived. Pillar 3 binds a change to the life of its world, so the end of a season does not break it.
- **Population.** 8 to 32 seats per world, at least half of them human when it opens, and AI houses in the rest. One person holds one seat.
- **Operations.** Worlds run on dedicated servers. Communities host them first, within set rules for season length, seats and windows, and each host sets its world's contest band. Titles are recorded in each world's archive and name the world and its host; there is no central account service. Official worlds follow when the population justifies them. A hosted server is the authoritative simulation of [ADR-002](ADR/ADR-002-authoritative-simulation.md), so this changes nothing in the architecture.
- **Demand.** Online work needs a counted demand signal. The single-player release asks its players to sign up for a world, and online work starts only when enough have.

**Proposed:**

- **The demand threshold** is 32 sign-ups, enough to open two 32-seat worlds at half human.
- **Online work starts only when** the MVP's hypotheses and the season tests (*Kill criteria*) have passed, and the demand threshold is met. The simulation must build headless as its own project, with no dependency on the client. [ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)'s generator versioning must be in force, and a save must survive a change of generator version. An operations budget must be written down, in hours per week and money per month. Two questions must be decided apart: whether distribution goes through a store, which needs no SDK in the executable, and whether identity goes through a platform such as Steam, whose SDK R14 excludes.

## MVP (Decided: the stages, the hypotheses, the control, the roster and where the MVP ends; Proposed: the rest)

The MVP exists to prove that pillars 1 and 2 are fun. It does not try to prove persistence or networking, only to keep both possible. The simulation sits behind a server boundary ([ADR-002](ADR/ADR-002-authoritative-simulation.md)), and the world is a seed plus edits ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)). It is built in stages (below), so the test of the pillars does not wait for an AI that plays the whole game. The MVP ends with the verdict on stages 1 and 2; Skirmish, stage 3, comes after it under its own plan. [MvpPlan.md](MvpPlan.md) plans the MVP's build: accepted through M2, Proposed from M3 (*Starting the MVP*).

**In:**

- One map, 512 m × 512 m, generated from a seed and held entirely in memory.
- One human player against one AI faction.
- One faction type shared by everyone. Its roster is workers, a small military roster built around counters, the ram and at least one lobbed engine. The period is the high medieval, before gunpowder. Decided: the military roster is spearmen, archers, outriders and men-at-arms; the siege roster is the ram, sappers and the trebuchet; and each trial gives the player a fixed army, so the Trials need no unit training, provisions or iron.
- Only art, fonts and sound made for the project or in the public domain (Decided).
- Gathering by excavation, construction and repair from templates, and rule-based collapse.
- Fog of war as a vision radius, enforced by the simulation ([ADR-002](ADR/ADR-002-authoritative-simulation.md), decision 4). A fogged area shows what the player last saw.
- A win condition for each stage.
- Save and load as a seed plus edits. A scenario holds the seeds, the structures and earthworks placed on the generated world, and the entities; a save holds the same and the work in flight ([ADR-014](ADR/ADR-014-edits-saves-and-replays.md), decision 7; changed 2026-11-01, *M2's values*, item 1).
- The simulation running in-process behind the server boundary.

**Out.** Each item here is a later decision, not a never:

- Online play of any kind, including the shared world.
- More than one faction type.
- Fire.
- Worlds larger than memory, and streaming.
- Technology progression beyond what the MVP roster needs.
- AI factions that plan undermining. They shoot sappers and repair, and the Hold trial's scripted sapper wave is the one exception.
- In the Trials: provisions, iron, unit training, the mangonel, the mantlet, crossbowmen, knights and thick walls (Decided; they come with Skirmish).
- Skirmish itself (Decided). It is built after the MVP, under its own plan.
- Licensed art, fonts and sound (Decided).

### Hypotheses (Decided)

Each pillar is stated as a claim a playtest can refute.

- **H1, pillar 1: where a player quarries changes how their settlement is attacked.** It fails if, in stage 2, where players quarried has no more visible relation to where the waves breached or how the defense went than the same players show in the control, or if the only trade-off players can name in choosing where to dig is about hauling.
- **H2, pillar 2: players choose where to breach for reasons they can state, and predict the result.** It fails if the breach points players choose cluster at the nearest point of the wall, or at the gate, whatever the wall and the ground are like; if players asked "will this section be open after the next N shots?" fall short of the pass mark below; or if players do not prefer it to the control. "Open" means the passability view shows a route for foot soldiers through the section, and N is fixed for each question.

The control is the same stages with walls built of segments that have health bars, and resources gathered from nodes that run out. It swaps the collapse resolver ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 3) and the gathering rule and leaves the rest of the game unchanged. Both versions keep every cosmetic, so only the rules differ, and the integrity view is on by default, as the health bars are.

**The playtest (Proposed).**
- **Who plays.** 8 to 10 people who play RTS games, in two rounds: the Siege trial first, for H2, and then the Hold trial, for H1. Each person plays the stage in both versions, with half of them playing the voxel version first.
- **What is recorded.** Their reasons are recorded before anyone explains the rules.
- **Pass marks.** Preference passes at 7 of 8 or better, scaled to the final number. Prediction questions are split evenly between sections that will open and sections that will not, and pass at a median player accuracy of 0.65 or more.
- **Learnability.** Two or three people who do not play RTS games also play, reported separately as a check on how learnable the game is.

### Stages (Decided)

1. **Siege.** The player attacks a pre-built, garrisoned castle. Its AI faction fights and repairs, and does nothing else.
   - The player wins by breaking the keep: it has stopped working, and their soldiers have held its foot for two minutes without a repair.
   - They lose when no attacking unit is left, or when a time limit runs out.
   - The castle's nearest section is not its weakest, so choosing the nearest point and choosing the weakest can be told apart.
2. **Hold.** The player starts with the hall and a finished keep, quarries and builds for a set time, then scripted waves attack.
   - **How a wave picks its breach.** A wave scores each stretch of wall it can reach on the four reasons an attacker reads (GameDescription.md, *How a siege plays*): how close the section is to opening, which it asks the collapse resolver as the trench a sapper would still need; whether it stands on earth with no ditch in front; how far it is from the towers; and whether it is a gate. A quarry in front of a wall counts as a ditch in front, and where it undermines the wall the undermining weighs more. An open section is always taken ahead of any intact one; otherwise the wave picks among the best few with a seeded draw. The gate scores only for waves that bring a ram. (Decided, 2026-10-31; the values are Proposed in *2026-10-31: the paper tests' values*.)
   - **Sappers.** One wave brings sappers, which trench beside a section standing on earth.
   - **Maps.** Every Hold map has at least two outcrops within reach of the hall, on different sides.
   - **Winning.** The player wins if the keep still works when the last unit of the last wave is dead or gone.
3. **Skirmish.** One or two AI factions that play the whole game. It is built only after stages 1 and 2 have passed, and lies outside the MVP. Every faction plays for itself. A faction loses when its keep is broken, or when all its trained units, garrisoned ones included, are dead.

Each stage builds a piece the shared world needs: the Siege stage's defending AI, with the Skirmish AI's economy, becomes the steward, the Hold stage's waves become the world's raiders, and the Skirmish AI becomes the AI houses.

### Kill criteria (Proposed, except the renderer spike's, which is Decided)

- If H2 still fails after one round of work on legibility, destruction becomes cosmetic over walls with health bars, and this document is rewritten before more content is made.
- If H1 fails, resources become nodes that leave visible scars on the land. Ditches, trenches and pits stay dug, and Earthstone is still dug out of the Deep, because the Deep is the objective, not the economy. Whether Worlds goes ahead without pillar 1 is decided then, before Skirmish starts.
- Before any Worlds code, the season is tested on paper. At least 16 players play an eight-week campaign, each holding a window inside a band, with sieges settled by dice or by a Trial build. If fewer than three in four are still playing in week 6, or both sides are present in fewer than half the windows that see a siege, the season is reworked first.
- If the demand signal stays below its threshold (*Persistence*), Worlds is not built.
- If the renderer spike misses its pass mark with the 1 m baseline, a new ADR revisits [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md) before any gameplay code is written.

## Renderer spike (Decided: it happens, draws a changing scene, answers the six questions below and has a pass mark)

The spike is the first code in the repository. It conforms to AGENTS.md like any other code, which also means it settles the solution layout (AGENTS.md §2). It draws a generated test scene with [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md)'s method: terrain, a dense forest, and a town with walls, roughly what a late-game settlement looks like. The scene also changes while it is drawn (question 5). It answers six questions:

1. **Voxel size.** It is decided: a 1 m world and simulation, with 0.5 m models ([ADR-006](ADR/ADR-006-voxel-sizes.md), [RendererLook.md](RendererLook.md)). The spike shows it at the default zoom, the closest and the farthest, and checks the look's acceptance tests on screen. It measures a 0.5 m world too, so that ADR-006 is decided on figures.
2. **Camera.** It is decided: orthographic, 34° above the horizon, rotating in 90° steps, from about 12 m to about 260 m of view height. The spike checks it on a ditched wall and a terraced pit added to the scene, with units drawn as their 0.5 m models. It also checks that unit classes and sides can be told apart at the default zoom.
3. **Cost.** GPU time at the default and the farthest zoom, measured on stated hardware: the RTX 3060-class machine, and natively on a Snapdragon X Surface Pro at a reduced internal resolution ([ADR-007](ADR/ADR-007-platforms-and-input.md)).
4. **Look.** Whether shadow maps, baked corner shading and jittered colour reach RendererLook.md's acceptance checks. If they fall short, a lighting ADR follows: traced shadows and occlusion through a voxel occupancy volume, or DXR for secondary rays, which ADR-001 leaves room for.
5. **Edits.** The scene runs quarrying at the peak rate the Skirmish tempo implies, back-solved from template volumes at 1 m, and one wall collapse. The spike measures the cost of rebuilding an edited chunk's render records, its neighbours' corner shading included. No world voxel falls below about 4 px anywhere in the zoom range, and units switch to impostors below 3 px. So ADR-001's stochastic pruning is not used, and the problem of finding a pruning key that survives edits does not arise.
6. **Simulation cost** at 1 m, and at 0.5 m for comparison: memory for the MVP map, one support search on the test town after a breach, and one navigation tile rebuild. It measures the same on a synthetic, headless world of 2 km × 2 km that carries a season's edits and a dozen sieges at once.

The pass mark is decided: an RTX 3060-class GPU at 1920×1080 and 60 frames a second, with the voxel pass inside 8 ms of the 16.7 ms frame at the default zoom. It is measured on a desktop RTX 3060 as the 95th percentile of the voxel pass's GPU time, shadow passes included, over a scripted camera path at the default zoom through all four rotations, with units moving and the scripted edits running. The Surface's pass mark is set from the spike's figures once a Surface is available.

Its output is ADR-006's table of measurements, for the 1 m baseline and for a 0.5 m world, plus the same measurements on the Surface. If question 4 calls for one, a lighting ADR follows. The spike needs Windows and a Direct3D 12 GPU to build and run, and a Snapdragon X Surface Pro for its ARM64 figures.

## Open questions

- The skirmish AI's capabilities, in detail.
- **Craters at 1 m:** can players tell a ram, a mangonel and a trebuchet apart by the size and depth of their craters? The M3 breach lab has only the trebuchet, the ram's craters come in M4, and the mangonel is out of the Trials, so where and how this is tested is open. Settle before M3.
- **The Hold control's designations (M6):** Hud.md has laborers GATHER from nodes in place of all six designations, ditch, trench and pit included, while the control swaps only "the gathering rule" and keeps ditches in the build window (*MVP*). Which designations count as gathering?
- **The HUD's sizes (M3, M4):** Hud.md asks for hit targets of at least 44 logical px, but gives the camera buttons 36 × 36 and control groups 40 px high. Its deck is at least 972 logical px wide, too wide for 1280 px at 1.5× and for 1920 px at 2×, though its done-when asks for both. The font has no icons for SPLIT, MERGE, WALL, PAL.GATE, DEEP or DEPLOY.
- **Asset ids M1 and M2** for the material samples ([MvpAssets.md](MvpAssets.md)) share their names with milestones M1 and M2. Renaming them means editing ids and files.
- **"Masonry"** names palette index 6, ordinary laid stone (*the paper tests' values*, item 2), and an upgrade line in GameDescription.md, *Technology* ("Masonry: dressed stone"). One of them is renamed before the Masons' upgrade is built.
- **The Surface's pass mark,** set from the renderer spike's figures ([ADR-007](ADR/ADR-007-platforms-and-input.md)).
- **The touch scheme:** its gestures and layout, designed after the MVP's verdict. The HUD package's layout B and the second asset package's landscape phone layout are the starting points ([Hud.md](Hud.md) §4, [RendererLook.md](RendererLook.md)).
- Unit statistics, costs, gather rates and every other number behind the description's targets. The Trials and Skirmish tune them. The review named the ones the rules now lean on: a shot's stone price, each template's critical-part threshold, and the repair rate per section. (The warning time before a collapse was decided on 2026-11-01, *M2's values*, item 4.)
- **The sizes behind the Worlds rules:** the daily work allowance, deposits per holding, the Deep's yield by phase and the distance the steward sallies.
