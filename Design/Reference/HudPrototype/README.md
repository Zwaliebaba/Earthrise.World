# Earthrise.World: MVP HUD package

This package holds the HUD for the Siege and Hold trials (MvpPlan M3 and M4): two layouts, every command card, the input rules for mouse, keyboard and touch, and a bitmap font drawn for the project.

Start with **`INTEGRATION.md`** (for Claude Code). **`HUD-SPEC.md`** is the source of truth.

```
README.md
INTEGRATION.md              ← phased steps tied to M3 and M4, with done-checks
HUD-SPEC.md                 ← layouts A and B, selection, minimap, command cards, hotkeys, input, HUD model, alerts
commands/integrate-hud.md   ← copy to .claude/commands/ to run /integrate-hud
font/
  EarthriseMason-glyphs.json      ← bitmaps + metrics, the input for the atlas tool
  EarthriseMason-atlas-1x.png …3x ← reference atlases (16 columns of 10 × 10 cells)
  EarthriseMason.otf              ← for tools and mockups only
  earthrise-font.js               ← source of truth for the glyph bitmaps
reference/
  MVP HUD Prototype.dc.html       ← live prototype over the v5 scene (open in a browser)
  Earthrise Bitmap Font.dc.html   ← glyph sheet and exports
  earthrise-font.js, support.js   ← needed by the reference pages
```

- **Requirements:** the reference pages need an internet connection for three.js and opentype.js.
- **Related package:** the world look and asset package (Look v2: 1 m world, 0.5 m models) is separate, and this HUD assumes it.
