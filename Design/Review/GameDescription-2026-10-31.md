# Review of the game description, 2026-10-31

Reviewed: `Design/GameDescription.md` at `2d739df`, with `GameDesign.md` and ADR-001 to ADR-004. The design documents on the review branch are identical to `main` at that commit.
Method: nine lens reviewers and three verifiers. 106 findings raised, 63 after merging; 33 confirmed, 29 downgraded, 1 refuted. After verification: 12 Blockers, 17 Majors, 33 Minors.

**Resolution.** The owner answered the decisions below on the same day, and accepted the other recommendations. Both design documents now carry them; [GameDesign.md](../GameDesign.md), under *2026-10-31: the design review*, records what was decided. The owner kept two things the review offered to cut, AI houses that play the whole game and Worlds that open with three cultures; both now head the cut order. Line numbers and quotes below refer to the documents as reviewed, at `2d739df`.

Line numbers are in `GameDescription.md` unless another file is named. The working material (each lens's full findings, the merged index and the three verifications) is in the session scratchpad, not in the repository.

## Verdict

The renderer spike can start as planned, after two additions to its pass mark (F-51). The Trials cannot start as specified. Five Blockers bite at them, and all five are rule text, not code: engines that can break the keep without a breach (F-01), a gate that is always the best breach (F-02), repair that is either decisive or useless around the 10-second lockout (F-03), an integrity view that shows a different variable from the one the collapse rule uses (F-04), and a Hold wave rule that cannot see where the player quarried (F-45). Left as written, the Trials can return a false verdict on pillars 1 and 2. They can kill a pillar for a reason that has nothing to do with it, or pass it on evidence that would pass the health-bar control just as well. All five can be fixed on paper before any Trials code is written.

The three things most likely to stop the game succeeding, in order:
1. **The product needs a population and a build effort that nothing in the plan provides or gates.** Worlds needs 64–96 committed lords at all times to keep its promise of "always one in Settlement". The online gate has no demand criterion (F-55), and no test of the season shape exists before Worlds is built (F-54). The plan is roughly 80–140 FTE-months of solo work, with no build budget and no cut order (F-52).
2. **Worlds, as specified, breaks under rational play.** At a quarter of Skirmish speed, with the steward gathering around the clock, a lord reaches the Citadel on day one (F-27). The siege window rewards picking the world's dead hour (F-33). Earthstone is safe anywhere except the keep (F-31), the sack rule can be turned into a shield (F-32), and holding protection covers structures but not the ground under them (F-30).
3. **The proving plan can mislead the owner** (F-45, F-04, F-47). This one is the cheapest to fix and bites first.

A review cannot establish commercial success. It can only find what would prevent it. Most market findings were downgraded because the documents are a design description, not a commercial plan. The commercial case is simply absent (F-62): that is not evidence of failure, but it means there is no evidence either way.

## Blockers

### F-01: Lobbed engines can break the keep over an unbreached wall (Trials)
- **Evidence:** "Engines lob their shot and strike whatever lies at the end of the arc." (:205); "A keep is broken when it has been damaged until it stops working and is not repaired within two minutes." (:34); the trebuchet has "Long range and large craters" (:237). Nothing restricts what an engine may target.
- **Failure scenario:**
  1. In the Siege trial, a trebuchet deploys outside tower range and shells the great tower over the curtain.
  2. One hit at least every 10 s blocks repair.
  3. The keep breaks two minutes later with no breach chosen, so H2's breach data is empty.
  4. The keep's own garrison cannot answer it, because elevation caps at two steps.
- **Working:** shelling works once engine range ≥ keep-to-curtain distance + tower reach. With tower reach of 40–70 m and a keep 30–60 m inside the curtain, any engine range above 70–130 m can shell such a castle. Out-walling a 200 m trebuchet needs a ring about 880 m long, which takes about 7,900 m³ of stone.
- **Smallest fix:** either make the win require attackers holding the keep's foot for the two minutes, which changes a Decided win condition, or state a balance bound: engine range < tower reach + minimum keep-to-curtain distance.
- **Cheapest test:** draw the Siege castle to scale with range circles from every point outside tower range.

### F-02: The gate is the best breach whenever there is a gatehouse, and is the same in both test arms (Trials)
- **Evidence:** "The timber gate is its weak point." (:160); the ram's "roof shrugs off arrows; it is weak to melee" (:233); "a gate in about two" minutes (:264), against "about three minutes" for a trebuchet on a standard section (:262). The control swaps only the resolver and the gathering rule (GameDesign.md:105).
- **Failure scenario:**
  1. A timber-only, arrow-proof ram takes the gate in about two minutes, with contact hits keeping it inside the repair lockout.
  2. Testers pick the gate in both arms "because it is the weak point".
  3. That reason passes H2's "nearest point of the wall" test, yet says nothing about voxel destruction.
  4. Correction from verification: this applies from the Keep era, not the Hall era, because gatehouses are Keep-era.
- **Smallest fix:**
  - Add "or cluster at the gate" to H2's failure list.
  - Give gatehouses a garrison attack on their own passage that the ram's roof does not stop.
  - Set the gate's target time near the trebuchet's time with repair.
- **Cheapest test:** a table of every breach route on the Siege castle (time, era, stone, which defenders can hurt the breaker). A route that wins every column is dominant.

### F-03: Repair is all or nothing around the 10-second lockout (Trials)
- **Evidence:** "A section hit in the last ten seconds cannot be repaired." (:129); "Repair between volleys at most doubles those times." (:263); "No unit ever stands on a wall." (:162).
- **Failure scenario:**
  - Against one trebuchet with a 20 s reload, about seven laborers refill every crater, so the section never opens.
  - Two trebuchets fired 10 s apart lock repair out permanently, so the second engine removes repair altogether rather than halving the breach time.
  - H2's "open after N shots" then depends on a hidden crew size and on the reload, not on the wall.
  - Where a repairer stands is unstated: outside among the besiegers, or inside behind 1.5 m of stone.
- **Working:** breach-time multiplier = c / (c − n·ρ·max(0, I − 10)), where c is the volume removed per hit, n the number of repairers, ρ the repair rate per repairer and I the interval between hits. It is exactly 1 for I ≤ 10 s, and grows without bound in n above that. Correction from verification: "section" is defined for walls (the 8 m template) but not for keeps, towers or gatehouses.
- **Smallest fix:**
  - A per-section repair rate that does not grow with crew size, set so that "at most doubles" holds.
  - A "hit" is an engine or ram hit that removes voxels.
  - Repairers work from the owner's side.
  - Decide whether the lockout is per section or per crater region.
- **Cheapest test:** a spreadsheet of breach time over reload (6–30 s), number of engines (1–2) and repairers (1–10).

### F-04: The integrity view shows thickness, but collapse is decided by span (Trials)
- **Evidence:**
  - "The integrity view shows each wall section's thinnest remaining cross-section" (:244, :387).
  - "A voxel is supported if its support distance is no more than its material's span" (ADR-004 decision 2).
  - "Nothing outside the resolver predicts a collapse. The AI planning a breach, and any interface that previews where something will fall, ask the resolver." (ADR-004 decision 5)
- **Failure scenario:**
  - A face shaved thin everywhere reads as the weakest section and never falls.
  - A full-thickness section with one hole wider than twice the stone span drops the slab above it.
  - The player bombards the wrong section, answers the prediction question wrongly, and H2 fails on a mis-specified readout. The kill criterion then makes destruction cosmetic.
  - "Open" is defined only by implication (a route exists, :211 and :390), with no unit class named, and N is not fixed. H2 therefore has no answer key.
- **Smallest fix:** derive the readout from the resolver instead of thickness: the widest through-hole against the collapse width, the opening's floor height against the climb limit, and any undercut against the span. Define "open" as "the passability view shows an infantry route through the section", and fix N.
- **Cheapest test:** a paper prototype. Draw twelve wall-face states on a grid, resolve each by hand under the span rule, and compare prediction accuracy with a thickness readout against a support readout.

### F-06: Loose material can ramp over an intact wall (Worlds; Major in Skirmish)
- **Evidence:** "A rampart alone is a slope an army can climb." (:156); "Each column has one walkable surface, because units never stand on structures" (GameDesign.md:55). Nothing stops loose material from resting on top of a wall voxel.
- **Failure scenario:**
  1. An attacker piles 144–250 m³ of earth against a 6 m wall, using ditch spoil, the cut-and-fill of construction, or a toppled structure of their own.
  2. Every column along the wall now ends in earth, so the army walks over a wall that was never damaged.
  3. In Worlds this needs no window. In Skirmish it is a Hall-era crossing that skips H2's choice of breach entirely.
- **Verification note:** the spoil route needs the ditch's "near side" to fall on the wall. The toppled-structure route depends on whether the window suppresses the fall's damage.
- **Smallest fix:** loose material never comes to rest above a standing structure voxel within N voxels, and a column with a structure voxel under its loose surface is not walkable. Pair it with F-30's boundary margin.
- **Cheapest test:** in the first Trial build, place a rampart against a wall template and ask navigation for a route.

### F-27: At a quarter speed, a Worlds castle takes hours, not weeks (Worlds)
- **Evidence:** "Gathering and new construction run at a quarter of Skirmish speed. Battles feel like an RTS, while castles and the Deep take weeks to build and dig." (:318); the steward works "gathering priorities" while the lord is offline (:328).
- **Working:**
  - The steward gathers around the clock, so a holding does 24 h × ¼ = 6 Skirmish-hours of work per real day.
  - The Citadel, reached at Skirmish minutes 25–30, arrives after 100–120 world-minutes: 0.6% of Settlement.
  - Settlement alone holds 84 Skirmish-hours of economy.
  - A uniform factor slow enough to give "weeks" (1/336 to 1/672) leaves a 60-minute session with 5–11 s of Skirmish work. No single factor satisfies both.
  - Culture traits tied to an era are spent during Settlement, when nobody can be attacked (COMBAT-11).
- **Smallest fix:** replace the factor with a daily work allowance per holding, sized from target durations, spent at full speed while the lord is online, with the steward spending what is left. Alternatively, gate eras by the calendar.
- **Cheapest test:** a spreadsheet of milestones against target days.

### F-30: Holding protection covers structures, not the ground they stand on (Worlds)
- **Evidence:** "Inside it, their structures can be damaged only during their siege window" (:322); "Demolition … Used inside captured ground." (:103), which is never defined; "Lords claim holdings and build. No lord can damage another lord's structures." (:312), against outposts "always open to attack" (:324).
- **Failure scenario:**
  - A rival trenches beside a wall at 03:00. The window says the wall cannot be damaged, and the support rule says it falls; nothing says which wins.
  - A rival strips a neighbour's outcrop or forest. No structure is involved, so nothing protects it.
  - During Settlement, a palisade across a valley mouth on shared ground blocks the neighbour for two weeks.
  - Nothing says who may place a designation where.
- **Verification note:** EXPLOIT-3's headline misreads line 322, whose first sentence bars any damage to structures whatever the cause. The finding stands on the unprotected ground, the missing precedence and the missing placement rule.
- **Smallest fix:** one predicate over edits:
  - Inside a holding and outside its window, no other faction's action may remove a voxel, whatever the cause.
  - Designations go only in your own holding, or on shared ground with a margin from every boundary at least as wide as the slump reach plus the tallest fall.
  - A collapse that starts inside a window runs to completion.
  - Raiders obey windows.
  - Define captured ground and demolition.
- **Cheapest test:** on paper, run eight cases through the predicate.

### F-31: Earthstone is exposed only while it sits in the keep (Worlds)
- **Evidence:** "you win by holding the most Earthstone in your keep when the season ends" (:36); "The window grows by thirty minutes for every 25 blocks of Earthstone in the keep" (:322); a sack takes "one third of its Earthstone" (:332).
- **Failure scenario:**
  1. A lord holds the season's Earthstone anywhere but the keep: a storehouse (storage is silent) or loaded carts.
  2. Their window stays at two hours, the public count reads zero, and a sack takes a third of nothing.
  3. On the last afternoon they unload into the keep, with the final window placed at or after the freeze.
  4. Visibility, exposure and sacking then bind nobody.
- **Verification note:** carts are weaker than claimed, because carts can be attacked at any hour. Silence on other storage, and the placement of the final window, still carry the finding.
- **Smallest fix:**
  - All Earthstone a lord owns counts, lengthens the window and can be sacked, wherever it is.
  - A day's window uses the most Earthstone held at any moment of the previous 24 hours.
  - Every lord's last window ends at the freeze.
- **Cheapest test:** a 56-day spreadsheet comparing a lord who deposits with one who hoards, plus a ten-minute tabletop.

### F-32: The sack rule can be turned into a shield (Worlds)
- **Evidence:** "If a keep stops working during its lord's window and is still broken when the window closes, the holding is sacked. … the sacked holding gets a 48-hour truce in both directions." (:332)
- **Failure scenario:** three ways, the first the strongest.
  - **A keep that stays broken.** After one sack, a lord who never repairs the keep can never be sacked again, because the keep never "stops working during" a later window. Verification found this stronger than the reviewer stated.
  - **A staged sack.** A friend breaks an empty keep and so buys 48 hours of two-way truce.
  - **A window that never closes.** No cap is stated. At about 1,100 blocks the window reaches 24 hours and never closes, so the sack can never fire. This depends on a Deep yield that is not stated.
- **Smallest fix:**
  - The sack fires on "a keep that is not working at any moment of the window and still not working at the close".
  - The loot goes to the faction that did the most damage, and a sack with no hostile damage pays nothing.
  - Replace the two-way truce with "cannot be sacked again for 48 hours".
  - Cap the window.
- **Cheapest test:** a three-day tabletop played by two cooperating players under the rules as written.

### F-33: The rational siege window is the world's dead hour (Worlds)
- **Evidence:** "two hours a day, at a time the lord chooses" (:322); communities host "within set rules for season length and windows" (:369), but those rules are not written anywhere. Nothing rewards defending in person.
- **Failure scenario:** most lords set their window to the small hours. Sieges become AI houses against stewards, the evening session finds no open targets, and banners that span time zones own the always-open Deep at night.
- **Working:** with evening play bands in UTC+1, UTC−5 and UTC−8, a shared dead window exists at about 08:00–10:00 UTC. Verification: the claim holds while no lord passes roughly 400–625 blocks.
- **Smallest fix:** each world declares a home time zone and a contest band, and every window must start inside it. The outposts and the pit follow the same band.
- **Cheapest test:** a form on which 20 RTS players choose a window and an attack time, compared against their stated play hours.

### F-45: H1 cannot see pillar 1 (Trials)
- **Evidence:**
  - "A wave chooses its breach point by one rule: the thinnest stretch of wall it can reach from where it arrives." (GameDesign.md:110)
  - H1 fails only "if … where players quarried has no visible relation to where the waves breached or how the defense went, and players cannot name a trade-off" (GameDesign.md:102). The description, by contrast, requires both conditions to hold (:416).
- **Failure scenario:**
  1. Standard sections tie on "thinnest", so an unstated tie-break picks the breach.
  2. Quarries keep ramps, so every stretch stays reachable, and nothing reacts to where the player quarried.
  3. A deliberately thin stretch baits every wave.
  4. A tester names a hauling trade-off. That satisfies GameDesign's wording, in the control too, so H1 passes on nothing.
  5. A strict facilitator fails H1 for an AI reason instead, and the kill criterion replaces pillar 1 with resource nodes.
- **Corrections from verification:** the player's own quarry still slumps their own earth-founded wall. The MVP excludes undermining by AI factions only, so it is "nothing undermines the player", not "nothing undermines".
- **Smallest fix:**
  - Score each reachable section on the four reasons the description already names (:245): thin, on earth with no ditch, far from towers, a gate. Use the resolver to score them.
  - Choose among the best few with a seeded draw, and state the tie-break.
  - Add one scripted sapper wave.
  - Reword GameDesign.md:102 to fail on no relation, or on a trade-off that is about hauling alone.
- **Cheapest test:** on paper, draw three holdings with quarries in different places and apply the rule by hand. If the breach does not move, the test cannot see H1.

### F-55: Worlds needs a population that nothing provides or gates (Worlds)
- **Evidence:** "Worlds open on a regular schedule, so there is always one in Settlement." (:361); "16–24 human lords" (:25); communities host "choosing their own schedule" (:369); the online gate (GameDesign.md:72) has no demand criterion.
- **Working:**
  - Always one world in Settlement means a new world every two weeks, so four run at once: 64–96 committed lords at all times.
  - Correction from verification: that is a total. It is per region only if regions are added, which is MARKET-1's assumption.
  - Community hosts on their own schedules split the sign-ups. Three worlds of 13 are all below the floor, where two worlds would have filled.
  - Crowfall had seasonal campaigns on a destructible voxel world with scheduled sieges. It went offline on 2022-11-22, citing the cost of running campaigns (verified through search summaries).
- **Smallest fix:**
  - Add a demand criterion to the online gate, for example opt-ins counted from the first release.
  - Open a world when its queue fills, not on a calendar.
  - Let the seat count scale down.
- **Cheapest test:** a "tell me when Season 1 opens" counter in the first release, and a spreadsheet of the funnel.

## Decisions for the owner

Each item below challenges something already Decided. The recommendation comes first in each list.

1. **Worlds pace** (F-27; Decided: a quarter of Skirmish speed).
   - (a) A daily work allowance per holding, spent at full speed: one meter in the interface, and the pace matches its target. **Recommended.**
   - (b) A smaller uniform factor: castles take weeks, but sessions show no visible progress.
   - (c) Calendar-gated eras: simple, but holdings sit idle with nothing to build.
2. **The win condition** (F-01; Decided: break the keep).
   - (a) The two-minute clock runs only while attackers hold the keep's foot: breaching becomes mandatory whatever the geometry. **Recommended**, with (b) kept as a tuning guard.
   - (b) A balance bound on engine range: keeps the rule, but it ties engine range to castle size.
3. **The Hold wave and H1's wording** (F-45; Decided: "one rule" and the H1 text). Score sections on the four stated reasons through the resolver, add one sapper wave, and make GameDesign.md:102 match the description's two conditions. **Recommended** as a pair: either alone leaves H1 blind.
4. **Siege windows** (F-33; Decided: any two hours the lord chooses).
   - (a) A contest band per world. **Recommended.**
   - (b) Sieges declared a day ahead: protects absent lords through their banner (F-34).
   - (c) A reward for defending in person: needs tuning and does not fix time zones on its own.
5. **Earthstone and sacking** (F-31, F-32; Decided: Earthstone counts "in the keep", sacks are decided at the window's close, the truce runs both ways). Count all Earthstone a lord owns; change the sack trigger, the loot and the truce as in F-32; cap the window. **Recommended** as one change, because the two findings interact.
6. **Population promises** (F-55; Decided: 16–24 humans, "always one in Settlement", communities choose their own schedules).
   - (a) Open a world when its queue fills, with a scalable seat count and a demand gate. **Recommended.**
   - (b) Keep the promise for official worlds only, which the developer must then run.
7. **Scope and the yardstick** (F-52, F-53, F-62; Decided: AI houses "play the whole game", Worlds open with three cultures). The documents call this a one-developer hobby project, and the ambition is a commercial hit. These two goals give opposite answers to most of the findings above.
   - First decide which governs.
   - Then write a build budget for each stage, with a cut order. The two largest savings are AI houses becoming fortress garrisons and Worlds opening with one culture. **Recommended.**

## Major findings by stage

### Spike
- **F-51:** The spike measures the wrong load.
  - The "stated rate" of quarrying is not stated. Back-solved from the tempo, it is 28,000–94,000 voxel edits per second at 10 cm, depending on 2–4 factions.
  - Question 6 measures only the 512 m map. A 2 km world needs about 24 GB for its near-surface store at 10 cm.
  - Spans are counted in voxels, so the choice of size is expensive to undo.
  - Fix: write the back-solved rate into the pass mark, and add a synthetic headless 2 km load to question 6.

### Trials
- **F-07:** Earthworks contradict themselves.
  - With one climb limit, the ditch side and the rampart share earth's slump slope, so either "a slope an army can climb" (:156) or "a ditch … keeps sappers away" (:193, :254) is false.
  - The "near side" (:99) has no reference point.
  - Fix: climb limits per unit class, the text reworded, and "near side" defined.
- **F-17:** All rubble is "loose stone" that clears for stone at half its volume (:62, :189). Collapsed timber, or a rock lip undercut past its span, therefore becomes stone away from any outcrop; a 120 m³ lip yields 60 m³. Fix: rubble remembers its source, and only rubble from structure stone yields stone.
- **F-21:** Soil depth, bedrock and slump reach are invisible when placing a wall or drawing a dig. Placement shows only cut and fill (:393), so H1's trade-off is chosen blind. Fix: a ground readout at placement, plus a what-if query to the resolver that highlights every structure a dig would undermine.
- **F-47:** The playtest is too weak to decide anything.
  - With 6–10 testers, a majority rule passes with no real preference 36% of the time, and 71% of the time on novelty alone.
  - An "always no" player scores 70% if most sections stay shut.
  - Health bars are always visible, but the integrity view is not said to be on by default.
  - Fix, decided before the control is built:
    - fixed thresholds (7/8 for preference, median accuracy 0.65 or better);
    - counterbalanced order;
    - the same cosmetics in both arms;
    - prediction items split 50/50 between open and shut;
    - the integrity view on by default.
  - UX-4's specific "two against four testers" arithmetic was refuted, but the parity point stands.

### Skirmish
- **F-05:** The end-state rules are incomplete. The Trials can begin on their own end rules, but Skirmish needs answers to all of these:
  - which building counts as "the keep" while the keep is being built around the hall;
  - free-for-all or teams;
  - two timers expiring on the same tick;
  - what "all units" counts;
  - a loss rule for the Siege trial.
- **F-53:** AI houses "play the whole game" (:340) with no fallback, and the steward is more than the Siege defender, which "fights and repairs, and does nothing else" (GameDesign.md:109). The Skirmish AI's castle building is part of the Open item but carries the most risk. Fix: write the fallback now (AI houses become fortress garrisons) and correct the reuse chain (the steward is the defender plus the Skirmish AI's economy layer).
- **F-06** is also Major here (see Blockers).

### Worlds
- **F-29:** The Deep has no phase gate. Nothing opens the "richest levels" in Reckoning, and :313 ("reached") disagrees with :314 ("open"). Fix: a phase rule by depth.
- **F-34:** How strong the steward must be is never stated, and "never attacks" (:328) sits against the defender's "cavalry sally" (:256). Downgraded from Blocker, because the banner can garrison and repair for an absent lord.
- **F-35:** No standing order sends or escorts carts, and nothing says where dug Earthstone waits between sessions. That undercuts "a lord who plays twice a week stays in the race" (:381).
- **F-36:** No rule covers a lord who quits. Their steward-run holding becomes a target their neighbour sacks repeatedly.
- **F-38:** None of these season edge cases has a rule:
  - banner switching on the last day;
  - ties;
  - a lord without a banner;
  - whether AI houses can win;
  - a seat a human leaves;
  - windows still open at the freeze;
  - carts on the road at the freeze.
- **F-41:** Nothing says what happens to Earthstone on a destroyed cart, or to Earthstone voxels in a collapse, which ends "as rubble" (:189). A one-cut stranding in the Deep also conflicts with "the game never strands a laborer" (:105).
- **F-42:** No rule gives one seat per person. Alts run by stewards, plus shared banner vision, let one player see most of the world.
- **F-54:** None of the hypotheses Worlds rests on is tested before Worlds is built:
  - commitment over 56 days;
  - both sides present at windows;
  - the twice-weekly lord;
  - catching the leader;
  - AI houses over eight weeks;
  - communities willing to host.

  Fix: an eight-week paper campaign and a season spreadsheet, each with a kill threshold, as gates.
- **F-56:** Nothing says who mints titles that carry between seasons, and a community host who also plays holds the resource seed and sees everything. Verified: Valve states the Steamworks SDK is not required to ship on Steam, so only identity needs a decision under R14, not distribution.

### All stages
- **F-52:** One experienced developer needs 79–137 FTE-months (judgment estimates). At 20 hours a week, that puts the Trials verdict 5–9 years away and Worlds 13–23 years away, with no build budget and no cut order. Downgraded from Blocker, because the stage gates already sequence the work.

## Contradictions and gaps

| Location | Problem | Wording the owner could accept |
|---|---|---|
| :227 against :224–225 (F-11) | The prose says knights beat "shooters"; the table says crossbowmen beat knights | "knights beat men-at-arms and archers" |
| :61 against :290 (F-15) | Timber "burns from the Citadel era on", but Wealdfolk have fire from the Keep era | "Timber burns when fire reaches it. Fire unlocks in the Citadel era, and in the Keep era for the Wealdfolk." |
| :291 against :234 and :145 (F-14) | Hall-era Delver sappers cost iron, which needs the Keep-era mine head | "Delver sappers cost provisions and timber" |
| :85 against :289–290 (F-14) | Exact volume cost, against Mason and Wealdfolk building that "cost less" | Express both as faster building and repair, not less material |
| :313 against :314 (F-29) | The Deep's levels are "reached" by digging but "open" by calendar | "Rock below depth Dₖ cannot be dug before phase k" |
| :302 against :63 (F-50) | The Skirmish Earthstone race, against Earthstone "in Worlds only" | Mark the race post-MVP and list Earthstone in it |
| :156 against :193, :254 (F-07) | A rampart is climbable, yet a ditch with the same slope keeps sappers away | Climb limits per unit class; "a ditch keeps sappers under fire for longer" |
| :99 (F-07, F-25) | "Near side" has no reference point | "The side facing the designating lord's keep; flip it while drawing" |
| :244, :387 against ADR-004 decision 2 (F-04) | The view shows thickness; collapse depends on span | A readout from the resolver (see F-04) |
| GameDesign.md:102 against :416 (F-45) | H1 fails on "neither" in one, but needs "both" in the other | Fail H1 if there is no visible relation, or if the trade-off named is only about hauling |
| GameDesign.md:55 against :57 (F-24) | One walkable surface per column, but rock may overhang | "The highest walkable cell; the space under an overhang is impassable" |
| :62, :189 (F-17) | All rubble yields stone, whatever it came from | "Rubble keeps its source; only rubble from structure stone yields stone" |
| :158 (F-23) | "About twice as many shots" for 1.67× the volume | State the intended ratio |
| :103 (F-30) | "Captured ground" is undefined | Define demolition as of your own structures, or of a sacked ruin during its window |
| :322 against :180 (F-30) | No precedence between the window and the support rule | The F-30 predicate |
| :129 against :318 (F-40) | The line between repair and new construction is undefined | "Repair restores only voxels that were once built and later removed" |
| :328 against :256 (F-34) | The steward "never attacks", yet the defence relies on a sally | "Engages hostiles inside its holding, out to a leash" |
| :322 (F-32) | The window's growth has no cap | A cap, for example 12 hours |
| :42 against GameDesign.md:16 (F-26) | The pillar rule marks the Decided RTS layer as cuttable | "Pillars test the differentiators; the Decided RTS layer is exempt" |

## Load-bearing: protect these

- **One rule-based collapse resolver that every view, preview and AI asks** (ADR-004 decisions 2–5). Six lenses named it. It is what makes destruction buildable by one person under R14, and predictable enough for H2 to be reachable. Every fix above (F-01, F-04, F-21, F-45) goes through it and must not copy its rule.
- **Stone counted by volume across buildings, shots, repair and rubble** (:85, :240). Five lenses named it. It joins pillar 1 to pillar 2 and makes a siege an economic contest. Re-price shots and culture bonuses as prices and rates (F-13, F-14). Never break the identity for buildings.
- **Walls are crossed only through gates and breaches, and a breach is a route** (:162, :211). This is what makes where to breach the attacker's main decision. F-02's and F-09's fixes must not reopen ladders or wall-walks.
- **The staged proving order, with a gate and a health-bar control** (:403–419). It is the only thing that makes years of solo work depend on evidence. F-45, F-47 and F-54 sharpen it; none replaces it.
- **The bounded window and a season that ends in an archive** (:322, :365). They let people with jobs keep a destructible castle and stop runaway leads from carrying over. Fixes should constrain where the window sits (F-33), not remove it.
- **The integrity view as the place where pillars 1 and 2 become legible.** Keep its role. F-04 changes the field it shows, not the view.

## Cheapest tests before code

Ranked by how much risk each retires per unit of effort.

1. **A Worlds season spreadsheet** (F-27, F-29, F-31, F-32; also F-28, F-39). Settles the pace, deposits, the Deep's timeline, window growth and hoarding. Model 56 days of one holding: rates, deposits, upkeep, Earthstone, window length. Pass: the Citadel arrives on its target day, deposits last to the intended week, and the leader's window stays under the cap.
2. **A paper Hold test** (F-45). Draw three layouts with quarries in different places and apply the wave rule by hand. Pass: under the revised rule, the breach moves with quarry placement in at least two of the three.
3. **A paper breach-prediction prototype** (F-04, F-47). Twelve to twenty wall states on a grid, resolved by hand; six people shown a thickness readout or a support readout, with items split 50/50 between open and shut. Pass: median accuracy of 0.65 or better with the support readout.
4. **A route table for the Siege castle** (F-01, F-02, F-03). Draw range circles, and tabulate gate and ram against section and trebuchet against shelling the keep, with a repair model. Pass: no route wins every column, no engine circle from outside tower range covers the great tower, and repair adds 1.3–2× for one or two engines.
5. **A rules tabletop for Worlds** (F-30, F-31, F-32). Two or three players get the revised rules and 30 minutes to find an exploit. Pass: none found.
6. **A window survey and a demand counter** (F-33, F-55). Twenty RTS players choose windows and attack hours, and the first release counts opt-ins. Pass: windows overlap attackers' play hours, and opt-ins reach 64 or more.
7. **A build budget, and the spike's estimate against its actual time** (F-52). Write the spike's estimate before it starts. Pass: the actual time is within 1.5× the estimate; otherwise rescale every stage.
8. **An eight-week paper campaign** (F-54). Sixteen or more players on Discord with chosen windows. Pass: at least 75% still active at week 6, and both sides present in at least half the windows that see a siege. It is cheap in code and costly in calendar time.

## Minor findings

- **F-08:** Shot rules, such as where a garrison's shots start and what a blocked order does, are ambiguous but cheap to state. Dead ground at a tower's foot depends on the parapet setback.
- **F-09:** Tower archers are poor against the Keep-era assault kit, and only units that cost iron beat men-at-arms. That is a question of garrison mix and tuning, and the iron cost applies to the attacker too.
- **F-10:** Deep foundations are priced by volume (:85), so undermining stays viable on deep soil. The remaining need is a generator constraint for mixed ground at each start.
- **F-11:** The knight cell is a one-line conflict. "A mix beats everything" is wrong (it only ties). In a model that ignores cost, archers and knights get zero weight under the table. This bites at Skirmish.
- **F-12:** Counter-battery is implied (:205). Whether elevation applies to engines is ambiguous, and the script names a weaker screen.
- **F-13:** Priced at a crater, a shot gives a 2–3× stone advantage to the attacker; priced at its projectile, 29–42×. No shot price is set, so it is a tuning number.
- **F-14:** Each culture contradiction is a one-line fix. Upgrade-specific materials remove the need for per-voxel ownership.
- **F-15:** Fire is out of the MVP and cultures are Worlds-only. Only "whose era" decides when timber burns is ambiguous.
- **F-16:** The Hall-era ram against the tempo is a tuning target.
- **F-18:** The warning's length is a tuning value the text already binds (:184). The missing off-screen alert and the side a garrison is put out on are interface gaps.
- **F-19:** There is no repair-lock indicator, and no rule for which tools are always on. The passability view does show new routes (:390).
- **F-20:** Fall of shot becomes a problem only if engine vision is less than engine range. That is a stat choice.
- **F-22:** At a steep pitch, opposite rotations show every ditch floor and terrace. The spike needs a readability criterion for its choice of pitch.
- **F-23:** The thick wall's shot ratio; one wall thickness until the Citadel; back-to-back sections not addressed.
- **F-24:** Overhangs break one-surface-per-column. Negligible if the stone span is about one voxel.
- **F-25:** Five of the seven designations do not fit "footprint and depth".
- **F-26:** The pillar rule marks banners, the market, the ranking and the smithy as cuttable.
- **F-28:** Deposits run out because of F-27's pace. Once the pace is set, deposit size is an Open number.
- **F-37:** Truce count and duration are tuning.
- **F-39:** Stone upkeep caps castle size within a finite budget. Where weathering lands, whether unpaid upkeep builds a debt, and whether earth and timber owe stone are ambiguous.
- **F-40:** On the plain reading, a part-built template is not a ruin. The line between repair and construction is undefined (see the table).
- **F-43:** Rank and feat titles remain for a lord out of the race. What earns each title is undefined.
- **F-44:** When players get defensive practice against sappers and fire is an onboarding question.
- **F-46:** A Hold map with one outcrop is an authoring requirement, not a rule.
- **F-48:** A "breach lab" in the spike would give an earlier reading on H2. This is a sequencing saving.
- **F-49:** Whether Worlds may proceed without pillar 1 is an open choice inside the Proposed kill criteria.
- **F-50:** 8 of 26 quantitative targets are testable as written; the rest are ambiguities in targets meant to be tuned.
- **F-57:** The pitch describes Worlds, which the first release lacks. Keep store copy to what ships.
- **F-58:** No human-versus-human play before Worlds is a positioning question. Single-player first is Decided.
- **F-59:** Naming one primary audience per stage is a positioning task.
- **F-61:** Nothing ties the default zoom to unit legibility, because the spike scene has no units.
- **F-62:** No price, store or revenue stream is named. Server costs per world are small; nothing ties them to the population.
- **F-63:** How the season timelapse is drawn is never said. A tool that renders top-down images avoids a level-of-detail renderer for the whole world.

## Refuted

- **F-60:** "The preference test is not a market test." Nothing in the documents treats it as one. The in-game control exists to isolate the pillar, and the gap in market evidence is F-62 and F-55.

## Not covered

These were noticed by the orchestrator, or appear only in a reviewer's probe notes. None has been verified.

- Water never flows (:65), so no moat ever holds water. A familiar part of the castle-defense fantasy is excluded, and no lens examined that choice.
- Accessibility: the integrity, collapse and passability readouts are visual overlays, and nothing requires them to work for colour-blind players.
- Audio appears nowhere in the design except UX-6's call for alerts.
- From SPEC's notes:
  - "Every game can be replayed from its commands" (:397) promises more than ADR-002's replay within one build.
  - The keep is built before deep foundations unlock (:274), so the building that decides a match cannot have one unless retrofitting is allowed.
  - The mangonel's "small craters" (:236) differs from its "Large" crater in timber (:173).
