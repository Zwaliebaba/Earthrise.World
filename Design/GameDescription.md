# Earthrise.World — Game Description

*A description of the whole game, written for game designers reviewing its logic. The owner accepted it on 2026-10-31. [GameDesign.md](GameDesign.md) records that decision and holds what is still Proposed or Open; where the two differ, GameDesign.md governs.*

## The pitch

Earthrise.World is a real-time strategy game about medieval lords on land made entirely of voxels. You dig your wealth out of the hills, raise your castle from the stone you quarried, and break your rivals' walls exactly where your trebuchets strike. It is played in shared worlds that last a season. Up to twenty-four lords, and AI houses alongside them, settle a generated land, race to excavate a deposit buried beneath its heart, and take it from one another by siege. Nothing is reset during a season. Every quarry, ditch, breach and ruin stays where it was made, and when the season ends the world is kept as a chronicle of what its players did to it.

Three things set it apart:

- **The economy is the terrain.** Resources are not piles on the map. They are the map. A quarry is a hole you dug, and where you dig it decides how your settlement can be attacked.
- **Siege is physical.** A wall is breached where it is hit. A breach is a hole an army walks through, a collapsing tower crushes what it falls on, and a trench dug at a wall's foot brings the wall down.
- **Worlds remember.** The land a lord ends a season with is a record of how they played, and the archived world is a record of everyone.

It is for players of classic historical RTS games who know their counters and build orders, for players who love building and breaking castles, and for players of persistent strategy who want their choices to leave a mark. It runs on Windows PCs.

## At a glance

| | |
|---|---|
| Genre | Real-time strategy in persistent, seasonal shared worlds |
| Setting | A fictional high-medieval land, before gunpowder |
| Modes | Trials (single-player siege challenges), Skirmish (against AI houses), Worlds (shared seasons online) |
| Skirmish | One human against one to three AI houses, 40–60 minutes, maps of 512 m to 768 m square |
| Worlds | 32 seats: 16–24 human lords, AI houses in the rest; about 2 km × 2 km; an eight-week season |
| Population cap | 150 per faction in Skirmish, 120 per lord in Worlds |
| Resources | Provisions, timber, stone and iron; Earthstone in Worlds |
| Eras | Hall, Keep and Citadel, each built from quarried stone |
| Cultures | Masons, Wealdfolk and Delvers, each built around one material |
| Camera | An isometric strategy view that rotates in 90° steps and zooms from single stones to a whole holding |

## The objective

**In a match**, you win by breaking the enemy's keep. A keep is broken when it has been damaged until it stops working and is not repaired within two minutes. A faction whose units are all dead also loses.

**In a world**, you win by holding the most Earthstone in your keep when the season ends. Earthstone is a pale, dense stone that the old kings were crowned on, and the only deposit lies in the Deep, far beneath the centre of the world. You get it in three ways. You dig for it, you haul it home on carts, and you sack the keeps of lords who hold it. Lords form banners of up to four, and the season's winner is the banner holding the most Earthstone. Each lord is also ranked.

**Across seasons**, a lord earns a name in the chronicle: titles, heraldry and the archived world itself. Nothing that wins battles carries from one season to the next.

## Pillars

Every feature has to serve at least one of these.

1. **Where you dig decides how you can be attacked.** Resources sit in finite deposits at fixed places. Building stone lies in outcrops, iron ore in underground veins, and timber in forests that stay cut for the life of the world. A quarry dug in front of your wall is a ditch the attacker has to cross. A quarry dug in earth beside your wall is a slope that can slump your foundation away.
2. **Walls fall where they are struck.** No wall has a health bar. A hit removes a crater. A structure stands only where it is supported, and a piece that loses its support falls. Breaching is a choice of where, and defending is a choice of what to build, how thick, and on what ground.
3. **The land remembers for the life of a world.** Quarries, ditches, ruins and breaches last until the season ends. Then the world is archived, not deleted, and its timelapse shows the land being dug out and fought over.

## The land

### Materials

Everything solid is made of voxels small enough that a crater, a ditch or a breach has the shape of whatever made it. Each voxel is one material, and the materials behave differently.

