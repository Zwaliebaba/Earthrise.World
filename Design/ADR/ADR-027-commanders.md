# ADR-027 — Commanders: the AI is a command source inside the simulation, seeing through its player's fog

**Status:** Proposed, 2026-11-01, with the M4 plan ([MvpPlan.md](../MvpPlan.md), M4; [MvpPlan-M4.md](../MvpPlan-M4.md)). It records where the Siege defender's AI runs, what it may read, how its commands reach the tick, and how it replays. Its behaviours are the owner's input (MvpPlan.md, M4, input 18). The owner gave it the go-ahead on 2026-11-01, as written (MvpPlan.md, M4, input 1; GameDesign.md, *M4's values*, item 1). Order M4-F2, which first implements it, marks it Accepted (AGENTS.md §6).

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
