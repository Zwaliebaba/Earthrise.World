# Earthrise.World — Game Description

*A description of the whole game, written for game designers reviewing its logic. The owner accepted it on 2026-10-31, and revised it the same day after a [design review](Review/GameDescription-2026-10-31.md). [GameDesign.md](GameDesign.md) records those decisions and holds what is still Proposed or Open; where the two differ, GameDesign.md governs.*

## The pitch

Earthrise.World is a real-time strategy game about medieval lords on land made entirely of voxels. You dig your wealth out of the hills, raise your castle from the stone you quarried, and break your rivals' walls exactly where your trebuchets strike. It is played in shared worlds that last a season. From a handful of lords to two dozen, with AI houses alongside them, settle a generated land, race to excavate a deposit buried beneath its heart, and take it from one another by siege. Nothing is reset during a season. Every quarry, ditch, breach and ruin stays where it was made, and when the season ends the world is kept as a chronicle of what its players did to it.

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
| Worlds | 8–32 seats, at least half of them human lords when the world opens, AI houses in the rest; land sized to the seats, up to about 2 km × 2 km; an eight-week season |
| Population cap | 150 per faction in Skirmish, 120 per lord in Worlds |
| Resources | Provisions, timber, stone and iron; Earthstone in Worlds |
| Eras | Hall, Keep and Citadel, each built from quarried stone |
| Cultures | Masons, Wealdfolk and Delvers, each built around one material |
| Camera | An isometric strategy view that rotates in 90° steps and zooms from single stones to a whole holding |

## The objective

**In a match**, you win by breaking the enemy's keep. A keep is broken when it has stopped working and the attacker's units have held its foot for two minutes. The clock stops if they are driven off, and starts again from zero if a repair brings the keep back into working. Engines can bring a keep down, but only soldiers at its foot can break it. Until the keep is complete, the hall is the keep. A faction whose trained units are all dead, garrisoned units included, also loses.

**In a world**, you win by holding the most Earthstone when the season ends. A lord's Earthstone is all they own, wherever it is: in the keep, in a storehouse or on a cart. Earthstone is a pale, dense stone that the old kings were crowned on, and the only deposit lies in the Deep, far beneath the centre of the world. You get it in three ways. You dig for it, you haul it home on carts, and you sack the keeps of lords who hold it. Lords form banners of up to four, and the season's winner is the banner holding the most Earthstone. Each lord is also ranked. A lord without a banner is a banner of one, AI houses are ranked but cannot win, and a tie goes to the banner that reached its total first.

**Across seasons**, a lord earns a name in the chronicle: titles, heraldry and the archived world itself. A title is recorded in the archive of the world that granted it. Nothing that wins battles carries from one season to the next.

## Pillars

Every feature has to serve at least one of these. The classic RTS layer they sit on, of workers, eras and counters, is the game's chassis rather than a feature, and the test does not apply to it.

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
| Timber | Forests | Felled for timber. Burns when fire reaches it; fire arrives in the Citadel era, and in the Keep era for the Wealdfolk. |
| Rubble | What collapses leave behind | Loose: slumps like earth and can be walked over slowly. It keeps the material it fell from: rubble of building stone is cleared for stone at half its volume, and rubble of timber, earth or common rock yields nothing. |
| Earthstone | The Deep, in Worlds and the Earthstone race | Dug in blocks. Counts toward winning the season. Stays Earthstone when it falls. |

Water is part of the terrain and does not flow. Rivers and lakes are barriers with fords. A ditch stays dry.

Loose material never comes to rest on top of a standing structure, and a heap against one stays a set height below its top: earth or rubble that would rise higher slumps away. No pile of spoil or rubble ever makes a ramp over an intact wall.

### Generated maps

Every map is generated from a seed. The generator lays out valleys, ridges, cliffs, forests, rivers and lakes. It places building-stone outcrops sparsely enough that the nearest one is a positional decision, and iron veins out of sight underground. Bedrock depth varies across the map. Thin soil over rock makes strong foundations, and deep soil leaves a wall open to undermining. Every earth slope the generator makes is already stable, so the land does not shift until someone digs. Outcrops dip under the soil at their edges, so a quarry dug deeper strips earth that could have been a field.