| Material | Where it is | Behavior |
|---|---|---|
| Earth | The soil over the rock, of varying depth | Dug quickly. Slumps to a maximum slope, so earth never stands steeper than that. Absorbs shot: craters slump closed, and earthworks flatten instead of holing. |
| Common rock | Under the earth and in cliffs | Dug slowly and yields nothing. Holds vertical faces and overhangs up to its span. |
| Building stone | Sparse outcrops | Quarried for stone. The only source of new stone besides rubble. |
| Bedrock | Deep down, nearer the surface in places | Cannot be dug. Anything founded on it cannot be undermined. |
| Iron ore | Veins in the rock, hidden until prospected | Dug at a mine head and smelted into iron. |
| Timber | Forests | Felled for timber. Burns from the Citadel era on. |
| Rubble | What collapses leave behind | Loose: slumps like earth, can be walked over slowly, and is cleared for stone at half its volume. |
| Earthstone | The Deep, in Worlds only | Dug in blocks. Counts toward winning the season. |

Water is part of the terrain and does not flow. Rivers and lakes are barriers with fords. A ditch stays dry.

### Generated maps

Every map is generated from a seed. The generator lays out valleys, ridges, cliffs, forests, rivers and lakes. It places building-stone outcrops sparsely enough that the nearest one is a positional decision, and iron veins out of sight underground. Bedrock depth varies across the map. Thin soil over rock makes strong foundations, and deep soil leaves a wall open to undermining. Every earth slope the generator makes is already stable, so the land does not shift until someone digs.

Skirmish maps give each start position a matched budget of resources, ground and approaches. World maps divide the land into holdings of about 300 m across, each a defensible valley. At the centre lies the Deep: a rise of common rock with the Earthstone deposit far below it, ringed by the old fortresses of the AI houses.

## The economy

### Resources

| Resource | Source | Counted in | Pays for |
|---|---|---|---|
| Provisions | Fields on flat earth | Provisions | Training every unit. In Worlds, soldiers' daily upkeep. |
| Timber | Felling forests | Cubic metres | Timber buildings, palisades, engines, mantlets, carts |
| Stone | Quarrying outcrops; clearing rubble at half yield | Cubic metres | Stone buildings and walls, era advances, siege shot, repair |
| Iron | Ore from veins, smelted at a forge | Bars | Armored units, crossbows, engine fittings, upgrades |
| Earthstone | The Deep (Worlds) | Blocks | Winning the season |

**Stone and timber are counted by volume, and buildings cost exactly the volume they are made of.** A stone wall section of 8 m × 1.5 m × 6 m costs 72 m³ of stone. Quarrying gives back exactly the volume removed. The pit beside a castle is visibly the size of the castle, and a besieged lord's rubble is the besieger's next quarry.

**Earth is never stockpiled.** Digging a ditch piles its spoil into a rampart placed with the ditch, so the two are built together.

**Provisions are the one resource that is not dug.** Fields need flat earth. A quarry or ditch cut through a field destroys it, and fields cannot be placed on rock. Farmland, quarries and buildings compete for the same flat ground, and every lord decides how to use their valley.

### Gathering by designation

A player never orders single voxels to be removed. They draw a designation with a footprint and a depth, and laborers work it in a fixed pattern.

| Designation | What it does |
|---|---|
| Quarry | Cuts building stone from an outcrop in terraces, always leaving a ramp out |
| Pit | Digs a terraced hole to a set depth through any diggable material (the Deep is dug this way) |
| Ditch | Digs a ditch along a line and piles the spoil into a rampart on the near side |
| Trench | Digs a narrow cut, used to undermine a wall from beside its foundation |
| Felling | Clears a forest area for timber |
| Clearance | Clears rubble for stone at half its volume |
| Demolition | Takes a structure apart slowly. Used inside captured ground. |

Each designation has a drop site: a quarry yard, a lumber camp, a mine head or a granary. Laborers carry loads to the nearest one, so hauling distance matters. A pattern never leaves a hole without a way out, and the game never strands a laborer.

**Prospecting.** Iron veins stay hidden until a laborer prospects the ground around them or a mine head is sunk nearby. Rivals cannot see where your veins are, or where theirs are, without prospecting too.

### Population

Housing sets the population cap: cottages, plus the hall and its successors. The cap is 150 in Skirmish and 120 in Worlds. Every unit costs provisions to train. In Worlds, soldiers also eat provisions each day (see Upkeep).

### Economic tempo in a match

Skirmish is tuned to these timings:

- **Minutes 0–10, Hall era.** Fields, felling, the first quarry, scouting. Palisades, ditches and ramparts. Outriders raid laborers.
- **Minutes 10–15.** The first lords advance to the Keep era by building their keep around the hall.
- **Minutes 15–25.** Stone walls and towers go up. Rams and mangonels break the first palisades.
- **Minutes 25–30.** The first lords advance to the Citadel era. Trebuchets and knights appear.
- **Minutes 30–60.** Decisive sieges.

## Building

### Templates

Every building is a template: a fixed shape made of voxels of set materials, with named critical parts. The player places it, and laborers build it voxel by voxel. On sloping ground, construction first levels the footprint by cut and fill. The template's foundation reaches down to firm ground. A deep foundation reaches bedrock at extra cost, and a structure founded on bedrock cannot be undermined.

A building works until one of its critical parts falls below its threshold: a gatehouse's gate, a tower's firing platform, a keep's great tower. It is then a ruin until repaired. Repair puts back missing voxels and pays for their material. A section hit in the last ten seconds cannot be repaired. Repairs happen between volleys, not under them.

Players build only from templates, never voxel by voxel. The skill is in placement, sequence and layering: which wall goes on rock and which on earth, how thick, what lies in front, and where the second ring goes.

### Buildings

| Building | Era | Material | Role |
|---|---|---|---|
| Hall → Keep → Citadel | Hall, Keep, Citadel | Timber, then stone | Trains laborers, drop site for everything, advances the era, garrisons and fires. Losing it loses the match. |
| Cottage | Hall | Timber | Housing |
| Granary and fields | Hall | Timber, earth | Provisions |
| Lumber camp | Hall | Timber | Timber drop site |
| Quarry yard | Hall | Timber | Stone drop site; cuts siege shot |
| Barracks | Hall | Timber | Infantry |
| Archery butts | Hall | Timber | Archers, later crossbowmen |
| Stable | Hall | Timber | Cavalry |
| Mine head and forge | Keep | Stone | Iron drop site and smelting |
| Smithy | Keep | Stone | Weapon and armor upgrades |
| Engineers' yard | Hall | Timber | Rams, then sappers, mantlets, mangonels, trebuchets |
| Storehouse | Hall | Timber | Stockpile capacity in Worlds |
| Market | Keep | Stone | Trade between allied lords by cart (Worlds) |

### Fortifications

| Fortification | Era | Material | Character |
|---|---|---|---|
| Palisade | Hall | Timber | Cheap and fast, with its own timber gates. Rams and engines shatter it; fire destroys it. |
| Ditch and rampart | Hall | Earth | Dug as a pair. The ditch slows and channels attackers; the rampart absorbs shot and flattens instead of holing. A rampart alone is a slope an army can climb. |
| Stone wall, standard (1.5 m thick) | Keep | Stone | The everyday curtain wall |
| Stone wall, thick (2.5 m thick) | Citadel | Stone | Takes about twice as many shots to breach and costs two thirds more stone |
| Tower | Keep | Stone | Garrisons ranged units, which fire from height |
| Gatehouse | Keep | Stone, timber | Opens for its owner. The timber gate is its weak point. |

Wall sections snap together along a line, follow the ground, and join towers and gatehouses. **No unit ever stands on a wall.** Walls are crossed only through gates and breaches, and there are no ladders or siege towers. Towers and keeps fight through their garrisons.

## Destruction

### Craters

A hit removes a crater of voxels. The crater's size depends on the weapon and the material, and voxels carry no hit points or damage state.

| | Timber | Earth | Stone |
|---|---|---|---|
| Battering ram (contact) | Large | Small | Small, at the base only |
| Mangonel | Large | Medium, slumps closed | Small |
| Trebuchet | Very large | Large, slumps closed | Medium |

Upgrades shift these sizes. For example, dressed stone makes craters in stone smaller.

### Support and collapse

A voxel stands if it rests, through solid material, on bedrock or on anything below a fixed depth, with no more than a maximum unsupported horizontal run on the way. Each material has its own span. Timber spans furthest and stone spans a little. Earth and rubble have no span: they slump to their slope instead. A wall dug out from underneath therefore falls once the gap under it is wider than stone can span.

When a piece of structure loses its support:

