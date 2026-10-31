# Earthrise.World — Game Design

**Status:** Draft, 2026-10-30; revised 2026-10-31, when the owner accepted the game description, and again the same day after the [design review](Review/GameDescription-2026-10-31.md). Nothing here is implemented. Each section says what kind of statement it holds: **Decided** records the owner's answers, **Proposed** is a recommendation awaiting the owner, **Open** is what nobody has decided yet. A proposal becomes a decision only when the owner accepts it, and this document is updated when that happens.

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
8. **Two cuts now.** Titles are recorded in each world's archive, not by a central account service. The season timelapse is rendered from top-down daily images by a tool under `Tools/`, not by the game's renderer. AI houses that play the whole game, and Worlds that open with three cultures, stay; they head the cut order (*Build budget*). (F-52, F-56, F-63)
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
2. **Earth stands at up to 45°,** so the generator's terraces are stable, and [ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)'s slope invariant uses the same rule. At 1 m voxels, no earth column stands more than one voxel above its neighbour.
3. **The camera is orthographic,** 34° above the horizon. The floors of deep ditches and the near terraces of pits can be hidden at that angle, and units in them show as silhouettes.
4. **Units are tokens** of 1 × 1 × 4 voxels, banded in colour. No limbs animate.
5. **Four new materials** settle what the prototype's colours do in the simulation:
   - grass is earth that only the generator makes;
   - leaves span as wide as a canopy, yield no timber, and burn;
   - openings in structures are a colour on solid voxels;
   - shallow water is lighter, so fords show.

   Rubble takes its colour from what it fell from, so rubble that yields stone looks different from waste rubble.

### 2026-10-31: the MVP's assets and the content format

1. **The MVP's voxel assets are the owner's package,** now in `Assets/` and catalogued in [MvpAssets.md](MvpAssets.md). It brings nine templates (T1–T9), unit and engine models, and illustrations, and its own choices with them:
   - the proportions of the hall, keep, gatehouse, tower and drop sites;
   - the hall's frame and each drop site's shed as critical parts;
   - hollow towers and keeps;
   - a standard ditch 5 m wide and 2 m deep, with 1:1 sides;
   - one cue per unit type;
   - render-only colours for horse, leather and rubble.
2. **The content format is [ADR-005](ADR/ADR-005-content-formats.md),** which closes that question. JSON layer files are the source, with MagicaVoxel copies beside them. Decorations behave and cost as the material they sit in. Everything is baked into a `Content` library, and nothing is loaded at runtime.
3. **The palisade gate (T7) is one of the MVP's templates.**

### 2026-10-31: the 1 m world

The owner adopted a second asset package and its proposed change of voxel sizes as the design baseline. [ADR-006](ADR/ADR-006-voxel-sizes.md) records the engineering decision. It stays Proposed until the M1 spike measures it, with a 0.5 m world measured as the fallback.

1. **The world and the simulation use 1 m voxels:** terrain, templates, earthworks, rubble and trees.
2. **Models use 0.5 m voxels:** units, siege engines, mantlets, carts, banners and debris. They are drawn over the world, never written into it, and never simulated.
3. **Walls.** A standard wall is 2 m thick, so a section of 8 × 2 × 6 m costs 96 m³ of stone. A thick wall is 3 m thick and costs half as much again. The balance targets stay as they are, and are now tuned for these walls.
4. **Craters show their weapon by size and depth** rather than by shape, and the client's debris adds texture. Whether players can still tell the weapons apart is an open question for the M3 breach lab.
5. **The second package replaces the first in `Assets/`** ([MvpAssets.md](MvpAssets.md)). Its world templates are automatic conversions from 0.5 m, to be redrawn natively at 1 m before the M4 Siege castle is final.
6. **Rendering.** Models shade where they touch the world, and units far away are drawn as impostors ([RendererLook.md](RendererLook.md)).

## Pillars (Decided)

A pillar is a test every feature has to pass. A feature that serves none of them is a candidate for cutting.