Skirmish maps give each start position a matched budget of resources, ground and approaches: at least two outcrops within about a castle's width of the hall, on different sides, and ground that mixes thin soil over rock with deep earth. World maps are sized to their seats and divide the land into holdings of about 300 m across, each a defensible valley whose boundary follows ridges and rivers. At the centre lies the Deep: a rise of common rock with the Earthstone deposit far below it, ringed by the old fortresses of the AI houses.

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

A player never orders single voxels to be removed. They draw a designation as an area or a line, with a depth for a dig, and laborers work it in a fixed pattern.

| Designation | What it does |
|---|---|
| Quarry | Cuts building stone from an outcrop in terraces, always leaving a ramp out |
| Pit | Digs a terraced hole to a set depth through any diggable material (the Deep is dug this way) |
| Ditch | Digs a ditch along a line and piles the spoil into a rampart on the side towards the lord's keep, which the player can flip while drawing |
| Trench | Digs a narrow cut, used to undermine a wall from beside its foundation |
| Felling | Clears a forest area for timber |
| Clearance | Clears rubble for stone at half its volume |
| Demolition | Takes one of the lord's own structures apart slowly, returning its material at half its volume |

Each designation has a drop site: a quarry yard, a lumber camp, a mine head or a granary. Laborers carry loads to the nearest one, so hauling distance matters. A pattern always leaves a ramp out, by a rim the player chooses, so a laborer's own dig never strands it. In Worlds, holdings limit where a lord may draw designations (see *Holdings and siege windows*).

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

Every building is a template: a fixed shape made of voxels of set materials, with named critical parts. The player places it, and laborers build it voxel by voxel. On sloping ground, construction first levels the footprint by cut and fill. The template's foundation reaches down to firm ground. A deep foundation reaches bedrock and costs its volume in stone, its footprint times the depth to bedrock, so it is cheap on thin soil and dear on deep earth. A structure founded on bedrock cannot be undermined.

A building works until one of its critical parts falls below its threshold: a gatehouse's gate, a tower's firing platform, a keep's great tower. A part's threshold is the share of its voxels that must still stand and be supported, set for each template. Below it the building is a ruin until repaired. A building that was never finished is under construction, not a ruin.

Repair puts back voxels that were built and later removed, and pays for their material. Repairers work from the owner's side, and each section repairs at a fixed rate, however many laborers are on it. A section is one section of a wall line, or one whole building. A section hit in the last ten seconds by an engine shot, a ram blow or a sapper's trench cannot be repaired, so repairs happen between volleys, not under them. Two engines alternating on one section keep it locked; that is what concentrating engines buys.

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
| Ditch and rampart | Hall | Earth | Dug as a pair. Foot soldiers can cross both and stand on the rampart's crest; cavalry, carts, rams and engines cannot cross either. The ditch slows and channels infantry; the rampart absorbs shot and flattens instead of holing. |
| Stone wall, standard (1.5 m thick) | Keep | Stone | The everyday curtain wall |
| Stone wall, thick (2.5 m thick) | Citadel | Stone | Takes about twice as many shots to breach and costs two thirds more stone |
| Tower | Keep | Stone | Garrisons ranged units, which fire from height |
| Gatehouse | Keep | Stone, timber | Opens for its owner. The timber gate is its weak point, and the garrison fires down into the passage in front of it with crush damage that a ram's roof does not stop. |

Wall sections snap together along a line, follow the ground, and join towers and gatehouses. Sections join end to end, never face to face; a thicker wall is the thick template. **No unit ever stands on a wall.** Walls are crossed only through gates and breaches, and there are no ladders or siege towers. Towers and keeps fight through their garrisons.

## Destruction

### Craters

A hit removes a crater of voxels. The crater's size depends on the weapon and the material, and voxels carry no hit points or damage state.

| | Timber | Earth | Stone |
|---|---|---|---|
| Battering ram (contact) | Large | Small | Small, at the base only |
| Mangonel | Large | Medium, slumps closed | Small |
| Trebuchet | Very large | Large, slumps closed | Medium |

Upgrades shift these sizes. An upgrade that changes a material makes a new one: dressed stone is its own material, with smaller craters, and walls built before the upgrade keep their old stone.

### Support and collapse

A voxel stands if it rests, through solid material, on bedrock or on anything below a fixed depth, with no more than a maximum unsupported horizontal run on the way. Each material has its own span. Timber spans furthest and stone spans a little. Earth and rubble have no span: they slump to their slope instead. A wall dug out from underneath therefore falls once the gap under it is wider than stone can span.