1. **It warns.** The piece shudders and dust falls for a moment before it goes, and the collapse preview shows where it will land. A player has time to pull units out of the way.
2. **It falls** towards the side that lost support, or straight down if it lost support evenly. It lands as rubble along the line of the fall, over a length up to its own height.
3. **It damages what it lands on.** It removes voxels from structures in its path and deals crush damage to units under it.
4. **It can cascade.** If the fall knocks the support out from under another piece, that piece starts its own fall a moment later.

A collapse ends as rubble: a slope of loose stone. A fallen wall section therefore leaves a ramp into the breach, slower to cross than open ground. Dust, tumbling blocks and debris are spectacle drawn by the client. They never change what happened.

### Undermining

Undermining is open digging, never tunnelling. Sappers cut a trench beside a wall's foundation. The earth under the foundation slumps into the trench, and the wall loses support and falls. It works only where the wall stands on earth. A wall on bedrock, or on a deep foundation, cannot be undermined. A ditch in front of a wall keeps sappers away from its foot, and towers that overlook the foot make the work deadly.

### Fire

From the Citadel era, fire arrows and burning shot set timber alight. Fire spreads through connected timber and burns it away, and burning timber structures collapse. Stone and earth do not burn. Fire makes the choice between timber and stone sharper: timber is cheap and fast, and it is a liability once fire arrives.

## Armies

### Combat model

Combat between units is stat-based: hit points, attack, armor, range, speed and bonuses against classes. At strategy distance a hit location on a soldier is invisible, and counters keep a battle legible. There are three damage types: melee, pierce (arrows and bolts) and crush (shot, collapses and falls). Armor is rated against each.

Shots are physical. Every arrow, bolt and stone travels through the voxel grid and stops at the first solid thing it meets. A wall stops an arrow, and so does a mound of rubble or a fold of ground. Cover comes from the land itself. Arrows and bolts fly flat, so a wall between shooter and target blocks them. Engines lob their shot and strike whatever lies at the end of the arc.

**Elevation counts in steps.** Each 3 m of height over the target is one step, up to two. Each step gives ranged attacks +15% damage and one more band of range. Shooting uphill loses the same.

**Garrisons.** Towers and keeps hold units. Garrisoned ranged units fire from height, and garrisoned units heal slowly. When a tower stops working, its garrison is put out at the foot of the tower.

**Units and the land.** Units walk on ground, climb slopes up to a limit, wade fords, and cross rubble slowly. A gap a unit fits through is a route. When a breach opens, every unit that is ordered inside will path through it. A unit whose ground is dug away drops to the ground below it and takes crush damage if the drop is more than 3 m. A unit buried by a collapse is placed on top of the rubble and takes the collapse's damage.

**Soldiers never change the land.** Only engines, sappers, laborers on a designation, collapses and fire remove voxels.

### Roster

| Unit | Era | Cost | Role | Strong against | Weak against |
|---|---|---|---|---|---|
| Laborer | Hall | Provisions | Gathers, builds, repairs, digs | — | Everything |
| Spearmen | Hall | Provisions, timber | Cheap line infantry, holds against horse | Outriders, knights | Archers, men-at-arms |
| Archers | Hall | Provisions, timber | Long-ranged foot | Spearmen, crossbowmen | Outriders, knights, men-at-arms |
| Outriders | Hall | Provisions, timber | Fast light horse: scouting, raids, engine hunting | Archers, crossbowmen, engines, laborers | Spearmen, men-at-arms |
| Men-at-arms | Keep | Provisions, iron | Armored foot that holds and storms breaches | Spearmen, archers, outriders | Crossbowmen, knights |
| Crossbowmen | Keep | Provisions, timber, iron | Armor-piercing, shorter range than archers | Men-at-arms, knights | Outriders, archers |
| Knights | Citadel | Provisions, iron (high) | Armored shock cavalry | Archers, men-at-arms | Spearmen, crossbowmen |

The counters form two linked cycles. In the Hall era: spearmen beat horse, horse beats archers, archers beat spearmen. The Keep and Citadel eras add an armored tier: men-at-arms beat the light units, crossbowmen beat armor, knights beat men-at-arms and shooters, and spearmen still beat knights.

### Siege units