1. **Where you dig decides how you can be attacked.** Resources are voxels in finite, located deposits: building stone in outcrops, ore in underground veins, timber in forests that stay cut for the life of a world. A quarry beside a wall is a ditch an attacker has to cross; a quarry in earth is a slope that can slump a foundation away.
2. **Walls fall where they are struck.** A wall is breached where it is hit, not when a health bar runs out. A breach is a hole an army can walk through. A collapse damages what it lands on, and undermining a foundation brings down what stands on it. A structure stands only where it is supported: connected down to bedrock or below a fixed depth, with no more than a maximum unsupported span for each material ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 2).
3. **The land remembers for the life of a world.** Changes to the land last until the world's season ends. A world that ends is archived, not deleted: its seeds, generator version and edits stay viewable.

## Where RTS logic meets destruction (Decided)

- **Unit-versus-unit combat stays stat-based**: attack, armor and counters, with melee, pierce and crush damage. At RTS distance a hit location on a soldier is invisible, and counter-based combat is what keeps a battle legible.
- **Only engines, sappers, laborers on a designation, collapses and fire remove voxels.** Soldiers never do. A shot travels through the voxel grid and stops at the first solid thing it meets, so a wall stops an arrow. Vision is a radius and is not blocked by walls. An engine sees as far as it shoots.
- **Structures and terrain are physical.** A hit removes a crater where it lands, with a radius that depends on the weapon and the material. Voxels carry no hit points.
- **Materials behave differently.** Wood, earth and stone each have their own strength: how large a crater a hit leaves, and how far the material spans without support. Earth slumps to 45°, so no earth column stands more than one voxel above its neighbour, and digging beside a wall's foundation is a siege tactic. Grass is earth that only the generator makes. Leaves span as wide as a canopy, yield no timber and burn. Common rock holds vertical faces. Bedrock cannot be dug, and nothing founded on it can be undermined. The generated world holds earth only at or below its slope; steeper ground is stone. Timber burns once the attacker has fire: from the Citadel era, or the Keep era for the Wealdfolk. Fire is not in the MVP.
- **Excavation is a designation.** The player draws a quarry, pit, ditch, trench, felling, clearance or demolition designation with a footprint and a depth. Workers dig it in a pattern that always leaves a ramp out, so a dig never traps its own workers. A moat is a ditch, and undermining is a trench beside a foundation.
- **A building is a template.** Construction fills in the template's voxels as workers build. Each template has a foundation that reaches the ground, and construction levels the footprint first; a deep foundation reaches bedrock. Repair puts back voxels that were built and later removed, and pays for the material. It works from the owner's side at a fixed rate per section, and not on a section hit in the last ten seconds by an engine shot, a ram blow or a trench. A building stops working when one of its critical parts, such as a gatehouse's gate or a tower's firing platform, falls below its threshold, the share of the part's voxels still standing and supported; it is then a ruin until repaired. A building never finished is under construction, not a ruin. Nothing is built outside templates.
- **No unit stands on a structure.** Towers and keeps garrison and fire. Walls are crossed only through gates and breaches. Ramparts are earth, not structures, and units stand on them. Loose material never comes to rest on top of a structure or near the top of one beside it, so no heap makes a ramp over an intact wall.
- **A unit whose ground is removed drops to the next walkable surface in its column** and takes crush damage if the drop is more than 3 m. A unit whose cell fills with rubble is placed on top and takes the collapse's damage. Navigation moves it; a collapse never does ([ADR-004](ADR/ADR-004-rule-based-destruction.md), decision 6).
- **A breach is a path.** Navigation updates from edits once per tick, on a grid sized for units rather than voxels. A gap a unit fits through, one world voxel wide and two high, is a route. Each column has one walkable surface, its highest walkable cell, because units never stand on structures and nothing is tunnelled; the space under a rock overhang is not walkable. Foot soldiers climb slopes as steep as settled earth, and cavalry, carts, rams and engines climb less.
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

## Build budget (Decided: there is one, with a cut order; Proposed: its figures and the order)