When a piece of structure loses its support:

1. **It warns.** The piece shudders and dust falls before it goes, for long enough to walk clear of where it will land, so a taller piece warns for longer. The collapse preview shows where it will land, and an alert marks it on the minimap. A player has time to pull units out of the way.
2. **It falls** towards the side that lost support, or straight down if it lost support evenly. It lands as rubble along the line of the fall, over a length up to its own height.
3. **It damages what it lands on.** It removes voxels from structures in its path and deals crush damage to units under it.
4. **It can cascade.** If the fall knocks the support out from under another piece, that piece starts its own fall a moment later.

A collapse ends as rubble: a slope of loose stone. A fallen wall section therefore leaves a ramp into the breach, slower to cross than open ground. Dust, tumbling blocks and debris are spectacle drawn by the client. They never change what happened.

### Undermining

Undermining is open digging, never tunnelling. Sappers cut a trench beside a wall's foundation. The earth under the foundation slumps into the trench, and the wall loses support and falls. It works only where the wall stands on earth. A wall on bedrock, or on a deep foundation, cannot be undermined. A ditch in front of a wall keeps rams and engines away from its foot and holds sappers under fire while they cross it, and towers that overlook the foot make the work deadly.

### Fire

From the Citadel era, and from the Keep era for the Wealdfolk, fire arrows and burning shot set timber alight. Fire spreads through connected timber and burns it away, so a gap between buildings is a firebreak, and burning timber structures collapse. Rams, mantlets, engines and carts are timber and burn too. Stone and earth do not burn. Fire makes the choice between timber and stone sharper: timber is cheap and fast, and it is a liability once fire arrives.

## Armies

### Combat model

Combat between units is stat-based: hit points, attack, armor, range, speed and bonuses against classes. At strategy distance a hit location on a soldier is invisible, and counters keep a battle legible. There are three damage types: melee, pierce (arrows and bolts) and crush (shot, collapses and falls). Armor is rated against each.

Shots are physical. Every arrow, bolt and stone travels through the voxel grid and stops at the first solid thing it meets. A wall stops an arrow, and so does a mound of rubble or a fold of ground. Cover comes from the land itself. Arrows and bolts fly flat, so a wall between shooter and target blocks them. Engines lob their shot and strike whatever lies at the end of the arc. A shot passes units other than its target, except mantlets, which stop it. A garrison's shots start at the outer face of its tower, so a tower covers the ground at its own foot. Engines can target engines. A unit ordered to shoot a target whose line is blocked moves to the nearest place it can shoot from.

**Elevation counts in steps.** Each 3 m of height over the target is one step, up to two. Each step gives ranged attacks +15% damage and one more band of range. Shooting uphill loses the same. Steps apply to arrows and bolts, which fly flat; engines lob, and gain nothing from height.

**Garrisons.** Towers and keeps hold units. Garrisoned ranged units fire from height, and garrisoned units heal slowly. When a tower stops working, its garrison is put out at the foot of the tower, on its owner's side and clear of where the tower would fall.

**Units and the land.** Units walk on ground, wade fords, and cross rubble slowly. Foot soldiers climb slopes as steep as settled earth; cavalry, carts, rams and engines climb less, so a ditch or a rampart stops them. Units stand on ramparts, which are earth, but never on structures, and the space under a rock overhang is not walkable. A gap a unit fits through is a route. When a breach opens, every unit that is ordered inside will path through it. A unit whose ground is dug away drops to the ground below it and takes crush damage if the drop is more than 3 m. A unit buried by a collapse is placed on top of the rubble and takes the collapse's damage.

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

The counters form two linked cycles. In the Hall era: spearmen beat horse, horse beats archers, archers beat spearmen. The Keep and Citadel eras add an armored tier: men-at-arms beat the light units, crossbowmen beat armor, knights beat men-at-arms and archers, and spearmen still beat knights.

### Siege units

| Unit | Era | Cost | Role |
|---|---|---|---|
| Battering ram | Hall | Timber | Breaks gates and timber at contact and chips stone at the base. Its roof shrugs off arrows; it is weak to melee. |
| Sappers | Keep | Provisions, iron | Dig trenches at twice a laborer's speed. Armored against arrows, weak in melee. Cannot gather. |
| Mantlet | Keep | Timber | A movable shield. Shots stop at it, so it covers the units behind. Melee and engines break it. |
| Mangonel | Keep | Timber, iron | Medium range and small craters. Scatter shot against massed units. |
| Trebuchet | Citadel | Timber, iron | Long range and large craters. Must deploy and pack, and cannot hit anything close. |
| Cart | Hall | Timber | Hauls Earthstone and trade goods in Worlds. Unarmed. |