| Unit | Era | Cost | Role |
|---|---|---|---|
| Battering ram | Hall | Timber | Breaks gates and timber at contact and chips stone at the base. Its roof shrugs off arrows; it is weak to melee. |
| Sappers | Keep | Provisions, iron | Dig trenches at twice a laborer's speed. Armored against arrows, weak in melee. Cannot gather. |
| Mantlet | Keep | Timber | A movable shield. Shots stop at it, so it covers the units behind. Melee and engines break it. |
| Mangonel | Keep | Timber, iron | Medium range and small craters. Scatter shot against massed units. |
| Trebuchet | Citadel | Timber, iron | Long range and large craters. Must deploy and pack, and cannot hit anything close. |
| Cart | Hall | Timber | Hauls Earthstone and trade goods in Worlds. Unarmed. |

**Every engine shot costs stone** from the attacker's stockpile, just as every repaired voxel costs the defender stone. A siege is fought in the economy as well as the field.

## How a siege plays

1. **Read the wall.** Scouts reveal the defenses. The integrity view shows each wall section's thinnest remaining cross-section, what it stands on (earth, rock or a deep foundation), and what lies in front of it.
2. **Choose the point.** Look for a thin section, a wall on earth with no ditch, the far side of the castle from its towers, or a gate. The choice is the attacker's main decision, and the land gives the reasons.
3. **Approach.** Mantlets screen the advance from tower fire. Outriders screen the engines from sallies.
4. **Open the wall.** Trebuchets and mangonels bombard the chosen section, rams go for the gate, or sappers trench at the foot. The defender repairs between volleys, sallies against the engines, and shoots the sappers.
5. **The breach.** The section shudders and falls, crushing whoever stands under it, and leaves a ramp of rubble. Both sides' units now see a route.
6. **Storm and hold.** Men-at-arms storm up the rubble. Spearmen and archers hold the gap. The defender's second ring, if they built one, starts the siege again further in.

Defenders win sieges with:

- walls founded on rock;
- ditches that keep engines and sappers at a distance;
- towers that overlap their fire;
- a cavalry sally to catch the engines;
- an inner ring built in time;
- enough stone in the stockpile to keep repairing.

**Balance targets.**

- One trebuchet, unopposed, opens a walkable breach in a standard stone section in about three minutes, and in a thick section in about six.
- Repair between volleys at most doubles those times.
- A ram breaks a palisade in under a minute and a gate in about two.
- At equal spending, the defender holds a modest edge. Walls buy time; they do not make a lord invulnerable.

## Technology

Advancing an era means building it. The Keep era begins when the keep, built in stone around the hall, is complete. The Citadel era begins when the citadel's great tower is complete. Era advances are paid in quarried stone, so they show on the land.

| Era | Unlocks |
|---|---|
| Hall | Laborers, spearmen, archers, outriders, battering rams. Timber buildings, palisades, ditches and ramparts. |
| Keep | Men-at-arms, crossbowmen, sappers, mantlets, mangonels. Stone walls, towers, gatehouses. Iron, the smithy, the market, prospecting, deep foundations. |
| Citadel | Knights and trebuchets. Thick walls, fire arrows and burning shot. Late upgrades. |

Upgrades fall into five lines:

- **Economy:** better tools for faster digging, carts that carry more, and better fields.
- **Arms and armor:** three smithy tiers for each damage type.
- **Masonry:** dressed stone for smaller craters in stone, and longer spans.
- **Engineering:** engine range, deployment speed and repair speed.
- **Fire:** from the Citadel era.

## Cultures

Each culture is built around one of the three building materials. All share the common roster, with these differences:

- **Masons (stone).** Stone walls cost less and repair faster, and they have dressed stone from the Keep era. Their unique gatehouse, the barbican, forces attackers through a second gate. They are slow and expensive in the Hall era.
- **Wealdfolk (timber).** Timber building is faster and cheaper, timber spans further, and their outriders and archers are stronger. They have fire from the Keep era, one era earlier than the others. They struggle once enemy fire and trebuchets arrive.
- **Delvers (earth and iron).** They dig faster, can train sappers from the Hall era, and build stronger ramparts. Iron costs them less, and their prospecting reveals veins at once. They are slow to wall in stone.

The first release ships one culture with the common roster. Worlds open with all three.

## Modes

**Trials** are single-player challenges that teach the game and test its core:

- **Siege:** take a pre-built, garrisoned castle whose defenders fight and repair.
- **Hold:** quarry and build for a set time, then survive waves of raiders that choose their breach by reading your walls.

