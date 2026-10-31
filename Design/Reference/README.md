# Design/Reference

The papers and prototypes the decisions cite. Everything here but this file is committed unchanged, byte for byte (`.gitattributes`). A reference binds only through the document or ADR that cites it, and only as far as that says (AGENTS.md).

| Path | What it is | Cited by | Origin and licence |
|---|---|---|---|
| `Majercik2018Voxel.pdf` | Majercik et al., *A Ray-Box Intersection Algorithm and Efficient Dynamic Voxel Rendering*, JCGT 7(3), 2018 | [ADR-001](../ADR/ADR-001-direct-voxel-ray-casting.md) | CC BY-ND 3.0, which allows redistributing it unchanged. None of its code is used (ADR-001). |
| `SiegePrototype/` | The first look and asset package: 0.5 m voxels, the v1 scene prompt and look spec | [RendererLook.md](../RendererLook.md), [MvpPlan.md](../MvpPlan.md) | Made by the owner with Claude Design |
| `SiegePrototypeV2/` | The second package: the 1 m world and 0.5 m models, the v5 scene, the asset sheet, a draft of ADR-006 | [ADR-006](../ADR/ADR-006-voxel-sizes.md), [RendererLook.md](../RendererLook.md), [MvpAssets.md](../MvpAssets.md) | Made by the owner with Claude Design |
| `HudPrototype/` | The HUD package: its spec, the HUD prototype, and the Earthrise Mason font | [Hud.md](../Hud.md) | Made by the owner with Claude Design |

**The prototypes are the owner's.** The owner made all three packages with Claude Design, and states that every asset in them is their own (GameDesign.md, 2026-10-31). That includes `support.js`, the runtime the design tool writes beside its pages. It is byte-identical in all three packages and carries no licence notice of its own.

**What the reference pages load from the internet** when opened: three.js, opentype.js and Google Fonts. None of it is committed, and none of it reaches the game (R14).

**Not installed:** the packages' integration commands (`commands/*.md`) are kept as a record of what each package asked, and are not copied to `.claude/commands/`.
