# Integration guide for Claude Code: MVP HUD

You are folding the HUD package into Earthrise.World. **Read `AGENTS.md`, `Design/MvpPlan.md`, `Design/GameDescription.md` and `Design/RendererLook.md` first.** `AGENTS.md` governs code, naming and project registration, and wins over this guide; report any conflict.

The HUD is built in **M3 (thin client)** and **M4 (interface)**, so nothing here is built earlier. Phases 0–1 are documents only, and the owner approves them before code.

---

## Phase 0: Place the package

1. Copy the package to `Design/HUD/`, or wherever `AGENTS.md` puts design packages. Keep `HUD-SPEC.md`, `font/` and `reference/` together.
2. Mark `.png` and `.otf` as binary in `.gitattributes`, if `AGENTS.md` requires it.
3. Copy `commands/integrate-hud.md` to `.claude/commands/`.
4. `reference/*.dc.html` files are design references. Never build or ship them.

**Done when:** the CI checkers pass, and nothing outside `Design/` and `.claude/` has changed.

## Phase 1: Record the decisions (owner approval)

1. Link `Design/HUD/HUD-SPEC.md` from MvpPlan M3 and M4 › interface.
2. In MvpPlan › *Decisions to record*, the text-rendering row (M4) now names **Earthrise Mason** (`Design/HUD/font/`) as the bitmap font. Its atlas tool reads `EarthriseMason-glyphs.json`.
3. Add the **§11 open decisions** of HUD-SPEC to GameDesign.md as **Proposed**.

**Stop and show the owner the diff.**

## Phase 2: Font tool and atlas (M3 or M4, `Tools/`)

1. Write the atlas tool (Python, under `Tools/`) to read `font/EarthriseMason-glyphs.json`. Use `rows` as the source; don't trust pixels in a PNG you didn't generate.
   - It writes the runtime atlas in the format the text ADR picks, at 1×, 2× and 3×.
   - It embeds metrics: advance = w + 1, baseline row 7, line height 12 / 24 / 36 at 1 / 2 / 3×, and icons in the PUA.
2. Check the output against `font/EarthriseMason-atlas-1x.png`. It must match pixel for pixel at 1×.
3. In `Engine`, add a text renderer that draws atlas glyphs at **integer scale only**: 1, 2 or 3 screen px per font px. It also draws filled rectangles for panels and borders, in palette colours from HUD-SPEC §2.

   `Engine` stays game-agnostic: it knows glyphs, quads and colours, not "command cards".

**Done when:** a test screen renders every glyph at 1×, 2× and 3× with no filtering or seams.

## Phase 3: M3 thin client (breach lab)

Build only what the breach lab needs:
- the camera buttons;
- an alert line for collapse warnings ("*Wn* WILL FALL IN *n* S", HUD-SPEC §10);
- the FIRE attack cursor (§8).

Use layout A's top bar and mode banner (§3). No command card is needed yet beyond FIRE.

## Phase 4: M4 interface (Client)

Implement HUD-SPEC §3–§10 in `Client`.

1. **HUD model.** Build the HUD model (§10) from Protocol state each tick. **Client never includes Simulation** (MvpPlan layout rule).
   - Readouts that need the resolver (*can be sapped*, slump preview, *undermines Wn*, collapse warnings) come through Protocol queries answered by Simulation's resolver, per ADR-004 decision 5.
   - Readouts that are plain sums (area, cost, yield) may come from snapshots.
2. **Layouts.** Implement layouts **A and B** from one model, with a setting to choose between them: A by default for mouse, B when the last input was touch.
   - Use logical px with UI scale 1, 1.5 or 2 (§2).
   - Keep the **minimum widths** in §3 and §4: selection panel at least 340, and command cells shrinking 96 → 72 before it does.
3. **Input** (§8), with the exact touch rules:
   - one-finger drag pans;
   - long-press of 450 ms, then drag, box-selects;
   - long-press and release selects all of the type on screen;
   - tapping the ground with your own units selected orders a move;
   - pinch zooms.

   Mouse and keyboard follow the same table. Hotkey schemes are mnemonic (default, WASD pans) or grid (arrows pan).
4. **Command card:** contents and disabled rules from §7. Commands are issued as Protocol commands (ADR-002), never applied locally.
5. **Control build** (§1): swap only the wall readout for health bars over sections and HEALTH for towers. Every other element stays identical, and a test asserts it.
6. **Logging** for the playtests (MvpPlan M4): command issued, its hotkey or click or touch source, and the time.

**Done when:**
- every element in §3 or §4 renders at 1280 × 720, 1920 × 1080 and 1024 × 768 (tablet) with no overlaps and no clipped text;
- the input table in §8 works with mouse, keyboard and touch on a Surface;
- the AGENTS.md checklist passes.

## Phase 5: Verify against the reference

Open `reference/MVP HUD Prototype.dc.html`. The top strip switches situation and layout, and the Tweaks panel switches the hotkey scheme. Put the engine's HUD beside it at the same size, and record differences in the M4 report. Match behaviour and layout, not pixels.

## Decisions made in this package (confirm with the owner)

See HUD-SPEC §11. In short: the UI scale steps, mnemonic hotkeys by default, layout A for mouse and B for touch, placeholder unit stats, and colour-blind work deferred.

## Summary to write when done

List the files changed per phase, any `AGENTS.md` conflicts, and the open decisions.
