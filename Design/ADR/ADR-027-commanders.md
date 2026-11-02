# ADR-027 — Commanders: the AI is a command source inside the simulation, seeing through its player's fog

**Status:** Accepted, 2026-11-02, by order M4-F2 ([MvpPlan-M4.md](../MvpPlan-M4.md)), which builds `Commander`, its view, step 13, its saving and `SiegeDefender`'s first half; the sally, the gate and the breaches are M4-F3's, and `SiegeAttacker` M4-F5's. What the code fixed that the decisions left unsaid, as amendments (2026-11-02, M4-F2):
- **`Commander`** decides, writes its state, names its kind and clones itself, so that a world copies with its commanders, as the breach lab copies its site. `MakeCommander` builds one from a scenario's name or a saved record; `human` gives none.
- **`CommanderView`** gives its player's update from step 11, a section's integrity for 16 units, the integrity query's own cost, and only for a structure its player sees, and reach between two cells for 1 unit; it also gives the rules and statistics, which every player knows. Its draw is `KeyedDraw` on the match seed, a salt, and the tick shifted by 8 bits with the player below.
- **Step 13 runs after step 12** on every tick whose count plus the player's id is a multiple of `commanderDecisionTicks`, 10, so that two players decide on different ticks. Its commands are encoded as a player's and applied in the next step 1 after the transport's, in the order given, and kept in `World::LastTickCommanderCommands`, apart from `LastTickCommands`.
- **The commanders are saved in a section of their own** after the trial's progress, not among the entities, which have ids: each its player, its kind and its state, then the commands they gave for the next step 1, at save format version 6, and the state hash holds all of it. A replay starts from a save, so it names each seat's commander already and its format is unchanged.
- **A scenario's player gets the commander its `commander` names** when the scenario loads.
- **The defender aims a garrison by `AttackTarget`** on its squad, which a garrison now takes (ADR-024, decision 5, amended): its ranged figures loose at the aimed squad's figures when one is within reach with nothing solid on the line, and at the nearest enemy otherwise. It aims only at sappers, rams and the trebuchet, in that order, the nearest of the most wanted; for anything else the nearest stands.
- **The defender repairs by a structure's standing share,** the share of its built voxels that stand, every part, which `StructureView::standingPercent` now carries, kept on the structure at record version 3; a section has no critical part to read it from.
- **In the Siege castle every building that holds a garrison is full at the start,** the keep included, six in each, so archers put out find no room until a building loses its own; the test that sends a stopped tower's archers into the keep is on a site built for it.

The defender's second half landed with order M4-F3 (2026-11-02), as amendments:
- **The sally force is the defender's men-at-arms and outriders together,** the route table's sally, and the same squads are the reserve (MvpPlan.md, M4, input 5). It goes after the nearest enemy engine the defender sees within `sallyLeashVoxels`, 70 m, of a section of its curtain, ties to the lower id, while the force holds half its full figures or more; it comes home, to the cell each squad's lead first stood in, when that engine is gone from its sight, the force falls below half, or a figure of it outside stands past the leash. The leash and the gate's distance are `RULE`'s `sallyLeashMeters` and `gateShutMeters`, inputs 18's 70 m and 13's 10 m.
- **The gate** is the defender's first structure whose critical part is named Gate. It opens while the force must pass it, going out or coming home, and shuts once none of the force is on the side it is going from, or at once while an enemy soldier, a figure that is no engine's body, stands within `gateShutVoxels` of the gatehouse's footprint with no figure of the force as near; inside and outside are the sides of the gatehouse's centre line, as the simulation reads an owner's side.
- **A section that opens** sends the force storming to its inner mouth, 2 m inside its inner face at its middle, once a section, the lowest id first, after any sally has come home; the force then holds there and sallies no more. The defender's record is at version 2, holding the sally's state, the homes and the sections answered.

`SiegeAttacker` landed with order M4-F5 (2026-11-02), as amendments:
- **Its plan is the Siege castle's,** read from its player's update. The flank is the side of the gatehouse that the south curtain's middle tower stands on, east on Siege and west on its mirror, so the plan mirrors itself. Its trebuchet goes round the flank by a waypoint outside every garrison's reach to a first stand, from which it stops the north flank tower. It then goes to a second stand that only that tower covers, from which it stops the keep and then opens the flank section beside the tower. Whichever of these works again is fired at again first, and a building that stops takes 6 stones more. It fires each template's fixed pattern, `PatternAim`, the one the control's calibration counts, and counts a stone once it has flown.
- **The rest of its army escorts the trebuchet,** in rows behind it, and falls on an enemy figure within 35 m of an engine. **Its men-at-arms storm** once the keep has stopped and the section is open: they gather 14 m out from the breach for up to 30 s, go in together to hold places 2 m out from the keep's far face, and one squad sweeps the ring's corners while a defender contests it.
- **Its record** is at version 1: its phase, its target and the stones flown past the target's stop, the sweep's corner, whether the storm has gone in and when it began to gather, the stones flown at each structure, and each engine's last ordered target until its stone is seen to fly.
- **It does not win the trial** at M4's values, in either version (GameDesign.md, *2026-11-02: M4's phase F*, item 8).

