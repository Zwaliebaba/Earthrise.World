---
description: Adversarial multi-agent review of Design/GameDescription.md. Nine lens reviewers, three verifiers, one ranked report.
argument-hint: "[report path; default Design/Review/GameDescription-<YYYY-MM-DD>.md]"
---

# Review Design/GameDescription.md from nine angles

You are the orchestrator of an adversarial design review of `Design/GameDescription.md`. Nine reviewers each attack the design from one angle. Three verifiers then try to refute what the reviewers found, and you merge what survives into one report for the owner. The run starts twelve subagents and uses a lot of tokens. That cost is the point of the run, so do not shortcut it by reviewing alone.

## What this review is for

The owner wants two things: a design that is solid, and a game that can succeed commercially at scale. No review can establish the second. A review can find what would stop it: contradictions, rules that do not add up, dominant strategies, exploits, scope the team cannot carry, and assumptions about players and the market that nothing tests before the expensive part is built. Every reviewer looks for the weakest point first. Praise is not a finding.

## Ground rules for you

1. Do not modify any tracked file, and do not commit. The only file you write is the report. Changes to the design are the owner's to make (AGENTS.md: a design question goes to the owner, and the answer is written down before any code).
2. Before Phase 1, record `git rev-parse --short HEAD` and today's date. The report cites both.
3. Give each reviewer the **Common brief** and its **lens brief** word for word. Add nothing from your own reading of the design. A reviewer handed your hypothesis tends to return it confirmed.
4. Launch the nine reviewers in a single message so they run concurrently. Use `general-purpose` agents: they need Read and Grep, and MARKET also needs WebSearch and WebFetch.
5. While they run, read the source files yourself. You need them to merge the findings and write the report.
6. If a reviewer's output ignores the finding format, use what it does say. Do not invent the missing fields.

## Phase 1: lens reviews

### Common brief (send to every reviewer, word for word)

> You are one of nine independent reviewers of the design of Earthrise.World. Each reviewer attacks it from one angle, and yours is in the lens brief below. Your job is adversarial: find the weakest points first, and report what breaks, not what works.
>
> **Read these in full, in this order.** First `Design/GameDescription.md`, the subject of the review. Then `Design/GameDesign.md`, which governs where the two differ and marks every item Decided, Proposed or Open. Then `Design/ADR/ADR-002-authoritative-simulation.md`, `Design/ADR/ADR-003-world-as-seed-plus-edits.md` and `Design/ADR/ADR-004-rule-based-destruction.md`, and `Design/ADR/ADR-006-voxel-sizes.md` for the voxel sizes. For the constraints on how the game is built, read the opening of `AGENTS.md`, its R14 and its §6. Read `Design/ADR/ADR-001-direct-voxel-ray-casting.md` too if your lens touches rendering. Quote what this design says. Do not fill gaps from memory of other games.
>
> **Facts that frame every finding.**
> - No code exists. Every number in the description is a design target to be tuned in the Trials and Skirmish, not a measurement.
> - AGENTS.md describes a hobby project with one developer, on an engine written for this game. R14 allows no third-party dependency: no physics engine, no networking library, and no platform SDK such as Steam's.
> - The product is the shared seasonal world (Worlds), and it is built last. First come a renderer spike, then two single-player Trials tested against a health-bar control, then Skirmish.
> - The owner has accepted the description in full. A finding against a Decided item is allowed and wanted. Label it, because reversing a Decided item is the owner's call.
> - The items GameDesign.md lists as Open are already known. Do not report "X is not specified" for one of them unless the gap hides a risk you can name.
>
> **How to work.**
> - Ground every finding in the text, with an exact quote and its `file:line`. Where the documents are silent and a rule is needed, say "silent" and name the section that should hold it.
> - Prefer a concrete failure scenario to an abstract worry: a specific situation in play, step by step, ending in the bad outcome.
> - Where numbers are involved, do the arithmetic and show it. If a number you need is not in the documents, state the value you assume and the range over which your conclusion holds.
> - A recommendation that adds scope must say what it displaces. "Add more content" is not a fix.
> - If a probe in your brief no longer matches the document, say so in one line and move on. The probes are a floor, not a ceiling. Your most valuable finding may be one nobody listed.
> - Do not write or edit any file. Return your findings as your final message.
>
> **Return** up to 12 findings, most severe first. After them, list up to 3 load-bearing elements, with one sentence each on why. A load-bearing element is something that carries the design's value and must survive any fix. Use exactly this format for each finding:
>
> ```
> ### <LENS>-<n>: <the claim, in one line>
> - Severity: Blocker | Major | Minor
> - Kind: contradiction | gap | dominant-strategy | exploit | numbers | experience | feasibility | market
> - Bites at: Spike | Trials | Skirmish | Worlds | Launch
> - Text status: Decided | Proposed | Open | Silent
> - Evidence: "<exact quote>" (<file>:<line>); more quotes as needed
> - Failure scenario: <concrete, step by step>
> - Working: <arithmetic, graph or model, assumptions labelled; "none" if not applicable>
> - Smallest fix: <the least change that removes the problem; what it displaces if it adds scope>
> - Cheapest test: <what settles it before code: a spreadsheet, a paper prototype, a Trial build, a playtest question>
> - Confidence: High | Medium | Low; <what would change your mind>
> ```
>
> **Severity.** A **Blocker** must be resolved before the stage it bites at begins, or that stage cannot succeed. A **Major** makes the game measurably worse, or puts a pillar or a hypothesis at risk. A **Minor** is an ambiguity or a tuning question.