**Every engine shot costs stone** from the attacker's stockpile, just as every repaired voxel costs the defender stone. A shot is priced rather than measured: its stone is tuned so that opening a breach costs the attacker about half what rebuilding it costs the defender. A siege is fought in the economy as well as the field.

## How a siege plays

1. **Read the wall.** Scouts reveal the defenses. The integrity view shows how close each wall section is to opening, what it stands on (earth, rock or a deep foundation), and what lies in front of it.
2. **Choose the point.** Look for a thin section, a wall on earth with no ditch, the far side of the castle from its towers, or a gate. The choice is the attacker's main decision, and the land gives the reasons.
3. **Approach.** Mantlets screen the advance from tower fire. Spearmen screen the engines from cavalry sallies.
4. **Open the wall.** Trebuchets and mangonels bombard the chosen section, rams go for the gate, or sappers trench at the foot. The defender repairs between volleys, sallies against the engines, and shoots the sappers.
5. **The breach.** The section shudders and falls, crushing whoever stands under it, and leaves a ramp of rubble. Both sides' units now see a route.
6. **Storm and hold.** Men-at-arms storm up the rubble. Spearmen and archers hold the gap. The defender's second ring, if they built one, starts the siege again further in. The keep falls only when the attacker's soldiers reach its foot and hold it.

Defenders win sieges with:

- walls founded on rock;
- ditches that keep engines and sappers at a distance;
- towers that overlap their fire;
- a cavalry sally to catch the engines;
- an inner ring built in time;
- enough stone in the stockpile to keep repairing.

**Balance targets.**

- One trebuchet, unopposed, opens a walkable breach in a standard stone section in about three minutes, and in a thick section in about six.
- Repair between the volleys of one engine at most doubles those times.
- A ram breaks a palisade in under a minute, and a gatehouse's gate in about four under fire from the gatehouse's own garrison.
- No culture's walls take more than one and a half times these targets to breach.
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

- **Masons (stone).** Their stone walls build and repair faster, and they have dressed stone from the Keep era. Their unique gatehouse, the barbican, forces attackers through a second gate. They are slow and expensive in the Hall era.
- **Wealdfolk (timber).** Their timber building is faster, their timber is a material of its own that spans further, and their outriders and archers are stronger. They have fire from the Keep era, one era earlier than the others. They struggle once enemy fire and trebuchets arrive.
- **Delvers (earth and iron).** They dig faster, can train sappers from the Hall era, paying timber in place of iron, and build stronger ramparts. Iron costs them less, and their prospecting reveals veins at once. They are slow to wall in stone.

The first release ships one culture with the common roster. Worlds open with all three.

## Modes

**Trials** are single-player challenges that teach the game and test its core:

- **Siege:** take a pre-built, garrisoned castle whose defenders fight and repair.
- **Hold:** quarry and build for a set time, then survive waves of raiders that choose their breach by reading your walls and the ground beneath them. One wave brings sappers.

**Skirmish** pits one human against one to three AI houses on a generated map, in matches of 40 to 60 minutes. Every house plays for itself, and the last with a working keep wins. A second mode, the Earthstone race, plays a season's objective in an hour; it comes after the first release.

**Worlds** are the game's home: shared seasons online, described below.

## The shared world

### The season

A world runs for eight weeks in three phases:

- **Settlement, weeks 1–2.** Lords claim holdings and build. No lord can damage anything another lord owns. Raider bands test every holding's defenses during its window, more often as the weeks pass.
- **Contest, weeks 3–6.** Siege windows open to lords (below). The Deep's upper levels open to digging, and the first Earthstone moves.
- **Reckoning, weeks 7–8.** The Deep's richest levels open, and every window grows by an hour, still within the cap. At a fixed hour on the last day, inside the contest band, the world freezes: every open window closes, and the ranking is final.

### Pace