The review estimated the plan at 79–137 FTE-months of work for one experienced developer. These are judgment estimates, not measurements (review finding F-52). The two cuts taken in the design review save about 3–6 of them.

| Stage | FTE-months (estimate) |
|---|---|
| Renderer spike, Trials and their playtest | 32–56 |
| Skirmish, Worlds and release, after the two cuts | 44–76 |

At 20 hours a week that puts the Trials verdict 5–9 years out. The spike's own estimate is written down before it starts, and when it ends, the ratio of actual time to the estimate rescales every figure here.

**Cut order.** When a stage overruns its budget by half, the next cut in this order is taken before the stage goes on:

1. AI houses become fortress garrisons: they hold the old fortresses with the Siege defender's AI, and never hold Earthstone or rank (saves about 4–8).
2. Worlds open with one culture, and the other two follow (3–5).
3. Banners share vision and truces only (1–2).
4. Fire stays out of the first season (1–1.5).

## MVP (Decided: the stages, the hypotheses, the control, the roster and where the MVP ends; Proposed: the rest)

The MVP exists to prove that pillars 1 and 2 are fun. It does not try to prove persistence or networking, only to keep both possible. The simulation sits behind a server boundary ([ADR-002](ADR/ADR-002-authoritative-simulation.md)), and the world is a seed plus edits ([ADR-003](ADR/ADR-003-world-as-seed-plus-edits.md)). It is built in stages (below), so the test of the pillars does not wait for an AI that plays the whole game. The MVP ends with the verdict on stages 1 and 2; Skirmish, stage 3, comes after it under its own plan. [MvpPlan.md](MvpPlan.md) plans the MVP's build (Proposed).

**In:**

- One map, 512 m × 512 m, generated from a seed and held entirely in memory.
- One human player against one AI faction.
- One faction type shared by everyone. Its roster is workers, a small military roster built around counters, the ram and at least one lobbed engine. The period is the high medieval, before gunpowder. Decided: the military roster is spearmen, archers, outriders and men-at-arms; the siege roster is the ram, sappers and the trebuchet; and each trial gives the player a fixed army, so the Trials need no unit training, provisions or iron.
- Only art, fonts and sound made for the project or in the public domain (Decided).
- Gathering by excavation, construction and repair from templates, and rule-based collapse.
- Fog of war as a vision radius, enforced by the simulation ([ADR-002](ADR/ADR-002-authoritative-simulation.md), decision 4). A fogged area shows what the player last saw.
- A win condition for each stage.
- Save and load as a seed plus edits. A scenario is a save: the seeds, the edits and the entities.
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
2. **Hold.** The player quarries and builds for a set time, then scripted waves attack.
   - **How a wave picks its breach.** A wave scores each stretch of wall it can reach on the four reasons an attacker reads (GameDescription.md, *How a siege plays*): how close the section is to opening, which it asks the collapse resolver; whether it stands on earth with no ditch in front; how far it is from the towers; and whether it is a gate. It picks among the best few with a seeded draw.
   - **Sappers.** One wave brings sappers, which trench beside a section standing on earth.
   - **Maps.** Every Hold map has at least two outcrops within reach of the hall, on different sides.
   - **Winning.** The player wins if the keep still works when the last unit of the last wave is dead or gone.
3. **Skirmish.** One or two AI factions that play the whole game. It is built only after stages 1 and 2 have passed, and lies outside the MVP. Every faction plays for itself. A faction loses when its keep is broken, or when all its trained units, garrisoned ones included, are dead.

Each stage builds a piece the shared world needs: the Siege stage's defending AI, with the Skirmish AI's economy, becomes the steward, the Hold stage's waves become the world's raiders, and the Skirmish AI becomes the AI houses.

### Kill criteria (Proposed)