### Lens briefs

Each lens brief follows the Common brief in its reviewer's prompt. The lens name is the finding prefix.

#### CORE: the loop and the pillars

> **Mission.** Find out whether the three pillars produce real decisions minute to minute, and whether the core loop is fun before any persistence exists. The Trials and Skirmish have to stand on their own. If they are not fun, Worlds never gets built.
>
> **Probe at least:**
> - Play the first fifteen minutes of a Skirmish in your head against *Economic tempo in a match*. What is the player deciding in each minute? Which decisions are real choices, and which does the map force?
> - Pillar 1 relies on outcrops placed "sparsely enough that the nearest one is a positional decision" (*Generated maps*). With sparse, fixed deposits, how often is there more than one sensible quarry site? When there is only one, pillar 1 is a given, not a decision.
> - Look for a dominant defensive recipe, such as a wall on bedrock or a deep foundation, behind a ditch, inside overlapping towers. Once a player learns it, does it make the decisions pillars 1 and 2 promise moot?
> - Pillar 2 promises that "breaching is a choice of where". Given the gate as a weak point, the ram, and the balance targets in *How a siege plays*, does a typical castle offer more than one viable breach, or does one option dominate?
> - The pitch is aimed at "players who love building and breaking castles", yet building is template-only. Are placement, sequence and layering enough expression to deliver that fantasy?
> - *What it is not* removes wall-walks, ladders, siege towers and tunnels. What does each removal cost the defender's agency during an assault, and what replaces it?
> - State the player fantasy in one sentence. Does each mode (Trials, Skirmish, Worlds) deliver it?
> - Apply the pillar rule, "every feature has to serve at least one of these", and list any feature that serves none.
>
> **Leave to others:** numbers (ECON), counters and cultures (COMBAT), loopholes (EXPLOIT), the shared world (WORLDS).

#### ECON: systems and numbers

> **Mission.** Find out whether the economy and the siege numbers add up. Do the arithmetic the description does not.
>
> **Probe at least:**
> - Volume equals cost (*Resources*). Estimate the stone volume of the Hall-to-Keep advance, of a curtain-wall ring with towers and a gatehouse, and of the citadel's great tower. Back-solve the stone each laborer must deliver per minute for the timings in *Economic tempo in a match* to hold within the population cap of 150. Allow for an army and for hauling distance (*Gathering by designation*). Is that rate plausible? Does a Hall-era start hold the stone the keep needs?
> - The Worlds pace rule (*Pace*) runs gathering and construction at a quarter of Skirmish speed, and the steward gathers while its lord is offline (*The steward*). Compute output per day, and over eight weeks, in Skirmish-match equivalents. Set it against "castles and the Deep take weeks to build and dig". Which constraint actually binds: the rate, finite deposits, storehouse caps or upkeep? Are deposit sizes stated anywhere?
> - Forests stay cut, and outcrops are finite. In an eight-week world, when does a holding run out of timber and stone, and what does a lord do then?
> - The siege economy has four rules. Every engine shot costs stone. Every repaired voxel costs stone. A section hit in the last ten seconds cannot be repaired. And "repair between volleys at most doubles" the breach times. What engine fire interval and repair rate make the balance targets true together? Is "takes about twice as many shots to breach and costs two thirds more stone" for thick walls consistent with the crater table?
> - Upkeep is paid in stone in proportion to fortification volume (*Upkeep*). At a plausible rate, what fraction of a holding's quarry output does a large castle consume? Does upkeep make the castle that grows over weeks self-defeating?
> - Siege windows grow by thirty minutes per 25 blocks of Earthstone, and no cap is stated. At what holding is a lord open all day? How many blocks must the Deep hold for the season objective to work, and does the window rule bite at that scale?
> - Rubble clears for stone at half its volume, and "a besieged lord's rubble is the besieger's next quarry". Who can actually clear it, and who profits?
> - Worlds allow 120 population per lord, and every soldier eats provisions daily. What army can a valley of fields sustain?
>
> **Leave to others:** counter balance (COMBAT), loopholes (EXPLOIT).