It was proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). It records where the Siege defender's AI runs, what it may read, how its commands reach the tick, and how it replays. Its behaviours are the owner's input (MvpPlan.md, M4, input 18). The owner gave it the go-ahead on 2026-11-01, as written (MvpPlan.md, M4, input 1; GameDesign.md, *M4's values*, item 1). Order M4-F2, which first implements it, marks it Accepted (AGENTS.md §6).

## Context

- **[ADR-002](ADR-002-authoritative-simulation.md), decision 1:** the simulation owns the AI factions; **decision 5:** they issue the same commands a player does, through the same interface, so an AI can take over a player's faction later (the steward of Worlds).
- **[ADR-013](ADR-013-ticks-numbers-and-randomness.md):** one build replays the same commands to the same result; draws are keyed; deferred work is budgeted in units.
- **[ADR-025](ADR-025-fog-of-war.md), decision 7:** the AI sees through its own fog. The steward and the AI houses that follow must not see more than a player would (GameDesign.md, *Stages*: each stage builds a piece the shared world needs).
- **[ADR-004](ADR-004-rule-based-destruction.md), decision 5:** an AI planning a breach asks the resolver; it carries no copy of the rule. The Siege defender plans no breach, but it reads which sections are open.
- **The Siege defender's job** (MvpPlan.md, *In*): it fights, repairs, sallies out to a leash and shoots sappers.

## Decision

1. **A commander is a source of commands for one player,** `Simulation::Commander`, with one implementation a kind of player: `SiegeDefender` for the trial, and `SiegeAttacker`, a scripted attacker for the tests, the bench, and the owner's checks and measured run (`Earthrise --watch`), never offered as a way to play. A seat a human plays has no commander: its commands arrive through the transport.
2. **Commanders run at the end of the tick,** after step 12, as step 13: each, in player order, reads its player's view and queues commands for the next tick's step 1, where they are decoded and applied exactly as the transport's are, but kept apart from them: `World::LastTickCommands`, which a replay records, holds the transport's alone. A commander's command is therefore one tick late, as a player's is, and never applied twice on replay.
3. **A commander reads only its player's view:** the state update `PublishState` built for its player in step 11, and the pure readings its player could ask for ([ADR-020](ADR-020-the-in-process-client.md), decision 5), called directly within its own budget rather than through the query queue. It never reads the world past that filter.
4. **Its work is budgeted in units,** `commanderWorkPerTick` in `RULE`, and it decides on a fixed cadence, every `commanderDecisionTicks`, offset by its player's id, so that decisions spread across ticks. A reading costs what its query costs.
5. **Its state is saved,** as one record a commander in the save's entities ([ADR-014](ADR-014-edits-saves-and-replays.md), decision 4), so a save mid-trial continues identically; its draws are keyed on the match seed, its player and the tick.
6. **Replays record the transport's commands only.** A replay starts from the scenario, and its header names the commander in each seat, the scenario's or the one `--watch` put in a human's seat; the replay rebuilds those commanders, and they recompute their commands. A replay therefore needs the same build and server data, as every replay does (ADR-014, decision 6).

## Consequences for the code

- **`Simulation`** gains `Commander`, `SiegeDefender` and `SiegeAttacker`, and the tick's step 13; `World` holds one commander per player that has one, created from the scenario.
- **MvpPlan.md's tick** gains step 13.
- **Tests** run the trial headless with `SiegeAttacker` against `SiegeDefender`, replay it, and continue it from saves.

## What this forecloses

- An AI that reads hidden state, cheats on resources, or calls the world's mutators.
- AI commands that skip the command stream.
- Behaviour that depends on wall time or on how fast the machine is.

## Left open

- **The Skirmish AI**, a commander of another kind, after the verdict.
- **The steward**, a commander standing in for an absent player in Worlds (ADR-002, decision 5).
- **Threads:** commanders run on the tick's thread, as everything does (ADR-013, decision 7).