- If H2 still fails after one round of work on legibility, destruction becomes cosmetic over walls with health bars, and this document is rewritten before more content is made.
- If H1 fails, resources become nodes that leave visible scars on the land. Ditches, trenches and pits stay dug, and Earthstone is still dug out of the Deep, because the Deep is the objective, not the economy. Whether Worlds goes ahead without pillar 1 is decided then, before Skirmish starts.
- Before any Worlds code, the season is tested on paper. At least 16 players play an eight-week campaign, each holding a window inside a band, with sieges settled by dice or by a Trial build. If fewer than three in four are still playing in week 6, or both sides are present in fewer than half the windows that see a siege, the season is reworked first.
- If the demand signal stays below its threshold (*Persistence*), Worlds is not built.
- If the renderer spike misses its pass mark with the 1 m baseline, a new ADR revisits [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md) before any gameplay code is written.

## Renderer spike (Decided: it happens, draws a changing scene and measures drawing and editing; Proposed: the rest of what it answers)

The spike is the first code in the repository. It conforms to AGENTS.md like any other code, which also means it settles the solution layout (AGENTS.md §2). It draws a generated test scene with [ADR-001](ADR/ADR-001-direct-voxel-ray-casting.md)'s method: terrain, a dense forest, and a town with walls, roughly what a late-game settlement looks like. The scene also changes while it is drawn (question 5). It answers six questions:

1. **Voxel size.** It is decided: a 1 m world and simulation, with 0.5 m models ([ADR-006](ADR/ADR-006-voxel-sizes.md), [RendererLook.md](RendererLook.md)). The spike shows it at the default zoom, the closest and the farthest, and checks the look's acceptance tests on screen. It measures a 0.5 m world too, so that ADR-006 is decided on figures.
2. **Camera.** It is decided: orthographic, 34° above the horizon, rotating in 90° steps, from about 12 m to about 260 m of view height. The spike checks it on a ditched wall and a terraced pit added to the scene, with units drawn as their 0.5 m models. It also checks that unit classes and sides can be told apart at the default zoom.
3. **Cost.** GPU time at the default and the farthest zoom, measured on stated hardware.
4. **Look.** Whether shadow maps, baked corner shading and jittered colour reach RendererLook.md's acceptance checks. If they fall short, a lighting ADR follows: traced shadows and occlusion through a voxel occupancy volume, or DXR for secondary rays, which ADR-001 leaves room for.
5. **Edits.** The scene runs quarrying at the peak rate the Skirmish tempo implies, back-solved from template volumes at 1 m, and one wall collapse. The spike measures the cost of rebuilding an edited chunk's render records, its neighbours' corner shading included. No world voxel falls below about 4 px anywhere in the zoom range, and units switch to impostors below 3 px. So ADR-001's stochastic pruning is not used, and the problem of finding a pruning key that survives edits does not arise.
6. **Simulation cost** at 1 m, and at 0.5 m for comparison: memory for the MVP map, one support search on the test town after a breach, and one navigation tile rebuild. It measures the same on a synthetic, headless world of 2 km × 2 km that carries a season's edits and a dozen sieges at once.

The pass mark is decided: an RTX 3060-class GPU at 1920×1080 and 60 frames a second, with the voxel pass inside 8 ms of the 16.7 ms frame at the default zoom.

Its output is ADR-006's table of measurements, for the 1 m baseline and for a 0.5 m world. If question 4 calls for one, a lighting ADR follows. The spike needs Windows and a Direct3D 12 GPU to build and run.

## Open questions

- The skirmish AI's capabilities, in detail.
- **Craters at 1 m:** can players tell a ram, a mangonel and a trebuchet apart by the size and depth of their craters? Tested in the M3 breach lab.
- **Platforms beyond the Windows PC:** the owner wants to support laptops with a mouse and keyboard, the Surface Pro with touch, and Android later. Under discussion, 2026-10-31.
- Unit statistics, costs, gather rates and every other number behind the description's targets. The Trials and Skirmish tune them. The review named the ones the rules now lean on: a shot's stone price, each template's critical-part threshold, the repair rate per section, and the warning time before a collapse.
- **The sizes behind the Worlds rules:** the daily work allowance, deposits per holding, the Deep's yield by phase and the distance the steward sallies.