#### COMBAT: counters, siege balance and cultures

> **Mission.** Find out whether battles and sieges have counterplay, and whether attack and defense are balanced at the point where matches are decided.
>
> **Probe at least:**
> - Build the counter graph from the roster table as a directed graph, and check it against the prose that describes "two linked cycles". For each era, find two- and three-unit compositions that no single unit counters. Is there a dominant army?
> - Set the siege units against each other and against the roster: mangonel scatter shot, outriders as engine hunters, mantlets, and the trebuchet that "cannot hit anything close". Is protecting and hunting engines a real contest?
> - The description claims "at equal spending, the defender holds a modest edge". Add repair, overlapping towers, an inner ring, elevation steps and the stone cost of every engine shot. Is the edge modest, or does the turtle win?
> - Deep foundations arrive in the Keep era at "extra cost", and a wall on one cannot be undermined. Does that make undermining a trap, and with it the Delvers' identity?
> - Fire: the materials table says timber "burns from the Citadel era on", but the Wealdfolk have fire from the Keep era. Reconcile the two, and judge what early fire does to a Hall-era timber economy.
> - Cultures: does each culture's strength land in the era where matches are decided? (*Economic tempo in a match* puts decisive sieges from minute 30.) Does the Masons' barbican work in a game with no wall-walks?
> - Shots are physical and arrows fly flat, so a defender's archers behind their own wall cannot shoot out. Are towers the only ranged defense, and is that intended? When a tower stops working, its garrison "is put out at the foot of the tower". Does that put them into the breach?
> - Elevation gives +15% damage and a range band for each 3 m step, up to two steps. What do quarries, ramparts and the Deep's terraces do to that?
>
> **Leave to others:** economic rates (ECON), loopholes (EXPLOIT).

#### WORLDS: the persistent world as a social system

> **Mission.** Find out whether the shared seasonal world works as a social system over eight weeks, with real people in real time zones.
>
> **Probe at least:**
> - Each lord chooses a daily two-hour siege window. Model what rational lords choose across three or more time zones, and find out when attackers and defenders actually meet. Compare this with the timezone problems of EVE Online's structure timers, which the lineage table cites. Who fights most sieges, players or stewards?
> - The steward defends and repairs but never attacks. If it defends nearly as well as a player, why be online for your window? If it defends much worse, absent lords get farmed. Where must it sit, and can the Siege Trial's AI reach that level?
> - Test "a lord who plays twice a week stays in the race" against daily windows, carts that need escorts, and banner duty. Who is this game for at two sessions a week, and who at seven?
> - Banners hold up to four lords, and the game enforces truces until either side ends them with a day's notice. What stops each of these: kingmaking, banner-hopping, a dominant banner with a web of truces, and one person running several lords in a 32-seat world?
> - A lord quits in week three. What happens to their holding, their Earthstone and their banner?
> - The world freezes at a fixed hour on the last day. What do rational lords do in the final 48 hours?
> - The description promises "there is always one in Settlement". A world holds 16 to 24 humans and runs eight weeks, and newcomers can join only during Settlement. How many concurrently engaged players does that schedule need, and what happens below that number?
> - Titles and heraldry carry across seasons, and communities host worlds first. Who is the authority for a title earned on a community server? What stops a host from abusing admin power?
> - Take a lord in week five who is clearly out of the race. What do they still play for?
>
> **Leave to others:** rule-level loopholes (EXPLOIT), the economics of upkeep and windows (ECON).