Units move and fight at full speed, and so do repair and siege fire. Gathering and new construction draw on a daily work allowance for each holding. The lord spends it at full Skirmish speed while online, and the steward spends what is left. It is sized so that a lord who plays most days reaches the Keep era in about a week and the Citadel about two weeks after that, and so that an evening's session visibly grows a castle. Deposits are sized to the allowance: a holding's outcrops and forests last into Contest, and beyond them lie contested outcrops and forests on shared ground. Battles feel like an RTS, while castles and the Deep take weeks to build and dig.

### Holdings and siege windows

Each lord holds a valley. **Inside it, nobody else can change anything except during the lord's siege window.** Each world sets a home time zone and a contest band of evening hours, for example 18:00 to 24:00. A lord's window is two hours a day and starts inside the band, at a time the lord chooses. It can be moved once a week, within the band, and a move takes effect the next day. The window grows by thirty minutes for every 25 blocks of Earthstone the lord held at any moment of the previous day, up to twelve hours, so the richer a lord gets, the more exposed they are.

Outside the window, no other faction can remove or add a voxel inside the holding, whatever the cause: shot, sappers, laborers, fire, a slump or a falling piece. A collapse that starts inside the window runs to its end. Raider bands keep the same windows as lords. Armies and carts can be attacked anywhere at any time.

**Where a lord may dig and build.** A lord draws designations and places templates only inside their own holding, and on shared ground no closer to another holding than a margin wider than any slump or fall can reach. Inside another lord's holding, during its window, an attacker may draw only trenches for sappers and clearance on rubble.

**Declared sieges.** To sack a holding, an attacker declares a siege against it a day ahead. The declaration is public, so the defender's banner can come to garrison and repair for an absent lord. An undeclared attack inside a window can damage the holding but cannot sack it.

**Outposts** outside a holding are open to attack every day during the contest band: siege camps, forts on the rim of the Deep, and roadside towers.

### The steward

While a lord is offline, their steward runs the holding. The steward is an AI that works the lord's standing orders: gathering priorities, repair policy, garrison policy, a cart order that sends Earthstone home with an escort once enough is waiting, and an alarm radius that brings laborers inside while enemies who could reach them are near. It defends: its soldiers and garrisons fight enemies inside the holding and its alarm radius, and sally against engines out to a set distance from its walls. It repairs. It never attacks another lord. The steward uses the same commands a player does. When the lord returns, it hands over a chronicle of what happened while they were away. The chronicle opens on the map, with whether the keep held, what changed and whether the stockpile will cover the next window.

### Earthstone, carts and sacking

Earthstone comes out of the Deep in blocks and travels by cart. All of it that a lord owns counts toward the season, wherever it is, and it does not count against a storehouse's capacity. Carts can be ambushed anywhere. A destroyed cart drops its blocks where it stood, for anyone to pick up.

A holding is sacked when, under a declared siege, the attacker breaks its keep during its lord's window, as in a match, and the keep is still not working when the window closes. One third of the lord's Earthstone passes at once to the faction that did the most damage to the keep. It leaves on that faction's carts, which appear at the broken keep and can be ambushed on the road like any others. A sack with no damage from an enemy takes nothing. A sacked holding cannot be sacked again for 48 hours.

### The Deep

The Deep is dug as a pit: a terraced crater that every banner works from its own side. The pit itself is shared ground, and the forts around its rim are outposts. Its levels open by phase: rock below each level's depth cannot be dug until its phase begins. As the season goes on, it becomes the largest feature on the map: an inverted fortress of terraces, ramps and rubble, fought over level by level.

### AI houses and raiders

AI houses fill the seats humans do not. They play the whole game and compete for Earthstone like any lord, and they are ranked, but an AI house cannot win the season. Their old fortresses around the Deep are their holdings, with windows in the band like any lord's, and the human lords must besiege them to reach the deposit. Raider bands are neutral attackers that test settlements in the early weeks and trouble the outskirts afterwards.

### Banners

Up to four lords form a banner. Banner members share vision, can garrison each other's towers, repair each other's walls and trade by cart through markets. Banners can agree truces with one another, which the game enforces until either side ends them with a day's notice. Membership is fixed when Contest begins: a lord may leave a banner after that, but joins no other and plays on as a banner of one.

### Upkeep

Every soldier eats provisions each day. Every stone fortification costs stone each day in proportion to its volume. A lord who cannot pay sees their walls weather: the most exposed sections lose a little of their face each day until the upkeep is met. Unpaid provisions make soldiers desert, the newest first. Unpaid upkeep does not build into a debt. Storehouses cap what a lord can stockpile. A large castle is expensive to keep standing, and a large army is expensive to keep fed.

