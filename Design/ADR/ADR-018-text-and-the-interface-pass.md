# ADR-018 — Text and the interface pass

**Status:** Proposed, 2026-11-01, with the M3 plan ([MvpPlan.md](../MvpPlan.md), M3; [MvpPlan-M3.md](../MvpPlan-M3.md)). It is the text ADR that [ADR-005](ADR-005-content-formats.md), decision 1, [ADR-011](ADR-011-renderer-first-choices.md), *Consequences*, and [Hud.md](../Hud.md) §9 schedule for M3: the font's baked form, and how text and the HUD's panels reach the screen. The owner gave it the go-ahead on 2026-11-01, as written (MvpPlan.md, M3, input 1; GameDesign.md, *M3's values*, item 1), and the commit that first implements it, order M3-D1, marks it Accepted (AGENTS.md §6).

## Context

- **The font is Earthrise Mason** ([Hud.md](../Hud.md) §9), drawn for the project. Its source is `GameData/Client/Fonts/EarthriseMason-glyphs.json`: 127 glyphs, each as rows of text, in a 10-row cell with the baseline at row 7. Caps and digits are 5 × 7, the 25 command icons 9 × 9. A glyph advances by its width plus one.
- **It reaches the game the way voxel content does:** a tool under `Tools/` bakes it into the `Content` library, and nothing is loaded at runtime (ADR-005, decision 9; [ADR-008](ADR-008-server-and-client-game-data.md), decision 3). Hud.md leaves the baked form to this ADR.
- **Glyphs land on whole pixels** (Hud.md §2 and §9): drawn only at whole multiples of a font pixel, 2 to 6 screen px, with no filtering. Labels and values draw at 2 px a font pixel at a UI scale of 1, names at 3; at 1.5× they draw at 3 and 4, and at 2× at 4 and 6. The UI scale follows the display's DPI in steps of 1, 1.5 and 2.
- **The HUD is laid out in logical pixels** times the UI scale, in sRGB colours with alpha: the panel token is `rgba(21,19,16,0.92)` (Hud.md §2 and §3).
- **ADR-011 fixes where the interface draws:** after the scene has been scaled to the back buffer, at the display's resolution, "so text stays on whole pixels" (decision 3). The back buffer is drawn through an sRGB render-target view (decision 2).
- **R9:** `Engine` knows no game concept. A glyph atlas and a rectangle are not game concepts; Earthrise Mason and the HUD's tokens are, and belong to `Client` and `Content`.
- **R14:** no text library, no font rasterizer. The font is a bitmap, so none is needed.

## Decision

1. **The font's baked form is its bitmaps, in `Content`.** `Tools/BakeContent.py --client` reads the glyph JSON and writes `FontTable.h`: for each glyph its code point, its width and height in font pixels, its first row in the 10-row cell, its advance, whether it is an icon, and its rows as 16-bit masks, bit x set for a pixel at column x. `Content/Font.h` gives `Glyphs()` in ascending code point and `FindGlyph(char32_t)`. No atlas image is committed; the three `.png` atlases in `Design/Previews/` stay previews.
2. **The client builds the glyph atlas at startup,** once, from those bitmaps: an `R8_UNORM` texture of 16 by 8 cells of 10 × 10 texels, 160 × 80, one cell per glyph in table order, 255 where a pixel is set. It is uploaded through the upload ring and never changes.
3. **One interface pass in `Engine` draws the HUD and every piece of text,** `Engine::InterfacePass`: one instanced draw of screen-space quads, in the order given, which is their stacking order, alpha-blended over what is there, with no depth test, at the back buffer's size. A quad is a rectangle in pixels and either a solid colour or a glyph: a rectangle of the atlas, point-sampled, whose red channel is the alpha. Colours reach the pass as linear RGBA, so the blend happens in linear light through the sRGB view. The pass knows nothing of fonts, scales or layouts.
4. **Text is laid out by the client, on whole pixels.** `Client::TextLayout` measures and places a UTF-8 string with the font's metrics at a scale that is a whole number of screen pixels per font pixel, 2 to 6, and at whole-pixel positions; the advance is the glyph's times the scale. A string is drawn as one quad per glyph. A code point the font lacks is a defect, not a fallback: every string the client can draw is in one table, `Client/Strings.h`, and a test in `ClientTests` checks that every code point of every string has a glyph (the glyph coverage test Hud.md §9 proposes).
5. **The UI scale** is 1, 1.5 or 2, from the display's DPI by Hud.md §2's bands, 100–124 % gives 1, 125–174 % gives 1.5 and 175 % and above gives 2 (Decided 2026-11-01; MvpPlan.md, M3, input 9), with a command-line override, `--ui-scale`. Every HUD size is a logical size times the scale, rounded to a whole pixel; the text sizes at each step are Hud.md §2's.
6. **Panel colours blend in linear light,** which the HUD prototype, a browser page, does not: a browser blends in sRGB. The panel at 0.92 alpha therefore reads a little darker over a bright scene than the prototype shows. The owner judges the result on screen; a tuned alpha is a change of the token's value, not of this decision.

## Consequences for the code

- **`Tools/BakeContent.py`** gains the font, `Content` gains `Font.h`, and `Tools/CheckAssets.py` checks the glyph JSON: 127 glyphs, rows as wide as the glyph says, no code point twice, and the symbols and icons Hud.md §9 lists all present.
- **`Engine`** gains `InterfacePass`, with its shaders `InterfaceVS.hlsl` and `InterfacePS.hlsl`, and a render test on WARP ([ADR-012](ADR-012-render-tests-on-warp.md)): a glyph quad at scale 2 covers exactly its pixels and nothing else, and a solid quad at 0.92 alpha blends to the expected colour.
- **`Client`** gains the atlas, the text layout, the UI scale and the string table. **`Engine::Window`** gives the DPI of the monitor it is on.
- **The frame's order** becomes: shadows, the scene at its render scale, the overlays ([ADR-020](ADR-020-world-overlays.md)), the upscale, then the interface pass into the back buffer.

## What this forecloses

- Loading a font at runtime, a font rasterizer, and a text library.
- Filtered or fractional text: text is drawn at whole multiples of a font pixel, on whole pixels, or not at all.
- A second way to draw a rectangle on the screen. Every HUD element is quads through this pass.
- A string outside the string table, which the coverage test could not see.

## Left open

- Icons the font lacks for M4's cards: SPLIT, MERGE, WALL, PAL.GATE, DEEP and DEPLOY (GameDesign.md, *Open questions*). They are drawn into the font, not around it.
- Text in languages the font does not cover. The MVP is in English.
- Whether the atlas grows beyond one cell a glyph, for a second face or a second size. Nothing in the MVP's HUD asks for one.