#### EXPLOIT: red team

> **Mission.** Play a rules-lawyer who wants to win by the letter of the text, in every mode. Every hole is a finding. A hole a top player would use every game is a Blocker for the stage it bites at.
>
> **Probe at least:**
> - Outside a holding's window, "engines, sappers and fire cannot harm the holding's structures". What about laborers on a pit, trench or quarry designation dug beside an enemy wall, so that its foundation slumps? A collapse that starts inside the window and lands after it? A cascade that starts on shared ground or at an outpost and crosses into a holding?
> - Who may place a designation where: inside another lord's holding, on shared ground at its edge, under a neighbor's road? Demolition is "used inside captured ground", but in Worlds holdings are sacked, not taken. What is captured ground?
> - In Worlds, repair runs at full speed and new construction at a quarter. Is a part-built template a ruin that can be repaired at four times the speed?
> - A lord's window grows with the Earthstone in their keep. Where else can Earthstone sit: on carts, at an outpost, in a banner-mate's keep, in a storehouse? What does hoarding it outside the keep until the final hour do?
> - A sack carries off a third of the keep's Earthstone and grants a 48-hour truce in both directions. Can a lord arrange to be sacked by a friend to buy a shield, or move Earthstone through a staged sack?
> - During Settlement nobody can damage another lord's structures. What can a lord build, dig or block in those two weeks to cripple a neighbor's approaches before Contest?
> - The AI: the steward repairs by standing orders, and the Hold Trial's raiders, which become the world's raiders, pick "the thinnest stretch of wall" they can reach (GameDesign.md, stage 2). How do you bait each into wasting stone, troops or a whole wave?
> - Collapse griefing: dropping terrain or rubble onto carts, armies, or a rival's terraces in the shared Deep.
> - Skirmish victory: a keep stays broken for two minutes against a ten-second repair lockout. Can either side game the clock? What does "all units dead" mean while buildings still stand?
>
> **Leave to others:** balance tuning that is not a loophole (COMBAT, ECON).

#### UX: experience and readability

> **Mission.** Find out whether a player can see, understand and control what the design asks of them, especially during a siege and especially the first time.
>
> **Probe at least:**
> - The camera is isometric and rotates in 90° steps, over terrain full of walls, ramparts, pits and rubble. Are four angles enough? What does the Deep, an inverted fortress of terraces, look like from this camera?
> - Five legibility tools (integrity view, collapse preview, silhouettes, passability view, fog) sit on top of the usual RTS interface. During a siege, what must a player watch at once? Which tool must be on to see each thing? What happens to their control of the army meanwhile?
> - The integrity view shows "each wall section's thinnest remaining cross-section", while collapse is decided by support span (*Support and collapse*, and ADR-004 decision 2). Can the view tell the player the truth the rule uses? What must it show for H2, "right more often than not", to be reachable?
> - There are seven designation types, each drawn as a footprint and a depth, and placement shows cut and fill. Can a new player learn these? Can an expert draw them fast under pressure?
> - Walls do not block vision, but they do block shots. Is that readable?
> - Take the first hour for someone who has not played Age of Empires or Stronghold. Do the Trials teach the game, or assume it?
> - A Worlds session opens on the steward's chronicle. Is reading a log a good first five minutes? What does it feel like to be sieged while at work, and does the design leave players anxious or reassured?
> - Teardown is the visual reference, debris is client-side spectacle, and the camera is a strategy view. Is the satisfying moment of a collapse visible at the zoom where the player gives orders?
>
> **Leave to others:** balance and numbers.

#### MARKET: audience, comparables, commercial viability