**Skirmish** pits one human against one to three AI houses on a generated map, in matches of 40 to 60 minutes. The usual victory is breaking the enemy keep. A second mode, the Earthstone race, plays a season's objective in an hour.

**Worlds** are the game's home: shared seasons online, described below.

## The shared world

### The season

A world runs for eight weeks in three phases:

- **Settlement, weeks 1–2.** Lords claim holdings and build. No lord can damage another lord's structures. Raider bands test every holding's defenses, more often as the weeks pass.
- **Contest, weeks 3–6.** Siege windows open (below). The upper levels of the Deep are reached, and the first Earthstone moves.
- **Reckoning, weeks 7–8.** The Deep's richest levels open, and every window grows by an hour. At a fixed hour on the last day the world freezes and the ranking is final.

### Pace

Units move and fight at full speed, and so do repair and siege fire. Gathering and new construction run at a quarter of Skirmish speed. Battles feel like an RTS, while castles and the Deep take weeks to build and dig.

### Holdings and siege windows

Each lord holds a valley. **Inside it, their structures can be damaged only during their siege window**: two hours a day, at a time the lord chooses. The window can be moved once a week, and a move takes effect the next day. The window grows by thirty minutes for every 25 blocks of Earthstone in the keep, so the richer a lord gets, the more exposed they are. Outside the window, engines, sappers and fire cannot harm the holding's structures. Armies and carts can be attacked anywhere at any time.

**Outposts** outside a holding are always open to attack: siege camps, forts on the rim of the Deep, and roadside towers.

### The steward

While a lord is offline, their steward runs the holding. The steward is an AI that works the lord's standing orders: gathering priorities, repair policy, garrison policy and an alarm radius that brings laborers inside when enemies come near. During the window it defends and repairs, but it never attacks. The steward uses the same commands a player does. When the lord returns, it hands over a chronicle of what happened while they were away.

### Earthstone, carts and sacking

Earthstone comes out of the Deep in blocks and travels by cart to a lord's keep, where it counts toward the season. Carts can be ambushed anywhere. If a keep stops working during its lord's window and is still broken when the window closes, the holding is sacked. The attacker carries off one third of its Earthstone by cart, and the sacked holding gets a 48-hour truce in both directions.

### The Deep

The Deep is dug as a pit: a terraced crater that every banner works from its own side. The pit itself is shared ground, and the forts around its rim are outposts. As the season goes on, it becomes the largest feature on the map: an inverted fortress of terraces, ramps and rubble, fought over level by level.

### AI houses and raiders

AI houses fill the seats humans do not. They play the whole game and compete for Earthstone like any lord. They also hold the old fortresses around the Deep, which the human lords must besiege to reach the deposit. Raider bands are neutral attackers that test settlements in the early weeks and trouble the outskirts afterwards.

### Banners

Up to four lords form a banner. Banner members share vision, can garrison each other's towers, repair each other's walls and trade by cart through markets. Banners can agree truces with one another, which the game enforces until either side ends them with a day's notice.

### Upkeep

Every soldier eats provisions each day. Every fortification costs stone each day in proportion to its volume. A lord who cannot pay sees their walls weather: the most exposed sections lose a little of their face each day until the upkeep is met. Storehouses cap what a lord can stockpile. A large castle is expensive to keep standing, and a large army is expensive to keep fed.

### Limits on the strongest lord

- **Visibility:** the ranking and each keep's Earthstone count are public.
- **Exposure:** siege windows lengthen with Earthstone held.
- **Cost:** upkeep grows with every wall and soldier.
- **Banner size:** four lords at most.
- **Sacking:** the winner of a siege carries away a third, so a leader is worth attacking.
- **The calendar:** nothing that wins battles survives the end of the season.

### Newcomers

A lord can claim a holding during Settlement. After that, newcomers join a newer world. Worlds open on a regular schedule, so there is always one in Settlement.

### The end of a season

At the final hour the world freezes and is archived. Its timelapse is built from a snapshot of the land taken every day: valleys emptied into castles, the Deep sinking week by week, walls rising and breaking. The archive keeps the record of every siege and its outcome. Lords keep their heraldry and titles, such as Holder of the Deep, Wallbreaker or Unbroken, and cosmetic styles for their keeps. Then the next season begins on a new world.

### Hosting

Worlds run on dedicated servers. Communities host them first, choosing their own schedule within set rules for season length and windows. Official worlds follow when the population justifies them.