### Limits on the strongest lord

- **Visibility:** the ranking and each lord's Earthstone count are public.
- **Exposure:** siege windows lengthen with Earthstone held, up to twelve hours.
- **Cost:** upkeep grows with every wall and soldier.
- **Banner size:** four lords at most.
- **Sacking:** the winner of a siege carries away a third, so a leader is worth attacking.
- **The calendar:** nothing that wins battles survives the end of the season.

### Joining and leaving

A world opens when enough lords have queued for it to fill at least half its seats. Its seat count, from 8 to 32, is set when it opens, and its land is sized to match. One person holds one seat. A lord can claim a holding during Settlement; after that, newcomers queue for the next world.

A lord who has not logged in for seven days retires. They leave their banner, their Earthstone is lost to the season, and their holding loses its protection and becomes a ruin anyone may quarry.

### The end of a season

At the final hour the world freezes and is archived. Its timelapse is built from a top-down image of the land taken every day: valleys emptied into castles, the Deep sinking week by week, walls rising and breaking. The archive keeps the record of every siege and its outcome. Lords keep their heraldry, their titles, such as Holder of the Deep, Wallbreaker or Unbroken, and cosmetic styles for their keeps. A title is recorded in the world's archive and names the world and its host. Then the next season begins on a new world.

### Hosting

Worlds run on dedicated servers. Communities host them first. A host sets the world's home time zone and contest band and opens the world when its queue fills, within set rules for season length, seats and windows. A world's listing names its host and any lord the host plays. Official worlds follow when the population justifies them.

### A session in a world

A lord logs in for an evening:

1. Read the steward's chronicle.
2. Adjust designations and queue construction.
3. Send carts with an escort.
4. March on a neighbor whose window is open, under a siege declared the day before.
5. Hold their own window if a siege has been declared against them.

Sessions of 30 to 90 minutes are the norm, mostly inside the contest band. A lord who plays twice a week keeps their holding and supports their banner through the steward, standing orders and banner-mates who answer declared sieges. A lord who means to win plays most evenings of Contest.

## Readability

The camera is an isometric strategy view. It rotates in 90° steps, because walls and rubble hide units, and zooms from single stones to a whole holding. The tools that make physical siege legible are part of the game, not an afterthought:

- **Integrity view:** on by default. For each wall section, how close it is to opening, read from the collapse rule: its widest hole against the width at which the stone above falls, its lowest opening against what foot soldiers can climb, and any gap under its foundation. It also shows the ground the section stands on, marks its weak point, where that reading is closest to open, and shows the repair lock on a section hit in the last ten seconds.
- **Collapse preview:** where a damaged piece will fall, before it falls, and a warning and a minimap alert when it starts to go.
- **Silhouettes:** units behind walls, rubble and terrain stay visible.
- **Passability view:** where units can walk, climb and cross, and where a breach has opened a route.
- **Fog of war:** vision is a radius, and walls do not block it. Ground you are not watching shows the land as you last saw it, so scouting reveals new quarries, ditches and breaches. An engine sees as far as it shoots, so its crew watches where its shot lands.

Controls are standard for the genre: selection, control groups, a command card, a minimap and build placement. Placement shows the cut and fill a foundation needs and the depth to rock and bedrock beneath it. For a dig, it shows where the ground will slump and which structures that would undermine. The attack cursor shows whether a target can be hit, is in range but blocked, or is out of range.

## How it is built

- **One simulation, authoritative.** Single-player runs the same simulation as a world server, in the same process. AI houses, raiders and stewards issue the same commands a player does. Every game can be replayed from its commands, in the build that recorded it.
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

Each step builds a piece the world needs. The Siege trial's defending AI, with the Skirmish AI's economy, becomes the steward, the Hold trial's raiders become the world's raiders, and the Skirmish AI becomes the AI houses.

The Trials have to show two things:

- **Where a player quarries changes how their settlement is attacked**, and players can name the trade-off they made.
- **Players choose where to breach for reasons they can state, and can predict the result.** Asked whether a section will be open after the next few shots, they are right more often than not, and they prefer this to the health-bar version.

The shared world is built only after the Trials show both, and after the season itself has been tested: an eight-week campaign played on paper, and a count of single-player players who sign up for a world.

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
- **Not an MMO.** A world is at most a few dozen lords who come to know one another, not thousands.
