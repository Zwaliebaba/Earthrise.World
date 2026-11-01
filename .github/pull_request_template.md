<!--
  AGENTS.md §7 is the checklist this mirrors. Delete the parts that do not apply; do not delete
  the parts that do because they are inconvenient.
-->

## What this changes

<!-- One paragraph. What the change does, not what you did to make it. -->

## Why

<!-- The problem, or the decision this implements. Link the ADR if there is one; add one under
     Design/ADR/ if this change IS a decision (AGENTS.md §6). -->

## How it was verified

<!-- Be specific and be honest. "Builds clean, not run" and "builds and runs" are different
     claims. Say which configurations you actually built. -->

- [ ] Builds clean, `Debug|x64`, through the solution
- [ ] Every test suite runs and passes on x64
- [ ] Builds `Debug|ARM64` (**required** if this touches intrinsics, `/arch` or anything conditioned on the platform; CI does not build it)
- [ ] `python Build\CheckProjectFiles.py`
- [ ] `python Build\CheckFormat.py`
- [ ] `python Build\RunClangTidy.py`
- [ ] `python Build\TestCheckers.py`, if a checker changed
- [ ] Release built locally, if the change could break only under optimisation (CI does not build it — AGENTS.md §6)
- [ ] Ran the executable (**required** if this touches rendering, input, audio or presentation)

<!-- A box that does not apply is struck through, with the reason. -->

## Conformance

- [ ] Naming follows AGENTS.md §1 — `_` on parameters, `m_` on class state, `UPPER_CASE`
      constants, `PascalCase` enumerators, no `I`/`C`/`Base` affixes
- [ ] New, removed or moved files are in the `.vcxproj` **and** the `.filters`
- [ ] A new project was added to `.clang-tidy`'s `HeaderFilterRegex`, or this PR adds none
- [ ] Debug and Release, and x64 and ARM64, still agree on everything AGENTS.md §3 says they must
- [ ] No warning silenced, no `ConformanceMode`/`LanguageStandard`/`WarningLevel`/`TreatWarningAsError` changed
- [ ] `Design/ADR/` has a new file if the change was a decision, or an ADR it implements is marked Accepted (AGENTS.md §6)
- [ ] No new third-party dependency (R14)
- [ ] Only the lines the task required were changed

## Anything you had to bend

<!-- Rules you deviated from and why, assumptions you made, things you noticed but left alone.
     An empty section here is a claim; make sure it is true. -->