### A session in a world

A lord logs in for an evening:

1. Read the steward's chronicle.
2. Adjust designations and queue construction.
3. Send carts with an escort.
4. March on a neighbor whose window is open.
5. Hold their own window if a rival is expected.

Sessions of 30 to 90 minutes are the norm. A lord who plays twice a week stays in the race through the steward, standing orders and their banner.

## Readability

The camera is an isometric strategy view. It rotates in 90° steps, because walls and rubble hide units, and zooms from single stones to a whole holding. The tools that make physical siege legible are part of the game, not an afterthought:

- **Integrity view:** each wall section's thinnest remaining cross-section, the ground it stands on, and its weak points.
- **Collapse preview:** where a damaged piece will fall, before it falls, and a visible warning when it starts to go.
- **Silhouettes:** units behind walls and rubble stay visible.
- **Passability view:** where units can walk, climb and cross, and where a breach has opened a route.
- **Fog of war:** vision is a radius, and walls do not block it. Ground you are not watching shows the land as you last saw it, so scouting reveals new quarries, ditches and breaches.

Controls are standard for the genre: selection, control groups, a command card, a minimap and build placement. Placement shows the cut and fill a foundation needs. Designations are drawn as a footprint and a depth.

## How it is built

- **One simulation, authoritative.** Single-player runs the same simulation as a world server, in the same process. AI houses, raiders and stewards issue the same commands a player does. Every game can be replayed from its commands.
- **The world is a seed plus its edits.** A generated world is rebuilt from its seed, and only what players changed is stored. Saves are small, and archiving a finished world is cheap. Where resources lie is kept on the server, so a modified client cannot find hidden veins.
- **Destruction is resolved by rules in the simulation.** Outcomes are predictable and the same for every player. Tumbling debris is drawn locally and never matters.
- **Voxels are drawn directly, without meshing**, using ray-casting after Majercik et al. (2018). An edit shows on screen in the next frame.
- **Platform:** Windows and Direct3D 12, in C++, on an engine written for this game.

## How the design is proven

The game is built in the order that tests it:

1. **A renderer prototype** shows the land, a forest and a walled town, and measures the cost of drawing and editing them.
2. **The Trials** test the pillars, against a version of the same game with health-bar walls and resource nodes.
3. **Skirmish.**
4. **Worlds.**

Each step builds a piece the world needs. The Siege trial's defending AI becomes the steward, the Hold trial's raiders become the world's raiders, and the Skirmish AI becomes the AI houses.

The Trials have to show two things:

- **Where a player quarries changes how their settlement is attacked**, and players can name the trade-off they made.
- **Players choose where to breach for reasons they can state, and can predict the result.** Asked whether a section will be open after the next few shots, they are right more often than not, and they prefer this to the health-bar version.

The shared world is built only after the Trials show both.

## Lineage

| Game | What Earthrise.World takes | What it changes |
|---|---|---|
| Age of Empires II | The worker economy, eras and counters, and the feel of a match | Resources are the terrain, and eras are built from quarried stone |
| Stronghold (2001) | Castle building and sieges against a built castle | Walls break where they are struck, and the castle is dug out of the map |
| Teardown | Voxel destruction and its look | Collapse is a rule read from a strategy camera; debris is spectacle |
| Rust | Persistent destructible bases and periodic wipes | Seasons with an objective and an archive, and siege windows instead of offline raids |
| Foxhole | Persistent wars that end and begin again | A season ends in a reckoning, and the world is archived rather than discarded |
| EVE Online | Structures whose owners choose when they can be attacked | Daily siege windows per holding that grow with what the lord holds |
| Travian | Servers that end in a race to a shared goal, and late joiners sent to newer servers | The race is won by excavation and siege |

## What it is not

- **Not a physics sandbox.** Outcomes are rules a player can learn and predict.
- **Not a free-form builder.** Every structure is a template, and the skill is in how templates are placed and combined.
- **Not about climbing walls.** No unit stands on a wall, and walls are crossed through gates and breaches.
- **Not about tunnelling.** All digging is open to the sky.
- **Not gunpowder.** The strongest weapon is the trebuchet.
- **Not a world without end.** Every world has a season, and every season has a winner.
- **Not an MMO.** A world is dozens of lords who come to know one another, not thousands.