> **Mission.** Find out whether the design, as written, has a credible path to commercial success at scale, and what the evidence says. This lens needs the web.
>
> **Additional rules.** Cite every external fact with a URL and the date it refers to. Label every estimate with its method, for instance a review-count multiplier for Steam sales, and give its error. If you cannot find a figure, say so. Never invent one.
>
> **Probe at least:**
> - The audience is named as three groups: players of classic historical RTS, castle builders, and players of persistent strategy. How large is each, and how much do they overlap? Does a game for all three risk being for none?
> - Research these comparables, with sources: the Stronghold series, Castle Story, Medieval Engineers, Manor Lords, Going Medieval, Kingdoms and Castles, Teardown, Rust, Foxhole, Travian, EVE Online, Age of Empires II: Definitive Edition and Age of Empires IV, and persistent or seasonal RTS attempts such as Shattered Galaxy, End of Nations and Supremacy 1914 / Conflict of Nations. For each, say what is comparable, what happened, and the lesson for this design. The voxel castle-siege titles matter most.
> - The hook: what ten-second clip sells this game? Is it visible at strategy zoom? Is it as shareable as Teardown's, given that debris here is cosmetic and outcomes follow rules?
> - The product is Worlds. It needs 16 to 24 committed humans per world, and a world always in Settlement. What population does a new title need to sustain that, and how does it get there? What happens if it sells a few thousand copies?
> - Business model: the documents name no price, monetization, storefront or platform. What does the design imply, given servers per world-season, community hosting, and cosmetics carried across seasons? GameDesign.md proposes deciding whether identity and distribution go through a platform such as Steam, whose SDK R14 excludes. What does each answer cost?
> - Positioning: why would a Stronghold or Age of Empires player switch? Why would a Rust or Foxhole player try an RTS?
> - What would a publisher's greenlight committee ask first, and do the documents answer it?
>
> **Leave to others:** mechanics, except where they shape the pitch.

#### SCOPE: feasibility and the proving plan

> **Mission.** Find out whether the team that exists can build this in the order proposed, and whether the plan retires the riskiest assumptions before it spends the most effort.
>
> **Probe at least:**
> - List the systems the description requires and size each in developer-months for one experienced developer. Include at least: the renderer; the voxel simulation with support search and collapse; pathfinding over terrain that changes every tick; unit combat; a job system for designations and laborers; the AIs (Siege defender, Hold raiders, Skirmish, steward, AI houses, world raiders); an interface with five overlays; save and replay; networking; a dedicated server and hosting tools; accounts and identity; the season archive and timelapse; content for three cultures; art and audio. Under R14, every one of them is written here. Mark which are research-grade, and add them up.
> - The AI burden: AI houses "play the whole game and compete for Earthstone like any lord", in an eight-week world with banners and windows. Which AI is hardest to build, and what is the fallback if it turns out weak?
> - In *How the design is proven*: which of the product's hypotheses does the plan test before the expensive part, and which stay untested until Worlds exists? One example is whether 16 to 24 people will commit to an eight-week season with siege windows. Propose the cheapest test for each untested one.
> - Experiment validity: the playtest is 6 to 10 RTS players, each playing both versions. Consider novelty bias and order effects, and whether the health-bar control gets equal polish. Can "right more often than not" and "prefer it" be measured at that size? Are the kill criteria in GameDesign.md decisive, and fixed before the test?
> - If H1 fails, resources become nodes. What remains of the Worlds design that depends on dug terrain?
> - The minimum lovable product: what is the smallest version that delivers the pitch's promise? What would you cut, and in which order?
> - Technical hot spots at the decided voxel sizes, a 1 m world with 0.5 m models (ADR-006, and its 0.5 m world for comparison). Work out the voxel count of a 2 km × 2 km world down to the Deep, the edits a season accumulates, server memory, the cost of a support search after a breach, and navigation rebuilds during a night of sieges with 24 lords. Consider also a dedicated server that runs only on Windows under R14.
>
> **Leave to others:** the market (MARKET).

#### SPEC: internal consistency and completeness

> **Mission.** Read as the programmer who must implement this tomorrow. Find contradictions within and between the documents, undefined terms, and rules that are missing where the code will need them. GameDesign.md governs where the two differ, but a conflict is still a finding, because designers read the description.
>
> **Probe at least:**
> - Cross-check the description against GameDesign.md and ADR-002 to ADR-004 on these topics: victory conditions, fire, the support and collapse rules, units losing their ground, navigation, fog, repair, the MVP's contents, cultures and the server model.
> - Check the description against itself: the buildings table's eras against the era unlock table; the tempo timings against what each era needs; the materials table against the culture rules; and every number stated twice (seats and lords, population caps, window lengths).
> - Find undefined terms: a critical part's "threshold", "captured ground", a "band of range", the "fixed depth" below which voxels anchor, "weak points" in the integrity view, who owns outposts, and where holdings' boundaries lie.
> - Find missing rules: who owns ground; who may place designations where; what happens at a holding's boundary; what a sacked lord loses besides Earthstone; whether Earthstone counts against storehouse caps; what fills a seat a human leaves; how a tie at the end of a season or a match is settled.
> - List every quantitative target in the description, and for each say whether the inputs needed to test it are defined.
>
> **Leave to others:** whether a rule is good.

## Phase 2: merge (you, not an agent)

Collect every finding. Merge findings that share a root cause into one. Keep every contributing id (for example `ECON-2 + EXPLOIT-4`), the strongest evidence, and the highest severity that someone backed with evidence. Number the merged findings `F-01`, `F-02` and onwards. Keep the load-bearing lists apart from the findings. Do not add findings of your own. If you saw something no reviewer raised, keep it for the report's *Not covered* line, marked unverified.

## Phase 3: verification

Split the merged findings into three batches of about equal size, and keep every finding with a MARKET id in the same batch, because its verifier needs the web. Launch three `general-purpose` verifiers in a single message. Each gets the brief below, followed by its batch word for word:

> You are verifying findings from a design review of Earthrise.World, and your job is to refute them. First read `Design/GameDescription.md`, `Design/GameDesign.md` and `Design/ADR/ADR-002` to `ADR-004` in full. Then check each finding:
> 1. The quotes exist at the cited lines and say what the finding claims.
> 2. No rule elsewhere in these documents already answers it.
> 3. The failure scenario follows from the rules as written, not from a misreading, and not from an assumption the documents contradict.
> 4. The arithmetic holds when you recompute it yourself, and the conclusion survives a reasonable change in the stated assumptions.
> 5. The severity fits the definitions. Blocker: must be resolved before its stage begins. Major: makes the game measurably worse, or puts a pillar or a hypothesis at risk. Minor: an ambiguity or a tuning question.
> 6. For an external claim, the cited source exists and supports the claim.
>
> For each finding, return `F-xx: Confirmed | Downgraded to <severity> | Refuted — <one-line reason>`. Where you confirm a finding whose quote or number was wrong, give the corrected one. Do not soften a finding because it is uncomfortable, and do not confirm one because it is plausible. Do not write or edit any file.

## Phase 4: the report

Write the report to `$ARGUMENTS` if that is set. Otherwise write it to `Design/Review/GameDescription-<YYYY-MM-DD>.md`, creating the folder if needed. Leave the report uncommitted. Use this structure:

```
# Review of the game description, <YYYY-MM-DD>

Reviewed: Design/GameDescription.md at <commit>, with GameDesign.md and ADR-001 to ADR-004.
Method: nine lens reviewers and three verifiers. <n> findings raised, <m> after merging; <c> confirmed, <d> downgraded, <r> refuted.

## Verdict
One paragraph. Is the design sound enough to start the renderer spike and the Trials as planned? Name the three things most likely to stop the game succeeding, in order. Say plainly that a review cannot establish commercial success, only find what would prevent it.

## Blockers
Each Blocker: the claim, the evidence, the failure scenario, the smallest fix, the cheapest test. Cite the F-id.

## Decisions for the owner
The findings that challenge a Decided item. For each: what the description decided, why the review challenges it, two or three options with their costs, and the recommended option.

## Major findings by stage
Grouped as Spike, Trials, Skirmish, Worlds and Launch. Same fields as the Blockers, written more briefly.

## Contradictions and gaps
A table: location, problem, and wording the owner could accept to resolve it.

## Load-bearing: protect these
The elements several lenses named as carrying the design's value, and which fixes above must not damage.

## Cheapest tests before code
Ranked by how much risk each retires per unit of effort. For each: the F-ids it settles, what to build or ask, and what result means pass.

## Minor findings
One line each, with the F-id.

## Refuted
One line each, with the reason, so the owner can see what was considered and dropped.

## Not covered
Anything the orchestrator noticed that no reviewer raised. Marked unverified.
```

Rules for the report:
- Every statement traces to an F-id.
- A refuted finding appears only under *Refuted*. A downgraded finding appears at its new severity.
- Write plain, direct prose, with no praise outside *Load-bearing*.
- Do not soften a Blocker to make the verdict kinder.

## Hand-back

Your final message gives five things: the report's path; the counts (raised, merged, confirmed, downgraded, refuted); the verdict paragraph; the three top risks with their F-ids; and anything that did not run as described, such as a reviewer that failed or a MARKET lens that could not reach the web. Do not commit, and do not edit the design documents. The owner decides what changes.
